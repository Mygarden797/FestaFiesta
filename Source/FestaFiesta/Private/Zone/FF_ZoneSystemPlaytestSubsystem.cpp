#include "Zone/FF_ZoneSystemPlaytestSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Zone/FF_ZoneSystemPlaytestOverviewPawn.h"
#include "Zone/FF_ZoneSystemPlaytestHUD.h"

// 이 파일은 ZoneTestMap에만 런타임 테스트 환경을 덧붙인다.
// 맵과 프로젝트 기본 설정은 수정하지 않고 WorldSubsystem의 수명 안에서만 임시 객체를 관리한다.
DEFINE_LOG_CATEGORY_STATIC(LogFFZonePlaytest, Log, All);

const TCHAR* UFF_ZoneSystemPlaytestSubsystem::TargetMapPackage =
    TEXT("/Game/FestaFiesta/Zone/Maps/ZoneTestMap");
const FName UFF_ZoneSystemPlaytestSubsystem::DefaultZoneId(TEXT("GZ_DadaPocha"));

namespace
{
    constexpr float PreviewRefreshIntervalSeconds = 0.1f;
    constexpr float TestZoneSizeCm = 10000.0f;
    constexpr float TestFloorScale = 100.0f;
    constexpr int32 PlacementSnapCm = 10;
    constexpr int32 BuiltInPresetCount = 7;

    // 전체 Navigation 검사는 개발자 설정의 전역 값을 사용하므로 명령 실행 구간에서만 임시 변경하고
    // 소멸 시 반드시 원래 값으로 되돌리는 RAII 보호 객체를 사용한다.
    class FScopedNavigationValidation
    {
    public:
        explicit FScopedNavigationValidation(const bool bEnable)
            : Settings(GetMutableDefault<UFFZSDeveloperSettings>())
            , bPreviousValue(Settings ? Settings->bEnableNavigationProjectionChecks : false)
        {
            if (Settings)
            {
                Settings->bEnableNavigationProjectionChecks = bEnable;
            }
        }

        ~FScopedNavigationValidation()
        {
            if (Settings)
            {
                Settings->bEnableNavigationProjectionChecks = bPreviousValue;
            }
        }

    private:
        UFFZSDeveloperSettings* Settings = nullptr;
        bool bPreviousValue = false;
    };

    FString EnumName(const UEnum* Enum, const int64 Value)
    {
        return Enum ? Enum->GetNameStringByValue(Value) : TEXT("Unknown");
    }
}

UFF_ZoneSystemPlaytestSubsystem::UFF_ZoneSystemPlaytestSubsystem()
{
    // 네이티브 문자열 참조이므로 패키징 설정의 Always Cook 경로와 함께 유지해야 한다.
    HudWidgetClass = TSoftClassPtr<UFF_ZoneSystemPlaytestHUD>(FSoftObjectPath(
        TEXT("/Game/FestaFiesta/Zone/UI/WBP_ZoneSystemPlaytestHUD.WBP_ZoneSystemPlaytestHUD_C")));
}

// PIE가 붙이는 UEDPIE 접두사를 제거한 실제 패키지명이 ZoneTestMap과 같을 때만 생성한다.
bool UFF_ZoneSystemPlaytestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!Super::ShouldCreateSubsystem(Outer))
    {
        return false;
    }

    const UWorld* World = Cast<UWorld>(Outer);
    if (!World || !DoesSupportWorldType(World->WorldType))
    {
        return false;
    }

    const FString NormalizedPackageName = UWorld::RemovePIEPrefix(World->GetPackage()->GetName());
    return NormalizedPackageName == TargetMapPackage;
}

void UFF_ZoneSystemPlaytestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UFFZSWorldSubsystem>();
    Super::Initialize(Collection);
}

// 월드가 닫힐 때 WBP와 Transient 참조를 정리해 다음 맵으로 입력 상태가 새지 않게 한다.
void UFF_ZoneSystemPlaytestSubsystem::Deinitialize()
{
    if (HudWidget)
    {
        HudWidget->RemoveFromParent();
    }

    HudWidget = nullptr;
    PlaytestPawn = nullptr;
    TestZone = nullptr;
    TestProtectedArea = nullptr;
    PlacementPreview = nullptr;
    RuntimeFloor = nullptr;
    bEnvironmentReady = false;
    Super::Deinitialize();
}

bool UFF_ZoneSystemPlaytestSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::PIE ||
        WorldType == EWorldType::Game ||
        WorldType == EWorldType::GamePreview;
}

