// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Types/FF_CrowdAvoidanceConfig.h"

struct FFF_CrowdSpatialGrid;
struct FUnitData;

namespace FFF_CrowdUnits
{
	inline constexpr float MetersPerUU = 0.01f;
	inline constexpr float UUPerMeter = 100.f;
}

// 벽 선분(m). 시뮬 경계 4개 + 시설 footprint 4개씩. 펜스·게이트는 아직 없다.
struct FFF_CrowdWallSegment
{
	FVector2f A = FVector2f::ZeroVector;
	FVector2f B = FVector2f::ZeroVector;
};

namespace FFF_CrowdWalls
{
	void AppendOrientedRect(TArray<FFF_CrowdWallSegment>& OutWalls,
	                        const FVector2f& CenterMeters, float YawRadians,
	                        float HalfExtentXMeters, float HalfExtentYMeters);

	bool IsSegmentClear(const TArray<FFF_CrowdWallSegment>& Walls,
	                    const FVector2f& FromMeters, const FVector2f& ToMeters, float ClearanceMeters);
}

enum class EFF_CrowdPosture : uint8
{
	None = 0,
	Moving = 1,
	Holding = 2
};

// 한 개체가 받는 힘(N)을 비접촉/접촉으로 나눠 담는다.
// 두 항은 스케일이 10배 이상 다르고 상한도 달라야 하므로 합치지 않는다.
//   Social  : 닿기 전 심리적 반발. 보행 스케일(수 m/s^2)
//   Contact : 겹쳤을 때만. 관통 방지가 목적이라 수십 m/s^2가 정상
// 합계만 필요하면 Total().
struct FFF_CrowdForceTerms
{
	FVector2f Social = FVector2f::ZeroVector;
	FVector2f Contact = FVector2f::ZeroVector;

	FVector2f Total() const { return Social + Contact; }

	FFF_CrowdForceTerms& operator+=(const FFF_CrowdForceTerms& Other)
	{
		Social += Other.Social;
		Contact += Other.Contact;
		return *this;
	}
};

// 한 스텝의 회피 계산 입력. 전부 미러 배열(UnitDatas를 미터 단위로 복사한 것)이다.
// 인덱스는 AgentIndex와 같아야 한다 — 압축하면 엉뚱한 사람을 밀어낸다.
// 속도·반경까지 미러하는 이유는 이웃 순회에서 FUnitData(200바이트+)를 건드리지 않기 위해서다.
struct FFF_CrowdForceInput
{
	const TArray<FVector2f>* PosMeters = nullptr;
	const TArray<FVector2f>* VelMeters = nullptr;
	const TArray<float>* RadiusMeters = nullptr;

	// 이번 스텝의 희망 속도(m/s). 세 기능이 이 배열을 필요로 한다.
	const TArray<FVector2f>* DesiredVelMeters = nullptr;

	/**
	 * 개체별 사회력 배수. nullptr 이면 전원 1.0 이다.
	 *
	 * **Social 항에만 곱한다.** Contact(겹쳤을 때의 강성·마찰)와 벽 반발은 그대로 둬야
	 * 몸이 관통하지 않고 무대 안으로도 안 들어간다. 이 값을 낮추면 "개인 공간"만 좁아져
	 * 어깨가 닿을 만큼 붙어 서고, 그 상태에서도 서로를 뚫지는 않는다.
	 *
	 * 탈출(unstuck) 가속에도 같은 배수를 건다. 안 걸면 제자리에 선 관객이 진척 0 으로
	 * 판정돼 매 스텝 옆으로 밀려나며 대열이 흩어진다.
	 */
	const TArray<float>* SocialScale = nullptr;

	/**
	 * 개체별 자세(EFF_CrowdPosture). nullptr 이면 전원 None 이다.
	 *
	 * 자리를 잡고 멈춰 선(Holding) 개체는 개체 간 힘(Social, Contact, 차선 편향)을 전부 받지 않는다.
	 * 이동 중(Moving)인 개체도 Holding 이웃에게서는 같은 힘을 받지 않는다.
	 * 벽 반발은 그대로라 시설 안으로는 들어가지 않는다. None 인 개체와 Holding 의 관계는
	 * None 쪽이 받는 힘만 그대로다.
	 */
	const TArray<uint8>* Posture = nullptr;

	// 구동력을 합에 넣는다. StepAgent의 Lerp와 배타적이다.
	bool bApplyDrivingForce = false;

	float FixedDtSeconds = 1.f / 30.f;

	// 활성 개체의 조밀 인덱스 배열. 오름차순이어야 결정성이 유지된다.
	const TArray<int32>* ActiveIds = nullptr;

	bool IsValid() const
	{
		return PosMeters && VelMeters && RadiusMeters && ActiveIds
			&& PosMeters->Num() == VelMeters->Num()
			&& PosMeters->Num() == RadiusMeters->Num()
			&& (!DesiredVelMeters || DesiredVelMeters->Num() == PosMeters->Num())
			&& (!SocialScale || SocialScale->Num() == PosMeters->Num())
			&& (!Posture || Posture->Num() == PosMeters->Num());
	}

	bool HasDesiredVelocities() const
	{
		return DesiredVelMeters && PosMeters && DesiredVelMeters->Num() == PosMeters->Num();
	}

	bool HasDrivingForce() const
	{
		return bApplyDrivingForce && HasDesiredVelocities();
	}
};

