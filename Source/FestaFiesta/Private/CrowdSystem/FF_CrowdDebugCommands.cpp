// Fill out your copyright notice in the Description page of Project Settings.

#include "Camera/CameraActor.h"
#include "CollisionQueryParams.h"
#include "CrowdSystem/FF_BaseCrowd.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "DataTables/FF_ArrivalCurveRow.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "NavMesh/RecastNavMesh.h"
#include "Queue/FF_QueueCrowdLink.h"
#include "Queue/FF_QueueSubsystem.h"
#include "TimerManager.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Engine/GameInstance.h"
#include "NPC/Metrics/FF_SatisfactionMetricsSubsystem.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealClient.h"

#if !UE_BUILD_SHIPPING

namespace FFCrowdConsole
{
	FVector GetViewerLocation(const UWorld* World)
	{
		if (const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
		{
			if (const APawn* Pawn = PC->GetPawn())
			{
				return Pawn->GetActorLocation();
			}

			FVector CameraLocation;
			FRotator CameraRotation;
			const_cast<APlayerController*>(PC)->GetPlayerViewPoint(CameraLocation, CameraRotation);
			return CameraLocation;
		}

		return FVector::ZeroVector;
	}

	FVector SnapToGround(UWorld* World, const FVector& Location)
	{
		if (!World)
		{
			return Location;
		}

		FCollisionQueryParams Params(SCENE_QUERY_STAT(FFCrowdGroundSnap),  false);
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			Params.AddIgnoredActor(PC->GetPawn());
		}

		const FVector Start = Location + FVector(0.f, 0.f, 500.f);
		const FVector End = Location - FVector(0.f, 0.f, 50000.f);

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			return FVector(Location.X, Location.Y, Hit.ImpactPoint.Z);
		}

		return Location;
	}

	UFF_CrowdSimulationSubsystem* GetCrowdSim(UWorld* World, FOutputDevice& Ar)
	{
		UFF_CrowdSimulationSubsystem* CrowdSim = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
		if (!CrowdSim)
		{
			Ar.Logf(ELogVerbosity::Error, TEXT("CrowdSimulationSubsystem을 찾을 수 없습니다 (게임이 실행 중인지 확인)"));
		}
		return CrowdSim;
	}

	int32 ParseIntArg(const TArray<FString>& Args, int32 Index, int32 DefaultValue)
	{
		return Args.IsValidIndex(Index) ? FCString::Atoi(*Args[Index]) : DefaultValue;
	}

	float ParseFloatArg(const TArray<FString>& Args, int32 Index, float DefaultValue)
	{
		return Args.IsValidIndex(Index) ? FCString::Atof(*Args[Index]) : DefaultValue;
	}
}

namespace FFCrowdConsole
{
	static FString PendingCrowdClassPath;

	FString BuildClassObjectPath(const FString& InPath)
	{
		FString Path = InPath;
		Path.TrimStartAndEndInline();

		Path.RemoveFromStart(TEXT("Blueprint"));
		Path.RemoveFromStart(TEXT("BlueprintGeneratedClass"));
		Path = Path.Replace(TEXT("'"), TEXT("")).Replace(TEXT("\""), TEXT(""));

		if (Path.EndsWith(TEXT("_C")))
		{
			return Path;
		}

		int32 DotIndex = INDEX_NONE;
		if (!Path.FindChar(TEXT('.'), DotIndex))
		{
			FString AssetName;
			if (!Path.Split(TEXT("/"), nullptr, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd)
				|| AssetName.IsEmpty())
			{
				return Path;
			}
			Path = Path + TEXT(".") + AssetName;
		}

		return Path + TEXT("_C");
	}

	TSubclassOf<AFF_BaseCrowd> ResolvePendingCrowdClass()
	{
		if (PendingCrowdClassPath.IsEmpty())
		{
			return nullptr;
		}

		const FString ClassPath = BuildClassObjectPath(PendingCrowdClassPath);

		UClass* LoadedClass = StaticLoadClass(AFF_BaseCrowd::StaticClass(), nullptr, *ClassPath);
		if (!LoadedClass)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[CrowdDebug] Crowd 클래스를 로드하지 못했습니다: %s (AFF_BaseCrowd 상속인지 확인)"), *ClassPath);
			return nullptr;
		}

		return LoadedClass;
	}
}