// Zone 환경을 먼저 만든 뒤 초기 스냅샷을 저장한다. 이 스냅샷은 R 키 전체 초기화의 기준이다.
void UFF_ZoneSystemPlaytestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    bEnvironmentReady = BuildTestEnvironment(InWorld);
    if (!bEnvironmentReady)
    {
        SetStatus(TEXT("FF Zone System playtest environment failed to initialize."), FColor::Red);
        return;
    }

    InitialSnapshot = GetZoneSystem()->CaptureWorldSnapshot();
    SetStatus(TEXT("Overview ready. Pan with WASD, zoom with the wheel, and press Tab for HUD controls."), FColor::Green);

    bExperienceSmokeRequested = FParse::Param(FCommandLine::Get(), TEXT("FFZSPlaytestExperienceSmoke"));
    if (FParse::Param(FCommandLine::Get(), TEXT("FFZSPlaytestSmoke")))
    {
        RunCommandLineSmokeTest();
    }
}

TStatId UFF_ZoneSystemPlaytestSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_ZoneSystemPlaytestSubsystem, STATGROUP_Tickables);
}

// 매 Tick에는 플레이어 경험과 입력을 보장하고, 비용이 있는 배치 검증은 0.1초 간격으로 갱신한다.
void UFF_ZoneSystemPlaytestSubsystem::Tick(const float DeltaTime)
{
    if (bSmokeExitPending)
    {
        bSmokeExitPending = false;
        FPlatformMisc::RequestExitWithStatus(false, SmokeExitCode);
        return;
    }

    if (!bEnvironmentReady)
    {
        return;
    }

    EnsurePlayerExperience();
    HandleInput();

    PreviewRefreshElapsed += DeltaTime;
    if (PreviewRefreshElapsed >= PreviewRefreshIntervalSeconds)
    {
        PreviewRefreshElapsed = 0.0f;
        UpdatePlacementFromPlayer();
        RefreshPreview();
    }

    if (bShowWorldDebug)
    {
        DrawWorldDebug();
    }
    if (!HudWidget)
    {
        DrawHud();
    }
}

// 시뮬레이션 객체 대신 WBP가 바로 그릴 수 있는 텍스트, 색상과 버튼 조건만 복사한다.
FFF_ZoneSystemPlaytestHudState UFF_ZoneSystemPlaytestSubsystem::GetHudState() const
{
    FFF_ZoneSystemPlaytestHudState State;
    const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    const FFFZSReadModel ReadModel = ZoneSystem ? ZoneSystem->BuildReadModel() : FFFZSReadModel{};
    const FFFZSFacilitySpec Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(SelectedPreset);

    State.ZoneText = FText::FromString(FString::Printf(
        TEXT("ZONE  %s    REV  %lld    FACILITIES  %d"),
        *ActiveZoneId.ToString(), ReadModel.WorldRevision, ReadModel.Facilities.Num()));
    State.PresetText = FText::FromString(FString::Printf(
        TEXT("PRESET  %s"), *Spec.DisplayName.ToString()));
    State.PlacementText = FText::FromString(FString::Printf(
        TEXT("TARGET  X %d  Y %d    ROT %d DEG    %s"),
        CurrentPlacement.OriginCell.X,
        CurrentPlacement.OriginCell.Y,
        CurrentPlacement.QuarterTurns * 90,
        bUiInteractionMode ? TEXT("UI MODE") : TEXT("OVERVIEW MODE")));
    State.ValidationText = !bHasPlacementTarget
        ? FText::FromString(TEXT("NO GROUND TARGET"))
        : LastValidation.bIsValid
            ? FText::FromString(TEXT("PLACEMENT VALID"))
            : FText::FromString(FString::Printf(TEXT("INVALID  %s"), *LastValidation.Summary.ToString()));
    State.StatusText = FText::FromString(StatusMessage);
    State.HelpText = FText::FromString(
        TEXT("WASD / Arrows  Pan  |  Shift  Fast Pan  |  Mouse Wheel  Zoom\n")
        TEXT("RMB Drag  Rotate View  |  Home  Recenter  |  Center Marker  Placement Target\n")
        TEXT("LMB / I  Install  |  Q/E  Rotate Facility  |  1-7  Preset  |  Tab  HUD Controls"));
    State.ValidationColor = bHasPlacementTarget && LastValidation.bIsValid
        ? FLinearColor(0.20f, 0.95f, 0.58f, 1.0f)
        : FLinearColor(1.0f, 0.25f, 0.25f, 1.0f);
    State.StatusColor = FLinearColor(StatusColor);
    State.bHasSelectedFacility = GetLastFacility() != nullptr;
    State.bHasSavedSnapshot = bHasSavedSnapshot;
    State.bHasPlacementTarget = bHasPlacementTarget;
    State.bCanCommitPlacement = bHasPlacementTarget && LastValidation.bIsValid;
    State.bShowHelp = bShowHelp;
    State.bUiInteractionMode = bUiInteractionMode;
    State.bShowWorldDebug = bShowWorldDebug;
    return State;
}

