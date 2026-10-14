#include "FF_MapCameraLibrary.h"

#include "FF_MapOverviewBridge.h"
#include "FF_MapOverviewViewTarget.h"
#include "FF_MapOverviewSubsystem.h"
#include "FF_MapOverviewWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "GameFramework/PlayerController.h"

namespace
{
    AFFMapOverviewViewTarget* FindOverviewSession(APlayerController* Controller)
    {
        if (!IsValid(Controller) || !Controller->GetWorld()) return nullptr;
        if (AFFMapOverviewViewTarget* Current = Cast<AFFMapOverviewViewTarget>(Controller->GetViewTarget()))
        {
            if (Current->MapController.Get() == Controller) return Current;
        }
        // Called only on a view transition, never for per-frame minimap or grid presentation.
        for (TActorIterator<AFFMapOverviewViewTarget> It(Controller->GetWorld()); It; ++It)
        {
            if (It->MapController.Get() == Controller) return *It;
        }
        return nullptr;
    }

    bool IsFiniteBox(const FBox& Box)
    {
        return Box.IsValid && !Box.Min.ContainsNaN() && !Box.Max.ContainsNaN()
            && Box.Min.X <= Box.Max.X && Box.Min.Y <= Box.Max.Y && Box.Min.Z <= Box.Max.Z;
    }

    float ReadMapPanelRightInset(APlayerController* Controller)
    {
        const UFFMapOverviewSubsystem* Presentation = Controller && Controller->GetWorld()
            ? Controller->GetWorld()->GetSubsystem<UFFMapOverviewSubsystem>() : nullptr;
        const UFFMapOverviewWidget* Widget = Presentation ? Presentation->GetMapWidget() : nullptr;
        return Widget ? FMath::Max(0.0f, Widget->GetMapPanelRightInsetSlateUnits()) : 0.0f;
    }
}

bool FFMapCamera::TryGetMapBounds(UWorld* World, FBox& OutBounds)
{
    OutBounds = FBox(ForceInit);
    if (!IsValid(World)) return false;

    const UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!Zones) return false;
    if (const AFFZSBuildArea* BuildArea = Zones->FindBuildArea())
    {
        const FBox Bounds = BuildArea->GetBuildBounds();
        if (IsFiniteBox(Bounds) && Bounds.GetSize().X > 1.0 && Bounds.GetSize().Y > 1.0)
        {
            OutBounds = Bounds;
            return true;
        }
    }

    // Registry ownership excludes invalid or unloaded zones. Sky/water background geometry
    // must never enlarge the festival map's extents to the size of the environment sphere.
    for (const FName ZoneId : Zones->GetRegisteredZoneIds())
    {
        const AFFZSGameplayZone* Zone = Zones->FindZone(ZoneId);
        if (!IsValid(Zone)) continue;
        const TArray<FVector2D> Polygon = Zone->GetBoundaryPolygonLocalCm();
        if (Polygon.Num() < 3) continue;
        const FTransform Transform = Zone->GetActorTransform();
        const double HalfHeight = FMath::Max(0.0, static_cast<double>(Zone->VerticalExtentCm) * 0.5);
        for (const FVector2D& Vertex : Polygon)
        {
            const FVector Point = Transform.TransformPosition(FVector(Vertex.X, Vertex.Y, 0.0));
            if (Point.ContainsNaN()) continue;
            OutBounds += Point + FVector(0.0, 0.0, HalfHeight);
            OutBounds += Point - FVector(0.0, 0.0, HalfHeight);
        }
    }
    return IsFiniteBox(OutBounds) && OutBounds.GetSize().X > 1.0 && OutBounds.GetSize().Y > 1.0;
}

