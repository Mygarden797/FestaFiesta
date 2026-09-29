// Fill out your copyright notice in the Description page of Project Settings.

#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "NPC/IndividualVariation/FF_DeterministicRandom.h"
#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdSteering.h"
#include "CrowdSystem/Lane/FF_CrowdLane.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "Engine/DataTable.h"
#include "NPC/Destination/FF_UtilityCoefficientHelpers.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h"
#include "NPC/Metrics/FF_NpcNeedMetricsSubsystem.h"
#include "NPC/Metrics/FF_SatisfactionMetricsSubsystem.h"
#include "NPC/Party/FF_NeedRules.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Queue/FF_QueueTypes.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSAudienceLibrary.h"
#include "DataTables/FF_ArrivalCurveRow.h"
#include "Serialization/GameplayAbilityTargetingLocationInfoNetSerializer.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "DataTables/FF_CrowdAppearenceRow.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "ProfilingDebugging/CsvProfiler.h"

// 인원수별 프레임 부하를 재기 위한 프레임 단위 카운터.
// `CsvProfile start` / `CsvProfile stop` 로 뜬 CSV에 FrameTime·GameThreadTime과 같은 행에 실린다.
// N을 손으로 기록할 필요가 없어지고, StepsThisFrame 이 MaxStepsPerFrame(3)에 붙는 지점이
// 곧 시뮬이 실시간을 놓치기 시작하는 한계선이다.
CSV_DEFINE_CATEGORY(FFCrowd, true);

namespace
{
	constexpr int32 NoSpawnGeneration = 0;
	constexpr double RoadSupportBucketSizeCm = 400.0;

	// 서비스를 마친 파티는 시설의 퇴장 지점으로 보내진다.
	constexpr float QueueExitMatchToleranceUU = 50.f;

	// 멤버 추격을 푸는 거리 = PartyCohesionRadiusUU * 이 값. 켜는 임계보다 작아야 히스테리시스다.
	constexpr float PartyCatchUpReleaseRatio = 0.6f;

	// 대형 안에 있는 멤버가 리더 속도 대비 낼 수 있는 최대 배율. 1.0이면 자리를 영영 못 메운다.
	constexpr float PartyFormationSpeedHeadroom = 1.25f;

	// 이 속도 이하의 리더는 서 있는 것으로 본다, UU/s. 잼에서의 미세 진동을 걷는 것으로 세지 않는다.
	constexpr float PartyLeaderStoppedSpeedUU = 5.f;

	constexpr uint32 FormationBlockCheckIntervalSteps = 6;
	constexpr int32 FormationBlockReleaseChecks = 3;
	constexpr float FormationRepathToleranceUU = 400.f;

	// 게이트 투영 익스텐트. 공용 앵커용(300, 300, 2000)은 수평이 넓어 벽 안의 문이 옆 통로에 붙어 통과한다.
	const FVector GateProbeExtentCm(50.0, 50.0, 250.0);

	// 투영 낙차가 이보다 크면 공중이나 지형 아래에 놓인 것이다. 통과시키되 경고한다.
	constexpr double GateProjectionDropWarnCm = 150.0;

	// 게이트 원판 둘레를 몇 방향 찍어볼지. 중심만 보면 벽에 걸친 문을 못 잡는다.
	constexpr int32 GateRimSampleCount = 8;

	// 가중치 총합이 이보다 작으면 정규화가 폭발한다.
	constexpr float MinGateWeightSum = KINDA_SMALL_NUMBER;

	// 폴백 게이트 ID. 지표에서 "오늘은 폴백으로 돌았다"가 바로 보인다.
	const FName FallbackGateId(TEXT("gate_fallback"));
}

void UFF_CrowdSimulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	RefreshLanesFromWorld();
}

void UFF_CrowdSimulationSubsystem::InitializeSimulation(FVector NewOrigin,
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
	
	CachedAppearenceTable = Settings->CrowdAppearanceTable.LoadSynchronous();

	if (!CachedAppearenceTable)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] CrowdAppearanceTable 미설정 — 전원 프록시 원기둥으로 표시된다. ")
			TEXT("Project Settings에 지정했는지, 지정한 DataTable 애셋이 이동·삭제되지 않았는지 확인할 것"));
	}

	if (CachedCoefficientTable)
	{
		NeedRiseRates = FFF_NeedRules::LoadRiseRates(CachedCoefficientTable.Get());
		NeedReliefAmounts = FFF_NeedRules::LoadReliefAmounts(CachedCoefficientTable.Get());
		StayDrainMultiplierAtMaxNeed =
			FFF_NeedRules::LoadStayDrainMultiplierAtMaxNeed(CachedCoefficientTable.Get());
		FFF_NeedRules::LoadInitialNeedFractions(
			CachedCoefficientTable.Get(), InitialNeedModeFraction, InitialNeedMaxFraction);

		// 기존 멤버 값을 기본값으로 넘긴다: 행이 없거나 테이블이 없으면 저작값이 살아남고,
		// 행이 있으면 CSV 가 이긴다. UPROPERTY 로 편집한 값도 여기서 덮이므로 그 의미를
		// 헤더 주석에 적어 두었다.
		ProgramReturnBufferMinutes = FFF_UtilityCoefficientHelpers::GetCoefficient(
			CachedCoefficientTable.Get(), TEXT("reservation_return_buffer_minutes"),
			ProgramReturnBufferMinutes);
		ProgramExitGraceMinutes = FFF_UtilityCoefficientHelpers::GetCoefficient(
			CachedCoefficientTable.Get(), TEXT("reservation_exit_buffer_minutes"),
			ProgramExitGraceMinutes);
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

	ConfigureSimArea(NewOrigin);

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
		Stage->OnMainProgramStarted.AddUniqueDynamic(this, &UFF_CrowdSimulationSubsystem::HandleMainProgramStarted);
		Stage->OnMainProgramEnded.AddUniqueDynamic(this, &UFF_CrowdSimulationSubsystem::HandleMainProgramEnded);
		SimulationTimeScale = Stage->GetEffectiveTimeScale();
	}
}

// 격자 범위의 우선순위는 직접 지정 > NavMesh 경계다. 통행 가능 영역이 곧 관측 대상이다
bool UFF_CrowdSimulationSubsystem::ResolveSimAreaBounds(FBox& OutBoundsCm) const
{
	if (bSimAreaBoundsOverridden && SimAreaBoundsCm.IsValid)
	{
		OutBoundsCm = SimAreaBoundsCm;
		return true;
	}

	const UWorld* World = GetWorld();
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance() : nullptr;
	const FBox NavBounds = NavData ? NavData->GetBounds() : FBox(ForceInit);

	if (!NavBounds.IsValid || NavBounds.GetSize().GetMax() <= 0.0)
	{
		OutBoundsCm = FBox(ForceInit);
		return false;
	}

	OutBoundsCm = NavBounds;
	return true;
}

void UFF_CrowdSimulationSubsystem::ConfigureSimArea(FVector NewOrigin)
{
	bSimAreaBoundsOverridden = false;

	FBox Resolved(ForceInit);
	ResolveSimAreaBounds(Resolved);

	ApplySimArea(NewOrigin, Resolved);
}

void UFF_CrowdSimulationSubsystem::InitializeSimulationWithBounds(FVector NewOrigin, const FBox& AreaBoundsCm,
	int32 NewActorPoolSize, int32 NewAgentPoolSize, TSubclassOf<AFF_BaseCrowd> InCrowdActorClass)
{
	InitializeSimulation(NewOrigin, NewActorPoolSize, NewAgentPoolSize, InCrowdActorClass);
	ConfigureSimAreaWithBounds(NewOrigin, AreaBoundsCm);
}

void UFF_CrowdSimulationSubsystem::ConfigureSimAreaWithBounds(FVector NewOrigin, const FBox& AreaBoundsCm)
{
	bSimAreaBoundsOverridden = AreaBoundsCm.IsValid != 0;

	ApplySimArea(NewOrigin, AreaBoundsCm);
}

void UFF_CrowdSimulationSubsystem::ApplySimArea(FVector NewOrigin, const FBox& AreaBoundsCm)
{
	SimOrigin = NewOrigin;
	SimAreaBoundsCm = AreaBoundsCm;

	RebuildDensityGrid();

	RebuildAvoidanceWalls();
	UE_LOG(LogTemp, Warning,
	TEXT("[Crowd] FacilityWallCount = %d"),
	FacilityWallCount);

	RebuildHeightField();

	// 퇴장 지점을 안 받았으면 범위의 -X 가장자리 안쪽에 둔다
	if (!bTemp_ExitLocationSet)
	{
		Temp_ExitWorldLocation = SimAreaBoundsCm.IsValid
			? FVector(
				FMath::Lerp(SimAreaBoundsCm.Min.X, SimOrigin.X, 0.2),
				SimOrigin.Y,
				SimOrigin.Z)
			: SimOrigin;
	}
}

void UFF_CrowdSimulationSubsystem::RebuildDensityGrid()
{
	if (!SimAreaBoundsCm.IsValid)
	{
		DensityGrid = FCrowdDensityGrid();

		UE_LOG(LogTemp, Warning,
			TEXT("[Crowd] 밀도 격자를 세우지 못했다 — NavMesh 경계가 없다. "
				 "NavMesh를 깔거나 ConfigureSimAreaWithBounds로 범위를 지정할 것"));
		return;
	}

	const FVector GridOrigin(SimAreaBoundsCm.Min.X, SimAreaBoundsCm.Min.Y, SimOrigin.Z);
	const FVector BoundsSize = SimAreaBoundsCm.GetSize();

	DensityGrid.Build(GridOrigin, FVector2D(BoundsSize.X, BoundsSize.Y), DensityCellSizeUU);
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

	// OnTimeScaleChange는 사용자가 고른 raw 배속만 알린다. 공연/미니게임 잠금은 매 프레임
	// Effective 값을 읽어야 Stage 시계와 Crowd 이동이 같은 속도로 흐른다.
	float EffectiveSimulationTimeScale = SimulationTimeScale;
	if (const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const UFF_FestivalStageSubsystem* Stage =
			GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
		{
			EffectiveSimulationTimeScale = Stage->GetEffectiveTimeScale();
		}
	}
	const float Time = DeltaTime * EffectiveSimulationTimeScale;

	float DiscardedSimSeconds = 0.f;
	const int32 StepsThisFrame = CrowdSteering::ConsumeFixedSteps(
		SimAccumulator, Time, FixedStepSeconds, MaxStepsPerFrame, &DiscardedSimSeconds);

	if (DiscardedSimSeconds > 0.f)
	{
		DroppedSimSeconds += DiscardedSimSeconds;
		++DroppedStepFrames;
	}

	for (int32 Step = 0; Step < StepsThisFrame; ++Step)
	{
		StepSimulation(FixedStepSeconds);
		SimTimeSeconds += FixedStepSeconds;
	}

	ProcessPendingDespawns();
	FlushMissingEvent();
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

	CSV_CUSTOM_STAT(FFCrowd, ActiveAgents, ActiveAgentCount, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(FFCrowd, BoundActors, PoolBinding.GetBoundCount(), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(FFCrowd, StepsThisFrame, StepsThisFrame, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(FFCrowd, DiscardedSimMs, DiscardedSimSeconds * 1000.f, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(FFCrowd, DroppedSimSeconds, DroppedSimSeconds, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(FFCrowd, MaxDensity, DensityGrid.GetMaxDensity(), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(FFCrowd, PathQueryBacklog, PathQueryBacklog, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(FFCrowd, PendingDecisions, DecisionQueue.Num(), ECsvCustomStatOp::Set);
}

bool UFF_CrowdSimulationSubsystem::ValidateGate(const FFF_CrowdGate& InGate, int32 AuthoredIndex,
	FFF_CrowdGate& OutGate)
{
	// 싼 검사부터 돌린다. NavMesh 쿼리까지 가기 전에 걸러낼 수 있는 것을 먼저 본다.
	// 투영 실패와 도달 불가는 제외, 낙차와 둘레 결손은 통과시키고 경고만 남긴다.
	OutGate = InGate;

	if (OutGate.GateId.IsNone())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] 게이트 %d 제외 — GateId가 비었다. 지표 집계 키가 없으면 ")
			TEXT("게이트별 유입/유출을 셀 수 없다"), AuthoredIndex);
		return false;
	}

	if (!OutGate.bAllowSpawn && !OutGate.bAllowExit)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] 게이트 '%s' 제외 — 입장도 퇴장도 허용하지 않는다"),
			*OutGate.GateId.ToString());
		return false;
	}

	if (OutGate.GateRadiusUU <= 0.f)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] 게이트 '%s' 제외 — GateRadiusUU가 %.1f다"),
			*OutGate.GateId.ToString(), OutGate.GateRadiusUU);
		return false;
	}

	if (OutGate.bAllowSpawn && OutGate.GateWeight <= 0.f)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] 게이트 '%s' 제외 — 입장 게이트인데 GateWeight가 %.3f다"),
			*OutGate.GateId.ToString(), OutGate.GateWeight);
		return false;
	}

	if (IsValid(OutGate.GateAnchor))
	{
		OutGate.GateLocation = OutGate.GateAnchor->GetActorLocation();
	}

	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance() : nullptr;

	// 검증 안 함과 검증 실패는 다르다. Dynamic 생성이면 BeginPlay 시점에 NavMesh가 아직 없다.
	if (!NavData)
	{
		++UnverifiedGateCount;
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] 게이트 '%s' NavMesh 검증 생략 — NavigationData가 없다. ")
			TEXT("NavMesh 생성이 Dynamic이면 BeginPlay 시점에 아직 안 올라와 있다"),
			*OutGate.GateId.ToString());
		return true;
	}

	FNavLocation Projected;
	if (!NavSys->ProjectPointToNavigation(OutGate.GateLocation, Projected, GateProbeExtentCm))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] 게이트 '%s' 제외 — NavMesh 투영 실패. 좌표 %s"),
			*OutGate.GateId.ToString(), *OutGate.GateLocation.ToCompactString());
		return false;
	}

	const double DropCm = FMath::Abs(OutGate.GateLocation.Z - Projected.Location.Z);
	if (DropCm > GateProjectionDropWarnCm)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] 게이트 '%s' 투영 낙차 %.0fcm — 공중이나 지형 아래에 놓였는지 확인할 것"),
			*OutGate.GateId.ToString(), DropCm);
	}

	OutGate.GateLocation = Projected.Location;

	// 원판 검사. 스폰은 중심이 아니라 원판에 흩어지고 멤버는 거기서 대형만큼 더 나간다.
	if (OutGate.bAllowSpawn)
	{
		// 그룹 크기 상한. 대형 반경은 PartySpacingUU * sqrt(SlotIndex)이므로 마지막 슬롯이 가장 멀다.
		constexpr int32 MaxExpectedPartySize = 6;
		const double RimRadius = OutGate.GateRadiusUU
			+ GetPartyFormationOffset(MaxExpectedPartySize - 1).Size2D();

		int32 RimFailures = 0;
		for (int32 Sample = 0; Sample < GateRimSampleCount; ++Sample)
		{
			const double Angle = 2.0 * PI * Sample / GateRimSampleCount;
			const FVector RimPoint = OutGate.GateLocation
				+ FVector(RimRadius * FMath::Cos(Angle), RimRadius * FMath::Sin(Angle), 0.0);

			FNavLocation RimProjected;
			if (!NavSys->ProjectPointToNavigation(RimPoint, RimProjected, GateProbeExtentCm))
			{
				++RimFailures;
			}
		}

		if (RimFailures > 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[CrowdSimulationSubsystem] 게이트 '%s' 둘레 %d/%d 방향이 NavMesh 밖이다 — ")
				TEXT("이 문은 반쪽만 쓸 수 있다. 대형이 삐져나가면 파티 크기 상한을 검토할 것"),
				*OutGate.GateId.ToString(), RimFailures, GateRimSampleCount);
		}
	}

	// 도달성. 강 건너·펜스 밖에도 NavMesh는 깔려 있어 투영만으로는 끊긴 섬을 못 거른다.
	// 부분 경로를 금지해야 "강가까지는 간다"가 성공 처리되지 않는다.
	// 끝점에 NavMesh를 요구하지 않는 것은 SimOrigin이 지면에서 떠 있으면 전 게이트가 탈락해서다.
	FPathFindingQuery Query(nullptr, *NavData, OutGate.GateLocation, SimOrigin);
	Query.SetAllowPartialPaths(false);
	Query.SetRequireNavigableEndLocation(false);

	const FPathFindingResult Result = NavSys->FindPathSync(Query);
	if (!Result.IsSuccessful() || !Result.Path.IsValid())
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] 게이트 '%s' 제외 — 시뮬 중심까지 경로가 없다. ")
			TEXT("NavMesh 위이지만 축제장과 끊긴 섬이다"),
			*OutGate.GateId.ToString());
		return false;
	}

	return true;
}

void UFF_CrowdSimulationSubsystem::RebuildSpawnGateCdf()
{
	SpawnGateIndices.Reset();
	SpawnGateCdf.Reset();

	float RunningSum = 0.f;
	for (int32 GateIndex = 0; GateIndex < ValidGates.Num(); ++GateIndex)
	{
		const FFF_CrowdGate& Gate = ValidGates[GateIndex];
		if (!Gate.bAllowSpawn)
		{
			continue;
		}

		RunningSum += Gate.GateWeight;
		SpawnGateIndices.Add(GateIndex);
		SpawnGateCdf.Add(RunningSum);
	}

	if (SpawnGateCdf.Num() == 0 || RunningSum <= MinGateWeightSum)
	{
		if (SpawnGateCdf.Num() > 0)
		{
			UE_LOG(LogTemp, Error,
				TEXT("[CrowdSimulationSubsystem] 입장 게이트 %d개의 가중치 총합이 %.6f다 — 정규화할 수 없다. 폴백으로 돌린다"),
				SpawnGateCdf.Num(), RunningSum);
		}
		SpawnGateIndices.Reset();
		SpawnGateCdf.Reset();
		return;
	}

	// 마지막 원소로 정규화한다. 뽑을 때 나눗셈이 없다.
	for (float& Cumulative : SpawnGateCdf)
	{
		Cumulative /= RunningSum;
	}
	SpawnGateCdf.Last() = 1.f;
}

void UFF_CrowdSimulationSubsystem::EnsureFallbackGate()
{
	if (SpawnGateIndices.Num() > 0)
	{
		bUsingFallbackGate = false;
		return;
	}

	// 좌표만 갱신한다. 날마다 새로 붙이면 배열이 자라고 남아 있는 게이트 인덱스가 딴 문을 가리킨다.
	int32 FallbackIndex = ValidGates.IndexOfByPredicate(
		[](const FFF_CrowdGate& Gate) { return Gate.GateId == FallbackGateId; });

	if (FallbackIndex == INDEX_NONE)
	{
		// 퇴장 전용 게이트가 살아 있을 수 있으므로 배열을 비우지 않고 뒤에 붙인다.
		FFF_CrowdGate Fallback;
		Fallback.GateId = FallbackGateId;
		Fallback.bAllowSpawn = true;
		Fallback.bAllowExit = true;
		FallbackIndex = ValidGates.Add(Fallback);
	}

	FFF_CrowdGate& Fallback = ValidGates[FallbackIndex];
	Fallback.GateLocation = ArrivalSpawnCenter;
	Fallback.GateRadiusUU = FMath::Max(ArrivalSpawnRadiusUU, 1.f);
	Fallback.GateWeight = 1.f;

	RebuildSpawnGateCdf();
	bUsingFallbackGate = true;
}

int32 UFF_CrowdSimulationSubsystem::PickSpawnGate(int32 InVisitorCursor) const
{
	static const FName EntryGateTraitKey(TEXT("entry_gate_choice"));
	static const FName EntryGatePurpose(TEXT("Gate"));
	
	const uint32 Seed = FFF_DeterministicRandom::MakeTraitSeed(GlobalSimulationSeed,InVisitorCursor,EntryGateTraitKey,ArrivalGeneration,EntryGatePurpose);
	const float Fraction = FFF_DeterministicRandom::MakeUniformFraction(Seed);
	
	const int32 Slot = Algo::UpperBound(SpawnGateCdf,Fraction);
	return SpawnGateIndices[FMath::Min(Slot, SpawnGateIndices.Num()-1)];
	
}