// 의존 순서가 있는 Zone → 바닥 → 보호 구역 → 프리뷰 순으로 테스트 환경을 구성한다.
bool UFF_ZoneSystemPlaytestSubsystem::BuildTestEnvironment(UWorld& World)
{
    UFFZSWorldSubsystem* ZoneSystem = World.GetSubsystem<UFFZSWorldSubsystem>();
    if (!ZoneSystem)
    {
        return false;
    }

    TestZone = FindOrCreateTestZone(World, *ZoneSystem);
    if (!TestZone)
    {
        return false;
    }

    ActiveZoneId = TestZone->ZoneId;
    EnsureTestFloor(World, *TestZone);
    CreateProtectedArea(World, *ZoneSystem);
    CreatePlacementPreview(World);
    if (!PlacementPreview || !TestProtectedArea)
    {
        return false;
    }

    CurrentPlacement.OriginCell = FIntPoint(1000, 1000);
    CurrentPlacement.QuarterTurns = 0;
    bHasPlacementTarget = true;
    RebuildPreviewCommand();
    RefreshPreview();
    return true;
}

// 이미 등록된 기본 Zone, 맵에 배치된 유효 Zone, 빈 ID Zone을 차례로 재사용하고
// 모두 없을 때만 가장 큰 바닥을 기준으로 Transient 테스트 Zone을 만든다.
AFFZSGameplayZone* UFF_ZoneSystemPlaytestSubsystem::FindOrCreateTestZone(
    UWorld& World,
    UFFZSWorldSubsystem& ZoneSystem)
{
    if (AFFZSGameplayZone* RegisteredZone = ZoneSystem.FindZone(DefaultZoneId))
    {
        bOwnsTestZone = false;
        return RegisteredZone;
    }

    AFFZSGameplayZone* EmptyIdCandidate = nullptr;
    for (TActorIterator<AFFZSGameplayZone> It(&World); It; ++It)
    {
        AFFZSGameplayZone* Zone = *It;
        if (!Zone)
        {
            continue;
        }

        if (Zone->ZoneId.IsNone())
        {
            EmptyIdCandidate = EmptyIdCandidate ? EmptyIdCandidate : Zone;
            continue;
        }

        TArray<FText> Errors;
        if (Zone->ValidateZoneData(Errors) && ZoneSystem.RegisterZone(Zone))
        {
            bOwnsTestZone = false;
            return Zone;
        }
    }

    if (EmptyIdCandidate)
    {
        EmptyIdCandidate->ZoneId = DefaultZoneId;
        EmptyIdCandidate->DisplayName = FText::FromString(TEXT("Zone System Playtest"));
        TArray<FText> Errors;
        if (EmptyIdCandidate->ValidateZoneData(Errors) && ZoneSystem.RegisterZone(EmptyIdCandidate))
        {
            bOwnsTestZone = false;
            return EmptyIdCandidate;
        }
    }

    FVector Center = FVector::ZeroVector;
    float GroundZ = 0.0f;
    double LargestGroundArea = 0.0;
    for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
    {
        const UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
        if (!Mesh || !Mesh->GetStaticMesh())
        {
            continue;
        }

        const FVector Extent = Mesh->Bounds.BoxExtent;
        const double Area = static_cast<double>(Extent.X) * Extent.Y;
        if (Area > LargestGroundArea)
        {
            LargestGroundArea = Area;
            Center = Mesh->Bounds.Origin;
            GroundZ = Mesh->Bounds.Origin.Z + Mesh->Bounds.BoxExtent.Z;
        }
    }

    if (LargestGroundArea <= 0.0)
    {
        for (TActorIterator<APlayerStart> It(&World); It; ++It)
        {
            Center = It->GetActorLocation();
            GroundZ = 0.0f;
            break;
        }
    }

    const FVector ZoneOrigin(
        FMath::GridSnap(Center.X - TestZoneSizeCm * 0.5f, 1.0f),
        FMath::GridSnap(Center.Y - TestZoneSizeCm * 0.5f, 1.0f),
        FMath::GridSnap(GroundZ, 1.0f));

    AFFZSGameplayZone* NewZone = World.SpawnActorDeferred<AFFZSGameplayZone>(
        AFFZSGameplayZone::StaticClass(),
        FTransform(FRotator::ZeroRotator, ZoneOrigin),
        nullptr,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!NewZone)
    {
        return nullptr;
    }

    NewZone->SetFlags(RF_Transient);
    NewZone->ZoneId = DefaultZoneId;
    NewZone->DisplayName = FText::FromString(TEXT("Zone System Playtest"));
    NewZone->SoftCapacityOverride = 100;
    UGameplayStatics::FinishSpawningActor(NewZone, NewZone->GetActorTransform());

    bOwnsTestZone = true;
    return ZoneSystem.RegisterZone(NewZone) ? NewZone : nullptr;
}

