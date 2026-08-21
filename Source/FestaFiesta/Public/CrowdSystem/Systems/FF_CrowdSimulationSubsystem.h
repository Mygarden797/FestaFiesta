// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Systems/FF_CrowdActorPool.h"
#include "CrowdSystem/Systems/FF_CrowdDensityGrid.h"
#include "CrowdSystem/Systems/FF_CrowdForces.h"
#include "CrowdSystem/Systems/FF_CrowdHeightField.h"
#include "CrowdSystem/Systems/FF_CrowdSpatialGrid.h"
#include "CrowdSystem/Systems/FF_CrowdSteering.h"
#include "CrowdSystem/Types/FF_CrowdAvoidanceConfig.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "Math/RandomStream.h"
#include "NPC/Party/FF_NeedRules.h"
#include "NPC/Party/FF_VisitorDistributionBuilder.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "FF_CrowdSimulationSubsystem.generated.h"

class AFF_BaseCrowd;
class UDataTable;

DECLARE_DELEGATE_RetVal_TwoParams(bool, FFF_OnPartyNeedsDecision,
	const FFF_PartyDecisionContext& , FFF_PartyDecisionResult& );

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
UCLASS()
class FESTAFIESTA_API UFF_CrowdSimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual TStatId GetStatId() const override;

	void BindAgentToPool(int32 AgentIndex);

	void ReleaseAgentToPool(int32 AgentIndex);

	int32 SpawnAgent(UWorld& InWorld, FVector SpawnLocation, FRotator SpawnRotation);

	void InitializeCrowdActorPool(UWorld& InWorld, FVector ActorPoolLocation);

	void InitializeCrowdAgentPool(UWorld& InWorld, FVector AgentPoolLocation);

	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void InitializeSimulation(FVector NewOrigin, float NewRadiusUU,
		int32 NewActorPoolSize, int32 NewAgentPoolSize, TSubclassOf<AFF_BaseCrowd> InCrowdActorClass);

	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void ConfigureSimArea(FVector NewOrigin, float NewRadiusUU);

	UFUNCTION(BlueprintPure, Category = "Crowd")
	bool IsSimulationInitialized() const { return bSimulationInitialized; }

	UFUNCTION(BlueprintCallable, Category = "Crowd")
	int32 SpawnAgents(int32 Count, FVector SpawnCenter, float SpawnRadiusUU);

	UFUNCTION(BlueprintCallable, Category = "Crowd")
	void SetAgentDestination(int32 AgentIndex, FVector Destination);

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

	UFUNCTION(BlueprintPure, Category = "Crowd|Metrics")
	int32 GetSnapshotCount() const { return SnapshotCount; }

	// 스냅샷 발행 주기, 고정 스텝 단위. 30 = 1초(30Hz).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Metrics", meta = (ClampMin = "1"))
	int32 SnapshotIntervalSteps = 30;

	void DespawnParty(int32 PartyID);

	// 조정자가 여기에 물어 목적지를 내려준다. 비어 있으면 Crowd가 배회로 폴백한다
	FFF_OnPartyNeedsDecision OnPartyNeedsDecision;

	void RequestDecision(int32 LeaderAgentIndex, EFF_DecisionReason Reason);

	UFUNCTION(BlueprintPure, Category = "Crowd|Decision")
	int32 GetPendingDecisionCount() const { return DecisionQueue.Num(); }

	// 대기열 배선을 끊거나 다시 잇는다. 끄면 시설에 도착해도 줄을 서지 않고 곧바로 재평가한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Queue")
	bool bEnableQueueJoin = true;

	// 한 파티가 줄에 매여 있을 수 있는 최대 시간(초). 0 이하면 무제한.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Queue", meta = (ClampMin = "0.0"))
	float QueueHoldTimeoutSeconds = 1200.f;

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

	// 시뮬레이션/밀도 그리드가 덮는 영역의 중심, 월드 좌표
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	FVector SimOrigin = FVector::ZeroVector;

	// SimOrigin 기준 반경. 밀도 그리드는 이 반경을 감싸는 정사각형으로 만들어진다
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float SimRadiusUU = 10000.f;

	// 목적지에 이만큼 가까워지면 도착으로 본다(접근점·배회 목적지 기준), UU.
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Sim")
	float ArrivalRadiusUU = 150.f;

	// 대기열 슬롯·서비스 지점의 도착 판정 반경, UU. **슬롯 간격보다 작아야 한다.**
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "10.0"))
	float QueueSlotArrivalRadiusUU = 45.f;

	// 시설 접근점(큐에 가입하기 전)의 도착 판정 반경, UU.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "10.0"))
	float FacilityApproachRadiusUU = 250.f;

	// 배회(시설이 아닌 목적지) 도착 판정 반경, UU.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "10.0"))
	float WanderArrivalRadiusUU = 400.f;

	// 같은 파티가 '도착'으로 다시 결정하기까지의 최소 간격, 초. 0이면 끈다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "0.0"))
	float DecisionCooldownSeconds = 2.f;

	UFUNCTION(BlueprintPure, Category = "Crowd|Flow")
	int32 GetDecisionCooldownSkips() const { return DecisionCooldownSkips; }

	UFUNCTION(BlueprintPure, Category = "Crowd|Flow")
	int32 GetDecisionsProcessed() const { return DecisionsProcessed; }

	// 목적지 앞에서 감속을 시작하는 거리, UU. 0이면 ArrivalRadiusUU * 3으로 폴백한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Sim", meta = (ClampMin = "0.0"))
	float BrakingDistanceUU = 450.f;

	// 회피를 켠다. ★ 기본값은 false다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	bool bEnableAvoidance = false;

	// 사회력 계수. 전부 SI(m, kg, N) 단위다 — 자세한 이유는 구조체 주석 참고.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	FFF_CrowdAvoidanceConfig AvoidanceConfig;

	// 시뮬 사각 경계(SimOrigin ± SimRadiusUU) 4벽을 벽 반발의 대상으로 넣는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	bool bAvoidanceUseBoundaryWalls = true;

	// 시설(부스·무대·화장실 등) footprint를 벽 반발의 대상으로 넣는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	bool bAvoidanceUseFacilityWalls = true;

	// 시설 벽에 여유(Clearance) 링까지 포함시킨다. 기본값은 **끔**(바닥면만).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	bool bAvoidanceFacilityWallsIncludeClearance = false;

	// 개체의 Z를 지형 높이에 맞춘다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Terrain")
	bool bEnableHeightField = true;

	// 높이 격자 꼭짓점 간격(UU). 작을수록 정확하고 빌드가 비싸다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Terrain")
	float HeightFieldSpacingUU = 200.f;

	// 목표 추종을 사회력의 **구동력**으로 계산한다. bEnableAvoidance가 켜져 있을 때만 의미가 있다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Avoidance")
	bool bAvoidanceDrivesVelocity = true;

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
	float MaxTurnRateDegrees = 360.f;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category = "Crowd|Need", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InitialNeedMaxFraction = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Need", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float InitialNeedModeFraction = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Need")
	FFF_NeedRiseRates NeedRiseRates;
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
	float Temp_ExitArrivalRadiusUU = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Temp")
	float Temp_LeaveTimeoutSeconds = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Need")
	float Temp_FestivalMinutesPerRealSecond = 1.f;
	UFUNCTION(BlueprintPure, Category = "Crowd|Need")
	float GetLiveMinutes() const { return SimTimeSeconds * Temp_FestivalMinutesPerRealSecond; }

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
	
	void BeginArrivalSchedule(int32 TotalVisitorCount, FVector SpawnCenter, float SpawnRadiusUU);
	
	int32 DrainVisitorUntil(int32 TargetCumulativeCount);
	
	void UpdateArrivalSchedule();