void UFF_CrowdSimulationSubsystem::GatherCrowdGates(const TArray<FFF_CrowdGate>& InGates)
{
	ValidGates.Reset();
	RejectedGateCount = 0;
	UnverifiedGateCount = 0;

	TSet<FName> SeenGateIds;
	SeenGateIds.Reserve(InGates.Num());

	for (int32 AuthoredIndex = 0; AuthoredIndex < InGates.Num(); ++AuthoredIndex)
	{
		const FFF_CrowdGate& Authored = InGates[AuthoredIndex];

		if (!Authored.GateId.IsNone() && SeenGateIds.Contains(Authored.GateId))
		{
			++RejectedGateCount;
			UE_LOG(LogTemp, Error,
				TEXT("[CrowdSimulationSubsystem] 게이트 '%s' 제외 — GateId가 중복이다. 두 문의 지표가 ")
				TEXT("합쳐지면 한쪽이 죽어도 안 보인다"),
				*Authored.GateId.ToString());
			continue;
		}

		FFF_CrowdGate Validated;
		if (!ValidateGate(Authored, AuthoredIndex, Validated))
		{
			++RejectedGateCount;
			continue;
		}

		SeenGateIds.Add(Validated.GateId);
		ValidGates.Add(Validated);
	}

	RebuildSpawnGateCdf();
	bGatesGathered = true;

	// 폴백 합성은 BeginArrivalSchedule 이 한다. ArrivalSpawnCenter 가 아직 안 채워져 있다.
	UE_LOG(LogTemp, Log,
		TEXT("[CrowdSimulationSubsystem] 게이트 등록: 저작 %d개 → 유효 %d개(입장 %d개), 제외 %d개, 미검증 %d개"),
		InGates.Num(), ValidGates.Num(), SpawnGateIndices.Num(), RejectedGateCount, UnverifiedGateCount);

	if (SpawnGateIndices.Num() == 0 && InGates.Num() > 0)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[CrowdSimulationSubsystem] 쓸 수 있는 입장 게이트가 하나도 없다 — 단일 앵커 폴백으로 돈다. ")
			TEXT("위의 제외 사유를 확인할 것"));
	}
}

void UFF_CrowdSimulationSubsystem::BeginArrivalSchedule(int32 TotalVisitorCount, FVector SpawnCenter,
	float SpawnRadiusUU, const TArray<FFF_ReservedPartyRequest>& ReservedParties)
{
	// 다음 날 재개는 이 값을 그대로 다시 넘긴다. CachedDistribution.RequestedVisitorCount 를
	// 재사용하지 않는 것은 바로 아래에서 그 구조체를 덮기 때문이고, ActualVisitorCount 를
	// 쓰지 않는 것은 그룹을 통째로만 만드느라 요청보다 작아서 날마다 인원이 깎이기 때문이다.
	ArrivalRequestedVisitorCount = TotalVisitorCount;
	++ArrivalGeneration;

	CachedDistribution = BuildVisitorDistribution(TotalVisitorCount, ReservedParties);
	VisitorCursor = 0;
	ArrivalTotalVisitorCount = CachedDistribution.ActualVisitorCount;
	// 게이트를 쓰더라도 이 두 값은 남긴다. 폴백 게이트의 원본이다.
	ArrivalSpawnCenter = SpawnCenter;
	ArrivalSpawnRadiusUU = SpawnRadiusUU;
	bArrivalPoolExhaustedLogged = false;

	EnsureFallbackGate();
	
	
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

		// 파티가 통째로 들어갈 자리가 없으면 다음 프레임에 다시 시도한다. 중간에 끊기면 커서는
		// 파티 전체만큼 넘어가 남은 멤버가 영영 안 온다.
		if (FreeAgentSlotIndex.Num() < PartySize)
		{
			bAgentPoolExhausted = true;
			break;
		}

		checkf(!LeadVisitor.bIsDependentChildSlot || !bEnablePartyGrouping,
			TEXT("SpawnAgents: 그룹 선두가 아동 슬롯이다 — 빌더가 성인을 먼저 배출한다는 전제가 깨졌다. ")
			TEXT("아동이 리더가 되면 목적지를 상속할 대상이 없다"));
		
		const int32 GateIndex = PickSpawnGate(VisitorCursor);
		const FFF_CrowdGate& Gate = ValidGates[GateIndex];
		
		const float Angle = Rng.FRandRange(0.f, 2.f * PI);
		const float Radius = Gate.GateRadiusUU * FMath::Sqrt(Rng.FRand());
		const FVector Anchor = Gate.GateLocation + FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.f);

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
			Unit.CrowdRole = EFF_CrowdRole::Visitor;
			Unit.EntryGateIndex = GateIndex;
			
			ApplyAgentAppearance(AgentIndex);
			
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

			// 값을 보관한다. 안 하면 아래에서 속도·체류시간에 녹아든 2종만 남고 나머지가 버려져,
			// 변주를 8종 뽑아 놓고 화면에는 걷는 속도 차이만 나오는 상태가 된다.
			if (AgentTraits.IsValidIndex(AgentIndex) && AgentPersonalThresholds.IsValidIndex(AgentIndex))
			{
				AgentTraits[AgentIndex] = Traits;
				AgentPersonalThresholds[AgentIndex] =
					FFF_IndividualVariationResolver::ResolvePersonalThresholds(
						Traits, CachedCoefficientTable.Get());
			}

			const float MemberBoost = (Slot == 0) ? 1.f : FMath::Max(PartyMemberSpeedBoost,1.f);
			const float MemberBoost2 = (Slot == 0) ? 1.f : FMath::Max(PartyMemberSpeedMultiplier,1.f);
			Unit.Speed = BaseSpeed * Traits.WalkSpeedMultiplier* MemberBoost;
			Unit.MaxSpeed = Unit.Speed * MemberBoost2;
			Unit.CruiseSpeed = Unit.Speed;
			Unit.CruiseMaxSpeed = Unit.MaxSpeed;

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
		// 풀은 동시 인원 상한이다. 차면 누가 나갈 때까지 입장이 밀리고, Live 가 끝날 때까지 못 들어온
		// 손님은 그날 오지 않는다. 자주 보이면 AgentPoolSize·ActorPoolSize 를 함께 올린다.
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] Agent 풀(%d)이 찼다 — %d/%d명째에서 입장이 밀린다"),
			UnitDatas.Num(), VisitorCursor, ArrivalTotalVisitorCount);
	}

	if (SpawnedCount > 0)
	{
		OnVisitorsAdmitted.Broadcast(SpawnedCount);
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

	++SimStepIndex;

	const float VelocityAlpha = CrowdSteering::ComputeVelocityAlpha(VelocitySmoothTime, FixedDt);

	CrowdSteering::FStepParams StepParams = MakeStepParams();

	const bool bDriveByForce = bEnableAvoidance && bAvoidanceDrivesVelocity;
	StepParams.bDrivingForceInForce = bDriveByForce;

	SampleQueueLiveness();
	UpdateProgramReservationStates();
	UpdateAudienceShowState();

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
		if (Unit.bHasQueueSlot)
		{
			// 일행도 리더와 같은 자리 도착 기준을 쓴다. 기본 10m 반경이면
			// 멤버가 줄 밖에 있어도 도착한 것으로 판단하고 줄 방향을 바라본다.
			Unit.ArrivalRadiusUU = Unit.bUsePreciseSlotPosition
				? GetAssignedSlotArrivalRadius() : QueueSlotArrivalRadiusUU;
		}
		else if (Unit.LeaderAgentIndex == AgentIndex && PartyDecisionStates.IsValidIndex(AgentIndex))
		{
			const FPartyDecisionState& RadiusState = PartyDecisionStates[AgentIndex];

			if (RadiusState.bIsLeaving)
			{
				Unit.ArrivalRadiusUU = Temp_ExitArrivalRadiusUU;
			}
			else if (RadiusState.ProgramPhase == EFF_PartyProgramPhase::Returning)
			{
				// 슬롯 저작이 없으면 전원이 접근점 한 점으로 모인다. 45cm 로는 그 안에
				// 다 못 들어가 시설 앞에서 맴돌다 프로그램이 끝난다.
				Unit.ArrivalRadiusUU = FacilityApproachRadiusUU;
			}
			else if (RadiusState.QueuePhase != EFF_PartyQueuePhase::None
				|| RadiusState.ProgramPhase != EFF_PartyProgramPhase::None)
			{
				Unit.ArrivalRadiusUU = QueueSlotArrivalRadiusUU;
			}
			else if (RadiusState.AudiencePhase == EFF_PartyAudiencePhase::GoingToEntry
				|| RadiusState.AudiencePhase == EFF_PartyAudiencePhase::Exiting)
			{
				// 입구·출구는 좌표 하나를 여러 파티가 공유한다. 좌석과 같은 허용 오차를 쓰면
				// 그 점 반경 안에 다 들어갈 수 없어 게이트 앞에서 영원히 맴돈다.
				Unit.ArrivalRadiusUU = FacilityApproachRadiusUU;
			}
			else if (RadiusState.AudiencePhase != EFF_PartyAudiencePhase::None)
			{
				Unit.ArrivalRadiusUU = RadiusState.AudienceArrivalTolerance > 0.f
					? RadiusState.AudienceArrivalTolerance
					: QueueSlotArrivalRadiusUU;
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
				if (!HandleProgramArrival(AgentIndex)
					&& !HandleAudienceArrival(AgentIndex)
					&& !AdvancePartyQueueOnArrival(AgentIndex))
				{
					RequestDecision(AgentIndex, EFF_DecisionReason::Arrived);
				}
			}
			else if ((State.AudiencePhase == EFF_PartyAudiencePhase::GoingToSlot
					|| State.ProgramPhase == EFF_PartyProgramPhase::Returning)
				&& !Unit.bUsePreciseSlotPosition
				&& DecisionStuckSteps > 0
				&& Unit.StuckSteps >= DecisionStuckSteps)
			{
				// 관람·예약 잠금 중에는 목적지 포기가 꺼져 있다. 자리 근처까지 왔는데 앞사람들에
				// 막혀 지정 좌표를 못 밟으면, 그대로 두면 프로그램이 끝날 때까지 밀고 있는다.
				Unit.StuckSteps = 0;
				if (FVector::DistSquared2D(Unit.Position, Unit.TargetLocation)
					<= FMath::Square(ScheduledSettleRadiusUU))
				{
					if (State.ProgramPhase == EFF_PartyProgramPhase::Returning)
					{
						++ProgramSettledInPlace;
						HandleProgramArrival(AgentIndex);
					}
					else
					{
						++AudienceSettledInPlace;
						HandleAudienceArrival(AgentIndex);
					}
				}
			}
			else if (DecisionStuckSteps > 0
				&& Unit.StuckSteps >= DecisionStuckSteps
				&& State.QueuePhase == EFF_PartyQueuePhase::None
				&& State.ProgramPhase == EFF_PartyProgramPhase::None
				&& State.AudiencePhase == EFF_PartyAudiencePhase::None)
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
			else if (bEnablePeriodicReevaluation
				&& State.QueuePhase == EFF_PartyQueuePhase::None
				&& State.ProgramPhase == EFF_PartyProgramPhase::None
				&& State.AudiencePhase == EFF_PartyAudiencePhase::None
				&& !State.bNeedsDecision
				&& AgentPersonalThresholds.IsValidIndex(AgentIndex))
			{
				// 개인 주기 재평가. reevaluate_interval_multiplier 의 결선 지점이다.
				//
				// ★ 큐·프로그램 구간을 뺀다. 줄에 선 파티를 주기로 끌어내면 인내심과 무관하게
				//   전원이 같은 주기로 이탈하고, patience_multiplier 를 개인별로 뽑은 의미가
				//   사라진다 — 줄에서의 이탈은 AdvancePartyQueueOnArrival 이 판정한다.
				//
				// ★ bNeedsDecision 을 확인한다. 이미 결정 대기 줄에 들어간 파티에 또 걸면
				//   프레임 예산에 밀린 파티가 매 스텝 요청을 반복해 DecisionQueue 가 넘친다.
				//
				// ★ LastDecisionSimTime 을 기준으로 잰다. 목적지를 고른 시점이 재평가 주기의
				//   시작이라, 결정이 늦게 처리된 파티는 그만큼 다음 재평가도 늦어진다.
				//   0(아직 한 번도 결정 안 됨)이면 Spawned 요청이 처리되기 전이므로 건너뛴다.
				const float SecondsSinceLastDecision = (State.LastDecisionSimTime > 0.f)
					? FMath::Max(SimTimeSeconds - State.LastDecisionSimTime, 0.f)
					: 0.f;

				// 대기·밀집 항은 끈다 — 각각 큐 이탈과 조정자의 Relocate 경로가 맡는다.
				const bool bReevaluate = (State.LastDecisionSimTime > 0.f)
					&& FFF_UtilityDestinationSelector::ShouldReevaluate(
						/*CurrentWaitMinutes=*/0.f, /*PersonalWaitThresholdMinutes=*/TNumericLimits<float>::Max(),
						/*CurrentDensityRatio=*/0.f, /*PersonalDensityThresholdRatio=*/1.f,
						SecondsSinceLastDecision,
						AgentPersonalThresholds[AgentIndex].ReevaluateIntervalSeconds,
						/*bDestinationOpenStateChanged=*/false, /*bLiveEventStateChanged=*/false);

				if (bReevaluate)
				{
					// 계수기만으로는 "주기가 짧아서 자주 걸리는가"와 "주기가 안 차는가"를 못 가른다.
					// 개인 주기를 같이 찍어 두면 0회일 때 원인이 바로 보인다.
					UE_LOG(LogFestaFiestaNpc, Verbose,
						TEXT("[주기 재평가] 파티 %d가 %.1f초 만에 다시 고른다 (개인 주기 %.1f초)"),
						UnitDatas[AgentIndex].PartyID, SecondsSinceLastDecision,
						AgentPersonalThresholds[AgentIndex].ReevaluateIntervalSeconds);

					++ReevaluateRequests;
					RequestDecision(AgentIndex, EFF_DecisionReason::Reevaluated);
				}
			}
		}
		else
		{
			if (!UpdatePartyMemberTarget(AgentIndex, Unit))
			{
				continue;
			}

			UpdateFormationBlockState(AgentIndex, Unit);
		}

		// 공연 보러 갈 때는 걸음을 재촉한다. 리더·멤버 두 경로가 방금 Speed 를 세웠으므로
		// 여기서 한 번만 곱한다 — 상태 전환 시점에 걸면 멤버 추종이 매 스텝 되돌린다.
		//
		// ★ MaxSpeed 도 같이 올려야 한다. StepAgent 가 Velocity 를 MaxSpeed 로 자르기 때문에
		//   Speed 만 올리면 숫자만 커지고 실제 이동은 그대로다.
		{
			const int32 AudienceLeaderIndex = Unit.LeaderAgentIndex;
			// 마지막 3m 는 평소 속도로 걷는다. 전력으로 들어오면 감속 거리가 모자라 자리를
			// 지나치고, 빠르게 맴도는 동안은 StuckSteps 도 안 쌓여 제자리착석 폴백까지 죽는다.
			const bool bFinalApproach = FVector::DistSquared2D(Unit.Position, Unit.TargetLocation)
				<= FMath::Square(FMath::Max(ScheduledSettleRadiusUU, 1.f));
			const bool bHeadingToSchedule = PartyDecisionStates.IsValidIndex(AudienceLeaderIndex)
				&& (PartyDecisionStates[AudienceLeaderIndex].AudiencePhase
						== EFF_PartyAudiencePhase::GoingToSlot
					|| PartyDecisionStates[AudienceLeaderIndex].ProgramPhase
						== EFF_PartyProgramPhase::Returning);
			const bool bHurrying = !bFinalApproach && bHeadingToSchedule;
			if (bHurrying)
			{
				const float Hurry = FMath::Max(ScheduledHurrySpeedMultiplier, 1.f);
				Unit.MaxSpeed = Unit.CruiseMaxSpeed * Hurry;

				// ★ 곱셈 누적 금지. 리더의 Speed 는 매 스텝 다시 세워지지 않고, 멤버의 추격
				//   분기는 방금 올려둔 MaxSpeed 를 그대로 받아쓴다 — 어느 쪽이든 `*=` 로 쓰면
				//   매 스텝 배가돼 1초 안에 감속 구간이 사라지고 자리를 영원히 지나친다.
				if (Unit.LeaderAgentIndex == AgentIndex)
				{
					Unit.Speed = Unit.CruiseSpeed * Hurry;
				}
				else if (Unit.Speed > KINDA_SMALL_NUMBER)
				{
					// 멤버는 UpdatePartyMemberTarget 이 방금 세웠다. 0(자리 도착)은 그대로 둔다.
					Unit.Speed = FMath::Min(Unit.Speed, Unit.CruiseMaxSpeed) * Hurry;
				}
			}
			else
			{
				Unit.MaxSpeed = Unit.CruiseMaxSpeed;
				if (Unit.LeaderAgentIndex == AgentIndex)
				{
					Unit.Speed = FMath::Min(Unit.Speed, Unit.CruiseSpeed);
				}
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

		if (bUnstuckEnabled
			&& (Unit.StuckSteps >= AvoidanceConfig.StuckDetectSteps || Unit.UnstuckHoldSteps > 0))
		{
			++Out.UnstuckActive;
		}

		if (Unit.StuckSteps > 0)
		{
			++Out.Blocked;
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

	PublishCongestionSnapshot();

	DensityGrid.ResetThroughputWindow();

	++SnapshotCount;
}

void UFF_CrowdSimulationSubsystem::PublishCongestionSnapshot()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_PublishCongestion);

	const TArray<int32>& Congested = DensityGrid.GetCongestedCells();
	const TArray<int32>& Counts = DensityGrid.GetCellCounts();
	const TArray<int32>& Throughput = DensityGrid.GetCellThroughput();
	const TArray<float>& Density = DensityGrid.GetDensityField();
	const int32 NumX = FMath::Max(1, DensityGrid.GetNumX());

	CongestionSnapshot.Cells.Reset(Congested.Num());
	CongestionSnapshot.SimTimeMinutes = GetLiveMinutes();
	CongestionSnapshot.SnapshotIndex = SnapshotCount;
	CongestionSnapshot.CellSizeMeters = DensityGrid.GetCellSizeMeters();

	for (const int32 CellIndex : Congested)
	{
		const int32 CellX = CellIndex % NumX;
		const int32 CellY = CellIndex / NumX;

		FFF_CrowdCongestedCell& Cell = CongestionSnapshot.Cells.AddDefaulted_GetRef();
		Cell.Cell = FIntPoint(CellX, CellY);
		Cell.CenterWorldUU = DensityGrid.GetCellCenterWorld(CellX, CellY);
		Cell.DensityPerSqm = Density.IsValidIndex(CellIndex) ? Density[CellIndex] : 0.f;
		Cell.Occupancy = Counts.IsValidIndex(CellIndex) ? Counts[CellIndex] : 0;
		Cell.Throughput = Throughput.IsValidIndex(CellIndex) ? Throughput[CellIndex] : 0;
	}

	// 목록이 비어도 쏜다. 빈 스냅샷이 "다 풀렸다"는 신호라서, 걸러 보내면 구독자가 사건을 닫지 못한다.
	OnCongestionSampled.Broadcast(CongestionSnapshot);
}

CrowdSteering::FStepParams UFF_CrowdSimulationSubsystem::MakeStepParams() const
{
	CrowdSteering::FStepParams Params;
	Params.MaxTurnRateDegrees = MaxTurnRateDegrees;
	Params.BrakingDistanceUU = (BrakingDistanceUU > 0.f)
		? BrakingDistanceUU
		: FMath::Max(ArrivalRadiusUU * 3.f, 1.f);
	Params.StuckSpeedThresholdUU = AvoidanceConfig.StuckSpeedThresholdUU;
	Params.StuckDetectSteps = AvoidanceConfig.StuckDetectSteps;
	Params.UnstuckHoldSteps = AvoidanceConfig.UnstuckHoldSteps;
	Params.FacingSmoothSeconds = FacingSmoothTime;
	Params.FacingEnterSpeedUU = FacingEnterSpeedUU;
	Params.FacingExitSpeedUU = FacingExitSpeedUU;
	Params.FacingDeadzoneDegrees = FacingDeadzoneDegrees;

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

	// 관람 중인 개체만 개인 공간을 좁힌다. 리더의 단계를 파티원 전원이 물려받는다 —
	// 일행 중 리더만 붙어 서면 대열이 갈라진다.
	ScratchSocialScale.SetNum(UnitDatas.Num(), EAllowShrinking::No);
	for (int32 AgentIndex = 0; AgentIndex < UnitDatas.Num(); ++AgentIndex)
	{
		float Scale = 1.f;
		const int32 LeaderIndex = UnitDatas[AgentIndex].LeaderAgentIndex;
		if (UnitDatas[AgentIndex].bIsActive && PartyDecisionStates.IsValidIndex(LeaderIndex))
		{
			switch (PartyDecisionStates[LeaderIndex].AudiencePhase)
			{
			case EFF_PartyAudiencePhase::Waiting:
			case EFF_PartyAudiencePhase::Spectating:
				Scale = FMath::Clamp(AudienceSocialForceScale, 0.f, 1.f);
				break;
			case EFF_PartyAudiencePhase::GoingToSlot:
				Scale = FMath::Clamp(AudienceApproachSocialForceScale, 0.f, 1.f);
				break;
			default:
				break;
			}
		}
		ScratchSocialScale[AgentIndex] = Scale;
	}
	Input.SocialScale = &ScratchSocialScale;

	Input.bApplyDrivingForce = Params.bDrivingForceInForce;
	Input.FixedDtSeconds = FixedStepSeconds;

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
		Unit.UnstuckHoldSteps = 0;
	}
	AvoidanceNeighbourCounts.Reset();

	ScratchDesiredVel2DMeters.Reset();
	ScratchSocialScale.Reset();
}

