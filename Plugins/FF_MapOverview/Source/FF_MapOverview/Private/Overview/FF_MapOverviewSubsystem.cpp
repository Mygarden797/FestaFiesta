#include "FF_MapOverviewSubsystem.h"

#include "FF_MapCameraLibrary.h"
#include "FF_MapGridActor.h"
#include "FF_MapOverviewBridge.h"
#include "FF_MapOverviewWidget.h"
#include "FF_MapTerrainCapture.h"
#include "FF_MapWholeGridButtonWidget.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSBuildableGridActor.h"
#include "FF_FFZSBuildableGridComponent.h"
#include "FF_FFZSWorldSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFMapOverview, Log, All);

UFFMapOverviewSubsystem::UFFMapOverviewSubsystem()
{
    static ConstructorHelpers::FClassFinder<UFFMapOverviewWidget> DesignerWidget(
        TEXT("/FF_MapOverview/UI/WBP_FF_MapOverview"));
    MapWidgetClass = DesignerWidget.Class;
    static ConstructorHelpers::FClassFinder<UFFMapWholeGridButtonWidget> ToolbarWidget(
        TEXT("/FF_MapOverview/UI/WBP_FF_WholeGridButton"));
    WholeGridToolbarClass = ToolbarWidget.Class;
}

bool UFFMapOverviewSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const auto* World = Cast<UWorld>(Outer);
    return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE ||
        World->WorldType == EWorldType::GamePreview) && World->GetNetMode() != NM_DedicatedServer &&
        Super::ShouldCreateSubsystem(Outer);
}

void UFFMapOverviewSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UFFZSWorldSubsystem>();
    Super::Initialize(Collection);
    UWorld* World = GetWorld();
    ActorSpawnHandle = World->AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::DiscoverGridActor));
    for (TActorIterator<AFFZSBuildableGridActor> It(World); It; ++It) DiscoverGridActor(*It);
}

void UFFMapOverviewSubsystem::DiscoverGridActor(AActor* Actor)
{
    if (auto* Grid = Cast<AFFZSBuildableGridActor>(Actor)) LegacyGridActors.AddUnique(Grid);
}

void UFFMapOverviewSubsystem::Deinitialize()
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->RemoveOnActorSpawnedHandler(ActorSpawnHandle);
        if (auto* PC = World->GetFirstPlayerController(); PC && FFMapCamera::IsWholeMapView(PC))
            FFMapCamera::RestoreCamera(PC);
    }
    SetLegacyGridSuppressed(false);
    if (auto* Bridge = IFFMapOverviewBridge::Find(); WholeGridToolbarWidget && Bridge)
        Bridge->UnmountWholeGridToolbar(World, WholeGridToolbarWidget);
    WholeGridToolbarWidget = nullptr;
    if (MapWidget) MapWidget->RemoveFromParent();
    MapWidget = nullptr;
    if (TerrainCapture) TerrainCapture->Shutdown();
    TerrainCapture = nullptr;
    if (GridActor) GridActor->Destroy();
    GridActor = nullptr;
    LegacyGridActors.Reset();
    Super::Deinitialize();
}

TStatId UFFMapOverviewSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UFFMapOverviewSubsystem, STATGROUP_Tickables);
}

void UFFMapOverviewSubsystem::SetLegacyGridSuppressed(const bool bSuppress)
{
    if (!bSuppress)
    {
        for (const auto& Entry : SuppressedComponents)
            if (Entry.Key.IsValid()) Entry.Key->SetVisibility(Entry.Value);
        SuppressedComponents.Reset();
        return;
    }
    // The original presenter can add a component after its actor-spawn notification.
    // Visibility is independent of SetHiddenInGame, so its normal updates cannot re-show it.
    // Only grid components are touched; original values are restored on teardown.
    LegacyGridActors.RemoveAllSwap([](const auto& Actor) { return !Actor.IsValid(); });
    for (const auto& Actor : LegacyGridActors)
    {
        TInlineComponentArray<UFFZSBuildableGridComponent*> Components(Actor.Get());
        for (auto* Component : Components)
        {
            if (!SuppressedComponents.Contains(Component))
                SuppressedComponents.Add(Component, Component->IsVisible());
            Component->SetVisibility(false);
        }
    }
}