static void CrowdClassCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num() == 0)
	{
		Ar.Logf(TEXT("현재 예약된 Crowd 클래스: %s"),
			FFCrowdConsole::PendingCrowdClassPath.IsEmpty()
				? TEXT("(없음 - 대체 원기둥으로 표시됩니다)")
				: *FFCrowdConsole::PendingCrowdClassPath);
		Ar.Logf(TEXT("사용법: FF.Crowd.Class /Game/FestaFiesta/Crowd/BP_Crowd   (해제는 FF.Crowd.Class none)"));
		return;
	}

	if (Args[0].Equals(TEXT("none"), ESearchCase::IgnoreCase) || Args[0].Equals(TEXT("clear"), ESearchCase::IgnoreCase))
	{
		FFCrowdConsole::PendingCrowdClassPath.Empty();
		Ar.Logf(TEXT("Crowd 클래스 예약을 해제했습니다 (대체 원기둥으로 표시)"));
		return;
	}

	const FString RequestedPath = Args[0];
	const FString ClassPath = FFCrowdConsole::BuildClassObjectPath(RequestedPath);

	if (!StaticLoadClass(AFF_BaseCrowd::StaticClass(), nullptr, *ClassPath))
	{
		Ar.Logf(ELogVerbosity::Error, TEXT("로드 실패: %s"), *ClassPath);
		Ar.Logf(TEXT("AFF_BaseCrowd를 상속한 BP인지, 경로가 맞는지 확인하세요"));
		return;
	}

	FFCrowdConsole::PendingCrowdClassPath = RequestedPath;
	Ar.Logf(TEXT("Crowd 클래스 예약: %s"), *ClassPath);

	if (const UFF_CrowdSimulationSubsystem* CrowdSim = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr)
	{
		if (CrowdSim->IsSimulationInitialized())
		{
			Ar.Logf(ELogVerbosity::Warning,
				TEXT("이미 초기화가 끝났습니다 - 이번 실행에는 적용되지 않습니다. PIE를 다시 시작하고 스폰 전에 이 커맨드를 먼저 쓰세요"));
		}
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdClassCommand(
	TEXT("FF.Crowd.Class"),
	TEXT("콘솔 스폰에 쓸 Crowd Actor BP를 지정한다(스폰 전에). FF.Crowd.Class [경로|none]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdClassCommand));

namespace FFCrowdConsole
{
	// AgentPool / ActorPool 은 0이면 Count 로 채운다(기존 동작). 둘을 다르게 주면
	// Agent 만 늘리고 화면 Actor 수는 묶어둘 수 있어서 시뮬 비용과 표현 비용을 나눠 잴 수 있다.
	// 풀 크기는 최초 1회만 정해지므로, 계단식 인원 스윕을 한 PIE 세션에서 돌리려면
	// 첫 호출에서 목표 최대 인원만큼 풀을 잡아둬야 한다.
	bool SpawnCrowd(UWorld* World, FOutputDevice& Ar, int32 Count, float SpawnRadiusUU,
		int32 AgentPoolSize = 0, int32 ActorPoolSize = 0)
	{
		UFF_CrowdSimulationSubsystem* CrowdSim = GetCrowdSim(World, Ar);
		if (!CrowdSim)
		{
			return false;
		}

		const FVector Origin = SnapToGround(World, GetViewerLocation(World));

		if (!CrowdSim->IsSimulationInitialized())
		{
			const TSubclassOf<AFF_BaseCrowd> CrowdClass = ResolvePendingCrowdClass();

			const int32 ResolvedAgentPool = FMath::Max(AgentPoolSize > 0 ? AgentPoolSize : Count, Count);
			const int32 ResolvedActorPool = FMath::Max(ActorPoolSize > 0 ? ActorPoolSize : ResolvedAgentPool, 1);

			CrowdSim->InitializeSimulation(Origin, ResolvedActorPool, ResolvedAgentPool, CrowdClass);

			Ar.Logf(TEXT("풀 — Agent %d, Actor %d%s"),
				ResolvedAgentPool, ResolvedActorPool,
				(ResolvedActorPool < ResolvedAgentPool)
					? TEXT("  <-- Actor가 모자라 일부는 표시되지 않는다(시뮬 비용만 재는 모드)")
					: TEXT(""));

			Ar.Logf(TEXT("표시 클래스: %s"),
				CrowdClass ? *CrowdClass->GetName() : TEXT("AFF_BaseCrowd (대체 원기둥) - FF.Crowd.Class로 BP 지정 가능"));
		}
		else if (AgentPoolSize > 0 || ActorPoolSize > 0)
		{
			Ar.Logf(ELogVerbosity::Warning,
				TEXT("풀 크기 인자는 무시된다 — 이미 초기화가 끝났다. 풀은 최초 스폰 때만 정해진다"));
		}

		const int32 Spawned = CrowdSim->SpawnAgents(Count, Origin, SpawnRadiusUU);

		Ar.Logf(TEXT("%d명 스폰 (활성 %d, Actor %d개 바인딩). 원점 %s, 반경 %.0fUU"),
			Spawned, CrowdSim->GetActiveAgentCount(), CrowdSim->GetBoundActorCount(),
			*Origin.ToCompactString(), SpawnRadiusUU);

		return true;
	}
}

static void CrowdSpawnCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const int32 Count = FFCrowdConsole::ParseIntArg(Args, 0, 500);
	const float SpawnRadiusUU = FFCrowdConsole::ParseFloatArg(Args, 1, 3000.f);
	const int32 AgentPoolSize = FFCrowdConsole::ParseIntArg(Args, 2, 0);
	const int32 ActorPoolSize = FFCrowdConsole::ParseIntArg(Args, 3, 0);

	if (FFCrowdConsole::SpawnCrowd(World, Ar, Count, SpawnRadiusUU, AgentPoolSize, ActorPoolSize))
	{
		Ar.Logf(TEXT("FF.Crowd.Debug 1 로 밀도 그리드를, 2로 Agent까지 표시합니다"));
		Ar.Logf(TEXT("메시가 붙은 BP로 보려면 스폰 전에 FF.Crowd.Class <BP경로>"));
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdSpawnCommand(
	TEXT("FF.Crowd.Spawn"),
	TEXT("군중을 스폰한다. FF.Crowd.Spawn [인원=500] [스폰반경UU=3000] [Agent풀=인원] [Actor풀=Agent풀]  "
	     "풀은 최초 1회만 정해진다 — 계단식 스윕은 첫 호출에서 목표 최대 인원으로 풀을 잡을 것"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdSpawnCommand));

static void CrowdDebugCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	const int32 Mode = FFCrowdConsole::ParseIntArg(Args, 0, 1);

	CrowdSim->bDrawDebugDensity = (Mode >= 1);
	CrowdSim->bDrawDebugAgents = (Mode >= 2);

	Ar.Logf(TEXT("디버그 표시 — 밀도 그리드 %s, Agent %s"),
		CrowdSim->bDrawDebugDensity ? TEXT("켬") : TEXT("끔"),
		CrowdSim->bDrawDebugAgents ? TEXT("켬") : TEXT("끔"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdDebugCommand(
	TEXT("FF.Crowd.Debug"),
	TEXT("디버그 표시. 0=끔, 1=밀도 그리드(LOS 색상), 2=밀도+Agent"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdDebugCommand));

static void CrowdStatsCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	const FCrowdDensityGrid& Grid = CrowdSim->GetDensityGrid();
	const float MaxDensity = Grid.GetMaxDensity();

	const ECrowdLOS WorstLOS = FCrowdDensityGrid::DensityToLOS(MaxDensity);
	const UEnum* LOSEnum = StaticEnum<ECrowdLOS>();
	const FString LOSName = LOSEnum ? LOSEnum->GetDisplayNameTextByValue(static_cast<int64>(WorstLOS)).ToString() : TEXT("?");

	const int32 ActiveAgents = CrowdSim->GetActiveAgentCount();
	const int32 PartyCount = CrowdSim->GetActivePartyCount();
	const float AveragePartySize = (PartyCount > 0) ? static_cast<float>(ActiveAgents) / PartyCount : 0.f;

	Ar.Logf(TEXT("--- Crowd ---"));
	Ar.Logf(TEXT("활성 Agent    : %d"), ActiveAgents);

	const float Fidelity = CrowdSim->GetSimTimeFidelity();
	Ar.Logf(TEXT("시뮬 충실도   : %.1f%% (버린 시간 %.2fs, %d프레임)"),
		Fidelity * 100.f, CrowdSim->GetDroppedSimSeconds(), CrowdSim->GetDroppedStepFrames());

	if (Fidelity < 0.99f)
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("시뮬이 실시간을 놓치고 있다 — 프레임률이 멀쩡해도 축제 시간이 느리게 흐른다. "
			     "인원을 줄이거나 부하 항목을 끄고 다시 잴 것 (CsvProfile start/stop, FFCrowd 카테고리)"));
	}

	Ar.Logf(TEXT("파티          : %d개 (평균 %.2f명, 그룹핑 %s)"),
		PartyCount, AveragePartySize, CrowdSim->bEnablePartyGrouping ? TEXT("켬") : TEXT("끔"));
	Ar.Logf(TEXT("화면 표시 Actor: %d"), CrowdSim->GetBoundActorCount());
	Ar.Logf(TEXT("밀도 집계 인원 : %d"), Grid.GetSampledUnitCount());
	Ar.Logf(TEXT("그리드        : %d x %d 셀, 셀 %.2fm (%.2f m^2)"),
		Grid.GetNumX(), Grid.GetNumY(), Grid.GetCellSizeMeters(), Grid.GetCellAreaSqM());
	Ar.Logf(TEXT("최대 밀도     : %.2f 명/m^2  (LOS %s)"), MaxDensity, *LOSName);
	Ar.Logf(TEXT("혼잡 셀(LOS D+): %d"), Grid.GetCongestedCellCount());

	const FFF_CrowdFlowBreakdown Flow = CrowdSim->GetFlowBreakdown();
	const float BlockedPercent = (Flow.Total() > 0)
		? (100.f * Flow.Blocked / Flow.Total())
		: 0.f;

	Ar.Logf(TEXT("--- 이동 상태 ---"));
	Ar.Logf(TEXT("걷는 중       : %d"), Flow.Walking);
	Ar.Logf(TEXT("도착·체류     : %d  (목적지 반경 안 — 막힘이 아니다)"), Flow.ArrivedIdle);
	Ar.Logf(TEXT("  도착 반경    : 큐슬롯 %.0f / 접근점 %.0f / 배회 %.0f / 퇴장 %.0f UU"),
		CrowdSim->QueueSlotArrivalRadiusUU, CrowdSim->FacilityApproachRadiusUU,
		CrowdSim->WanderArrivalRadiusUU, CrowdSim->Temp_ExitArrivalRadiusUU);

	Ar.Logf(TEXT("  결정 처리    : %d회, 쿨다운으로 버림 %d회 (간격 %.1fs)"),
		CrowdSim->GetDecisionsProcessed(), CrowdSim->GetDecisionCooldownSkips(),
		CrowdSim->DecisionCooldownSeconds);
	Ar.Logf(TEXT("  이동 중 목적지: %d곳, 한 곳 최대 %d명  <-- 최대치가 시설 정원을 넘으면 도착 후 거절이 난다"),
		CrowdSim->GetInTransitDestinationCount(), CrowdSim->GetMaxInTransitMemberCount());
	Ar.Logf(TEXT("  막혀서 포기   : %d회 (임계 %d스텝 = %.1fs)  <-- 0이면 이 경로가 안 걸린다"),
		CrowdSim->GetStuckDecisionRequests(), CrowdSim->DecisionStuckSteps,
		CrowdSim->DecisionStuckSteps / 30.f);
	Ar.Logf(TEXT("  막힘 백오프   : %d회  <-- 파티원·큐·관람·퇴장 중 리더의 막힘 카운터를 비운 횟수"),
		CrowdSim->GetStuckCounterBackoffs());
	Ar.Logf(TEXT("  주기 재평가   : %d회 (%s)  <-- 켰는데 0이면 개인 주기가 안 차고 있다"),
		CrowdSim->GetReevaluateRequests(),
		CrowdSim->bEnablePeriodicReevaluation ? TEXT("켬") : TEXT("끔"));
	Ar.Logf(TEXT("막힘          : %d (%.1f%%)  <-- 가려는데 못 가는 사람"),
		Flow.Blocked, BlockedPercent);
	Ar.Logf(TEXT("  탈출 가속 중: %d  (임계 %d스텝, 밀기 %d / 쉼 %d스텝, 최장 %d스텝 = %.1f초)"),
		Flow.UnstuckActive, CrowdSim->AvoidanceConfig.StuckDetectSteps,
		CrowdSim->AvoidanceConfig.UnstuckHoldSteps, CrowdSim->AvoidanceConfig.UnstuckCooldownSteps,
		Flow.MaxStuckSteps, Flow.MaxStuckSteps / 30.f);

	const int32 FacilityWalls = CrowdSim->GetFacilityWallCount();
	const int32 WallSegments = CrowdSim->GetWallSegmentCount();

	Ar.Logf(TEXT("--- 벽 ---"));
	Ar.Logf(TEXT("선분          : %d개  (시설 %d개 × 4)"), WallSegments, FacilityWalls);
	Ar.Logf(TEXT("대형 직선 막힘: 지금 %d명, 누적 %d회, 우회 경로 쿼리 %d건  <-- 건물에 가려 슬롯으로 직진할 수 없는 멤버"),
		CrowdSim->GetFormationBlockedMemberCount(), CrowdSim->GetFormationBlockEvents(),
		CrowdSim->GetFormationPathQueries());
	Ar.Logf(TEXT("시설 벽       : %s%s"),
		CrowdSim->bAvoidanceUseFacilityWalls ? TEXT("on") : TEXT("off"),
		CrowdSim->bAvoidanceFacilityWallsIncludeClearance ? TEXT(" + 여유 1m 링") : TEXT(""));
	Ar.Logf(TEXT("벽 역방향 상한: %.2f  %s"),
		CrowdSim->AvoidanceConfig.MaxWallOpposingAccelMps2,
		(CrowdSim->AvoidanceConfig.MaxWallOpposingAccelMps2 < 0.f)
			? TEXT("(면제 — 벽이 구동력을 이긴다)")
			: TEXT("<-- 켜져 있다. 구동력이 이기면 부스에 파고든다"));

	if (CrowdSim->bAvoidanceUseFacilityWalls && !CrowdSim->DidFacilityWallsSeeZoneSystem())
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("시설 벽이 켜져 있는데 ZoneSystem을 찾지 못했다 — 부스가 전부 통과된다"));
	}
	else if (CrowdSim->bAvoidanceUseFacilityWalls && FacilityWalls == 0)
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("ZoneSystem은 있는데 시설이 0개다 — 배치가 비었거나 액터를 못 찾았다"));
	}

	const int32 PathQueries = CrowdSim->GetPathQueriesIssued();
	const int32 PathFailures = CrowdSim->GetPathQueryFailures();
	const int32 PartialPaths = CrowdSim->GetPartialPathCount();
	const int32 Following = CrowdSim->GetPathFollowingCount();
	const int32 Backlog = CrowdSim->GetPathQueryBacklog();

	Ar.Logf(TEXT("--- 경로 ---"));
	Ar.Logf(TEXT("경로추종      : %s"), CrowdSim->bEnablePathFollowing ? TEXT("켬") : TEXT("끔"));

	// 시설 nav 구멍은 ZoneSystem의 FacilityNavigationObstacleBounds가 소유한다. Crowd가 확인할 수
	// 있는 것은 그 modifier가 런타임에 반영되는 모드인지 하나뿐이고, 여기가 어긋나면 경로가 조용히
	// 부스를 관통한다.
	const ERuntimeGenerationType NavRuntimeMode = GetDefault<ARecastNavMesh>()->GetRuntimeGenerationMode();
	const bool bModifiersBake = (NavRuntimeMode == ERuntimeGenerationType::DynamicModifiersOnly)
		|| (NavRuntimeMode == ERuntimeGenerationType::Dynamic);

	Ar.Logf(TEXT("시설 nav 구멍 : ZoneSystem 소유, 런타임 갱신 %s"),
		bModifiersBake ? TEXT("켬") : TEXT("끔"));

	if (!bModifiersBake)
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("RecastNavMesh가 Static이다 — 시설 nav 구멍이 구워지지 않아 경로가 부스를 관통한다. "
			     "DefaultEngine.ini의 RuntimeGeneration=DynamicModifiersOnly를 확인할 것"));
	}

	if (CrowdSim->bEnablePathFollowing)
	{
		Ar.Logf(TEXT("따라가는 중   : %d명  <-- 0이면 우회가 전혀 안 걸리고 있다"), Following);
		Ar.Logf(TEXT("쿼리 / 실패   : %d / %d건 (%.0f%%)"),
			PathQueries, PathFailures,
			PathQueries > 0 ? (100.f * PathFailures / PathQueries) : 0.f);
		Ar.Logf(TEXT("부분 경로     : %d건 (%.1f%%)  <-- 목적지가 NavMesh 밖. 닿는 데까지만 간다"),
			PartialPaths,
			PathQueries > 0 ? (100.f * PartialPaths / PathQueries) : 0.f);
		Ar.Logf(TEXT("경로 대기     : %d명 (예산 %d건/스텝)"), Backlog, CrowdSim->PathQueriesPerStep);
		Ar.Logf(TEXT("배치변경 폐기 : %d회  <-- 시설 설치·이동·철거 횟수와 맞아야 한다"),
			CrowdSim->GetPathLayoutInvalidations());
		Ar.Logf(TEXT("웨이포인트 반경: %.0fUU   재경로 허용오차 %.0fUU   최소 쿼리거리 %.0fUU"),
			CrowdSim->WaypointAcceptanceRadiusUU, CrowdSim->RepathToleranceUU,
			CrowdSim->MinPathQueryDistanceUU);

		if (PathQueries > 0 && PathFailures * 2 > PathQueries)
		{
			Ar.Logf(ELogVerbosity::Warning,
				TEXT("경로 쿼리 절반 이상이 실패한다 — NavMesh가 없거나 개체가 그 위에 없다. "
				     "그 개체들은 직선으로 걸어 건물에 붙는다"));
		}

		if (PathQueries > 0 && PartialPaths * 10 > PathQueries)
		{
			Ar.Logf(ELogVerbosity::Warning,
				TEXT("경로 10건 중 1건 이상이 부분 경로다 — 목적지가 NavMesh 위에 있는지 확인할 것. "
				     "그 개체는 닿는 데까지 간 뒤 남은 거리를 직선으로 밀어 건물에 붙어 미끄러진다"));
		}

		if (Backlog > 0)
		{
			Ar.Logf(ELogVerbosity::Warning,
				TEXT("경로 예산이 모자란다(%d명 대기) — PathQueriesPerStep을 올릴 것"), Backlog);
		}

		const int32 LaneCount = CrowdSim->GetLaneCount();
		const int32 LanePaths = CrowdSim->GetLanePathCount();
		const int32 NavFallbacks = CrowdSim->GetNavFallbackCount();
		const int32 LaneAttempts = LanePaths + NavFallbacks;

		Ar.Logf(TEXT("--- 레인 ---"));
		if (!CrowdSim->bEnableLanePathing)
		{
			Ar.Logf(TEXT("레인 경로     : off — NavMesh만 쓴다"));
		}
		else if (LaneCount == 0)
		{
			Ar.Logf(TEXT("레인 경로     : 레벨에 AFF_CrowdLane이 없다 (FF.Crowd.Lanes refresh 로 재수집)"));
		}
		else
		{
			Ar.Logf(TEXT("레인          : %d개 (사용 가능 %d개)"), LaneCount, CrowdSim->GetUsableLaneCount());
			Ar.Logf(TEXT("레인 / NavMesh: %d / %d건 (레인 채택 %.0f%%)  <-- 레인을 깔았는데 0%%면 반경이 안 맞는다"),
				LanePaths, NavFallbacks,
				LaneAttempts > 0 ? (100.f * LanePaths / LaneAttempts) : 0.f);
			Ar.Logf(TEXT("진입 / 이탈   : %.0f / %.0fcm   우회 상한 %.1f배"),
				CrowdSim->LaneEntrySnapRadiusCm, CrowdSim->LaneExitSnapRadiusCm,
				CrowdSim->LaneMaxDetourRatio);

			if (LaneAttempts > 0 && LanePaths == 0)
			{
				Ar.Logf(ELogVerbosity::Warning,
					TEXT("레인이 %d개 있는데 채택이 0건이다 — 개체가 진입 반경 밖이거나 목적지가 이탈 반경 밖이다. "
					     "FF.Crowd.Lanes 로 레인 끝점과 목적지 거리를 볼 것"), LaneCount);
			}
		}
	}
	else
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("경로추종이 꺼져 있다 — 건물 반대편이 목적지면 벽에 붙어 미끄러진다. "
			     "벽 반발은 비키게 할 뿐 돌아가게 하지 못한다"));
	}

	const FFF_CrowdHeightField& Ground = CrowdSim->GetHeightField();
	const FFF_CrowdHeightField::FBuildStats& GroundStats = Ground.GetStats();

	Ar.Logf(TEXT("--- 지형 ---"));
	if (!CrowdSim->bEnableHeightField)
	{
		Ar.Logf(TEXT("높이 격자     : off — 개체 Z가 스폰 값에 고정된다"));
	}
	else if (!Ground.IsValid())
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("높이 격자     : 비었다 (트레이스 %d회 중 적중 0) — 지면을 못 찾았다"),
			GroundStats.SampleCount);
	}
	else
	{
		Ar.Logf(TEXT("높이 격자     : %dx%d, 적중 %d/%d, 간격 %.0fUU"),
			Ground.GetNumX(), Ground.GetNumY(),
			GroundStats.HitCount, GroundStats.SampleCount, CrowdSim->HeightFieldSpacingUU);

		Ar.Logf(TEXT("  지형 기복    : %.0f ~ %.0f (폭 %.0fcm = %.1fm)"),
			GroundStats.MinHeightCm, GroundStats.MaxHeightCm,
			GroundStats.HeightSpanCm(), GroundStats.HeightSpanCm() * 0.01);

		if (GroundStats.HitCount < GroundStats.SampleCount)
		{
			Ar.Logf(TEXT("  구멍         : %d곳 — 그 위를 지나는 개체는 이웃 값으로 보간된다"),
				GroundStats.SampleCount - GroundStats.HitCount);
		}

		if (CrowdSim->MaxWalkableHeightAboveOriginUU > 0.f)
		{
			Ar.Logf(TEXT("  통행 상한    : 원점+%.0fUU (절대 Z %.0f), 막아 되돌린 스텝 %d회, 구멍은 %s"),
				CrowdSim->MaxWalkableHeightAboveOriginUU,
				CrowdSim->SimOrigin.Z + static_cast<double>(CrowdSim->MaxWalkableHeightAboveOriginUU),
				CrowdSim->GetHeightBlockedSteps(),
				CrowdSim->bBlockUnknownGround ? TEXT("통행 불가") : TEXT("통행 가능"));
		}
		else
		{
			Ar.Logf(TEXT("  통행 상한    : off — 지형이 아무리 높아도 올라간다 (FF.Crowd.Tune maxwalkheight <UU>)"));
		}
	}

	Ar.Logf(TEXT("(막힘 판정: 목적지 반경 밖 + 목적지 방향 진척 %.0fUU/s 미만. 회피 on/off와 무관하게 집계된다)"),
		CrowdSim->AvoidanceConfig.StuckSpeedThresholdUU);

	const FFF_CrowdReadModel& ReadModel = CrowdSim->GetCrowdReadModel();
	Ar.Logf(TEXT("스냅샷        : %d회 발행, 유효 셀 %d개, 주기 %d스텝 (셀 %.2fm)"),
		CrowdSim->GetSnapshotCount(), ReadModel.OccupiedCells.Num(),
		CrowdSim->SnapshotIntervalSteps, ReadModel.CellSizeMeters);
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdStatsCommand(
	TEXT("FF.Crowd.Stats"),
	TEXT("현재 군중/혼잡도 수치를 출력한다"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdStatsCommand));