private:

	
	
	
	void StepSimulation(float FixedDt);

	void PushPresentationToActors(float DeltaTime, float InterpAlpha);

	void PublishCrowdReadModel();

	void UpdateAvoidanceMirrors(const CrowdSteering::FStepParams& Params, bool bWantDesiredVel);

	void ComputeAvoidanceForces(const CrowdSteering::FStepParams& Params);

	CrowdSteering::FStepParams MakeStepParams() const;

	void ClearAvoidanceForces();

public:
	int32 GetWallSegmentCount() const { return AvoidanceWalls.Num(); }
	int32 GetFacilityWallCount() const { return FacilityWallCount; }
	bool DidFacilityWallsSeeZoneSystem() const { return bFacilityWallsSawZoneSystem; }

	const FFF_CrowdHeightField& GetHeightField() const { return HeightField; }

	bool TryGetGroundZ(const FVector& WorldLocation, double& OutZ) const;

private:
	void SnapUnitToGround(FUnitData& Unit) const;

	void RebuildHeightField();
	void RebuildAvoidanceWalls();

	void EnsureFacilityWalls();

	int32 AppendFacilityWalls();

	FVector PickRandomDestination();

	void ProcessDecisions();

	FVector PickWanderDestination(int32 LeaderAgentIndex) const;

	FFF_VisitorDistribution BuildVisitorDistribution(int32 RequestedCount) const;
	FFF_NeedState MakeInitialNeeds(int32 StableAgentId) const;
	void AdvanceNeeds();

	FVector GetPartyFormationOffset(int32 SlotIndex) const;

	bool UpdatePartyMemberTarget(int32 AgentIndex, FUnitData& Unit);

	bool AdvancePartyQueueOnArrival(int32 LeaderAgentIndex);

	void ClearPartyQueueState(int32 LeaderAgentIndex, bool bLeaveQueue);

	void SampleQueueLiveness();

	void DrawDebugAgents() const;
	
	void DespawnAgentInternal(int32 AgentID);
	