// Visibility를 Block하는 기존 바닥이 Zone 전체를 덮지 못할 때만 테스트용 큐브 바닥을 생성한다.
void UFF_ZoneSystemPlaytestSubsystem::EnsureTestFloor(UWorld& World, const AFFZSGameplayZone& Zone)
{
    FBox ZoneBounds(EForceInit::ForceInit);
    const FVector LocalCorners[] = {
        FVector(0.0f, 0.0f, 0.0f),
        FVector(TestZoneSizeCm, 0.0f, 0.0f),
        FVector(TestZoneSizeCm, TestZoneSizeCm, 0.0f),
        FVector(0.0f, TestZoneSizeCm, 0.0f)
    };
    for (const FVector& LocalCorner : LocalCorners)
    {
        ZoneBounds += Zone.GetActorTransform().TransformPosition(LocalCorner);
    }

    for (TActorIterator<AStaticMeshActor> It(&World); It; ++It)
    {
        const UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
        if (!Mesh || !Mesh->GetStaticMesh() || Mesh->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Ignore)
        {
            continue;
        }

        const FBox GroundBounds = Mesh->Bounds.GetBox();
        const bool bCoversZone =
            GroundBounds.Min.X <= ZoneBounds.Min.X + 1.0f &&
            GroundBounds.Min.Y <= ZoneBounds.Min.Y + 1.0f &&
            GroundBounds.Max.X >= ZoneBounds.Max.X - 1.0f &&
            GroundBounds.Max.Y >= ZoneBounds.Max.Y - 1.0f;
        if (bCoversZone)
        {
            return;
        }
    }

    UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (!PlaneMesh)
    {
        UE_LOG(LogFFZonePlaytest, Error, TEXT("Could not load /Engine/BasicShapes/Plane.Plane."));
        return;
    }

    const FVector ZoneCenter = Zone.GetActorTransform().TransformPosition(
        FVector(TestZoneSizeCm * 0.5f, TestZoneSizeCm * 0.5f, 0.0f));
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.ObjectFlags |= RF_Transient;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    RuntimeFloor = World.SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(),
        FTransform(FRotator::ZeroRotator, ZoneCenter, FVector(TestFloorScale, TestFloorScale, 1.0f)),
        SpawnParameters);
    if (!RuntimeFloor)
    {
        return;
    }

    UStaticMeshComponent* FloorMesh = RuntimeFloor->GetStaticMeshComponent();
    FloorMesh->SetStaticMesh(PlaneMesh);
    FloorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    FloorMesh->SetCollisionResponseToAllChannels(ECR_Block);
    FloorMesh->SetGenerateOverlapEvents(false);
}

// 보호 구역 거부와 빨간 프리뷰를 즉시 확인할 수 있도록 Zone 안에 작은 NoBuild 영역을 만든다.
void UFF_ZoneSystemPlaytestSubsystem::CreateProtectedArea(
    UWorld& World,
    UFFZSWorldSubsystem& ZoneSystem)
{
    const FVector ProtectedLocation = TestZone->GetActorTransform().TransformPosition(
        FVector(8000.0f, 5000.0f, 100.0f));
    const FTransform ProtectedTransform(FRotator(0.0f, 15.0f, 0.0f), ProtectedLocation);

    TestProtectedArea = World.SpawnActorDeferred<AFFZSProtectedArea>(
        AFFZSProtectedArea::StaticClass(),
        ProtectedTransform,
        nullptr,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestProtectedArea)
    {
        return;
    }

    TestProtectedArea->SetFlags(RF_Transient);
    TestProtectedArea->ProtectedAreaId = TEXT("PA_ZoneTest_NoBuild");
    TestProtectedArea->LinkedZoneId = ActiveZoneId;
    TestProtectedArea->AreaType = EFFZSProtectedAreaType::NoBuild;
    TestProtectedArea->RequiredClearWidthCm = 0.0f;
    TestProtectedArea->AreaBounds->SetBoxExtent(FVector(600.0f, 900.0f, 200.0f));
    UGameplayStatics::FinishSpawningActor(TestProtectedArea, ProtectedTransform);
    ZoneSystem.RegisterProtectedArea(TestProtectedArea);
}