void UFF_CrowdSimulationSubsystem::RebuildAvoidanceWalls()
{
	AvoidanceWalls.Reset();

	FacilityWallCount = AppendFacilityWalls();

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
	RoadSupportRevision = -1;

	if (!bEnableHeightField)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 높이 격자는 분석 격자와 같은 범위를 덮는다
	if (!SimAreaBoundsCm.IsValid)
	{
		UE_LOG(LogTemp, Log,
			TEXT("높이 격자: 덮을 범위가 없다(NavMesh 경계 미확보) — 개체 Z는 스폰 값 그대로 유지된다"));
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

	FVector2D CenterCm(SimAreaBoundsCm.GetCenter().X, SimAreaBoundsCm.GetCenter().Y);
	double HalfSizeCm = FMath::Max(SimAreaBoundsCm.GetSize().X, SimAreaBoundsCm.GetSize().Y) * 0.5;
	if (HeightFieldRadiusUU > 0.f)
	{
		HalfSizeCm = FMath::Min(HalfSizeCm, static_cast<double>(HeightFieldRadiusUU));
	}

	const double TopZ = FMath::Max(SimOrigin.Z, SimAreaBoundsCm.Max.Z);

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
		TEXT("높이 격자: 중심 (%.0f, %.0f) 반폭 %.0f (%s), 간격 %.0f, 트레이스 시작 Z=%.0f"),
		CenterCm.X, CenterCm.Y, HalfSizeCm,
		bSimAreaBoundsOverridden ? TEXT("지정 범위") : TEXT("NavMesh 경계"),
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
	bool bHasGround = HeightField.TryGetHeightCm(FVector2D(WorldLocation.X, WorldLocation.Y), OutZ);
	if (!bEnableHeightField || WorldLocation.ContainsNaN()) return bHasGround;
	EnsureRoadSupportCache();
	const FIntPoint Bucket(FMath::FloorToInt(WorldLocation.X / RoadSupportBucketSizeCm),
		FMath::FloorToInt(WorldLocation.Y / RoadSupportBucketSizeCm));
	if (const TArray<int32>* Surfaces = RoadSupportBuckets.Find(Bucket))
	{
		for (const int32 Index : *Surfaces)
		{
			const FRoadSupportSurface& Surface = RoadSupportSurfaces[Index];
			const FVector Local = Surface.Transform.InverseTransformPosition(WorldLocation);
			if (FMath::Abs(Local.X) <= Surface.HalfExtentCm.X + UE_KINDA_SMALL_NUMBER &&
				FMath::Abs(Local.Y) <= Surface.HalfExtentCm.Y + UE_KINDA_SMALL_NUMBER)
			{
				OutZ = bHasGround ? FMath::Max(OutZ, Surface.TopZ) : Surface.TopZ;
				bHasGround = true;
			}
		}
	}
	return bHasGround;
}

void UFF_CrowdSimulationSubsystem::EnsureRoadSupportCache() const
{
	UWorld* World = GetWorld();
	UFFZSWorldSubsystem* System = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	const int64 Revision = System ? System->GetWorldRevision() : -1;
	if (RoadSupportSystem.Get() == System && RoadSupportRevision == Revision) return;
	if (RoadSupportSystem.Get() != System)
	{
		UFF_CrowdSimulationSubsystem* MutableThis = const_cast<UFF_CrowdSimulationSubsystem*>(this);
		if (UFFZSWorldSubsystem* Previous = RoadSupportSystem.Get())
		{
			Previous->OnWorldSnapshotRestored.RemoveDynamic(MutableThis,
				&UFF_CrowdSimulationSubsystem::HandleRoadSupportSnapshotRestored);
		}
		if (System)
		{
			System->OnWorldSnapshotRestored.AddUniqueDynamic(MutableThis,
				&UFF_CrowdSimulationSubsystem::HandleRoadSupportSnapshotRestored);
		}
	}
	RoadSupportSystem = System;
	RoadSupportRevision = Revision;
	RoadSupportSurfaces.Reset();
	RoadSupportBuckets.Reset();
	if (!System) return;
	const FFFZSReadModel ReadModel = System->BuildReadModel();
	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		if (Facility.Category != EFFZSFacilityCategory::Road && Facility.Category != EFFZSFacilityCategory::Tile) continue;
		const AFFZSFacilityActor* Actor = System->FindFacility(Facility.FacilityInstanceId);
		if (!IsValid(Actor)) continue;
		const FFFZSFacilitySpec& Spec = Actor->GetFacilitySpec();
		FRoadSupportSurface Surface;
		Surface.Transform = Actor->GetActorTransform();
		Surface.HalfExtentCm = FVector2D(Spec.FootprintSizeCells.X, Spec.FootprintSizeCells.Y) * 0.5;
		Surface.TopZ = Actor->GetActorLocation().Z + Spec.PlacementObstacleHeightCm;
		FBox2D Bounds(ForceInit);
		for (const double XSign : {-1.0, 1.0})
		{
			for (const double YSign : {-1.0, 1.0})
			{
				const FVector Corner = Surface.Transform.TransformPosition(FVector(
					Surface.HalfExtentCm.X * XSign, Surface.HalfExtentCm.Y * YSign, 0.0));
				Bounds += FVector2D(Corner.X, Corner.Y);
			}
		}
		const int32 Index = RoadSupportSurfaces.Add(Surface);
		const FIntPoint Min(FMath::FloorToInt(Bounds.Min.X / RoadSupportBucketSizeCm),
			FMath::FloorToInt(Bounds.Min.Y / RoadSupportBucketSizeCm));
		const FIntPoint Max(FMath::FloorToInt(Bounds.Max.X / RoadSupportBucketSizeCm),
			FMath::FloorToInt(Bounds.Max.Y / RoadSupportBucketSizeCm));
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
		{
			for (int32 X = Min.X; X <= Max.X; ++X) RoadSupportBuckets.FindOrAdd(FIntPoint(X, Y)).Add(Index);
		}
	}
}

void UFF_CrowdSimulationSubsystem::HandleRoadSupportSnapshotRestored()
{
	// Restore may reuse the same serialized revision with a different set of actors.
	RoadSupportRevision = -1;
	FacilityWallsRevision = -1;

	// 순회 레인도 같은 리비전을 보고 캐시를 유지하므로 같이 무효화한다. 빠뜨리면 복원된
	// 도로와 레인이 어긋난 채 남아 개체가 없어진 길 위의 목적지를 받는다.
	RoadLaneRevision = -1;
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
		if (Spec.IsGroundSurface()) continue;

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

	const int32 Dropped = DropAllPaths();
	++PathLayoutInvalidations;

	UE_LOG(LogTemp, Log,
		TEXT("배치 변경으로 경로 폐기: 리비전 %lld, 경로 %d개 (예산 %d건/스텝으로 재계산)"),
		Revision, Dropped, PathQueriesPerStep);
}

// 커서를 되돌려 첫 파티부터 예산을 받게 한다. 그대로 두면 커서 앞쪽 파티가
// 한 바퀴 도는 동안 낡은 경로 없이 직선 조향으로 남는다.
int32 UFF_CrowdSimulationSubsystem::DropAllPaths()
{
	int32 Dropped = 0;
	for (FFF_CrowdPathFollower& Follower : PathFollowers)
	{
		if (Follower.bHasPath || Follower.bQueryFailed)
		{
			Follower.Reset();
			++Dropped;
		}
	}

	for (int32& LaneIndex : AgentLaneIndices)
	{
		LaneIndex = INDEX_NONE;
	}

	PathQueryCursor = 0;
	return Dropped;
}

// 저작 액터에서 폴리라인을 다시 굽는다.
//
// AFF_CrowdLane은 아무 곳에도 자기를 등록하지 않으므로 BeginPlay 순서에 기대지 않고
// TActorIterator로 직접 훑는다. 빈도가 낮은 연산이고(맵 로드 / 레인 변경 시 1회)
// 스플라인을 매 경로 쿼리에서 들여다보는 것을 이걸로 막는다.
//
// 레벨에 레인이 하나도 없으면 Lanes가 비고, TryBuildPath가 항상 실패해 NavMesh로만
// 돈다 — 레인을 도입하기 전과 동작이 같다.
int32 UFF_CrowdSimulationSubsystem::RefreshLanesFromWorld()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return 0;
	}

	TArray<FFF_CrowdSplineLane> Collected;
	int32 SkippedCount = 0;

	for (TActorIterator<AFF_CrowdLane> It(World); It; ++It)
	{
		const AFF_CrowdLane* LaneActor = *It;
		if (!IsValid(LaneActor))
		{
			continue;
		}

		TArray<FVector> PointsCm;
		LaneActor->BuildPolylineCm(PointsCm);

		if (PointsCm.Num() < 2)
		{
			++SkippedCount;
			UE_LOG(LogTemp, Warning,
				TEXT("[CrowdSimulationSubsystem] 레인 %s: 점이 %d개라 건너뛴다 (스플라인 점 2개 이상 필요)"),
				*LaneActor->GetActorNameOrLabel(), PointsCm.Num());
			continue;
		}

		FFF_CrowdSplineLane Lane;
		Lane.Points = MoveTemp(PointsCm);
		Lane.HalfWidthCm = LaneActor->LaneHalfWidthCm;
		Lane.bOneWay = LaneActor->bOneWay;
		Lane.RebuildDerived();

		if (!Lane.IsUsable())
		{
			++SkippedCount;
			continue;
		}

		Collected.Add(MoveTemp(Lane));
	}

	const int32 LaneNum = Collected.Num();
	SetLanes(MoveTemp(Collected));

	// 도로 레인도 같은 호출에서 다시 굽는다. 콘솔에서 refresh를 쳤는데 한쪽만 갱신되면
	// 어느 쪽이 낡았는지 알 수 없다.
	RoadLaneRevision = -1;
	const int32 RoadLaneNum = EnsureRoadLanes();

	UE_LOG(LogTemp, Log,
		TEXT("[CrowdSimulationSubsystem] 레인 %d개 베이크 (저작 %d개 + 도로 %d개, 건너뜀 %d개), "
		     "경로추종 %s, 레인경로 %s"),
		Lanes.Num(), LaneNum, RoadLaneNum, SkippedCount,
		bEnablePathFollowing ? TEXT("on") : TEXT("off"),
		bEnableLanePathing ? TEXT("on") : TEXT("off"));

	return Lanes.Num();
}

void UFF_CrowdSimulationSubsystem::SetLanes(TArray<FFF_CrowdSplineLane> NewLanes)
{
	AuthoredLanes = MoveTemp(NewLanes);
	RebuildCombinedLanes();
}

// 저작 레인과 도로 레인을 한 배열로 잇는다. 쿼리는 Lanes만 보므로 출처가 섞여도
// TryBuildPath는 총 거리로만 고른다 — 도로 레인에 우선권을 주지 않는 것이 의도다.
//
// 저작 레인을 앞에 두는 이유는 도로가 깔리고 걷혀도 저작 레인의 인덱스가 안 밀리게 하기
// 위해서다. AgentLaneIndices와 디버그 표시가 인덱스를 그대로 쓴다.
void UFF_CrowdSimulationSubsystem::RebuildCombinedLanes()
{
	Lanes.Reset(AuthoredLanes.Num() + RoadLanes.Num());
	Lanes.Append(AuthoredLanes);
	Lanes.Append(RoadLanes);

	// 낡은 경로는 이전 레인 배치로 뽑힌 것이다. 목적지가 그대로여서 UpdatePathFollowing의
	// Reset 조건에 걸리지 않으므로 여기서 버려야 한다 — 배치 변경 무효화와 같은 이유다
	DropAllPaths();
}

// 도로 타일에서 순회 레인을 굽는다. 배치 리비전으로 걸러 실제 변경이 있을 때만 돈다 —
// 매 스텝 호출되는 자리에서 불리므로 이 게이트가 없으면 그래프를 프레임마다 다시 만든다.
int32 UFF_CrowdSimulationSubsystem::EnsureRoadLanes()
{
	const UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	const int64 Revision = ZoneSystem ? ZoneSystem->GetWorldRevision() : -1;

	if (!bEnableRoadLanes)
	{
		if (RoadLanes.IsEmpty())
		{
			return 0;
		}

		RoadLanes.Reset();
		RoadLaneStats = FFF_CrowdRoadLaneBuildStats();
		RoadLaneRevision = Revision;
		RebuildCombinedLanes();
		return 0;
	}

	if (Revision == RoadLaneRevision)
	{
		return RoadLanes.Num();
	}

	RoadLaneRevision = Revision;

	TArray<FFF_CrowdRoadTile> Tiles;
	if (ZoneSystem)
	{
		const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
		for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
		{
			if (Facility.Category != EFFZSFacilityCategory::Road && Facility.Category != EFFZSFacilityCategory::Tile) continue;

			const AFFZSFacilityActor* Actor = ZoneSystem->FindFacility(Facility.FacilityInstanceId);
			if (!IsValid(Actor)) continue;

			const FFFZSFacilitySpec& Spec = Actor->GetFacilitySpec();
			FFF_CrowdRoadTile& Tile = Tiles.AddDefaulted_GetRef();
			Tile.CenterCm = Actor->GetActorLocation();
			Tile.HalfExtentCm = FVector2D(Spec.FootprintSizeCells.X, Spec.FootprintSizeCells.Y) * 0.5;
			Tile.YawDegrees = static_cast<float>(Actor->GetActorRotation().Yaw);
		}
	}

	FFF_CrowdRoadLaneBuildParams Params;
	RoadLaneStats = CrowdRoadLanes::BuildLanes(Tiles, Params, RoadLanes);

	if (RoadLaneStats.bTruncated)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CrowdSimulationSubsystem] 도로 레인이 상한 %d개를 넘어 잘렸다. 잘린 쪽 도로는 "
			     "순회에 안 쓰인다 — 도로 타일 %d개, 교차점 %d개"),
			Params.MaxLanes, RoadLaneStats.TileCount, RoadLaneStats.JunctionCount);
	}

	RebuildCombinedLanes();
	return RoadLanes.Num();
}

// 레인 경로의 Points[0]은 언제나 TryBuildPath가 낸 진입점이다 — 중간 정점은
// 1cm 병합을 거쳐 그 뒤에 붙으므로 첫 원소가 밀려나지 않는다.
bool UFF_CrowdSimulationSubsystem::GetLaneFollowInfo(int32 AgentIndex,
	FFF_CrowdLaneFollowInfo& OutInfo) const
{
	OutInfo = FFF_CrowdLaneFollowInfo();

	if (!UnitDatas.IsValidIndex(AgentIndex) || !PathFollowers.IsValidIndex(AgentIndex))
	{
		return false;
	}

	const FFF_CrowdPathFollower& Follower = PathFollowers[AgentIndex];
	if (!Follower.bHasPath || Follower.Points.IsEmpty())
	{
		return false;
	}

	OutInfo.bHasPath = true;
	OutInfo.LaneIndex = AgentLaneIndices.IsValidIndex(AgentIndex)
		? AgentLaneIndices[AgentIndex]
		: INDEX_NONE;
	OutInfo.EntryPointCm = Follower.Points[0];
	OutInfo.DistanceToEntryCm = static_cast<float>(
		FVector::Dist2D(UnitDatas[AgentIndex].Position, Follower.Points[0]));
	OutInfo.bApproachingEntry = (Follower.NextIndex == 0);
	OutInfo.ConsumedCount = Follower.NextIndex;
	OutInfo.TotalCount = Follower.Points.Num();

	return true;
}

