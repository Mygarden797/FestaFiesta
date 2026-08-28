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
#include "NPC/Metrics/FF_NpcNeedMetricsSubsystem.h"
#include "NPC/Party/FF_NeedRules.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Queue/FF_QueueTypes.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSFacilityActor.h"
#include "DataTables/FF_ArrivalCurveRow.h"
#include "Serialization/GameplayAbilityTargetingLocationInfoNetSerializer.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"

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
		NeedReliefAmounts = FFF_NeedRules::LoadReliefAmounts(CachedCoefficientTable.Get());
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
	
	
	// Stage 가 없는 맵을 지원한다 — IsFestivalLive 가 같은 상황에서 true 를 돌려주는 것과 같은
	// 취급이다(단계 게이팅 없이 군중만 도는 테스트 맵). 여기서 확인하지 않으면 그 맵이 죽는다.
	UGameInstance* GameInstance = World->GetGameInstance();
	if (UFF_FestivalStageSubsystem* Stage =
		GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr)
	{
		Stage->OnStageChanged.AddUniqueDynamic(this, &UFF_CrowdSimulationSubsystem::HandleStageChanged);
		Stage->OnTimeScaleChange.AddDynamic(this,&UFF_CrowdSimulationSubsystem::GetTimeScale);
	}
}

void UFF_CrowdSimulationSubsystem::ConfigureSimArea(FVector NewOrigin, float NewRadiusUU)
{
	SimOrigin = NewOrigin;
	SimRadiusUU = FMath::Max(NewRadiusUU, 100.f);

	const FVector GridOrigin = SimOrigin - FVector(SimRadiusUU, SimRadiusUU, 0.f);
	const FVector2D GridSize(SimRadiusUU * 2.f, SimRadiusUU * 2.f);
	DensityGrid.Build(GridOrigin, GridSize, DensityCellSizeUU);

	RebuildAvoidanceWalls();
	UE_LOG(LogTemp, Warning,
	TEXT("[Crowd] FacilityWallCount = %d"),
	FacilityWallCount);

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

void UFF_CrowdSimulationSubsystem::GetTimeScale(float NewTimeScale)
{
	SimulationTimeScale = NewTimeScale;
	
	
}

void UFF_CrowdSimulationSubsystem::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Tick);

	Super::Tick(DeltaTime);

	if (!bSimulationInitialized)
	{
		return;
	}
	
	if (!IsFestivalLive())
	{
		return;
	}
	
	// 조회 시점에만 쓰이는 값이라 재빌드 없이 콘솔에서 바로 듣는다
	HeightField.SetCliffStepCm(static_cast<double>(HeightFieldCliffStepUU));
	ApplyWalkableCeiling();

	const float Time = DeltaTime * SimulationTimeScale;

	const int32 StepsThisFrame = CrowdSteering::ConsumeFixedSteps(
		SimAccumulator, Time, FixedStepSeconds, MaxStepsPerFrame);

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

	// Need 지표는 스냅샷과 같은 이유로 스텝 단위로 센다 — 프레임률로 세면 UI 갱신 간격이
	// 기기 성능에 따라 달라진다.
	StepsSinceNeedMetrics += StepsThisFrame;
	const bool bCommitNeedMetrics =
		StepsSinceNeedMetrics >= FMath::Max(1, NeedMetricsIntervalSteps);
	if (bCommitNeedMetrics)
	{
		StepsSinceNeedMetrics = 0;
	}

	AdvanceNeeds(bCommitNeedMetrics);
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
	float SpawnRadiusUU, const TArray<FFF_ReservedPartyRequest>& ReservedParties)
{
	// 다음 날 재개는 이 값을 그대로 다시 넘긴다. CachedDistribution.RequestedVisitorCount 를
	// 재사용하지 않는 것은 바로 아래에서 그 구조체를 덮기 때문이고, ActualVisitorCount 를
	// 쓰지 않는 것은 그룹을 통째로만 만드느라 요청보다 작아서 날마다 인원이 깎이기 때문이다.
	ArrivalRequestedVisitorCount = TotalVisitorCount;

	CachedDistribution = BuildVisitorDistribution(TotalVisitorCount, ReservedParties);
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

	if (CachedDistribution.RejectedReservedCount > 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] 예약 주문서 %d건이 규칙에 안 맞아 빠졌다 — 산 정원보다 손님이 적게 온다"),
			CachedDistribution.RejectedReservedCount);
	}

	if (CachedDistribution.ReservedGroupCount > 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[CrowdSimulationSubsystem] 예약 손님 %d명(%d그룹)이 명단 앞쪽에 실렸다"),
			CachedDistribution.ReservedVisitorCount, CachedDistribution.ReservedGroupCount);
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
		int32 MembersSpawnedThisParty = 0;

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
			const float MemberBoost2 = (Slot == 0) ? 1.f : FMath::Max(PartyMemberSpeedMultiplier,1.f);
			Unit.Speed = BaseSpeed * Traits.WalkSpeedMultiplier* MemberBoost;
			Unit.MaxSpeed = Unit.Speed * MemberBoost2;

			if (Slot == 0 && PartyRemainingStayMinutes.IsValidIndex(LeaderIndex))
			{
				const float StayShare = FFF_UtilityCoefficientHelpers::GetCoefficient(
					CachedCoefficientTable.Get(), TEXT("stay_duration_share_of_live_stage_mean"), 0.5f);
				PartyRemainingStayMinutes[LeaderIndex] =
					FMath::Max(Temp_LiveStageMinutes * StayShare * Traits.StayDurationMultiplier, 0.f);
			}


			++SpawnedCount;
			++MembersSpawnedThisParty;
		}

		if (LeaderIndex != INDEX_NONE)
		{
			++PartiesFormed;

			if (LeadVisitor.ReservationId.IsValid())
			{
				ConfirmReservedPartySpawn(LeadVisitor.ReservationId, PartyID, MembersSpawnedThisParty);
			}

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
	//현재 Progress 정도를 Stage의 시계에서 받아온다.
	const float Progress = GetLiveProgress01();
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

float UFF_CrowdSimulationSubsystem::GetLiveProgress01() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	if (UFF_FestivalStageSubsystem* Stage =
		GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr)
	{
		const float LiveDurationSeconds = Stage->GetLiveDurationForCurrentDay();
		if (LiveDurationSeconds > 0.f)
		{
			return FMath::Clamp(Stage->ElapsedLiveSeconds / LiveDurationSeconds, 0.f, 1.f);
		}
	}

	return FMath::Clamp(
		GetLiveMinutes() / FMath::Max(Temp_LiveStageMinutes, KINDA_SMALL_NUMBER), 0.f, 1.f);
}

