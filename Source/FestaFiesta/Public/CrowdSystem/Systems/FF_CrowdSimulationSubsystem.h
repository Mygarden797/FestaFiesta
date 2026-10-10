// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Lane/FF_CrowdRoadLaneBuilder.h"
#include "CrowdSystem/Systems/FF_CrowdActorPool.h"
#include "CrowdSystem/Systems/FF_CrowdDensityGrid.h"
#include "CrowdSystem/Systems/FF_CrowdForces.h"
#include "CrowdSystem/Systems/FF_CrowdHeightField.h"
#include "CrowdSystem/Systems/FF_CrowdPathFollow.h"
#include "CrowdSystem/Systems/FF_CrowdSpatialGrid.h"
#include "CrowdSystem/Systems/FF_CrowdSplinePath.h"
#include "CrowdSystem/Systems/FF_CrowdSteering.h"
#include "CrowdSystem/Types/FF_CrowdAvoidanceConfig.h"
#include "CrowdSystem/Types/FF_CrowdCongestionSnapshot.h"
#include "CrowdSystem/Types/FF_CrowdItemLostEvent.h"
#include "CrowdSystem/Types/FF_CrowdMemberMissingEvent.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "Math/RandomStream.h"
#include "NPC/IndividualVariation/FF_DeterministicRandom.h"
#include "NPC/Party/FF_NeedRules.h"
#include "NPC/Party/FF_VisitorDistributionBuilder.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "Time/FF_FestivalStage.h"
#include "FF_CrowdSimulationSubsystem.generated.h"

class AFF_BaseCrowd;
class UDataTable;
class UFFZSWorldSubsystem;
struct FFFZSAudienceSnapshot;

// 개체 하나가 레인의 어디를 어디까지 타고 있는가. 디버그 표시와 콘솔 전용이다.
//
// "레인이 몇 % 채택됐다"는 집계로는 **어느 지점에서 레인에 올라탔는지**를 알 수 없다.
// 진입점이 개체 바로 옆이면 레인이 동선과 맞는 것이고, 20m 뒤라면 개체는 레인에 닿기까지
// 직선으로 걸어간다 — 같은 "채택 100%"인데 둘은 전혀 다른 상태다.
struct FFF_CrowdLaneFollowInfo
{
	bool bHasPath = false;

	// INDEX_NONE이면 이 경로는 NavMesh가 낸 것이다.
	int32 LaneIndex = INDEX_NONE;

	// 레인 경로의 첫 점. 개체가 레인에 올라타는 자리다.
	FVector EntryPointCm = FVector::ZeroVector;

	// 개체 현재 위치에서 진입점까지의 평면 거리. 이미 지났으면 지난 거리다.
	float DistanceToEntryCm = 0.f;

	// 진입점을 아직 소비하지 않았다 = 레인에 올라타기 전 직선 구간을 걷는 중이다.
	bool bApproachingEntry = false;

	int32 ConsumedCount = 0;
	int32 TotalCount = 0;
};

DECLARE_DELEGATE_RetVal_TwoParams(bool, FFF_OnPartyNeedsDecision,
	const FFF_PartyDecisionContext& , FFF_PartyDecisionResult& );

// 받을 쪽이 사건 판정·Heat Map·디버그로 여럿이다. 싱글캐스트면 나중에 붙는 쪽이 앞 구독을 조용히 덮어쓴다.
DECLARE_MULTICAST_DELEGATE_OneParam(FFF_OnCrowdCongestionSampled,
	const FFF_CrowdCongestionSnapshot& );

DECLARE_MULTICAST_DELEGATE_OneParam(FFF_OnCrowdMemberMissingChanged,
	const TArray<FFF_CrowdMemberMissingEvent>& );

DECLARE_MULTICAST_DELEGATE_OneParam(FFF_OnCrowdItemLost,
	const TArray<FFF_CrowdItemLostEvent>&  );

// "도착해서 서 있는가, 가려는데 못 가는가"를 가른 집계.
//
// 밀도 격자의 '막힌 셀'은 부스 앞 정상 체류와 통로 교착을 구분하지 못한다.
// 판정은 StepAgent가 이미 계산해 둔 Unit.StuckSteps를 그대로 읽는다 —
// 기준을 여기서 다시 적으면 두 곳이 갈리고 지표를 믿을 수 없게 된다.
//
// 회피 on/off와 무관하게 채워지므로 같은 시점에서 A/B 비교가 된다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_CrowdFlowBreakdown
{
	GENERATED_BODY()

	// 목적지가 도착 반경 밖이고 실제로 이동 중인 사람.
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Flow")
	int32 Walking = 0;

	// 목적지가 도착 반경 안인 사람. 체류·대기이며 막힌 것이 아니다.
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Flow")
	int32 ArrivedIdle = 0;

	// 가려는데 못 가는 사람. "길이 막혔다"의 답은 이 숫자다.
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Flow")
	int32 Blocked = 0;

	// 그중 막힘 임계(StuckDetectSteps)를 넘겨 탈출 가속을 받고 있는 사람.
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Flow")
	int32 UnstuckActive = 0;

	// 가장 오래 막혀 있는 사람의 연속 스텝 수. 일시 정체와 영구 교착을 가른다. 30Hz이므로 300이면 10초째 같은 자리다.
	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Flow")
	int32 MaxStuckSteps = 0;

	int32 Total() const { return Walking + ArrivedIdle + Blocked; }
};

// 군중 시뮬레이션 서브시스템. Actor 풀과 Agent 풀을 분리해 필요한 Agent에만 Actor를 바인딩한다.
//
// 한 프레임의 흐름
//   Tick -> StepSimulation 고정 스텝 0~MaxStepsPerFrame회 (Agent 데이터만)
//        -> DensityGrid.Update           (혼잡도 계측)
//        -> PushPresentationToActors     (바인딩된 Actor에만 결과 전달)
//
// Actor는 시뮬레이션에 참여하지 않고 계산이 끝난 위치·회전·속도를 받기만 한다.

// 방문객이 실제로 입장(스폰)했음을 알린다. 개인 단위가 아니라 한 번의 DrainVisitorUntil 호출에서
// 스폰된 인원 수를 묶어서 보낸다 — 경제의 입장료 연결용. 스폰 로직 자체엔 관여하지 않는다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVisitorsAdmitted, int32, AdmittedCount);