void UFF_ZoneSystemPlaytestSubsystem::CreatePlacementPreview(UWorld& World)
{
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.ObjectFlags |= RF_Transient;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    PlacementPreview = World.SpawnActor<AFFZSPlacementPreviewActor>(
        AFFZSPlacementPreviewActor::StaticClass(),
        FTransform::Identity,
        SpawnParameters);
    if (!PlacementPreview)
    {
        return;
    }

    PlacementPreview->bAutoRefreshPreview = false;
    PlacementPreview->bDrawDebugCells = false;
    PlacementPreview->DebugDrawLifetimeSeconds = PreviewRefreshIntervalSeconds * 1.5f;
}

// PlayerController가 준비되는 시점까지 기다렸다가 관찰 Pawn과 WBP를 각각 한 번만 연결한다.
void UFF_ZoneSystemPlaytestSubsystem::EnsurePlayerExperience()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    if (!Controller || !Controller->IsLocalController())
    {
        return;
    }

    if (!IsValid(PlaytestPawn))
    {
        SpawnPlaytestPawn(*Controller);
    }
    else if (Controller->GetPawn() != PlaytestPawn)
    {
        Controller->Possess(PlaytestPawn);
    }
    if (!HudWidget)
    {
        CreatePlaytestHud(*Controller);
    }

    if (bExperienceSmokeRequested &&
        IsValid(PlaytestPawn) &&
        Controller->GetPawn() == PlaytestPawn &&
        PlaytestPawn->HasActiveOverviewCamera() &&
        bHasPlacementTarget &&
        HudWidget)
    {
        CompleteExperienceSmokeTest();
    }
}

// Zone과 런타임 바닥의 Bounds를 합쳐 전체가 보이는 중심과 카메라 거리를 계산한다.
// 기존 Pawn은 엔진 기본 ADefaultPawn일 때만 제거해 팀의 사용자 Pawn을 함부로 파괴하지 않는다.
void UFF_ZoneSystemPlaytestSubsystem::SpawnPlaytestPawn(APlayerController& Controller)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    APawn* PreviousPawn = Controller.GetPawn();

    FBox ViewBounds(ForceInit);
    if (TestZone)
    {
        const FBox ZoneBounds = TestZone->GetComponentsBoundingBox(true);
        if (ZoneBounds.IsValid)
        {
            ViewBounds += ZoneBounds;
        }
    }
    if (RuntimeFloor)
    {
        ViewBounds += RuntimeFloor->GetComponentsBoundingBox(true);
    }

    FVector FocusLocation = PreviousPawn ? PreviousPawn->GetActorLocation() : FVector::ZeroVector;
    float CameraDistance = TestZoneSizeCm * 1.35f;
    if (ViewBounds.IsValid)
    {
        FocusLocation = ViewBounds.GetCenter();
        FocusLocation.Z = ViewBounds.Max.Z + 50.0f;
        const FVector ViewSize = ViewBounds.GetSize();
        CameraDistance = FMath::Max(ViewSize.X, ViewSize.Y) * 1.35f;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.ObjectFlags |= RF_Transient;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    PlaytestPawn = World->SpawnActor<AFF_ZoneSystemPlaytestOverviewPawn>(
        AFF_ZoneSystemPlaytestOverviewPawn::StaticClass(),
        FTransform(FRotator::ZeroRotator, FocusLocation),
        SpawnParameters);
    if (!PlaytestPawn)
    {
        SetStatus(TEXT("Could not spawn the Zone overview camera."), FColor::Red);
        return;
    }

    PlaytestPawn->InitializeOverview(FocusLocation, CameraDistance);
    Controller.Possess(PlaytestPawn);
    if (PreviousPawn && PreviousPawn != PlaytestPawn && PreviousPawn->GetClass() == ADefaultPawn::StaticClass())
    {
        PreviousPawn->Destroy();
    }

    PlaytestPawn->SetUiInteractionMode(bUiInteractionMode);
    ApplyUiInputMode();
    UpdatePlacementFromPlayer();
    RefreshPreview();
    UE_LOG(LogFFZonePlaytest, Display, TEXT("ZoneTestMap overview pawn possessed."));
}