// 레인이 실제로 먹었는지 보는 명령.
//
// 수치만으로는 "레인이 없다"와 "레인이 있는데 반경이 안 맞는다"를 못 가른다. 그래서
// 목록에 시야 위치까지의 거리를 같이 찍고, draw로 폴리라인을 월드에 그려 눈으로 맞춘다.
static void CrowdLanesCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	const FString Mode = Args.Num() > 0 ? Args[0].ToLower() : FString();

	if (Mode == TEXT("refresh"))
	{
		const int32 LaneNum = CrowdSim->RefreshLanesFromWorld();
		Ar.Logf(TEXT("레인 %d개를 다시 구웠다. 살아 있던 경로는 전부 버렸다"), LaneNum);
		return;
	}

	if (Mode == TEXT("off") || Mode == TEXT("on"))
	{
		CrowdSim->bEnableLanePathing = (Mode == TEXT("on"));
		CrowdSim->RefreshLanesFromWorld();
		Ar.Logf(TEXT("레인 경로: %s (경로를 버려 즉시 반영됨)"),
			CrowdSim->bEnableLanePathing ? TEXT("on") : TEXT("off"));
		return;
	}

	if (Mode == TEXT("agents"))
	{
		const int32 Limit = Args.Num() > 1 ? FMath::Max(FCString::Atoi(*Args[1]), 1) : 12;
		const FVector Viewer = FFCrowdConsole::GetViewerLocation(World);

		// 가까운 리더부터 본다. 멀리 있는 개체를 섞어 찍으면 눈으로 맞출 수가 없다
		TArray<TPair<float, int32>> Leaders;
		for (int32 AgentIndex = 0; AgentIndex < CrowdSim->GetAgentPoolSize(); ++AgentIndex)
		{
			if (!CrowdSim->IsAgentActive(AgentIndex)
				|| CrowdSim->GetPartyLeader(AgentIndex) != AgentIndex)
			{
				continue;
			}

			Leaders.Emplace(
				static_cast<float>(FVector::Dist2D(Viewer, CrowdSim->GetAgentPosition(AgentIndex))),
				AgentIndex);
		}

		if (Leaders.IsEmpty())
		{
			Ar.Logf(ELogVerbosity::Warning, TEXT("활성 파티 리더가 없다 — 아직 스폰되지 않았다"));
			return;
		}

		Leaders.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B)
		{
			return A.Key < B.Key;
		});

		Ar.Logf(TEXT("가까운 리더 %d명 (전체 %d명)"), FMath::Min(Limit, Leaders.Num()), Leaders.Num());
		Ar.Logf(TEXT("  Agent  경로원천   진입까지    소비/전체   상태"));

		int32 LaneRiders = 0;
		int32 Approaching = 0;

		for (int32 Rank = 0; Rank < Leaders.Num() && Rank < Limit; ++Rank)
		{
			const int32 AgentIndex = Leaders[Rank].Value;

			FFF_CrowdLaneFollowInfo Info;
			if (!CrowdSim->GetLaneFollowInfo(AgentIndex, Info))
			{
				Ar.Logf(TEXT("  %5d  경로없음   -           -           직선 조향 (목적지가 코앞이거나 쿼리 실패)"),
					AgentIndex);
				continue;
			}

			if (Info.LaneIndex == INDEX_NONE)
			{
				Ar.Logf(TEXT("  %5d  NavMesh    -           %d/%d         레인 미채택"),
					AgentIndex, Info.ConsumedCount, Info.TotalCount);
				continue;
			}

			++LaneRiders;
			if (Info.bApproachingEntry)
			{
				++Approaching;
			}

			Ar.Logf(TEXT("  %5d  레인 %-2d    %7.0fcm   %d/%d         %s"),
				AgentIndex, Info.LaneIndex, Info.DistanceToEntryCm,
				Info.ConsumedCount, Info.TotalCount,
				Info.bApproachingEntry
					? TEXT("진입 전 — 레인까지 직선으로 걷는 중")
					: TEXT("레인 위"));

			Ar.Logf(TEXT("           진입점 (%.0f, %.0f, %.0f)"),
				Info.EntryPointCm.X, Info.EntryPointCm.Y, Info.EntryPointCm.Z);
		}

		Ar.Logf(TEXT("레인 탑승 %d명 (그 중 진입 전 %d명)"), LaneRiders, Approaching);
		Ar.Logf(TEXT("(bDrawDebugAgents 를 켜면 청록=레인 경로, 하늘색=NavMesh 경로, 자홍 기둥=진입점)"));
		return;
	}

	const TArray<FFF_CrowdSplineLane>& Lanes = CrowdSim->GetLanes();

	if (Lanes.IsEmpty())
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("레인이 없다. 레벨에 AFF_CrowdLane을 놓거나 도로를 깐 뒤 "
			     "FF.Crowd.Lanes refresh 를 실행할 것"));
		return;
	}

	const FVector Viewer = FFCrowdConsole::GetViewerLocation(World);
	const int32 AuthoredCount = CrowdSim->GetAuthoredLaneCount();
	const FFF_CrowdRoadLaneBuildStats& RoadStats = CrowdSim->GetRoadLaneStats();

	Ar.Logf(TEXT("레인 %d개 = 저작 %d + 도로 %d (시야 위치 기준 거리)"),
		Lanes.Num(), AuthoredCount, CrowdSim->GetRoadLaneCount());
	Ar.Logf(TEXT("  도로 그래프  : 타일 %d개, 간선 %d개, 교차·막다른 %d개, 고립 %d개, 루프 %d개%s"),
		RoadStats.TileCount, RoadStats.EdgeCount, RoadStats.JunctionCount,
		RoadStats.IsolatedTileCount, RoadStats.ClosedLoopCount,
		RoadStats.bTruncated ? TEXT("  ※ 상한에 걸려 잘림") : TEXT(""));
	Ar.Logf(TEXT("  배회 출처    : 순회 %d회 / 원판 %d회(%s)  (순회 비율이 낮으면 도로가 동선과 멀다)"),
		CrowdSim->GetPatrolWanderCount(), CrowdSim->GetDiscWanderCount(),
		CrowdSim->bEnableDiscWander ? TEXT("켬") : TEXT("끔"));
	Ar.Logf(TEXT("  Zone 봉쇄    : %s   기각 %d회 / 복귀 %d회  (기각이 0이면 봉쇄가 한 번도 안 걸렸다)"),
		CrowdSim->bConfineWanderToZones ? TEXT("켬") : TEXT("끔"),
		CrowdSim->GetWanderZoneRejects(), CrowdSim->GetWanderContainmentNudges());
	Ar.Logf(TEXT("  목적지 없음  : %d회  ← 배회가 제자리를 돌려준 횟수. 원판을 끄면 여기가 오른다"),
		CrowdSim->GetWanderStalls());

	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		const FFF_CrowdSplineLane& Lane = Lanes[LaneIndex];
		const TCHAR* Source = LaneIndex < AuthoredCount ? TEXT("저작") : TEXT("도로");

		FVector Closest = FVector::ZeroVector;
		float ArcCm = 0.f;
		float DistCm = 0.f;
		const bool bProjected = CrowdSplinePath::FindClosestOnLane(Lane, Viewer, Closest, ArcCm, DistCm);

		Ar.Logf(TEXT("  [%d] %s 점 %d개, 길이 %.0fcm(%.0fm), 반폭 %.0fcm, %s%s, 시야까지 %s"),
			LaneIndex, Source, Lane.Points.Num(),
			Lane.TotalLengthCm(), Lane.TotalLengthCm() * 0.01f,
			Lane.HalfWidthCm,
			Lane.bOneWay ? TEXT("일방통행") : TEXT("양방향"),
			Lane.bClosedLoop ? TEXT("/루프") : TEXT(""),
			bProjected ? *FString::Printf(TEXT("%.0fcm"), DistCm) : TEXT("측정 실패"));

		if (bProjected && DistCm > CrowdSim->LaneEntrySnapRadiusCm)
		{
			Ar.Logf(TEXT("       진입 반경(%.0fcm) 밖이다 — 여기 있는 개체는 이 레인을 못 쓴다"),
				CrowdSim->LaneEntrySnapRadiusCm);
		}
	}

	if (Mode != TEXT("draw"))
	{
		Ar.Logf(TEXT("(FF.Crowd.Lanes draw [초] 로 월드에 그린다)"));
		return;
	}

	const float Duration = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 20.f;

	for (const FFF_CrowdSplineLane& Lane : Lanes)
	{
		for (int32 Index = 0; Index + 1 < Lane.Points.Num(); ++Index)
		{
			DrawDebugLine(World, Lane.Points[Index], Lane.Points[Index + 1],
				FColor::Cyan, false, Duration, 0, 12.f);
		}

		for (const FVector& Point : Lane.Points)
		{
			DrawDebugPoint(World, Point, 14.f, FColor::Yellow, false, Duration);
		}
	}

	Ar.Logf(TEXT("레인 %d개를 %.0f초간 그렸다 (청록 선 = 개체가 따라갈 폴리라인)"),
		Lanes.Num(), Duration);
}