UCLASS()
class FESTAFIESTA_API UFF_CrowdSimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual TStatId GetStatId() const override;
	
	//시간 설정을 가져오는 Wrapper Class. 다만 배속이 실행될 경우 MaxStepPerFrame에 의해 제한 될 수 있어 결정성에 원인이 될 수 있다.
	UFUNCTION()
	void GetTimeScale(float NewTimeScale);
	

	void BindAgentToPool(int32 AgentIndex);

	void ReleaseAgentToPool(int32 AgentIndex);

	int32 SpawnAgent(UWorld& InWorld, FVector SpawnLocation, FRotator SpawnRotation);

	void InitializeCrowdActorPool(UWorld& InWorld, FVector ActorPoolLocation);

	void InitializeCrowdAgentPool(UWorld& InWorld, FVector AgentPoolLocation);

	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void InitializeSimulation(FVector NewOrigin,
		int32 NewActorPoolSize, int32 NewAgentPoolSize, TSubclassOf<AFF_BaseCrowd> InCrowdActorClass);

	// 분석 격자 범위를 NavMesh 경계에서 뽑는다. 시뮬레이션 반경 개념은 없다
	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void ConfigureSimArea(FVector NewOrigin);

	// NavMesh가 없는 맵·테스트에서 격자 범위를 직접 지정한다. 넘긴 상자는 이후 재구축에서도 유지된다.
	void ConfigureSimAreaWithBounds(FVector NewOrigin, const FBox& AreaBoundsCm);

	// 위 둘을 묶은 편의 함수. NavMesh를 깔 수 없는 테스트 월드에서 쓴다
	void InitializeSimulationWithBounds(FVector NewOrigin, const FBox& AreaBoundsCm,
		int32 NewActorPoolSize, int32 NewAgentPoolSize, TSubclassOf<AFF_BaseCrowd> InCrowdActorClass);

	// 분석 격자(밀도·높이)가 덮는 범위, cm. 유효하지 않으면 격자가 서지 않았다는 뜻이다.
	const FBox& GetSimAreaBounds() const { return SimAreaBoundsCm; }

	// 게이트를 등록한다. InitializeSimulation 직후 1회만 부르며, 재검증하면 인덱스가 밀려 소속이 바뀐다.
	UFUNCTION(BlueprintCallable, Category = "Crowd|Gate")
	void GatherCrowdGates(const TArray<FFF_CrowdGate>& InGates);

	// 검증을 통과한 게이트. 저작 배열이 아니라 이쪽이 런타임의 진실이다.
	const TArray<FFF_CrowdGate>& GetValidGates() const { return ValidGates; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Gate")
	int32 GetValidGateCount() const { return ValidGates.Num(); }

	UFUNCTION(BlueprintPure, Category = "Crowd|Gate")
	int32 GetSpawnGateCount() const { return SpawnGateIndices.Num(); }

	// 검증에서 제외된 게이트 수. 0 이 아니면 로그를 확인해야 한다.
	UFUNCTION(BlueprintPure, Category = "Crowd|Gate")
	int32 GetRejectedGateCount() const { return RejectedGateCount; }

	// NavMesh 가 아직 없어 검증을 건너뛴 게이트 수. "검증 통과"와 "검증 안 함"은 다르다.
	UFUNCTION(BlueprintPure, Category = "Crowd|Gate")
	int32 GetUnverifiedGateCount() const { return UnverifiedGateCount; }

	// 저작 게이트가 하나도 유효하지 않아 폴백 게이트로 돌고 있는가.
	UFUNCTION(BlueprintPure, Category = "Crowd|Gate")
	bool IsUsingFallbackGate() const { return bUsingFallbackGate; }

	UFUNCTION(BlueprintPure, Category = "Crowd")
	bool IsSimulationInitialized() const { return bSimulationInitialized; }

	UFUNCTION(BlueprintCallable, Category = "Crowd")
	int32 SpawnAgents(int32 Count, FVector SpawnCenter, float SpawnRadiusUU);

	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void SetAgentDestination(int32 AgentIndex, FVector Destination);
	
	//시설 큐에 설정한다.
	bool SetPartyQueueColumn(int32 PartyID, const TArray<FVector>& Locations, float InLaneYaw);
	
	//시설 큐에서 제거한다.
	bool ClearPartyQueueColumn(int32 PartyID);

	UFUNCTION(BlueprintPure, Category = "Crowd")
	int32 GetActiveAgentCount() const { return ActiveAgentCount; }

	UFUNCTION(BlueprintPure, Category = "Crowd")
	int32 GetBoundActorCount() const { return PoolBinding.GetBoundCount(); }

	UFUNCTION(BlueprintPure, Category = "Crowd|Flow")
	FFF_CrowdFlowBreakdown GetFlowBreakdown() const;

	UFUNCTION(BlueprintPure, Category = "Crowd")
	FVector GetAgentPosition(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].Position : FVector::ZeroVector;
	}

	UFUNCTION(BlueprintPure, Category = "Crowd")
	float GetAgentArrivalRadius(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].ArrivalRadiusUU : 0.f;
	}

	UFUNCTION(BlueprintPure, Category = "Crowd")
	FVector GetAgentDestination(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].TargetLocation : FVector::ZeroVector;
	}

	int32 GetAgentStuckSteps(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].StuckSteps : 0;
	}

	UFUNCTION(BlueprintPure, Category = "Crowd")
	bool IsAgentActive(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) && UnitDatas[AgentIndex].bIsActive;
	}

	UFUNCTION(BlueprintPure, Category = "Crowd")
	int32 GetAgentPoolSize() const { return UnitDatas.Num(); }
	
	UFUNCTION(BlueprintPure, Category = "Crowd")
	FName GetAgentArchetypeID(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].ArchetypeID : NAME_None;
	}
	UFUNCTION(BlueprintPure, Category = "Crowd")
	FFF_NeedState GetAgentNeeds(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].Needs : FFF_NeedState();
	}

	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	int32 GetAgentPartyID(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].PartyID : INDEX_NONE;
	}

	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	bool IsPartyLeader(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) && UnitDatas[AgentIndex].LeaderAgentIndex == AgentIndex;
	}

	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	int32 GetPartyLeader(int32 AgentIndex) const
	{
		return UnitDatas.IsValidIndex(AgentIndex) ? UnitDatas[AgentIndex].LeaderAgentIndex : INDEX_NONE;
	}

	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	int32 GetActivePartyCount() const;

	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	TArray<int32> GetPartyMembers(int32 PartyID) const;

	/**
	 * AgentIndex 주변 RadiusMeters(m) 안의 활성 Unit 인덱스. 자기 자신은 빠지고, 인덱스 오름차순이다.
	 * 회피 격자가 이번 풀과 맞으면 격자로 찾고(가까운 순으로 MaxCount개), 아니면 활성 Unit 전수 순회로 대신한다.
	 * 격자 경로의 위치는 마지막 회피 스텝 기준이라 한 스텝 늦을 수 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "Crowd|Query")
	TArray<int32> GetUnitsNearAgent(int32 AgentIndex, float RadiusMeters, int32 MaxCount = 32) const;

	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	float GetDensityAtLocation(FVector WorldLocation) const { return DensityGrid.GetDensityAtLocation(WorldLocation); }

	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	ECrowdLOS GetLOSAtLocation(FVector WorldLocation) const { return DensityGrid.GetLOSAtLocation(WorldLocation); }

	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	float GetMaxDensity() const { return DensityGrid.GetMaxDensity(); }

	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	int32 GetCongestedCellCount() const { return DensityGrid.GetCongestedCellCount(); }

	const FCrowdDensityGrid& GetDensityGrid() const { return DensityGrid; }

	const FFF_CrowdReadModel& GetCrowdReadModel() const { return CrowdReadModel; }

	// 스냅샷 주기마다 발행. 지속시간·히스테리시스·사건 판정은 구독자 몫이다 — 그 상태를 격자에 들이면 World 없는 테스트가 깨진다
	FFF_OnCrowdCongestionSampled OnCongestionSampled;

	// 중간에 붙은 구독자가 첫 발행을 기다리지 않고 현재 상태를 읽을 때 쓴다
	const FFF_CrowdCongestionSnapshot& GetCongestionSnapshot() const { return CongestionSnapshot; }

	FFF_OnCrowdMemberMissingChanged OnMemberMissingChanged;
	
	FFF_OnCrowdItemLost OnCrowdItemLostEventChanged;

	/**
	 * 분실물 발생 빈도. 축제 시간 1시간당 평균 건수이고, 인원·배속과 무관하다. 0이면 끈다.
	 * 발생 시각은 시뮬레이션 분 기준 지수분포로 미리 뽑고, 그 시각이 되면 조건에 맞는 방문객 한 명을 고른다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|ItemLost", meta = (ClampMin = "0.0"))
	float ItemLostPerHour = 6.f;

	// 이 속도(UU/s) 미만이면 서 있는 것으로 보고 분실 후보에서 뺀다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|ItemLost", meta = (ClampMin = "0.0"))
	float ItemLostMinSpeedUU = 30.f;

	AActor* GetAgentActor(int32 AgentIndex) const;

	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	int32 GetSnapshotCount() const { return SnapshotCount; }

	// 스텝 상한에 걸려 버려진 시뮬 시간의 총합, 초
	float GetDroppedSimSeconds() const { return DroppedSimSeconds; }

	// 위가 발생한 프레임 수
	int32 GetDroppedStepFrames() const { return DroppedStepFrames; }

	// 시뮬 시간 / 실시간. 1.0 미만이면 시뮬이 실시간을 따라가지 못하고 있다
	float GetSimTimeFidelity() const
	{
		const float Elapsed = SimTimeSeconds + DroppedSimSeconds;
		return (Elapsed > KINDA_SMALL_NUMBER) ? (SimTimeSeconds / Elapsed) : 1.f;
	}

	// 스냅샷 발행 주기, 고정 스텝 단위. 30 = 1초(30Hz).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Metrics", meta = (ClampMin = "1"))
	int32 SnapshotIntervalSteps = 30;

	// Need 지표(UFF_NpcNeedMetricsSubsystem) 확정 주기, 고정 스텝 단위. 30 = 1초.
	// 매 프레임 확정하면 OnNeedUnmetRatioChanged 가 초당 수십 번 뜬다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Metrics", meta = (ClampMin = "1"))
	int32 NeedMetricsIntervalSteps = 30;

	/**
	 * 영역 안에 있는 방문객 전원의 만족도를 DropPoints 점 깎는다. 사건 처리 실패 같은 벌칙용이다.
	 *
	 * 만족도는 방문객별 값이 아니라 창마다 다시 계산되는 전체 지수라서, 감점을 방문객 한 명 한 명에게
	 * 붙여 두고 전체 평균으로 반영한다. 그래서 영역 안 인원이 전체의 20% 면 DropPoints 의
	 * 20% 만큼만 전체 점수가 내려간다. 영역을 벗어나도 감점은 그 사람에게 남는다.
	 *
	 * 여러 번 부르면 쌓이고(사람당 최대 100), 시간이 지나면 SatisfactionPenaltyRecoverPerMinute 로 회복된다.
	 *
	 * @param Center     영역 중심(월드 좌표, cm). 높이는 무시하고 평면 거리로 잰다.
	 * @param RadiusCm   영역 반지름(cm).
	 * @param DropPoints 한 사람당 깎을 점수(0~100).
	 * @return 감점이 붙은 방문객 수. 0이면 영역에 아무도 없었던 것이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crowd|Satisfaction")
	int32 ApplySatisfactionPenaltyInArea(const FVector& Center, float RadiusCm, float DropPoints);

	/**
	 * 방문객에게 붙은 감점이 회복되는 속도. 축제 시간 1분당 몇 점씩 줄어드는지.
	 * 임시로 2점으로 둔다. (30점 감점이 축제 시간 15분 뒤 사라진다)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Satisfaction", meta = (ClampMin = "0.0"))
	float SatisfactionPenaltyRecoverPerMinute = 2.f;

	// 만족도 대기 항의 큐 성공률 계산용 — 직전 확정 창에서의 누적 카운터 스냅샷.
	// QueueJoinAccepted/QueueJoinRejected/QueuePatienceAbandons는 시뮬레이션 시작부터의
	// 누적값이라, 이번 창만의 델타를 구하려면 지난 창의 값을 들고 있어야 한다.
	int32 LastWindowQueueJoinAccepted = 0;
	int32 LastWindowQueueJoinRejected = 0;
	int32 LastWindowQueuePatienceAbandons = 0;

	// GetNeedCoverage 가 돌려주는 값. EFF_NeedType 순서
	float NeedCoverage[static_cast<int32>(EFF_NeedType::Count)] = { 1.f, 1.f, 1.f, 1.f, 1.f };

	void DespawnParty(int32 PartyID);

	// 조정자가 여기에 물어 목적지를 내려준다. 비어 있으면 Crowd가 배회로 폴백한다
	FFF_OnPartyNeedsDecision OnPartyNeedsDecision;

	void RequestDecision(int32 LeaderAgentIndex, EFF_DecisionReason Reason);

	// 외부가 정한 애니메이션 상태를 풀의 모든 Actor 에게 전달만 한다(저장·판단·읽기 없음)
	void SetCrowdState(EFF_CrowdRunState NewState);

	// 개체 하나(UnitDatas 인덱스)의 Actor 에게만 전달한다. 바인딩된 Actor 가 없으면 false
	bool SetCrowdStateForAgent(int32 AgentIndex, EFF_CrowdRunState NewState);

	/**
	 * 새 목적지가 열렸을 때 유휴 파티에게만 재결정을 요청하고 요청한 파티 수를 돌려준다.
	 *
	 * ⛔ 줄에 서 있는 파티는 건드리지 않는다. RequestDecision 이 내부에서
	 * ClearPartyQueueState(.., false) 를 부르는데 그 경로는 큐 상태를 **로컬에서만** 지운다 —
	 * 줄에 선 파티에 걸면 이쪽은 "줄 안 섰음"이 되는데 Queue 는 그 파티를 계속 들고 있어
	 * 자리가 영영 안 비고, 없는 파티를 서비스하게 된다.
	 * 퇴장 중인 파티도 같은 이유로 제외한다(목적지가 출구로 이미 정해져 있다).
	 *
	 * @param MaxParties 이번에 요청할 상한. 0 이하면 상한 없음.
	 *                   한 프레임에 결정 큐가 통째로 부풀지 않게 막는 안전판이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crowd|Decision")
	int32 RequestRedecisionForIdleParties(int32 MaxParties = 0);

	/** Stage의 공연 3축제분 전 휴식 구간에만 정원만큼 적격 파티를 직접 집결시킨다. */
	int32 GatherPartiesToMainStageAudience();

	/**
	 * 직선거리 이동시간에 곱하는 여유 배수. 실제 경로는 시설을 우회하고 군중에 막혀 느리다.
	 * 1.0 이면 직선거리 그대로라 대부분 지각한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Audience", meta = (ClampMin = "1.0"))
	float AudienceTravelSlack = 1.6f;

	/** 이동시간 환산과 별개로 붙이는 고정 여유(초). 자리 배정·경로 재계산 지연을 흡수한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Audience", meta = (ClampMin = "0.0"))
	float AudienceGatherSafetySeconds = 10.f;

	/**
	 * 집결 판정을 시작하는 바깥 창(Live 경과 초). 대기열 중단(3축제분 = 3초)과 다른 값이다 -
	 * 집결을 거기에 묶으면 전원이 공연 3초 전에 출발해 공연 후반에야 도착한다.
	 * 실제 출발 시각은 파티마다 자기 이동시간으로 정해지므로 이 값은 상한일 뿐이다.
	 *
	 * 3이면 대기열 중단과 같아 한광훈의 9/29 규칙 그대로(대부분 공연 중반 이후 도착).
	 * 창을 줄이면 먼 파티가 공연 시작 뒤에 도착한다 - 실측(Test_Main, 10/3) 도보가 100초를 넘는 파티가
	 * 많아 90초로 줄이면 시작 시점 착석이 0 이 된다. 쏠려 보이는 문제는 창이 아니라
	 * AudienceAttendanceRatio 로 푼다. 이용 중인 팀은 이 창과 무관하게 줄에서 빼지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Audience", meta = (ClampMin = "0.0"))
	float AudienceGatherLeadSeconds = 180.f;

	/**
	 * 공연마다 관람하러 가는 파티의 비율(0~1). 나머지는 평소처럼 부스를 돈다.
	 * 파티ID 와 공연 시각으로 결정론적으로 고르므로 같은 판에서는 같은 파티가 간다.
	 * 1 이면 전원이 간다(예전 동작). 모두가 무대로 달려가 보이지 않게 하려는 값이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Audience", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AudienceAttendanceRatio = 0.6f;

	/**
	 * 집결 구간 동안 이 주기로 다시 훑는다. 한 번만 부르면 그 뒤에 입장한 손님과
	 * 그때 줄에 있어서 빠진 손님이 영영 안 온다 — 입장은 Live 내내 이어진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Audience", meta = (ClampMin = "0.5"))
	float AudienceGatherIntervalSeconds = 5.f;

	/**
	 * 좌석·대기 슬롯 좌표에 정확히 못 닿고 막혔을 때 이 거리 안이면 그 자리에 선다.
	 *
	 * 밀집 관람에서는 이미 앉은 사람들의 회피력 때문에 뒤에 온 사람이 지정 좌표까지
	 * 못 들어간다. 이게 없으면 자리 앞에서 영원히 밀고 있고(관람 잠금 중에는 목적지 포기도
	 * 꺼져 있다) 관람객 수가 늘지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Audience", meta = (ClampMin = "0.0"))
	float ScheduledSettleRadiusUU = 300.f;

	/**
	 * 관람 중인 개체의 사회력(개인 공간) 배수. 1.0 이면 평소와 같다.
	 *
	 * 공연 관람은 서로 어깨가 닿을 만큼 붙어 서는 상황이라, 통로 보행용 개인 공간을
	 * 그대로 쓰면 자리가 아무리 많아도 블록이 안 채워진다 — 뒤에 온 사람이 앞사람의
	 * 반발에 밀려 자기 좌석에 못 들어간다.
	 *
	 * ★ 줄어드는 것은 **심리적 거리(Social)** 뿐이다. 겹침 방지(Contact)와 벽 반발은
	 *   그대로라 몸이 관통하거나 무대 안으로 들어가지 않는다.
	 * ★ 0 으로 두면 탈출(unstuck) 가속까지 꺼져 완전히 정지한다. 밀집 연출에는 맞지만
	 *   진짜로 낀 개체도 못 빠져나온다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Audience", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AudienceSocialForceScale = 0.1f;

	/**
	 * 약속된 장소(관람 좌석·유료 프로그램 대기 슬롯)로 가는 동안의 보행 속도 배수.
	 *
	 * 실측에서 공연 시각의 관객이 좌석까지 평균 124m·최대 230m 남아 있었다. 기본 1.5 m/s 로는
	 * 230m 에 153초가 걸리는데 공연까지 남은 시간은 그 절반도 안 된다 — 방문객 입장 곡선상
	 * 대부분이 공연 40초 전에는 아직 스폰되지도 않았다. 리드타임만으로는 못 메운다.
	 *
	 * 공연을 보러 뛰어가는 것은 실제 축제에서도 나오는 행동이라 연출로도 맞다.
	 * 1.0 이면 이 기능이 꺼진다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Audience", meta = (ClampMin = "1.0"))
	float ScheduledHurrySpeedMultiplier = 2.f;

	/** 좌석으로 걸어가는 동안의 사회력 배수. 착석분보다는 높게 둬야 서로 뚫고 들어가지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Audience", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AudienceApproachSocialForceScale = 0.5f;

	// 이 시설에 지금 잡혀 있는 관람 좌석 수. 진단 전용이다.
	UFUNCTION(BlueprintPure, Category = "Crowd|Audience")
	int32 GetOccupiedAudienceSlotCount(FGuid FacilityInstanceId) const;

	// 관람 상태에 묶여 있는 파티 수.
	UFUNCTION(BlueprintPure, Category = "Crowd|Audience")
	int32 GetAudiencePartyCount() const;

	// 도착 불가로 돌려받은 좌석 수(파티 단위, 누적).
	UFUNCTION(BlueprintPure, Category = "Crowd|Audience")
	int32 GetAudienceSeatsReclaimed() const { return AudienceSeatsReclaimed; }

	// 집결 구간인가. 잠금 진입(공연 시각 - 리드타임)부터 공연 종료까지 true 다.
	UFUNCTION(BlueprintPure, Category = "Crowd|Audience")
	bool IsMainProgramAudienceActive() const { return bMainProgramAudienceActive; }

	// 공연이 실제로 시작했는가. 관람 도착이 Waiting 으로 갈지 Spectating 으로 갈지를 가른다.
	UFUNCTION(BlueprintPure, Category = "Crowd|Audience")
	bool IsMainProgramShowStarted() const { return bMainProgramShowStarted; }

	/**
	 * 약속된 장소에 묶인 파티 수. 관람과 유료 프로그램을 나눠 센다.
	 *
	 * `걷는 중` 과 `도착` 을 가르는 것이 핵심이다 — 배정만 되고 못 닿는 상태를
	 * 배정 수만 보면 성공으로 오인한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Crowd|Audience")
	void GetScheduledPhaseCounts(int32& OutAudienceSeated, int32& OutAudienceWalking,
		int32& OutProgramAtVenue, int32& OutProgramWalking) const;

	/**
	 * 좌석으로 걷는 관람 파티(리더)의 이동 진단. 도착이 늦는 원인이 거리인지 속도인지 막힘인지 가른다.
	 * 계측용이라 시나리오 테스트만 부른다.
	 */
	void GetAudienceWalkDiagnostics(int32& OutWalkingLeaders, float& OutMeanRemainingMeters,
		float& OutMaxRemainingMeters, float& OutMeanSpeedMetersPerSecond,
		float& OutMeanCruiseMetersPerSecond, int32& OutStalledLeaders,
		int32& OutNoPathLeaders, int32& OutQueryFailedLeaders, float& OutStalledMeanRemainingMeters,
		int32& OutStalledBlockedLine) const;

	// 집결 시도 / 집결 파티 / 정원 부족 거절 / 스냅샷 실패 / 예약 우선 제외.
	void GetAudienceStats(int32& OutAttempts, int32& OutGatheredParties, int32& OutGatheredMembers,
		int32& OutCapacityRejects, int32& OutSnapshotFailures, int32& OutProgramConflicts) const;

	UFUNCTION(BlueprintPure, Category = "Crowd|Decision")
	int32 GetPendingDecisionCount() const { return DecisionQueue.Num(); }

	/**
	 * 이 목적지로 지금 걸어가고 있는 인원. 줄에 들어간 파티는 세지 않는다.
	 *
	 * 결정 배치(ProcessDecisions) 시작 시점에 다시 지은 값이므로, 결정이 한 건도 없는
	 * 프레임에서는 갱신되지 않는다 — 계수기가 아니라 그 배치가 본 스냅샷이다.
	 */
	UFUNCTION(BlueprintPure, Category = "Crowd|Decision")
	int32 GetInTransitMemberCount(FName DestinationId) const
	{
		return InTransitMemberCounts.FindRef(DestinationId);
	}

	// 지금 누군가 향하고 있는 목적지의 수. 0이면 전원이 배회 중이거나 줄에 있다
	UFUNCTION(BlueprintPure, Category = "Crowd|Decision")
	int32 GetInTransitDestinationCount() const { return InTransitMemberCounts.Num(); }

	// 한 목적지에 몰린 최대 인원. 몰림이 잡히는지 보는 값이다 — 시설 정원보다 크면 도착 후 거절이 난다
	UFUNCTION(BlueprintPure, Category = "Crowd|Decision")
	int32 GetMaxInTransitMemberCount() const;

	// 대기열 배선을 끊거나 다시 잇는다. 끄면 시설에 도착해도 줄을 서지 않고 곧바로 재평가한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Queue")
	bool bEnableQueueJoin = true;

	// 한 파티가 줄에 매여 있을 수 있는 최대 시간(초). 0 이하면 무제한.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Queue", meta = (ClampMin = "0.0"))
	float QueueHoldTimeoutSeconds = 1200.f;

	/**
	 * 프로그램 종료 직후 다시 자유활동할 최소 체류 예산(축제 분).
	 *
	 * ★ UtilityCoefficientTable 의 `reservation_exit_buffer_minutes` 행이 OnWorldBeginPlay 에서
	 *   이 값을 덮는다. 여기서 편집한 값은 그 행이 **없을 때만** 남는다 — 튜닝은 CSV 에서 할 것.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Reservation", meta = (ClampMin = "0.0"))
	float ProgramExitGraceMinutes = 10.f;

	/**
	 * 프로그램 시작보다 이만큼 일찍 도착하도록 복귀한다. 후보 귀환 판정에도 같은 축제분을 사용한다.
	 *
	 * ★ `reservation_return_buffer_minutes` 행이 OnWorldBeginPlay 에서 이 값을 덮는다
	 *   (위 ProgramExitGraceMinutes 와 같은 규칙).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Reservation", meta = (ClampMin = "0.0"))
	float ProgramReturnBufferMinutes = 10.f;

	UFUNCTION(BlueprintPure, Category = "Crowd|Queue")
	int32 GetQueueHoldTimeoutCount() const { return QueueHoldTimeouts; }

	// 큐 시간이 이 초 동안 한 번도 늘지 않으면 "멈췄다"로 보고 가입을 하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Queue", meta = (ClampMin = "0.1"))
	float QueueStallToleranceSeconds = 5.f;

	UFUNCTION(BlueprintPure, Category = "Crowd|Queue")
	bool IsQueueAdvancing() const;

	UFUNCTION(BlueprintPure, Category = "Crowd|Queue")
	int32 GetQueueStalledSkipCount() const { return QueueStalledSkips; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Queue")
	int32 GetQueueJoinAttemptCount() const { return QueueJoinAttempts; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Queue")
	int32 GetQueueJoinAcceptedCount() const { return QueueJoinAccepted; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Queue")
	int32 GetQueueJoinRejectedCount() const { return QueueJoinRejected; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Queue")
	int32 GetQueueFacilityUnresolvedCount() const { return QueueFacilityUnresolved; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Queue")
	int32 GetQueueArrivalNotifyCount() const { return QueueArrivalNotifies; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Queue")
	int32 GetQueuedPartyCount() const;

	UFUNCTION(BlueprintCallable, Category = "Crowd|Queue")
	void ResetQueueStats();

	// 한 프레임에 처리할 결정 개수 상한.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Decision")
	int32 MaxDecisionsPerFrame = 64;

	/**
	 * 목적지 방향으로 이 스텝 수만큼 연속으로 못 나아가면 목적지를 다시 고른다. 0이면 끈다.
	 *
	 * ### 왜 필요한가 — 걷는 구간에만 안전장치가 없었다
	 * 오래 걸리는 다른 두 경로에는 이미 타임아웃이 있다(퇴장은 Temp_LeaveTimeoutSeconds,
	 * 큐는 QueueHoldTimeoutSeconds). 시설로 걸어가는 구간만 없어서, 잼에 막힌 파티는
	 * 도착하거나 영원히 밀어붙이는 두 가지뿐이었다 — 그 사이에 흩어질 방법이 없으므로
	 * 인기 시설 앞 병목이 스스로 풀리지 않는다.
	 *
	 * ### 왜 조향의 StuckDetectSteps(40)와 다른 값인가
	 * 그쪽은 "탈출 가속을 태울 시점"이고 1.33초다. 회피는 서로 비켜가며 잠깐 막히는 것이
	 * 정상 동작이라, 그 임계로 목적지를 바꾸면 아무도 도착하지 못한다. 이 값은 "이 목적지는
	 * 포기한다"의 임계이므로 한 자리 크게 잡는다. 300스텝 = 10초(30Hz).
	 *
	 * 근거 없는 초기값이다. FF.Crowd.Stats 의 막힘 비율과 아래 계수기를 같이 보고 잡을 것.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Decision", meta = (ClampMin = "0"))
	int32 DecisionStuckSteps = 50;

	// 막힘 때문에 목적지를 다시 고른 횟수. 0이면 이 경로가 한 번도 안 걸린 것이다
	UFUNCTION(BlueprintPure, Category = "Crowd|Decision")
	int32 GetStuckDecisionRequests() const { return StuckDecisionRequests; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Decision")
	int32 GetStuckCounterBackoffs() const { return StuckCounterBackoffs; }

	// 통행 상한에 막혀 되돌린 스텝 수. 0이면 상한이 한 번도 안 걸린 것이다
	UFUNCTION(BlueprintPure, Category = "Crowd|Terrain")
	int32 GetHeightBlockedSteps() const { return HeightBlockedSteps; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	int32 GetFormationBlockEvents() const { return FormationBlockEvents; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	int32 GetFormationBlockedMemberCount() const;

	UFUNCTION(BlueprintPure, Category = "Crowd|Party")
	int32 GetFormationPathQueries() const { return FormationPathQueries; }

	// 목적지를 못 받았을 때 배회할 반경, UU. 리더의 현재 위치 기준이다.
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Decision")
	float WanderRadiusUU = 2000.f;

private:
	void ProcessPendingDespawns();
	
public:
	// 실제 스폰되어 있는 Crowd Actor들의 풀
	UPROPERTY()
	TArray<AFF_BaseCrowd*> CrowdPool;

	// 풀에 채울 Actor 클래스. 지정하지 않으면 AFF_BaseCrowd를 그대로 쓴다(메시가 없어 보이지 않음).
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

	// 축제장 기준점, 월드 좌표. 통행 상한 Z와 임시 퇴장 지점의 기준면이다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	FVector SimOrigin = FVector::ZeroVector;

	// 목적지에 이만큼 가까워지면 도착으로 본다(접근점·배회 목적지 기준), UU.
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float ArrivalRadiusUU = 1000.f;

	// 대기열 슬롯·서비스 지점의 도착 판정 반경, UU. **슬롯 간격보다 작아야 한다.**
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "10.0"))
	float QueueSlotArrivalRadiusUU = 45.f;

	// 개별 좌표가 배정된 줄·관람석의 최종 오차. 공용 입구/프로그램 접근점에는 적용하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "1.0", ClampMax = "10.0"))
	float AssignedSlotArrivalRadiusUU = 5.f;

	float GetAssignedSlotArrivalRadius() const
	{
		return FMath::Min(FMath::Max(QueueSlotArrivalRadiusUU, 1.f),
			FMath::Clamp(AssignedSlotArrivalRadiusUU, 1.f, 10.f));
	}

	// 시설 접근점(큐에 가입하기 전)의 도착 판정 반경, UU.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "10.0"))
	float FacilityApproachRadiusUU = 250.f;

	// 배회(시설이 아닌 목적지) 도착 판정 반경, UU.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "10.0"))
	float WanderArrivalRadiusUU = 400.f;
	
	int32 ArrivalRequestedVisitorCount = 0;

	// 외부(경제)가 채워주는 값. INDEX_NONE 이 아니면 다음 Live 진입부터 ArrivalRequestedVisitorCount
	// 대신 이 값을 쓴다 — Crowd는 이 값의 출처를 모른다, 누가 채우든 상관없는 순수 입력 슬롯이다.
	int32 ExpectedVisitorCountOverride = INDEX_NONE;

	// 같은 파티가 '도착'으로 다시 결정하기까지의 최소 간격, 초. 0이면 끈다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "0.0"))
	float DecisionCooldownSeconds = 2.f;

	UFUNCTION(BlueprintPure, Category = "Crowd|Flow")
	int32 GetDecisionCooldownSkips() const { return DecisionCooldownSkips; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Flow")
	int32 GetDecisionsProcessed() const { return DecisionsProcessed; }

	// 개인 주기가 차서 걷던 중 재평가한 횟수(reevaluate_interval_multiplier 결선 지점).
	// 끄려면 bEnablePeriodicReevaluation 을 false 로 둔다.
	UFUNCTION(BlueprintPure, Category = "Crowd|Flow")
	int32 GetReevaluateRequests() const { return ReevaluateRequests; }

	// 인내심 한계로 줄에서 이탈한 파티 수(patience_multiplier 결선 지점).
	UFUNCTION(BlueprintPure, Category = "Crowd|Queue")
	int32 GetQueuePatienceAbandons() const { return QueuePatienceAbandons; }

	// 마지막 만족도 창의 시설 커버리지(하한~1). 그 욕구를 해소하는 운영 중 시설 수 / 목표.
	// Hunger·Restroom·Stimulation 만 계산하고 나머지는 1. 1 미만이면 그 시설이 모자란다는 뜻이다
	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	float GetNeedCoverage(EFF_NeedType NeedType) const;

	// 걷는 도중 개인 주기가 차면 목적지를 다시 고른다(jockeying).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim")
	bool bEnablePeriodicReevaluation = true;

	// 시설로 걸어가는 중일 때 개인 재평가 주기에 곱하는 배수. 1 이면 배회 중과 같다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "1.0"))
	float EnRouteReevaluateIntervalScale = 3.f;

	// 손님 기본 보행 속도 배수(150cm/s 에 곱한다). 새로 스폰되는 파티부터 적용된다.
	// 1.0 이면 Test_Main 의 입구~시설 거리(100~500m)를 걷는 데 체류 시간을 거의 다 써서 줄 서기가 하루 수십 건뿐이었다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "0.1"))
	float GuestWalkSpeedScale = 1.6f;

	// 줄에서 개인 인내심 한계를 넘기면 이탈한다(reneging).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Queue")
	bool bEnableQueuePatienceAbandon = true;

	// 목적지 앞에서 감속을 시작하는 거리, UU. 0이면 ArrivalRadiusUU * 3으로 폴백한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "0.0"))
	float BrakingDistanceUU = 450.f;

	// 회피를 켠다. ★ 기본값은 false다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	bool bEnableAvoidance = true;

	// 사회력 계수. 전부 SI(m, kg, N) 단위다 — 자세한 이유는 구조체 주석 참고.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	FFF_CrowdAvoidanceConfig AvoidanceConfig;

	// 시설(부스·무대·화장실 등) footprint를 벽 반발의 대상으로 넣는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	bool bAvoidanceUseFacilityWalls = true;

	// 개체의 Z를 지형 높이에 맞춘다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Terrain")
	bool bEnableHeightField = true;

	// 높이 격자 꼭짓점 간격(UU). 작을수록 정확하고 빌드가 비싸다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Terrain")
	float HeightFieldSpacingUU = 500.f;

	// 높이 격자만 따로 좁힐 때 쓰는 반폭(UU). 0이면 분석 격자 범위(NavMesh 경계)를 그대로 쓴다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Terrain")
	float HeightFieldRadiusUU = 0.f;

	// 한 칸 높이 차가 이를 넘으면 보간 없이 끊어 올린다(UU). 0이면 항상 보간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Terrain")
	float HeightFieldCliffStepUU = 50.f;

	// 트레이스 시작 높이(UU). 지형 최고점보다 낮으면 그 위 지형을 못 잡아 개체가 파묻힌다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Terrain")
	float HeightFieldTraceAboveUU = 5000.f;

	// 트레이스가 맞은 면 근처에 NavMesh가 있을 때만 지면으로 인정한다. 아치·가로등·지붕처럼
	// 걸을 수 없는 물체는 건너뛰고 그 아래 면을 다시 찾는다. NavMesh가 없으면 이 검사는 생략된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Terrain")
	bool bHeightFieldRequireNavMesh = true;

	// SimOrigin.Z 기준 상대 통행 상한(UU). 지면이 이보다 높은 곳으로는 들어가지 않는다.
	// 0 이하면 제한이 없다.
	//
	// 절대 Z가 아니라 상대값인 이유는 맵을 옮기거나 SpawnActor를 다른 높이에 두어도 값이
	// 그대로 통하기 때문이다. 기준면은 SpawnActor 위치이므로 축제장 지면이 곧 0이다.
	//
	// HeightFieldCliffStepUU와 혼동하지 않는다 — 그쪽은 절벽에서 Z를 끊어 올리라는 표시일
	// 뿐이고 통행을 막지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Terrain")
	float MaxWalkableHeightAboveOriginUU = 250.f;

	// 지면을 못 찾은 격자점도 통행 불가로 본다. 구멍이면 막는 것이 맞지만 트레이스가 놓친
	// 지형이면 없던 벽이 생긴다. 켜기 전에 빌드 로그의 '지면을 못 찾은 격자점' 수를 볼 것.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Terrain")
	bool bBlockUnknownGround = false;

	// 목표 추종을 사회력의 **구동력**으로 계산한다. bEnableAvoidance가 켜져 있을 때만 의미가 있다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	bool bAvoidanceDrivesVelocity = true;

	// --- 경로추종 --------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Path")
	bool bEnablePathFollowing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Path", meta = (ClampMin = "1"))
	int32 PathQueriesPerStep = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Path", meta = (ClampMin = "1.0"))
	float WaypointAcceptanceRadiusUU = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Path", meta = (ClampMin = "1.0"))
	float RepathToleranceUU = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Path", meta = (ClampMin = "0.0"))
	float MinPathQueryDistanceUU = 400.f;

	UFUNCTION(BlueprintPure, Category = "Crowd|Path")
	int32 GetPathFollowingCount() const;

	UFUNCTION(BlueprintPure, Category = "Crowd|Path")
	int32 GetPathQueriesIssued() const { return PathQueriesIssued; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Path")
	int32 GetPathQueryFailures() const { return PathQueryFailures; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Path")
	int32 GetPathQueryBacklog() const { return PathQueryBacklog; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Path")
	int32 GetPartialPathCount() const { return PartialPathCount; }

	// 배치가 바뀌어 경로를 통째로 버린 횟수. 시설 설치·이동·철거 횟수와 맞아야 한다
	UFUNCTION(BlueprintPure, Category = "Crowd|Path")
	int32 GetPathLayoutInvalidations() const { return PathLayoutInvalidations; }

	// --- 레인 경로 --------------------------------------------------------------------------
	//
	// 저작된 AFF_CrowdLane 스플라인을 NavMesh보다 먼저 시도한다. 레인이 없거나 진입·이탈
	// 반경 밖이면 NavMesh로 떨어지므로, 레인을 한 개도 깔지 않은 맵의 동작은 그대로다.
	//
	// 레인은 "여기로 다녔으면 하는 길"이고 NavMesh는 "갈 수 있는 길"이다. 둘을 섞으면
	// 어느 쪽이 경로를 냈는지 알 수 없어져서, 레인이 실제로 먹었는지는 GetLanePathCount와
	// GetNavFallbackCount의 비율로만 판정한다.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Lane")
	bool bEnableLanePathing = true;

	// 개체가 이보다 멀리 있으면 그 레인은 후보에서 빠진다. 넓히면 레인과 무관한 위치의
	// 개체까지 먼 레인으로 끌려가 직선으로 진입점까지 걸어간다.b
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Lane", meta = (ClampMin = "0.0", Units = "cm"))
	float LaneEntrySnapRadiusCm = 2000.f;

	// 목적지가 이보다 멀면 그 레인으로는 못 간다. 시설 접근점이 레인에서 떨어져 있으면
	// 여기를 키우기보다 레인을 시설 쪽으로 늘리는 것이 맞다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Lane", meta = (ClampMin = "0.0", Units = "cm"))
	float LaneExitSnapRadiusCm = 1500.f;

	// 직선 거리의 몇 배까지 레인 우회를 허용하는가. 1.0이면 우회를 아예 안 받는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Lane", meta = (ClampMin = "1.0"))
	float LaneMaxDetourRatio = 5.f;

	// 월드의 AFF_CrowdLane을 모두 훑어 폴리라인으로 다시 굽고 살아 있는 경로를 버린다.
	// 저작 액터를 PIE 중에 옮겼거나 인게임 배치가 레인을 바꾼 뒤 호출한다.
	UFUNCTION(BlueprintCallable, Category = "Crowd|Lane")
	int32 RefreshLanesFromWorld();

	// 레인 권위가 Crowd 밖으로 나가는 날의 진입점. 인게임 배치 서브시스템이 여기로 밀어 넣는다.
	// 저작 레인 자리만 갈아 끼우며 도로에서 구운 레인은 건드리지 않는다.
	void SetLanes(TArray<FFF_CrowdSplineLane> NewLanes);

	// --- 도로 순회 --------------------------------------------------------------------------
	//
	// 배회 목적지가 현재 위치 중심 원판이면 순수 랜덤워크라 확산이 √n으로 느리고, 개체가
	// 스폰 지점 주변을 벗어나지 못한다. 도로 레인 위의 먼 지점을 목적지로 주면 한 걸음이
	// 세그먼트 길이가 되어 같은 횟수로 훨씬 멀리 퍼진다.
	//
	// 세그먼트 끝에 도착한 개체는 그 지점에서 다시 뽑는데, 교차로에서는 이어진 세그먼트가
	// 모두 후보로 들어오므로 갈림길을 한 번 고른 것과 같아진다. 막다른 길에서는 왔던
	// 방향만 남아 자연히 되돌아선다.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Lane")
	bool bEnableRoadPatrolWander = true;

	// 도로에서 레인을 구울지. 끄면 저작 레인만 남아 도로를 깔기 전과 동작이 같다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Lane")
	bool bEnableRoadLanes = true;

	// 이 안에 레인이 있는 개체만 순회로 뽑는다. LaneEntrySnapRadiusCm보다 좁게 잡는 이유는,
	// 넓히면 도로에서 먼 개체까지 레인으로 끌려가 직선으로 걸어오기 때문이다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Lane", meta = (ClampMin = "0.0", Units = "cm"))
	float PatrolLaneSnapRadiusCm = 1000.f;

	// 목적지가 현재 호 위치에서 이보다 가깝지 않은 후보만 쓴다. 세그먼트 끝에 이미 서 있는
	// 개체가 같은 끝을 다시 목적지로 받아 제자리 재결정을 반복하는 것을 여기서 끊는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Lane", meta = (ClampMin = "0.0", Units = "cm"))
	float PatrolMinTravelCm = 600.f;

	// 닫힌 레인에는 끝이 없으므로 전진 거리를 값으로 준다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Lane", meta = (ClampMin = "100.0", Units = "cm"))
	float PatrolLoopStepCm = 4000.f;

	// 배회·순회 목적지를 Zone 안으로 제한한다. 끄면 봉쇄가 NavMesh 통행성 하나만 남는다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim")
	bool bConfineWanderToZones = true;

	// 원판 배회. 끄면 레인 순회만 남고, 레인에서 먼 개체는 목적지를 못 받아 제자리에 선다
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Lane")
	bool bEnableDiscWander = false;

	// 배회 목적지를 못 내 제자리를 돌려준 횟수. 원판을 끄기 전후로 이 값을 비교할 것
	UFUNCTION(BlueprintPure, Category = "Crowd|Lane")
	int32 GetWanderStalls() const { return WanderStalls; }

	// Zone 밖이라 기각한 배회·순회 후보 수. 0이면 봉쇄가 한 번도 안 걸린 것이다
	UFUNCTION(BlueprintPure, Category = "Crowd|Lane")
	int32 GetWanderZoneRejects() const { return WanderZoneRejects; }

	// 이미 Zone 밖에 있던 파티를 축제장 쪽으로 되돌린 횟수
	UFUNCTION(BlueprintPure, Category = "Crowd|Lane")
	int32 GetWanderContainmentNudges() const { return WanderContainmentNudges; }

	// 순회로 목적지를 뽑은 횟수. 원판 배회와의 비율이 도로가 실제로 먹는지를 말해 준다.
	UFUNCTION(BlueprintPure, Category = "Crowd|Lane")
	int32 GetPatrolWanderCount() const { return PatrolWanderCount; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Lane")
	int32 GetDiscWanderCount() const { return DiscWanderCount; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Lane")
	int32 GetRoadLaneCount() const { return RoadLanes.Num(); }

	UFUNCTION(BlueprintPure, Category = "Crowd|Lane")
	int32 GetAuthoredLaneCount() const { return AuthoredLanes.Num(); }

	const FFF_CrowdRoadLaneBuildStats& GetRoadLaneStats() const { return RoadLaneStats; }

	// 도로 타일을 다시 읽어 순회 레인을 굽는다. 리비전이 그대로면 아무 일도 하지 않는다.
	int32 EnsureRoadLanes();

	// --- 확산 계측 --------------------------------------------------------------------------
	//
	// "퍼진다"를 눈으로 판정하면 안 된다. 표준편차는 무리가 얼마나 넓게 흩어졌는지를,
	// 엔트로피는 그 넓이가 한쪽에 쏠려 있는지를 본다 — 두 개체가 축제장 양 끝에만 있어도
	// 표준편차는 크게 나오므로 엔트로피 없이는 속는다.
	UFUNCTION(BlueprintCallable, Category = "Crowd|Spread")
	bool GetSpreadStats(FVector& OutCentroidCm, float& OutStdDevCm,
		float& OutNormalizedEntropy, int32& OutSampleCount, int32& OutOccupiedBuckets) const;

	// 엔트로피를 셀 때 쓰는 격자 한 칸의 크기.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Spread", meta = (ClampMin = "100.0", Units = "cm"))
	float SpreadBucketSizeCm = 5000.f;

	UFUNCTION(BlueprintPure, Category = "Crowd|Lane")
	int32 GetLaneCount() const { return Lanes.Num(); }

	UFUNCTION(BlueprintPure, Category = "Crowd|Lane")
	int32 GetUsableLaneCount() const;

	// 레인으로 경로를 낸 횟수.
	UFUNCTION(BlueprintPure, Category = "Crowd|Lane")
	int32 GetLanePathCount() const { return LanePathCount; }

	// 레인이 후보를 못 내서 NavMesh로 떨어진 횟수. 레인을 깔았는데 이쪽만 오르면
	// 진입·이탈 반경이나 레인 배치가 실제 동선과 맞지 않는다는 뜻이다.
	UFUNCTION(BlueprintPure, Category = "Crowd|Lane")
	int32 GetNavFallbackCount() const { return NavFallbackCount; }

	// 마지막으로 레인 경로를 낸 개체가 쓴 레인 인덱스. 디버그 표시 전용이다.
	UFUNCTION(BlueprintPure, Category = "Crowd|Lane")
	int32 GetLastLaneIndexForAgent(int32 AgentIndex) const
	{
		return AgentLaneIndices.IsValidIndex(AgentIndex) ? AgentLaneIndices[AgentIndex] : INDEX_NONE;
	}

	const TArray<FFF_CrowdSplineLane>& GetLanes() const { return Lanes; }

	bool GetLaneFollowInfo(int32 AgentIndex, FFF_CrowdLaneFollowInfo& OutInfo) const;

	UFUNCTION(BlueprintPure, Category = "Crowd|Avoidance")
	bool WasAvoidanceAppliedLastStep() const { return bAvoidanceAppliedLastStep; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Avoidance")
	int32 GetAvoidanceNeighbourCount(int32 AgentIndex) const
	{
		return AvoidanceNeighbourCounts.IsValidIndex(AgentIndex) ? AvoidanceNeighbourCounts[AgentIndex] : 0;
	}

	// 출구의 도착 지점은 더 작은 범위에 들어올 수 있어야 한다.
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float ExitRadiusUU = 150.f;

	// 희망 속도까지 붙는 데 걸리는 시간, 초. 0에 가까울수록 즉시 최고 속도가 된다.
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float VelocitySmoothTime = 0.35f;

	// 초당 회전 한계, 도. 이게 없으면 밀렸을 때 제자리에서 홱 돈다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float MaxTurnRateDegrees = 180.f;

	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float FacingSmoothTime = 0.35f;

	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float FacingEnterSpeedUU = 40.f;

	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float FacingExitSpeedUU = 15.f;

	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float FacingDeadzoneDegrees = 4.f;

	// 밀도 그리드 셀 한 변, UU. 200UU(2m) = 4㎡를 평균 창으로 쓴다.
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Metrics")
	float DensityCellSizeUU = FFF_CrowdMetrics::AnalysisCellSizeUU;

	// --- 파티(동반 그룹) 설정 --------------------------------------------------------------

	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	bool bEnablePartyGrouping = true;

	// 파티원 간 대형 간격, UU. Radius(34UU)의 2배 이상이어야 서로 겹쳐 보이지 않는다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	float PartySpacingUU = 90.f;

	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	float PartyCohesionRadiusUU = 400.f;

	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	float PartyMemberSpeedBoost = 1.15f;
	
	//파티 리더하고 심하게 이격 되었을 경우 SpeedBoost이상으로 빠르게 움직이기 위한 변수
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	float PartyMemberSpeedMultiplier = 1.3f;
	
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Party")
	int32 MissingThreshholdSteps = 120;
	
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category = "Crowd|Need", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InitialNeedMaxFraction = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Need", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InitialNeedModeFraction = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Need")
	FFF_NeedRiseRates NeedRiseRates;

	/**
	 * 시설 이용 완료 시 파티 전원의 Need 를 깎고 방문을 기록한다.
	 *
	 * 해소와 방문 기록을 한 함수에 묶은 이유: 둘 다 "이용 완료" 한 시점에만 일어나야 하는데
	 * 호출부가 갈리면 한쪽만 불리는 상태가 생긴다. 실제로 RecordVisit 이 RequestDecision 에
	 * 있던 동안 거절·포기한 시설까지 "가봤음"으로 기록되고 있었다.
	 *
	 * 어떤 성분을 깎을지는 결정 시점에 저장해 둔 State.CurrentNeedRelief 를 쓴다.
	 * 목적지가 없거나(NAME_None) 배회로 온 파티는 아무것도 하지 않는다.
	 *
	 * 파티원 전원에게 같은 해소량을 적용한다. Relieve 가 0 아래로 clamp 하므로
	 * 원래 Need 가 낮았던 멤버는 0 이 되어 개체차가 일부 사라진다 — 의도한 단순화다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Crowd|Need")
	void ApplyPartyNeedRelief(int32 PartyID);

	// 해소가 실제로 몇 번 적용됐는지. 0 에서 안 올라가면 완료 통지가 안 오는 것이다.
	UFUNCTION(BlueprintPure, Category = "Crowd|Need")
	int32 GetNeedReliefApplied() const { return NeedReliefApplied; }

	// 완료 통지는 왔는데 해소할 성분이 없었던 횟수(배회 목적지·플래그 미설정 시설).
	UFUNCTION(BlueprintPure, Category = "Crowd|Need")
	int32 GetNeedReliefSkipped() const { return NeedReliefSkipped; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Stay")
	float Temp_LiveStageMinutes = 240.f;
	
	UFUNCTION(BlueprintPure, Category = "Crowd|Stay")
	float GetPartyRemainingStayMinutes(int32 LeaderAgentIndex) const
	{
		return PartyRemainingStayMinutes.IsValidIndex(LeaderAgentIndex)
			? PartyRemainingStayMinutes[LeaderAgentIndex] : 0.f;
	}
	UFUNCTION(BlueprintPure, Category = "Crowd|Stay")
	int32 GetLeftPartyCount() const { return LeftPartyCount; }
	UFUNCTION(BlueprintPure, Category = "Crowd|Stay")
	int32 GetLeavingPartyCount() const;
	UFUNCTION(BlueprintPure, Category = "Crowd|Stay")
	int32 GetForcedLeaveCount() const { return ForcedLeaveCount; }
	UFUNCTION(BlueprintCallable, Category = "Crowd|Temp")
	void SetTemp_ExitWorldLocation(FVector NewExitWorldLocation);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Temp")
	float Temp_ExitArrivalRadiusUU = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Temp")
	float Temp_LeaveTimeoutSeconds = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Need")
	float Temp_FestivalMinutesPerRealSecond = 1.f;

	
	float SimulationTimeScale = 1.0f;
	float AppliedCrowdAnimPlayRate = 1.0f;

	UFUNCTION(BlueprintPure, Category = "Crowd|Need")
	float GetLiveMinutes() const { return (SimTimeSeconds - LiveStartSimTime) * Temp_FestivalMinutesPerRealSecond; }

	/**
	 * 이번 날 Live 진행도 0~1. 입장 곡선의 버킷 선택 기준이다.
	 *
	 * Crowd 자체 시계로 계산하지 않는다. UFF_FestivalStageSubsystem 은 ElapsedLiveSeconds 에
	 * 배속을 곱해 누적하고 메인 공연 구간에는 1배속으로 잠그는데, SimTimeSeconds 는 배속과
	 * 무관한 실초다. 두 시계가 다른 속도로 흐르므로 상수 하나로 환산할 수 없다 —
	 * 그래서 Stage 의 값을 그대로 읽는다. 그러면 배속 변경과 공연 잠금이 자동으로 따라온다.
	 *
	 * Stage 가 없는 맵(FF.Crowd.Spawn 으로 띄운 군중 전용 맵)에서는 자체 시계로 폴백한다.
	 * IsFestivalLive 가 같은 상황에서 true 를 돌려주는 것과 같은 취급이다.
	 */
	UFUNCTION(BlueprintPure, Category = "Crowd|Need")
	float GetLiveProgress01() const;

	// Agent의 목적지 선과 위치를 그린다. 에디터 확인용
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Debug")
	bool bDrawDebugAgents = false;

	// 밀도 그리드를 색 박스로 그린다. 셀 수만큼 드로우콜이 나가므로 시연용으로만
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Debug")
	bool bDrawDebugDensity = false;
	
	UFUNCTION(BlueprintPure, Category = "Crowd|Sim")
	float GetSimTimeSeconds() const { return SimTimeSeconds; }
	
	UFUNCTION(BlueprintPure, Category = "Crowd|Sim")
	int32 GetGlobalSimulationSeed() const { return GlobalSimulationSeed; }
	
	UFUNCTION(BlueprintPure, Category = "Crowd|Arrival")
	bool IsArrivalCurveActive() const { return CachedBucketCount > 0;}
	
	UFUNCTION(BlueprintPure, Category = "Crowd|Arrival")
	int32 GetArrivalSpawnedCount() const { return VisitorCursor;}
	
	UFUNCTION(BlueprintPure, Category = "Crowd|Arrival")
	int32 GetArrivalBucketCount() const { return CachedBucketCount;}

	// Inspect the live cache without loading an asset and accidentally hiding a lifetime failure.
	const UDataTable* GetLoadedArrivalCurveTable() const { return CachedArrivalCurveTable.Get(); }
	int32 GetArrivalPlannedCount() const { return ArrivalTotalVisitorCount; }
	
	void BeginArrivalSchedule(int32 TotalVisitorCount, FVector SpawnCenter, float SpawnRadiusUU,
		const TArray<FFF_ReservedPartyRequest>& ReservedParties = TArray<FFF_ReservedPartyRequest>());
	
	int32 DrainVisitorUntil(int32 TargetCumulativeCount);

	void UpdateArrivalSchedule();

	// 방문객이 입장할 때마다(이번 Drain 호출에서 스폰된 인원 수) 알린다. 경제의 입장료 연결용.
	UPROPERTY(BlueprintAssignable, Category = "Crowd|Arrival")
	FOnVisitorsAdmitted OnVisitorsAdmitted;

	// 다음 Live 진입부터 쓸 방문객 수를 외부에서 채운다(홍보 배수 반영용). INDEX_NONE 이면
	// 기존 캐시값을 그대로 쓴다 — 아무도 안 채우면 예전과 동작이 같다.
	UFUNCTION(BlueprintCallable, Category = "Crowd|Arrival")
	void SetExpectedVisitorCountOverride(int32 NewCount) { ExpectedVisitorCountOverride = NewCount; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Arrival")
	int32 GetExpectedVisitorCountOverride() const { return ExpectedVisitorCountOverride; }

private:

	// 게이트 하나를 검증하고, 통과하면 OutGate 의 GateLocation 을 NavMesh 투영 좌표로 덮는다.
	bool ValidateGate(const FFF_CrowdGate& InGate, int32 AuthoredIndex, FFF_CrowdGate& OutGate);

	// bAllowSpawn 게이트만으로 CDF 를 다시 만든다. 가중치만 바뀌면 검증 없이 이것만 부르면 된다.
	void RebuildSpawnGateCdf();

	// 입장 게이트가 없을 때 단일 앵커로 폴백을 합성한다. 게이트로 만들어야 뽑는 쪽에 분기가 안 생긴다.
	void EnsureFallbackGate();
	
	// 파티 하나가 들어올 게이트를 고른다. 반환값은 ValidGates 인덱스이며 항상 유효해야 한다.
	int32 PickSpawnGate(int32 InVisitorCursor) const;
	

	bool IsFestivalLive();

	void StepSimulation(float FixedDt);

	void PushPresentationToActors(float DeltaTime, float InterpAlpha);
	void ApplyCrowdAnimPlayRate(float PlayRate);

	void PublishCrowdReadModel();

	// 혼잡 셀 목록을 스냅샷으로 옮기고 구독자에게 쏜다. 유량을 읽으므로 ResetThroughputWindow보다 먼저 돈다
	void PublishCongestionSnapshot();

	void UpdateAvoidanceMirrors(const CrowdSteering::FStepParams& Params, bool bWantDesiredVel);

	void ComputeAvoidanceForces(const CrowdSteering::FStepParams& Params);

	void UpdatePathFollowing();

	bool BuildPathForAgent(int32 AgentIndex);

	bool BuildNavMeshPath(int32 AgentIndex, const FVector& Goal);

	void InvalidatePathsOnLayoutChange();

	bool TryBuildLanePathForAgent(int32 AgentIndex);

	int32 DropAllPaths();

	CrowdSteering::FStepParams MakeStepParams() const;

	void ClearAvoidanceForces();

public:
	int32 GetWallSegmentCount() const { return AvoidanceWalls.Num(); }
	int32 GetFacilityWallCount() const { return FacilityWallCount; }
	bool DidFacilityWallsSeeZoneSystem() const { return bFacilityWallsSawZoneSystem; }

	const FFF_CrowdHeightField& GetHeightField() const { return HeightField; }
	const FFF_CrowdHeightField& GetHeightFieldAt(const FVector2D& WorldXYCm) const;
	int32 GetGroundRefinementCount() const { return GroundRefinements.Num(); }
	int32 GetGroundRefinementSampleCount() const;
	double GetGroundRefinementBuildSeconds() const { return GroundRefinementBuildSeconds; }
	int32 GetGateGroundRefinementCount() const { return GateGroundRefinements.Num(); }
	// Grid samples plus all boundary probe attempts, including rejected cells.
	int32 GetGateGroundRefinementSampleCount() const;
	SIZE_T GetGateGroundRefinementAllocatedBytes() const;
	int32 GetGateGroundEdgeRefinementCount() const { return GateGroundEdgeFields.Num(); }
	double GetGateGroundRefinementBuildSeconds() const { return GateGroundRefinementBuildSeconds; }
	bool IsGroundWalkable(const FVector& WorldLocation) const;

	bool TryGetGroundZ(const FVector& WorldLocation, double& OutZ) const;

private:
	void SnapUnitToGround(FUnitData& Unit) const;
	void EnsureRoadSupportCache() const;
	UFUNCTION()
	void HandleRoadSupportSnapshotRestored();

	// 분석 격자 범위를 NavMesh 경계에서 뽑는다. 직접 지정된 범위가 있으면 그것이 이긴다
	bool ResolveSimAreaBounds(FBox& OutBoundsCm) const;

	void ApplySimArea(FVector NewOrigin, const FBox& AreaBoundsCm);

	void RebuildDensityGrid();
	void RebuildHeightField(bool bOnlyGateRefinements = false);
	void RebuildAvoidanceWalls();

	void EnsureFacilityWalls();

	int32 AppendFacilityWalls();

	void ProcessDecisions();

	FVector PickWanderDestination(int32 LeaderAgentIndex) const;

	bool TryPickPatrolDestination(int32 LeaderAgentIndex, uint32 BaseSeed, FVector& OutDestination) const;

	bool IsInsideFestivalBounds(const FVector& WorldPointCm) const;

	FVector MakeContainmentDestination(const FVector& FromPositionCm) const;

	void RebuildCombinedLanes();

	FFF_VisitorDistribution BuildVisitorDistribution(int32 RequestedCount,
		const TArray<FFF_ReservedPartyRequest>& ReservedParties) const;

	TArray<FFF_ReservedPartyRequest> GatherReservedPartyRequests() const;

	void ConfirmReservedPartySpawn(const FGuid& ReservationId, int32 PartyID, int32 MemberCount);

	FFF_NeedState MakeInitialNeeds(int32 StableAgentId) const;
	/**
	 * Need 를 시간만큼 올리고 파티별 체류 예산을 깎는다.
	 *
	 * @param bAggregateMetrics 이번 호출에서 Need 지표를 확정할 것인가.
	 *                          켜면 이 함수의 유닛 순회에 표본 수집을 얹는다 —
	 *                          집계 전용 순회를 따로 만들면 같은 배열을 두 번 돈다.
	 */
	void AdvanceNeeds(bool bAggregateMetrics);

	FVector GetPartyFormationOffset(int32 SlotIndex) const;

	bool UpdatePartyMemberTarget(int32 AgentIndex, FUnitData& Unit);

	void UpdateFormationBlockState(int32 AgentIndex, FUnitData& Unit);

	void UpdateBlockedMemberPath(int32 AgentIndex, int32& Budget, int32& Backlog, float MinQueryDistSq);

	bool AdvancePartyQueueOnArrival(int32 LeaderAgentIndex);

	// Audience가 활성화된 공연장 도착을 FIFO Queue보다 먼저 처리한다.
	bool HandleAudienceArrival(int32 LeaderAgentIndex);
	// 집결 구간이 끝나고 공연이 시작하는 순간 착석한 파티를 관람으로 넘긴다.
	void UpdateAudienceShowState();
	// 집결을 시작한다. 잠금 신호보다 먼저 올 수 있어 멱등하게 둔다.
	void BeginMainStageGathering(float ShowStartSeconds);
	// 공연이 끝나기 전에 못 닿는 파티의 좌석을 돌려놓는다. 반환값은 회수한 파티 수다.
	int32 ReclaimUnreachableAudienceSeats();
	bool BeginAudienceVisit(int32 LeaderAgentIndex, const FGuid& FacilityId,
		const FFFZSAudienceSnapshot& Snapshot);
	// 파티원 각자에게 좌석 좌표를 물린다. 한 자리라도 못 받으면 전부 되돌리고 false 다.
	bool AssignAudienceSeats(int32 LeaderAgentIndex, const FFFZSAudienceSnapshot& Snapshot,
		const TArray<int32>& Members);
	// 관람 파티가 지금 어느 단계에 몇이나 있는가. 집결이 실제로 도착까지 갔는지 보는 유일한 창구다.
	void LogAudiencePhaseBreakdown(const TCHAR* Reason) const;
	bool AllocateAudienceSlots(const FGuid& FacilityId, int32 RequiredSlots, int32 Capacity,
		int32& OutFirstSlotIndex);
	void ReleaseAudienceParty(int32 LeaderAgentIndex, bool bMoveToExit);
	void ReleaseAllAudienceParties(bool bMoveToExit);
	bool IsAudienceLocked(const FPartyDecisionState& State) const;
	// 유료 프로그램 약속이 남아 있는가. 관람 집결이 예약자를 가로채지 못하게 막는다.
	bool HasPendingProgramObligation(int32 LeaderAgentIndex) const;
	bool FindMainStageAudience(FGuid& OutFacilityId, FFFZSAudienceSnapshot& OutSnapshot) const;

	// 시설 접근점. ZoneSystem WorldRevision 이 바뀔 때만 다시 짓는다.
	bool TryGetProgramApproachLocation(const FGuid& FacilityInstanceId, FVector& OutApproach);

	// 매 고정 스텝에서 예약 파티의 복귀 마감·참여 시작·종료를 검사하고 상태를 전환한다.
	void UpdateProgramReservationStates();
	
	// 현재 축제분 + 직접 이동 예상분이 시작 시각에 도달하면 복귀를 시작하고 true를 반환한다.
	bool TryBeginProgramReturnIfDue(int32 LeaderIndex, const FFF_ProgramReservation& Reservation,
		const FVector& Approach, float NowMinutes);
	
	// Stage에서 Live 경과 축제분을 읽는다. 조회할 수 없으면 false를 반환하고 출력값은 유지한다.
	bool TryGetProgramLiveMinutes(float& OutMinutes) const;
	
	// 예약 복귀 도착을 처리해 시작 전이면 대기, 시작 이후면 참여로 전환하고 일반 도착 처리를 막는다.
	bool HandleProgramArrival(int32 LeaderAgentIndex);
	
	// 일반 Queue에서 이탈한 뒤 예약자 대기 슬롯을 목표로 복귀한다. 슬롯이 없으면 FallbackLocation을 사용한다.
	bool BeginProgramReturn(
		int32 LeaderAgentIndex,
		const FFF_ProgramReservation& Reservation,
		const FVector& FallbackLocation);
	
	// 예약 행동 상태를 해제하고 시설 이탈 목표와 최소 잔여 체류시간을 설정해 자유활동을 재개한다.
	void ReleaseProgramParty(int32 LeaderAgentIndex);
	
	friend struct FFF_CrowdProgramTestAccess;
	friend struct FFF_CrowdQueueTestAccess;

	void ClearPartyQueueState(int32 LeaderAgentIndex, bool bLeaveQueue);

	void SampleQueueLiveness();

	void DrawDebugAgents() const;
	
	void DespawnAgentInternal(int32 AgentID);
	
	void DespawnAllAgent();
	
	void AllAgentLeavingTrigger();
	
	//AgentMesh 적용
	void ApplyAgentAppearance(int32 AgentIndex);

private:
	
	// Settings keep a soft asset path; the running world must retain the loaded
	// table. A soft cache lets GC stop arrivals after Setup or between days.
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> CachedArrivalCurveTable;
	
	FName CachedArrivalScenarioId;
	
	int32 CachedBucketCount = 0;
	
	FFF_VisitorDistribution CachedDistribution;
	
	int32 VisitorCursor = 0;
	
	int32 ArrivalTotalVisitorCount = 0;
	
	FVector ArrivalSpawnCenter = FVector::ZeroVector;
	
	float ArrivalSpawnRadiusUU = 100.f;
	
	bool bArrivalPoolExhaustedLogged = false;


	//게이트 관련 변수
	// GateLocation 은 NavMesh 투영 좌표로 덮여 있다. 저작 좌표를 그대로 쓰면 지면에서 뜬 곳에 사람이 나온다.
	TArray<FFF_CrowdGate> ValidGates;
	// Validated authored gates occupy this prefix. EnsureFallbackGate may append
	// an emergency entry, which must not acquire an authored precision patch.
	int32 ValidatedAuthoredGateCount = 0;

	TArray<int32> SpawnGateIndices;

	// SpawnGateIndices 와 같은 길이. 마지막 원소로 정규화돼 있어 뽑을 때 나눗셈이 없다.
	TArray<float> SpawnGateCdf;

	bool bGatesGathered = false;

	bool bUsingFallbackGate = false;

	int32 RejectedGateCount = 0;

	int32 UnverifiedGateCount = 0;

	//Live 재진입 시에 상승시켜서, 매일 같은 배분이 나오지 못하도록 하는 장치이다.
	int32 ArrivalGeneration = 0;

	// Appearance is read again for later arrivals and pooled actors after GC.
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> CachedAppearenceTable;
	
	
	
	
	// 다음에 발급할 파티 ID. 슬롯 인덱스와 달리 재사용하지 않아 로그를 되짚을 수 있다
	int32 NextPartyID = 0;
	
	int32 NextStableAgentID = 0;

	static constexpr float FixedStepSeconds = 1.f / 30.f;

	static constexpr int32 MaxStepsPerFrame = 3;

	// 아직 소비하지 못한 프레임 시간, 초
	float SimAccumulator = 0.f;

	// 스텝 상한(MaxStepsPerFrame)에 걸려 버려진 시뮬 시간의 총합, 초. 프레임률과 무관하게
	// 이 값이 늘어나면 축제 시간이 실시간보다 느리게 흐른 것이다 — 인원수 한계 판정에 쓴다.
	float DroppedSimSeconds = 0.f;

	// 위가 발생한 프레임 수
	int32 DroppedStepFrames = 0;

	// 시뮬레이션 누적 시간, 초
	float SimTimeSeconds = 0.f;

	uint32 SimStepIndex = 0;

	//라이브가 시작된 시간
	float LiveStartSimTime = 0.f;

	// 각 Agent의 실제 시뮬레이션 데이터(위치, 상태 등)를 저장하는 배열
	TArray<FUnitData> UnitDatas;

	// InitializeSimulation이 끝났는지. 중복 초기화로 Actor가 두 배 스폰되는 것을 막는다
	bool bSimulationInitialized = false;

	// 활성 Agent 수. UnitDatas를 매번 세지 않기 위해 증감으로 관리한다
	int32 ActiveAgentCount = 0;

	// 혼잡도 계측용 밀도 그리드
	FCrowdDensityGrid DensityGrid;

	// 분석 격자가 덮는 범위(cm). NavMesh 경계에서 뽑거나 ConfigureSimAreaWithBounds로 직접 받는다
	FBox SimAreaBoundsCm = FBox(ForceInit);

	// 직접 받은 범위인가. true면 NavMesh 경계로 덮어쓰지 않는다
	bool bSimAreaBoundsOverridden = false;

	// 결정·UI가 읽는 사본. 발행 주기 사이에는 값이 그대로 남아 있다
	FFF_CrowdReadModel CrowdReadModel;

	// 마지막으로 쏜 혼잡 스냅샷. 멤버로 들고 있어 발행마다 배열을 새로 할당하지 않는다
	FFF_CrowdCongestionSnapshot CongestionSnapshot;

	// 마지막 발행 이후 지난 고정 스텝 수. 프레임 수가 아니라 스텝 수다 — 프레임률로 세면 유량 집계 구간이 기기 성능에 따라 달라진다
	int32 StepsSinceSnapshot = 0;

	// 마지막 Need 지표 확정 이후 지난 고정 스텝 수. 위와 같은 이유로 프레임이 아니라 스텝으로 센다
	int32 StepsSinceNeedMetrics = 0;

	int32 SnapshotCount = 0;

	// --- 회피 상태 ------------------------------------------------------------------------- 미러 배열.

	TArray<FVector2f> ScratchPos2DMeters;
	TArray<FVector2f> ScratchVel2DMeters;
	TArray<float> ScratchRadiusMeters;
	TArray<uint8> ScratchActive;

	// 이번 스텝의 희망 속도(m/s). 구동력을 쓰는 스텝에만 채운다 — 비면 솔버가 구동력을 건너뛴다
	TArray<FVector2f> ScratchDesiredVel2DMeters;
	// 개체별 사회력 배수. 관람 단계에 따라 매 스텝 채운다.
	TArray<float> ScratchSocialScale;
	// 개체별 자세(EFF_CrowdPosture). 관람 단계에서 매 스텝 채우며, 멈춰 선 사람에게서 받는 Social 을 끄는 데 쓴다.
	TArray<uint8> ScratchPosture;

	// 활성 개체의 조밀 인덱스. 오름차순으로 채운다 — 힘 합산 순서가 결정성의 전제다
	TArray<int32> ScratchActiveIds;

	// 이번 스텝의 회피 가속도(m/s^2). UU 변환 전 값이다
	TArray<FVector2f> ScratchAccelMps2;

	// 진단용 이웃 수(자기 자신 제외)
	TArray<int32> AvoidanceNeighbourCounts;

	FFF_CrowdSpatialGrid AvoidanceGrid;

	// 이웃 버퍼를 청크별로 들고 있으므로 스텝 간 재사용을 위해 멤버로 둔다
	FFF_CrowdForceSolver ForceSolver;

	// 벽 반발 대상 선분(m). 시설 footprint만 들어간다 — 시뮬 경계 벽은 존재하지 않는다
	TArray<FFF_CrowdWallSegment> AvoidanceWalls;

	// AvoidanceWalls를 마지막으로 지은 시점의 ZoneSystem WorldRevision.
	int64 FacilityWallsRevision = -1;

	// 마지막 재구축에서 벽으로 들어간 시설 수.
	int32 FacilityWallCount = 0;

	// 마지막 재구축 때 ZoneSystem을 찾았는가. 조용히 벽 없이 도는 것이 가장 위험하다
	bool bFacilityWallsSawZoneSystem = true;

	// 마지막 재구축이 어떤 손잡이 값으로 지어졌는지.
	bool bBuiltWithFacilityWalls = false;

	// --- 경로추종 상태 -----------------------------------------------------------------------

	TArray<FFF_CrowdPathFollower> PathFollowers;

	int32 PathQueryCursor = 0;

	int32 PathQueriesIssued = 0;
	int32 PathQueryFailures = 0;
	int32 PathQueryBacklog = 0;
	int32 PartialPathCount = 0;

	// 경로를 뽑을 때의 배치 리비전. 이게 바뀌면 살아 있는 모든 경로가 낡은 것이다
	int64 PathLayoutRevision = -1;

	int32 PathLayoutInvalidations = 0;

	// --- 레인 상태 --------------------------------------------------------------------------

	// 베이크된 레인. USplineComponent를 매 쿼리마다 들여다보지 않기 위해 폴리라인으로 들고 있다.
	// AuthoredLanes + RoadLanes를 이어 붙인 결과이며, 쿼리는 전부 이것만 본다
	TArray<FFF_CrowdSplineLane> Lanes;

	// AFF_CrowdLane 저작 액터에서 온 레인. 레벨 로드와 SetLanes로만 바뀐다
	TArray<FFF_CrowdSplineLane> AuthoredLanes;

	// 건설로 깔린 도로 타일에서 구운 레인. 배치 리비전이 바뀔 때마다 다시 굽는다
	TArray<FFF_CrowdSplineLane> RoadLanes;

	// RoadLanes를 구운 시점의 배치 리비전
	int64 RoadLaneRevision = -1;

	FFF_CrowdRoadLaneBuildStats RoadLaneStats;

	// AgentIndex와 같은 인덱스. 마지막으로 쓴 레인이며 NavMesh로 떨어졌으면 INDEX_NONE이다
	TArray<int32> AgentLaneIndices;

	int32 LanePathCount = 0;
	int32 NavFallbackCount = 0;

	mutable int32 PatrolWanderCount = 0;
	mutable int32 DiscWanderCount = 0;
	mutable int32 WanderZoneRejects = 0;
	mutable int32 WanderContainmentNudges = 0;
	mutable int32 WanderStalls = 0;

	// "이 월드에 Zone 이 있는가"만 WorldRevision 을 키로 캐시한다. 등록 목록 조회가 TArray를 할당해서다
	mutable TWeakObjectPtr<UFFZSWorldSubsystem> WanderBoundsSystem;
	mutable int64 WanderBoundsRevision = -1;
	mutable bool bWanderBoundsActive = false;

	// --- 지형 ------------------------------------------------------------------------------

	FFF_CrowdHeightField HeightField;
	// Only explicitly tagged, static cube walkways receive a local physics bake.
	// The apron also samples overlapping landings; a cube plane alone would put
	// feet through a higher deck where the two surfaces overlap.
	struct FGroundRefinement
	{
		FBox2D BoundsCm = FBox2D(ForceInit);
		FFF_CrowdHeightField Field;
		FBox2D CoreBoundsCm = FBox2D(ForceInit);
		FFF_CrowdHeightField CoreField;
	};
	TArray<FGroundRefinement> GroundRefinements;
	double GroundRefinementBuildSeconds = 0.0;
	// Entrances need fine ground before visitors spread into the coarse world
	// grid. Built once from validated gate disks, independently of map labels.
	TArray<FGroundRefinement> GateGroundRefinements;
	// Only discontinuous 5 cm gate-core cells receive a cached 1 cm subgrid.
	// Mixed subcells store a sampled boundary and interpolate within one class.
	// Keys are world-aligned 5 cm cell coordinates, shared by overlapping gates.
	TMap<FIntPoint, FFF_CrowdHeightField> GateGroundEdgeFields;
	int32 GateGroundRefinementProbeCount = 0;
	double GateGroundRefinementBuildSeconds = 0.0;

	// Roads overlay the coarse terrain height without rebuilding its physics samples.
	// Buckets refresh once per layout revision; each NPC only inspects its local bucket.
	struct FRoadSupportSurface
	{
		FTransform Transform;
		FVector2D HalfExtentCm = FVector2D::ZeroVector;
		double TopZ = 0.0;
	};
	mutable TArray<FRoadSupportSurface> RoadSupportSurfaces;
	mutable TMap<FIntPoint, TArray<int32>> RoadSupportBuckets;
	mutable TWeakObjectPtr<UFFZSWorldSubsystem> RoadSupportSystem;
	mutable int64 RoadSupportRevision = -1;

	// 직전 스텝에 회피가 실제로 적용됐는지. 껐을 때 낡은 Force를 한 번만 지우기 위한 것이다
	bool bAvoidanceAppliedLastStep = false;

	// 목적지 추첨용. 시드를 고정해야 같은 조건에서 같은 결과가 재현된다
	FRandomStream Rng;
	
	int32 GlobalSimulationSeed = 0;
	
	UPROPERTY()
	TObjectPtr<UDataTable> CachedVariationTable;
	
	UPROPERTY()
	TObjectPtr<UDataTable> CachedGroupSizeTable;
	UPROPERTY()
	TObjectPtr<UDataTable> CachedChildCompositionTable;
	UPROPERTY()
	TObjectPtr<UDataTable> CachedArchetypeTable;
	FName CachedGroupScopeId;
	FName CachedArchetypeScenarioId;
	TArray<FFF_VisitHistory> PartyVisitHistories;
	TArray<FFF_NeedState> PartyAggregatedNeeds;
	TArray<int32> PartyMemberCounts;
	TArray<float> PartyRemainingStayMinutes;

	/**
	 * 개체별로 확정된 변주값. AgentIndex 로 인덱싱한다. (StableAgentID 가 아니다)
	 * 스폰 시 한 번 Resolve 해서 여기 넣는다. 리더가 아닌 멤버 자리도 채운다.
	 */
	TArray<FFF_ResolvedIndividualTraits> AgentTraits;

	/**
	 * AgentTraits 에 전역 재평가 계수를 곱해 둔 개인 임계값. 인덱싱 규칙은 AgentTraits 와 같다.
	 * 계수는 런타임에 안 바뀌므로 스폰 때 한 번 곱한 값을 들고 있는다.
	 */
	TArray<FFF_PersonalReevaluationThresholds> AgentPersonalThresholds;
	UPROPERTY()
	TObjectPtr<UDataTable> CachedCoefficientTable;
	float StayDrainMultiplierAtMaxNeed = 1.f;

	// 계수 테이블에서 1회 로드. Relieve 가 성분별로 깎을 양이다.
	FFF_NeedReliefAmounts NeedReliefAmounts;

	int32 NeedReliefApplied = 0;
	int32 NeedReliefSkipped = 0;

	int32 LeftPartyCount = 0;
	int32 ForcedLeaveCount = 0;

	// 대기열 계수기. 전부 파티 단위다(인원 단위가 아니다)
	int32 QueueJoinAttempts = 0;
	int32 QueueJoinAccepted = 0;
	int32 QueueJoinRejected = 0;
	int32 QueueFacilityUnresolved = 0;
	int32 QueueArrivalNotifies = 0;
	int32 QueueHoldTimeouts = 0;
	// 메인 공연 휴지 구간으로 흐른 시뮬 시간 누적. 줄 인내 판정에서 뺀다.
	float QueueBreakSimSeconds = 0.f;
	int32 QueueStalledSkips = 0;

	// 결정 churn 계수기. 쿨다운이 실제로 무엇을 막았는지 보려면 둘을 같이 봐야 한다
	int32 DecisionCooldownSkips = 0;
	int32 DecisionsProcessed = 0;

	// 막힘으로 목적지를 포기한 횟수. 위 둘과 같이 봐야 한다 — 이 값만 크면 잼이 안 풀리는 것이고,
	// 쿨다운 스킵만 크면 임계가 짧아서 같은 파티가 계속 재시도하는 것이다
	int32 StuckDecisionRequests = 0;

	int32 StuckCounterBackoffs = 0;

	// 개인 주기가 차서 걷던 중 재평가한 횟수. DecisionsProcessed 와 같이 봐야 한다 —
	// 이 값이 처리량의 대부분을 차지하면 reevaluate_interval_seconds 가 너무 짧다
	int32 ReevaluateRequests = 0;

	// 인내심 한계로 줄에서 이탈한 파티 수(reneging). QueueJoinAccepted 대비 비율로 읽는다.
	// QueueHoldTimeouts 와 구분할 것 — 그쪽은 큐 이벤트 유실 신호고 이쪽은 정상 행동이다
	int32 QueuePatienceAbandons = 0;

	// 통행 상한에 막혀 되돌린 스텝 수. 이 값이 계속 오르는데 StuckDecisionRequests가 안 오르면
	// 상한이 목적지를 감싸고 있다는 뜻이다 — 상한을 올리거나 목적지 쪽을 봐야 한다
	int32 HeightBlockedSteps = 0;

	int32 FormationBlockEvents = 0;

	int32 FormationPathQueries = 0;

	// 큐 진행 감시. SampleQueueLiveness가 매 스텝 갱신한다
	float LastObservedQueueSimTime = -1.f;
	float LastQueueAdvanceSimTime = 0.f;
	FVector Temp_ExitWorldLocation = FVector::ZeroVector;
	bool bTemp_ExitLocationSet = false;
	float LastNeedAdvanceSimTime = 0.f;
	float CachedCorrelationStrength = 0.3f;
	
	TMap<int32, int32> PartyLeaderByID;

	TSet<int32> PendingDespawnParty;

	// 파티별 결정 상태. **리더 AgentIndex로 인덱싱한다** (PartyID가 아니다).
	TArray<FPartyDecisionState> PartyDecisionStates;

	// 결정을 기다리는 리더 AgentIndex, FIFO. 앞에서 잘라내므로 예산에 걸려 밀린 파티가 다음 프레임 맨 앞이 된다 — 굶는 파티가 없다
	TArray<int32> DecisionQueue;
	
	TMap<FName, int32> InTransitMemberCounts;

	// 시설별 점유 좌석. 좌석 API는 예약을 소유하지 않으므로 Crowd가 수명과 중복을 관리한다.
	TMap<FGuid, TSet<int32>> OccupiedAudienceSlots;
	// 도착 불가로 회수된 파티는 이번 공연에서만 제외한다. 다음 공연에는 다시 참여한다.
	TSet<int32> AudiencePartiesExcludedThisShow;
	TSet<FGuid> MissingProgramApproachWarnings;
	TMap<FGuid, FVector> CachedProgramApproachByFacility;
	int64 ProgramApproachCacheRevision = -1;
	bool bMainProgramAudienceActive = false;

	// 리드타임 구간(집결 중)과 공연 중을 가른다. Stage 시계로 매 스텝 판정한다 —
	// 시작 전용 델리게이트를 새로 만들면 구독자마다 두 신호의 순서를 또 맞춰야 한다.
	bool bMainProgramShowStarted = false;
	float MainProgramStartLiveSeconds = 0.f;
	float MainProgramEndLiveSeconds = 0.f;
	float NextAudienceGatherSimTime = 0.f;
	// 무대가 없는 맵에서 5초마다 같은 경고를 찍지 않는다. 집결 창마다 한 번이면 충분하다.
	bool bWarnedMissingAudienceThisShow = false;
	FName ActiveMainStageEventLabel = NAME_None;

	int32 AudienceGatherAttempts = 0;
	int32 AudienceGatheredParties = 0;
	int32 AudienceGatheredMembers = 0;
	int32 AudienceCapacityRejects = 0;
	int32 AudienceSnapshotFailures = 0;
	int32 AudienceProgramConflictSkips = 0;
	int32 AudienceSettledInPlace = 0;
	int32 ProgramSettledInPlace = 0;
	int32 AudienceSeatsReclaimed = 0;
	
	
	

	void RebuildInTransitMemberCounts();

	// 상대 통행 상한 손잡이를 높이 격자가 쓰는 절대 Z(cm)로 환산해 밀어넣는다.
	// 조회 시점 값이라 빌드와 Tick 양쪽에서 부르며, Tick 쪽이 콘솔 변경을 바로 듣게 한다.
	void ApplyWalkableCeiling();

	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage CurrentStage);

	UFUNCTION()
	void HandleMainProgramStarted(FString EventLabel);

	UFUNCTION()
	void HandleMainProgramEnded(FString EventLabel);
	
	
	void FlushMissingEvent();
	
	void QueueMissingEvent(int32 AgentIndex, const FUnitData& Unit, EFF_CrowdMissingChange Change);

	void TickItemLost();
	int32 PickItemLostCandidate() const;
	void FlushItemLostEvent();

	//이벤트 관련
private:
	TArray<FFF_CrowdMemberMissingEvent> CrowdMemberMissingEvents;

	TArray<FFF_CrowdItemLostEvent> CrowdItemLostEvents;
	float NextItemLostAtMinutes = -1.f;
	float LastItemLostClockMinutes = -1.f;
};