// 사회력 모델(Helbing) 힘 계산.
//
// RVO 대신 이 모델을 쓰는 이유는 5천 명 예산과 '분석 가능성'이다 —
// 이웃당 덧셈 몇 번이라 격자 + ParallelFor와 맞고, 밀도-속도 관계가 실측 보행 데이터와 맞는다.
//
// 힘의 구성
//   F = F_drive + sum(F_repulsion + F_lane + F_contact) + F_wall  (+ 탈출 가속)
//     F_drive     : m * (v_desired - v) / tau       목표 추종
//     F_repulsion : A * exp((r-d)/B) * n * w        심리적 거리 유지 (w = 이방성)
//     F_lane      : 마주 오는 상대를 오른쪽으로     정면 대치 해소
//     F_contact   : 겹쳤을 때만. 강성 + 접선 마찰
//     F_wall      : 벽 반발
//
// 상한은 두 단계다(ComposeAcceleration이 소유). 한 상한으로 뭉치면 비접촉 반발이
// 한 스텝에 MaxSpeed를 넘는 속도 변화를 만들어 목표 추종을 압도하고 통로가 닫힌다.
//
// 구동력과 StepAgent의 Lerp는 배타적이다 — 둘 다 완화(tau) 역할이라 같이 켜면 두 번 걸린다.
// DesiredVelMeters가 nullptr이면 회피분만 계산하고 Lerp가 목표 추종을 맡는다.
//
// 병렬화 규약 (어기면 조용히 틀린다)
//   1. 쓰기는 자기 인덱스 [i]에만
//   2. 대칭 최적화(Out[j] -= f) 금지 — 데이터 레이스이자 결정성 파괴
//   3. 이웃 순회 순서는 GatherNearest가 준 오름차순을 따른다
struct FESTAFIESTA_API FFF_CrowdForceSolver
{
	void ComputeAll(const TArray<FUnitData>& Units, const FFF_CrowdForceInput& In,
	                const FFF_CrowdSpatialGrid& Grid, const TArray<FFF_CrowdWallSegment>& Walls,
	                const FFF_CrowdAvoidanceConfig& Config,
	                TArray<FVector2f>& OutAccelMps2, TArray<int32>* OutNeighbourCount = nullptr);

	static FVector2f ComputeDrivingForce(const FVector2f& DesiredVel, const FVector2f& Vel,
	                                     float MassKg, float Tau);

	static FFF_CrowdForceTerms ComputeAgentInteractionTerms(
		const FVector2f& PosI, const FVector2f& VelI, float RadiusI,
		const FVector2f& PosJ, const FVector2f& VelJ, float RadiusJ,
		const FFF_CrowdAvoidanceConfig& Config,
		float MassKg = 70.f, float FixedDtSeconds = 1.f / 30.f);

	static FFF_CrowdForceTerms ComputeWallForceTerms(
		const FVector2f& Pos, const FVector2f& Vel, float Radius,
		const FVector2f& SegA, const FVector2f& SegB,
		const FFF_CrowdAvoidanceConfig& Config,
		float MassKg = 70.f, float FixedDtSeconds = 1.f / 30.f);

	static FVector2f ComputeLaneBias(const FVector2f& PosI, float RadiusI,
	                                 const FVector2f& PosJ, float RadiusJ,
	                                 const FVector2f& DesiredVelI, const FVector2f& DesiredVelJ,
	                                 const FFF_CrowdAvoidanceConfig& Config);

	static FVector2f ComputeAgentInteraction(const FVector2f& PosI, const FVector2f& VelI, float RadiusI,
	                                         const FVector2f& PosJ, const FVector2f& VelJ, float RadiusJ,
	                                         const FFF_CrowdAvoidanceConfig& Config,
	                                         float MassKg = 70.f, float FixedDtSeconds = 1.f / 30.f)
	{
		return ComputeAgentInteractionTerms(PosI, VelI, RadiusI, PosJ, VelJ, RadiusJ,
			Config, MassKg, FixedDtSeconds).Total();
	}

	static FVector2f ComputeWallForce(const FVector2f& Pos, const FVector2f& Vel, float Radius,
	                                  const FVector2f& SegA, const FVector2f& SegB,
	                                  const FFF_CrowdAvoidanceConfig& Config,
	                                  float MassKg = 70.f, float FixedDtSeconds = 1.f / 30.f)
	{
		return ComputeWallForceTerms(Pos, Vel, Radius, SegA, SegB, Config, MassKg, FixedDtSeconds).Total();
	}

	static FVector2f ComputeGroupForce(const FVector2f& Pos, const FVector2f& Vel,
	                                   const FVector2f& Centroid, const FVector2f& AvgVel,
	                                   float CohesionRadiusMeters);

	static FVector2f CapOpposingAccel(const FVector2f& Accel, const FVector2f& DesiredDir,
	                                  float MaxOpposingAccelMps2);

	static FVector2f ClampAcceleration(const FVector2f& Accel, float MaxAccelMps2);

	static FVector2f ComputeUnstuckImpulse(int32 StableAgentId, const FVector2f& SuggestedDir, int32 AttemptIndex = 0);

	static FVector2f ComposeAcceleration(const FFF_CrowdForceTerms& AgentTerms,
	                                     const FFF_CrowdForceTerms& WallTerms,
	                                     const FVector2f& Vel, const FVector2f& DesiredVel,
	                                     bool bHasDesired, bool bDrivingForce, float MassKg,
	                                     const FFF_CrowdAvoidanceConfig& Config);

private:
	// GatherNearest 결과 버퍼. 청크별로 하나씩 두어 매 호출 할당을 피한다.
	TArray<TArray<int32>> NeighbourScratch;
};