bool UFF_CrowdSimulationSubsystem::IsFestivalLive()
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World->GetGameInstance();
	
	if (!GameInstance)
	{
		return false;
	}
	
	const UFF_FestivalStageSubsystem* Stage= GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>();
	return Stage ? (Stage->CurrentStage == EFF_FestivalStage::Live 
		|| Stage->CurrentStage == EFF_FestivalStage::Closing  ) : true;
}

void UFF_CrowdSimulationSubsystem::StepSimulation(float FixedDt)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Step);

	const float VelocityAlpha = CrowdSteering::ComputeVelocityAlpha(VelocitySmoothTime, FixedDt);

	CrowdSteering::FStepParams StepParams = MakeStepParams();

	const bool bDriveByForce = bEnableAvoidance && bAvoidanceDrivesVelocity;
	StepParams.bDrivingForceInForce = bDriveByForce;

	SampleQueueLiveness();

	UpdatePathFollowing();

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
			else if (DecisionStuckSteps > 0
				&& Unit.StuckSteps >= DecisionStuckSteps
				&& State.QueuePhase == EFF_PartyQueuePhase::None)
			{
				// ★ 큐 구간을 반드시 뺀다. 줄에 서 있으면 진척이 0인 것이 정상이고, 도착 반경도
				// 슬롯 기준(작다)이라 슬롯에 닿기 전까지 조향이 계속 막힘으로 센다. 이 조건이
				// 없으면 줄 선 파티가 임계마다 전원 이탈한다 — 줄에서의 이탈은 대기시간 임계
				// (reneging)가 판정할 몫이고 막힘이 대신할 수 없다.
				//
				// 리더만 걸린다(이 분기 자체가 리더 전용이다). 파티원은 리더를 따라가므로
				// 리더와 무관하게 막힘이 오를 수 있고, 멤버가 목적지를 고르면 안 된다.

				// 리셋이 먼저다. 쿨다운에 걸려 요청이 버려지면 다시 임계를 채워야 재시도한다 —
				// 그게 자연스러운 백오프이고, 리셋을 빼면 매 스텝 요청이 나가 큐가 넘친다.
				Unit.StuckSteps = 0;
				++StuckDecisionRequests;

				UE_LOG(LogTemp, Verbose,
					TEXT("[CrowdSimulationSubsystem] 파티 %d가 %d스텝 동안 목적지 방향으로 못 나아가 목적지를 포기한다"),
					Unit.PartyID, DecisionStuckSteps);

				RequestDecision(AgentIndex, EFF_DecisionReason::MoveFailed);
			}
		}
		else
		{
			if (!UpdatePartyMemberTarget(AgentIndex, Unit))
			{
				continue;
			}
		}

		// 통행 상한을 넘는 칸으로 들어가는 스텝은 무효로 돌린다.
		//
		// ★ 축을 나눠 미끄러뜨리지 않는다. 경계에 붙어 서고, 그 자리에서 StuckSteps가 임계에
		//   닿으면 MoveFailed 재결정이 목적지를 바꿔 준다 — 그 경로가 유일한 탈출구다.
		//
		// ★ StuckSteps를 직접 다시 세운다. StepAgent는 되돌리기 전 변위로 진척을 재므로,
		//   상한에 머리를 박는 개체는 매 스텝 '잘 나아갔다'로 판정되어 카운터가 0으로 리셋된다.
		//   그러면 재결정이 영원히 안 걸려 경계에 붙은 채 갇힌다. 실제 변위는 0이므로
		//   스텝 전 값 + 1이 맞다(도착해 서 있는 개체는 세지 않는다).
		//
		// ★ 스텝 전 위치가 이미 상한 밖일 때는 막지 않는다. 높은 곳에 스폰됐거나 손잡이를
		//   플레이 중에 올린 개체가 영구히 얼어붙는 것을 막는다 — 들어오는 것만 막고
		//   나가는 것은 허용한다. 그래서 새 위치를 먼저 보고, 막힐 때만 이전 위치를 되묻는다.
		const FVector PreStepPosition = Unit.Position;
		const int32 PreStepStuckSteps = Unit.StuckSteps;

		CrowdSteering::StepAgent(Unit, StepParams, VelocityAlpha, FixedDt);

		if (!HeightField.IsWalkable(FVector2D(Unit.Position.X, Unit.Position.Y))
			&& HeightField.IsWalkable(FVector2D(PreStepPosition.X, PreStepPosition.Y)))
		{
			Unit.Position = PreStepPosition;
			Unit.PrevPosition = PreStepPosition;
			Unit.Velocity = FVector::ZeroVector;

			FVector ToFinal = Unit.TargetLocation - PreStepPosition;
			ToFinal.Z = 0.f;

			Unit.StuckSteps = (ToFinal.SizeSquared() > FMath::Square(Unit.ArrivalRadiusUU))
				? PreStepStuckSteps + 1
				: 0;

			++HeightBlockedSteps;
		}

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

		const FVector FinalTarget = (Unit.LeaderAgentIndex != INDEX_NONE
			&& UnitDatas.IsValidIndex(Unit.LeaderAgentIndex)
			&& UnitDatas[Unit.LeaderAgentIndex].bIsActive)
			? UnitDatas[Unit.LeaderAgentIndex].TargetLocation
			: Unit.TargetLocation;

		FVector ToTarget = FinalTarget - Unit.Position;
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

