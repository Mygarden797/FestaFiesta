// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Types/FF_CrowdAvoidanceConfig.h"

struct FFF_CrowdSpatialGrid;
struct FUnitData;

/** UU(cm) <-> m 변환 상수. 회피 코드가 UU를 만나는 곳은 미러 배열 생성/결과 반환뿐이다. */
namespace FFF_CrowdUnits
{
	inline constexpr float MetersPerUU = 0.01f;
	inline constexpr float UUPerMeter = 100.f;
}

/** 벽 선분(m). v0에서는 시뮬 사각 경계 4개만 들어온다. 부스·펜스는 NavMesh 단계의 몫이다. */
struct FFF_CrowdWallSegment
{
	FVector2f A = FVector2f::ZeroVector;
	FVector2f B = FVector2f::ZeroVector;
};

/**
 * 한 스텝의 회피 계산에 필요한 입력 묶음.
 *
 * 전부 '미러 배열'이다 — UnitDatas(AoS)를 미터 단위 float 배열로 한 번 복사한 것.
 *
 * ★ 인덱스는 반드시 AgentIndex와 같다(전체 풀 크기 유지, 압축하지 않는다).
 *   압축 배열로 만들면 인덱스 변환이 힘 계산 안쪽까지 들어가고, 거기서 한 번 어긋나면
 *   엉뚱한 사람을 밀어낸다 — 크래시가 아니라 "가끔 이상하게 움직인다"로 나타나서 최악이다.
 *
 * 왜 위치만이 아니라 속도·반경까지 미러하는가: 이웃 순회는 개체당 최대 12번 돌고,
 * 거기서 FUnitData(200바이트 넘는 구조체)를 건드리면 캐시가 통째로 밀린다.
 * 읽기 집합을 float 배열 세 개로 좁히는 것이 목적이다.
 */
struct FFF_CrowdForceInput
{
	const TArray<FVector2f>* PosMeters = nullptr;
	const TArray<FVector2f>* VelMeters = nullptr;
	const TArray<float>* RadiusMeters = nullptr;

	// 활성 개체의 조밀 인덱스 배열. 오름차순이어야 결정성이 유지된다.
	const TArray<int32>* ActiveIds = nullptr;

	bool IsValid() const
	{
		return PosMeters && VelMeters && RadiusMeters && ActiveIds
			&& PosMeters->Num() == VelMeters->Num()
			&& PosMeters->Num() == RadiusMeters->Num();
	}
};

/**
 * 사회력 모델(Helbing) 힘 계산.
 *
 * ── 왜 Social Force인가 (RVO/DetourCrowd가 아니라) ──
 *
 * RVO는 개체마다 속도 공간에서 제약 다면체를 풀어야 해서 5,000명 규모에서 비싸다.
 * Social Force는 이웃당 덧셈 몇 번이라 격자 + ParallelFor와 궁합이 맞고,
 * 무엇보다 밀도-속도 관계가 실측 보행 데이터와 맞아 '분석'에 쓸 수 있다.
 * 디지털 트윈에서는 "그럴듯하게 피하는 것"보다 "밀도가 맞는 것"이 중요하다.
 *
 * ── 힘의 구성 ──
 *
 *   F = sum(F_repulsion + F_contact) + F_wall  (+ 막힘 해소 가속)
 *
 *   F_repulsion : A * exp((r_ij - d_ij) / B) * n_ij * w  심리적 거리 유지
 *   F_contact   : 겹쳤을 때만. 강성 + 접선 마찰          물리적 밀어내기
 *   F_wall      : 벽에 대한 반발                        경계 이탈 방지
 *
 * w는 이방성 가중치 — 사람은 뒤에서 오는 사람을 덜 피한다.
 *
 * ★ 구동력(F_drive)이 합에 없는 이유
 *
 * 팀 레포의 CrowdSteering::StepAgent은 이미 목표 속도로 지수 평활(VelocitySmoothTime)해서
 * 다가간다. 그게 Helbing 구동력과 정확히 같은 역할(완화 시간 tau)이다.
 * 둘을 같이 넣으면 완화가 두 번 걸려 출발이 두 배로 굼실거리고,
 * 무엇보다 bEnableAvoidance=false일 때의 기존 동작과 비교가 불가능해진다.
 * 그래서 여기서 계산하는 것은 '회피분'뿐이고, StepAgent이 그걸 속도에 더한다.
 * ComputeDrivingForce는 나중에 구동력까지 사회력으로 넘길 때를 위해 남겨 둔다(테스트도 있다).
 *
 * ── 병렬화 규약 (어기면 조용히 틀린다) ──
 *
 *   1. 쓰기는 결과 배열의 자기 인덱스 [i]에만.
 *   2. 대칭 최적화(Out[j] -= f) 금지. 데이터 레이스이자 결정성 파괴.
 *      계산량 2배를 내고 재현성을 산다.
 *   3. 이웃 순회 순서는 GatherNearest가 정렬해 준 오름차순을 그대로 따른다.
 */