int32 UFF_CrowdSimulationSubsystem::GetUsableLaneCount() const
{
	int32 Count = 0;
	for (const FFF_CrowdSplineLane& Lane : Lanes)
	{
		if (Lane.IsUsable())
		{
			++Count;
		}
	}
	return Count;
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

	if (AgentLaneIndices.Num() != PoolSize)
	{
		AgentLaneIndices.Init(INDEX_NONE, PoolSize);
	}

	EnsureRoadLanes();
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

	// The short-approach visibility check below must use the current layout.
	// Waiting until force calculation would cache a skipped query as failed on
	// the first step after placing a facility between an NPC and its slot.
	EnsureFacilityWalls();

	int32 Budget = FMath::Max(PathQueriesPerStep, 1);
	int32 Backlog = 0;

	const float MinQueryDistSq = FMath::Square(FMath::Max(MinPathQueryDistanceUU, 0.f));
	const float RepathToleranceSq = FMath::Square(FMath::Max(RepathToleranceUU, 1.f));
	// 경로를 하나 만들 때마다 다음 프레임의 커서는 이동한다. 이번 순회도
	// 그 커서를 쓰면 다음 멤버를 건너뛰고 앞 NPC를 중복 방문하게 된다.
	const int32 ScanStartIndex = PathQueryCursor;

	for (int32 Scanned = 0; Scanned < PoolSize; ++Scanned)
	{
		const int32 AgentIndex = (ScanStartIndex + Scanned) % PoolSize;

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

		if (Unit.LeaderAgentIndex != AgentIndex && !Unit.bHasQueueSlot)
		{
			// 자리 배정 전에는 리더를 따라가지만, 배정 후에는 멤버도 자기
			// 자리까지 보행 경로를 구해야 꺾인 줄 안쪽의 시설을 가로지르지 않는다.
			if (Unit.bFormationPathBlocked && UnitDatas.IsValidIndex(Unit.LeaderAgentIndex))
			{
				UpdateBlockedMemberPath(AgentIndex, Budget, Backlog, MinQueryDistSq);
			}
			else if (Follower.bHasPath || Follower.bQueryFailed)
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
			const bool bBlockedSlotApproach = Unit.bHasQueueSlot
				&& !FFF_CrowdWalls::IsSegmentClear(AvoidanceWalls,
					FVector2f(Unit.Position.X, Unit.Position.Y) * FFF_CrowdUnits::MetersPerUU,
					FVector2f(Unit.TargetLocation.X, Unit.TargetLocation.Y) * FFF_CrowdUnits::MetersPerUU,
					Unit.Radius * FFF_CrowdUnits::MetersPerUU);
			if (FVector::DistSquared2D(Unit.Position, Unit.TargetLocation) <= MinQueryDistSq
				&& !bBlockedSlotApproach)
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

void UFF_CrowdSimulationSubsystem::UpdateBlockedMemberPath(int32 AgentIndex, int32& Budget, int32& Backlog,
	float MinQueryDistSq)
{
	FUnitData& Unit = UnitDatas[AgentIndex];
	FFF_CrowdPathFollower& Follower = PathFollowers[AgentIndex];
	const FVector LeaderPosition = UnitDatas[Unit.LeaderAgentIndex].Position;

	if ((Follower.bHasPath || Follower.bQueryFailed)
		&& FVector::DistSquared2D(Follower.PlannedForTarget, LeaderPosition) > FMath::Square(FormationRepathToleranceUU))
	{
		Follower.Reset();
	}

	if (!Follower.bHasPath && !Follower.bQueryFailed)
	{
		if (FVector::DistSquared2D(Unit.Position, LeaderPosition) <= MinQueryDistSq)
		{
			Follower.PlannedForTarget = LeaderPosition;
			Follower.bQueryFailed = true;
		}
		else if (Budget <= 0)
		{
			++Backlog;
		}
		else
		{
			--Budget;
			PathQueryCursor = AgentIndex + 1;

			Follower.PlannedForTarget = LeaderPosition;
			AgentLaneIndices[AgentIndex] = INDEX_NONE;
			++PathQueriesIssued;
			++FormationPathQueries;

			BuildNavMeshPath(AgentIndex, LeaderPosition);
		}
	}

	Unit.SteerTargetLocation = LeaderPosition;
	Unit.bHasSteerTarget = true;

	if (!Follower.bHasPath)
	{
		return;
	}

	Follower.NextIndex = CrowdPathFollow::AdvanceToReachableWaypoint(
		Follower.Points, Follower.NextIndex, Unit.Position, WaypointAcceptanceRadiusUU);

	if (Follower.Points.IsValidIndex(Follower.NextIndex))
	{
		Unit.SteerTargetLocation = Follower.Points[Follower.NextIndex];
	}
}

// 저작된 레인으로 경로를 낸다. 실패하면 호출부가 NavMesh로 떨어진다.
//
// TryBuildPath가 내는 점 목록은 [진입점, 중간 정점들, 이탈점, 목적지] 순서이고
// 시작점을 포함하지 않는다 — FFF_CrowdPathFollower::Points의 규약과 그대로 맞는다.
// 마지막 원소가 목적지 자체이므로 NavMesh 완전 경로와 도착 판정이 같다.
//
// 레인은 NavMesh와 달리 진입점까지의 직선을 검사하지 않는다. 그 구간의 건물 우회는
// 사회력의 벽 반발이 맡고, 진입 반경(LaneEntrySnapRadiusCm)이 그 직선의 길이 상한이다.
bool UFF_CrowdSimulationSubsystem::TryBuildLanePathForAgent(int32 AgentIndex)
{
	if (!bEnableLanePathing || Lanes.IsEmpty())
	{
		return false;
	}

	const FUnitData& Unit = UnitDatas[AgentIndex];
	FFF_CrowdPathFollower& Follower = PathFollowers[AgentIndex];

	CrowdSplinePath::FQueryParams Params;
	Params.EntrySnapRadiusCm = LaneEntrySnapRadiusCm;
	Params.ExitSnapRadiusCm = LaneExitSnapRadiusCm;
	Params.MaxDetourRatio = LaneMaxDetourRatio;

	TArray<FVector> LanePoints;
	int32 LaneIndex = INDEX_NONE;

	if (!CrowdSplinePath::TryBuildPath(Lanes, Unit.Position, Unit.TargetLocation,
		Params, LanePoints, LaneIndex))
	{
		++NavFallbackCount;
		return false;
	}

	if (LanePoints.Num() < 1)
	{
		++NavFallbackCount;
		return false;
	}

	Follower.Points = MoveTemp(LanePoints);
	Follower.NextIndex = 0;
	Follower.bHasPath = true;
	Follower.bQueryFailed = false;

	AgentLaneIndices[AgentIndex] = LaneIndex;
	++LanePathCount;

	return true;
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

	if (AgentLaneIndices.Num() != UnitDatas.Num())
	{
		AgentLaneIndices.Init(INDEX_NONE, UnitDatas.Num());
	}
	AgentLaneIndices[AgentIndex] = INDEX_NONE;

	// 줄 내부 이동은 해당 시설의 보행 경로를 쓴다. 도로 레인에 다시 합류하면
	// 한 칸 앞으로 갈 때마다 줄에서 빠져나가 도로를 우회하게 된다.
	if (!Unit.bHasQueueSlot && TryBuildLanePathForAgent(AgentIndex))
	{
		return true;
	}

	return BuildNavMeshPath(AgentIndex, Unit.TargetLocation);
}

bool UFF_CrowdSimulationSubsystem::BuildNavMeshPath(int32 AgentIndex, const FVector& Goal)
{
	const FUnitData& Unit = UnitDatas[AgentIndex];
	FFF_CrowdPathFollower& Follower = PathFollowers[AgentIndex];

	UWorld* World = GetWorld();
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance() : nullptr;

	if (!NavData)
	{
		Follower.bQueryFailed = true;
		++PathQueryFailures;
		return false;
	}

	FPathFindingQuery Query(nullptr, *NavData, Unit.Position, Goal);

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
		Follower.Points.Last() = Goal;
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
	if (State.ProgramPhase == EFF_PartyProgramPhase::Waiting
		|| State.ProgramPhase == EFF_PartyProgramPhase::Participating)
	{
		return;
	}
	if (State.ProgramPhase == EFF_PartyProgramPhase::Returning)
	{
		// 복귀 중 막힘·도착은 예약 상태 머신이 처리한다. Utility 재결정으로 새지 않는다.
		if (UnitDatas.IsValidIndex(LeaderAgentIndex))
		{
			UnitDatas[LeaderAgentIndex].TargetLocation = State.ProgramWaitingLocation;
		}
		return;
	}
	if (IsAudienceLocked(State))
	{
		return;
	}
	float ProgramNow = 0.f;
	if (TryGetProgramLiveMinutes(ProgramNow) && UnitDatas.IsValidIndex(LeaderAgentIndex))
	{
		const UGameInstance* GameInstance = GetWorld()->GetGameInstance();
		const UFF_ProgramReservationSubsystem* Reservations = GameInstance
			? GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>() : nullptr;
		if (Reservations && Reservations->IsPartyLockedAt(UnitDatas[LeaderAgentIndex].PartyID, ProgramNow))
		{
			return; // 다음 예약 갱신이 복귀를 시작한다. 일반 Queue 상태를 먼저 지우지 않는다.
		}
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
		if (State.QueuePhase != EFF_PartyQueuePhase::None || State.bIsLeaving
			|| State.ProgramPhase != EFF_PartyProgramPhase::None || IsAudienceLocked(State))
		{
			continue;
		}

		RequestDecision(AgentIndex, EFF_DecisionReason::DestinationOpened);
		++RequestedCount;
	}

	return RequestedCount;
}

bool UFF_CrowdSimulationSubsystem::IsAudienceLocked(const FPartyDecisionState& State) const
{
	return State.AudiencePhase != EFF_PartyAudiencePhase::None;
}

bool UFF_CrowdSimulationSubsystem::AllocateAudienceSlots(
	const FGuid& FacilityId, const int32 RequiredSlots, const int32 Capacity, int32& OutFirstSlotIndex)
{
	OutFirstSlotIndex = INDEX_NONE;
	if (!FacilityId.IsValid() || RequiredSlots <= 0 || Capacity < RequiredSlots)
	{
		return false;
	}

	TSet<int32>& Occupied = OccupiedAudienceSlots.FindOrAdd(FacilityId);
	for (int32 First = 0; First <= Capacity - RequiredSlots; ++First)
	{
		bool bFree = true;
		for (int32 Offset = 0; Offset < RequiredSlots; ++Offset)
		{
			if (Occupied.Contains(First + Offset))
			{
				bFree = false;
				First += Offset;
				break;
			}
		}
		if (!bFree)
		{
			continue;
		}
		for (int32 Offset = 0; Offset < RequiredSlots; ++Offset)
		{
			Occupied.Add(First + Offset);
		}
		OutFirstSlotIndex = First;
		return true;
	}
	return false;
}

bool UFF_CrowdSimulationSubsystem::HasPendingProgramObligation(const int32 LeaderAgentIndex) const
{
	if (!PartyDecisionStates.IsValidIndex(LeaderAgentIndex) || !UnitDatas.IsValidIndex(LeaderAgentIndex))
	{
		return false;
	}
	if (PartyDecisionStates[LeaderAgentIndex].ProgramPhase != EFF_PartyProgramPhase::None)
	{
		return true;
	}

	// 시작 전 자유 행동 중인 예약자도 붙잡으면 안 된다. 관람 잠금이 걸린 파티에 복귀 마감이
	// 오면 UpdateProgramReservationStates 가 ProgramPhase 를 따로 세워서, 한 파티에
	// ProgramPhase 와 AudiencePhase 가 동시에 살아 있는 상태가 만들어진다.
	float NowMinutes = 0.f;
	if (!TryGetProgramLiveMinutes(NowMinutes))
	{
		return false;
	}
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_ProgramReservationSubsystem* Reservations = GameInstance
		? GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>() : nullptr;
	return Reservations
		&& Reservations->IsPartyExitBlockedAt(UnitDatas[LeaderAgentIndex].PartyID, NowMinutes);
}

bool UFF_CrowdSimulationSubsystem::BeginAudienceVisit(
	const int32 LeaderAgentIndex, const FGuid& FacilityId, const FFFZSAudienceSnapshot& Snapshot)
{
	if (!PartyDecisionStates.IsValidIndex(LeaderAgentIndex)
		|| !UnitDatas.IsValidIndex(LeaderAgentIndex)
		|| IsAudienceLocked(PartyDecisionStates[LeaderAgentIndex])
		|| AudiencePartiesExcludedThisShow.Contains(UnitDatas[LeaderAgentIndex].PartyID))
	{
		return false;
	}

	// Utility로 무대에 먼저 도착해도 공연 3축제분 전보다 일찍 관람 잠금을 걸지 않는다.
	const UWorld* World = GetWorld();
	const UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	const AFFZSFacilityActor* Facility = Zones ? Zones->FindFacility(FacilityId) : nullptr;
	const UFF_FestivalStageSubsystem* Stage = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	if (Facility && FFF_ZoneSystemBridge::IsMainStageFacility(
		Facility->GetFacilitySpec().Category, Facility->GetFacilitySpec().FacilityDefinitionId))
	{
		if (!Stage || !Stage->IsMainProgramQueueBreakActive())
		{
			return false;
		}
		// 도착 콜백이 정기 집결보다 먼저 실행돼도 이용 중인 팀의 정산을 먼저 끝낸다.
		if (UFF_ZoneSystemQueueBridgeSubsystem* Bridge = World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>())
		{
			Bridge->SynchronizeMainProgramQueueBreak();
		}
	}

	// 유료 프로그램이 관람보다 우선한다. 돈을 받은 약속이고 정원이 이미 확정돼 있다.
	if (HasPendingProgramObligation(LeaderAgentIndex))
	{
		++AudienceProgramConflictSkips;
		return false;
	}

	TArray<int32> Members = GetPartyMembers(UnitDatas[LeaderAgentIndex].PartyID);
	Members.RemoveAll([this](const int32 Index)
	{
		return !UnitDatas.IsValidIndex(Index) || !UnitDatas[Index].bIsActive;
	});
	Members.Sort();
	if (Members.IsEmpty())
	{
		Members.Add(LeaderAgentIndex);
	}

	int32 FirstSlot = INDEX_NONE;
	if (!AllocateAudienceSlots(FacilityId, Members.Num(), Snapshot.Capacity, FirstSlot))
	{
		++AudienceCapacityRejects;
		return false;
	}

	ClearPartyQueueState(LeaderAgentIndex, true);
	FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];
	State.AudiencePhase = EFF_PartyAudiencePhase::GoingToEntry;
	State.AudienceFacilityId = FacilityId;
	State.AudienceFirstSlotIndex = FirstSlot;
	State.AudienceSlotCount = Members.Num();
	State.AudienceEntryLocation = Snapshot.EntryWorldCm;
	State.AudienceExitLocation = Snapshot.ExitWorldCm;
	// 좌석은 사람마다 다르다. 반 간격이나 3m 근방을 도착으로 인정하면 이웃 자리에 선다.
	State.AudienceArrivalTolerance = GetAssignedSlotArrivalRadius();
	State.CurrentDestinationId = FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(FacilityId);
	State.CurrentNeedRelief = FFF_ZoneSystemBridge::GetNeedReliefFlags(
		EFFZSFacilityCategory::Performance, TEXT("Performance_MainStage_SunsetMainStage"));
	State.ConsecutiveNoChoiceCount = 0;
	State.bNeedsDecision = false;
	UnitDatas[LeaderAgentIndex].TargetLocation = Snapshot.EntryWorldCm;

	// 좌석을 지금 나눠 준다. 입구는 좌표 하나뿐이라 수백 명이 같은 점을 목표로 삼으면
	// 도착 판정 반경 안에 들어가려고 서로 밀어내며 게이트에서 멈춘다 — 관람석이 비고
	// 입구에만 뭉치는 그림이 된다. 좌석은 사람마다 다른 좌표라 자연히 흩어진다.
	if (AssignAudienceSeats(LeaderAgentIndex, Snapshot, Members))
	{
		State.AudiencePhase = EFF_PartyAudiencePhase::GoingToSlot;
	}

	UE_LOG(LogTemp, Verbose, TEXT("[Audience] 파티 %d(%d명) 좌석 %d~%d 배정(%s)"),
		UnitDatas[LeaderAgentIndex].PartyID, Members.Num(), FirstSlot, FirstSlot + Members.Num() - 1,
		State.AudiencePhase == EFF_PartyAudiencePhase::GoingToSlot ? TEXT("좌석 직행") : TEXT("입구 경유"));
	return true;
}

bool UFF_CrowdSimulationSubsystem::AssignAudienceSeats(
	const int32 LeaderAgentIndex, const FFFZSAudienceSnapshot& Snapshot, const TArray<int32>& Members)
{
	if (!PartyDecisionStates.IsValidIndex(LeaderAgentIndex))
	{
		return false;
	}
	const FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];

	// 한 자리라도 못 받으면 전부 되돌린다. 일부만 좌석을 들고 나머지가 리더를 따라가면
	// 같은 파티가 두 곳으로 갈라진다.
	TArray<FTransform> SeatTransforms;
	SeatTransforms.Reserve(Members.Num());
	for (int32 MemberOffset = 0; MemberOffset < Members.Num(); ++MemberOffset)
	{
		FTransform Seat;
		if (!UFFZSAudienceLibrary::GetAudienceSlotTransform(
			Snapshot, State.AudienceFirstSlotIndex + MemberOffset, Seat))
		{
			return false;
		}
		SeatTransforms.Add(Seat);
	}

	for (int32 MemberOffset = 0; MemberOffset < Members.Num(); ++MemberOffset)
	{
		if (!UnitDatas.IsValidIndex(Members[MemberOffset]))
		{
			continue;
		}
		FUnitData& Member = UnitDatas[Members[MemberOffset]];
		const FTransform& Seat = SeatTransforms[MemberOffset];
		Member.QueueSlotLocation = Seat.GetLocation();
		Member.TargetLocation = Seat.GetLocation();
		Member.LaneYaw = Seat.Rotator().Yaw;
		Member.bHasQueueSlot = true;
		Member.bUsePreciseSlotPosition = true;
		Member.ArrivalRadiusUU = GetAssignedSlotArrivalRadius();
		Member.Speed = Member.CruiseSpeed;
		Member.MaxSpeed = Member.CruiseMaxSpeed;
		Member.bHasSteerTarget = false;
		Member.bFormationPathBlocked = false;
		Member.FormationClearStreak = 0;
		Member.StuckSteps = 0;
		Member.UnstuckHoldSteps = 0;
		if (PathFollowers.IsValidIndex(Members[MemberOffset]))
		{
			PathFollowers[Members[MemberOffset]].Reset();
		}
		if (AgentLaneIndices.IsValidIndex(Members[MemberOffset]))
		{
			AgentLaneIndices[Members[MemberOffset]] = INDEX_NONE;
		}
	}
	return true;
}

bool UFF_CrowdSimulationSubsystem::HandleAudienceArrival(const int32 LeaderAgentIndex)
{
	if (!PartyDecisionStates.IsValidIndex(LeaderAgentIndex)
		|| !UnitDatas.IsValidIndex(LeaderAgentIndex))
	{
		return false;
	}

	FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];
	if (State.AudiencePhase == EFF_PartyAudiencePhase::Exiting)
	{
		ReleaseAudienceParty(LeaderAgentIndex, false);
		RequestDecision(LeaderAgentIndex, EFF_DecisionReason::Arrived);
		return true;
	}
	if (State.AudiencePhase == EFF_PartyAudiencePhase::GoingToEntry)
	{
		FFFZSAudienceSnapshot Snapshot;
		FText Error;
		if (!UFFZSAudienceLibrary::GetFacilityAudienceSnapshot(
			GetWorld(), State.AudienceFacilityId, Snapshot, Error))
		{
			++AudienceSnapshotFailures;
			UE_LOG(LogTemp, Warning, TEXT("[Audience] 파티 %d 좌석 이동 취소: %s"),
				UnitDatas[LeaderAgentIndex].PartyID, *Error.ToString());
			ReleaseAudienceParty(LeaderAgentIndex, false);
			RequestDecision(LeaderAgentIndex, EFF_DecisionReason::Interrupted);
			return true;
		}

		TArray<int32> Members = GetPartyMembers(UnitDatas[LeaderAgentIndex].PartyID);
		Members.RemoveAll([this](const int32 Index)
		{
			return !UnitDatas.IsValidIndex(Index) || !UnitDatas[Index].bIsActive;
		});
		Members.Sort();
		if (!AssignAudienceSeats(LeaderAgentIndex, Snapshot, Members))
		{
			++AudienceSnapshotFailures;
			ReleaseAudienceParty(LeaderAgentIndex, false);
			RequestDecision(LeaderAgentIndex, EFF_DecisionReason::Interrupted);
			return true;
		}
		State.AudiencePhase = EFF_PartyAudiencePhase::GoingToSlot;
		return true;
	}
	if (State.AudiencePhase == EFF_PartyAudiencePhase::GoingToSlot)
	{
		// 리더만 도착하거나 막힘 콜백이 호출된 것만으로 일행 전체를 관람 중으로 바꾸지 않는다.
		const TArray<int32> Members = GetPartyMembers(UnitDatas[LeaderAgentIndex].PartyID);
		if (Members.IsEmpty()) return true;
		for (const int32 MemberIndex : Members)
		{
			const FUnitData& Member = UnitDatas[MemberIndex];
			if (!Member.bHasQueueSlot || !Member.bUsePreciseSlotPosition
				|| FVector::DistSquared2D(Member.Position, Member.QueueSlotLocation)
					> FMath::Square(GetAssignedSlotArrivalRadius()))
			{
				return true;
			}
		}
		State.AudiencePhase = bMainProgramShowStarted
			? EFF_PartyAudiencePhase::Spectating
			: EFF_PartyAudiencePhase::Waiting;
		UnitDatas[LeaderAgentIndex].Speed = 0.f;
		return true;
	}
	if (State.AudiencePhase == EFF_PartyAudiencePhase::Waiting
		|| State.AudiencePhase == EFF_PartyAudiencePhase::Spectating)
	{
		UnitDatas[LeaderAgentIndex].Speed = 0.f;
		return true;
	}

	if (State.AudiencePhase != EFF_PartyAudiencePhase::None)
	{
		return true;
	}

	FGuid FacilityId;
	if (!FFF_ZoneSystemBridge::TryParseDestinationId(State.CurrentDestinationId, FacilityId))
	{
		return false;
	}
	FFFZSAudienceSnapshot Snapshot;
	FText Error;
	if (!UFFZSAudienceLibrary::GetFacilityAudienceSnapshot(GetWorld(), FacilityId, Snapshot, Error))
	{
		return false;
	}
	if (BeginAudienceVisit(LeaderAgentIndex, FacilityId, Snapshot))
	{
		return true;
	}

	// Audience 시설은 만석이어도 12명 FIFO Queue로 폴백하지 않는다.
	RequestDecision(LeaderAgentIndex, EFF_DecisionReason::JoinRejected);
	return true;
}

void UFF_CrowdSimulationSubsystem::ReleaseAudienceParty(
	const int32 LeaderAgentIndex, const bool bMoveToExit)
{
	if (!PartyDecisionStates.IsValidIndex(LeaderAgentIndex)
		|| !UnitDatas.IsValidIndex(LeaderAgentIndex))
	{
		return;
	}
	FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];
	if (State.AudiencePhase == EFF_PartyAudiencePhase::None)
	{
		return;
	}

	if (TSet<int32>* Occupied = OccupiedAudienceSlots.Find(State.AudienceFacilityId))
	{
		for (int32 Offset = 0; Offset < State.AudienceSlotCount; ++Offset)
		{
			Occupied->Remove(State.AudienceFirstSlotIndex + Offset);
		}
		if (Occupied->IsEmpty())
		{
			OccupiedAudienceSlots.Remove(State.AudienceFacilityId);
		}
	}

	const FVector ExitLocation = State.AudienceExitLocation;
	for (const int32 MemberIndex : GetPartyMembers(UnitDatas[LeaderAgentIndex].PartyID))
	{
		if (UnitDatas.IsValidIndex(MemberIndex))
		{
			UnitDatas[MemberIndex].bHasQueueSlot = false;
			UnitDatas[MemberIndex].bUsePreciseSlotPosition = false;
			UnitDatas[MemberIndex].QueueSlotLocation = FVector::ZeroVector;
			UnitDatas[MemberIndex].Speed = UnitDatas[MemberIndex].CruiseSpeed;
			UnitDatas[MemberIndex].MaxSpeed = UnitDatas[MemberIndex].CruiseMaxSpeed;
			UnitDatas[MemberIndex].bHasSteerTarget = false;
			if (PathFollowers.IsValidIndex(MemberIndex)) PathFollowers[MemberIndex].Reset();
		}
	}

	State.AudienceFacilityId.Invalidate();
	State.AudienceFirstSlotIndex = INDEX_NONE;
	State.AudienceSlotCount = 0;
	State.AudienceEntryLocation = FVector::ZeroVector;
	State.AudienceExitLocation = FVector::ZeroVector;
	State.AudienceArrivalTolerance = 0.f;
	if (bMoveToExit)
	{
		State.AudiencePhase = EFF_PartyAudiencePhase::Exiting;
		UnitDatas[LeaderAgentIndex].TargetLocation = ExitLocation;
	}
	else
	{
		State.AudiencePhase = EFF_PartyAudiencePhase::None;
		State.CurrentDestinationId = NAME_None;
		State.CurrentNeedRelief = FFF_NeedReliefFlags();
	}
}

void UFF_CrowdSimulationSubsystem::ReleaseAllAudienceParties(const bool bMoveToExit)
{
	for (int32 LeaderIndex = 0; LeaderIndex < PartyDecisionStates.Num(); ++LeaderIndex)
	{
		if (UnitDatas.IsValidIndex(LeaderIndex)
			&& UnitDatas[LeaderIndex].bIsActive
			&& UnitDatas[LeaderIndex].LeaderAgentIndex == LeaderIndex
			&& IsAudienceLocked(PartyDecisionStates[LeaderIndex]))
		{
			ReleaseAudienceParty(LeaderIndex, bMoveToExit);
		}
	}
	if (!bMoveToExit)
	{
		OccupiedAudienceSlots.Reset();
		AudiencePartiesExcludedThisShow.Reset();
	}
}