// 개체가 얼마나 퍼졌는지 한 줄로 본다. 도로 순회를 켜기 전후로 같은 시각에 찍어 비교하는
// 것이 목적이므로, 판단 근거가 되는 두 값(표준편차·균등도)을 항상 같이 낸다.
static void CrowdSpreadCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	if (Args.Num() > 0)
	{
		CrowdSim->SpreadBucketSizeCm = FMath::Max(FCString::Atof(*Args[0]), 100.f);
	}

	FVector Centroid = FVector::ZeroVector;
	float StdDevCm = 0.f;
	float Evenness = 0.f;
	int32 SampleCount = 0;
	int32 OccupiedBuckets = 0;

	if (!CrowdSim->GetSpreadStats(Centroid, StdDevCm, Evenness, SampleCount, OccupiedBuckets))
	{
		Ar.Logf(ELogVerbosity::Warning, TEXT("활성 개체가 없다 — 아직 스폰되지 않았다"));
		return;
	}

	Ar.Logf(TEXT("개체 %d명, 격자 %.0fm"), SampleCount, CrowdSim->SpreadBucketSizeCm * 0.01f);
	Ar.Logf(TEXT("  중심        : (%.0f, %.0f)"), Centroid.X, Centroid.Y);
	Ar.Logf(TEXT("  표준편차    : %.0fcm (%.0fm)   ← 중심에서 얼마나 멀리 흩어졌나"),
		StdDevCm, StdDevCm * 0.01f);
	Ar.Logf(TEXT("  점유 칸     : %d개"), OccupiedBuckets);
	Ar.Logf(TEXT("  균등도      : %.3f (1에 가까울수록 고르게 퍼짐)   ← 덩어리로 뭉쳤는지"),
		Evenness);
	Ar.Logf(TEXT("  배회 출처   : 순회 %d회 / 원판 %d회"),
		CrowdSim->GetPatrolWanderCount(), CrowdSim->GetDiscWanderCount());
	Ar.Logf(TEXT("  Zone 봉쇄   : %s   기각 %d회 / 복귀 %d회"),
		CrowdSim->bConfineWanderToZones ? TEXT("켬") : TEXT("끔"),
		CrowdSim->GetWanderZoneRejects(), CrowdSim->GetWanderContainmentNudges());
	Ar.Logf(TEXT("  목적지 없음 : %d회"), CrowdSim->GetWanderStalls());
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdSpreadCommand(
	TEXT("FF.Crowd.Spread"),
	TEXT("개체 확산을 잰다 (표준편차·균등도). [격자크기cm] 로 칸 크기를 바꾼다"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdSpreadCommand));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdLanesCommand(
	TEXT("FF.Crowd.Lanes"),
	TEXT("레인 목록/채택 여부를 본다. refresh | draw [초] | agents [명] | on | off"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdLanesCommand));

static void CrowdDumpTraitsCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}
	TMap<FName, int32> CountByArchetype;
	int32 ActiveCount = 0;
	int32 NoneCount = 0;
	float HungerSum = 0.f;
	float HungerMin = 1.f;
	float HungerMax = 0.f;
	float RestroomSum = 0.f;
	int32 CorrelatedCount = 0;
	const int32 PoolSize = CrowdSim->GetAgentPoolSize();
	for (int32 AgentIndex = 0; AgentIndex < PoolSize; ++AgentIndex)
	{
		if (!CrowdSim->IsAgentActive(AgentIndex))
		{
			continue;
		}
		++ActiveCount;
		const FName ArchetypeId = CrowdSim->GetAgentArchetypeID(AgentIndex);
		if (ArchetypeId.IsNone())
		{
			++NoneCount;
		}
		else
		{
			++CountByArchetype.FindOrAdd(ArchetypeId);
		}
		const FFF_NeedState Needs = CrowdSim->GetAgentNeeds(AgentIndex);
		HungerSum += Needs.Hunger;
		HungerMin = FMath::Min(HungerMin, Needs.Hunger);
		HungerMax = FMath::Max(HungerMax, Needs.Hunger);
		RestroomSum += Needs.Restroom;
		if (FMath::IsNearlyEqual(Needs.Hunger, Needs.Restroom, UE_KINDA_SMALL_NUMBER))
		{
			++CorrelatedCount;
		}
	}
	if (ActiveCount == 0)
	{
		Ar.Logf(TEXT("활성 Agent가 없다. FF.Crowd.Demo로 먼저 스폰할 것"));
		return;
	}
	Ar.Logf(TEXT("--- 아키타입 분포 (활성 %d명) ---"), ActiveCount);
	CountByArchetype.KeySort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	for (const TPair<FName, int32>& Pair : CountByArchetype)
	{
		Ar.Logf(TEXT("%-24s : %5d명 (%5.1f%%)"),
			*Pair.Key.ToString(), Pair.Value, 100.f * Pair.Value / ActiveCount);
	}
	Ar.Logf(TEXT("%-24s : %5d명 (%5.1f%%)"), TEXT("(NAME_None)"), NoneCount, 100.f * NoneCount / ActiveCount);
	Ar.Logf(TEXT("--- 초기 Need 분포 ---"));
	Ar.Logf(TEXT("Hunger   평균 %.3f  최소 %.3f  최대 %.3f"),
		HungerSum / ActiveCount, HungerMin, HungerMax);
	Ar.Logf(TEXT("Restroom 평균 %.3f"), RestroomSum / ActiveCount);
	Ar.Logf(TEXT("Hunger == Restroom 인 개체 : %d명 (전원이면 TraitKey를 나누지 않은 것이다)"), CorrelatedCount);
}
static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdDumpTraitsCommand(
	TEXT("FF.Crowd.DumpTraits"),
	TEXT("활성 Agent의 아키타입 분포와 초기 Need 분포를 출력한다"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdDumpTraitsCommand));

namespace FFCrowdConsole
{
	bool MoveViewAbove(UWorld* World, FOutputDevice& Ar, float HeightUU, float PitchDegrees);
}