// 실제 WBP를 동기 로드하고 실패하면 DrawHud의 단순 텍스트 폴백을 사용한다.
void UFF_ZoneSystemPlaytestSubsystem::CreatePlaytestHud(APlayerController& Controller)
{
    const TSubclassOf<UFF_ZoneSystemPlaytestHUD> LoadedClass = HudWidgetClass.LoadSynchronous();
    if (!LoadedClass)
    {
        UE_LOG(LogFFZonePlaytest, Warning,
            TEXT("WBP_ZoneSystemPlaytestHUD is missing; using the debug-text fallback."));
        return;
    }

    HudWidget = CreateWidget<UFF_ZoneSystemPlaytestHUD>(&Controller, LoadedClass);
    if (!HudWidget)
    {
        UE_LOG(LogFFZonePlaytest, Warning, TEXT("Could not create WBP_ZoneSystemPlaytestHUD."));
        return;
    }

    HudWidget->AddToPlayerScreen(20);
    HudWidget->SetVisibility(bHudVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ApplyUiInputMode();
    UE_LOG(LogFFZonePlaytest, Display, TEXT("Editable Zone playtest WBP added to the viewport."));
}

// 관찰 모드에서는 게임 입력과 중앙 표시점을, HUD 모드에서는 커서와 버튼 포커스를 사용한다.
void UFF_ZoneSystemPlaytestSubsystem::ApplyUiInputMode()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    if (!Controller)
    {
        return;
    }

    Controller->bShowMouseCursor = bUiInteractionMode;
    if (bUiInteractionMode && HudWidget)
    {
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(HudWidget->TakeWidget());
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        Controller->SetInputMode(InputMode);
    }
    else
    {
        Controller->SetInputMode(FInputModeGameOnly());
    }

    if (PlaytestPawn)
    {
        PlaytestPawn->SetUiInteractionMode(bUiInteractionMode);
    }
}

// 화면 중앙 카메라 레이를 지면까지 쏜 뒤 다음 순서로 안전한 시설 좌표를 만든다.
// 1) 수평에 가까운 표면과 Zone 내부 여부 확인, 2) 10cm 스냅,
// 3) 회전된 footprint 중심 보정, 4) 프리뷰 명령의 좌하단 OriginCell 갱신.
bool UFF_ZoneSystemPlaytestSubsystem::UpdatePlacementFromPlayer()
{
    UWorld* World = GetWorld();
    APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
    if (!World || !Controller || !PlaytestPawn || !TestZone || !PlacementPreview || bUiInteractionMode)
    {
        return false;
    }

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    const FVector TraceStart = ViewLocation;
    const FVector TraceEnd = TraceStart + ViewRotation.Vector() * 200000.0f;

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FFZonePlaytestPlacement), false);
    QueryParams.AddIgnoredActor(PlaytestPawn);
    QueryParams.AddIgnoredActor(PlacementPreview);
    QueryParams.AddIgnoredActor(TestZone);
    QueryParams.AddIgnoredActor(TestProtectedArea);
    for (TActorIterator<AFFZSFacilityActor> It(World); It; ++It)
    {
        QueryParams.AddIgnoredActor(*It);
    }

    FHitResult GroundHit;
    if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams) ||
        GroundHit.ImpactNormal.Z < 0.45f ||
        !TestZone->ContainsWorldPoint(GroundHit.ImpactPoint))
    {
        bHasPlacementTarget = false;
        PlacementPreview->SetActorHiddenInGame(true);
        return false;
    }

    FIntPoint TargetCell = TestZone->WorldToGridCell1Cm(GroundHit.ImpactPoint);
    TargetCell.X = FMath::RoundToInt(static_cast<double>(TargetCell.X) / PlacementSnapCm) * PlacementSnapCm;
    TargetCell.Y = FMath::RoundToInt(static_cast<double>(TargetCell.Y) / PlacementSnapCm) * PlacementSnapCm;

    const FFFZSFacilitySpec Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(SelectedPreset);
    const FFFZSCellRect RotatedLocal = UFFZSGridLibrary::RotateRect(
        FFFZSCellRect(FIntPoint::ZeroValue, Spec.FootprintSizeCells), CurrentPlacement.QuarterTurns);
    if (!RotatedLocal.IsValid())
    {
        bHasPlacementTarget = false;
        PlacementPreview->SetActorHiddenInGame(true);
        return false;
    }

    const FVector2D CenterOffset(
        (static_cast<double>(RotatedLocal.Min.X) + RotatedLocal.Max.X) * 0.5,
        (static_cast<double>(RotatedLocal.Min.Y) + RotatedLocal.Max.Y) * 0.5);
    const FIntPoint NewOrigin(
        FMath::RoundToInt(static_cast<double>(TargetCell.X) - CenterOffset.X),
        FMath::RoundToInt(static_cast<double>(TargetCell.Y) - CenterOffset.Y));

    const bool bRecoveredTarget = !bHasPlacementTarget;
    const bool bPlacementChanged = NewOrigin != CurrentPlacement.OriginCell;
    bHasPlacementTarget = true;
    PlacementPreview->SetActorHiddenInGame(false);
    if (!bPlacementChanged && !bRecoveredTarget)
    {
        return false;
    }

    CurrentPlacement.OriginCell = NewOrigin;
    RebuildPreviewCommand();
    return true;
}