bool UFF_CrowdSimulationSubsystem::FindMainStageAudience(
	FGuid& OutFacilityId, FFFZSAudienceSnapshot& OutSnapshot) const
{
	OutFacilityId.Invalidate();
	OutSnapshot = FFFZSAudienceSnapshot();
	const UWorld* World = GetWorld();
	const UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!Zones)
	{
		return false;
	}
	const FFFZSReadModel ReadModel = Zones->BuildReadModel();
	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		if (Facility.State != EFFZSFacilityState::Active || Facility.bNeedsRelocation
			|| !FFF_ZoneSystemBridge::IsMainStageFacility(
				Facility.Category, Facility.FacilityDefinitionId))
		{
			continue;
		}
		FText Error;
		if (UFFZSAudienceLibrary::GetFacilityAudienceSnapshot(
			World, Facility.FacilityInstanceId, OutSnapshot, Error))
		{
			OutFacilityId = Facility.FacilityInstanceId;
			return true;
		}
		UE_LOG(LogTemp, Warning, TEXT("[Audience] 메인 무대 관람 데이터 사용 불가: %s"), *Error.ToString());
	}
	return false;
}

int32 UFF_CrowdSimulationSubsystem::GatherPartiesToMainStageAudience()
{
	const UWorld* World = GetWorld();
	const UFF_FestivalStageSubsystem* Stage = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	if (!Stage || !Stage->IsMainProgramQueueBreakActive())
	{
		return 0;
	}
	if (UFF_ZoneSystemQueueBridgeSubsystem* Bridge = World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>())
	{
		Bridge->SynchronizeMainProgramQueueBreak();
	}

	++AudienceGatherAttempts;
	FGuid FacilityId;
	FFFZSAudienceSnapshot Snapshot;
	if (!FindMainStageAudience(FacilityId, Snapshot))
	{
		++AudienceSnapshotFailures;
		if (!bWarnedMissingAudienceThisShow)
		{
			bWarnedMissingAudienceThisShow = true;
			UE_LOG(LogTemp, Warning,
				TEXT("[Audience] 메인 공연 강제 집결을 건너뛴다 — 관람 공간이 켜진 활성 SunsetMainStage가 없다. ")
				TEXT("무대가 설치됐는지, PDA 의 Audience.bEnabled 가 켜져 있는지 확인할 것"));
		}
		return 0;
	}

	TArray<int32> Leaders;
	for (int32 LeaderIndex = 0; LeaderIndex < UnitDatas.Num(); ++LeaderIndex)
	{
		if (!PartyDecisionStates.IsValidIndex(LeaderIndex)) continue;
		const FUnitData& Unit = UnitDatas[LeaderIndex];
		const FPartyDecisionState& State = PartyDecisionStates[LeaderIndex];
		if (Unit.bIsActive && Unit.LeaderAgentIndex == LeaderIndex && !State.bIsLeaving
			&& State.ProgramPhase == EFF_PartyProgramPhase::None
			&& State.AudiencePhase == EFF_PartyAudiencePhase::None
			&& !AudiencePartiesExcludedThisShow.Contains(Unit.PartyID))
		{
			Leaders.Add(LeaderIndex);
		}
	}
	Leaders.Sort([this, &Snapshot](const int32 A, const int32 B)
	{
		const double DistA = FVector::DistSquared2D(UnitDatas[A].Position, Snapshot.EntryWorldCm);
		const double DistB = FVector::DistSquared2D(UnitDatas[B].Position, Snapshot.EntryWorldCm);
		return DistA == DistB ? UnitDatas[A].PartyID < UnitDatas[B].PartyID : DistA < DistB;
	});

	// 자기 이동시간만큼만 일찍 떠난다. 전원을 같은 시각에 끌어내면 하루 대부분이
	// "무대로 걸어가기"가 되어 부스가 비고, 반대로 다 같이 늦게 떠나면 먼 손님이 못 온다.
	float RemainingSeconds = TNumericLimits<float>::Max();
	if (const UFF_FestivalStageSubsystem* GatherStage = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr)
	{
		RemainingSeconds = MainProgramStartLiveSeconds - GatherStage->ElapsedLiveSeconds;
	}

	int32 GatheredParties = 0;
	int32 GatheredMembers = 0;
	int32 NotDueYet = 0;
	for (const int32 LeaderIndex : Leaders)
	{
		const float SpeedCmPerSecond = FMath::Max(UnitDatas[LeaderIndex].CruiseSpeed, 1.f);
		const float TravelSeconds = static_cast<float>(FVector::Dist2D(
			UnitDatas[LeaderIndex].Position, Snapshot.EntryWorldCm)) / SpeedCmPerSecond;
		const float DueSeconds = TravelSeconds * FMath::Max(AudienceTravelSlack, 1.f)
			+ FMath::Max(AudienceGatherSafetySeconds, 0.f);
		if (RemainingSeconds > DueSeconds)
		{
			++NotDueYet;
			continue;
		}
		if (BeginAudienceVisit(LeaderIndex, FacilityId, Snapshot))
		{
			++GatheredParties;
			GatheredMembers += FMath::Max(GetPartyMembers(UnitDatas[LeaderIndex].PartyID).Num(), 1);
		}
	}
	AudienceGatheredParties += GatheredParties;
	AudienceGatheredMembers += GatheredMembers;
	if (GatheredParties > 0 || NotDueYet > 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Audience] 집결 판정: 출발 %d파티 %d명 / 아직 여유 %d파티, 정원 %d, 점유 %d"),
			GatheredParties, GatheredMembers, NotDueYet, Snapshot.Capacity,
			OccupiedAudienceSlots.FindRef(FacilityId).Num());
	}
	return GatheredParties;
}

int32 UFF_CrowdSimulationSubsystem::GetOccupiedAudienceSlotCount(const FGuid FacilityInstanceId) const
{
	const TSet<int32>* Occupied = OccupiedAudienceSlots.Find(FacilityInstanceId);
	return Occupied ? Occupied->Num() : 0;
}

int32 UFF_CrowdSimulationSubsystem::GetAudiencePartyCount() const
{
	int32 Count = 0;
	for (int32 LeaderIndex = 0; LeaderIndex < PartyDecisionStates.Num(); ++LeaderIndex)
	{
		if (UnitDatas.IsValidIndex(LeaderIndex)
			&& UnitDatas[LeaderIndex].bIsActive
			&& UnitDatas[LeaderIndex].LeaderAgentIndex == LeaderIndex
			&& IsAudienceLocked(PartyDecisionStates[LeaderIndex]))
		{
			++Count;
		}
	}
	return Count;
}

void UFF_CrowdSimulationSubsystem::GetScheduledPhaseCounts(
	int32& OutAudienceSeated, int32& OutAudienceWalking,
	int32& OutProgramAtVenue, int32& OutProgramWalking) const
{
	OutAudienceSeated = 0;
	OutAudienceWalking = 0;
	OutProgramAtVenue = 0;
	OutProgramWalking = 0;
	for (int32 LeaderIndex = 0; LeaderIndex < PartyDecisionStates.Num(); ++LeaderIndex)
	{
		if (!UnitDatas.IsValidIndex(LeaderIndex) || !UnitDatas[LeaderIndex].bIsActive
			|| UnitDatas[LeaderIndex].LeaderAgentIndex != LeaderIndex)
		{
			continue;
		}
		const FPartyDecisionState& State = PartyDecisionStates[LeaderIndex];
		switch (State.AudiencePhase)
		{
		case EFF_PartyAudiencePhase::Waiting:
		case EFF_PartyAudiencePhase::Spectating:
			++OutAudienceSeated;
			break;
		case EFF_PartyAudiencePhase::GoingToEntry:
		case EFF_PartyAudiencePhase::GoingToSlot:
			++OutAudienceWalking;
			break;
		default:
			break;
		}
		switch (State.ProgramPhase)
		{
		case EFF_PartyProgramPhase::Waiting:
		case EFF_PartyProgramPhase::Participating:
			++OutProgramAtVenue;
			break;
		case EFF_PartyProgramPhase::Returning:
			++OutProgramWalking;
			break;
		default:
			break;
		}
	}
}

void UFF_CrowdSimulationSubsystem::GetAudienceStats(
	int32& OutAttempts, int32& OutGatheredParties, int32& OutGatheredMembers,
	int32& OutCapacityRejects, int32& OutSnapshotFailures, int32& OutProgramConflicts) const
{
	OutAttempts = AudienceGatherAttempts;
	OutGatheredParties = AudienceGatheredParties;
	OutGatheredMembers = AudienceGatheredMembers;
	OutCapacityRejects = AudienceCapacityRejects;
	OutSnapshotFailures = AudienceSnapshotFailures;
	OutProgramConflicts = AudienceProgramConflictSkips;
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
		ClearPartyQueueColumn(GetAgentPartyID(LeaderAgentIndex));
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

	// 멤버 슬롯도 같이 푼다. 안 풀면 UpdatePartyMemberTarget 이 계속 옛 슬롯 좌표로 보내서
	// 리더만 떠나고 일행은 그 부스 앞에 남는다(QueuePhase 가 None 인 분기는 이미 풀고 있었다).
	ClearPartyQueueColumn(GetAgentPartyID(LeaderAgentIndex));
}

bool UFF_CrowdSimulationSubsystem::BeginProgramReturn(
	const int32 LeaderAgentIndex,
	const FFF_ProgramReservation& Reservation,
	const FVector& FallbackLocation)
{
	if (!Reservation.IsValid()
		|| !PartyDecisionStates.IsValidIndex(LeaderAgentIndex)
		|| !UnitDatas.IsValidIndex(LeaderAgentIndex))
	{
		return false;
	}

	FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];
	FUnitData& Leader = UnitDatas[LeaderAgentIndex];
	ClearPartyQueueState(LeaderAgentIndex, true);

	int32 WaitingIndex = 0;
	for (const int32 AssignedPartyId : Reservation.AssignedGroupIds)
	{
		if (AssignedPartyId == Leader.PartyID) break;
		WaitingIndex += FMath::Max(GetPartyMembers(AssignedPartyId).Num(), 1);
	}
	FTransform WaitingTransform;
	const UWorld* World = GetWorld();
	const UFF_ZoneSystemQueueBridgeSubsystem* Bridge =
		World ? World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>() : nullptr;
	const bool bHasWaitingSlot = Bridge && Bridge->TryGetProgramWaitingTransform(
		Reservation.FacilityInstanceId, WaitingIndex, WaitingTransform);

	State.ProgramPhase = EFF_PartyProgramPhase::Returning;
	State.ProgramFacilityId = Reservation.FacilityInstanceId;
	State.ProgramStartMinutes = Reservation.StartMinutes;
	State.ProgramEndMinutes = Reservation.EndMinutes;
	State.ProgramWaitingLocation = bHasWaitingSlot
		? WaitingTransform.GetLocation()
		: FallbackLocation;
	State.CurrentDestinationId = FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(
		Reservation.FacilityInstanceId);
	State.CurrentNeedRelief = FFF_NeedReliefFlags();
	State.ConsecutiveNoChoiceCount = 0;
	State.bNeedsDecision = false;
	Leader.TargetLocation = State.ProgramWaitingLocation;
	TArray<int32> Members = GetPartyMembers(Leader.PartyID);
	Members.Sort();
	for (int32 MemberOffset = 0; MemberOffset < Members.Num(); ++MemberOffset)
	{
		if (!UnitDatas.IsValidIndex(Members[MemberOffset])) continue;
		FTransform MemberWaitingTransform;
		const bool bHasMemberSlot = Bridge && Bridge->TryGetProgramWaitingTransform(
			Reservation.FacilityInstanceId, WaitingIndex + MemberOffset, MemberWaitingTransform);
		FUnitData& Member = UnitDatas[Members[MemberOffset]];
		const FVector MemberTarget = bHasMemberSlot
			? MemberWaitingTransform.GetLocation()
			: State.ProgramWaitingLocation + GetPartyFormationOffset(MemberOffset);
		Member.bUsePreciseSlotPosition = false; // 프로그램 폴백 접근점은 개별 저작 좌석이 아니다.
		if (Members[MemberOffset] == LeaderAgentIndex)
		{
			Member.TargetLocation = MemberTarget;
			State.ProgramWaitingLocation = MemberTarget;
		}
		else
		{
			Member.bHasQueueSlot = true;
			Member.QueueSlotLocation = MemberTarget;
		}
	}
	UE_LOG(LogTemp, Log,
		TEXT("[예약] 파티 %d(%d명) 프로그램 시설로 출발 — 시작 %.1f분, 첫 대기 슬롯 %d"),
		Leader.PartyID, Members.Num(), Reservation.StartMinutes, WaitingIndex);
	return true;
}

bool UFF_CrowdSimulationSubsystem::HandleProgramArrival(const int32 LeaderAgentIndex)
{
	if (!PartyDecisionStates.IsValidIndex(LeaderAgentIndex)
		|| !UnitDatas.IsValidIndex(LeaderAgentIndex))
	{
		return false;
	}

	FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];
	if (State.ProgramPhase == EFF_PartyProgramPhase::None)
	{
		return false;
	}
	if (State.ProgramPhase == EFF_PartyProgramPhase::Returning)
	{
		float NowMinutes = 0.f;
		if (!TryGetProgramLiveMinutes(NowMinutes))
		{
			return true;
		}
		State.ProgramPhase = NowMinutes >= State.ProgramStartMinutes
			? EFF_PartyProgramPhase::Participating
			: EFF_PartyProgramPhase::Waiting;
		UE_LOG(LogTemp, Log, TEXT("[예약] 파티 %d 프로그램 시설 도착 (%.1f분, %s)"),
			UnitDatas[LeaderAgentIndex].PartyID, NowMinutes,
			State.ProgramPhase == EFF_PartyProgramPhase::Participating
				? TEXT("바로 참여") : TEXT("시작까지 대기"));
		State.CurrentDestinationId = NAME_None;
		State.CurrentNeedRelief = FFF_NeedReliefFlags();
		UnitDatas[LeaderAgentIndex].TargetLocation = State.ProgramWaitingLocation;
	}
	return true;
}

void UFF_CrowdSimulationSubsystem::ReleaseProgramParty(const int32 LeaderAgentIndex)
{
	if (!PartyDecisionStates.IsValidIndex(LeaderAgentIndex)
		|| !UnitDatas.IsValidIndex(LeaderAgentIndex))
	{
		return;
	}

	FPartyDecisionState& State = PartyDecisionStates[LeaderAgentIndex];
	FUnitData& Leader = UnitDatas[LeaderAgentIndex];
	if (State.ProgramPhase == EFF_PartyProgramPhase::None)
	{
		return;
	}

	// 어느 단계에서 풀렸는지가 중요하다. Returning/Waiting 으로 끝났으면 그 파티는
	// 프로그램을 못 본 것이고, 체험비는 이미 정원 기준으로 걷혔다.
	{
		const TCHAR* PhaseName =
			State.ProgramPhase == EFF_PartyProgramPhase::Participating ? TEXT("참여 완료")
			: State.ProgramPhase == EFF_PartyProgramPhase::Waiting ? TEXT("대기 중 종료(미참여)")
			: TEXT("이동 중 종료(미도착)");
		const float RemainingM = 0.01f * static_cast<float>(
			FVector::Dist2D(Leader.Position, State.ProgramWaitingLocation));
		UE_LOG(LogTemp, Log, TEXT("[예약] 파티 %d 프로그램 종료 — %s (시설까지 %.0fm)"),
			Leader.PartyID, PhaseName, RemainingM);
	}
	if (PartyRemainingStayMinutes.IsValidIndex(LeaderAgentIndex))
	{
		PartyRemainingStayMinutes[LeaderAgentIndex] = FFF_ProgramReservationRules::ApplyExitGrace(
			PartyRemainingStayMinutes[LeaderAgentIndex], ProgramExitGraceMinutes);
	}
	for (const int32 MemberIndex : GetPartyMembers(Leader.PartyID))
	{
		if (UnitDatas.IsValidIndex(MemberIndex))
		{
			UnitDatas[MemberIndex].bHasQueueSlot = false;
			UnitDatas[MemberIndex].QueueSlotLocation = FVector::ZeroVector;
			UnitDatas[MemberIndex].Speed = UnitDatas[MemberIndex].CruiseSpeed;
			UnitDatas[MemberIndex].bUsePreciseSlotPosition = false;
		}
	}

	FVector ExitLocation = State.ProgramWaitingLocation;
	if (UWorld* World = GetWorld())
	{
		if (const UFF_ZoneSystemQueueBridgeSubsystem* Bridge =
			World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>())
		{
			if (const UFF_QueueSubsystem* Queue = World->GetSubsystem<UFF_QueueSubsystem>())
			{
				const int32 QueueFacilityId = Bridge->FindQueueFacilityId(State.ProgramFacilityId);
				const FVector QueueExit = Queue->GetFacilityExitLocation(QueueFacilityId);
				if (!QueueExit.IsNearlyZero())
				{
					ExitLocation = QueueExit;
				}
			}
		}
	}

	State.ProgramPhase = EFF_PartyProgramPhase::None;
	State.ProgramFacilityId.Invalidate();
	State.ProgramStartMinutes = 0.f;
	State.ProgramEndMinutes = 0.f;
	State.ProgramWaitingLocation = FVector::ZeroVector;
	State.CurrentDestinationId = NAME_None;
	State.CurrentNeedRelief = FFF_NeedReliefFlags();
	State.ConsecutiveNoChoiceCount = 0;
	Leader.TargetLocation = ExitLocation;
}

bool UFF_CrowdSimulationSubsystem::TryGetProgramLiveMinutes(float& OutMinutes) const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	return Stage && Stage->TryGetLiveElapsedMinutes(OutMinutes);
}

bool UFF_CrowdSimulationSubsystem::TryBeginProgramReturnIfDue(
	int32 LeaderIndex, const FFF_ProgramReservation& Reservation, const FVector& Approach, float NowMinutes)
{
	const FUnitData& Leader = UnitDatas[LeaderIndex];
	const float TravelMinutes = Leader.Speed > KINDA_SMALL_NUMBER
		? UFF_FestivalStageSubsystem::StageSecondsToFestivalMinutes(
			FVector::Dist2D(Leader.Position, Approach) / Leader.Speed)
		: 0.f;
	return NowMinutes + TravelMinutes >= Reservation.StartMinutes - FMath::Max(ProgramReturnBufferMinutes, 0.f)
		&& BeginProgramReturn(LeaderIndex, Reservation, Approach);
}

bool UFF_CrowdSimulationSubsystem::TryGetProgramApproachLocation(
	const FGuid& FacilityInstanceId, FVector& OutApproach)
{
	// BuildReadModel 은 전 시설을 정렬해 값 복사하는 함수다. 고정 스텝마다 부르면 미복귀
	// 예약자가 한 명만 남아도 그 비용이 계속 나간다. 배치가 바뀔 때만 다시 짓는다.
	const UWorld* World = GetWorld();
	const UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!ZoneSystem)
	{
		return false;
	}

	const int64 Revision = ZoneSystem->GetWorldRevision();
	if (Revision != ProgramApproachCacheRevision)
	{
		ProgramApproachCacheRevision = Revision;
		CachedProgramApproachByFacility.Reset();
		const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
		for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
		{
			if (Facility.bHasApproachWorldTransform)
			{
				CachedProgramApproachByFacility.Add(
					Facility.FacilityInstanceId, Facility.ApproachWorldTransform.GetLocation());
			}
		}
	}

	if (const FVector* Found = CachedProgramApproachByFacility.Find(FacilityInstanceId))
	{
		OutApproach = *Found;
		return true;
	}
	return false;
}

