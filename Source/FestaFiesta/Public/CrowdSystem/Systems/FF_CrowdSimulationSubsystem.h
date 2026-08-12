// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Systems/FF_CrowdActorPool.h"
#include "CrowdSystem/Systems/FF_CrowdDensityGrid.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "Math/RandomStream.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "FF_CrowdSimulationSubsystem.generated.h"

class AFF_BaseCrowd;

/**
 * 군중 시뮬레이션을 관리하는 TickableWorldSubsystem
 * Actor 풀과 Agent 풀을 분리 관리하여 필요한 Agent에만 Actor를 바인딩하는 방식으로 동작
 *
 * 한 프레임의 흐름:
 *   Tick
 *     -> StepSimulation을 고정 스텝으로 0~MaxStepsPerFrame회 (Agent 데이터만 갱신)
 *     -> DensityGrid.Update            (혼잡도 계측)
 *     -> PushPresentationToActors      (바인딩된 Actor에만 결과를 밀어 넣음)
 *
 * Actor는 시뮬레이션에 전혀 참여하지 않는다. 계산이 끝난 위치/회전/속도를 받기만 한다.
 */
UCLASS()
class FESTAFIESTA_API UFF_CrowdSimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;

	// 월드 시작 시 Actor/Agent 풀을 초기화
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// 통계 시스템(Stat Group)에 등록할 StatId 반환
	virtual TStatId GetStatId() const override;

	// 지정한 AgentIndex에 비어있는 Actor 슬롯을 바인딩
	void BindAgentToPool(int32 AgentIndex);

	// AgentIndex에 바인딩된 Actor를 다시 풀로 반환
	void ReleaseAgentToPool(int32 AgentIndex);

	// Agent 풀에서 빈 슬롯을 찾아 새 Agent를 스폰하고, 스폰된 Agent의 인덱스를 반환
	int32 SpawnAgent(UWorld& InWorld, FVector SpawnLocation, FRotator SpawnRotation);

	// ActorPoolSize만큼 CrowdActorClass Actor를 미리 생성하여 CrowdPool을 채움
	void InitializeCrowdActorPool(UWorld& InWorld, FVector ActorPoolLocation);

	// AgentPoolSize만큼 Agent 데이터(UnitDatas)를 미리 생성하여 Agent 풀을 채움
	void InitializeCrowdAgentPool(UWorld& InWorld, FVector AgentPoolLocation);

	// --- 시뮬레이션 ------------------------------------------------------------------------

	/**
	 * 시뮬레이션을 준비한다. 영역을 잡고, 밀도 그리드를 만들고, Actor/Agent 풀을 채운다.
	 *
	 * OnWorldBeginPlay에서 하지 않고 이 함수로 분리한 이유가 두 가지 있다.
	 *   1. 어떤 Actor 클래스를 쓸지(CrowdActorClass) 서브시스템은 알 방법이 없다.
	 *      레벨에 놓인 AFF_SpawnActor가 알려줘야 하는데, 액터의 BeginPlay는
	 *      서브시스템의 OnWorldBeginPlay보다 나중에 돈다(UWorld::BeginPlay 순서).
	 *   2. 군중이 없는 레벨(메뉴 등)에서까지 Actor 수백 개를 스폰할 이유가 없다.
	 *
	 * 두 번 이상 불러도 두 번째부터는 아무 일도 하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void InitializeSimulation(FVector NewOrigin, float NewRadiusUU,
		int32 NewActorPoolSize, int32 NewAgentPoolSize, TSubclassOf<AFF_BaseCrowd> InCrowdActorClass);

	/**
	 * 시뮬레이션 영역을 다시 지정하고 밀도 그리드를 재생성한다.
	 * 풀은 건드리지 않는다. 영역만 바꾸고 싶을 때 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void ConfigureSimArea(FVector NewOrigin, float NewRadiusUU);

	// InitializeSimulation이 이미 끝났는지
	UFUNCTION(BlueprintPure, Category = "Crowd")
	bool IsSimulationInitialized() const { return bSimulationInitialized; }

	// Count명을 SpawnCenter 반경 SpawnRadiusUU 안에 흩뿌려 스폰하고, 실제 스폰된 수를 반환
	UFUNCTION(BlueprintCallable, Category = "Crowd")
	int32 SpawnAgents(int32 Count, FVector SpawnCenter, float SpawnRadiusUU);

	// AgentIndex의 목적지를 지정. 도착 판정은 ArrivalRadiusUU로 한다
	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void SetAgentDestination(int32 AgentIndex, FVector Destination);

	// 활성 Agent 수. Actor 바인딩 여부와 무관하다
	UFUNCTION(BlueprintPure, Category = "Crowd")
	int32 GetActiveAgentCount() const { return ActiveAgentCount; }

	// 현재 화면에 실체화된(= Actor가 붙은) Agent 수. 항상 GetActiveAgentCount 이하다
	UFUNCTION(BlueprintPure, Category = "Crowd")
	int32 GetBoundActorCount() const { return PoolBinding.GetBoundCount(); }

	// AgentIndex의 현재 시뮬레이션 위치. 범위 밖이면 ZeroVector
	UFUNCTION(BlueprintPure, Category = "Crowd")
	FVector GetAgentPosition(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].Position : FVector::ZeroVector;
	}

	// AgentIndex가 향하고 있는 목적지. 범위 밖이면 ZeroVector.
	// 파티 멤버의 목적지는 리더 목적지 + 대형 오프셋이다
	UFUNCTION(BlueprintPure, Category = "Crowd")
	FVector GetAgentDestination(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].TargetLocation : FVector::ZeroVector;
	}

	// AgentIndex가 활성 상태인지
	UFUNCTION(BlueprintPure, Category = "Crowd")
	bool IsAgentActive(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) && UnitDatas[AgentIndex].bIsActive;
	}

	// --- 파티 조회 -------------------------------------------------------------------------

	// AgentIndex의 소속 파티 ID. 없으면 INDEX_NONE
	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	int32 GetAgentPartyID(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].PartyID : INDEX_NONE;
	}

	/**
	 * AgentIndex가 자기 파티의 리더인지.
	 *
	 * 리더만 목적지를 스스로 정한다. 1인 파티는 자기 자신이 리더이므로 항상 true.
	 */
	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	bool IsPartyLeader(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) && UnitDatas[AgentIndex].LeaderAgentIndex == AgentIndex;
	}

	// AgentIndex의 리더 Agent 인덱스. 리더 자신이면 자기 인덱스
	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	int32 GetPartyLeader(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].LeaderAgentIndex : INDEX_NONE;
	}

	// 현재 활성 파티 수(= 활성 리더 수). 활성 Agent를 순회하므로 매 프레임 부를 용도는 아니다
	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	int32 GetActivePartyCount() const;

	// PartyID에 속한 활성 Agent 인덱스 목록. 파티 전체에 명령을 내릴 때 쓴다
	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	TArray<int32> GetPartyMembers(int32 PartyID) const;

	// --- 혼잡도 조회 -----------------------------------------------------------------------

	// 해당 위치의 밀도, 명/m^2
	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	float GetDensityAtLocation(FVector WorldLocation) const { return DensityGrid.GetDensityAtLocation(WorldLocation); }

	// 해당 위치의 Fruin LOS 등급
	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	ECrowdLOS GetLOSAtLocation(FVector WorldLocation) const { return DensityGrid.GetLOSAtLocation(WorldLocation); }

	// 현재 전체 셀 중 최대 밀도, 명/m^2
	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	float GetMaxDensity() const { return DensityGrid.GetMaxDensity(); }

	// LOS D 이상인 셀 개수 (혼잡 구역의 넓이)
	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	int32 GetCongestedCellCount() const { return DensityGrid.GetCongestedCellCount(); }

	const FCrowdDensityGrid& GetDensityGrid() const { return DensityGrid; }