bool FFCrowdConsole::MoveViewAbove(UWorld* World, FOutputDevice& Ar, float HeightUU, float PitchDegrees)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		Ar.Logf(ELogVerbosity::Error, TEXT("PlayerController를 찾을 수 없습니다"));
		return false;
	}

	const FVector Ground = FFCrowdConsole::SnapToGround(World, FFCrowdConsole::GetViewerLocation(World));
	const FVector ViewLocation = Ground + FVector(0.f, 0.f, HeightUU);

	ACameraActor* ViewActor = World->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(), ViewLocation, FRotator(PitchDegrees, 0.f, 0.f));
	if (!ViewActor)
	{
		Ar.Logf(ELogVerbosity::Error, TEXT("시점용 카메라 스폰 실패"));
		return false;
	}

	PC->SetViewTarget(ViewActor);

	Ar.Logf(TEXT("카메라 위치 %s"), *ViewActor->GetActorLocation().ToCompactString());

	Ar.Logf(TEXT("시점 이동 — 높이 %.0fUU, 피치 %.0f도"), HeightUU, PitchDegrees);
	return true;
}

static void CrowdPartyCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	const bool bEnable = (FFCrowdConsole::ParseIntArg(Args, 0, 1) != 0);
	CrowdSim->bEnablePartyGrouping = bEnable;

	Ar.Logf(TEXT("파티 그룹핑 %s"), bEnable ? TEXT("켬") : TEXT("끔 (전원 1인)"));

	if (CrowdSim->GetActiveAgentCount() > 0)
	{
		Ar.Logf(TEXT("이미 스폰된 %d명은 그대로다. 다음 FF.Crowd.Spawn부터 적용된다"),
			CrowdSim->GetActiveAgentCount());
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdPartyCommand(
	TEXT("FF.Crowd.Party"),
	TEXT("파티 그룹핑을 켜고 끈다(스폰 전에 설정). FF.Crowd.Party [0|1=1]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdPartyCommand));

// 개체 변주가 만드는 두 행동을 런타임에 끄고 켠다. **재스폰이 필요 없다** —
// 두 판정 모두 매 스텝 플래그를 다시 보므로, 같은 세션에서 전/후를 대조할 수 있다.
// (특성값 자체는 스폰 때 확정되므로 여기서 끈다고 값이 사라지지는 않는다)
static void CrowdVariationCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	if (Args.Num() > 0)
	{
		CrowdSim->bEnableQueuePatienceAbandon = (FFCrowdConsole::ParseIntArg(Args, 0, 1) != 0);
	}
	if (Args.Num() > 1)
	{
		CrowdSim->bEnablePeriodicReevaluation = (FFCrowdConsole::ParseIntArg(Args, 1, 1) != 0);
	}

	Ar.Logf(TEXT("--- 개체 변주 행동 ---"));
	Ar.Logf(TEXT("인내심 이탈   : %s   %d회   (wait_threshold_minutes x patience_multiplier)"),
		CrowdSim->bEnableQueuePatienceAbandon ? TEXT("켬") : TEXT("끔"),
		CrowdSim->GetQueuePatienceAbandons());
	Ar.Logf(TEXT("주기 재평가   : %s   %d회   (reevaluate_interval_seconds x reevaluate_interval_multiplier)"),
		CrowdSim->bEnablePeriodicReevaluation ? TEXT("켬") : TEXT("끔"),
		CrowdSim->GetReevaluateRequests());
	Ar.Logf(TEXT("끄면 두 값은 그 자리에 멈춘다(0으로 돌아가지 않는다). 리셋은 FF.Crowd.Queue 쪽 통계 초기화를 쓴다"));

	if (!CrowdSim->IsQueueAdvancing())
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("★ 큐가 진행 중이 아니다 — 인내심 이탈은 큐 진행 판정 뒤에 있어서 절대 발화하지 않는다. ")
			TEXT("FF.Crowd.GoLive 로 Live 로 올리고 FF.Crowd.Queue 를 볼 것"));
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdVariationCommand(
	TEXT("FF.Crowd.Variation"),
	TEXT("개체 변주 행동(인내심 이탈·주기 재평가)을 켜고 끈다. FF.Crowd.Variation [인내심 0|1] [주기 0|1] (인자 없으면 현재 상태)"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdVariationCommand));

static void CrowdDemoCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const int32 Count = FFCrowdConsole::ParseIntArg(Args, 0, 500);
	const float SpawnRadiusUU = FFCrowdConsole::ParseFloatArg(Args, 1, 1500.f);

	if (!FFCrowdConsole::SpawnCrowd(World, Ar, Count, SpawnRadiusUU))
	{
		return;
	}

	if (UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar))
	{
		CrowdSim->bDrawDebugDensity = true;
		CrowdSim->bDrawDebugAgents = false;
	}

	FFCrowdConsole::MoveViewAbove(World, Ar, FMath::Max(SpawnRadiusUU * 2.5f, 2000.f), -88.f);

	Ar.Logf(TEXT("바닥 격자가 혼잡도(LOS)다. 초록=여유 -> 빨강=위험. 수치는 FF.Crowd.Stats"));
	Ar.Logf(TEXT("파티 묶임을 보려면 FF.Crowd.Debug 2 (리더-멤버 선이 파티 색으로 그려진다)"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdDemoCommand(
	TEXT("FF.Crowd.Demo"),
	TEXT("스폰+디버그표시+시점이동을 한 번에. FF.Crowd.Demo [인원=500] [스폰반경UU=1500]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdDemoCommand));

static void CrowdCellsCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	const int32 TopN = FMath::Clamp(FFCrowdConsole::ParseIntArg(Args, 0, 10), 1, 200);
	const FFF_CrowdReadModel& ReadModel = CrowdSim->GetCrowdReadModel();

	if (CrowdSim->GetSnapshotCount() == 0)
	{
		Ar.Logf(TEXT("아직 발행된 스냅샷이 없다. %d스텝(약 %.1f초) 기다릴 것"),
			CrowdSim->SnapshotIntervalSteps, CrowdSim->SnapshotIntervalSteps / 30.f);
		return;
	}

	if (ReadModel.OccupiedCells.Num() == 0)
	{
		Ar.Logf(TEXT("유효 셀이 없다 — 활성 Agent가 없거나 전원이 그리드 밖이다 (FF.Crowd.Stats의 '밀도 집계 인원' 확인)"));
		return;
	}

	TArray<FFF_CrowdCellSample> Sorted = ReadModel.OccupiedCells;
	Sorted.Sort([](const FFF_CrowdCellSample& A, const FFF_CrowdCellSample& B)
	{
		if (A.DensityPerSqm != B.DensityPerSqm) { return A.DensityPerSqm > B.DensityPerSqm; }
		if (A.CellY != B.CellY) { return A.CellY < B.CellY; }
		return A.CellX < B.CellX;
	});

	int32 TotalFlow = 0;
	int32 StalledCells = 0;
	for (const FFF_CrowdCellSample& Cell : Sorted)
	{
		TotalFlow += Cell.Throughput;
		if (Cell.Throughput == 0 && FCrowdDensityGrid::DensityToLOS(Cell.DensityPerSqm) >= ECrowdLOS::D)
		{
			++StalledCells;
		}
	}

	Ar.Logf(TEXT("--- 셀 상위 %d (스냅샷 #%d, 프레임 %d, %.1f분) ---"),
		FMath::Min(TopN, Sorted.Num()), CrowdSim->GetSnapshotCount(),
		ReadModel.SnapshotFrame, ReadModel.SimTimeMinutes);
	Ar.Logf(TEXT("셀 %.2fm (%.2f m^2), 유효 셀 %d개, 구간 %d스텝"),
		ReadModel.CellSizeMeters, ReadModel.CellSizeMeters * ReadModel.CellSizeMeters,
		Sorted.Num(), CrowdSim->SnapshotIntervalSteps);
	Ar.Logf(TEXT("%5s %5s %6s %8s %7s  %s"), TEXT("X"), TEXT("Y"), TEXT("인원"), TEXT("명/m^2"), TEXT("유량"), TEXT("LOS"));

	const UEnum* LOSEnum = StaticEnum<ECrowdLOS>();
	for (int32 i = 0; i < FMath::Min(TopN, Sorted.Num()); ++i)
	{
		const FFF_CrowdCellSample& Cell = Sorted[i];
		const ECrowdLOS LOS = FCrowdDensityGrid::DensityToLOS(Cell.DensityPerSqm);
		const FString LOSName = LOSEnum
			? LOSEnum->GetDisplayNameTextByValue(static_cast<int64>(LOS)).ToString() : TEXT("?");

		Ar.Logf(TEXT("%5d %5d %6d %8.2f %7d  %s%s"),
			Cell.CellX, Cell.CellY, Cell.Occupancy, Cell.DensityPerSqm, Cell.Throughput,
			*LOSName,
			(Cell.Throughput == 0 && LOS >= ECrowdLOS::D) ? TEXT("  <-- 막힘") : TEXT(""));
	}

	Ar.Logf(TEXT("구간 총 유량 : %d회 진입"), TotalFlow);
	Ar.Logf(TEXT("막힌 셀      : %d개 (LOS D+ 인데 진입 0)"), StalledCells);

	if (TotalFlow == 0)
	{
		Ar.Logf(TEXT("총 유량이 0이다 — 전원이 제자리다. 목적지가 배정됐는지(FF.Crowd.Debug 2) 확인할 것"));
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdCellsCommand(
	TEXT("FF.Crowd.Cells"),
	TEXT("스냅샷의 밀도 상위 셀을 인원·유량과 함께 출력한다. FF.Crowd.Cells [개수=10]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdCellsCommand));

static void CrowdAvoidanceCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	if (Args.Num() > 0)
	{
		CrowdSim->bEnableAvoidance = (FFCrowdConsole::ParseIntArg(Args, 0, 0) != 0);
	}

	if (Args.Num() > 1)
	{
		CrowdSim->bAvoidanceDrivesVelocity = (FFCrowdConsole::ParseIntArg(Args, 1, 1) != 0);
	}

	const FFF_CrowdAvoidanceConfig& Config = CrowdSim->AvoidanceConfig;

	FString ConfigError;
	const bool bConfigOk = Config.Validate(ConfigError);

	Ar.Logf(TEXT("--- 회피(사회력) ---"));
	Ar.Logf(TEXT("bEnableAvoidance : %s"), CrowdSim->bEnableAvoidance ? TEXT("켬") : TEXT("끔"));
	Ar.Logf(TEXT("목표 추종        : %s"), CrowdSim->bAvoidanceDrivesVelocity
		? TEXT("구동력 (tau)") : TEXT("지수 평활 (Lerp)"));
	Ar.Logf(TEXT("이웃 반경 / 셀   : %.2fm / %.2fm  (셀 >= 반경이어야 한다)"),
		Config.NeighbourRadiusMeters, Config.BroadphaseCellSizeMeters);
	Ar.Logf(TEXT("MaxNeighbours    : %d"), Config.MaxNeighbours);
	Ar.Logf(TEXT("A / B / 이방성   : %.0fN / %.3fm / %.2f"),
		Config.RepulsionA, Config.RepulsionB, Config.Anisotropy);
	Ar.Logf(TEXT("접촉 강성 / 마찰 : %.0f / %.0f"), Config.ContactStiffness, Config.ContactFriction);
	Ar.Logf(TEXT("가속 상한        : 전체 %.0f / 비접촉 %.1f m/s^2   exp 인자 상한 %.0f"),
		Config.MaxAccelerationMps2, Config.MaxSocialAccelMps2, Config.MaxExponentArg);
	Ar.Logf(TEXT("완화 시간 tau    : %.2fs  (구동력 경로에서만 쓴다)"), Config.RelaxationTau);
	Ar.Logf(TEXT("접선 편향        : %.2f (우측 통행, 0이면 끔 — 구동력 경로에서만 걸린다)"),
		Config.LaneBiasGain);
	Ar.Logf(TEXT("막힘 임계 / 탈출 : %d스텝 / %.1f m/s^2"),
		Config.StuckDetectSteps, Config.UnstuckAccelMps2);

	if (!bConfigOk)
	{
		Ar.Logf(ELogVerbosity::Error, TEXT("계수 모순: %s"), *ConfigError);
	}

	Ar.Logf(TEXT("켜기 전에 FF.Crowd.Stats / FF.Crowd.DumpTraits로 기준선을 먼저 기록할 것 —"));
	Ar.Logf(TEXT("회피를 켠 뒤에는 이전 밀도 수치를 다시 만들 수 없다"));
}

static void CrowdTuneCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	FFF_CrowdAvoidanceConfig& Config = CrowdSim->AvoidanceConfig;

	struct FKnob
	{
		const TCHAR* Name;
		float* Value;
		const TCHAR* Note;
	};

	const FKnob Knobs[] = {
		{ TEXT("opposing"),  &Config.MaxOpposingAccelMps2,    TEXT("역방향 가속 상한 m/s^2. 음수면 끔. 막힘 방지의 핵심") },
		{ TEXT("wallopposing"), &Config.MaxWallOpposingAccelMps2, TEXT("벽에 걸 역방향 상한 m/s^2. 음수면 면제(기본). 켜면 부스에 파고든다") },
		{ TEXT("social"),    &Config.MaxSocialAccelMps2,      TEXT("비접촉 반발 크기 상한 m/s^2. 이웃 전원의 합에 걸린다") },
		{ TEXT("repulsionb"), &Config.RepulsionB,            TEXT("대인 반발 감쇠 거리 m. 뭉침의 1순위 손잡이. 키우면 개인 공간이 넓어진다") },
		{ TEXT("repulsiona"), &Config.RepulsionA,            TEXT("대인 반발 계수 N. 접촉 거리의 세기. 보통 social 클램프에 먹히므로 B를 먼저 볼 것") },
		{ TEXT("anisotropy"), &Config.Anisotropy,            TEXT("0=전방향 균등, 1=전방만. 낮추면 뒤에서 밀리는 것도 피한다") },
		{ TEXT("neighbour"), &Config.NeighbourRadiusMeters,  TEXT("이웃 탐색 반경 m. ★ cell도 같은 값 이상으로 같이 올릴 것") },
		{ TEXT("cell"),      &Config.BroadphaseCellSizeMeters, TEXT("브로드페이즈 셀 한 변 m. neighbour 미만이면 이웃을 놓친다") },
		{ TEXT("lanebias"),  &Config.LaneBiasGain,            TEXT("우측 통행 편향. 0이면 끔. 밀집 군중에서는 악화됨") },
		{ TEXT("friction"),  &Config.ContactFriction,         TEXT("접촉 접선 마찰. 낮추면 겹침·막힘이 같이 늘어난다") },
		{ TEXT("queueslot"), &CrowdSim->QueueSlotArrivalRadiusUU, TEXT("큐 슬롯 도착 반경 UU. 슬롯 간격 90보다 작아야 한다") },
		{ TEXT("approach"),  &CrowdSim->FacilityApproachRadiusUU, TEXT("시설 접근점 도착 반경 UU. 예약 폭 280 이하") },
		{ TEXT("wander"),    &CrowdSim->WanderArrivalRadiusUU,    TEXT("배회 도착 반경 UU") },
		{ TEXT("braking"),   &CrowdSim->BrakingDistanceUU,        TEXT("감속 시작 거리 UU. 0이면 반경 연동 폴백") },
		{ TEXT("cooldown"),  &CrowdSim->DecisionCooldownSeconds,  TEXT("도착 재결정 최소 간격 s. 0이면 끔") },
		{ TEXT("wallcull"),  &Config.WallCullRadiusMeters,     TEXT("벽 계산 컷오프 m. ★ wallrepulsionb를 올리면 같이 올릴 것") },
		{ TEXT("wallrepulsionb"), &Config.WallRepulsionB,     TEXT("벽 반발 감쇠 거리 m. 벽에서 비켜 서는 거리. 0.25면 균형점이 벽에서 56cm") },
		{ TEXT("wallrepulsiona"), &Config.WallRepulsionA,     TEXT("벽 반발 계수 N. 보통 B를 먼저 볼 것") },
		{ TEXT("waypoint"),  &CrowdSim->WaypointAcceptanceRadiusUU, TEXT("웨이포인트 통과 판정 반경 UU. 작으면 맴돌고 크면 코너를 지른다") },
		{ TEXT("repath"),    &CrowdSim->RepathToleranceUU,    TEXT("목적지가 이만큼 움직이면 경로 재계산 UU") },
		{ TEXT("minpath"),   &CrowdSim->MinPathQueryDistanceUU, TEXT("이 거리 미만은 경로를 안 뽑는다 UU") },
		{ TEXT("traceabove"), &CrowdSim->HeightFieldTraceAboveUU, TEXT("높이 격자 트레이스 시작 높이 UU. 지형 최고점보다 높아야 파묻히지 않는다") },
		{ TEXT("cliffstep"), &CrowdSim->HeightFieldCliffStepUU, TEXT("이 높이 차를 넘는 격자 칸은 보간 없이 끊어 올린다 UU. 0이면 항상 보간(비스듬히 떠오름)") },
		{ TEXT("maxwalkheight"), &CrowdSim->MaxWalkableHeightAboveOriginUU, TEXT("SimOrigin.Z 기준 통행 상한 UU. 이보다 높은 지형에는 들어가지 않는다. 0이면 끔") },
	};

	struct FSwitch
	{
		const TCHAR* Name;
		bool* Value;
		const TCHAR* Note;
	};

	const FSwitch Switches[] = {
		{ TEXT("facilitywalls"), &CrowdSim->bAvoidanceUseFacilityWalls,
		  TEXT("시설 footprint를 벽으로. 끄면 부스를 통과한다") },
		{ TEXT("clearance"), &CrowdSim->bAvoidanceFacilityWallsIncludeClearance,
		  TEXT("시설 벽에 여유 링 포함. 켜면 nav 구멍보다 벽이 커져 경로와 힘이 어긋난다") },
		{ TEXT("pathfollow"), &CrowdSim->bEnablePathFollowing,
		  TEXT("NavMesh 경로 우회. 끄면 최종 목적지로 직선 조향(벽에 붙어 미끄러진다)") },
		{ TEXT("blockunknown"), &CrowdSim->bBlockUnknownGround,
		  TEXT("지면을 못 찾은 격자점도 통행 불가로. maxwalkheight가 켜져 있을 때만 의미가 있다") },
		{ TEXT("confinezones"), &CrowdSim->bConfineWanderToZones,
		  TEXT("배회·순회 목적지를 Zone 안으로 제한. 끄면 봉쇄가 NavMesh 통행성 하나만 남는다") },
		{ TEXT("discwander"), &CrowdSim->bEnableDiscWander,
		  TEXT("원판 배회. 끄면 레인 순회만 남는다 — 레인에서 먼 개체는 제자리에 서므로 stall을 같이 볼 것") },
		{ TEXT("patrolwander"), &CrowdSim->bEnableRoadPatrolWander,
		  TEXT("레인 순회 배회. discwander와 함께 끄면 배회가 전멸한다") },
	};

	struct FIntKnob
	{
		const TCHAR* Name;
		int32* Value;
		const TCHAR* Note;
	};

	const FIntKnob IntKnobs[] = {
		{ TEXT("pathbudget"), &CrowdSim->PathQueriesPerStep,
		  TEXT("스텝당 NavMesh 쿼리 상한. Stats의 '경로 대기'가 계속 0이 아니면 올린다") },
	};

	if (Args.Num() >= 2)
	{
		const FString& Name = Args[0];
		const float NewValue = FCString::Atof(*Args[1]);

		for (const FKnob& Knob : Knobs)
		{
			if (Name.Equals(Knob.Name, ESearchCase::IgnoreCase))
			{
				const float Old = *Knob.Value;
				*Knob.Value = NewValue;

				Ar.Logf(TEXT("%s : %.2f -> %.2f"), Knob.Name, Old, NewValue);

				FString ConfigError;
				if (!Config.Validate(ConfigError))
				{
					Ar.Logf(ELogVerbosity::Error, TEXT("계수 모순: %s"), *ConfigError);
				}
				return;
			}
		}

		for (const FSwitch& Sw : Switches)
		{
			if (Name.Equals(Sw.Name, ESearchCase::IgnoreCase))
			{
				const bool bOld = *Sw.Value;
				*Sw.Value = (NewValue != 0.f);

				Ar.Logf(TEXT("%s : %s -> %s"), Sw.Name,
					bOld ? TEXT("on") : TEXT("off"), *Sw.Value ? TEXT("on") : TEXT("off"));

				return;
			}
		}

		for (const FIntKnob& Knob : IntKnobs)
		{
			if (Name.Equals(Knob.Name, ESearchCase::IgnoreCase))
			{
				const int32 Old = *Knob.Value;
				*Knob.Value = FMath::Max(FCString::Atoi(*Args[1]), 1);

				Ar.Logf(TEXT("%s : %d -> %d"), Knob.Name, Old, *Knob.Value);
				return;
			}
		}

		Ar.Logf(ELogVerbosity::Error, TEXT("'%s'는 없는 이름이다. 인자 없이 쳐서 목록을 볼 것"), *Name);
		return;
	}

	Ar.Logf(TEXT("--- 계수 (FF.Crowd.Tune <이름> <값>) ---"));
	for (const FKnob& Knob : Knobs)
	{
		Ar.Logf(TEXT("  %-13s %8.2f   %s"), Knob.Name, *Knob.Value, Knob.Note);
	}
	Ar.Logf(TEXT("--- 정수 ---"));
	for (const FIntKnob& Knob : IntKnobs)
	{
		Ar.Logf(TEXT("  %-13s %8d   %s"), Knob.Name, *Knob.Value, Knob.Note);
	}
	Ar.Logf(TEXT("--- on/off (1 또는 0) ---"));
	for (const FSwitch& Sw : Switches)
	{
		Ar.Logf(TEXT("  %-13s %8s   %s"), Sw.Name,
			*Sw.Value ? TEXT("on") : TEXT("off"), Sw.Note);
	}
	Ar.Logf(TEXT("목표 추종 경로는 FF.Crowd.Avoidance의 두 번째 인자로 바꾼다(구동력 vs Lerp)"));
	Ar.Logf(TEXT("판정은 FF.Crowd.Stats의 '막힘' 비율. 설정을 바꿀 때마다 재스폰할 것"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdTuneCommand(
	TEXT("FF.Crowd.Tune"),
	TEXT("회피 계수·도착 반경·쿨다운을 런타임에 바꾼다. FF.Crowd.Tune [이름] [값] (인자 없으면 목록)"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdTuneCommand));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdAvoidanceCommand(
	TEXT("FF.Crowd.Avoidance"),
	TEXT("회피(사회력) on/off와 계수를 출력한다. FF.Crowd.Avoidance [0|1]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdAvoidanceCommand));

static void CrowdGoLiveCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFF_FestivalStageSubsystem* Stage =
		GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;

	if (!Stage)
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("FestivalStageSubsystem이 없다 — 이 맵에서는 단계 게이팅 자체가 없으므로 대기열은 그냥 돈다"));
		return;
	}

	const EFF_FestivalStage Before = Stage->CurrentStage;
	Stage->RequestTransition(EFF_FestivalStage::Live);
	const EFF_FestivalStage After = Stage->CurrentStage;

	Ar.Logf(TEXT("축제 단계 : %d -> %d"), static_cast<int32>(Before), static_cast<int32>(After));

	if (After != EFF_FestivalStage::Live)
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("Live로 전환되지 않았다 — Stage 시스템이 이 순서를 허용하지 않는다. HUD의 단계 전환을 쓸 것"));
		return;
	}

	Ar.Logf(TEXT("이제 FF.Crowd.Queue를 두 번 쳐서 '큐 시간'이 늘어나는지 확인할 것"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdGoLiveCommand(
	TEXT("FF.Crowd.GoLive"),
	TEXT("축제 단계를 Live로 올린다. 대기열은 Live가 아니면 진행되지 않는다"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdGoLiveCommand));

static void CrowdQueueCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	if (Args.Num() > 0)
	{
		CrowdSim->bEnableQueueJoin = (FFCrowdConsole::ParseIntArg(Args, 0, 1) != 0);
	}

	const int32 Attempts = CrowdSim->GetQueueJoinAttemptCount();
	const int32 Accepted = CrowdSim->GetQueueJoinAcceptedCount();
	const int32 Rejected = CrowdSim->GetQueueJoinRejectedCount();
	const int32 Unresolved = CrowdSim->GetQueueFacilityUnresolvedCount();
	const int32 Timeouts = CrowdSim->GetQueueHoldTimeoutCount();

	Ar.Logf(TEXT("--- 대기열 (Crowd -> Queue) ---"));
	Ar.Logf(TEXT("bEnableQueueJoin : %s"), CrowdSim->bEnableQueueJoin ? TEXT("켬") : TEXT("끔"));
	Ar.Logf(TEXT("가입 시도        : %d   <- 0이면 결정이 시설을 안 고르거나 큐가 등록 안 됐다"), Attempts);
	Ar.Logf(TEXT("  수락           : %d   <- 0이면 전부 만석·폐쇄다"), Accepted);
	Ar.Logf(TEXT("  거절           : %d"), Rejected);
	Ar.Logf(TEXT("시설 미해결      : %d   (GUID 파싱 실패 + 큐 미등록 시설. 배회는 여기 안 들어온다)"), Unresolved);
	Ar.Logf(TEXT("슬롯 도착 통지   : %d"), CrowdSim->GetQueueArrivalNotifyCount());
	Ar.Logf(TEXT("지금 줄에 선 파티: %d"), CrowdSim->GetQueuedPartyCount());
	Ar.Logf(TEXT("인내심 이탈      : %d   (%s) <- 정상 행동이다. 아래 '강제 해제'와 다른 것이니 구분할 것"),
		CrowdSim->GetQueuePatienceAbandons(),
		CrowdSim->bEnableQueuePatienceAbandon ? TEXT("켬") : TEXT("끔"));

	if (!CrowdSim->IsQueueAdvancing())
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("★ 큐가 멈춰 있어 가입을 건너뛰는 중 (%d회). 아래 '대기열 진행'을 볼 것 —"),
			CrowdSim->GetQueueStalledSkipCount());
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("  멈춘 큐에도 줄은 서지지만 서비스가 끝나지 않아 인파가 얼어붙는다"));
	}
	else if (CrowdSim->GetQueueStalledSkipCount() > 0)
	{
		Ar.Logf(TEXT("멈춤 중 건너뜀   : %d   (지금은 진행 중이다)"), CrowdSim->GetQueueStalledSkipCount());
	}

	if (Timeouts > 0)
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("강제 해제        : %d   <- 큐 이벤트가 유실되고 있다. 수치를 올리지 말고 원인을 찾을 것"),
			Timeouts);
	}

	static float LastQueueSimTime = -1.f;
	static float LastWorldTime = -1.f;

	if (const UFF_QueueSubsystem* Queue = World->GetSubsystem<UFF_QueueSubsystem>())
	{
		const float QueueSimTime = Queue->GetSimTimeSeconds();
		const float WorldTime = World->GetTimeSeconds();
		const int32 RegisteredFacilities = Queue->GetAllQueueStates().Num();

		Ar.Logf(TEXT("--- 대기열 진행 ---"));
		Ar.Logf(TEXT("등록 시설        : %d   <- 0이면 Tick이 즉시 반환한다. 줄이 존재하지 않는다"),
			RegisteredFacilities);
		Ar.Logf(TEXT("일시정지         : %s"), Queue->IsPaused() ? TEXT("예 <- 진행 안 함") : TEXT("아니오"));

		if (const UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (const UFF_FestivalStageSubsystem* Stage = GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
			{
				const bool bLive = (Stage->CurrentStage == EFF_FestivalStage::Live);
				Ar.Logf(TEXT("축제 단계        : %s"),
					bLive ? TEXT("Live") : TEXT("Live 아님 <- 대기열이 진행되지 않을 수 있다"));
			}
		}

		Ar.Logf(TEXT("큐 시간          : %.2fs"), QueueSimTime);

		if (LastQueueSimTime >= 0.f && WorldTime > LastWorldTime)
		{
			const float QueueDelta = QueueSimTime - LastQueueSimTime;
			const float WorldDelta = WorldTime - LastWorldTime;

			Ar.Logf(TEXT("직전 조회 이후   : 월드 %.1fs 동안 큐 %.1fs 진행 %s"),
				WorldDelta, QueueDelta,
				(QueueDelta > KINDA_SMALL_NUMBER) ? TEXT("(진행 중)") : TEXT("<- 멈춰 있다"));
		}
		else
		{
			Ar.Logf(TEXT("진행 여부는 이 명령을 몇 초 뒤 한 번 더 쳐서 큐 시간이 늘어나는지로 판정한다"));
		}

		LastQueueSimTime = QueueSimTime;
		LastWorldTime = WorldTime;
	}
	else
	{
		Ar.Logf(ELogVerbosity::Warning, TEXT("QueueSubsystem이 월드에 없다 — 대기열 자체가 존재하지 않는다"));
	}

	if (const UFF_QueueCrowdLink* Link = World->GetSubsystem<UFF_QueueCrowdLink>())
	{
		Ar.Logf(TEXT("--- 대기열 (Queue -> Crowd) ---"));
		Ar.Logf(TEXT("결선             : %s"), Link->IsLinkEnabled() ? TEXT("연결") : TEXT("끊김"));
		Ar.Logf(TEXT("슬롯 목표 적용   : %d   <- 수락은 됐는데 이게 0이면 슬롯이 이동으로 안 넘어간다"),
			Link->GetAppliedSlotTargetCount());
		Ar.Logf(TEXT("인원 불일치      : %d   (파티 인원과 예약 슬롯 수가 어긋난 횟수)"),
			Link->GetMemberCountMismatches());
		Ar.Logf(TEXT("재평가 요청      : %d"), Link->GetDecisionRequests());
	}
	else
	{
		Ar.Logf(ELogVerbosity::Warning,
			TEXT("QueueCrowdLink가 월드에 없다 — 수락돼도 슬롯 좌표가 전달되지 않는다"));
	}

	Ar.Logf(TEXT("--- Need 해소 ---"));
	Ar.Logf(TEXT("해소 적용        : %d회  <- 서비스를 마친 파티가 있는데 0이면 배선이 끊긴 것이다"),
		CrowdSim->GetNeedReliefApplied());
	Ar.Logf(TEXT("해소 건너뜀      : %d회  (배회 목적지 또는 Need를 풀지 않는 카테고리 — 정상)"),
		CrowdSim->GetNeedReliefSkipped());
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdQueueCommand(
	TEXT("FF.Crowd.Queue"),
	TEXT("대기열 가입 배선 상태와 계수를 출력한다. FF.Crowd.Queue [0|1]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdQueueCommand));

static void CrowdArrivalCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	// Inspect the cache used by UpdateArrivalSchedule. Loading the configured soft
	// reference here would revive a collected table and hide the spawning failure.
	const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get();
	const UDataTable* Table = CrowdSim->GetLoadedArrivalCurveTable();
	const FName ScenarioId = Settings ? Settings->ArrivalScenarioId : NAME_None;
	const UGameInstance* DiagnosticGI = World ? World->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* DiagnosticStage = DiagnosticGI
		? DiagnosticGI->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	Ar.Logf(TEXT("[CrowdArrival] Initialized=%d Stage=%s Started=%d TimeScale=%.2f TableLoaded=%d Planned=%d Admitted=%d Active=%d Bound=%d"),
		CrowdSim->IsSimulationInitialized(), DiagnosticStage
			? *UEnum::GetValueAsString(DiagnosticStage->CurrentStage) : TEXT("NoStage"),
		DiagnosticStage ? DiagnosticStage->bFestivalStarted : true,
		DiagnosticStage ? DiagnosticStage->GetEffectiveTimeScale() : CrowdSim->SimulationTimeScale,
		Table != nullptr, CrowdSim->GetArrivalPlannedCount(), CrowdSim->GetArrivalSpawnedCount(),
		CrowdSim->GetActiveAgentCount(), CrowdSim->GetBoundActorCount());

	Ar.Logf(TEXT("--- 입장 곡선 ---"));
	Ar.Logf(TEXT("설정 경로        : %s"), Settings ? *Settings->ArrivalCurveTable.ToString() : TEXT("<미설정>"));
	Ar.Logf(TEXT("실행 중인 표     : %s"), Table ? *Table->GetName() : TEXT("<로드되지 않음>"));
	Ar.Logf(TEXT("설정 ScenarioId  : %s"), *ScenarioId.ToString());
	Ar.Logf(TEXT("곡선 활성        : %s"),
		CrowdSim->IsArrivalCurveActive() ? TEXT("예") : TEXT("아니오 — 일괄 스폰으로 폴백했다"));
	Ar.Logf(TEXT("캐시 버킷 수     : %d   <- 0이면 InitializeSimulation의 _00 행 조회가 실패했다"),
		CrowdSim->GetArrivalBucketCount());
	if (CrowdSim->IsArrivalCurveActive() && !Table)
	{
		Ar.Logf(ELogVerbosity::Error, TEXT("입장 시간표가 메모리에 없다. 입장이 중단된 상태이며 진단 명령은 표를 다시 로드하지 않는다."));
		return;
	}

	if (!CrowdSim->IsArrivalCurveActive())
	{
		// 표가 멀쩡한데 캐시가 비었다면 원인이 좁혀진다 — 대개 ScenarioId 불일치다
		if (Table)
		{
			const FName ProbeRow(*FString::Printf(TEXT("%s_00"), *ScenarioId.ToString()));
			const bool bProbeExists =
				Table->FindRow<FFF_ArrivalCurveRow>(ProbeRow, TEXT("ArrivalConsoleProbe"), false) != nullptr;

			Ar.Logf(ELogVerbosity::Warning,
				TEXT("표는 있는데 곡선이 꺼졌다. 진단 행 '%s' 존재 여부: %s"),
				*ProbeRow.ToString(), bProbeExists ? TEXT("있음") : TEXT("없음"));

			if (bProbeExists)
			{
				Ar.Logf(ELogVerbosity::Warning,
					TEXT("행은 있으나 캐시가 비었다 — InitializeSimulation이 ScenarioId를 캐시하지 않았을 가능성. ")
					TEXT("CachedArrivalScenarioId 대입 여부를 확인할 것"));
			}
			else
			{
				Ar.Logf(ELogVerbosity::Warning,
					TEXT("ScenarioId가 표에 없다. flat_v0 / single_peak_v0 / sunset_peak_v0 중 하나여야 한다"));
			}
		}
		else
		{
			Ar.Logf(ELogVerbosity::Warning,
				TEXT("Project Settings > Game > FestaFiesta Simulation > Arrival 에 DT_VisitorArrivalCurve를 지정할 것"));
		}
		return;
	}

	// 진행도는 서브시스템과 같은 함수로 구한다. 여기서 식을 복제하면 둘이 갈렸을 때
	// 이 커맨드가 거짓말을 하게 되고, 그러면 입장 곡선 진단이 통째로 무의미해진다.
	const int32 BucketCount = CrowdSim->GetArrivalBucketCount();
	const float Progress = CrowdSim->GetLiveProgress01();
	const int32 BucketIdx = FMath::Min(FMath::FloorToInt(Progress * BucketCount), BucketCount - 1);

	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFF_FestivalStageSubsystem* ArrivalStage =
		GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;

	if (ArrivalStage && ArrivalStage->GetLiveDurationForCurrentDay() > 0.f)
	{
		Ar.Logf(TEXT("Live 경과        : %.1f초 / %.1f초  (진행도 %.4f, Stage 시계 기준)"),
			ArrivalStage->ElapsedLiveSeconds, ArrivalStage->GetLiveDurationForCurrentDay(), Progress);
	}
	else
	{
		Ar.Logf(TEXT("Live 경과        : %.1f분 / %.1f분  (진행도 %.4f, Stage 없음 — Crowd 자체 시계로 폴백)"),
			CrowdSim->GetLiveMinutes(),
			FMath::Max(CrowdSim->Temp_LiveStageMinutes, KINDA_SMALL_NUMBER), Progress);
	}

	const FName RowName(*FString::Printf(TEXT("%s_%02d"), *ScenarioId.ToString(), BucketIdx));
	const FFF_ArrivalCurveRow* Row =
		Table ? Table->FindRow<FFF_ArrivalCurveRow>(RowName, TEXT("ArrivalConsole")) : nullptr;

	const int32 Spawned = CrowdSim->GetArrivalSpawnedCount();

	if (!Row)
	{
		Ar.Logf(ELogVerbosity::Error,
			TEXT("현재 버킷 행 '%s'을 찾지 못했다 — 버킷 %d/%d. 표가 중간에 잘렸을 가능성"),
			*RowName.ToString(), BucketIdx, BucketCount);
		Ar.Logf(TEXT("입장 누적        : %d명"), Spawned);
		return;
	}

	// UpdateArrivalSchedule과 같은 식으로 다시 계산한다. 두 값이 벌어지면 보간이나 커서가 어긋난 것이다
	const float Span = FMath::Max(Row->ProgressEnd - Row->ProgressStart, KINDA_SMALL_NUMBER);
	const float LocalT = FMath::Clamp((Progress - Row->ProgressStart) / Span, 0.f, 1.f);
	const float CumPct = (Row->CumulativeSharePct - Row->ArrivalSharePct) + Row->ArrivalSharePct * LocalT;

	Ar.Logf(TEXT("현재 버킷        : %d/%d  '%s'  (기본시간표 %s)"),
		BucketIdx, BucketCount, *RowName.ToString(), *Row->ClockStartAtDefault);
	Ar.Logf(TEXT("버킷 내 진행     : %.3f   (유입 %.4f%%, 누적 %.4f%%)"),
		LocalT, Row->ArrivalSharePct, Row->CumulativeSharePct);
	Ar.Logf(TEXT("보간 누적 목표   : %.4f%%"), CumPct);
	Ar.Logf(TEXT("입장 누적        : %d명   <- 목표는 총원 기준이라 아래 두 줄로 비교할 것"), Spawned);

	// 총원은 서브시스템 내부값(ArrivalTotalVisitorCount)이라 여기서 직접 못 읽는다.
	// 활성 인원으로 역산하면 퇴장분이 섞이므로, 오차 판정은 비율만 제시한다
	if (Spawned > 0 && CumPct > KINDA_SMALL_NUMBER)
	{
		Ar.Logf(TEXT("역산 총원        : 약 %.0f명   (입장누적 / 보간목표비율)"), Spawned * 100.f / CumPct);
	}
	Ar.Logf(TEXT("활성 Agent       : %d명   (입장누적에서 퇴장분이 빠진 값)"), CrowdSim->GetActiveAgentCount());

	if (Progress >= 1.f - KINDA_SMALL_NUMBER)
	{
		Ar.Logf(TEXT("진행도 1.0 도달 — 입장 누적이 총원에서 멈춰 있어야 정상이다"));
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdArrivalCommand(
	TEXT("FF.Crowd.Arrival"),
	TEXT("입장 곡선 진행 상태와 배선을 출력한다. 곡선이 꺼져 있으면 원인까지 좁혀준다"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdArrivalCommand));

static UFF_SatisfactionMetricsSubsystem* GetSatisfaction(UWorld* World, FOutputDevice& Ar)
{
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFF_SatisfactionMetricsSubsystem* Satisfaction =
		GameInstance ? GameInstance->GetSubsystem<UFF_SatisfactionMetricsSubsystem>() : nullptr;
	if (!Satisfaction)
	{
		Ar.Logf(ELogVerbosity::Error, TEXT("SatisfactionMetricsSubsystem을 찾을 수 없습니다 (게임이 실행 중인지 확인)"));
	}
	return Satisfaction;
}

static void SatShowCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const UFF_SatisfactionMetricsSubsystem* Satisfaction = GetSatisfaction(World, Ar);
	if (!Satisfaction)
	{
		return;
	}

	Ar.Logf(TEXT("만족도 %d  (환경 %d = 대기 %d + 밀도 %d / 개인 부합도 %d = 취향 %d + Need %d)"),
		Satisfaction->GetSimSatisfaction(), Satisfaction->GetEnvironmentScore(),
		Satisfaction->GetWaitScore(), Satisfaction->GetDensityScore(),
		Satisfaction->GetPersonalFitScore(), Satisfaction->GetPreferenceScore(),
		Satisfaction->GetNeedFulfillmentScore());
	Ar.Logf(TEXT("평균 감점 %d점, 이번 주기 최소 표본 %d  (50 고정이면 아직 표본이 안 들어온 것)"),
		Satisfaction->GetPenaltyPoints(), Satisfaction->GetSampleCount());
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GSatShowCommand(
	TEXT("FF.Sat.Show"),
	TEXT("현재 만족도와 하위 점수, 평균 감점을 출력한다. 취향 점수가 50에서 움직이는지 볼 때 쓴다"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SatShowCommand));

// 만족도 영역 감점 임시 검증용. 사건 시스템이 붙기 전까지 API 동작을 눈으로 확인한다
static void SatPenaltyCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	const UFF_SatisfactionMetricsSubsystem* Satisfaction = GetSatisfaction(World, Ar);
	if (!CrowdSim || !Satisfaction)
	{
		return;
	}

	const float DropPoints = FFCrowdConsole::ParseFloatArg(Args, 0, 30.f);
	const float RadiusCm = FFCrowdConsole::ParseFloatArg(Args, 1, 1500.f);
	const FVector Center = Args.Num() >= 5
		? FVector(FFCrowdConsole::ParseFloatArg(Args, 2, 0.f), FFCrowdConsole::ParseFloatArg(Args, 3, 0.f),
			FFCrowdConsole::ParseFloatArg(Args, 4, 0.f))
		: FFCrowdConsole::GetViewerLocation(World);

	const int32 Before = Satisfaction->GetSimSatisfaction();
	const int32 Affected = CrowdSim->ApplySatisfactionPenaltyInArea(Center, RadiusCm, DropPoints);

	Ar.Logf(TEXT("감점 적용: 중심 %s 반경 %.0fcm, %d명에게 사람당 %.1f점"),
		*Center.ToCompactString(), RadiusCm, Affected, DropPoints);
	Ar.Logf(TEXT("호출 직전 만족도 %d. 반영은 다음 집계 창(약 1초 뒤)이라 FF.Sat.Show 로 다시 볼 것"), Before);
	Ar.Logf(TEXT("회복 속도는 축제 시간 1분당 %.1f점이라, 시간 배속이 낮으면 회복이 느려 보인다"),
		CrowdSim->SatisfactionPenaltyRecoverPerMinute);
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GSatPenaltyCommand(
	TEXT("FF.Sat.Penalty"),
	TEXT("영역 안 방문객 만족도를 깎는다. FF.Sat.Penalty [점수=30] [반경cm=1500] [X Y Z]  중심을 생략하면 지금 시점 위치"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&SatPenaltyCommand));

#endif // !UE_BUILD_SHIPPING