bool FFMapCamera::ComputeWholeMapView(const FBox& MapBounds, const FRotator& Rotation,
    const double HorizontalFovDegrees, const double AspectRatio, const FBox2D& SafeNdcBounds,
    FTransform& OutCameraTransform)
{
    if (!IsFiniteBox(MapBounds) || Rotation.ContainsNaN() || !FMath::IsFinite(HorizontalFovDegrees)
        || HorizontalFovDegrees <= 1.0 || HorizontalFovDegrees >= 170.0
        || !FMath::IsFinite(AspectRatio) || AspectRatio <= 0.01 || !SafeNdcBounds.bIsValid
        || SafeNdcBounds.Min.ContainsNaN() || SafeNdcBounds.Max.ContainsNaN()
        || SafeNdcBounds.Min.X < -1.0 || SafeNdcBounds.Min.Y < -1.0
        || SafeNdcBounds.Max.X > 1.0 || SafeNdcBounds.Max.Y > 1.0
        || SafeNdcBounds.GetSize().X < 0.1 || SafeNdcBounds.GetSize().Y < 0.1)
    {
        return false;
    }

    const double TanHalfX = FMath::Tan(FMath::DegreesToRadians(HorizontalFovDegrees * 0.5));
    const double TanHalfY = TanHalfX / AspectRatio;
    const FRotationMatrix Basis(Rotation);
    const FVector Forward = Basis.GetUnitAxis(EAxis::X);
    const FVector Right = Basis.GetUnitAxis(EAxis::Y);
    const FVector Up = Basis.GetUnitAxis(EAxis::Z);
    const FVector Center = MapBounds.GetCenter();
    const FVector2D SafeCenter = SafeNdcBounds.GetCenter();

    double Distance = 100.0;
    for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        const FVector Corner(
            (CornerIndex & 1) ? MapBounds.Max.X : MapBounds.Min.X,
            (CornerIndex & 2) ? MapBounds.Max.Y : MapBounds.Min.Y,
            (CornerIndex & 4) ? MapBounds.Max.Z : MapBounds.Min.Z);
        const FVector Offset = Corner - Center;
        const double Depth = FVector::DotProduct(Offset, Forward);
        const double X = FVector::DotProduct(Offset, Right) / TanHalfX;
        const double Y = FVector::DotProduct(Offset, Up) / TanHalfY;
        Distance = FMath::Max(Distance, 100.0 - Depth);
        Distance = FMath::Max(Distance, (X - SafeNdcBounds.Max.X * Depth) / (SafeNdcBounds.Max.X - SafeCenter.X));
        Distance = FMath::Max(Distance, (SafeNdcBounds.Min.X * Depth - X) / (SafeCenter.X - SafeNdcBounds.Min.X));
        Distance = FMath::Max(Distance, (Y - SafeNdcBounds.Max.Y * Depth) / (SafeNdcBounds.Max.Y - SafeCenter.Y));
        Distance = FMath::Max(Distance, (SafeNdcBounds.Min.Y * Depth - Y) / (SafeCenter.Y - SafeNdcBounds.Min.Y));
    }
    // A small reserve handles floating-point projection and the near-plane difference across RHIs.
    Distance *= 1.02;
    const FVector CameraLocation = Center - Forward * Distance
        - Right * (SafeCenter.X * Distance * TanHalfX)
        - Up * (SafeCenter.Y * Distance * TanHalfY);
    if (CameraLocation.ContainsNaN() || !FMath::IsFinite(Distance)) return false;
    OutCameraTransform = FTransform(Rotation, CameraLocation);
    return true;
}

bool FFMapCamera::ShowWholeMap(APlayerController* Controller)
{
    if (!IsValid(Controller) || !Controller->IsLocalController()) return false;
    IFFMapOverviewBridge* Bridge = IFFMapOverviewBridge::Find();
    if (Bridge && Bridge->IsInputBlocked(Controller->GetWorld())) return false;
    FBox Bounds(ForceInit);
    if (!TryGetMapBounds(Controller->GetWorld(), Bounds)) return false;
    if (AFFMapOverviewViewTarget* Existing = FindOverviewSession(Controller))
    {
        if (Controller->GetViewTarget() == Existing) return Existing->RefreshMapView(Bounds);
        Existing->Destroy();
    }

    FVector CameraLocation;
    FRotator CameraRotation;
    Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);
    CameraRotation.Pitch = -62.0;
    CameraRotation.Roll = 0.0;
    FActorSpawnParameters Spawn;
    Spawn.Owner = Controller;
    Spawn.ObjectFlags |= RF_Transient;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFFMapOverviewViewTarget* View = Controller->GetWorld()->SpawnActor<AFFMapOverviewViewTarget>(Spawn);
    if (!View) return false;
    if (!View->InitializeMapView(Controller, Bounds, CameraRotation))
    {
        View->Destroy();
        return false;
    }
    View->bUsedNativeBridge = Bridge && Bridge->EnterMapView(Controller, View);
    if (!View->bUsedNativeBridge)
    {
        // A controller unsupported by the project adapter retains its original camera/input state.
        if (Bridge)
        {
            View->Destroy();
            return false;
        }
        Controller->SetViewTarget(View);
    }
    return true;
}

bool FFMapCamera::RestoreCamera(APlayerController* Controller)
{
    AFFMapOverviewViewTarget* View = FindOverviewSession(Controller);
    if (!View) return false;
    const bool bRestored = View->RestorePreviousView();
    View->Destroy();
    return bRestored;
}

bool FFMapCamera::IsWholeMapView(const APlayerController* Controller)
{
    const AFFMapOverviewViewTarget* View = IsValid(Controller)
        ? Cast<AFFMapOverviewViewTarget>(Controller->GetViewTarget()) : nullptr;
    return View && View->MapController.Get() == Controller;
}

bool FFMapCamera::FocusMapPoint(APlayerController* Controller, const FVector& WorldPoint)
{
    if (!IsValid(Controller) || !Controller->IsLocalController() || WorldPoint.ContainsNaN()) return false;
    IFFMapOverviewBridge* Bridge = IFFMapOverviewBridge::Find();
    if (!Bridge || Bridge->IsInputBlocked(Controller->GetWorld())) return false;
    if (IsWholeMapView(Controller) && !RestoreCamera(Controller)) return false;
    return Bridge->FocusMapPoint(Controller, WorldPoint);
}