void UFF_ZoneSystemPlaytestSubsystem::SetSelectedPreset(const EFFZSBuiltInFacilityPreset NewPreset)
{
    SelectedPreset = NewPreset;
    UpdatePlacementFromPlayer();
    RebuildPreviewCommand();
    RefreshPreview();
    SetStatus(FString::Printf(TEXT("Selected %s."),
        *EnumName(StaticEnum<EFFZSBuiltInFacilityPreset>(), static_cast<int64>(SelectedPreset))));
}

void UFF_ZoneSystemPlaytestSubsystem::SetPlacement(const FIntPoint& OriginCell, const int32 QuarterTurns)
{
    CurrentPlacement.OriginCell = OriginCell;
    CurrentPlacement.QuarterTurns = UFFZSGridLibrary::NormalizeQuarterTurns(QuarterTurns);
    bHasPlacementTarget = true;
    if (PlacementPreview)
    {
        PlacementPreview->SetActorHiddenInGame(false);
    }
    RebuildPreviewCommand();
    RefreshPreview();
}

// 선택 프리셋과 현재 좌표로 플러그인이 실제 검증할 Install 명령을 다시 만든다.
void UFF_ZoneSystemPlaytestSubsystem::RebuildPreviewCommand()
{
    if (!PlacementPreview || ActiveZoneId.IsNone())
    {
        return;
    }

    FFFZSFacilitySpec Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(SelectedPreset);
    Spec.VisualMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
    PlacementPreview->PreviewCommand = UFFZSGridLibrary::MakeInstallCommand(
        Spec, ActiveZoneId, CurrentPlacement, -1);
    ApplyPreviewVisualScale();
}

// 중앙 레이가 지면을 잃으면 오래된 좌표가 보이지 않도록 프리뷰를 숨기고 검증도 중단한다.
void UFF_ZoneSystemPlaytestSubsystem::RefreshPreview()
{
    if (!PlacementPreview)
    {
        return;
    }
    if (!bHasPlacementTarget)
    {
        PlacementPreview->SetActorHiddenInGame(true);
        return;
    }

    PlacementPreview->SetActorHiddenInGame(false);
    const FScopedNavigationValidation NavigationScope(bEnableFullNavigationValidation);
    LastValidation = PlacementPreview->RefreshPreview();
    ApplyPreviewVisualScale();
}

// 테스트용 단위 큐브를 시설 footprint와 장애물 높이에 맞춰 실제 점유 크기로 보이게 한다.
void UFF_ZoneSystemPlaytestSubsystem::ApplyPreviewVisualScale()
{
    if (!PlacementPreview || !PlacementPreview->PreviewMesh)
    {
        return;
    }

    const FFFZSFacilitySpec& Spec = PlacementPreview->PreviewCommand.FacilitySpec;
    PlacementPreview->PreviewMesh->SetRelativeScale3D(FVector(
        Spec.FootprintSizeCells.X / 100.0f,
        Spec.FootprintSizeCells.Y / 100.0f,
        Spec.PlacementObstacleHeightCm / 100.0f));
    PlacementPreview->PreviewMesh->SetRelativeLocation(
        FVector(0.0f, 0.0f, Spec.PlacementObstacleHeightCm * 0.5f));
}

