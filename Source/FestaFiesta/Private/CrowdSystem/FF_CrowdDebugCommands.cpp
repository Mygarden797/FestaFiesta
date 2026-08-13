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
	Ar.Logf(TEXT("그리드        : %d x %d 셀"), Grid.GetNumX(), Grid.GetNumY());
	Ar.Logf(TEXT("최대 밀도     : %.2f 명/m^2  (LOS %s)"), MaxDensity, *LOSName);
	Ar.Logf(TEXT("혼잡 셀(LOS D+): %d"), Grid.GetCongestedCellCount());
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GCrowdStatsCommand(
	TEXT("FF.Crowd.Stats"),
	TEXT("현재 군중/혼잡도 수치를 출력한다"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&CrowdStatsCommand));

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

#endif // !UE_BUILD_SHIPPING
