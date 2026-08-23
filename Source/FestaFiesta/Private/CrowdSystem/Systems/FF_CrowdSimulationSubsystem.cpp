// Fill out your copyright notice in the Description page of Project Settings.

#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "NPC/IndividualVariation/FF_DeterministicRandom.h"
#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdSteering.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "DrawDebugHelpers.h"
#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "Engine/DataTable.h"
#include "NPC/Destination/FF_UtilityCoefficientHelpers.h"
#include "NPC/Party/FF_NeedRules.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Queue/FF_QueueTypes.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSFacilityActor.h"
#include "DataTables/FF_ArrivalCurveRow.h"
#include "Serialization/GameplayAbilityTargetingLocationInfoNetSerializer.h"

namespace
{
	constexpr int32 NoSpawnGeneration = 0;

	// 서비스를 마친 파티는 시설의 퇴장 지점으로 보내진다.
	constexpr float QueueExitMatchToleranceUU = 50.f;
}

void UFF_CrowdSimulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

}

void UFF_CrowdSimulationSubsystem::InitializeSimulation(FVector NewOrigin, float NewRadiusUU,
	int32 NewActorPoolSize, int32 NewAgentPoolSize, TSubclassOf<AFF_BaseCrowd> InCrowdActorClass)
{
	if (bSimulationInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Already initialized, ignoring"));
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] World is inValid"));
		return;
	}

	const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get();

	CachedVariationTable = Settings->IndividualVariationTable.LoadSynchronous();
	CachedCorrelationStrength = Settings->VariationCorrelationStrength;

	if (!CachedVariationTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] IndividualVariationTable 미설정 — 개체 변주가 전부 1.0으로 동작한다"));
	}

	GlobalSimulationSeed = Settings->GlobalSimulationSeed;
	Rng.Initialize(GlobalSimulationSeed);
	CachedGroupSizeTable = Settings->GroupSizeTable.LoadSynchronous();
	CachedChildCompositionTable = Settings->ChildCompositionTable.LoadSynchronous();
	CachedArchetypeTable = Settings->ArchetypeTable.LoadSynchronous();
	CachedGroupScopeId = Settings->GroupScopeId;
	CachedArchetypeScenarioId = Settings->ArchetypeScenarioId;
	CachedCoefficientTable = Settings->UtilityCoefficientTable.LoadSynchronous();
	CachedArrivalCurveTable = Settings->ArrivalCurveTable.LoadSynchronous();
	CachedArrivalScenarioId = Settings->ArrivalScenarioId;
	CachedBucketCount = 0;
	
	if (CachedCoefficientTable)
	{
		NeedRiseRates = FFF_NeedRules::LoadRiseRates(CachedCoefficientTable.Get());
		StayDrainMultiplierAtMaxNeed =
			FFF_NeedRules::LoadStayDrainMultiplierAtMaxNeed(CachedCoefficientTable.Get());
		FFF_NeedRules::LoadInitialNeedFractions(
			CachedCoefficientTable.Get(), InitialNeedModeFraction, InitialNeedMaxFraction);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] UtilityCoefficientTable 미설정 — Need·체류 계수가 손잡이 기본값으로 동작한다"));
	}
	if (!CachedGroupSizeTable || !CachedArchetypeTable)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] GroupSizeTable/ArchetypeTable 미설정 — 방문객 분포를 만들 수 없다. ")
			TEXT("전원 1인 파티·ArchetypeID=NAME_None으로 스폰되고 preference 항이 꺼진다"));
	}
	else if (!CachedChildCompositionTable)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] ChildCompositionTable 미설정 — 아동 0명으로 구성된다"));
	}
	
	if (const UDataTable* Table = CachedArrivalCurveTable.Get())
	{
		const FName ProbeRow(*FString::Printf(TEXT("%s_00"),*CachedArrivalScenarioId.ToString()));
		
		if (const FFF_ArrivalCurveRow* Row = Table->FindRow<FFF_ArrivalCurveRow>(ProbeRow,TEXT("ArrivalCurveBootstrap")))
		{
			CachedBucketCount = Row->BucketCount;
		}
	}
	
	if (CachedBucketCount <=0)
	{
		UE_LOG(LogTemp, Error,         TEXT("[CrowdSimulationSubsystem] 입장 곡선을 읽지 못했다 (ScenarioId='%s') — ")
		TEXT("일괄 스폰으로 폴백한다. Project Settings의 ArrivalCurveTable/ArrivalScenarioId 확인"),*CachedArrivalScenarioId.ToString());
		
	}

	{
		FString AvoidanceError;
		if (!AvoidanceConfig.Validate(AvoidanceError))
		{
			UE_LOG(LogTemp, Error,
				TEXT("[CrowdSimulationSubsystem] AvoidanceConfig가 모순이다 — %s"), *AvoidanceError);
		}
	}

	ActorPoolSize = FMath::Max(NewActorPoolSize, 0);
	AgentPoolSize = FMath::Max(NewAgentPoolSize, 0);
	CrowdActorClass = InCrowdActorClass;

	ConfigureSimArea(NewOrigin, NewRadiusUU);

	InitializeCrowdActorPool(*World, NewOrigin);
	InitializeCrowdAgentPool(*World, NewOrigin);

	PoolBinding.Initialise(CrowdPool.Num(), UnitDatas.Num());

	bSimulationInitialized = true;

	UE_LOG(LogTemp, Log, TEXT("[CrowdSimulationSubsystem] Initialized: %d actors, %d agents"),
		CrowdPool.Num(), UnitDatas.Num());
}

void UFF_CrowdSimulationSubsystem::ConfigureSimArea(FVector NewOrigin, float NewRadiusUU)
{
	SimOrigin = NewOrigin;
	SimRadiusUU = FMath::Max(NewRadiusUU, 100.f);

	const FVector GridOrigin = SimOrigin - FVector(SimRadiusUU, SimRadiusUU, 0.f);
	const FVector2D GridSize(SimRadiusUU * 2.f, SimRadiusUU * 2.f);
	DensityGrid.Build(GridOrigin, GridSize, DensityCellSizeUU);

	RebuildAvoidanceWalls();

	RebuildHeightField();

	if (!bTemp_ExitLocationSet)
	{
		Temp_ExitWorldLocation = SimOrigin + FVector(-SimRadiusUU * 0.8f, 0.f, 0.f);
	}
}

TStatId UFF_CrowdSimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_CrowdSimulationSubsystem, STATGROUP_Tickables);
}

void UFF_CrowdSimulationSubsystem::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Tick);

	Super::Tick(DeltaTime);

	if (!bSimulationInitialized)
	{
		return;
	}

	const int32 StepsThisFrame = CrowdSteering::ConsumeFixedSteps(
		SimAccumulator, DeltaTime, FixedStepSeconds, MaxStepsPerFrame);

	for (int32 Step = 0; Step < StepsThisFrame; ++Step)
	{
		StepSimulation(FixedStepSeconds);
		SimTimeSeconds += FixedStepSeconds;
	}

	ProcessPendingDespawns();
	UpdateArrivalSchedule();

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Density);
		DensityGrid.Update(UnitDatas);
	}

	StepsSinceSnapshot += StepsThisFrame;
	if (StepsSinceSnapshot >= FMath::Max(1, SnapshotIntervalSteps))
	{
		PublishCrowdReadModel();
		StepsSinceSnapshot = 0;
	}

	AdvanceNeeds();
	ProcessDecisions();

	const float InterpAlpha = FMath::Clamp(SimAccumulator / FixedStepSeconds, 0.f, 1.f);
	PushPresentationToActors(DeltaTime, InterpAlpha);

	if (bDrawDebugDensity)
	{
		DensityGrid.DrawDebug(GetWorld());
	}
	if (bDrawDebugAgents)
	{
		DrawDebugAgents();
	}
}

void UFF_CrowdSimulationSubsystem::BeginArrivalSchedule(int32 TotalVisitorCount, FVector SpawnCenter,
	float SpawnRadiusUU)
{
	CachedDistribution = BuildVisitorDistribution(TotalVisitorCount);
	VisitorCursor = 0;
	ArrivalTotalVisitorCount = CachedDistribution.ActualVisitorCount;
	ArrivalSpawnCenter = SpawnCenter;
	ArrivalSpawnRadiusUU = SpawnRadiusUU;
	bArrivalPoolExhaustedLogged = false;
	
	
	if (CachedDistribution.Visitors.Num() == 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] 방문객 분포가 비었다 — 입력 테이블이나 ScopeId('%s')를 확인할 것. 스폰하지 않는다"),
			*CachedGroupScopeId.ToString());
		return;
	}

	
	if (CachedDistribution.ActualVisitorCount < CachedDistribution.RequestedVisitorCount)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[CrowdSimulationSubsystem] 요청 %d명 중 %d명 — 그룹을 통째로만 만들어 %d명이 남았다"),
			CachedDistribution.RequestedVisitorCount, CachedDistribution.ActualVisitorCount,
			CachedDistribution.RequestedVisitorCount - CachedDistribution.ActualVisitorCount);
	}
	
}