void UFF_ZoneSystemPlaytestSubsystem::ApplyFacilityVisualScale(AFFZSFacilityActor* Facility) const
{
    if (!Facility || !Facility->VisualMesh)
    {
        return;
    }

    const FFFZSFacilitySpec& Spec = Facility->GetFacilitySpec();
    Facility->VisualMesh->SetRelativeScale3D(FVector(
        Spec.FootprintSizeCells.X / 100.0f,
        Spec.FootprintSizeCells.Y / 100.0f,
        Spec.PlacementObstacleHeightCm / 100.0f));
    Facility->VisualMesh->SetRelativeLocation(
        FVector(0.0f, 0.0f, Spec.PlacementObstacleHeightCm * 0.5f));
}

void UFF_ZoneSystemPlaytestSubsystem::ApplyAllFacilityVisuals() const
{
    const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
    if (!ZoneSystem)
    {
        return;
    }

    const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
    for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
    {
        ApplyFacilityVisualScale(ZoneSystem->FindFacility(Facility.FacilityInstanceId));
    }
}

void UFF_ZoneSystemPlaytestSubsystem::SelectPreviousPreset()
{
    const int32 Index = static_cast<int32>(SelectedPreset);
    SetSelectedPreset(static_cast<EFFZSBuiltInFacilityPreset>((Index + BuiltInPresetCount - 1) % BuiltInPresetCount));
}

void UFF_ZoneSystemPlaytestSubsystem::SelectNextPreset()
{
    const int32 Index = static_cast<int32>(SelectedPreset);
    SetSelectedPreset(static_cast<EFFZSBuiltInFacilityPreset>((Index + 1) % BuiltInPresetCount));
}

void UFF_ZoneSystemPlaytestSubsystem::InstallAtPreview() { CommitPreview(); }
void UFF_ZoneSystemPlaytestSubsystem::MoveSelectedFacility() { MoveLastFacility(); }
void UFF_ZoneSystemPlaytestSubsystem::RemoveSelectedFacility() { ExecuteLifecycleCommand(EFFZSCommandType::Remove); }
void UFF_ZoneSystemPlaytestSubsystem::OpenSelectedFacility() { ExecuteLifecycleCommand(EFFZSCommandType::Open); }
void UFF_ZoneSystemPlaytestSubsystem::CloseSelectedFacility() { ExecuteLifecycleCommand(EFFZSCommandType::Close); }
void UFF_ZoneSystemPlaytestSubsystem::EmergencyCloseSelectedFacility() { ExecuteLifecycleCommand(EFFZSCommandType::EmergencyClose); }
void UFF_ZoneSystemPlaytestSubsystem::JoinSelectedQueue() { JoinQueueAgent(); }
void UFF_ZoneSystemPlaytestSubsystem::AdvanceSelectedQueue() { AdvanceQueueAgents(); }
void UFF_ZoneSystemPlaytestSubsystem::ToggleSelectedQueueBlocked() { ToggleQueueBlocked(); }
void UFF_ZoneSystemPlaytestSubsystem::SavePlaytestSnapshot() { CapturePlaytestSnapshot(); }
void UFF_ZoneSystemPlaytestSubsystem::LoadPlaytestSnapshot() { RestorePlaytestSnapshot(); }
void UFF_ZoneSystemPlaytestSubsystem::ResetPlaytestScenario() { ResetPlaytest(); }

void UFF_ZoneSystemPlaytestSubsystem::ToggleWorldDebug()
{
    bShowWorldDebug = !bShowWorldDebug;
    if (PlacementPreview)
    {
        PlacementPreview->bDrawDebugCells = bShowWorldDebug;
    }
    SetStatus(bShowWorldDebug ? TEXT("World debug enabled.") : TEXT("World debug hidden."), FColor::Cyan);
}

void UFF_ZoneSystemPlaytestSubsystem::ToggleHelp()
{
    bShowHelp = !bShowHelp;
}

void UFF_ZoneSystemPlaytestSubsystem::ToggleUiInteractionMode()
{
    bUiInteractionMode = !bUiInteractionMode;
    ApplyUiInputMode();
    SetStatus(bUiInteractionMode
        ? TEXT("HUD interaction mode. Click buttons or press Tab to return.")
        : TEXT("Overview mode. The center marker selects the ground target."), FColor::Cyan);
}

// 관찰 Pawn, 하향 카메라, 지면 타깃과 WBP가 모두 준비된 뒤에만 성공 마커를 남긴다.
void UFF_ZoneSystemPlaytestSubsystem::CompleteExperienceSmokeTest()
{
    bExperienceSmokeRequested = false;
    UE_LOG(LogFFZonePlaytest, Display,
        TEXT("FFZS_ZONETESTMAP_EXPERIENCE_PASS: overview pawn possessed and editable WBP created."));
    SmokeExitCode = 0;
    bSmokeExitPending = true;
}