void UFF_CrowdSimulationSubsystem::ApplyWalkableCeiling()
{
	if (MaxWalkableHeightAboveOriginUU > 0.f)
	{
		HeightField.SetWalkableCeilingCm(SimOrigin.Z + static_cast<double>(MaxWalkableHeightAboveOriginUU));
	}
	else
	{
		HeightField.ClearWalkableCeiling();
	}

	HeightField.SetBlockUnknownGround(bBlockUnknownGround);
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

	FVector2D CenterCm(SimOrigin.X, SimOrigin.Y);
	double HalfSizeCm = (HeightFieldRadiusUU > 0.f)
		? static_cast<double>(HeightFieldRadiusUU)
		: static_cast<double>(SimRadiusUU);
	double TopZ = SimOrigin.Z;
	bool bFittedToNav = false;

	if (bHeightFieldFitToNavBounds)
	{
		const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance() : nullptr;
		const FBox NavBounds = NavData ? NavData->GetBounds() : FBox(ForceInit);

		if (NavBounds.IsValid)
		{
			CenterCm = FVector2D(NavBounds.GetCenter().X, NavBounds.GetCenter().Y);
			HalfSizeCm = FMath::Max(NavBounds.GetSize().X, NavBounds.GetSize().Y) * 0.5;
			TopZ = FMath::Max(TopZ, NavBounds.Max.Z);
			bFittedToNav = true;
		}
	}

	constexpr double TraceBelowCm = 50000.0;

	const double TraceAboveCm = FMath::Max(static_cast<double>(HeightFieldTraceAboveUU), 0.0);

	const double StartZ = TopZ + TraceAboveCm;
	const double EndZ = FMath::Min(SimOrigin.Z, TopZ) - TraceBelowCm;

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

	HeightField.SetCliffStepCm(static_cast<double>(HeightFieldCliffStepUU));
	ApplyWalkableCeiling();

	const bool bBuilt = HeightField.Build(CenterCm, HalfSizeCm, SpacingCm, Sampler);

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

	UE_LOG(LogTemp, Log,
		TEXT("높이 격자: 중심 (%.0f, %.0f) 반경 %.0f (%s), 간격 %.0f, 트레이스 시작 Z=%.0f"),
		CenterCm.X, CenterCm.Y, HalfSizeCm,
		bFittedToNav ? TEXT("NavMesh 경계에 맞춤") : TEXT("SimRadius"),
		SpacingCm, StartZ);

	if (MaxWalkableHeightAboveOriginUU > 0.f)
	{
		UE_LOG(LogTemp, Log,
			TEXT("높이 격자: 통행 상한 Z=%.0f (SimOrigin.Z %.0f + %.0f), 지면 없는 칸은 %s"),
			SimOrigin.Z + static_cast<double>(MaxWalkableHeightAboveOriginUU),
			SimOrigin.Z, MaxWalkableHeightAboveOriginUU,
			bBlockUnknownGround ? TEXT("통행 불가") : TEXT("통행 가능"));
	}

	if (Stats.MaxHeightCm >= StartZ - 1.0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("높이 격자: 최고점 %.0f가 트레이스 시작면 %.0f에 닿았다 — 그보다 높은 지형은 ")
			TEXT("표본에서 빠져 개체가 파묻힌다. HeightFieldTraceAboveUU(현재 %.0f)를 올릴 것"),
			Stats.MaxHeightCm, StartZ, HeightFieldTraceAboveUU);
	}

	if (Stats.HitCount < Stats.SampleCount)
	{
		UE_LOG(LogTemp, Log,
			TEXT("높이 격자: 지면을 못 찾은 격자점 %d개 — 구멍이면 정상이고, 높은 지형이면 위 경고를 볼 것"),
			Stats.SampleCount - Stats.HitCount);
	}
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