void UFFMapOverviewSubsystem::Tick(const float DeltaTime)
{
    if (!GetWorld() || !GetWorld()->HasBegunPlay()) return;
    IFFMapOverviewBridge* Bridge = IFFMapOverviewBridge::Find();
    if (!Bridge) return;
    const bool bHudReady = Bridge->IsHudContextReady(GetWorld());
    const bool bBlocked = Bridge->IsInputBlocked(GetWorld());
    const bool bOwnsGrid = bHudReady && MapBounds.IsValid && Bridge->IsBuildSessionActive(GetWorld()) && IsValid(GridActor);
    SetLegacyGridSuppressed(bOwnsGrid);
    if (MapWidget) MapWidget->SetVisibility(bHudReady && !bBlocked && MapBounds.IsValid
        ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (WholeGridToolbarWidget)
    {
        const bool bShow = bHudReady && !bBlocked && Bridge->IsBuildSessionActive(GetWorld());
        WholeGridToolbarWidget->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        if (!Bridge->IsBuildSessionActive(GetWorld())) Bridge->UnmountWholeGridToolbar(GetWorld(), WholeGridToolbarWidget);
    }
    if (!bHudReady || bBlocked)
    {
        if (GridActor) GridActor->HideGrid();
        bForceRefresh = true;
        if (!bHudReady && IsWholeMapView()) FFMapCamera::RestoreCamera(GetWorld()->GetFirstPlayerController());
        return;
    }
    RefreshAccumulator += DeltaTime;
    BoundsAccumulator += DeltaTime;
    // Map markers 10 Hz; pending ground samples continue at the cache's 0.08 s budget cadence.
    const float Interval = GridActor && GridActor->HasPendingSamples() ? 0.08f : 0.1f;
    if (!bForceRefresh && RefreshAccumulator < Interval) return;
    RefreshAccumulator = 0.0f;
    bForceRefresh = false;
    RefreshPresentation();
}

void UFFMapOverviewSubsystem::RefreshPresentation()
{
    UWorld* World = GetWorld();
    auto* PC = World->GetFirstPlayerController();
    IFFMapOverviewBridge* Bridge = IFFMapOverviewBridge::Find();
    if (!Bridge || !PC || !PC->IsLocalController()) return;
    if (IsBuildSessionActive())
    {
        if (!WholeGridToolbarWidget)
        {
            if (!WholeGridToolbarClass && !bTriedLoadToolbarClass)
            {
                bTriedLoadToolbarClass = true;
                WholeGridToolbarClass = LoadClass<UFFMapWholeGridButtonWidget>(nullptr,
                    TEXT("/FF_MapOverview/UI/WBP_FF_WholeGridButton.WBP_FF_WholeGridButton_C"));
                if (!WholeGridToolbarClass)
                    UE_LOG(LogFFMapOverview, Error, TEXT("Designer whole-grid toolbar WBP missing: /FF_MapOverview/UI/WBP_FF_WholeGridButton."));
            }
            if (WholeGridToolbarClass)
            {
                WholeGridToolbarWidget = CreateWidget<UFFMapWholeGridButtonWidget>(PC, WholeGridToolbarClass);
                if (WholeGridToolbarWidget) WholeGridToolbarWidget->InitializeOverview(this);
            }
        }
        if (WholeGridToolbarWidget)
        {
            WholeGridToolbarWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            Bridge->MountWholeGridToolbar(World, WholeGridToolbarWidget);
            WholeGridToolbarWidget->RefreshState();
        }
    }
    if (BoundsAccumulator >= 1.0f || !MapBounds.IsValid)
    {
        FBox NewBounds(ForceInit);
        if (FFMapCamera::TryGetMapBounds(World, NewBounds))
        {
            const bool bBoundsChanged = !MapBounds.IsValid || !MapBounds.Equals(NewBounds, 0.01);
            MapBounds = NewBounds;
            if (bBoundsChanged && IsWholeMapView()) FFMapCamera::RefreshWholeMapBounds(PC, MapBounds);
        }
        else MapBounds = FBox(ForceInit);
        BoundsAccumulator = 0.0f;
    }
    if (!MapBounds.IsValid)
    {
        if (IsWholeMapView()) FFMapCamera::RestoreCamera(PC);
        if (MapWidget) MapWidget->SetVisibility(ESlateVisibility::Collapsed);
        if (GridActor) GridActor->HideGrid();
        SetLegacyGridSuppressed(false);
        return;
    }
    if (!MapWidget)
    {
        if (!MapWidgetClass && !bTriedLoadWidgetClass)
        {
            bTriedLoadWidgetClass = true;
            MapWidgetClass = LoadClass<UFFMapOverviewWidget>(nullptr,
                TEXT("/FF_MapOverview/UI/WBP_FF_MapOverview.WBP_FF_MapOverview_C"));
            if (!MapWidgetClass)
                UE_LOG(LogFFMapOverview, Error, TEXT("Designer minimap WBP missing: /FF_MapOverview/UI/WBP_FF_MapOverview. No native layout fallback."));
        }
        if (MapWidgetClass) MapWidget = CreateWidget<UFFMapOverviewWidget>(PC, MapWidgetClass);
        if (MapWidget) MapWidget->InitializeOverview(this);
    }
    if (auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>())
    {
        if (!TerrainCapture) TerrainCapture = NewObject<UFFMapTerrainCapture>(this);
        FVector PlayerLocation;
        if (TryGetPlayerMapLocation(PlayerLocation))
            TerrainCapture->Refresh(*World, MapBounds, Zones->GetWorldRevision(), PlayerLocation);
    }
    if (MapWidget)
    {
        Bridge->MountHudWidget(World, MapWidget);
        MapWidget->RefreshMap();
    }
    if (!IsBuildSessionActive() || !IsGridVisible())
    {
        if (GridActor) GridActor->HideGrid();
        return;
    }
    if (!GridActor)
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        GridActor = World->SpawnActor<AFFMapGridActor>(Params);
        if (!GridActor)
        {
            UE_LOG(LogFFMapOverview, Warning, TEXT("Map grid presenter could not be created; keeping legacy grid."));
            return;
        }
    }
    SetLegacyGridSuppressed(true);
    FVector SurfacePoint, SurfaceNormal;
    if (!ResolveSurface(SurfacePoint, SurfaceNormal))
    {
        GridActor->HideGrid();
        return;
    }
    FFFZSCommand Context;
    const bool bHasContext = Bridge->TryGetPlacementContext(World, Context);
    if (auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>())
    {
        const double StartSeconds = FPlatformTime::Seconds();
        GridActor->RefreshGrid(*Zones, SurfacePoint, SurfaceNormal, Bridge->GetVisualGridStepCm(World),
            bWholeGrid, bHasContext ? &Context : nullptr);
        LastGridUpdateMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
    }
}