void UFF_CrowdSimulationSubsystem::UpdateProgramReservationStates()
{
	float NowMinutes = 0.f;
	if (!TryGetProgramLiveMinutes(NowMinutes))
	{
		return; // 행사 종료의 전역 퇴장 경로는 예약이 가로막지 않는다.
	}
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFF_ProgramReservationSubsystem* Reservations = GameInstance
		? GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>() : nullptr;
	if (!Reservations)
	{
		return;
	}

	for (int32 LeaderIndex = 0; LeaderIndex < PartyDecisionStates.Num(); ++LeaderIndex)
	{
		if (!UnitDatas.IsValidIndex(LeaderIndex)
			|| !UnitDatas[LeaderIndex].bIsActive
			|| UnitDatas[LeaderIndex].LeaderAgentIndex != LeaderIndex)
		{
			continue;
		}

		FPartyDecisionState& State = PartyDecisionStates[LeaderIndex];
		if (State.bIsLeaving)
		{
			continue;
		}
		if (State.ProgramPhase != EFF_PartyProgramPhase::None
			&& NowMinutes >= State.ProgramEndMinutes)
		{
			ReleaseProgramParty(LeaderIndex);
			continue;
		}
		if (State.ProgramPhase == EFF_PartyProgramPhase::Waiting
			&& NowMinutes >= State.ProgramStartMinutes)
		{
			State.ProgramPhase = EFF_PartyProgramPhase::Participating;
			UE_LOG(LogTemp, Log, TEXT("[예약] 파티 %d 프로그램 참여 시작 (%.1f분)"),
				UnitDatas[LeaderIndex].PartyID, NowMinutes);
			continue;
		}
		if (State.ProgramPhase != EFF_PartyProgramPhase::None)
		{
			continue;
		}

		FFF_ProgramReservation Reservation;
		if (!Reservations->FindActiveReservationForParty(
			UnitDatas[LeaderIndex].PartyID, NowMinutes, Reservation))
		{
			continue;
		}

		// 예약이 관람보다 우선한다. 잠금이 걸려 있으면 자리를 반납하고 복귀 판정으로 넘긴다 —
		// 두 상태를 동시에 들고 있으면 목적지를 세우는 주체가 둘이 된다.
		if (IsAudienceLocked(State))
		{
			ReleaseAudienceParty(LeaderIndex, false);
		}

		FVector Approach = FVector::ZeroVector;
		const bool bHasApproach =
			TryGetProgramApproachLocation(Reservation.FacilityInstanceId, Approach);
		if (bHasApproach)
		{
			TryBeginProgramReturnIfDue(LeaderIndex, Reservation, Approach, NowMinutes);
		}
		else if (!MissingProgramApproachWarnings.Contains(Reservation.FacilityInstanceId))
		{
			MissingProgramApproachWarnings.Add(Reservation.FacilityInstanceId);
			UE_LOG(LogTemp, Error,
				TEXT("[예약 복귀] 시설 %s에 유효한 접근점이 없어 예약 파티가 복귀할 수 없다"),
				*Reservation.FacilityInstanceId.ToString());
		}
	}
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

	// 인내심 한계 이탈(reneging). patience_multiplier 의 결선 지점이다.
	//
	// ★ 판정은 AtSlot 에서만 한다. Joined(배정 슬롯으로 걸어가는 중)에 이탈시키면 걸어가다
	//   포기하는 것이 되고, 그 구간에서 진척이 없는 경우는 MoveFailed 가 이미 맡고 있다.
	//
	// ★ 기준점은 QueueEnteredSimTime = **줄에 가입한 시각**이라, 슬롯까지 걸어간 시간이
	//   대기 시간에 포함된다. 슬롯 도착 시각을 따로 들지 않은 것은 의도다 — 손님 입장에서
	//   "이 시설에 매인 시간"은 가입 시점부터이고, 필드를 하나 더 늘릴 값어치가 없다.
	//   순수 슬롯 대기만 재야 할 이유가 생기면 그때 타임스탬프를 추가할 것.
	//
	// ★ ClearPartyQueueState 를 bLeaveQueue=true 로 먼저 부른다. 이 순서가 중요하다 —
	//   RequestDecision 이 내부에서 부르는 것은 bLeaveQueue=false 라 큐에 통지하지 않고,
	//   그러면 Queue 는 이 파티를 계속 들고 있어 자리가 영영 안 비운다(헤더 주석의 그 문제).
	//   먼저 지우고 나면 QueuePhase 가 None 이라 RequestDecision 쪽 호출은 무해해진다.
	//
	// ★ 재결정을 여기서 직접 걸고 true 를 돌려준다. false 를 돌려주면 호출부가 Arrived 로
	//   요청하는데, 그 사유는 조정자가 직전 목적지를 감점하지 않아 같은 줄에 다시 선다.
	//   Arrived 는 쿨다운도 타므로 이탈 직후 요청이 조용히 버려질 수 있다.
	const UFF_QueueSubsystem* QueueSystem = World->GetSubsystem<UFF_QueueSubsystem>();
	// AtSlot includes both waiting and using teams. The queue core owns the actual
	// start state; waiting impatience must not cancel a use that is already in progress.
	const bool bHasStartedService = QueueSystem && QueueSystem->HasPartyStartedService(
		Bridge->FindQueueFacilityId(State.QueuedFacilityId), Leader.PartyID);
	if (bEnableQueuePatienceAbandon
		&& State.QueuePhase == EFF_PartyQueuePhase::AtSlot
		&& !bHasStartedService
		&& AgentPersonalThresholds.IsValidIndex(LeaderAgentIndex))
	{
		const float WaitedMinutes = FMath::Max(SimTimeSeconds - State.QueueEnteredSimTime, 0.f)
			* Temp_FestivalMinutesPerRealSecond;

		// 대기 항만 켜서 부른다. 밀집은 조정자의 Relocate 경로가 이미 판정하고 있고,
		// 개장·라이브 변화는 DestinationOpened 가 밖에서 밀어 넣는다 — 여기서 또 보면
		// 같은 사건에 두 경로가 반응한다.
		//
		// ★ 끄는 항은 조건이 **성립할 수 없는 값**을 넣는다. 부등호 방향을 확인할 것 —
		//   밀집은 `>` 라 (0, 1) 로 꺼지지만, 주기는 `>=` 라 (0, 0) 을 넣으면 0 >= 0 이
		//   참이 되어 매번 발화한다. 2026-09-08 실측에서 실제로 그 버그가 났고, 인내심과
		//   무관하게 슬롯 도착 즉시 전원이 이탈했다(`0.1분 기다리다 떠난다`).
		//   주기를 끌 때는 Max 를 넣어야 한다.
		const bool bAbandon = FFF_UtilityDestinationSelector::ShouldReevaluate(
			WaitedMinutes, AgentPersonalThresholds[LeaderAgentIndex].WaitThresholdMinutes,
			/*CurrentDensityRatio=*/0.f, /*PersonalDensityThresholdRatio=*/1.f,
			/*SecondsSinceLastEvaluation=*/0.f,
			/*PersonalReevaluateIntervalSeconds=*/TNumericLimits<float>::Max(),
			/*bDestinationOpenStateChanged=*/false, /*bLiveEventStateChanged=*/false);

		if (bAbandon)
		{
			// LogFestaFiestaNpc 에 붙인다 — 개체 변주 진단은 `Log LogFestaFiestaNpc Verbose` 하나로
			// 다 켜져야 한다. LogTemp 에 두면 [개체]·항별 덤프를 켜도 이 줄만 빠져서,
			// 계수기는 31회인데 로그는 0줄인 상태가 된다(2026-09-08 실측에서 실제로 겪었다).
			UE_LOG(LogFestaFiestaNpc, Verbose,
				TEXT("[줄 이탈] 파티 %d가 %.1f분 기다리다 떠난다 (개인 한계 %.1f분)"),
				Leader.PartyID, WaitedMinutes,
				AgentPersonalThresholds[LeaderAgentIndex].WaitThresholdMinutes);

			++QueuePatienceAbandons;

			// 큐에 먼저 통지하고(위 주석) 그 다음 재결정을 건다. 이 순서를 바꾸면 안 된다.
			ClearPartyQueueState(LeaderAgentIndex, true);
			RequestDecision(LeaderAgentIndex, EFF_DecisionReason::QueuePatienceExpired);
			return true;
		}
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
		if (Members.IsEmpty())
		{
			return false;
		}

		FFF_JoinRequest Request;
		Request.PartyId = Leader.PartyID;
		Request.MemberCount = Members.Num();
		Request.MemberAgentIds = Members;
		Request.CauseId = State.CurrentDestinationId;

		++QueueJoinAttempts;

		const FFF_QueueJoinResponse Response = Bridge->TryJoinZoneFacility(ZoneFacilityId, Request);
		if (!Response.bAccepted)
		{
			++QueueJoinRejected;
			RequestDecision(LeaderAgentIndex, EFF_DecisionReason::JoinRejected);
			return true;
		}

		++QueueJoinAccepted;
		State.QueuePhase = EFF_PartyQueuePhase::Joined;
		State.QueuedFacilityId = ZoneFacilityId;
		State.QueueEnteredSimTime = SimTimeSeconds;
		return true;
	}

	case EFF_PartyQueuePhase::Joined:
	{
		// 가입 이벤트와 자리 배정은 지연 전달된다. 아직 접근점을 목표로 삼거나
		// 일행이 따라오는 동안에는 도착 확인을 보내지 않는다.
		for (const int32 MemberIndex : GetPartyMembers(Leader.PartyID))
		{
			const FUnitData& Member = UnitDatas[MemberIndex];
			if (!Member.bHasQueueSlot
				|| FVector::DistSquared2D(Member.Position, Member.QueueSlotLocation)
					> FMath::Square(GetAssignedSlotArrivalRadius()))
			{
				return true;
			}
		}
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
	QueuePatienceAbandons = 0;
}

// 점이 축제장 안인지 본다. Zone 이 하나도 없으면 통과다 — 막으면 Zone 없는 맵에서 배회가 전멸한다
bool UFF_CrowdSimulationSubsystem::IsInsideFestivalBounds(const FVector& WorldPointCm) const
{
	if (!bConfineWanderToZones)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!ZoneSystem)
	{
		return true;
	}

	const int64 Revision = ZoneSystem->GetWorldRevision();
	if (WanderBoundsSystem.Get() != ZoneSystem || WanderBoundsRevision != Revision)
	{
		WanderBoundsSystem = ZoneSystem;
		WanderBoundsRevision = Revision;
		bWanderBoundsActive = !ZoneSystem->GetRegisteredZoneIds().IsEmpty();
	}

	if (!bWanderBoundsActive)
	{
		return true;
	}

	return !ZoneSystem->ResolveZoneAtWorldLocation(WorldPointCm).IsNone();
}

// 밖에 선 파티를 SimOrigin 쪽으로 한 걸음 당긴다. 끝까지 보내면 전원이 원점 한 점으로 수렴한다
FVector UFF_CrowdSimulationSubsystem::MakeContainmentDestination(const FVector& FromPositionCm) const
{
	const FVector ToOrigin(SimOrigin.X - FromPositionCm.X, SimOrigin.Y - FromPositionCm.Y, 0.f);
	const double DistanceCm = ToOrigin.Size();

	if (DistanceCm <= static_cast<double>(WanderRadiusUU))
	{
		return FVector(SimOrigin.X, SimOrigin.Y, FromPositionCm.Z);
	}

	return FromPositionCm + ToOrigin / DistanceCm * WanderRadiusUU;
}

// 조정자가 목적지를 못 정했을 때의 국소 배회. **살아 있는 유일한 랜덤 목적지 생성기다.**
//
// ★ Rng(FRandomStream)를 쓰지 않는 것이 핵심이다. 시드를 (전역시드, StableAgentID, 라운드)에서
//   뽑으므로 같은 입력이면 항상 같은 좌표가 나오고, 호출 순서와 프레임률이 결과에 새어들지 않는다.
//   공유 스트림에서 뽑으면 뽑는 순서가 곧 결과가 되고, 위치가 갈리면 다음 재평가의 거리가 갈려
//   결정 자체가 갈린다(FF_WalkDistanceMatrix.h가 경고하는 그 문제다).
//
// ★ 예전에 PickRandomDestination()이 따로 있었다. 공유 Rng에서 뽑고 범위가 시뮬 반경 전체라
//   개체를 축제장 밖으로 보내면서 결정론까지 깨는 함수였고, 호출부가 없어 삭제했다.
//   배회 범위를 넓히고 싶으면 그것을 되살리지 말고 WanderRadiusUU를 올릴 것 —
//   그래야 결정론이 유지된다.
//
// ★ 유일한 수평 울타리는 IsInsideFestivalBounds(Zone)다. IsWalkable은 통행 상한 Z만 보며 격자 밖은
//   통과시키므로 울타리가 아니다. Zone이 하나도 없는 맵에는 수평 봉쇄가 없다.
//
// ★ 통행 상한에 걸리는 좌표는 여기서 걸러 낸다. 안 걸러도 이동 쪽이 막아 주지만, 갈 수 없는
//   곳을 목표로 잡은 파티는 경계에 붙어 서서 StuckSteps를 채우고 MoveFailed로 다시 여기
//   들어온다. 그 왕복이 결정 큐를 잡아먹으므로 뽑는 자리에서 끊는 것이 맞다. Zone 기각도
//   같은 이유로 뽑는 자리에 둔다.
//
// ★ 재추첨 시드는 MakeDecisionSeed의 salt를 더 쓰지 않고 기본 시드에서 갈라낸다. salt 공간이
//   라운드당 두 칸(ChoiceRoundIndex * 2, * 2 + 1)으로 이미 꽉 차 있어, 더 쓰면 다음 라운드의
//   시드와 겹쳐 배회 좌표가 라운드 간에 상관된다.
//
// ★ 전부 막히면 제자리를 돌려준다. 다음 스텝에 도착 판정이 서고 재결정이 걸리지만 쿨다운을
//   타므로 폭주하지 않으며, 그때는 ChoiceRoundIndex가 올라 새 시드로 다시 뽑는다. 단 이미
//   Zone 밖에 선 개체는 예외다 — 제자리를 주면 영영 못 돌아오므로 안쪽으로 되돌린다.
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

	// 도로 위(또는 도로 곁)의 개체는 원판 대신 레인을 따라 멀리 보낸다. 실패하면 아래 원판으로
	// 떨어지므로 도로를 한 칸도 안 깐 맵의 동작은 그대로다.
	if (bEnableRoadPatrolWander)
	{
		FVector PatrolDestination = FVector::ZeroVector;
		if (TryPickPatrolDestination(LeaderAgentIndex, SeedAngle, PatrolDestination))
		{
			++PatrolWanderCount;
			return PatrolDestination;
		}
	}

	// 원판을 끄면 레인이 유일한 배회 수단이 된다. 레인에서 PatrolLaneSnapRadiusCm 밖에 선
	// 개체는 목적지를 못 받으므로, 끄기 전에 WanderStalls가 얼마나 오르는지 볼 것
	if (!bEnableDiscWander)
	{
		if (!IsInsideFestivalBounds(Unit.Position))
		{
			++WanderContainmentNudges;
			return MakeContainmentDestination(Unit.Position);
		}

		++WanderStalls;
		return Unit.Position;
	}

	++DiscWanderCount;

	for (int32 Attempt = 0; Attempt < MaxAttempts; ++Attempt)
	{
		const float Angle =
			FFF_DeterministicRandom::MakeUniformFraction(DeriveAttemptSeed(SeedAngle, Attempt)) * 2.f * PI;
		const float Radius = WanderRadiusUU * FMath::Sqrt(
			FFF_DeterministicRandom::MakeUniformFraction(DeriveAttemptSeed(SeedRadius, Attempt)));

		FVector Destination = Unit.Position + FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), 0.f);

		if (!HeightField.IsWalkable(FVector2D(Destination.X, Destination.Y)))
		{
			continue;
		}

		if (!IsInsideFestivalBounds(Destination))
		{
			++WanderZoneRejects;
			continue;
		}

		return Destination;
	}

	// 전부 기각됐을 때, 밖에 선 개체만 되돌린다. 안쪽이면 제자리로 둬도 다음 라운드에 다시 뽑는다
	if (!IsInsideFestivalBounds(Unit.Position))
	{
		++WanderContainmentNudges;
		return MakeContainmentDestination(Unit.Position);
	}

	++WanderStalls;
	return Unit.Position;
}

// 레인 위의 먼 지점을 배회 목적지로 고른다.
//
// ★ 후보는 "가까운 레인 x 두 방향"이다. 열린 레인은 양 끝이, 닫힌 레인은 앞뒤로 한 걸음이
//   후보가 된다. 교차로에 선 개체는 이어진 세그먼트가 전부 반경 안에 들어오므로 후보가
//   여러 개가 되고, 그중 하나를 뽑는 것이 곧 갈림길 선택이다. 막다른 길에서는 되돌아가는
//   방향만 남는다.
//
// ★ 끝까지 보내는 것이 핵심이다. 중간에 세워 두면 다음 추첨도 같은 세그먼트 안에서 일어나
//   교차로에 영영 못 닿고, 확산이 다시 원판 배회 수준으로 떨어진다.
//
// ★ 시드는 PickWanderDestination이 쓰는 것에서 갈라 쓴다. MakeDecisionSeed의 salt 공간은
//   라운드당 두 칸으로 이미 꽉 차 있어 새 salt를 쓰면 다음 라운드 시드와 겹친다.
//
// ★ 후보를 못 내거나 전부 통행 상한·비보행 지점이면 false를 돌려 원판 배회로 떨어진다.
//   도로를 한 칸도 안 깐 맵에서는 항상 이 경로를 탄다.
bool UFF_CrowdSimulationSubsystem::TryPickPatrolDestination(const int32 LeaderAgentIndex,
	const uint32 BaseSeed, FVector& OutDestination) const
{
	if (Lanes.IsEmpty() || !UnitDatas.IsValidIndex(LeaderAgentIndex))
	{
		return false;
	}

	const FUnitData& Unit = UnitDatas[LeaderAgentIndex];

	struct FPatrolCandidate
	{
		int32 LaneIndex = INDEX_NONE;
		float TargetArcCm = 0.f;
	};

	TArray<FPatrolCandidate, TInlineAllocator<8>> Candidates;

	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		const FFF_CrowdSplineLane& Lane = Lanes[LaneIndex];
		if (!Lane.IsUsable())
		{
			continue;
		}

		FVector ClosestCm = FVector::ZeroVector;
		float ArcCm = 0.f;
		float DistCm = 0.f;
		// 반폭을 더해서 잰다. FindClosestOnLane은 중심선까지의 거리라, 안 더하면 넓은 도로 위에
		// 서 있는 개체가 "도로 밖"으로 판정된다
		if (!CrowdSplinePath::FindClosestOnLane(Lane, Unit.Position, ClosestCm, ArcCm, DistCm)
			|| DistCm > PatrolLaneSnapRadiusCm + Lane.HalfWidthCm)
		{
			continue;
		}

		// 일방통행 레인은 역방향 후보를 안 낸다. 내면 경로 쪽(TryBuildPath)이 그 방향을 거부해
		// NavMesh로 떨어지고, 결국 레인을 벗어나 직선으로 가로지른다
		if (Lane.bClosedLoop)
		{
			Candidates.Add({ LaneIndex, ArcCm + PatrolLoopStepCm });
			if (!Lane.bOneWay)
			{
				Candidates.Add({ LaneIndex, ArcCm - PatrolLoopStepCm });
			}
			continue;
		}

		const float TotalCm = Lane.TotalLengthCm();
		if (TotalCm - ArcCm >= PatrolMinTravelCm)
		{
			Candidates.Add({ LaneIndex, TotalCm });
		}
		if (!Lane.bOneWay && ArcCm >= PatrolMinTravelCm)
		{
			Candidates.Add({ LaneIndex, 0.f });
		}
	}

	if (Candidates.IsEmpty())
	{
		return false;
	}

	constexpr uint32 PatrolSeedSalt = 0x9E3779B9u;
	const uint32 PatrolSeed = HashCombine(BaseSeed, PatrolSeedSalt);
	const int32 FirstPick = FMath::Clamp(
		FMath::FloorToInt(FFF_DeterministicRandom::MakeUniformFraction(PatrolSeed) * Candidates.Num()),
		0, Candidates.Num() - 1);

	for (int32 Offset = 0; Offset < Candidates.Num(); ++Offset)
	{
		const FPatrolCandidate& Candidate = Candidates[(FirstPick + Offset) % Candidates.Num()];

		FVector Destination = FVector::ZeroVector;
		if (!CrowdSplinePath::PointAtArc(Lanes[Candidate.LaneIndex], Candidate.TargetArcCm, Destination))
		{
			continue;
		}

		if (FVector::Dist2D(Destination, Unit.Position) < PatrolMinTravelCm)
		{
			continue;
		}

		if (!HeightField.IsWalkable(FVector2D(Destination.X, Destination.Y)))
		{
			continue;
		}

		// Zone 판정 전에 Z를 맞춘다 — ContainsWorldPoint가 수직 범위도 보기 때문이다
		Destination.Z = Unit.Position.Z;

		if (!IsInsideFestivalBounds(Destination))
		{
			++WanderZoneRejects;
			continue;
		}

		OutDestination = Destination;
		return true;
	}

	return false;
}