// 배치가 바뀌면 살아 있는 경로를 전부 버린다.
//
// ── 왜 필요한가 ────────────────────────────────────────────────────────────────
// 경로는 뽑힌 시점의 NavMesh를 찍은 사진이다. 그 뒤에 시설이 설치되면 그 자리를
// 관통하도록 계획된 경로가 그대로 남아, 파티가 새 부스 안으로 걸어 들어간다.
// UpdatePathFollowing의 Reset 조건은 **목적지가 움직였을 때**뿐이라 여기에 걸리지 않는다 —
// 목적지는 그대로인데 지형만 바뀐 경우가 정확히 이 구멍이다.
//
// ── 왜 전부 버리는가 ───────────────────────────────────────────────────────────
// 경로가 바뀐 시설을 지나는지 하나씩 판정하려면 선분-사각형 교차를 경로 길이만큼 돌려야 하고,
// 그 사각형은 이미 AvoidanceWalls에 있지만 경로 점은 파티 수만큼 있다. 리비전 변경은 사람이
// 건물을 세울 때만 일어나는 저빈도 사건이므로, 정확한 판정보다 전부 버리는 쪽이 싸고 안전하다.
//
// 재쿼리 폭주는 PathQueriesPerStep(기본 32건/스텝)이 이미 막는다. 버린 직후 한동안
// GetPathQueryBacklog가 치솟는 것이 정상이고, 그 사이 개체는 최종 목적지로 직선 조향한다
// (벽 반발이 있으므로 부스를 관통하지는 않는다).
void UFF_CrowdSimulationSubsystem::InvalidatePathsOnLayoutChange()
{
	const UWorld* World = GetWorld();
	const UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!ZoneSystem)
	{
		return;
	}

	const int64 Revision = ZoneSystem->GetWorldRevision();
	if (Revision == PathLayoutRevision)
	{
		return;
	}

	const bool bHadPaths = (PathLayoutRevision != -1);
	PathLayoutRevision = Revision;

	// 첫 관측은 무효화가 아니다. 버릴 경로가 아직 없으므로 리비전만 기억하고 넘어간다
	if (!bHadPaths)
	{
		return;
	}

	int32 Dropped = 0;
	for (FFF_CrowdPathFollower& Follower : PathFollowers)
	{
		if (Follower.bHasPath || Follower.bQueryFailed)
		{
			Follower.Reset();
			++Dropped;
		}
	}

	// 커서를 되돌려 첫 파티부터 예산을 받게 한다. 그대로 두면 커서 앞쪽 파티가
	// 한 바퀴 도는 동안 낡은 경로 없이 직선 조향으로 남는다
	PathQueryCursor = 0;
	++PathLayoutInvalidations;

	UE_LOG(LogTemp, Log,
		TEXT("배치 변경으로 경로 폐기: 리비전 %lld, 경로 %d개 (예산 %d건/스텝으로 재계산)"),
		Revision, Dropped, PathQueriesPerStep);
}

int32 UFF_CrowdSimulationSubsystem::GetPathFollowingCount() const
{
	int32 Count = 0;
	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		if (UnitDatas[AgentIndex].bIsActive
			&& PathFollowers.IsValidIndex(AgentIndex)
			&& PathFollowers[AgentIndex].HasRemainingWaypoint())
		{
			++Count;
		}
	}
	return Count;
}

void UFF_CrowdSimulationSubsystem::UpdatePathFollowing()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_PathFollow);

	const int32 PoolSize = UnitDatas.Num();
	if (PoolSize == 0)
	{
		return;
	}

	if (PathFollowers.Num() != PoolSize)
	{
		PathFollowers.SetNum(PoolSize);
	}

	InvalidatePathsOnLayoutChange();

	for (FUnitData& Unit : UnitDatas)
	{
		Unit.bHasSteerTarget = false;
	}

	if (!bEnablePathFollowing)
	{
		PathQueryBacklog = 0;
		return;
	}

	int32 Budget = FMath::Max(PathQueriesPerStep, 1);
	int32 Backlog = 0;

	const float MinQueryDistSq = FMath::Square(FMath::Max(MinPathQueryDistanceUU, 0.f));
	const float RepathToleranceSq = FMath::Square(FMath::Max(RepathToleranceUU, 1.f));

	for (int32 Scanned = 0; Scanned < PoolSize; ++Scanned)
	{
		const int32 AgentIndex = (PathQueryCursor + Scanned) % PoolSize;

		FUnitData& Unit = UnitDatas[AgentIndex];
		FFF_CrowdPathFollower& Follower = PathFollowers[AgentIndex];

		if (!Unit.bIsActive)
		{
			if (Follower.bHasPath || Follower.bQueryFailed)
			{
				Follower.Reset();
			}
			continue;
		}

		if (Unit.LeaderAgentIndex != AgentIndex)
		{
			if (Follower.bHasPath || Follower.bQueryFailed)
			{
				Follower.Reset();
			}
			continue;
		}

		if (FVector::DistSquared2D(Follower.PlannedForTarget, Unit.TargetLocation) > RepathToleranceSq)
		{
			Follower.Reset();
		}

		if (!Follower.bHasPath && !Follower.bQueryFailed)
		{
			if (FVector::DistSquared2D(Unit.Position, Unit.TargetLocation) <= MinQueryDistSq)
			{
				Follower.PlannedForTarget = Unit.TargetLocation;
				Follower.bQueryFailed = true;
				continue;
			}

			if (Budget <= 0)
			{
				++Backlog;
				continue;
			}

			--Budget;
			PathQueryCursor = AgentIndex + 1;
			BuildPathForAgent(AgentIndex);
		}

		if (!Follower.bHasPath)
		{
			continue;
		}

		Follower.NextIndex = CrowdPathFollow::AdvanceToReachableWaypoint(
			Follower.Points, Follower.NextIndex, Unit.Position, WaypointAcceptanceRadiusUU);

		if (Follower.Points.IsValidIndex(Follower.NextIndex))
		{
			Unit.SteerTargetLocation = Follower.Points[Follower.NextIndex];
			Unit.bHasSteerTarget = true;
		}
	}

	PathQueryBacklog = Backlog;
}