bool UFFMapOverviewSubsystem::ResolveSurface(FVector& OutPoint, FVector& OutNormal) const
{
    auto* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC || !MapBounds.IsValid) return false;
    OutNormal = FVector::UpVector;
    if (IsWholeMapView())
    {
        OutPoint = MapBounds.GetCenter();
        return true;
    }
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    FVector Origin, Direction;
    if (Width > 0 && Height > 0 &&
        PC->DeprojectScreenPositionToWorld(Width * 0.5f, Height * 0.45f, Origin, Direction))
    {
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(FFMapGridCenter), false);
        if (PC->GetPawn()) Params.AddIgnoredActor(PC->GetPawn());
        if (GridActor) Params.AddIgnoredActor(GridActor);
        if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 2000000.0,
            ECC_Visibility, Params) && Hit.ImpactNormal.Z >= 0.25)
        {
            OutPoint = Hit.ImpactPoint;
            OutNormal = Hit.ImpactNormal;
            return true;
        }
    }
    // A terrain hole at the camera center must not hide all surrounding valid build land.
    if (auto* Bridge = IFFMapOverviewBridge::Find(); Bridge && Bridge->TryGetCameraFocus(PC, OutPoint)) return true;
    OutPoint = MapBounds.GetCenter();
    return true;
}

bool UFFMapOverviewSubsystem::IsBuildSessionActive() const
{
    auto* Bridge = IFFMapOverviewBridge::Find();
    return Bridge && Bridge->IsBuildSessionActive(GetWorld());
}

bool UFFMapOverviewSubsystem::IsGridVisible() const
{
    auto* Bridge = IFFMapOverviewBridge::Find();
    return Bridge && Bridge->IsGridVisible(GetWorld());
}

bool UFFMapOverviewSubsystem::IsWholeMapView() const
{
    return GetWorld() && FFMapCamera::IsWholeMapView(GetWorld()->GetFirstPlayerController());
}

bool UFFMapOverviewSubsystem::TryGetCameraFocus(FVector& OutFocus) const
{
    if (IsWholeMapView() && MapBounds.IsValid)
    {
        OutFocus = MapBounds.GetCenter();
        return true;
    }
    auto* Bridge = IFFMapOverviewBridge::Find();
    return Bridge && GetWorld() && Bridge->TryGetCameraFocus(GetWorld()->GetFirstPlayerController(), OutFocus);
}