int32 UFF_CrowdSimulationSubsystem::DrainVisitorUntil(int32 TargetCumulativeCount)
{
	UWorld* World = GetWorld();
	if (!IsValid(World)) { return 0;}
	
	int32 SpawnedThisCall = 0;
	bool bAgentPoolExhausted = false;
	
	int32 SpawnedCount = 0;
	int32 PartiesFormed = 0;
	int32 ChildrenSpawned = 0;
	const float BaseSpeed = FUnitData().Speed;
	
	while (VisitorCursor < CachedDistribution.Visitors.Num() && VisitorCursor < TargetCumulativeCount && !bAgentPoolExhausted)
	{
				const FFF_TaggedVisitor& LeadVisitor = CachedDistribution.Visitors[VisitorCursor];
		const int32 PlannedSize = bEnablePartyGrouping ? FMath::Max(LeadVisitor.TargetGroupSize, 1) : 1;
		const int32 PartySize = FMath::Min(PlannedSize, CachedDistribution.Visitors.Num() - VisitorCursor);
		checkf(!LeadVisitor.bIsDependentChildSlot || !bEnablePartyGrouping,
			TEXT("SpawnAgents: 그룹 선두가 아동 슬롯이다 — 빌더가 성인을 먼저 배출한다는 전제가 깨졌다. ")
			TEXT("아동이 리더가 되면 목적지를 상속할 대상이 없다"));
		const float Angle = Rng.FRandRange(0.f, 2.f * PI);
		const float Radius = ArrivalSpawnRadiusUU * FMath::Sqrt(Rng.FRand());
		const FVector Anchor = ArrivalSpawnCenter + FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.f);

		const FRotator PartyRotation(0.f, Rng.FRandRange(0.f, 360.f), 0.f);

		int32 LeaderIndex = INDEX_NONE;
		int32 PartyID = INDEX_NONE;

		for (int32 Slot = 0; Slot < PartySize; ++Slot)
		{
			const FFF_TaggedVisitor& Visitor = CachedDistribution.Visitors[VisitorCursor + Slot];
			checkf(!bEnablePartyGrouping || Visitor.TargetGroupSize == LeadVisitor.TargetGroupSize,
				TEXT("SpawnAgents: 그룹 %d번째 멤버의 TargetGroupSize(%d)가 선두(%d)와 다르다 — ")
				TEXT("빌더의 배출 순서가 바뀌었다. 태그 기준 재조합으로 바꿀 것"),
				Slot, Visitor.TargetGroupSize, LeadVisitor.TargetGroupSize);
			const FVector SpawnLocation = Anchor + GetPartyFormationOffset(Slot);

			const int32 AgentIndex = SpawnAgent(*World, SpawnLocation, PartyRotation);
			if (AgentIndex == INDEX_NONE)
			{
				bAgentPoolExhausted = true;
				break;
			}

			FUnitData& Unit = UnitDatas[AgentIndex];
			Unit.ArchetypeID = Visitor.ArchetypeId;
			Unit.bIsDependentChild = Visitor.bIsDependentChildSlot;
			if (Unit.bIsDependentChild)
			{
				++ChildrenSpawned;
			}
			if (Slot == 0)
			{
				LeaderIndex = AgentIndex;
				PartyID = Unit.PartyID;
				PartyLeaderByID.Add(PartyID, LeaderIndex);
			}
			else
			{
				Unit.PartyID = PartyID;
				Unit.LeaderAgentIndex = LeaderIndex;
				Unit.PartySlotIndex = Slot;
				Unit.TargetLocation = UnitDatas[LeaderIndex].TargetLocation + GetPartyFormationOffset(Slot);
			}
			checkf(Slot == 0 || Unit.PartyID == PartyID,
				TEXT("SpawnAgents: 멤버(Slot %d)의 PartyID가 확정되기 전에 개체 변주를 계산했다. "
					 "ResolveWithGroupSharing 호출을 if/else 블록 뒤로 옮길 것"), Slot);

			checkf(Unit.StableAgentID != INDEX_NONE,
				TEXT("SpawnAgents: Agent %d의 StableAgentID가 발급되지 않았다 (SpawnAgent 확인)"), AgentIndex);

			checkf(Slot != 0 || Unit.LeaderAgentIndex == AgentIndex,
				TEXT("SpawnAgents: 리더 %d가 자기 자신을 리더로 갖고 있지 않다"), AgentIndex);

			const FFF_ResolvedIndividualTraits Traits =
				FFF_IndividualVariationResolver::ResolveWithGroupSharing(
					CachedVariationTable.Get(),
					GetGlobalSimulationSeed(),
					Unit.StableAgentID,
					Unit.PartyID,
					0,
					CachedCorrelationStrength);
			const float MemberBoost = (Slot == 0) ? 1.f : FMath::Max(PartyMemberSpeedBoost,1.f);
			Unit.Speed = BaseSpeed * Traits.WalkSpeedMultiplier* MemberBoost;
			Unit.MaxSpeed = Unit.Speed;

			if (Slot == 0 && PartyRemainingStayMinutes.IsValidIndex(LeaderIndex))
			{
				const float StayShare = FFF_UtilityCoefficientHelpers::GetCoefficient(
					CachedCoefficientTable.Get(), TEXT("stay_duration_share_of_live_stage_mean"), 0.5f);
				PartyRemainingStayMinutes[LeaderIndex] =
					FMath::Max(Temp_LiveStageMinutes * StayShare * Traits.StayDurationMultiplier, 0.f);
			}

			
			++SpawnedCount;
		}

		if (LeaderIndex != INDEX_NONE)
		{
			++PartiesFormed;

			NextPartyID = PartyID + 1;
		}
		VisitorCursor += PartySize;
	}
	
	if (bAgentPoolExhausted && !bArrivalPoolExhaustedLogged)
	{
		bArrivalPoolExhaustedLogged = true;
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] Agent 풀 소진 — %d/%d명에서 멈췄다. ")
			TEXT("곡선 뒷부분이 잘려 분석 결과가 왜곡된다. AgentPoolSize를 UnitCount 이상으로 올릴 것"),
			VisitorCursor, ArrivalTotalVisitorCount);
	}
	
	return SpawnedCount;
}

void UFF_CrowdSimulationSubsystem::UpdateArrivalSchedule()
{
	if (CachedBucketCount <= 0 || ArrivalTotalVisitorCount <= 0) { return; }
	if (VisitorCursor >= CachedDistribution.Visitors.Num())      { return; }

	const UDataTable* Table = CachedArrivalCurveTable.Get();
	if (!Table) { return; }
	//현재 Progress 정도를 계산한다.
	const float Progress = FMath::Clamp(GetLiveMinutes() / 
		FMath::Max(Temp_LiveStageMinutes, KINDA_SMALL_NUMBER),0.f,1.f);
	//BucketIdx 각 시간 버킷이 어디까지인지를 구한다.
	const int32 BucketIdx = FMath::Min(FMath::FloorToInt(Progress*CachedBucketCount), CachedBucketCount -1);
	
	const FName RowName(*FString::Printf(TEXT("%s_%02d"), *CachedArrivalScenarioId.ToString(), BucketIdx));
	const FFF_ArrivalCurveRow* Row = Table->FindRow<FFF_ArrivalCurveRow>(RowName, TEXT("ArrivalCurve"));
	if (!Row) { return; }
	
	
	//현재 버킷의 시간 길이
	const float Span = FMath::Max(Row->ProgressEnd - Row->ProgressStart,KINDA_SMALL_NUMBER);
	//현재 버킷에서 어디까지 진행되었는지의 정도
	const float LocalT = FMath::Clamp((Progress - Row->ProgressStart) / Span, 0.f, 1.f);
	//CumPct = 이번 함수 진행에서 몇 %만큼 들어와야 하는가?
	const float CumPct = (Row->CumulativeSharePct - Row->ArrivalSharePct) + Row->ArrivalSharePct * LocalT;
	//실제 스폰해야 하는 개수
	const int32 Target = FMath::FloorToInt(CumPct / 100.f * ArrivalTotalVisitorCount);
	
	DrainVisitorUntil(Target);
	
	
	
}