bool UFF_CrowdSimulationSubsystem::BuildPathForAgent(int32 AgentIndex)
{
	if (!UnitDatas.IsValidIndex(AgentIndex) || !PathFollowers.IsValidIndex(AgentIndex))
	{
		return false;
	}

	const FUnitData& Unit = UnitDatas[AgentIndex];
	FFF_CrowdPathFollower& Follower = PathFollowers[AgentIndex];

	Follower.PlannedForTarget = Unit.TargetLocation;
	++PathQueriesIssued;

	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance() : nullptr;

	if (!NavData)
	{
		Follower.bQueryFailed = true;
		++PathQueryFailures;
		return false;
	}

	FPathFindingQuery Query(nullptr, *NavData, Unit.Position, Unit.TargetLocation);

	Query.SetAllowPartialPaths(true);
	Query.SetRequireNavigableEndLocation(false);

	const FPathFindingResult Result = NavSys->FindPathSync(Query);

	if (!Result.IsSuccessful() || !Result.Path.IsValid())
	{
		Follower.bQueryFailed = true;
		++PathQueryFailures;
		return false;
	}

	const TArray<FNavPathPoint>& PathPoints = Result.Path->GetPathPoints();

	if (PathPoints.Num() < 2)
	{
		Follower.bQueryFailed = true;
		return false;
	}

	Follower.Points.Reset(PathPoints.Num() - 1);

	for (int32 PointIndex = 1; PointIndex < PathPoints.Num(); ++PointIndex)
	{
		Follower.Points.Add(PathPoints[PointIndex].Location);
	}

	if (Result.IsPartial())
	{
		++PartialPathCount;
	}
	else
	{
		Follower.Points.Last() = Unit.TargetLocation;
	}

	Follower.NextIndex = 0;
	Follower.bHasPath = true;
	Follower.bQueryFailed = false;

	return true;
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

	// MoveFailed 도 쿨다운을 탄다. 잼 안에서는 여러 파티가 동시에 임계를 채우므로, 안 걸면
	// 막힌 무리 전체가 같은 초에 목적지를 갈아치우며 결정 큐를 채운다(churn).
	if ((Reason == EFF_DecisionReason::Arrived || Reason == EFF_DecisionReason::MoveFailed)
		&& DecisionCooldownSeconds > 0.f
		&& State.LastDecisionSimTime > 0.f
		&& (SimTimeSeconds - State.LastDecisionSimTime) < DecisionCooldownSeconds)
	{
		++DecisionCooldownSkips;
		return;
	}

	ClearPartyQueueState(LeaderAgentIndex, false);

	// ★ 여기서 RecordVisit 을 부르지 않는다.
	//
	// 이 경로는 도착뿐 아니라 가입 거절·큐 비활성·줄 포기까지 전부 지나간다. 여기서 기록하면
	// "줄만 서다 떠난 시설"이 가봤음으로 남아 novelty 페널티를 먹고 다시 안 가게 된다
	// (FF_VisitHistory.h RecordVisit 주석이 경고하는 바로 그 상황이다).
	// 방문 기록은 이용 완료 시점 하나뿐이어야 하므로 ApplyPartyNeedRelief 로 옮겼다.
	// 비우기 전에 옮긴다. 조정자가 실패한 목적지를 후보에서 빼려면 이 값이 유일한 단서다
	// (조건을 걸지 않는다 — 배회 중이었으면 NAME_None 이 넘어가는 것이 맞다).
	State.PreviousDestinationId = State.CurrentDestinationId;

	State.CurrentDestinationId = NAME_None;
	State.CurrentNeedRelief = FFF_NeedReliefFlags();

	State.bNeedsDecision = true;

	State.PendingReason = Reason;

	if (!State.bQueued)
	{
		State.bQueued = true;
		DecisionQueue.Add(LeaderAgentIndex);
	}
}