struct FESTAFIESTA_API FFF_CrowdForceSolver
{
	/**
	 * 전 활성 개체의 회피 가속도를 채운다. 단위는 m/s^2다(UU 변환은 호출자가 한다).
	 *
	 * @param Units            질량·StableAgentID·StuckSteps를 읽는다. 쓰지 않는다
	 * @param In               미러 배열 묶음. 인덱스 == AgentIndex
	 * @param Grid             In.PosMeters로 이미 Build된 격자
	 * @param Walls            벽 선분(m). 비어 있어도 된다
	 * @param OutAccelMps2     크기 = Units.Num(). 비활성 개체 자리는 0으로 채운다
	 * @param OutNeighbourCount 크기 = Units.Num(). 진단용 이웃 수(자기 자신 제외). nullptr 허용
	 */
	void ComputeAll(const TArray<FUnitData>& Units, const FFF_CrowdForceInput& In,
	                const FFF_CrowdSpatialGrid& Grid, const TArray<FFF_CrowdWallSegment>& Walls,
	                const FFF_CrowdAvoidanceConfig& Config,
	                TArray<FVector2f>& OutAccelMps2, TArray<int32>* OutNeighbourCount = nullptr);

	//=================== 개별 항 ===================
	// static인 이유: 상태가 없어야 테스트에서 한 항만 떼어내 검증할 수 있다.
	// "2인 정면 충돌 시 관통 없음" 같은 검증이 이 단위로 이루어진다.

	// 목표 속도로의 완화력(N). tau가 작을수록 빠르게 수렴한다.
	// 현재 ComputeAll은 부르지 않는다 — 위의 ★ 참고.
	static FVector2f ComputeDrivingForce(const FVector2f& DesiredVel, const FVector2f& Vel,
	                                     float MassKg, float Tau);

	/**
	 * 대인 반발 + 접촉 + 마찰. 한 쌍(i, j)에 대해 i가 받는 힘(N).
	 *
	 * [중요] exp 인자를 반드시 Config.MaxExponentArg로 자른다.
	 * crush 밀도에서 d_ij가 0에 가까워지면 exp()가 오버플로되어 inf가 되고,
	 * 다음 스텝에 inf - inf = NaN이 되어 배열 전체로 전파된다.
	 * NaN이 퍼진 뒤에는 원인 지점을 역추적할 수 없으므로 사후 검사로는 늦다.
	 */
	static FVector2f ComputeAgentInteraction(const FVector2f& PosI, const FVector2f& VelI, float RadiusI,
	                                         const FVector2f& PosJ, const FVector2f& VelJ, float RadiusJ,
	                                         const FFF_CrowdAvoidanceConfig& Config);

	// 벽(선분)에 대한 반발(N). v0에서는 시뮬 경계 4벽만 넘긴다.
	static FVector2f ComputeWallForce(const FVector2f& Pos, const FVector2f& Vel, float Radius,
	                                  const FVector2f& SegA, const FVector2f& SegB,
	                                  const FFF_CrowdAvoidanceConfig& Config);

	// 파티 응집 + 정렬(N). CohesionRadius를 넘은 멤버만 당긴다.
	// 팀 레포는 대형 오프셋(GetPartyFormationOffset)으로 파티를 붙여 두므로 지금은 쓰지 않는다.
	static FVector2f ComputeGroupForce(const FVector2f& Pos, const FVector2f& Vel,
	                                   const FVector2f& Centroid, const FVector2f& AvgVel,
	                                   float CohesionRadiusMeters);

	/**
	 * 가속도 상한을 적용한다.
	 *
	 * 클램프가 없으면 접촉력이 지배하는 순간(밀집 구간) 한 스텝에서
	 * 개체가 수 미터를 튀어 벽을 통과한다. 이건 튜닝으로 막을 수 없다 —
	 * 접촉 강성을 낮추면 이번엔 겹침이 생긴다.
	 */
	static FVector2f ClampAcceleration(const FVector2f& Accel, float MaxAccelMps2);

	/**
	 * 막힌 개체를 빼낼 단위 방향을 만든다. StuckSteps가 임계를 넘은 개체에만 적용한다.
	 *
	 * ID 해시 기반 결정론적 지터를 쓴다 — RNG를 쓰면 같은 시드로 돌려도
	 * 결과가 달라진다. 같은 위치에 겹친 두 개체가 서로 다른 방향으로 빠져나가야 하므로
	 * 지터 자체는 필요하다. MakeTraitSeed 계열과 같은 이유다.
	 */
	static FVector2f ComputeUnstuckImpulse(int32 StableAgentId, const FVector2f& SuggestedDir);

private:
	// GatherNearest 결과 버퍼. 청크별로 하나씩 두어 매 호출 할당을 피한다.
	TArray<TArray<int32>> NeighbourScratch;
};