void UFF_CrowdSimulationSubsystem::StepSimulation(float FixedDt)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Step);

	const float VelocityAlpha = CrowdSteering::ComputeVelocityAlpha(VelocitySmoothTime, FixedDt);

	CrowdSteering::FStepParams StepParams = MakeStepParams();

	const bool bDriveByForce = bEnableAvoidance && bAvoidanceDrivesVelocity;
	StepParams.bDrivingForceInForce = bDriveByForce;

	SampleQueueLiveness();

	if (bEnableAvoidance)
	{
		ComputeAvoidanceForces(StepParams);
		bAvoidanceAppliedLastStep = true;
	}
	else if (bAvoidanceAppliedLastStep)
	{
		ClearAvoidanceForces();
		bAvoidanceAppliedLastStep = false;
	}

	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		FUnitData& Unit = UnitDatas[AgentIndex];
		if (!Unit.bIsActive)
		{
			continue;
		}

		Unit.ArrivalRadiusUU = ArrivalRadiusUU;
		if (Unit.LeaderAgentIndex == AgentIndex && PartyDecisionStates.IsValidIndex(AgentIndex))
		{
			const FPartyDecisionState& RadiusState = PartyDecisionStates[AgentIndex];

			if (RadiusState.bIsLeaving)
			{
				Unit.ArrivalRadiusUU = Temp_ExitArrivalRadiusUU;
			}
			else if (RadiusState.QueuePhase != EFF_PartyQueuePhase::None)
			{
				Unit.ArrivalRadiusUU = QueueSlotArrivalRadiusUU;
			}
			else if (!RadiusState.CurrentDestinationId.IsNone())
			{
				Unit.ArrivalRadiusUU = FacilityApproachRadiusUU;
			}
			else
			{
				Unit.ArrivalRadiusUU = WanderArrivalRadiusUU;
			}
		}

		if (Unit.LeaderAgentIndex == AgentIndex)
		{
			const FPartyDecisionState& State = PartyDecisionStates[AgentIndex];
			if (State.bIsLeaving)
			{
				const bool bReachedExit = CrowdSteering::HasArrived(Unit, Temp_ExitArrivalRadiusUU);
				const bool bTimedOut = (Temp_LeaveTimeoutSeconds > 0.f)
					&& (SimTimeSeconds - State.LeaveStartedSimTime > Temp_LeaveTimeoutSeconds);
				if (bReachedExit || bTimedOut)
				{
					DespawnParty(Unit.PartyID);
					++LeftPartyCount;
					if (bTimedOut)
					{
						++ForcedLeaveCount;
					}
				}
			}
			else if (CrowdSteering::HasArrived(Unit, Unit.ArrivalRadiusUU))
			{
				if (!AdvancePartyQueueOnArrival(AgentIndex))
				{
					RequestDecision(AgentIndex, EFF_DecisionReason::Arrived);
				}
			}
		}
		else
		{
			if (!UpdatePartyMemberTarget(AgentIndex, Unit))
			{
				continue;
			}
		}

		CrowdSteering::StepAgent(Unit, StepParams, VelocityAlpha, FixedDt);

		SnapUnitToGround(Unit);
	}
}

FFF_CrowdFlowBreakdown UFF_CrowdSimulationSubsystem::GetFlowBreakdown() const
{
	FFF_CrowdFlowBreakdown Out;

	const bool bUnstuckEnabled = (AvoidanceConfig.StuckDetectSteps > 0) && (AvoidanceConfig.UnstuckAccelMps2 > 0.f);

	for (const FUnitData& Unit : UnitDatas)
	{
		if (!Unit.bIsActive)
		{
			continue;
		}

		FVector ToTarget = Unit.TargetLocation - Unit.Position;
		ToTarget.Z = 0.f;
		const bool bWantsToMove = ToTarget.SizeSquared() > FMath::Square(Unit.ArrivalRadiusUU);

		if (!bWantsToMove)
		{
			++Out.ArrivedIdle;
			continue;
		}

		if (Unit.StuckSteps > 0)
		{
			++Out.Blocked;

			if (bUnstuckEnabled && Unit.StuckSteps >= AvoidanceConfig.StuckDetectSteps)
			{
				++Out.UnstuckActive;
			}
		}
		else
		{
			++Out.Walking;
		}

		Out.MaxStuckSteps = FMath::Max(Out.MaxStuckSteps, Unit.StuckSteps);
	}

	return Out;
}

void UFF_CrowdSimulationSubsystem::PublishCrowdReadModel()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_PublishReadModel);

	CrowdReadModel.CellSizeMeters = DensityGrid.GetCellSizeMeters();
	CrowdReadModel.GridOriginUU = DensityGrid.GetOriginUU();
	CrowdReadModel.SnapshotFrame = GFrameNumber;
	CrowdReadModel.SimTimeMinutes = GetLiveMinutes();

	CrowdReadModel.OccupiedCells.Reset();

	const TArray<int32>& Counts = DensityGrid.GetCellCounts();
	const TArray<int32>& Throughput = DensityGrid.GetCellThroughput();
	const TArray<float>& Density = DensityGrid.GetDensityField();
	const int32 NumX = FMath::Max(1, DensityGrid.GetNumX());

	for (int32 CellIndex = 0; CellIndex < Counts.Num(); ++CellIndex)
	{
		const int32 Occupancy = Counts[CellIndex];
		const int32 Flow = Throughput.IsValidIndex(CellIndex) ? Throughput[CellIndex] : 0;

		if (Occupancy == 0 && Flow == 0)
		{
			continue;
		}

		FFF_CrowdCellSample& Sample = CrowdReadModel.OccupiedCells.AddDefaulted_GetRef();
		Sample.CellX = CellIndex % NumX;
		Sample.CellY = CellIndex / NumX;
		Sample.Occupancy = Occupancy;
		Sample.Throughput = Flow;
		Sample.DensityPerSqm = Density.IsValidIndex(CellIndex) ? Density[CellIndex] : 0.f;
	}

	DensityGrid.ResetThroughputWindow();

	++SnapshotCount;
}

CrowdSteering::FStepParams UFF_CrowdSimulationSubsystem::MakeStepParams() const
{
	CrowdSteering::FStepParams Params;
	Params.MaxTurnRateDegrees = MaxTurnRateDegrees;
	Params.BrakingDistanceUU = (BrakingDistanceUU > 0.f)
		? BrakingDistanceUU
		: FMath::Max(ArrivalRadiusUU * 3.f, 1.f);
	Params.StuckSpeedThresholdUU = AvoidanceConfig.StuckSpeedThresholdUU;

	return Params;
}

void UFF_CrowdSimulationSubsystem::UpdateAvoidanceMirrors(const CrowdSteering::FStepParams& Params,
                                                          bool bWantDesiredVel)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_AvoidanceMirrors);

	const int32 PoolSize = UnitDatas.Num();

	ScratchPos2DMeters.Reset();
	ScratchPos2DMeters.SetNumUninitialized(PoolSize);
	ScratchVel2DMeters.Reset();
	ScratchVel2DMeters.SetNumUninitialized(PoolSize);
	ScratchRadiusMeters.Reset();
	ScratchRadiusMeters.SetNumUninitialized(PoolSize);
	ScratchActive.Reset();
	ScratchActive.SetNumUninitialized(PoolSize);
	ScratchActiveIds.Reset(PoolSize);

	ScratchDesiredVel2DMeters.Reset();
	if (bWantDesiredVel)
	{
		ScratchDesiredVel2DMeters.SetNumUninitialized(PoolSize);
	}

	for (int32 AgentIndex = 0; AgentIndex < PoolSize; ++AgentIndex)
	{
		const FUnitData& Unit = UnitDatas[AgentIndex];

		ScratchActive[AgentIndex] = Unit.bIsActive ? 1 : 0;
		ScratchPos2DMeters[AgentIndex] = FVector2f(
			static_cast<float>(Unit.Position.X) * FFF_CrowdUnits::MetersPerUU,
			static_cast<float>(Unit.Position.Y) * FFF_CrowdUnits::MetersPerUU);
		ScratchVel2DMeters[AgentIndex] = FVector2f(
			static_cast<float>(Unit.Velocity.X) * FFF_CrowdUnits::MetersPerUU,
			static_cast<float>(Unit.Velocity.Y) * FFF_CrowdUnits::MetersPerUU);
		ScratchRadiusMeters[AgentIndex] = Unit.Radius * FFF_CrowdUnits::MetersPerUU;

		if (bWantDesiredVel)
		{
			const FVector Desired = CrowdSteering::ComputeDesiredVelocity(Unit, Params);
			ScratchDesiredVel2DMeters[AgentIndex] = FVector2f(
				static_cast<float>(Desired.X) * FFF_CrowdUnits::MetersPerUU,
				static_cast<float>(Desired.Y) * FFF_CrowdUnits::MetersPerUU);
		}

		if (Unit.bIsActive)
		{
			ScratchActiveIds.Add(AgentIndex);
		}
	}
}