bool FFMapCamera::RefreshWholeMapBounds(APlayerController* Controller, const FBox& Bounds)
{
    if (!IsFiniteBox(Bounds) || !IsWholeMapView(Controller)) return false;
    AFFMapOverviewViewTarget* View = Cast<AFFMapOverviewViewTarget>(Controller->GetViewTarget());
    return View->RefreshMapView(Bounds);
}

AFFMapOverviewViewTarget::AFFMapOverviewViewTarget()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
    SetActorEnableCollision(false);
    bReplicates = false;
    if (UCameraComponent* Camera = GetCameraComponent())
    {
        Camera->ProjectionMode = ECameraProjectionMode::Perspective;
        Camera->FieldOfView = 60.0f;
        Camera->bConstrainAspectRatio = false;
        Camera->bOverrideAspectRatioAxisConstraint = true;
        Camera->SetAspectRatioAxisConstraint(EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV);
        Camera->PostProcessBlendWeight = 0.0f;
    }
}

bool AFFMapOverviewViewTarget::InitializeMapView(APlayerController* Controller, const FBox& Bounds,
    const FRotator& Rotation)
{
    MapController = Controller;
    PreviousViewTarget = Controller->GetViewTarget();
    MapBounds = Bounds;
    MapRotation = Rotation;
    return FitViewport();
}

bool AFFMapOverviewViewTarget::RefreshMapView(const FBox& Bounds)
{
    MapBounds = Bounds;
    return FitViewport();
}

bool AFFMapOverviewViewTarget::FitViewport()
{
    APlayerController* Controller = MapController.Get();
    UCameraComponent* Camera = GetCameraComponent();
    if (!Controller || !Camera) return false;
    int32 Width = 0;
    int32 Height = 0;
    Controller->GetViewportSize(Width, Height);
    if (Width < 64 || Height < 64) return false;
    const double ViewportScale = FMath::Max(0.1f, UWidgetLayoutLibrary::GetViewportScale(GetWorld()));
    const double EdgeReserve = 24.0 * ViewportScale;
    const float PanelRightInset = ReadMapPanelRightInset(Controller);
    // The WBP Designer owns panel width, position and render scale. Leave a bounded amount
    // of visible map even when an edited panel occupies most of a narrow viewport.
    const double RightReserve = FMath::Clamp(PanelRightInset * ViewportScale, EdgeReserve, Width * 0.75);
    const double BottomReserve = FMath::Min(240.0 * ViewportScale, Height * 0.28);
    const FBox2D SafeRect(
        FVector2D(-1.0 + 2.0 * EdgeReserve / Width, -1.0 + 2.0 * BottomReserve / Height),
        FVector2D(1.0 - 2.0 * RightReserve / Width, 1.0 - 2.0 * EdgeReserve / Height));
    FTransform Transform;
    if (!FFMapCamera::ComputeWholeMapView(MapBounds, MapRotation, Camera->FieldOfView,
        static_cast<double>(Width) / Height, SafeRect, Transform)) return false;
    SetActorTransform(Transform);
    Camera->SetAspectRatio(static_cast<float>(Width) / Height);
    FittedViewportSize = FIntPoint(Width, Height);
    FittedViewportScale = static_cast<float>(ViewportScale);
    FittedRightInsetSlateUnits = PanelRightInset;
    return true;
}

bool AFFMapOverviewViewTarget::RestorePreviousView()
{
    if (bRestored) return true;
    bRestored = true;
    APlayerController* Controller = MapController.Get();
    if (!Controller || Controller->GetViewTarget() != this) return false;
    AActor* Previous = PreviousViewTarget.Get();
    if (!Previous) Previous = Controller->GetPawn();
    if (!IsValid(Previous)) return false;
    if (bUsedNativeBridge)
    {
        if (IFFMapOverviewBridge* Bridge = IFFMapOverviewBridge::Find())
        {
            if (Bridge->ExitMapView(Controller, Previous)) return true;
        }
    }
    Controller->SetViewTarget(Previous);
    return true;
}

void AFFMapOverviewViewTarget::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    APlayerController* Controller = MapController.Get();
    if (!Controller || Controller->GetViewTarget() != this)
    {
        // A cinematic/possession changed the view: discard this session without overriding it.
        bRestored = true;
        Destroy();
        return;
    }
    int32 Width = 0;
    int32 Height = 0;
    Controller->GetViewportSize(Width, Height);
    if (FIntPoint(Width, Height) != FittedViewportSize
        || !FMath::IsNearlyEqual(FittedViewportScale, UWidgetLayoutLibrary::GetViewportScale(GetWorld()))
        || !FMath::IsNearlyEqual(FittedRightInsetSlateUnits, ReadMapPanelRightInset(Controller))) FitViewport();
}

void AFFMapOverviewViewTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (EndPlayReason == EEndPlayReason::Destroyed) RestorePreviousView();
    Super::EndPlay(EndPlayReason);
}