// 개체가 실제로 퍼졌는지 재는 두 값.
//
// 표준편차는 중심에서 얼마나 멀리 흩어졌는지만 본다. 개체가 축제장 양 끝 두 덩어리로
// 뭉쳐 있어도 크게 나오므로 이것만 보면 속는다. 엔트로피(Pielou 균등도)는 점유한 칸들에
// 인원이 고르게 퍼졌는지를 0..1로 내므로, 두 값을 같이 봐야 판정이 선다.
bool UFF_CrowdSimulationSubsystem::GetSpreadStats(FVector& OutCentroidCm, float& OutStdDevCm,
	float& OutNormalizedEntropy, int32& OutSampleCount, int32& OutOccupiedBuckets) const
{
	OutCentroidCm = FVector::ZeroVector;
	OutStdDevCm = 0.f;
	OutNormalizedEntropy = 0.f;
	OutSampleCount = 0;
	OutOccupiedBuckets = 0;

	const float BucketSizeCm = FMath::Max(SpreadBucketSizeCm, 1.f);

	double SumX = 0.0;
	double SumY = 0.0;
	TMap<FIntPoint, int32> Buckets;

	for (const FUnitData& Unit : UnitDatas)
	{
		if (!Unit.bIsActive)
		{
			continue;
		}

		SumX += Unit.Position.X;
		SumY += Unit.Position.Y;
		++OutSampleCount;

		const FIntPoint Key(
			FMath::FloorToInt(Unit.Position.X / BucketSizeCm),
			FMath::FloorToInt(Unit.Position.Y / BucketSizeCm));
		++Buckets.FindOrAdd(Key);
	}

	if (OutSampleCount == 0)
	{
		return false;
	}

	OutCentroidCm = FVector(SumX / OutSampleCount, SumY / OutSampleCount, 0.0);

	double SumSquaredCm = 0.0;
	for (const FUnitData& Unit : UnitDatas)
	{
		if (!Unit.bIsActive)
		{
			continue;
		}

		SumSquaredCm += FVector::DistSquared2D(Unit.Position, OutCentroidCm);
	}

	OutStdDevCm = static_cast<float>(FMath::Sqrt(SumSquaredCm / OutSampleCount));
	OutOccupiedBuckets = Buckets.Num();

	if (Buckets.Num() <= 1)
	{
		return true;
	}

	double Entropy = 0.0;
	for (const TPair<FIntPoint, int32>& Bucket : Buckets)
	{
		const double Probability = static_cast<double>(Bucket.Value) / OutSampleCount;
		Entropy -= Probability * FMath::Loge(Probability);
	}

	OutNormalizedEntropy = static_cast<float>(Entropy / FMath::Loge(static_cast<double>(Buckets.Num())));
	return true;
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
		bMainProgramAudienceActive = false;
		bMainProgramShowStarted = false;
		MainProgramStartLiveSeconds = 0.f;
		MainProgramEndLiveSeconds = 0.f;
		NextAudienceGatherSimTime = 0.f;
		bWarnedMissingAudienceThisShow = false;
		ActiveMainStageEventLabel = NAME_None;
		ReleaseAllAudienceParties(false);
	}
	if (CurrentStage == EFF_FestivalStage::Closing)
	{
		AllAgentLeavingTrigger();
	}
	else if (CurrentStage == EFF_FestivalStage::Live)
	{
		LiveStartSimTime = SimTimeSeconds;
		// 홍보 배수 반영. 외부가 채워준 값이 있으면 그걸 쓰고, 없으면(경제 없는
		// 독립 테스트 등) 기존처럼 지난 캐시값을 그대로 쓴다 — 아무도 안 채우면 동작이 안 바뀐다.
		const int32 VisitorCount = ExpectedVisitorCountOverride != INDEX_NONE
			? ExpectedVisitorCountOverride
			: ArrivalRequestedVisitorCount;
		BeginArrivalSchedule(VisitorCount,ArrivalSpawnCenter,ArrivalSpawnRadiusUU,
			GatherReservedPartyRequests());

	}
	else if (CurrentStage == EFF_FestivalStage::Report)
	{
		DespawnAllAgent();
	}
}

void UFF_CrowdSimulationSubsystem::LogAudiencePhaseBreakdown(const TCHAR* Reason) const
{
	int32 Counts[6] = {0, 0, 0, 0, 0, 0};
	int32 Members = 0;
	// 좌석까지 남은 거리. 이동 시간이 모자란 것과 자리 앞에서 막힌 것을 가르는 유일한 수치다.
	float WalkingDistanceSumM = 0.f;
	float FarthestWalkingM = 0.f;
	int32 NearlyThereCount = 0;
	for (int32 LeaderIndex = 0; LeaderIndex < PartyDecisionStates.Num(); ++LeaderIndex)
	{
		if (!UnitDatas.IsValidIndex(LeaderIndex) || !UnitDatas[LeaderIndex].bIsActive
			|| UnitDatas[LeaderIndex].LeaderAgentIndex != LeaderIndex)
		{
			continue;
		}
		const EFF_PartyAudiencePhase Phase = PartyDecisionStates[LeaderIndex].AudiencePhase;
		if (Phase == EFF_PartyAudiencePhase::None)
		{
			continue;
		}
		++Counts[static_cast<uint8>(Phase)];
		Members += PartyDecisionStates[LeaderIndex].AudienceSlotCount;

		if (Phase == EFF_PartyAudiencePhase::GoingToSlot)
		{
			const float DistanceM = 0.01f * static_cast<float>(FVector::Dist2D(
				UnitDatas[LeaderIndex].Position, UnitDatas[LeaderIndex].TargetLocation));
			WalkingDistanceSumM += DistanceM;
			FarthestWalkingM = FMath::Max(FarthestWalkingM, DistanceM);
			if (DistanceM <= 5.f)
			{
				++NearlyThereCount;
			}
		}
	}
	UE_LOG(LogTemp, Log,
		TEXT("[Audience] %s — 좌석이동 %d / 착석대기 %d / 관람 %d / 퇴장 %d / 입구이동 %d 파티, 좌석 %d석"),
		Reason,
		Counts[static_cast<uint8>(EFF_PartyAudiencePhase::GoingToSlot)],
		Counts[static_cast<uint8>(EFF_PartyAudiencePhase::Waiting)],
		Counts[static_cast<uint8>(EFF_PartyAudiencePhase::Spectating)],
		Counts[static_cast<uint8>(EFF_PartyAudiencePhase::Exiting)],
		Counts[static_cast<uint8>(EFF_PartyAudiencePhase::GoingToEntry)],
		Members);

	const int32 Walking = Counts[static_cast<uint8>(EFF_PartyAudiencePhase::GoingToSlot)];
	if (Walking > 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Audience]   좌석까지 평균 %.0fm / 최대 %.0fm, 5m 안 %d파티, 제자리착석 누적 %d"),
			WalkingDistanceSumM / Walking, FarthestWalkingM, NearlyThereCount, AudienceSettledInPlace);
		UE_LOG(LogTemp, Log, TEXT("[Audience]   좌석 회수 누적 %d파티"), AudienceSeatsReclaimed);
	}
}

int32 UFF_CrowdSimulationSubsystem::ReclaimUnreachableAudienceSeats()
{
	// 좌석은 배정 시점에 잡힌다. 끝내 못 오는 파티가 자리를 쥐고 있으면 그 자리는 공연 내내
	// 비어 있고, 정작 닿을 수 있는 손님이 정원에서 밀린다(`capacityRejects`).
	if (MainProgramEndLiveSeconds <= 0.f)
	{
		return 0;
	}
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	if (!Stage)
	{
		return 0;
	}

	const float RemainingShowSeconds = MainProgramEndLiveSeconds - Stage->ElapsedLiveSeconds;
	const float Hurry = FMath::Max(ScheduledHurrySpeedMultiplier, 1.f);
	const float Slack = FMath::Max(AudienceTravelSlack, 1.f);

	int32 Reclaimed = 0;
	for (int32 LeaderIndex = 0; LeaderIndex < PartyDecisionStates.Num(); ++LeaderIndex)
	{
		if (!UnitDatas.IsValidIndex(LeaderIndex)
			|| !UnitDatas[LeaderIndex].bIsActive
			|| UnitDatas[LeaderIndex].LeaderAgentIndex != LeaderIndex
			|| PartyDecisionStates[LeaderIndex].AudiencePhase != EFF_PartyAudiencePhase::GoingToSlot)
		{
			continue;
		}

		const FUnitData& Leader = UnitDatas[LeaderIndex];
		const float SpeedCmPerSecond = FMath::Max(Leader.CruiseSpeed, 1.f) * Hurry;
		const float TravelSeconds = static_cast<float>(
			FVector::Dist2D(Leader.Position, Leader.TargetLocation)) / SpeedCmPerSecond;
		if (TravelSeconds * Slack <= RemainingShowSeconds)
		{
			continue;
		}

		// 자리를 돌려주고 평소 행동으로 보낸다. 출구로 보내지 않는다 — 들어간 적이 없다.
		// 같은 집결 호출이 이 파티를 다시 배정하면 좌석만 바뀌며 영원히 도착하지 못한다.
		AudiencePartiesExcludedThisShow.Add(Leader.PartyID);
		ReleaseAudienceParty(LeaderIndex, false);
		RequestDecision(LeaderIndex, EFF_DecisionReason::Interrupted);
		++Reclaimed;
	}

	if (Reclaimed > 0)
	{
		AudienceSeatsReclaimed += Reclaimed;
		UE_LOG(LogTemp, Log,
			TEXT("[Audience] 좌석 회수: %d파티 — 공연 종료까지 %.1f초라 도착 불가. 누적 %d"),
			Reclaimed, RemainingShowSeconds, AudienceSeatsReclaimed);
	}
	return Reclaimed;
}

void UFF_CrowdSimulationSubsystem::BeginMainStageGathering(const float ShowStartSeconds)
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	if (!Stage || !Stage->IsMainProgramQueueBreakActive())
	{
		return;
	}

	// Queue의 서비스 완료·매출 이벤트를 반영한 뒤에 파티 목적지를 관람석으로 바꾼다.
	if (UFF_ZoneSystemQueueBridgeSubsystem* Bridge = World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>())
	{
		Bridge->SynchronizeMainProgramQueueBreak();
	}

	const bool bFirstCall = !bMainProgramAudienceActive;
	bMainProgramAudienceActive = true;
	MainProgramStartLiveSeconds = ShowStartSeconds;
	if (bFirstCall)
	{
		bMainProgramShowStarted = Stage && Stage->ElapsedLiveSeconds >= ShowStartSeconds;
	}

	ReclaimUnreachableAudienceSeats();
	const int32 Gathered = GatherPartiesToMainStageAudience();
	NextAudienceGatherSimTime = SimTimeSeconds + FMath::Max(AudienceGatherIntervalSeconds, 0.5f);

	if (bFirstCall || Gathered > 0)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Audience] %s: %d파티 배정, 공연 시각 %.1f초, 현재 %.1f초"),
			bFirstCall ? TEXT("집결 시작") : TEXT("집결 추가"), Gathered, ShowStartSeconds,
			Stage ? Stage->ElapsedLiveSeconds : -1.f);
	}
}

void UFF_CrowdSimulationSubsystem::UpdateAudienceShowState()
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	if (!Stage || Stage->CurrentStage != EFF_FestivalStage::Live)
	{
		return;
	}

	// Queue와 같은 창구를 사용한다. 20:00 공연이면 19:57부터이며 배속 잠금과는 별개다.
	float ShowStartSeconds = 0.f;
	float ShowEndSeconds = 0.f;
	if (!Stage->TryGetMainProgramQueueBreakWindow(ShowStartSeconds, ShowEndSeconds))
	{
		if (bMainProgramAudienceActive)
		{
			HandleMainProgramEnded(ActiveMainStageEventLabel.ToString());
		}
		return;
	}

	if (bMainProgramAudienceActive && MainProgramStartLiveSeconds != ShowStartSeconds)
	{
		HandleMainProgramEnded(ActiveMainStageEventLabel.ToString());
	}
	if (!bMainProgramAudienceActive)
	{
		MainProgramEndLiveSeconds = ShowEndSeconds;
		BeginMainStageGathering(ShowStartSeconds);
	}
	// 입장은 Live 내내 이어진다. 한 번만 훑으면 그 뒤에 온 손님이 영영 안 온다.
	else if (SimTimeSeconds >= NextAudienceGatherSimTime)
	{
		BeginMainStageGathering(MainProgramStartLiveSeconds);
	}

	if (bMainProgramShowStarted || Stage->ElapsedLiveSeconds < MainProgramStartLiveSeconds)
	{
		return;
	}

	bMainProgramShowStarted = true;
	int32 SeatedCount = 0;
	for (FPartyDecisionState& State : PartyDecisionStates)
	{
		if (State.AudiencePhase == EFF_PartyAudiencePhase::Waiting)
		{
			State.AudiencePhase = EFF_PartyAudiencePhase::Spectating;
			++SeatedCount;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("[Audience] 공연 시작 — 대기 중이던 %d파티가 관람으로 넘어간다"), SeatedCount);
	LogAudiencePhaseBreakdown(TEXT("공연 시각"));
}

void UFF_CrowdSimulationSubsystem::HandleMainProgramStarted(FString EventLabel)
{
	// 이 신호는 배속 잠금 진입이다. 실제 집결 시작은 공용 3축제분 경계가 결정한다.
	ActiveMainStageEventLabel = FName(*EventLabel);
	UpdateAudienceShowState();
}

void UFF_CrowdSimulationSubsystem::HandleMainProgramEnded(FString EventLabel)
{
	LogAudiencePhaseBreakdown(TEXT("공연 종료 직전"));
	bMainProgramAudienceActive = false;
	bMainProgramShowStarted = false;
	MainProgramStartLiveSeconds = 0.f;
	MainProgramEndLiveSeconds = 0.f;
	NextAudienceGatherSimTime = 0.f;
	AudiencePartiesExcludedThisShow.Reset();
	bWarnedMissingAudienceThisShow = false;
	ActiveMainStageEventLabel = NAME_None;
	for (int32 LeaderIndex = 0; LeaderIndex < PartyDecisionStates.Num(); ++LeaderIndex)
	{
		if (!UnitDatas.IsValidIndex(LeaderIndex)
			|| UnitDatas[LeaderIndex].LeaderAgentIndex != LeaderIndex
			|| !IsAudienceLocked(PartyDecisionStates[LeaderIndex]))
		{
			continue;
		}
		// 자리에 앉아 본 파티만 자극이 풀린다. 걸어오던 중에 끝났으면 관람한 것이 아니고,
		// 여기서 풀어 주면 방문 이력까지 남아 다음 공연의 revisit 감점을 먹는다.
		const EFF_PartyAudiencePhase Phase = PartyDecisionStates[LeaderIndex].AudiencePhase;
		if (Phase == EFF_PartyAudiencePhase::Waiting || Phase == EFF_PartyAudiencePhase::Spectating)
		{
			ApplyPartyNeedRelief(UnitDatas[LeaderIndex].PartyID);
		}
		ReleaseAudienceParty(LeaderIndex, true);
	}
	UE_LOG(LogTemp, Log, TEXT("[Audience] 메인 공연 종료('%s') — 관객을 Audience 출구로 이동"), *EventLabel);
}

void UFF_CrowdSimulationSubsystem::FlushMissingEvent()
{
	if (CrowdMemberMissingEvents.IsEmpty())
	{
		return;
	}

	const TArray<FFF_CrowdMemberMissingEvent> TempArray = MoveTemp(CrowdMemberMissingEvents);
	CrowdMemberMissingEvents.Reset();

	OnMemberMissingChanged.Broadcast(TempArray);
}

void UFF_CrowdSimulationSubsystem::QueueMissingEvent(int32 AgentIndex, const FUnitData& Unit,
	EFF_CrowdMissingChange Change)
{
	
	FFF_CrowdMemberMissingEvent MemberMissingEvent;
	MemberMissingEvent.StableAgentID = Unit.StableAgentID;
	MemberMissingEvent.PartyID = Unit.PartyID;
	MemberMissingEvent.Actor = GetAgentActor(AgentIndex);
	MemberMissingEvent.LocationUU = Unit.Position;
	MemberMissingEvent.Change = Change;
		
	CrowdMemberMissingEvents.Add(MemberMissingEvent);
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

		if (!Unit.bIsActive || Unit.LeaderAgentIndex != LeaderIndex || !State.bNeedsDecision
			|| State.bIsLeaving || State.ProgramPhase != EFF_PartyProgramPhase::None
			|| IsAudienceLocked(State))
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
		const bool bHasProgramClock = TryGetProgramLiveMinutes(Context.NowLiveMinutes);
		Context.WalkSpeedMetersPerSecond = Unit.Speed / 100.f;
		Context.ProgramReturnBufferMinutes = FMath::Max(ProgramReturnBufferMinutes, 0.f);
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

		// 리더의 개체 변주를 실어 보낸다. 파티 단위 결정이므로 리더 값만 넘어간다 —
		// 멤버 값을 섞으면 "누구 취향으로 고른 목적지인가"가 불명확해지고, 리더 상속 모델
		// (FFF_Party 주석)에서 결정을 내리는 것은 리더 한 명이다.
		if (AgentTraits.IsValidIndex(LeaderIndex))
		{
			Context.NoveltySeekingMultiplier = AgentTraits[LeaderIndex].NoveltySeekingMultiplier;
			Context.PreferenceStrengthMultiplier = AgentTraits[LeaderIndex].PreferenceStrengthMultiplier;
		}
		if (AgentPersonalThresholds.IsValidIndex(LeaderIndex))
		{
			Context.PersonalDensityThresholdRatio =
				AgentPersonalThresholds[LeaderIndex].DensityThresholdRatio;
		}
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				if (const UFF_ProgramReservationSubsystem* Reservations =
					GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>())
				{
					Context.bHasActiveReservation = bHasProgramClock && Reservations->FindActiveReservationForParty(
						Context.PartyId, Context.NowLiveMinutes, Context.ActiveReservation);
				}
			}
		}

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
			const UWorld* World = GetWorld();
			const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			const UFF_ProgramReservationSubsystem* Reservations = GameInstance
				? GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>() : nullptr;
			if (bHasProgramClock && Reservations && Reservations->IsPartyExitBlockedAt(
				Context.PartyId, Context.NowLiveMinutes))
			{
				Unit.TargetLocation = PickWanderDestination(LeaderIndex);
				State.CurrentDestinationId = NAME_None;
				State.CurrentNeedRelief = FFF_NeedReliefFlags();
				State.ConsecutiveNoChoiceCount = 0;
				State.bNeedsDecision = false;
				State.LastDecisionSimTime = SimTimeSeconds;
				++Processed;
				++DecisionsProcessed;
				continue;
			}
			State.bIsLeaving = true;
			State.LeaveStartedSimTime = SimTimeSeconds;
			Unit.TargetLocation = ValidGates.IsValidIndex(Unit.EntryGateIndex)
			? ValidGates[Unit.EntryGateIndex].GateLocation
			: Temp_ExitWorldLocation;
			State.CurrentDestinationId = bDecided ? Result.DestinationId : NAME_None;;
			State.CurrentNeedRelief = bDecided ? Result.NeedRelief : FFF_NeedReliefFlags();
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

		State.ConsecutiveNoChoiceCount = (bDecided || Context.bHasActiveReservation)
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
	
	if (Unit.bHasQueueSlot)
	{
		Unit.TargetLocation = Unit.QueueSlotLocation;
		const double DistSq = FVector::DistSquared2D(Unit.Position, Unit.QueueSlotLocation);
		Unit.Speed = (DistSq <= FMath::Square(Unit.ArrivalRadiusUU)) ? 0.f : Unit.CruiseSpeed;
		
		if (Unit.bMissing)
		{
			QueueMissingEvent(AgentIndex, Unit, EFF_CrowdMissingChange::EnteredQueue);
		}

		Unit.bMissing = false;
		Unit.MissingCount = 0;

		return true;
	}
	

	// 리더가 서면 멤버도 서야 추월-복귀 왕복이 끊긴다. 속도를 리더 실측 속도에 묶는 이유다
	const FUnitData& Leader = UnitDatas[LeaderIndex];
	const FVector Offset = GetPartyFormationOffset(Unit.PartySlotIndex);

	const float LeaderSpeedUU = static_cast<float>(Leader.Velocity.Size2D());
	const bool bLeaderStopped = LeaderSpeedUU <= PartyLeaderStoppedSpeedUU;

	Unit.TargetLocation = Leader.GetSteerTarget() + Offset;

	const float CatchUpRadius = FMath::Max(PartyCohesionRadiusUU, 1.f);
	const float ReleaseRadius = CatchUpRadius * PartyCatchUpReleaseRatio;

	const double DistToSlotSq = FVector::DistSquared2D(Unit.Position, Leader.Position + Offset);

	
	Unit.bCatchingUpToLeader = Unit.bCatchingUpToLeader
		? (DistToSlotSq > FMath::Square(ReleaseRadius))
		: (DistToSlotSq > FMath::Square(CatchUpRadius));
	
	const bool bPartyLeaving = PartyDecisionStates.IsValidIndex(LeaderIndex)
		&& (PartyDecisionStates[LeaderIndex].bIsLeaving
			|| PartyDecisionStates[LeaderIndex].AudiencePhase == EFF_PartyAudiencePhase::Exiting);

	if (bPartyLeaving)
	{
		if (Unit.bMissing)
		{
			QueueMissingEvent(AgentIndex, Unit, EFF_CrowdMissingChange::Returned);
		}

		Unit.bMissing = false;
		Unit.MissingCount = 0;
	}
	else if (Unit.MissingCount > MissingThreshholdSteps)
	{

		if (!Unit.bMissing )
		{
			QueueMissingEvent(AgentIndex,Unit,EFF_CrowdMissingChange::Started);
		}

		Unit.bMissing = true;

	}

	if (Unit.bCatchingUpToLeader)
	{
		Unit.Speed = Unit.MaxSpeed;
		if (!bPartyLeaving)
		{
			Unit.MissingCount++;
		}

		if (bLeaderStopped)
		{
			Unit.TargetLocation = Leader.Position + Offset;
		}

		return true;
	}
	else
	{
		if (Unit.bMissing)
		{
			QueueMissingEvent(AgentIndex, Unit, EFF_CrowdMissingChange::Returned);
		}

		Unit.MissingCount = 0;
		Unit.bMissing = false;
	}

	Unit.Speed = bLeaderStopped
		? 0.f
		: FMath::Min(Unit.CruiseSpeed, LeaderSpeedUU * PartyFormationSpeedHeadroom);

	return true;
}