void UFF_CrowdSimulationSubsystem::ComputeAvoidanceForces(const CrowdSteering::FStepParams& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Avoidance);

	UpdateAvoidanceMirrors(Params, true);

	AvoidanceGrid.Build(ScratchPos2DMeters, ScratchActive, ScratchActiveIds,
		AvoidanceConfig.BroadphaseCellSizeMeters);

	FFF_CrowdForceInput Input;
	Input.PosMeters = &ScratchPos2DMeters;
	Input.VelMeters = &ScratchVel2DMeters;
	Input.RadiusMeters = &ScratchRadiusMeters;
	Input.ActiveIds = &ScratchActiveIds;

	Input.DesiredVelMeters = &ScratchDesiredVel2DMeters;

	Input.bApplyDrivingForce = Params.bDrivingForceInForce;

	EnsureFacilityWalls();

	ForceSolver.ComputeAll(UnitDatas, Input, AvoidanceGrid, AvoidanceWalls,
		AvoidanceConfig, ScratchAccelMps2, &AvoidanceNeighbourCounts);

	for (const int32 AgentIndex : ScratchActiveIds)
	{
		if (!ScratchAccelMps2.IsValidIndex(AgentIndex))
		{
			continue;
		}
		const FVector2f& Accel = ScratchAccelMps2[AgentIndex];
		UnitDatas[AgentIndex].Force = FVector(
			Accel.X * FFF_CrowdUnits::UUPerMeter,
			Accel.Y * FFF_CrowdUnits::UUPerMeter,
			0.f);
	}
}

void UFF_CrowdSimulationSubsystem::ClearAvoidanceForces()
{
	for (FUnitData& Unit : UnitDatas)
	{
		Unit.Force = FVector::ZeroVector;
		Unit.StuckSteps = 0;
	}
	AvoidanceNeighbourCounts.Reset();

	ScratchDesiredVel2DMeters.Reset();
}

void UFF_CrowdSimulationSubsystem::RebuildAvoidanceWalls()
{
	AvoidanceWalls.Reset();

	if (bAvoidanceUseBoundaryWalls)
	{
		const float CX = static_cast<float>(SimOrigin.X) * FFF_CrowdUnits::MetersPerUU;
		const float CY = static_cast<float>(SimOrigin.Y) * FFF_CrowdUnits::MetersPerUU;
		const float H = SimRadiusUU * FFF_CrowdUnits::MetersPerUU;

		const FVector2f Corners[4] = {
			FVector2f(CX - H, CY - H), FVector2f(CX + H, CY - H),
			FVector2f(CX + H, CY + H), FVector2f(CX - H, CY + H)
		};

		for (int32 i = 0; i < 4; ++i)
		{
			FFF_CrowdWallSegment Wall;
			Wall.A = Corners[i];
			Wall.B = Corners[(i + 1) % 4];
			AvoidanceWalls.Add(Wall);
		}
	}

	FacilityWallCount = AppendFacilityWalls();

	bBuiltWithBoundaryWalls = bAvoidanceUseBoundaryWalls;
	bBuiltWithFacilityWalls = bAvoidanceUseFacilityWalls;
	bBuiltWithClearance = bAvoidanceFacilityWallsIncludeClearance;
}

void UFF_CrowdSimulationSubsystem::RebuildHeightField()
{
	HeightField.Reset();

	if (!bEnableHeightField)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrowdHeightField), false);

	int32 IgnoredFacilities = 0;
	if (const UFFZSWorldSubsystem* ZoneSystem = World->GetSubsystem<UFFZSWorldSubsystem>())
	{
		const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
		for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
		{
			if (const AFFZSFacilityActor* Actor = ZoneSystem->FindFacility(Facility.FacilityInstanceId))
			{
				Params.AddIgnoredActor(Actor);
				++IgnoredFacilities;
			}
		}
	}

	for (const AFF_BaseCrowd* PooledActor : CrowdPool)
	{
		if (IsValid(PooledActor))
		{
			Params.AddIgnoredActor(PooledActor);
		}
	}

	constexpr double TraceAboveCm = 0.0;
	constexpr double TraceBelowCm = 50000.0;

	const double StartZ = SimOrigin.Z + TraceAboveCm;
	const double EndZ = SimOrigin.Z - TraceBelowCm;

	int32 TraceCount = 0;

	auto Sampler = [&](const FVector2D& QueryCm, double& OutHeightCm) -> bool
	{
		++TraceCount;

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(
			Hit,
			FVector(QueryCm.X, QueryCm.Y, StartZ),
			FVector(QueryCm.X, QueryCm.Y, EndZ),
			ECC_WorldStatic,
			Params);

		if (!bHit)
		{
			return false;
		}

		OutHeightCm = Hit.ImpactPoint.Z;
		return true;
	};

	const double SpacingCm = FMath::Max(static_cast<double>(HeightFieldSpacingUU), 10.0);
	const bool bBuilt = HeightField.Build(
		FVector2D(SimOrigin.X, SimOrigin.Y), SimRadiusUU, SpacingCm, Sampler);

	const FFF_CrowdHeightField::FBuildStats& Stats = HeightField.GetStats();

	if (!bBuilt)
	{
		UE_LOG(LogTemp, Log,
			TEXT("높이 격자: 지면을 찾지 못했다(트레이스 %d회) — 개체 Z는 스폰 값 그대로 유지된다"),
			TraceCount);
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("높이 격자: %dx%d, 적중 %d/%d, 높이 %.0f~%.0f (기복 %.0fcm), 시설 %d개 제외"),
		HeightField.GetNumX(), HeightField.GetNumY(),
		Stats.HitCount, Stats.SampleCount,
		Stats.MinHeightCm, Stats.MaxHeightCm, Stats.HeightSpanCm(),
		IgnoredFacilities);
}

bool UFF_CrowdSimulationSubsystem::TryGetGroundZ(const FVector& WorldLocation, double& OutZ) const
{
	return HeightField.TryGetHeightCm(FVector2D(WorldLocation.X, WorldLocation.Y), OutZ);
}

void UFF_CrowdSimulationSubsystem::SnapUnitToGround(FUnitData& Unit) const
{
	double GroundZ = 0.0;
	if (TryGetGroundZ(Unit.Position, GroundZ))
	{
		Unit.Position.Z = GroundZ;
	}
}

int32 UFF_CrowdSimulationSubsystem::AppendFacilityWalls()
{
	bFacilityWallsSawZoneSystem = false;

	if (!bAvoidanceUseFacilityWalls)
	{
		return 0;
	}

	UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!ZoneSystem)
	{
		return 0;
	}
	bFacilityWallsSawZoneSystem = true;

	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();

	int32 Appended = 0;
	int32 MissingActors = 0;

	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		const AFFZSFacilityActor* Actor = ZoneSystem->FindFacility(Facility.FacilityInstanceId);
		if (!IsValid(Actor))
		{
			++MissingActors;
			continue;
		}

		const FFFZSFacilitySpec Spec = Actor->GetFacilitySpec();

		float HalfXCm = static_cast<float>(Spec.FootprintSizeCells.X) * 0.5f;
		float HalfYCm = static_cast<float>(Spec.FootprintSizeCells.Y) * 0.5f;

		if (bAvoidanceFacilityWallsIncludeClearance)
		{
			const float ClearanceCm = static_cast<float>(Spec.ClearanceCells);
			HalfXCm += ClearanceCm;
			HalfYCm += ClearanceCm;
		}

		const FVector2f CenterMeters(
			static_cast<float>(Facility.WorldLocation.X) * FFF_CrowdUnits::MetersPerUU,
			static_cast<float>(Facility.WorldLocation.Y) * FFF_CrowdUnits::MetersPerUU);

		const float YawRadians = FMath::DegreesToRadians(
			static_cast<float>(Actor->GetActorRotation().Yaw));

		FFF_CrowdWalls::AppendOrientedRect(AvoidanceWalls, CenterMeters, YawRadians,
			HalfXCm * FFF_CrowdUnits::MetersPerUU, HalfYCm * FFF_CrowdUnits::MetersPerUU);

		++Appended;
	}

	if (MissingActors > 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("시설 벽: %d개 시설의 액터를 찾지 못해 벽에서 빠졌다 — 그 자리는 통과된다"),
			MissingActors);
	}

	return Appended;
}