public:
private:
	
	//스케줄 관련 함수
	TSoftObjectPtr<UDataTable> CachedArrivalCurveTable;
	
	FName CachedArrivalScenarioId;
	
	int32 CachedBucketCount = 0;
	
	FFF_VisitorDistribution CachedDistribution;
	
	int32 VisitorCursor = 0;
	
	int32 ArrivalTotalVisitorCount = 0;
	
	FVector ArrivalSpawnCenter = FVector::ZeroVector;
	
	float ArrivalSpawnRadiusUU = 0.f;
	
	bool bArrivalPoolExhaustedLogged = false;
	
	
	// 다음에 발급할 파티 ID. 슬롯 인덱스와 달리 재사용하지 않아 로그를 되짚을 수 있다
	int32 NextPartyID = 0;
	
	int32 NextStableAgentID = 0;

	static constexpr float FixedStepSeconds = 1.f / 30.f;

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

	// 결정·UI가 읽는 사본. 발행 주기 사이에는 값이 그대로 남아 있다
	FFF_CrowdReadModel CrowdReadModel;

	// 마지막 발행 이후 지난 고정 스텝 수. 프레임 수가 아니라 스텝 수다 — 프레임률로 세면 유량 집계 구간이 기기 성능에 따라 달라진다
	int32 StepsSinceSnapshot = 0;

	int32 SnapshotCount = 0;

	// --- 회피 상태 ------------------------------------------------------------------------- 미러 배열.

	TArray<FVector2f> ScratchPos2DMeters;
	TArray<FVector2f> ScratchVel2DMeters;
	TArray<float> ScratchRadiusMeters;
	TArray<uint8> ScratchActive;

	// 이번 스텝의 희망 속도(m/s). 구동력을 쓰는 스텝에만 채운다 — 비면 솔버가 구동력을 건너뛴다
	TArray<FVector2f> ScratchDesiredVel2DMeters;

	// 활성 개체의 조밀 인덱스. 오름차순으로 채운다 — 힘 합산 순서가 결정성의 전제다
	TArray<int32> ScratchActiveIds;

	// 이번 스텝의 회피 가속도(m/s^2). UU 변환 전 값이다
	TArray<FVector2f> ScratchAccelMps2;

	// 진단용 이웃 수(자기 자신 제외)
	TArray<int32> AvoidanceNeighbourCounts;

	FFF_CrowdSpatialGrid AvoidanceGrid;

	// 이웃 버퍼를 청크별로 들고 있으므로 스텝 간 재사용을 위해 멤버로 둔다
	FFF_CrowdForceSolver ForceSolver;

	// 경계 4벽(m). ConfigureSimArea에서 갱신된다
	TArray<FFF_CrowdWallSegment> AvoidanceWalls;

	// AvoidanceWalls를 마지막으로 지은 시점의 ZoneSystem WorldRevision.
	int64 FacilityWallsRevision = -1;

	// 마지막 재구축에서 벽으로 들어간 시설 수.
	int32 FacilityWallCount = 0;

	// 마지막 재구축 때 ZoneSystem을 찾았는가. 조용히 벽 없이 도는 것이 가장 위험하다
	bool bFacilityWallsSawZoneSystem = false;

	// 마지막 재구축이 어떤 손잡이 값으로 지어졌는지.
	bool bBuiltWithBoundaryWalls = false;
	bool bBuiltWithFacilityWalls = false;
	bool bBuiltWithClearance = false;

	// --- 지형 ------------------------------------------------------------------------------

	FFF_CrowdHeightField HeightField;

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
	UPROPERTY()
	TObjectPtr<UDataTable> CachedCoefficientTable;
	float StayDrainMultiplierAtMaxNeed = 1.f;
	int32 LeftPartyCount = 0;
	int32 ForcedLeaveCount = 0;

	// 대기열 계수기. 전부 파티 단위다(인원 단위가 아니다)
	int32 QueueJoinAttempts = 0;
	int32 QueueJoinAccepted = 0;
	int32 QueueJoinRejected = 0;
	int32 QueueFacilityUnresolved = 0;
	int32 QueueArrivalNotifies = 0;
	int32 QueueHoldTimeouts = 0;
	int32 QueueStalledSkips = 0;

	// 결정 churn 계수기. 쿨다운이 실제로 무엇을 막았는지 보려면 둘을 같이 봐야 한다
	int32 DecisionCooldownSkips = 0;
	int32 DecisionsProcessed = 0;

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
	
};