void UFFMapOverviewSubsystem::SetWholeGridEnabled(const bool bEnabled)
{
    NotifyMapInteraction();
    bWholeGrid = bEnabled;
    bForceRefresh = true;
    if (MapWidget) MapWidget->RefreshMap();
}

void UFFMapOverviewSubsystem::ToggleWholeGrid()
{
    auto* Bridge = IFFMapOverviewBridge::Find();
    if (!Bridge || !IsBuildSessionActive() || Bridge->IsInputBlocked(GetWorld())) return;
    NotifyMapInteraction();
    // A click on an unlit whole-grid control also restores visibility after G/grid-off.
    const bool bEnable = !(bWholeGrid && IsGridVisible());
    if (bEnable && !IsGridVisible()) Bridge->SetGridVisible(GetWorld(), true);
    bWholeGrid = bEnable;
    bForceRefresh = true;
    if (WholeGridToolbarWidget) WholeGridToolbarWidget->RefreshState();
}

void UFFMapOverviewSubsystem::ToggleGridVisibility()
{
    if (auto* Bridge = IFFMapOverviewBridge::Find(); Bridge && IsBuildSessionActive())
    {
        NotifyMapInteraction();
        Bridge->SetGridVisible(GetWorld(), !IsGridVisible());
        bForceRefresh = true;
        if (MapWidget) MapWidget->RefreshMap();
    }
}

void UFFMapOverviewSubsystem::NotifyMapInteraction()
{
    if (auto* Bridge = IFFMapOverviewBridge::Find()) Bridge->NotifyMapInteraction(GetWorld());
}

void UFFMapOverviewSubsystem::ToggleWholeMapView()
{
    auto* Bridge = IFFMapOverviewBridge::Find();
    auto* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!Bridge || !PC || Bridge->IsInputBlocked(GetWorld())) return;
    NotifyMapInteraction();
    if (IsWholeMapView()) FFMapCamera::RestoreCamera(PC);
    else FFMapCamera::ShowWholeMap(PC);
    bForceRefresh = true;
    if (MapWidget) MapWidget->RefreshMap();
}

void UFFMapOverviewSubsystem::FocusMapLocation(const FVector Location)
{
    auto* Bridge = IFFMapOverviewBridge::Find();
    auto* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!Bridge || !PC || Bridge->IsInputBlocked(GetWorld())) return;
    NotifyMapInteraction();
    FFMapCamera::FocusMapPoint(PC, Location);
    bForceRefresh = true;
}

int32 UFFMapOverviewSubsystem::GetGridCellCount() const { return GridActor ? GridActor->GetRetainedCellCount() : 0; }
int32 UFFMapOverviewSubsystem::GetGridLineCount() const { return GridActor ? GridActor->GetRenderedLineCount() : 0; }
int32 UFFMapOverviewSubsystem::GetGridTraceBudget() const { return GridActor ? GridActor->GetLastTraceBudget() : 0; }
double UFFMapOverviewSubsystem::GetGridSpacingCm() const { return GridActor ? GridActor->GetOverviewSpacingCm() : 0.0; }
double UFFMapOverviewSubsystem::GetGridZoomSpacingPixels() const { return GridActor ? GridActor->GetZoomSpacingPixels() : 0.0; }
int32 UFFMapOverviewSubsystem::GetGridZoomStepCm() const { return GridActor ? GridActor->GetZoomStepCm() : 0; }
int32 UFFMapOverviewSubsystem::GetGridBaseStepCm() const { return GridActor ? GridActor->GetBaseStepCm() : 0; }
UTextureRenderTarget2D* UFFMapOverviewSubsystem::GetTerrainMapTexture() const
{
    return TerrainCapture ? TerrainCapture->GetTexture() : nullptr;
}

FBox UFFMapOverviewSubsystem::GetTerrainMapBounds() const
{
    return TerrainCapture ? TerrainCapture->GetBounds() : FBox(ForceInit);
}

bool UFFMapOverviewSubsystem::TryGetPlayerMapLocation(FVector& OutLocation) const
{
    auto* Bridge = IFFMapOverviewBridge::Find();
    return Bridge && GetWorld() && Bridge->TryGetPlayerMapLocation(GetWorld()->GetFirstPlayerController(), OutLocation);
}