void UFF_CrowdSimulationSubsystem::EnsureFacilityWalls()
{
	const bool bKnobsChanged =
		(bBuiltWithBoundaryWalls != bAvoidanceUseBoundaryWalls) ||
		(bBuiltWithFacilityWalls != bAvoidanceUseFacilityWalls) ||
		(bBuiltWithClearance != bAvoidanceFacilityWallsIncludeClearance);

	if (!bAvoidanceUseFacilityWalls)
	{
		if (bKnobsChanged)
		{
			FacilityWallsRevision = -1;
			RebuildAvoidanceWalls();
		}
		return;
	}

	const UWorld* World = GetWorld();
	const UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!ZoneSystem)
	{
		if (bKnobsChanged)
		{
			RebuildAvoidanceWalls();
		}
		return;
	}

	const int64 Revision = ZoneSystem->GetWorldRevision();
	if (Revision == FacilityWallsRevision && !bKnobsChanged)
	{
		return;
	}

	FacilityWallsRevision = Revision;
	RebuildAvoidanceWalls();

	UE_LOG(LogTemp, Log,
		TEXT("시설 벽 재구축: 리비전 %lld, 시설 %d개, 선분 %d개(경계 포함)"),
		Revision, FacilityWallCount, AvoidanceWalls.Num());
}

void UFF_CrowdSimulationSubsystem::PushPresentationToActors(float DeltaTime, float InterpAlpha)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Presentation);

	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		const FUnitData& Unit = UnitDatas[AgentIndex];
		if (!Unit.bIsActive)
		{
			continue;
		}

		const int32 ActorSlot = PoolBinding.GetBoundSlot(AgentIndex);
		if (ActorSlot == INDEX_NONE || !CrowdPool.IsValidIndex(ActorSlot))
		{
			continue;
		}

		AFF_BaseCrowd* Actor = CrowdPool[ActorSlot];
		if (!IsValid(Actor))
		{
			continue;
		}

		const FVector RenderPosition = FMath::Lerp(Unit.PrevPosition, Unit.Position, InterpAlpha);
		Actor->PushPresentation(RenderPosition, Unit.Rotation, Unit.Velocity.Size());
	}
}

FVector UFF_CrowdSimulationSubsystem::PickRandomDestination()
{
	const float Angle = Rng.FRandRange(0.f, 2.f * PI);
	const float Radius = SimRadiusUU * FMath::Sqrt(Rng.FRand());

	return SimOrigin + FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.f);
}

void UFF_CrowdSimulationSubsystem::RequestDecision(int32 LeaderAgentIndex, EFF_DecisionReason Reason)
{
	if (!PartyDecisionStates.IsValidIndex(LeaderAgentIndex))
	{
		return;
	}

	FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];

	if (State.bIsLeaving)
	{
		ClearPartyQueueState(LeaderAgentIndex, true);
		return;
	}

	if (Reason == EFF_DecisionReason::Arrived
		&& DecisionCooldownSeconds > 0.f
		&& State.LastDecisionSimTime > 0.f
		&& (SimTimeSeconds - State.LastDecisionSimTime) < DecisionCooldownSeconds)
	{
		++DecisionCooldownSkips;
		return;
	}

	ClearPartyQueueState(LeaderAgentIndex, false);

	if (Reason == EFF_DecisionReason::Arrived
		&& !State.CurrentDestinationId.IsNone()
		&& PartyVisitHistories.IsValidIndex(LeaderAgentIndex))
	{
		PartyVisitHistories[LeaderAgentIndex].RecordVisit(State.CurrentDestinationId, GetLiveMinutes());
		State.CurrentDestinationId = NAME_None;
	}
	State.bNeedsDecision = true;

	State.PendingReason = Reason;

	if (!State.bQueued)
	{
		State.bQueued = true;
		DecisionQueue.Add(LeaderAgentIndex);
	}
}

void UFF_CrowdSimulationSubsystem::SampleQueueLiveness()
{
	const UWorld* World = GetWorld();
	const UFF_QueueSubsystem* Queue = World ? World->GetSubsystem<UFF_QueueSubsystem>() : nullptr;
	if (!Queue)
	{
		LastQueueAdvanceSimTime = SimTimeSeconds;
		return;
	}

	const float QueueSimTime = Queue->GetSimTimeSeconds();
	if (QueueSimTime > LastObservedQueueSimTime)
	{
		LastObservedQueueSimTime = QueueSimTime;
		LastQueueAdvanceSimTime = SimTimeSeconds;
	}
}

bool UFF_CrowdSimulationSubsystem::IsQueueAdvancing() const
{
	if (QueueStallToleranceSeconds <= 0.f)
	{
		return true;
	}

	return (SimTimeSeconds - LastQueueAdvanceSimTime) <= QueueStallToleranceSeconds;
}

void UFF_CrowdSimulationSubsystem::ClearPartyQueueState(int32 LeaderAgentIndex, bool bLeaveQueue)
{
	if (!PartyDecisionStates.IsValidIndex(LeaderAgentIndex) || !UnitDatas.IsValidIndex(LeaderAgentIndex))
	{
		return;
	}

	FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];
	if (State.QueuePhase == EFF_PartyQueuePhase::None)
	{
		return;
	}

	if (bLeaveQueue && State.QueuedFacilityId.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UFF_ZoneSystemQueueBridgeSubsystem* Bridge = World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>())
			{
				Bridge->LeaveZoneFacilityQueue(State.QueuedFacilityId, UnitDatas[LeaderAgentIndex].PartyID);
			}
		}
	}

	State.QueuePhase = EFF_PartyQueuePhase::None;
	State.QueuedFacilityId.Invalidate();
}

bool UFF_CrowdSimulationSubsystem::AdvancePartyQueueOnArrival(int32 LeaderAgentIndex)
{
	if (!PartyDecisionStates.IsValidIndex(LeaderAgentIndex)
		|| !UnitDatas.IsValidIndex(LeaderAgentIndex))
	{
		return false;
	}

	if (!bEnableQueueJoin)
	{
		if (PartyDecisionStates[LeaderAgentIndex].QueuePhase != EFF_PartyQueuePhase::None)
		{
			ClearPartyQueueState(LeaderAgentIndex, true);
		}
		return false;
	}

	UWorld* World = GetWorld();
	UFF_ZoneSystemQueueBridgeSubsystem* Bridge =
		World ? World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>() : nullptr;

	if (!Bridge)
	{
		return false;
	}

	FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];
	const FUnitData& Leader = UnitDatas[LeaderAgentIndex];

	if (!IsQueueAdvancing())
	{
		if (State.QueuePhase != EFF_PartyQueuePhase::None)
		{
			ClearPartyQueueState(LeaderAgentIndex, true);
		}

		++QueueStalledSkips;
		return false;
	}

	if (State.QueuePhase != EFF_PartyQueuePhase::None
		&& QueueHoldTimeoutSeconds > 0.f
		&& (SimTimeSeconds - State.QueueEnteredSimTime) > QueueHoldTimeoutSeconds)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] 파티 %d가 대기열에 %.0f초 넘게 묶여 있어 강제 해제한다 — 큐 이벤트 유실을 의심할 것"),
			Leader.PartyID, QueueHoldTimeoutSeconds);

		++QueueHoldTimeouts;
		ClearPartyQueueState(LeaderAgentIndex, true);
		return false;
	}

	switch (State.QueuePhase)
	{
	case EFF_PartyQueuePhase::None:
	{
		if (State.CurrentDestinationId.IsNone())
		{
			return false;
		}

		FGuid ZoneFacilityId;
		if (!FFF_ZoneSystemBridge::TryParseDestinationId(State.CurrentDestinationId, ZoneFacilityId))
		{
			++QueueFacilityUnresolved;
			return false;
		}

		if (Bridge->FindQueueFacilityId(ZoneFacilityId) == INDEX_NONE)
		{
			++QueueFacilityUnresolved;
			return false;
		}

		const TArray<int32> Members = GetPartyMembers(Leader.PartyID);

		FFF_JoinRequest Request;
		Request.PartyId = Leader.PartyID;
		Request.MemberCount = FMath::Max(Members.Num(), 1);
		Request.MemberAgentIds = Members;
		Request.CauseId = State.CurrentDestinationId;

		++QueueJoinAttempts;

		const FFF_QueueJoinResponse Response = Bridge->TryJoinZoneFacility(ZoneFacilityId, Request);
		if (!Response.bAccepted)
		{
			++QueueJoinRejected;
			return false;
		}

		++QueueJoinAccepted;
		State.QueuePhase = EFF_PartyQueuePhase::Joined;
		State.QueuedFacilityId = ZoneFacilityId;
		State.QueueEnteredSimTime = SimTimeSeconds;
		return true;
	}

	case EFF_PartyQueuePhase::Joined:
	{
		Bridge->NotifyZoneFacilityPartyArrived(State.QueuedFacilityId, Leader.PartyID);
		++QueueArrivalNotifies;

		State.QueuePhase = EFF_PartyQueuePhase::AtSlot;
		return true;
	}

	case EFF_PartyQueuePhase::AtSlot:
	{
		const UFF_QueueSubsystem* Queue = World->GetSubsystem<UFF_QueueSubsystem>();
		const int32 QueueFacilityId = Bridge->FindQueueFacilityId(State.QueuedFacilityId);

		if (Queue && QueueFacilityId != INDEX_NONE)
		{
			const FVector ExitLocation = Queue->GetFacilityExitLocation(QueueFacilityId);
			if (!ExitLocation.IsNearlyZero()
				&& FVector::Dist2D(Leader.TargetLocation, ExitLocation) <= QueueExitMatchToleranceUU)
			{
				ClearPartyQueueState(LeaderAgentIndex, false);
				return false;
			}
		}

		return true;
	}

	default:
		return false;
	}
}