public:
	// 실제 스폰되어 있는 Crowd Actor들의 풀
	UPROPERTY()
	TArray<AFF_BaseCrowd*> CrowdPool;

	// 풀에 채울 Actor 클래스. 지정하지 않으면 AFF_BaseCrowd를 그대로 쓴다(메시가 없어 보이지 않음).
	// 실제로는 SkeletalMesh와 AnimBP가 설정된 BP를 넘겨야 한다
	UPROPERTY()
	TSubclassOf<AFF_BaseCrowd> CrowdActorClass;

	// Agent 풀 중 현재 비어있는(미할당) 슬롯 인덱스 목록
	UPROPERTY()
	TArray<int32> FreeAgentSlotIndex;

	// Actor 풀의 최대 크기 (동시에 실체화 가능한 Crowd Actor 수)
	UPROPERTY()
	int32 ActorPoolSize = 500;

	// Agent 풀의 최대 크기 (Actor 바인딩 여부와 무관한 논리적 개체 수)
	UPROPERTY()
	int32 AgentPoolSize = 500;

	// Agent <-> Actor 슬롯 바인딩 장부. 실제 Actor 조작은 이 클래스가 하고, 장부만 여기 위임한다
	FCrowdActorPoolBinding PoolBinding;

	// --- 시뮬레이션 설정 -------------------------------------------------------------------

	// 시뮬레이션/밀도 그리드가 덮는 영역의 중심, 월드 좌표
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	FVector SimOrigin = FVector::ZeroVector;

	// SimOrigin 기준 반경. 밀도 그리드는 이 반경을 감싸는 정사각형으로 만들어진다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float SimRadiusUU = 10000.f;

	// 목적지에 이만큼 가까워지면 도착으로 보고 새 목적지를 뽑는다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float ArrivalRadiusUU = 150.f;

	// 희망 속도까지 붙는 데 걸리는 시간, 초. 0에 가까울수록 즉시 최고 속도가 된다.
	// 사람은 순간적으로 1.5m/s가 되지 않으므로 이 값이 없으면 출발이 로봇처럼 보인다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float VelocitySmoothTime = 0.35f;

	// 초당 회전 한계, 도. 이게 없으면 밀렸을 때 제자리에서 홱 돈다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float MaxTurnRateDegrees = 360.f;

	// 밀도 그리드 셀 한 변, UU. 200UU(2m) = 4m^2를 평균 창으로 쓴다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Metrics")
	float DensityCellSizeUU = 200.f;

	// --- 파티(동반 그룹) 설정 --------------------------------------------------------------

	/**
	 * 끄면 전원이 1인 파티가 된다(= 종전과 같은 개별 이동).
	 * 그룹 이동이 혼잡도에 주는 영향을 비교할 때 이 스위치로 A/B를 본다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	bool bEnablePartyGrouping = true;

	/**
	 * 파티 크기 1~6인의 추첨 가중치. 배열 N번째가 (N+1)인 파티의 가중치다.
	 * 합이 100이 아니어도 된다 — 총합으로 정규화한다.
	 *
	 * 이 값은 **자리를 맞춘 임시값**이다. 실측 분포는 Utility AI 쪽
	 * FFF_GroupSizeDistributionRow(visitor_group_size_distribution_v0.csv)에 있고,
	 * 연동하면 이 배열을 그 테이블에서 채우게 된다. 지금은 데이터 의존 없이 눈으로 확인하는 것이 목적이다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	TArray<float> PartySizeWeights = { 22.f, 38.f, 16.f, 14.f, 6.f, 4.f };

	// 파티원 간 대형 간격, UU. Radius(34UU)의 2배 이상이어야 서로 겹쳐 보이지 않는다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	float PartySpacingUU = 90.f;

	/**
	 * 리더와 이 거리 이상 벌어진 멤버는 목적지가 아니라 리더 자신을 향한다.
	 *
	 * 목적지만 물려받으면 도중에 갈라져도 각자 제 목적지로 가버려서 파티가 흩어진다.
	 * 회피력(social force)이 붙으면 밀려서 벌어지는 일이 실제로 생기므로 이 복귀 규칙이 필요하다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	float PartyCohesionRadiusUU = 400.f;

	/**
	 * 멤버의 최고 속도 배율. 리더와 정확히 같은 속도면 한 번 벌어진 간격이 영원히 좁혀지지 않는다
	 * — 같은 속도로 쫓으면 거리가 유지될 뿐이다. 따라가는 쪽이 조금 더 빨리 걸어야 다시 붙는다.
	 *
	 * 실제 그룹은 가장 느린 사람 속도에 맞춘다. 회피력이 붙어 리더가 실제로 느려지면 이 방향을 뒤집어야 한다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	float PartyMemberSpeedBoost = 1.15f;

	// --- 디버그 ---------------------------------------------------------------------------

	// Agent의 목적지 선과 위치를 그린다. 에디터 확인용
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Debug")
	bool bDrawDebugAgents = false;

	// 밀도 그리드를 색 박스로 그린다. 셀 수만큼 드로우콜이 나가므로 시연용으로만
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Debug")
	bool bDrawDebugDensity = false;


private:
	// 고정 스텝 한 번을 진행한다. DeltaTime이 아니라 항상 FixedStepSeconds를 받는다
	void StepSimulation(float FixedDt);

	// 바인딩된 Actor에 이번 프레임 표시 위치를 밀어 넣는다
	// @param InterpAlpha 직전 스텝과 현재 스텝 사이 보간 계수, 0~1
	void PushPresentationToActors(float DeltaTime, float InterpAlpha);

	// SimOrigin/SimRadiusUU 안에서 목적지를 하나 뽑는다
	FVector PickRandomDestination();

	// --- 파티 내부 구현 ---------------------------------------------------------------------

	// PartySizeWeights로 파티 크기를 추첨한다. 그룹핑이 꺼져 있으면 항상 1
	int32 PickPartySize();

	/**
	 * 파티 내 SlotIndex번 자리의 리더 기준 상대 위치.
	 *
	 * 황금각(137.5도) 나선 배치다. 슬롯 번호만으로 서로 겹치지 않게 퍼지고,
	 * 난수를 쓰지 않으므로 같은 시드에서 같은 대형이 재현된다.
	 */
	FVector GetPartyFormationOffset(int32 SlotIndex) const;

	// 멤버의 목적지를 리더에게서 갱신한다. 리더가 유효하지 않으면 단독으로 승격시킨다
	void UpdatePartyMemberTarget(int32 AgentIndex, FUnitData& Unit);

	void DrawDebugAgents() const;

	// 다음에 발급할 파티 ID. 슬롯 인덱스와 달리 재사용하지 않아 로그를 되짚을 수 있다
	int32 NextPartyID = 0;

	/**
	 * 시뮬레이션 고정 스텝, 초. 30Hz.
	 *
	 * 프레임 DeltaTime을 그대로 쓰지 않는 이유는 두 가지다.
	 *   1. 재현성 — 같은 시드로 돌리면 프레임률과 무관하게 같은 결과가 나와야 분석 결과를 신뢰할 수 있다
	 *   2. 안정성 — 나중에 붙일 회피력은 스텝이 커지면 발산한다. 스텝 크기를 고정해야 튜닝값이 의미를 가진다
	 * 30Hz면 보행 속도(1.5m/s) 기준 한 스텝에 5cm 이동이라 충분히 촘촘하다.
	 */
	static constexpr float FixedStepSeconds = 1.f / 30.f;

	/**
	 * 한 프레임에 허용할 최대 스텝 수.
	 *
	 * 없으면 히칭이 났을 때 밀린 시간을 따라잡으려 여러 스텝을 몰아 돌고,
	 * 그게 다음 프레임을 더 늦추고, 더 많은 스텝이 밀리는 악순환(death spiral)에 빠진다.
	 * 한도를 넘긴 잔여 시간은 버린다. 시뮬레이션 시간이 실시간보다 느려지는 대신 프레임은 살아난다.
	 */
	static constexpr int32 MaxStepsPerFrame = 3;

	// 아직 소비하지 못한 프레임 시간, 초
	float SimAccumulator = 0.f;

	// 시뮬레이션 누적 시간, 초
	float SimTimeSeconds = 0.f;

	// 각 Agent의 실제 시뮬레이션 데이터(위치, 상태 등)를 저장하는 배열
	TArray<FUnitData> UnitDatas;

	// InitializeSimulation이 끝났는지. 중복 초기화로 Actor가 두 배 스폰되는 것을 막는다
	bool bSimulationInitialized = false;

	// 활성 Agent 수. UnitDatas를 매번 세지 않기 위해 증감으로 관리한다
	int32 ActiveAgentCount = 0;

	// 혼잡도 계측용 밀도 그리드
	FCrowdDensityGrid DensityGrid;

	// 목적지 추첨용. 시드를 고정해야 같은 조건에서 같은 결과가 재현된다
	FRandomStream Rng;
};