void UFF_CrowdSimulationSubsystem::ApplyPartyNeedRelief(int32 PartyID)
{
	const int32* LeaderPtr = PartyLeaderByID.Find(PartyID);
	if (!LeaderPtr)
	{
		return;
	}

	const int32 LeaderIndex = *LeaderPtr;
	if (!PartyDecisionStates.IsValidIndex(LeaderIndex) || !UnitDatas.IsValidIndex(LeaderIndex))
	{
		return;
	}

	FPartyDecisionState& State = PartyDecisionStates[LeaderIndex];

	// 배회로 온 파티이거나(목적지 없음) 해소 성분이 하나도 없는 시설이다.
	// 후자는 Booth·Info·Safety 처럼 Need 를 풀지 않는 카테고리라 정상이다 —
	// 다만 둘을 합쳐 세면 "배선이 끊긴 것"과 구분이 안 되므로 카운터로 남긴다.
	if (State.CurrentDestinationId.IsNone() || !State.CurrentNeedRelief.RelievesAnything())
	{
		++NeedReliefSkipped;
		return;
	}

	// 인덱스는 캐시하지 않고 매번 받는다. 슬롯이 재사용되므로 저장하면 다른 사람을 가리킨다.
	const TArray<int32> Members = GetPartyMembers(PartyID);
	for (const int32 AgentIndex : Members)
	{
		if (!UnitDatas.IsValidIndex(AgentIndex) || !UnitDatas[AgentIndex].bIsActive)
		{
			continue;
		}

		FUnitData& Unit = UnitDatas[AgentIndex];
		Unit.Needs = FFF_NeedRules::Relieve(Unit.Needs, State.CurrentNeedRelief, NeedReliefAmounts);
	}

	// 집계값도 같이 내린다. AdvanceNeeds 는 매 스텝 개체값으로 다시 쌓지만,
	// 그 사이에 결정이 걸리면 방금 푼 Need 가 그대로 반영돼 같은 시설을 또 고른다.
	if (PartyAggregatedNeeds.IsValidIndex(LeaderIndex))
	{
		PartyAggregatedNeeds[LeaderIndex] = FFF_NeedRules::Relieve(
			PartyAggregatedNeeds[LeaderIndex], State.CurrentNeedRelief, NeedReliefAmounts);
	}

	// 방문 기록도 여기서 한다 — 이용을 마친 시점이 유일하게 옳은 시점이다.
	if (PartyVisitHistories.IsValidIndex(LeaderIndex))
	{
		PartyVisitHistories[LeaderIndex].RecordVisit(State.CurrentDestinationId, GetLiveMinutes());
	}

	++NeedReliefApplied;

	UE_LOG(LogTemp, Verbose,
		TEXT("[CrowdSimulationSubsystem] 파티 %d가 '%s' 이용 완료 — %d명 Need 해소"),
		PartyID, *State.CurrentDestinationId.ToString(), Members.Num());
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

int32 UFF_CrowdSimulationSubsystem::RequestRedecisionForIdleParties(int32 MaxParties)
{
	int32 RequestedCount = 0;

	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		if (MaxParties > 0 && RequestedCount >= MaxParties)
		{
			break;
		}

		const FUnitData& Unit = UnitDatas[AgentIndex];

		// 리더만 결정을 받는다. 풀에 반납된 개체(bIsActive == false)는 데이터가 남아 있어도 제외한다.
		if (!Unit.bIsActive || Unit.LeaderAgentIndex != AgentIndex)
		{
			continue;
		}

		if (!PartyDecisionStates.IsValidIndex(AgentIndex))
		{
			continue;
		}

		const FPartyDecisionState& State = PartyDecisionStates[AgentIndex];

		// 헤더 주석의 이유로 줄에 선 파티와 퇴장 중인 파티는 제외한다.
		if (State.QueuePhase != EFF_PartyQueuePhase::None || State.bIsLeaving)
		{
			continue;
		}

		RequestDecision(AgentIndex, EFF_DecisionReason::DestinationOpened);
		++RequestedCount;
	}

	return RequestedCount;
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

// 조정자가 목적지를 못 정했을 때의 국소 배회. **살아 있는 유일한 랜덤 목적지 생성기다.**
//
// ★ Rng(FRandomStream)를 쓰지 않는 것이 핵심이다. 시드를 (전역시드, StableAgentID, 라운드)에서
//   뽑으므로 같은 입력이면 항상 같은 좌표가 나오고, 호출 순서와 프레임률이 결과에 새어들지 않는다.
//   공유 스트림에서 뽑으면 뽑는 순서가 곧 결과가 되고, 위치가 갈리면 다음 재평가의 거리가 갈려
//   결정 자체가 갈린다(FF_WalkDistanceMatrix.h가 경고하는 그 문제다).
//
// ★ 예전에 PickRandomDestination()이 따로 있었다. 공유 Rng에서 뽑고 범위가 SimRadiusUU 전체라
//   개체를 축제장 밖으로 보내면서 결정론까지 깨는 함수였고, 호출부가 없어 삭제했다.
//   배회 범위를 넓히고 싶으면 그것을 되살리지 말고 WanderRadiusUU를 올릴 것 —
//   그래야 결정론이 유지된다.
//
// ★ 통행 상한에 걸리는 좌표는 여기서 걸러 낸다. 안 걸러도 이동 쪽이 막아 주지만, 갈 수 없는
//   곳을 목표로 잡은 파티는 경계에 붙어 서서 StuckSteps를 채우고 MoveFailed로 다시 여기
//   들어온다. 그 왕복이 결정 큐를 잡아먹으므로 뽑는 자리에서 끊는 것이 맞다.
//
// ★ 재추첨 시드는 MakeDecisionSeed의 salt를 더 쓰지 않고 기본 시드에서 갈라낸다. salt 공간이
//   라운드당 두 칸(ChoiceRoundIndex * 2, * 2 + 1)으로 이미 꽉 차 있어, 더 쓰면 다음 라운드의
//   시드와 겹쳐 배회 좌표가 라운드 간에 상관된다.
//
// ★ 전부 막히면 제자리를 돌려준다. 다음 스텝에 도착 판정이 서고 재결정이 걸리지만 쿨다운을
//   타므로 폭주하지 않으며, 그때는 ChoiceRoundIndex가 올라 새 시드로 다시 뽑는다.
FVector UFF_CrowdSimulationSubsystem::PickWanderDestination(int32 LeaderAgentIndex) const
{
	const FUnitData& Unit = UnitDatas[LeaderAgentIndex];
	const FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];

	const uint32 SeedAngle = FFF_DeterministicRandom::MakeDecisionSeed(
		GlobalSimulationSeed, Unit.StableAgentID, State.ChoiceRoundIndex * 2);
	const uint32 SeedRadius = FFF_DeterministicRandom::MakeDecisionSeed(
		GlobalSimulationSeed, Unit.StableAgentID, State.ChoiceRoundIndex * 2 + 1);

	constexpr int32 MaxAttempts = 6;

	auto DeriveAttemptSeed = [](uint32 BaseSeed, int32 Attempt) -> uint32
	{
		return (Attempt == 0) ? BaseSeed : HashCombine(BaseSeed, GetTypeHash(Attempt));
	};

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const float Angle =
			FFF_DeterministicRandom::MakeUniformFraction(DeriveAttemptSeed(SeedAngle, Attempt)) * 2.f * PI;
		const float Radius = WanderRadiusUU * FMath::Sqrt(
			FFF_DeterministicRandom::MakeUniformFraction(DeriveAttemptSeed(SeedRadius, Attempt)));

		FVector Destination = Unit.Position + FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.f);

		const FVector FromOrigin(Destination.X - SimOrigin.X, Destination.Y - SimOrigin.Y, 0.f);
		if (FromOrigin.SizeSquared() > FMath::Square(SimRadiusUU))
		{
			Destination = SimOrigin + FromOrigin.GetSafeNormal() * SimRadiusUU;
			Destination.Z = Unit.Position.Z;
		}

		if (HeightField.IsWalkable(FVector2D(Destination.X, Destination.Y)))
		{
			return Destination;
		}
	}

	return Unit.Position;
}