int32 UFF_CrowdSimulationSubsystem::GetQueuedPartyCount() const
{
	int32 Count = 0;
	for (int32 AgentIndex = 0; AgentIndex < PartyDecisionStates.Num(); ++AgentIndex)
	{
		if (UnitDatas.IsValidIndex(AgentIndex)
			&& UnitDatas[AgentIndex].bIsActive
			&& UnitDatas[AgentIndex].LeaderAgentIndex == AgentIndex
			&& PartyDecisionStates[AgentIndex].QueuePhase != EFF_PartyQueuePhase::None)
		{
			++Count;
		}
	}
	return Count;
}

void UFF_CrowdSimulationSubsystem::ResetQueueStats()
{
	QueueJoinAttempts = 0;
	QueueJoinAccepted = 0;
	QueueJoinRejected = 0;
	QueueFacilityUnresolved = 0;
	QueueArrivalNotifies = 0;
	QueueHoldTimeouts = 0;
	QueueStalledSkips = 0;
}

FVector UFF_CrowdSimulationSubsystem::PickWanderDestination(int32 LeaderAgentIndex) const
{
	const FUnitData& Unit = UnitDatas[LeaderAgentIndex];
	const FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];

	const uint32 SeedAngle = FFF_DeterministicRandom::MakeDecisionSeed(
		GlobalSimulationSeed, Unit.StableAgentID, State.ChoiceRoundIndex * 2);
	const uint32 SeedRadius = FFF_DeterministicRandom::MakeDecisionSeed(
		GlobalSimulationSeed, Unit.StableAgentID, State.ChoiceRoundIndex * 2 + 1);

	const float Angle = FFF_DeterministicRandom::MakeUniformFraction(SeedAngle) * 2.f * PI;
	const float Radius = WanderRadiusUU * FMath::Sqrt(FFF_DeterministicRandom::MakeUniformFraction(SeedRadius));

	FVector Destination = Unit.Position + FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.f);

	const FVector FromOrigin(Destination.X - SimOrigin.X, Destination.Y - SimOrigin.Y, 0.f);
	if (FromOrigin.SizeSquared() > FMath::Square(SimRadiusUU))
	{
		Destination = SimOrigin + FromOrigin.GetSafeNormal() * SimRadiusUU;
		Destination.Z = Unit.Position.Z;
	}

	return Destination;
}

void UFF_CrowdSimulationSubsystem::ProcessDecisions()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Decisions);

	if (DecisionQueue.IsEmpty())
	{
		return;
	}

	const int32 Budget = FMath::Max(MaxDecisionsPerFrame, 1);

	int32 Head = 0;
	int32 Processed = 0;

	while (Head < DecisionQueue.Num() && Processed < Budget)
	{
		const int32 LeaderIndex = DecisionQueue[Head++];

		if (!UnitDatas.IsValidIndex(LeaderIndex))
		{
			continue;
		}

		FUnitData& Unit = UnitDatas[LeaderIndex];
		FPartyDecisionState& State = PartyDecisionStates[LeaderIndex];

		State.bQueued = false;

		if (!Unit.bIsActive || Unit.LeaderAgentIndex != LeaderIndex || !State.bNeedsDecision)
		{
			continue;
		}

		FFF_PartyDecisionContext Context;
		Context.PartyId = Unit.PartyID;
		Context.StableAgentId = Unit.StableAgentID;
		Context.ChoiceRoundIndex = State.ChoiceRoundIndex;
		Context.PartyWorldLocation = Unit.Position;
		Context.Reason = State.PendingReason;
		Context.ArchetypeId = Unit.ArchetypeID;
		Context.ConsecutiveNoChoiceCount = State.ConsecutiveNoChoiceCount;
		Context.NowLiveMinutes = GetLiveMinutes();
		Context.Needs = PartyAggregatedNeeds.IsValidIndex(LeaderIndex)
			? PartyAggregatedNeeds[LeaderIndex]
			: Unit.Needs;
		Context.PartySize = PartyMemberCounts.IsValidIndex(LeaderIndex)
			? FMath::Max(PartyMemberCounts[LeaderIndex], 1)
			: 1;
		if (PartyVisitHistories.IsValidIndex(LeaderIndex))
		{
			Context.VisitHistory = PartyVisitHistories[LeaderIndex];
		}
		Context.RemainingStayMinutes = GetPartyRemainingStayMinutes(LeaderIndex);
		FFF_PartyDecisionResult Result;

		const bool bDecided = OnPartyNeedsDecision.IsBound()
			&& OnPartyNeedsDecision.Execute(Context, Result);
		if (bDecided)
		{
			Unit.TargetLocation = Result.Destination;
		}
		else if (Result.FallbackAction == EFF_PartyFallbackAction::Leave)
		{
			State.bIsLeaving = true;
			State.LeaveStartedSimTime = SimTimeSeconds;
			Unit.TargetLocation = Temp_ExitWorldLocation;
			State.CurrentDestinationId = NAME_None;
			State.bNeedsDecision = false;
			State.LastDecisionSimTime = SimTimeSeconds;
			++Processed;
			++DecisionsProcessed;
			continue;
		}
		else
		{
			Unit.TargetLocation = PickWanderDestination(LeaderIndex);
		}
		State.CurrentDestinationId = bDecided ? Result.DestinationId : NAME_None;

		State.ConsecutiveNoChoiceCount = bDecided
			? 0
			: (Result.bCountAsNoChoice ? State.ConsecutiveNoChoiceCount + 1 : State.ConsecutiveNoChoiceCount);

		State.bNeedsDecision = false;
		State.LastDecisionSimTime = SimTimeSeconds;
		++State.ChoiceRoundIndex;
		++Processed;
		++DecisionsProcessed;
	}

	DecisionQueue.RemoveAt(0, Head, EAllowShrinking::No);
}

FVector UFF_CrowdSimulationSubsystem::GetPartyFormationOffset(int32 SlotIndex) const
{
	if (SlotIndex <= 0)
	{
		return FVector::ZeroVector;
	}

	constexpr float GoldenAngleDegrees = 137.5f;
	const float Angle = FMath::DegreesToRadians(GoldenAngleDegrees * SlotIndex);
	const float Radius = PartySpacingUU * FMath::Sqrt(static_cast<float>(SlotIndex));

	return FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.f);
}