void UFF_CrowdSimulationSubsystem::UpdateFormationBlockState(int32 AgentIndex, FUnitData& Unit)
{
	if (Unit.bHasQueueSlot)
	{
		Unit.bFormationPathBlocked = false;
		Unit.FormationClearStreak = 0;
		return;
	}

	if ((SimStepIndex + static_cast<uint32>(Unit.StableAgentID)) % FormationBlockCheckIntervalSteps != 0)
	{
		return;
	}

	const FVector2f FromMeters(
		static_cast<float>(Unit.Position.X) * FFF_CrowdUnits::MetersPerUU,
		static_cast<float>(Unit.Position.Y) * FFF_CrowdUnits::MetersPerUU);
	const FVector2f ToMeters(
		static_cast<float>(Unit.TargetLocation.X) * FFF_CrowdUnits::MetersPerUU,
		static_cast<float>(Unit.TargetLocation.Y) * FFF_CrowdUnits::MetersPerUU);

	const bool bClear = FFF_CrowdWalls::IsSegmentClear(AvoidanceWalls, FromMeters, ToMeters,
		Unit.Radius * FFF_CrowdUnits::MetersPerUU);

	if (!bClear)
	{
		Unit.FormationClearStreak = 0;

		if (!Unit.bFormationPathBlocked)
		{
			Unit.bFormationPathBlocked = true;
			++FormationBlockEvents;

			UE_LOG(LogFestaFiestaNpc, Verbose,
				TEXT("[대형 막힘] 파티 %d 멤버 %d: 목표까지 직선이 건물에 막혔다 (거리 %.0fUU)"),
				Unit.PartyID, AgentIndex, FVector::Dist2D(Unit.Position, Unit.TargetLocation));
		}
		return;
	}

	if (Unit.bFormationPathBlocked && ++Unit.FormationClearStreak >= FormationBlockReleaseChecks)
	{
		Unit.bFormationPathBlocked = false;
		Unit.FormationClearStreak = 0;

		UE_LOG(LogFestaFiestaNpc, Verbose,
			TEXT("[대형 막힘] 파티 %d 멤버 %d: 직선이 다시 뚫렸다"), Unit.PartyID, AgentIndex);
	}
}

int32 UFF_CrowdSimulationSubsystem::GetFormationBlockedMemberCount() const
{
	int32 Count = 0;
	for (const FUnitData& Unit : UnitDatas)
	{
		if (Unit.bIsActive && Unit.bFormationPathBlocked)
		{
			++Count;
		}
	}
	return Count;
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
	for (const int32 PartyId : DeleteParty)
	{
		if (const int32* LeaderIndex = PartyLeaderByID.Find(PartyId))
		{
			ReleaseAudienceParty(*LeaderIndex, false);
		}
	}

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
	
	if (UnitDatas[TargetAgent].bHasQueueSlot)
	{
		ClearPartyQueueColumn(UnitDatas[TargetAgent].PartyID);
	}

	UnitDatas[TargetAgent].TargetLocation = Destination;
}

bool UFF_CrowdSimulationSubsystem::SetPartyQueueColumn(int32 PartyID, const TArray<FVector>& Locations, float InLaneYaw)
{
	const int32* LeaderPtr = PartyLeaderByID.Find(PartyID);
	const TArray<int32> Members = GetPartyMembers(PartyID);
	if (!LeaderPtr || !UnitDatas.IsValidIndex(*LeaderPtr) || Members.IsEmpty()
		|| Members.Num() != Locations.Num() || !FMath::IsFinite(InLaneYaw)
		|| Locations.ContainsByPredicate([](const FVector& Location) { return Location.ContainsNaN(); }))
	{
		return false;
	}
	for (const int32 AgentIndex : Members)
	{
		if (!Locations.IsValidIndex(UnitDatas[AgentIndex].PartySlotIndex))
		{
			return false; // 인원 불일치를 마지막 한 자리에 겹쳐 세우지 않는다.
		}
	}

	bool bSlotChanged = false;
	for (const int32 AgentIndex : Members)
	{
		FUnitData& Unit = UnitDatas[AgentIndex];
		const FVector& NewLocation = Locations[Unit.PartySlotIndex];
		if (!Unit.bHasQueueSlot || !Unit.QueueSlotLocation.Equals(NewLocation, 0.1f))
		{
			bSlotChanged = true;
			// 한 칸(90cm)이 일반 재탐색 오차(100cm)보다 작아도 이전 목적지의
			// waypoint를 버린다. 멤버의 formation 우회 경로도 함께 해제한다.
			if (PathFollowers.IsValidIndex(AgentIndex))
			{
				PathFollowers[AgentIndex].Reset();
			}
			if (AgentLaneIndices.IsValidIndex(AgentIndex))
			{
				AgentLaneIndices[AgentIndex] = INDEX_NONE;
			}
			Unit.bHasSteerTarget = false;
			Unit.bFormationPathBlocked = false;
			Unit.FormationClearStreak = 0;
			Unit.StuckSteps = 0;
			Unit.UnstuckHoldSteps = 0;
		}
		Unit.QueueSlotLocation = NewLocation;
		Unit.TargetLocation = NewLocation;
		Unit.LaneYaw = InLaneYaw;
		Unit.bHasQueueSlot = true;
		Unit.bUsePreciseSlotPosition = true;
		Unit.ArrivalRadiusUU = GetAssignedSlotArrivalRadius();
		Unit.Speed = Unit.CruiseSpeed;
	}
	if (bSlotChanged && PartyDecisionStates.IsValidIndex(*LeaderPtr))
	{
		FPartyDecisionState& State = PartyDecisionStates[*LeaderPtr];
		if (State.QueuePhase == EFF_PartyQueuePhase::AtSlot)
		{
			// 앞팀이 떠나면 Queue도 기존 도착 확인을 지운다. 새 자리까지
			// 이동하고 다시 확인해야 다음 팀의 서비스가 계속 진행된다.
			State.QueuePhase = EFF_PartyQueuePhase::Joined;
		}
	}
	return true;
}

bool UFF_CrowdSimulationSubsystem::ClearPartyQueueColumn(int32 PartyID)
{
	const int32* LeaderPtr = PartyLeaderByID.Find(PartyID);
	
	if (!LeaderPtr)
	{
		return false;
	}
	
	for ( const int32 AgentIndex : GetPartyMembers(PartyID))
	{
		FUnitData& Unit = UnitDatas[AgentIndex];
		
		Unit.QueueSlotLocation = FVector::ZeroVector;
		Unit.LaneYaw = 0.f;
		Unit.bHasQueueSlot = false;
		Unit.bUsePreciseSlotPosition = false;
	}
	
	return true;
	
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

AActor* UFF_CrowdSimulationSubsystem::GetAgentActor(int32 AgentIndex) const
{
	const int32 ActorSlot = PoolBinding.GetBoundSlot(AgentIndex);
	if (ActorSlot == INDEX_NONE || !CrowdPool.IsValidIndex(ActorSlot))
	{
		return nullptr;
	}

	AFF_BaseCrowd* Actor = CrowdPool[ActorSlot];
	return IsValid(Actor) ? Actor : nullptr;
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
int32 UFF_CrowdSimulationSubsystem::ApplySatisfactionPenaltyInArea(
	const FVector& Center, const float RadiusCm, const float DropPoints)
{
	if (RadiusCm <= 0.f || DropPoints <= 0.f)
	{
		return 0;
	}

	const float RadiusSquared = FMath::Square(RadiusCm);
	int32 AffectedCount = 0;
	for (FUnitData& Unit : UnitDatas)
	{
		if (!Unit.bIsActive || FVector::DistSquared2D(Unit.Position, Center) > RadiusSquared)
		{
			continue;
		}
		Unit.SatisfactionPenalty = FMath::Min(Unit.SatisfactionPenalty + DropPoints, 100.f);
		++AffectedCount;
	}

	UE_LOG(LogFestaFiestaNpc, Log,
		TEXT("[만족도] 영역 감점: 중심 %s 반경 %.0fcm 에서 %d명에게 %.1f점 (사람당)."),
		*Center.ToCompactString(), RadiusCm, AffectedCount, DropPoints);
	return AffectedCount;
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
	UFF_SatisfactionMetricsSubsystem* Satisfaction = nullptr;
	if (bAggregateMetrics)
	{
		if (const UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				NeedMetrics = GameInstance->GetSubsystem<UFF_NpcNeedMetricsSubsystem>();
				Satisfaction = GameInstance->GetSubsystem<UFF_SatisfactionMetricsSubsystem>();
			}
		}
	}

	if (NeedMetrics)
	{
		NeedMetrics->BeginAggregation();
	}
	if (Satisfaction)
	{
		Satisfaction->BeginAggregation();
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

		// 감점은 집계하지 않는 스텝에도 시간이 흐른 만큼 회복시킨다. 안 그러면 집계가 꺼진 동안 멈춰 있다.
		if (Unit.SatisfactionPenalty > 0.f)
		{
			Unit.SatisfactionPenalty = FMath::Max(
				0.f, Unit.SatisfactionPenalty - SatisfactionPenaltyRecoverPerMinute * FMath::Max(ElapsedMinutes, 0.f));
		}
		// 감점이 없는 방문객도 0으로 넣는다. 평균이 감점된 사람끼리가 아니라 전체 방문객 기준이 되게 하려는 것이다.
		if (Satisfaction)
		{
			Satisfaction->AddPenaltySample(Unit.SatisfactionPenalty);
		}

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

	// 만족도의 대기·밀도 입력. 취향매칭 입력은 FF_CrowdDecisionSubsystem이 결정 확정
	// 시점마다 이미 넣어 뒀다 — 여기서는 이번 창의 대기·밀도만 채우고 셋을 함께 확정한다.
	if (Satisfaction)
	{
		const FFF_SatisfactionCoefficients Coefficients =
			FFF_SatisfactionCoefficients::LoadFromTable(CachedCoefficientTable);

		// 대기: Zone 시설 전수의 평균 완료 대기시간(초). 완료자가 없는 시설(0.0)은 뺀다 —
		// "아직 모름"과 "대기 없음"을 섞지 않는다(ViewModel::GetLiveFacilities와 같은 판단).
		if (const UWorld* World = GetWorld())
		{
			if (const UFFZSWorldSubsystem* Zone = World->GetSubsystem<UFFZSWorldSubsystem>())
			{
				const FFFZSReadModel ReadModel = Zone->BuildReadModel();
				for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
				{
					if (Facility.State == EFFZSFacilityState::Removed)
					{
						continue;
					}
					if (Facility.AverageCompletedWaitSeconds > 0.f)
					{
						Satisfaction->AddWaitSample(Facility.AverageCompletedWaitSeconds);
					}
				}
			}
		}

		// 밀도: 점유 셀마다 LOS 등급 점수를 매겨 그 셀의 인원 수로 가중한다 — "사람이 몇 명이나
		// 어떤 등급에 서 있는가"를 반영하기 위해서다. 새 순회가 아니라 이번 스텝에 이미
		// 갱신된 CrowdReadModel(SnapshotIntervalSteps)을 그대로 읽는다.
		for (const FFF_CrowdCellSample& Cell : CrowdReadModel.OccupiedCells)
		{
			if (Cell.Occupancy <= 0)
			{
				continue;
			}
			const ECrowdLOS Grade = FCrowdDensityGrid::DensityToLOS(Cell.DensityPerSqm);
			const float GradeScore = Coefficients.ScoreForLOSGrade(static_cast<uint8>(Grade));
			Satisfaction->AddDensitySample(GradeScore, Cell.Occupancy);
		}

		// 큐 가입 성공률
		// 이번 창(델타)의 수락/거절/인내이탈 비율을 대기 점수에 곱한다. 
		// 누적 카운터의 이번 창 델타만 쓴다 — 매 창마다 스냅샷을 남겨 두고 다음 창에서 뺀다.
		{
			const int32 AcceptedThisWindow = QueueJoinAccepted - LastWindowQueueJoinAccepted;
			const int32 RejectedThisWindow = QueueJoinRejected - LastWindowQueueJoinRejected;
			const int32 AbandonedThisWindow = QueuePatienceAbandons - LastWindowQueuePatienceAbandons;
			LastWindowQueueJoinAccepted = QueueJoinAccepted;
			LastWindowQueueJoinRejected = QueueJoinRejected;
			LastWindowQueuePatienceAbandons = QueuePatienceAbandons;

			const int32 TotalAttemptsThisWindow = AcceptedThisWindow + RejectedThisWindow + AbandonedThisWindow;
			if (TotalAttemptsThisWindow > 0)
			{
				Satisfaction->SetQueueSuccessRate(
					static_cast<float>(AcceptedThisWindow) / TotalAttemptsThisWindow);
			}
		}

		// Need 충족도
		// 지금 켜져 있는 성분(Hunger·Restroom·Stimulation)만 평균한다. 
		// Thirst·Fatigue는 Phase 게이트로 상승 계수가 0이라 넣으면 항상 100% 충족으로 왜곡된다
		// NeedMetrics 표본이 0이면(아직 아무도 안 들어왔거나 집계가 안 돌았으면) 넣지 않는다 —
		// "미해소 0%"가 진짜 0%인지 데이터 없음인지 구분해야 한다.
		if (NeedMetrics && NeedMetrics->GetSampleCount() > 0)
		{
			static const EFF_NeedType ActiveNeedTypes[] =
			{
				EFF_NeedType::Hunger, EFF_NeedType::Restroom, EFF_NeedType::Stimulation
			};
			for (const EFF_NeedType NeedType : ActiveNeedTypes)
			{
				Satisfaction->AddNeedFulfillmentSample(100.f - NeedMetrics->GetNeedUnmetRatio(NeedType));
			}
		}

		Satisfaction->CommitAggregation(Coefficients);
	}

	if (ElapsedMinutes > 0.f)
	{
		for (int32 LeaderIndex = 0; LeaderIndex < PartyRemainingStayMinutes.Num(); ++LeaderIndex)
		{
			if (PartyMemberCounts[LeaderIndex] <= 0)
			{
				continue;
			}
			if (PartyDecisionStates.IsValidIndex(LeaderIndex)
				&& PartyDecisionStates[LeaderIndex].ProgramPhase == EFF_PartyProgramPhase::Participating)
			{
				// 프로그램 중에도 Need는 위에서 계속 증가하지만 체류 예산만 멈춘다.
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
	// SetNumZeroed 를 쓰지 않는다 — 두 구조체의 기본값은 0 이 아니라 "변주 없음"(1.0)이고,
	// 0 으로 채우면 Resolve 가 아직 안 걸린 슬롯이 "속도 0배·임계 0"으로 동작한다.
	AgentTraits.Reset();
	AgentTraits.SetNum(UnitDatas.Num());
	AgentPersonalThresholds.Reset();
	AgentPersonalThresholds.SetNum(UnitDatas.Num());
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

				// 경로를 낸 쪽을 색으로 가른다. 레인과 NavMesh가 같은 색이면 "레인이 먹었는지"를
				// 그림만 보고 판정할 수 없다
				const int32 LaneIndex = AgentLaneIndices.IsValidIndex(AgentIndex)
					? AgentLaneIndices[AgentIndex]
					: INDEX_NONE;
				const bool bOnLane = (LaneIndex != INDEX_NONE);

				const FColor PathColor = bOnLane ? FColor(0, 255, 255) : FColor(120, 200, 255);

				FVector Previous = Unit.Position;
				for (int32 PointIndex = Follower.NextIndex; PointIndex < Follower.Points.Num(); ++PointIndex)
				{
					const FVector Point = Follower.Points[PointIndex] + FVector(0, 0, PathDrawZ);
					DrawDebugLine(World, Previous + FVector(0, 0, PathDrawZ), Point,
						PathColor, false, -1.f, 0, 2.f);
					DrawDebugPoint(World, Point, 8.f, FColor(255, 255, 120), false, -1.f, 0);
					Previous = Follower.Points[PointIndex];
				}

				// 진입점 = 레인에 올라타는 자리. 아직 소비하지 않았으면 개체에서 그 자리까지가
				// 레인 밖 직선 구간이므로 굵게 따로 긋는다
				if (bOnLane)
				{
					const FVector Entry = Follower.Points[0];

					DrawDebugLine(World, Entry, Entry + FVector(0, 0, 250.f),
						FColor(255, 0, 255), false, -1.f, 0, 3.f);
					DrawDebugSphere(World, Entry + FVector(0, 0, PathDrawZ), 35.f, 8,
						FColor(255, 0, 255), false, -1.f, 0, 2.f);

					if (Follower.NextIndex == 0)
					{
						DrawDebugLine(World, Unit.Position + FVector(0, 0, PathDrawZ),
							Entry + FVector(0, 0, PathDrawZ), FColor(255, 0, 255), false, -1.f, 0, 4.f);
					}
				}
			}
		}
		else if (UnitDatas.IsValidIndex(Unit.LeaderAgentIndex))
		{
			if (!Unit.bFormationPathBlocked)
			{
				DrawDebugLine(World, Unit.Position + FVector(0, 0, 5.f),
					UnitDatas[Unit.LeaderAgentIndex].Position + FVector(0, 0, 5.f), PartyColor, false, -1.f, 0, 1.5f);
			}
			else
			{
				const FColor BlockedColor = FColor::White;
				const FVector Lift(0, 0, 10.f);

				FVector Previous = Unit.Position;
				if (PathFollowers.IsValidIndex(AgentIndex) && PathFollowers[AgentIndex].HasRemainingWaypoint())
				{
					const FFF_CrowdPathFollower& Follower = PathFollowers[AgentIndex];
					for (int32 PointIndex = Follower.NextIndex; PointIndex < Follower.Points.Num(); ++PointIndex)
					{
						DrawDebugLine(World, Previous + Lift, Follower.Points[PointIndex] + Lift,
							BlockedColor, false, -1.f, 0, 2.f);
						Previous = Follower.Points[PointIndex];
					}
				}
				else
				{
					DrawDebugLine(World, Previous + Lift, Unit.GetSteerTarget() + Lift,
						BlockedColor, false, -1.f, 0, 2.f);
				}
			}
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
	
	if (Unit.bMissing)
	{
		QueueMissingEvent(AgentID,Unit,EFF_CrowdMissingChange::Despawned);
	}
	

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
		
		if (!UnitDatas.IsValidIndex(LeaderIndex) || !PartyDecisionStates.IsValidIndex(LeaderIndex))
		{
			continue;
		}

		FUnitData& Unit = UnitDatas[LeaderIndex];
		FPartyDecisionState& State = PartyDecisionStates[LeaderIndex];

		if (State.bIsLeaving) continue;
		ReleaseAudienceParty(LeaderIndex, false);

		// 게이트가 없거나 EntryGateIndex 가 안 잡힌 개체는 기존 단일 출구로 보낸다.
		//
		// ★ 임시 크래시 가드 추가
		//	 EntryGateIndex 기본값이 INDEX_NONE 이고 대입 지점이
		//   SpawnAgents 하나뿐이라, 콘솔 FF.Crowd.Spawn 이나 테스트의 직접 스폰으로 만든
		//   개체는 -1 로 남는다. 그대로 ValidGates[-1] 을 읽어 Crowd.Reservation.Lifecycle 이
		//   죽고 자동화 런 전체가 중단됐다.
		const FVector ExitLocation = ValidGates.IsValidIndex(Unit.EntryGateIndex)
			? ValidGates[Unit.EntryGateIndex].GateLocation
			: Temp_ExitWorldLocation;

		State.bIsLeaving = true;
		State.LeaveStartedSimTime = SimTimeSeconds;
		Unit.TargetLocation = ExitLocation;
		State.ProgramPhase = EFF_PartyProgramPhase::None;
		State.ProgramFacilityId.Invalidate();
		State.CurrentDestinationId = NAME_None;
		State.bNeedsDecision = false;
		State.LastDecisionSimTime = SimTimeSeconds;
		State.CurrentNeedRelief = FFF_NeedReliefFlags();
		ClearPartyQueueState(LeaderIndex, true);
		
		
		
	}
	
}

void UFF_CrowdSimulationSubsystem::ApplyAgentAppearance(int32 AgentIndex)
{
	
	if (!UnitDatas.IsValidIndex(AgentIndex))
	{
		return;
	}
	
	const int32 ActorSlot = PoolBinding.GetBoundSlot(AgentIndex);
	if (ActorSlot == INDEX_NONE || !CrowdPool.IsValidIndex(ActorSlot)) {return;}
	
	AFF_BaseCrowd* Actor = CrowdPool[ActorSlot];
	if (!IsValid(Actor)) { return; }
	
	FUnitData& Unit = UnitDatas[AgentIndex];
	
	const FName RowKey = Unit.bIsDependentChild ? FName(TEXT("child")) : (Unit.ArchetypeID.IsNone() ? FName(TEXT("default")) : Unit.ArchetypeID);
	
	

	const UDataTable* AppearenceTable = CachedAppearenceTable.Get();
	if (AppearenceTable == nullptr)
	{
		Actor->ApplyAppearance(nullptr,nullptr ,0.f, 1.f);
		return;
	}

	const FFF_CrowdAppearenceRow* Row = AppearenceTable->FindRow<FFF_CrowdAppearenceRow>(RowKey,TEXT("UFF_CrowdSimulationSubsystem::ApplyAgentAppearance"),false );

	if (Row == nullptr)
	{
		Actor->ApplyAppearance(nullptr, nullptr,0.f,1.f);
		return;
	}
	
	Actor->ApplyAppearance(Row->Mesh.LoadSynchronous(), Row->AnimClass.LoadSynchronous(), Row->MeshYawOffset, Row->MeshUniformScale);
	
	
	
	
	
}