int32 UFF_CrowdSimulationSubsystem::GetMaxInTransitMemberCount() const
{
	int32 Max = 0;
	for (const TPair<FName, int32>& Pair : InTransitMemberCounts)
	{
		Max = FMath::Max(Max, Pair.Value);
	}
	return Max;
}

void UFF_CrowdSimulationSubsystem::RebuildInTransitMemberCounts()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_InTransitCounts);

	// Reset 은 키만 비우고 할당은 남긴다 — 매 배치 재구축이라 재할당이 그대로 비용이 된다
	InTransitMemberCounts.Reset();

	for (int32 LeaderIndex = 0; LeaderIndex < PartyDecisionStates.Num(); ++LeaderIndex)
	{
		if (!UnitDatas.IsValidIndex(LeaderIndex))
		{
			continue;
		}

		const FUnitData& Unit = UnitDatas[LeaderIndex];

		// 리더만 센다. 멤버까지 돌면 파티 인원을 인원수만큼 곱해서 세게 된다
		if (!Unit.bIsActive || Unit.LeaderAgentIndex != LeaderIndex)
		{
			continue;
		}

		const FPartyDecisionState& State = PartyDecisionStates[LeaderIndex];

		if (State.bIsLeaving
			|| State.QueuePhase != EFF_PartyQueuePhase::None
			|| State.CurrentDestinationId.IsNone())
		{
			continue;
		}

		const int32 MemberCount = PartyMemberCounts.IsValidIndex(LeaderIndex)
			? FMath::Max(PartyMemberCounts[LeaderIndex], 1)
			: 1;

		InTransitMemberCounts.FindOrAdd(State.CurrentDestinationId) += MemberCount;
	}
}

void UFF_CrowdSimulationSubsystem::HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage CurrentStage)
{
	if (PreviousStage == EFF_FestivalStage::Live && CurrentStage != EFF_FestivalStage::Live)
	{
		AllAgentLeavingTrigger();
	}
	else if (CurrentStage == EFF_FestivalStage::Live)
	{
		LiveStartSimTime = SimTimeSeconds;
		BeginArrivalSchedule(ArrivalRequestedVisitorCount,ArrivalSpawnCenter,ArrivalSpawnRadiusUU,
			GatherReservedPartyRequests());
		
	}
	else if (PreviousStage == EFF_FestivalStage::Closing)
	{
		DespawnAllAgent();
	}
}

void UFF_CrowdSimulationSubsystem::ProcessDecisions()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Decisions);

	if (DecisionQueue.IsEmpty())
	{
		return;
	}

	// 배치 앞에서 한 번 짓고, 아래 루프에서 방금 내려진 결정을 그 위에 얹는다.
	// 배치 안에서 누적하는 것이 이 값의 핵심이다 — 같은 프레임에 결정하는 파티들이 서로를
	// 못 보면 전원이 같은 빈 시설을 고르고, 그게 지금 보이는 몰림이다.
	RebuildInTransitMemberCounts();

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
		Context.PreviousDestinationId = State.PreviousDestinationId;
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

		// 이 배치 동안만 유효한 스냅샷을 빌려준다. 조정자가 보관하면 안 된다
		Context.InTransitMemberCounts = &InTransitMemberCounts;

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

		// 방금 내려진 결정을 스냅샷에 얹는다. 이 파티의 직전 목적지는 RequestDecision 이 이미
		// 비워서 재구축에서 빠져 있으므로, 여기서 빼야 할 것은 없다.
		if (bDecided && !Result.DestinationId.IsNone())
		{
			InTransitMemberCounts.FindOrAdd(Result.DestinationId) += Context.PartySize;
		}

		// CurrentDestinationId 와 항상 같이 세운다 — 갈리면 다른 시설의 Need 가 풀린다.
		State.CurrentNeedRelief = bDecided ? Result.NeedRelief : FFF_NeedReliefFlags();

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
		Unit.Speed = Unit.MaxSpeed; 
	}
	else
	{
		Unit.TargetLocation = Leader.GetSteerTarget() + Offset;
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
FFF_VisitorDistribution UFF_CrowdSimulationSubsystem::BuildVisitorDistribution(int32 RequestedCount,
	const TArray<FFF_ReservedPartyRequest>& ReservedParties) const
{
	return FFF_VisitorDistributionBuilder::Build(
		CachedGroupSizeTable.Get(),
		CachedChildCompositionTable.Get(),
		CachedArchetypeTable.Get(),
		CachedGroupScopeId,
		CachedArchetypeScenarioId,
		RequestedCount,
		GlobalSimulationSeed,
		ReservedParties);
}

TArray<FFF_ReservedPartyRequest> UFF_CrowdSimulationSubsystem::GatherReservedPartyRequests() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	if (const UFF_FestivalProgressionSubsystem* Progression =
		GameInstance ? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr)
	{
		return Progression->GetReservedPartyRequests();
	}

	return TArray<FFF_ReservedPartyRequest>();
}