bool UFF_CrowdSimulationSubsystem::UpdatePartyMemberTarget(int32 AgentIndex, FUnitData& Unit)
{
	const int32 LeaderIndex = Unit.LeaderAgentIndex;

	if (!UnitDatas.IsValidIndex(LeaderIndex)
		|| !UnitDatas[LeaderIndex].bIsActive
		|| UnitDatas[LeaderIndex].PartyID != Unit.PartyID)
	{
		ensureMsgf(false, TEXT("고아 멤버 %d (파티 %d) — DespawnParty가 일부만 처리했다"),
		AgentIndex, Unit.PartyID);
		DespawnParty(Unit.PartyID);
		return false;
	}

	const FUnitData& Leader = UnitDatas[LeaderIndex];
	const FVector Offset = GetPartyFormationOffset(Unit.PartySlotIndex);

	if (FVector::DistSquared2D(Unit.Position, Leader.Position) > FMath::Square(PartyCohesionRadiusUU))
	{
		Unit.TargetLocation = Leader.Position + Offset;
	}
	else
	{
		Unit.TargetLocation = Leader.TargetLocation + Offset;
	}

	return true;
}

int32 UFF_CrowdSimulationSubsystem::GetActivePartyCount() const
{
	int32 PartyCount = 0;
	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		if (UnitDatas[AgentIndex].bIsActive && UnitDatas[AgentIndex].LeaderAgentIndex == AgentIndex)
		{
			++PartyCount;
		}
	}
	return PartyCount;
}

TArray<int32> UFF_CrowdSimulationSubsystem::GetPartyMembers(int32 PartyID) const
{
	TArray<int32> Members;
	if (PartyID == INDEX_NONE)
	{
		return Members;
	}

	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		if (UnitDatas[AgentIndex].bIsActive && UnitDatas[AgentIndex].PartyID == PartyID)
		{
			Members.Add(AgentIndex);
		}
	}
	return Members;
}

void UFF_CrowdSimulationSubsystem::DespawnParty(int32 PartyID)
{
	if (PartyID == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] PartyID is InValid"));
		return;
	}

	PendingDespawnParty.Add(PartyID);

}

void UFF_CrowdSimulationSubsystem::ProcessPendingDespawns()
{
	if (PendingDespawnParty.IsEmpty())
	{
		return;
	}

	TSet<int32> DeleteParty = MoveTemp(PendingDespawnParty);
	PendingDespawnParty.Reset();

	for (int32 AgentIndex = 0;AgentIndex < UnitDatas.Num();++AgentIndex)
	{
		if (UnitDatas[AgentIndex].bIsActive && DeleteParty.Contains(UnitDatas[AgentIndex].PartyID))
		{
			DespawnAgentInternal(AgentIndex);
		}
	}

}

void UFF_CrowdSimulationSubsystem::SetAgentDestination(int32 AgentIndex, FVector Destination)
{
	if (!UnitDatas.IsValidIndex(AgentIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] SetAgentDestination: Invalid AgentIndex %d"), AgentIndex);
		return;
	}

	int32 TargetAgent = AgentIndex;
	const int32 LeaderIndex = UnitDatas[AgentIndex].LeaderAgentIndex;

	if (LeaderIndex != AgentIndex)
	{
		if (UnitDatas.IsValidIndex(LeaderIndex) && UnitDatas[LeaderIndex].bIsActive)
		{
			TargetAgent = LeaderIndex;
			UE_LOG(LogTemp, Verbose,
				TEXT("[CrowdSimulationSubsystem] SetAgentDestination: Agent %d는 파티 멤버다 — 리더 %d로 돌린다"),
				AgentIndex, LeaderIndex);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[CrowdSimulationSubsystem] SetAgentDestination: Agent %d의 리더 %d가 무효다 — 목적지가 반영되지 않는다"),
				AgentIndex, LeaderIndex);
		}
	}

	UnitDatas[TargetAgent].TargetLocation = Destination;
}

void UFF_CrowdSimulationSubsystem::BindAgentToPool(int32 AgentIndex)
{
	if (!UnitDatas.IsValidIndex(AgentIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Invalid AgentIndex"));
		return;
	}

	const int32 ActorSlot = PoolBinding.Bind(AgentIndex);
	if (ActorSlot == INDEX_NONE || !CrowdPool.IsValidIndex(ActorSlot))
	{
		return;
	}

	AFF_BaseCrowd* Actor = CrowdPool[ActorSlot];
	if (!IsValid(Actor))
	{
		PoolBinding.Release(AgentIndex);
		return;
	}

	Actor->BindToAgent(AgentIndex, UnitDatas[AgentIndex].Position, UnitDatas[AgentIndex].Rotation);
	Actor->SetActorHiddenInGame(false);
	Actor->SetActorEnableCollision(true);
}

void UFF_CrowdSimulationSubsystem::ReleaseAgentToPool(int32 AgentIndex)
{
	if (!UnitDatas.IsValidIndex(AgentIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Invalid AgentIndex"));
		return;
	}

	const int32 ActorSlot = PoolBinding.Release(AgentIndex);
	if (ActorSlot == INDEX_NONE)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[CrowdSimulationSubsystem] Agent %d is already released"), AgentIndex);
		return;
	}

	if (CrowdPool.IsValidIndex(ActorSlot) && IsValid(CrowdPool[ActorSlot]))
	{
		AFF_BaseCrowd* Actor = CrowdPool[ActorSlot];
		Actor->ReleaseFromAgent();
		Actor->SetActorHiddenInGame(true);
		Actor->SetActorEnableCollision(false);
	}
}

void UFF_CrowdSimulationSubsystem::SetTemp_ExitWorldLocation(FVector NewExitWorldLocation)
{
	Temp_ExitWorldLocation = NewExitWorldLocation;
	bTemp_ExitLocationSet = true;
	if (NewExitWorldLocation.Equals(SimOrigin, 1.f))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] 퇴장 지점이 스폰 원점과 같다 — 퇴장 동선이 생기지 않는다. ")
			TEXT("AFF_SpawnActor의 Temp_ExitOffsetFromOrigin을 확인할 것"));
	}
}
int32 UFF_CrowdSimulationSubsystem::GetLeavingPartyCount() const
{
	int32 Count = 0;
	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		const FUnitData& Unit = UnitDatas[AgentIndex];
		if (Unit.bIsActive && Unit.LeaderAgentIndex == AgentIndex
			&& PartyDecisionStates.IsValidIndex(AgentIndex) && PartyDecisionStates[AgentIndex].bIsLeaving)
		{
			++Count;
		}
	}
	return Count;
}
FFF_VisitorDistribution UFF_CrowdSimulationSubsystem::BuildVisitorDistribution(int32 RequestedCount) const
{
	return FFF_VisitorDistributionBuilder::Build(
		CachedGroupSizeTable.Get(),
		CachedChildCompositionTable.Get(),
		CachedArchetypeTable.Get(),
		CachedGroupScopeId,
		CachedArchetypeScenarioId,
		RequestedCount,
		GlobalSimulationSeed);
}
FFF_NeedState UFF_CrowdSimulationSubsystem::MakeInitialNeeds(int32 StableAgentId) const
{
	return FFF_NeedRules::MakeInitialNeeds(
		GlobalSimulationSeed,
		StableAgentId,
		NoSpawnGeneration,
		NeedRiseRates,
		InitialNeedModeFraction,
		InitialNeedMaxFraction);
}
void UFF_CrowdSimulationSubsystem::AdvanceNeeds()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Needs);
	const float ElapsedMinutes =
		(SimTimeSeconds - LastNeedAdvanceSimTime) * Temp_FestivalMinutesPerRealSecond;
	LastNeedAdvanceSimTime = SimTimeSeconds;
	for (FFF_NeedState& Aggregated : PartyAggregatedNeeds)
	{
		Aggregated = FFF_NeedState();
	}
	for (int32& MemberCount : PartyMemberCounts)
	{
		MemberCount = 0;
	}
	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		FUnitData& Unit = UnitDatas[AgentIndex];
		if (!Unit.bIsActive)
		{
			continue;
		}
		Unit.Needs = FFF_NeedRules::Advance(Unit.Needs, ElapsedMinutes, NeedRiseRates);
		const int32 LeaderIndex = Unit.LeaderAgentIndex;
		if (!PartyAggregatedNeeds.IsValidIndex(LeaderIndex))
		{
			continue;
		}
		PartyAggregatedNeeds[LeaderIndex] =
			FFF_NeedState::MakeAggregated(PartyAggregatedNeeds[LeaderIndex], Unit.Needs);
		++PartyMemberCounts[LeaderIndex];
	}
	if (ElapsedMinutes > 0.f)
	{
		for (int32 LeaderIndex = 0; LeaderIndex < PartyRemainingStayMinutes.Num(); ++LeaderIndex)
		{
			if (PartyMemberCounts[LeaderIndex] <= 0)
			{
				continue;
			}
			const FFF_StayConsumption Consumption = FFF_NeedRules::ConsumeStay(
				PartyAggregatedNeeds[LeaderIndex], ElapsedMinutes, StayDrainMultiplierAtMaxNeed);
			PartyRemainingStayMinutes[LeaderIndex] = FMath::Max(
				PartyRemainingStayMinutes[LeaderIndex] - Consumption.ConsumedStayBudgetMinutes, 0.f);
		}
	}
}
int32 UFF_CrowdSimulationSubsystem::SpawnAgent(UWorld& InWorld, FVector SpawnLocation, FRotator SpawnRotation)
{
	if (FreeAgentSlotIndex.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] No free agent slot available"));
		return INDEX_NONE;
	}

	int32 index = FreeAgentSlotIndex.Pop();

	FUnitData& Unit = UnitDatas[index];

	Unit = FUnitData();

	Unit.Position = SpawnLocation;

	SnapUnitToGround(Unit);

	Unit.PrevPosition = Unit.Position;
	Unit.Rotation = SpawnRotation;
	Unit.Velocity = FVector::ZeroVector;
	Unit.Force = FVector::ZeroVector;
	Unit.StableAgentID = NextStableAgentID++;
	Unit.Needs = MakeInitialNeeds(Unit.StableAgentID);

	Unit.TargetLocation = Unit.Position;

	Unit.PartyID = NextPartyID++;
	Unit.LeaderAgentIndex = index;
	Unit.PartySlotIndex = 0;

	Unit.bIsActive = true;
	++ActiveAgentCount;

	if (PartyDecisionStates.IsValidIndex(index))
	{
		PartyDecisionStates[index] = FPartyDecisionState();
	}

	if (PartyVisitHistories.IsValidIndex(index))
	{
		PartyVisitHistories[index].Reset();
	}
	RequestDecision(index, EFF_DecisionReason::Spawned);

	BindAgentToPool(index);

	ensureMsgf(ActiveAgentCount + FreeAgentSlotIndex.Num() == UnitDatas.Num(),
		TEXT("인원 보존 위반(스폰): active %d + free %d != pool %d"),
		ActiveAgentCount, FreeAgentSlotIndex.Num(), UnitDatas.Num());

	return index;
}

