// Fill out your copyright notice in the Description page of Project Settings.


#include "Camera/CameraActor.h"
#include "CollisionQueryParams.h"
#include "CrowdSystem/FF_BaseCrowd.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealClient.h"

// 콘솔 커맨드 모음.
//
// 레벨에 AFF_SpawnActor를 놓지 않고도 아무 맵에서 군중을 띄워 볼 수 있게 하는 것이 목적이다.
// 시연 중에 인원을 바꿔 가며 혼잡도가 어떻게 변하는지 보여줄 때도 쓴다.
//
// 배포 빌드에는 들어가지 않는다.

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

		FCollisionQueryParams Params(SCENE_QUERY_STAT(FFCrowdGroundSnap), /*bTraceComplex*/ false);
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			Params.AddIgnoredActor(PC->GetPawn());
		}

		// 시작점을 조금 올리는 이유: 폰이 이미 바닥에 반쯤 박혀 있으면 아래로만 쏴서는 바닥을 놓친다.
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

// -------------------------------------------------------------------------------------------------
// FF.Crowd.Class
// -------------------------------------------------------------------------------------------------

namespace FFCrowdConsole
{
	static FString PendingCrowdClassPath;

	FString BuildClassObjectPath(const FString& InPath)
	{
		FString Path = InPath;
		Path.TrimStartAndEndInline();

		// 콘텐츠 브라우저의 "레퍼런스 복사"는 Blueprint'/Game/...' 형태로 준다. 따옴표와 접두사를 벗긴다.
		Path.RemoveFromStart(TEXT("Blueprint"));
		Path.RemoveFromStart(TEXT("BlueprintGeneratedClass"));
		Path = Path.Replace(TEXT("'"), TEXT("")).Replace(TEXT("\""), TEXT(""));

		if (Path.EndsWith(TEXT("_C")))
		{
			return Path;
		}

		// 패키지 경로만 준 경우(점이 없음) 에셋 이름을 붙여 오브젝트 경로를 만든다.
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

		// AFF_BaseCrowd를 BaseClass로 넘기면 상속 검사까지 해 준다.
		// 엉뚱한 BP를 지정했을 때 스폰 시점이 아니라 여기서 걸러진다.
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

	// 예약하기 전에 실제로 로드되는지 확인한다. 스폰할 때 알게 되면 원인 찾기가 번거롭다.
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

	// 이미 풀이 만들어졌으면 이번 세션에는 반영되지 않는다. 조용히 무시되면 한참 헤맨다.
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

// -------------------------------------------------------------------------------------------------
// FF.Crowd.Spawn
// -------------------------------------------------------------------------------------------------

namespace FFCrowdConsole
{
	bool SpawnCrowd(UWorld* World, FOutputDevice& Ar, int32 Count, float SpawnRadiusUU, float SimRadiusUU)
	{
		UFF_CrowdSimulationSubsystem* CrowdSim = GetCrowdSim(World, Ar);
		if (!CrowdSim)
		{
			return false;
		}

		// 보고 있는 위치를 기준으로 깔아야 즉시 눈에 들어온다. Z는 지면에 맞춘다.
		const FVector Origin = SnapToGround(World, GetViewerLocation(World));

		if (!CrowdSim->IsSimulationInitialized())
		{
			// FF.Crowd.Class로 지정한 BP가 있으면 그것을, 없으면 nullptr을 넘긴다.
			// nullptr이면 메시 없는 AFF_BaseCrowd가 스폰되고 대체 표시용 원기둥이 켜진다.
			const TSubclassOf<AFF_BaseCrowd> CrowdClass = ResolvePendingCrowdClass();

			CrowdSim->InitializeSimulation(Origin, SimRadiusUU, Count, Count, CrowdClass);

			Ar.Logf(TEXT("표시 클래스: %s"),
				CrowdClass ? *CrowdClass->GetName() : TEXT("AFF_BaseCrowd (대체 원기둥) - FF.Crowd.Class로 BP 지정 가능"));
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
	const float SimRadiusUU = FFCrowdConsole::ParseFloatArg(Args, 2, 10000.f);

	if (FFCrowdConsole::SpawnCrowd(World, Ar, Count, SpawnRadiusUU, SimRadiusUU))
	{
		Ar.Logf(TEXT("FF.Crowd.Debug 1 로 밀도 그리드를, 2로 Agent까지 표시합니다"));
		Ar.Logf(TEXT("메시가 붙은 BP로 보려면 스폰 전에 FF.Crowd.Class <BP경로>"));
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdSpawnCommand(
	TEXT("FF.Crowd.Spawn"),
	TEXT("군중을 스폰한다. FF.Crowd.Spawn [인원=500] [스폰반경UU=3000] [시뮬반경UU=10000]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdSpawnCommand));

// -------------------------------------------------------------------------------------------------
// FF.Crowd.Debug
// -------------------------------------------------------------------------------------------------

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

// -------------------------------------------------------------------------------------------------
// FF.Crowd.Stats
// -------------------------------------------------------------------------------------------------

static void CrowdStatsCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	const FCrowdDensityGrid& Grid = CrowdSim->GetDensityGrid();
	const float MaxDensity = Grid.GetMaxDensity();

	// LOS 등급을 글자로. 안전 판정에서 실제로 보고하는 형태다.
	const ECrowdLOS WorstLOS = FCrowdDensityGrid::DensityToLOS(MaxDensity);
	const UEnum* LOSEnum = StaticEnum<ECrowdLOS>();
	const FString LOSName = LOSEnum ? LOSEnum->GetDisplayNameTextByValue(static_cast<int64>(WorstLOS)).ToString() : TEXT("?");

	const int32 ActiveAgents = CrowdSim->GetActiveAgentCount();
	const int32 PartyCount = CrowdSim->GetActivePartyCount();
	const float AveragePartySize = (PartyCount > 0) ? static_cast<float>(ActiveAgents) / PartyCount : 0.f;

	Ar.Logf(TEXT("--- Crowd ---"));
	Ar.Logf(TEXT("활성 Agent    : %d"), ActiveAgents);
	Ar.Logf(TEXT("파티          : %d개 (평균 %.2f명, 그룹핑 %s)"),
		PartyCount, AveragePartySize, CrowdSim->bEnablePartyGrouping ? TEXT("켬") : TEXT("끔"));
	Ar.Logf(TEXT("화면 표시 Actor: %d"), CrowdSim->GetBoundActorCount());
	Ar.Logf(TEXT("밀도 집계 인원 : %d"), Grid.GetSampledUnitCount());
	Ar.Logf(TEXT("그리드        : %d x %d 셀, 셀 %.2fm (%.2f m^2)"),
		Grid.GetNumX(), Grid.GetNumY(), Grid.GetCellSizeMeters(), Grid.GetCellAreaSqM());
	Ar.Logf(TEXT("최대 밀도     : %.2f 명/m^2  (LOS %s)"), MaxDensity, *LOSName);
	Ar.Logf(TEXT("혼잡 셀(LOS D+): %d"), Grid.GetCongestedCellCount());

	// 스냅샷이 0에서 안 올라가면 발행 경로가 안 도는 것이다 — 소비자(UI·결정)가
	// 아무것도 못 읽는 상태이므로 밀도가 정상으로 보여도 지표는 죽어 있다.
	const FFF_CrowdReadModel& ReadModel = CrowdSim->GetCrowdReadModel();
	Ar.Logf(TEXT("스냅샷        : %d회 발행, 유효 셀 %d개, 주기 %d스텝 (셀 %.2fm)"),
		CrowdSim->GetSnapshotCount(), ReadModel.OccupiedCells.Num(),
		CrowdSim->SnapshotIntervalSteps, ReadModel.CellSizeMeters);
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdStatsCommand(
	TEXT("FF.Crowd.Stats"),
	TEXT("현재 군중/혼잡도 수치를 출력한다"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdStatsCommand));

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
// -------------------------------------------------------------------------------------------------
// FF.Crowd.View
// -------------------------------------------------------------------------------------------------

namespace FFCrowdConsole
{
	bool MoveViewAbove(UWorld* World, FOutputDevice& Ar, float HeightUU, float PitchDegrees);
}

static void CrowdViewCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const float HeightUU = FFCrowdConsole::ParseFloatArg(Args, 0, 6000.f);
	const float PitchDegrees = FFCrowdConsole::ParseFloatArg(Args, 1, -70.f);

	FFCrowdConsole::MoveViewAbove(World, Ar, HeightUU, PitchDegrees);
}

bool FFCrowdConsole::MoveViewAbove(UWorld* World, FOutputDevice& Ar, float HeightUU, float PitchDegrees)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		Ar.Logf(ELogVerbosity::Error, TEXT("PlayerController를 찾을 수 없습니다"));
		return false;
	}

	// 높이는 지면 기준이어야 한다. 폰 캡슐 중심에서 재면 맵마다 그림이 달라진다.
	const FVector Ground = FFCrowdConsole::SnapToGround(World, FFCrowdConsole::GetViewerLocation(World));
	const FVector ViewLocation = Ground + FVector(0.f, 0.f, HeightUU);

	// 폰을 직접 올리지 않고 카메라 액터를 만들어 그쪽으로 시점을 옮긴다.
	// 폰을 올리면 중력에 끌려 바로 떨어져서 원하는 그림이 안 나온다.
	//
	// ACameraActor를 쓰는 이유: 맨 AActor는 루트 컴포넌트가 없어서 SpawnActor에 넘긴 위치가 무시되고
	// 월드 원점에 놓인다. 옮길 수 있는 트랜스폼이 없기 때문이다.
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

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdViewCommand(
	TEXT("FF.Crowd.View"),
	TEXT("군중을 내려다보는 시점으로 이동. FF.Crowd.View [높이UU=6000] [피치=-70]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdViewCommand));

// -------------------------------------------------------------------------------------------------
// FF.Crowd.Shot
// -------------------------------------------------------------------------------------------------

static void CrowdShotCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (!World)
	{
		return;
	}

	const float DelaySeconds = FMath::Max(FFCrowdConsole::ParseFloatArg(Args, 0, 5.f), 0.f);

	// -ExecCmds로 넘긴 커맨드는 전부 첫 프레임에 실행된다.
	// 그 시점에 찍으면 전원이 스폰 위치에 서 있는 그림밖에 안 나오므로, 움직인 뒤를 찍기 위해 지연시킨다.
	TWeakObjectPtr<UWorld> WeakWorld(World);

	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld]()
	{
		FScreenshotRequest::RequestScreenshot(/*bShowUI*/ false);
		UE_LOG(LogTemp, Log, TEXT("[CrowdDebug] 스크린샷 요청 — Saved/Screenshots 아래에 저장됩니다"));

		// 그림과 수치를 같은 시점에 남긴다. 나중에 스크린샷만 보고 밀도를 되짚을 수 없기 때문이다.
		if (const UWorld* World = WeakWorld.Get())
		{
			if (const UFF_CrowdSimulationSubsystem* CrowdSim = World->GetSubsystem<UFF_CrowdSimulationSubsystem>())
			{
				const FCrowdDensityGrid& Grid = CrowdSim->GetDensityGrid();
				UE_LOG(LogTemp, Log,
					TEXT("[CrowdDebug] 활성 %d / 표시 %d / 집계 %d | 최대밀도 %.2f 명/m^2 | 혼잡셀 %d"),
					CrowdSim->GetActiveAgentCount(), CrowdSim->GetBoundActorCount(),
					Grid.GetSampledUnitCount(), Grid.GetMaxDensity(), Grid.GetCongestedCellCount());
			}
		}
	}), DelaySeconds, /*bLoop*/ false);

	Ar.Logf(TEXT("%.1f초 후 스크린샷을 찍습니다"), DelaySeconds);
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdShotCommand(
	TEXT("FF.Crowd.Shot"),
	TEXT("지정한 초 뒤에 스크린샷을 찍는다. FF.Crowd.Shot [지연초=5]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdShotCommand));

// -------------------------------------------------------------------------------------------------
// FF.Crowd.Party
// -------------------------------------------------------------------------------------------------

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

	// 이미 스폰된 Agent의 소속은 바뀌지 않는다. 파티 구성은 스폰 시점에 결정되기 때문이다.
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

// -------------------------------------------------------------------------------------------------
// FF.Crowd.MeshYaw
// -------------------------------------------------------------------------------------------------

// 메시 정면 축은 에셋마다 다르고 문서로 확인하기 어렵다.
// 빌드를 다시 하며 값을 맞추는 대신, PIE 중에 돌려 보고 맞는 값을 찾는 쪽이 빠르다.
static void CrowdMeshYawCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	if (Args.Num() == 0)
	{
		for (const AFF_BaseCrowd* Actor : CrowdSim->CrowdPool)
		{
			if (IsValid(Actor))
			{
				Ar.Logf(TEXT("현재 MeshYawOffset: %.0f도"), Actor->MeshYawOffset);
				break;
			}
		}
		Ar.Logf(TEXT("사용법: FF.Crowd.MeshYaw [각도]  — 마네킹 계열은 보통 -90"));
		return;
	}

	const float Yaw = FFCrowdConsole::ParseFloatArg(Args, 0, -90.f);

	int32 Applied = 0;
	for (AFF_BaseCrowd* Actor : CrowdSim->CrowdPool)
	{
		if (IsValid(Actor))
		{
			Actor->MeshYawOffset = Yaw;
			++Applied;
		}
	}

	Ar.Logf(TEXT("MeshYawOffset = %.0f도, Actor %d개에 적용 (다음 프레임부터 보입니다)"), Yaw, Applied);

	// 런타임 값이다. 이 커맨드는 이미 스폰된 풀 액터만 바꾼다.
	Ar.Logf(TEXT("맞는 값을 찾았으면 BP_Crowd 클래스 디폴트의 Mesh Yaw Offset에 넣어야 영구 적용된다"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdMeshYawCommand(
	TEXT("FF.Crowd.MeshYaw"),
	TEXT("메시 정면 보정 각도를 실시간으로 바꾼다. FF.Crowd.MeshYaw [각도=-90]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdMeshYawCommand));

// -------------------------------------------------------------------------------------------------
// FF.Crowd.Demo
// -------------------------------------------------------------------------------------------------

// 빌드한 사람이 처음 확인할 때 쓰는 커맨드.
// Spawn -> Debug -> View 세 개를 순서대로 치는 것과 같지만, 하나라도 빠뜨리면
// "아무것도 안 보인다"로 끝나기 때문에 한 줄로 묶어 둔다.
static void CrowdDemoCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	const int32 Count = FFCrowdConsole::ParseIntArg(Args, 0, 500);
	const float SpawnRadiusUU = FFCrowdConsole::ParseFloatArg(Args, 1, 1500.f);

	// 시뮬 반경은 스폰 반경의 4배. 좁으면 목적지가 몰려 인위적인 혼잡이 생긴다.
	if (!FFCrowdConsole::SpawnCrowd(World, Ar, Count, SpawnRadiusUU, SpawnRadiusUU * 4.f))
	{
		return;
	}

	if (UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar))
	{
		CrowdSim->bDrawDebugDensity = true;
		CrowdSim->bDrawDebugAgents = false; // Debug 2는 매우 비싸다. 기본은 끈다
	}

	// 스폰 반경 전체가 화면에 들어오는 높이. 피치 -88은 거의 수직으로 내려다보는 각이다.
	FFCrowdConsole::MoveViewAbove(World, Ar, FMath::Max(SpawnRadiusUU * 2.5f, 2000.f), -88.f);

	Ar.Logf(TEXT("바닥 격자가 혼잡도(LOS)다. 초록=여유 -> 빨강=위험. 수치는 FF.Crowd.Stats"));
	Ar.Logf(TEXT("파티 묶임을 보려면 FF.Crowd.Debug 2 (리더-멤버 선이 파티 색으로 그려진다)"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdDemoCommand(
	TEXT("FF.Crowd.Demo"),
	TEXT("스폰+디버그표시+시점이동을 한 번에. FF.Crowd.Demo [인원=500] [스폰반경UU=1500]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdDemoCommand));

// -------------------------------------------------------------------------------------------------
// FF.Crowd.Cells
// -------------------------------------------------------------------------------------------------

// 스냅샷에서 밀도 상위 N개 셀을 인원·유량과 함께 찍는다.
//
// 이 명령의 목적은 밀도 하나로는 구분할 수 없는 두 상황을 가르는 것이다.
//   인원 많음 + 유량 0    → 막혔다 (게이트 병목)
//   인원 많음 + 유량 높음 → 흐르는 중 (그냥 붐빔)
// "몇 명이 몰리는가"에서 "얼마나 막히는가"로 넘어가는 지점이 여기다.
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

	// 사본을 정렬한다. 스냅샷은 소비자가 읽는 값이므로 순서를 건드리지 않는다.
	TArray<FFF_CrowdCellSample> Sorted = ReadModel.OccupiedCells;
	Sorted.Sort([](const FFF_CrowdCellSample& A, const FFF_CrowdCellSample& B)
	{
		// 밀도 동률이면 셀 좌표로 가른다 — 동률에서 순서가 흔들리면
		// 같은 상황을 두 번 찍었을 때 목록이 달라 보인다.
		if (A.DensityPerSqm != B.DensityPerSqm) { return A.DensityPerSqm > B.DensityPerSqm; }
		if (A.CellY != B.CellY) { return A.CellY < B.CellY; }
		return A.CellX < B.CellX;
	});

	int32 TotalFlow = 0;
	int32 StalledCells = 0;
	for (const FFF_CrowdCellSample& Cell : Sorted)
	{
		TotalFlow += Cell.Throughput;
		// LOS D 이상인데 아무도 들어오지 않은 칸 = 막힌 칸
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

// -------------------------------------------------------------------------------------------------
// FF.Crowd.Avoidance
// -------------------------------------------------------------------------------------------------

// 회피 on/off를 런타임에 뒤집는다. 이 커맨드가 필요한 이유는 A/B 때문이다 —
// 회피를 켜면 밀도·LOS·퇴장 타이밍이 전부 바뀌므로, 같은 실행 안에서 껐다 켜며
// "무엇이 얼마나 변했는가"를 보여줄 수 있어야 한다.
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

	const FFF_CrowdAvoidanceConfig& Config = CrowdSim->AvoidanceConfig;

	FString ConfigError;
	const bool bConfigOk = Config.Validate(ConfigError);

	Ar.Logf(TEXT("--- 회피(사회력) ---"));
	Ar.Logf(TEXT("bEnableAvoidance : %s"), CrowdSim->bEnableAvoidance ? TEXT("켬") : TEXT("끔"));
	Ar.Logf(TEXT("경계 4벽         : %s"), CrowdSim->bAvoidanceUseBoundaryWalls ? TEXT("켬") : TEXT("끔"));
	Ar.Logf(TEXT("이웃 반경 / 셀   : %.2fm / %.2fm  (셀 >= 반경이어야 한다)"),
		Config.NeighbourRadiusMeters, Config.BroadphaseCellSizeMeters);
	Ar.Logf(TEXT("MaxNeighbours    : %d"), Config.MaxNeighbours);
	Ar.Logf(TEXT("A / B / 이방성   : %.0fN / %.3fm / %.2f"),
		Config.RepulsionA, Config.RepulsionB, Config.Anisotropy);
	Ar.Logf(TEXT("접촉 강성 / 마찰 : %.0f / %.0f"), Config.ContactStiffness, Config.ContactFriction);
	Ar.Logf(TEXT("가속 상한        : %.0f m/s^2   exp 인자 상한 %.0f"),
		Config.MaxAccelerationMps2, Config.MaxExponentArg);
	Ar.Logf(TEXT("막힘 임계 / 탈출 : %d스텝 / %.1f m/s^2"),
		Config.StuckDetectSteps, Config.UnstuckAccelMps2);

	if (!bConfigOk)
	{
		Ar.Logf(ELogVerbosity::Error, TEXT("계수 모순: %s"), *ConfigError);
	}

	Ar.Logf(TEXT("켜기 전에 FF.Crowd.Stats / FF.Crowd.DumpTraits로 기준선을 먼저 기록할 것 —"));
	Ar.Logf(TEXT("회피를 켠 뒤에는 이전 밀도 수치를 다시 만들 수 없다"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdAvoidanceCommand(
	TEXT("FF.Crowd.Avoidance"),
	TEXT("회피(사회력) on/off와 계수를 출력한다. FF.Crowd.Avoidance [0|1]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdAvoidanceCommand));

// -------------------------------------------------------------------------------------------------
// FF.Crowd.DumpNeighbours
// -------------------------------------------------------------------------------------------------

// "격자가 실제로 이웃을 찾는가"를 힘을 켜기 전에 확인하는 커맨드.
// 전원 이웃 0이면 격자가 무효이거나 셀 크기가 어긋난 것이고, 그 상태로 힘을 켜면
// 아무 일도 일어나지 않는데 원인은 힘 계수를 아무리 봐도 안 나온다.
static void CrowdDumpNeighboursCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdSimulationSubsystem* CrowdSim = FFCrowdConsole::GetCrowdSim(World, Ar);
	if (!CrowdSim)
	{
		return;
	}

	TArray<int32> Counts;
	const int32 GridItems = CrowdSim->DebugProbeAvoidanceNeighbours(Counts);

	int32 ActiveCount = 0;
	int32 Total = 0;
	int32 MaxCount = 0;
	int32 ZeroCount = 0;
	TArray<int32> Histogram;
	Histogram.SetNumZeroed(CrowdSim->AvoidanceConfig.MaxNeighbours + 2);

	for (int32 AgentIndex = 0; AgentIndex < Counts.Num(); ++AgentIndex)
	{
		if (!CrowdSim->IsAgentActive(AgentIndex))
		{
			continue;
		}
		const int32 Count = Counts[AgentIndex];
		++ActiveCount;
		Total += Count;
		MaxCount = FMath::Max(MaxCount, Count);
		if (Count == 0)
		{
			++ZeroCount;
		}
		Histogram[FMath::Clamp(Count, 0, Histogram.Num() - 1)] += 1;
	}

	if (ActiveCount == 0)
	{
		Ar.Logf(TEXT("활성 Agent가 없다. FF.Crowd.Demo로 먼저 스폰할 것"));
		return;
	}

	Ar.Logf(TEXT("--- 이웃 분포 (활성 %d명, 반경 %.2fm, 상한 %d) ---"),
		ActiveCount, CrowdSim->AvoidanceConfig.NeighbourRadiusMeters, CrowdSim->AvoidanceConfig.MaxNeighbours);
	Ar.Logf(TEXT("격자에 들어간 개체 : %d (활성 수와 다르면 위치에 NaN이 섞인 것이다)"), GridItems);
	Ar.Logf(TEXT("평균 %.2f명  최대 %d명  이웃 0명 %d명(%.1f%%)"),
		static_cast<float>(Total) / ActiveCount, MaxCount, ZeroCount, 100.f * ZeroCount / ActiveCount);

	for (int32 Bucket = 0; Bucket < Histogram.Num(); ++Bucket)
	{
		if (Histogram[Bucket] == 0)
		{
			continue;
		}
		const bool bClamped = (Bucket == Histogram.Num() - 1);
		Ar.Logf(TEXT("이웃 %2d%s : %5d명"), Bucket, bClamped ? TEXT("+") : TEXT(" "), Histogram[Bucket]);
	}

	// 상한에 붙어 있는 개체가 많으면 MaxNeighbours가 실제 밀도를 잘라내고 있다는 뜻이다.
	Ar.Logf(TEXT("상한에 걸린 개체가 많으면 MaxNeighbours가 반발력을 잘라내고 있다 — 밀도 해석 시 주의"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdDumpNeighboursCommand(
	TEXT("FF.Crowd.DumpNeighbours"),
	TEXT("격자가 찾는 이웃 수 분포를 출력한다(회피 on/off와 무관)"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdDumpNeighboursCommand));

#endif // !UE_BUILD_SHIPPING