void UFF_CrowdSimulationSubsystem::ConfirmReservedPartySpawn(const FGuid& ReservationId, int32 PartyID,
	int32 MemberCount)
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;

	UFF_ProgramReservationSubsystem* Reservations =
		GameInstance ? GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>() : nullptr;

	if (!Reservations)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] 예약 파티 %d를 스폰했지만 ProgramReservationSubsystem이 없다 — 프로그램 매출이 잡히지 않는다"),
			PartyID);
		return;
	}

	if (!Reservations->ConfirmReservedParty(ReservationId, PartyID, MemberCount))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] 예약 %s 확정이 거절됐다 — 파티 %d(%d명). 정원 초과이거나 없는 예약이다"),
			*ReservationId.ToString(), PartyID, MemberCount);
	}
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
void UFF_CrowdSimulationSubsystem::AdvanceNeeds(bool bAggregateMetrics)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Needs);
	const float ElapsedMinutes =
		(SimTimeSeconds - LastNeedAdvanceSimTime) * Temp_FestivalMinutesPerRealSecond;
	LastNeedAdvanceSimTime = SimTimeSeconds;

	// GameInstance 스코프라 캐시하지 않는다 — 이 서브시스템(World)보다 오래 산다.
	// 집계하는 스텝에만 조회하므로 매 프레임 비용이 아니다.
	UFF_NpcNeedMetricsSubsystem* NeedMetrics = nullptr;
	if (bAggregateMetrics)
	{
		if (const UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				NeedMetrics = GameInstance->GetSubsystem<UFF_NpcNeedMetricsSubsystem>();
			}
		}
	}

	if (NeedMetrics)
	{
		NeedMetrics->BeginAggregation();
	}
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

		// 표본은 파티가 아니라 개인 단위다 — 지표가 "이 Need 가 미해소인 방문객 비율"이다.
		if (NeedMetrics)
		{
			NeedMetrics->AddSample(Unit.Needs);
		}

		const int32 LeaderIndex = Unit.LeaderAgentIndex;
		if (!PartyAggregatedNeeds.IsValidIndex(LeaderIndex))
		{
			continue;
		}
		PartyAggregatedNeeds[LeaderIndex] =
			FFF_NeedState::MakeAggregated(PartyAggregatedNeeds[LeaderIndex], Unit.Needs);
		++PartyMemberCounts[LeaderIndex];
	}

	// 활성 개체가 0명이어도 확정한다 — 표본 0은 "데이터 없음"이라 UI 가 0% 와 구분한다.
	if (NeedMetrics)
	{
		NeedMetrics->CommitAggregation();
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

	if (PathFollowers.IsValidIndex(index))
	{
		PathFollowers[index].Reset();
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
	PathFollowers.Reset();
	PathFollowers.SetNum(UnitDatas.Num());
	PathQueryCursor = 0;
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

			if (PathFollowers.IsValidIndex(AgentIndex) && PathFollowers[AgentIndex].HasRemainingWaypoint())
			{
				const FFF_CrowdPathFollower& Follower = PathFollowers[AgentIndex];
				constexpr float PathDrawZ = 10.f;

				FVector Previous = Unit.Position;
				for (int32 PointIndex = Follower.NextIndex; PointIndex < Follower.Points.Num(); ++PointIndex)
				{
					const FVector Point = Follower.Points[PointIndex] + FVector(0, 0, PathDrawZ);
					DrawDebugLine(World, Previous + FVector(0, 0, PathDrawZ), Point,
						FColor(120, 200, 255), false, -1.f, 0, 2.f);
					DrawDebugPoint(World, Point, 8.f, FColor(255, 255, 120), false, -1.f, 0);
					Previous = Follower.Points[PointIndex];
				}
			}
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

	if (PathFollowers.IsValidIndex(AgentID))
	{
		PathFollowers[AgentID].Reset();
	}

	Unit.bHasSteerTarget = false;

	ensureMsgf(ActiveAgentCount + FreeAgentSlotIndex.Num() == UnitDatas.Num(),
	   TEXT("인원 보존 위반: active %d + free %d != pool %d"),
	   ActiveAgentCount, FreeAgentSlotIndex.Num(), UnitDatas.Num());

}

void UFF_CrowdSimulationSubsystem::DespawnAllAgent()
{
	for (int32 AgentIndex = 0;AgentIndex < UnitDatas.Num();AgentIndex++)
	{
		if(UnitDatas[AgentIndex].bIsActive)
		{
			DespawnAgentInternal(AgentIndex);
		}
	}
	PendingDespawnParty.Reset();
}

void UFF_CrowdSimulationSubsystem::AllAgentLeavingTrigger()
{
	for (TTuple<int, int> Party : PartyLeaderByID)
	{
		int PartyIndex = Party.Key;
		int LeaderIndex = Party.Value;
		
		FUnitData& Unit = UnitDatas[LeaderIndex];
		FPartyDecisionState& State = PartyDecisionStates[LeaderIndex];
		
		if (State.bIsLeaving) continue;
		
		State.bIsLeaving = true;
		State.LeaveStartedSimTime = SimTimeSeconds;
		Unit.TargetLocation = Temp_ExitWorldLocation;
		State.CurrentDestinationId = NAME_None;
		State.bNeedsDecision = false;
		State.LastDecisionSimTime = SimTimeSeconds;
		State.CurrentNeedRelief = FFF_NeedReliefFlags();
		ClearPartyQueueState(LeaderIndex, true);
		UnitDatas[LeaderIndex].TargetLocation = Temp_ExitWorldLocation;
		
		
		
	}
	
}