int32 UFF_CrowdSimulationSubsystem::SpawnAgents(int32 Count, FVector SpawnCenter, float SpawnRadiusUU)
{
	BeginArrivalSchedule(Count, SpawnCenter, SpawnRadiusUU);
	
	return DrainVisitorUntil(MAX_int32);
	
}

void UFF_CrowdSimulationSubsystem::InitializeCrowdActorPool(UWorld& InWorld, FVector PoolLocation)
{
	if (!IsValid(&InWorld))
	{
		UE_LOG(LogTemp,Warning, TEXT("[CrowdSimulationSubsystem] World is inValid"))
		return;
	}

	UClass* SpawnClass = CrowdActorClass ? CrowdActorClass.Get() : AFF_BaseCrowd::StaticClass();
	if (!CrowdActorClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] CrowdActorClass is not set - spawning bare AFF_BaseCrowd (no mesh, nothing will be visible)"));
	}

	CrowdPool.Reserve(ActorPoolSize);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i=0;i<ActorPoolSize;i++)
	{
		AFF_BaseCrowd* NewCrowd = InWorld.SpawnActor<AFF_BaseCrowd>(SpawnClass,PoolLocation,FRotator::ZeroRotator,SpawnParams);
		if (!IsValid(NewCrowd))
		{
			UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Failed to spawn crowd actor %d, pool size is %d"), i, i);
			break;
		}

		NewCrowd->SetActorHiddenInGame(true);
		NewCrowd->SetActorEnableCollision(false);
		CrowdPool.Add(NewCrowd);
	}
}

void UFF_CrowdSimulationSubsystem::InitializeCrowdAgentPool(UWorld& InWorld, FVector AgentPoolLocation)
{
	if (!IsValid(&InWorld))
	{
		UE_LOG(LogTemp,Warning, TEXT("[CrowdSimulationSubsystem] World is inValid"))
		return;
	}

	UnitDatas.Reserve(AgentPoolSize);
	FreeAgentSlotIndex.Reset(AgentPoolSize);

	for (int32 i=0;i<AgentPoolSize;i++)
	{
		FUnitData NewUnit;
		NewUnit.Position = AgentPoolLocation;
		NewUnit.PrevPosition = AgentPoolLocation;

		UnitDatas.Add(NewUnit);

	}

	for (int32 i = AgentPoolSize - 1; i >= 0; --i)
	{
		FreeAgentSlotIndex.Add(i);
	}

	PartyDecisionStates.Reset();
	PartyDecisionStates.SetNum(UnitDatas.Num());
	PartyVisitHistories.Reset();
	PartyVisitHistories.SetNum(UnitDatas.Num());
	PartyAggregatedNeeds.Reset();
	PartyAggregatedNeeds.SetNum(UnitDatas.Num());
	PartyMemberCounts.Reset();
	PartyMemberCounts.SetNumZeroed(UnitDatas.Num());
	PartyRemainingStayMinutes.Reset();
	PartyRemainingStayMinutes.SetNumZeroed(UnitDatas.Num());
	DecisionQueue.Reset();

	ActiveAgentCount = 0;
}

void UFF_CrowdSimulationSubsystem::DrawDebugAgents() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	static const FColor PartyColors[] = {
		FColor(255, 96, 96), FColor(96, 255, 96), FColor(96, 160, 255), FColor(255, 220, 80),
		FColor(220, 110, 255), FColor(80, 240, 240), FColor(255, 150, 60), FColor(180, 255, 140)
	};
	constexpr int32 NumPartyColors = UE_ARRAY_COUNT(PartyColors);

	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		const FUnitData& Unit = UnitDatas[AgentIndex];
		if (!Unit.bIsActive)
		{
			continue;
		}

		const bool bIsLeader = (Unit.LeaderAgentIndex == AgentIndex);
		const FColor PartyColor = (Unit.PartyID >= 0)
			? PartyColors[Unit.PartyID % NumPartyColors]
			: FColor::Cyan;

		DrawDebugCircle(World, Unit.Position + FVector(0, 0, 5.f), Unit.Radius, 12, PartyColor,
			false, -1.f, 0, bIsLeader ? 3.f : 1.f, FVector(1, 0, 0), FVector(0, 1, 0), false);

		DrawDebugLine(World, Unit.Position, Unit.Position + Unit.Velocity * 0.5f, FColor::Yellow, false, -1.f, 0, 2.f);

		if (bIsLeader)
		{
			DrawDebugLine(World, Unit.Position, Unit.TargetLocation, FColor(60, 60, 60), false, -1.f, 0, 0.5f);
		}
		else if (UnitDatas.IsValidIndex(Unit.LeaderAgentIndex))
		{
			DrawDebugLine(World, Unit.Position + FVector(0, 0, 5.f),
				UnitDatas[Unit.LeaderAgentIndex].Position + FVector(0, 0, 5.f), PartyColor, false, -1.f, 0, 1.5f);
		}
	}
}

void UFF_CrowdSimulationSubsystem::DespawnAgentInternal(int32 AgentID)
{
	if (!UnitDatas.IsValidIndex(AgentID))
	{
		return;
	}

	FUnitData& Unit = UnitDatas[AgentID];

	if (!Unit.bIsActive) return;

	ReleaseAgentToPool(AgentID);

	ClearPartyQueueState(AgentID, true);

	if (PartyLeaderByID.FindRef(Unit.PartyID) == AgentID)
	{
		PartyLeaderByID.Remove(Unit.PartyID);
	}

	Unit.bIsActive = false;
	--ActiveAgentCount;

	FreeAgentSlotIndex.Add(AgentID);
	Unit.PartyID = INDEX_NONE;
	Unit.LeaderAgentIndex = INDEX_NONE;

	if (PartyDecisionStates.IsValidIndex(AgentID))
	{
		PartyDecisionStates[AgentID] = FPartyDecisionState();
	}

	ensureMsgf(ActiveAgentCount + FreeAgentSlotIndex.Num() == UnitDatas.Num(),
	   TEXT("인원 보존 위반: active %d + free %d != pool %d"),
	   ActiveAgentCount, FreeAgentSlotIndex.Num(), UnitDatas.Num());

}
