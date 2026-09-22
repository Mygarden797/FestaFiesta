#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildPlacementGizmo.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

void UFF_BuildModeSubsystem::ResetPlacementAssistance()
{
    CancelNearbyPlacementSearch();
    bDirectPlacementAdjustment = false;
    bHasGizmoStartPlacement = false;
    PlacementAssistanceMessage = FText::GetEmpty();
    if (PlacementGizmo) PlacementGizmo->Destroy();
    PlacementGizmo = nullptr;
}

void UFF_BuildModeSubsystem::RefreshPlacementGizmo()
{
    const bool bVisible = bDirectPlacementAdjustment && bHasPlacementTarget && !bFindingNearbyPlacement &&
        !bInputSuspendedForPauseMenu && FFBuildModeModel::IsPreviewActive(State) && PlacementPreview;
    if (!bVisible)
    {
        if (PlacementGizmo) PlacementGizmo->SetGizmoVisible(false);
        return;
    }
    if (!PlacementGizmo && GetWorld())
    {
        FActorSpawnParameters Params;
        Params.ObjectFlags |= RF_Transient;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        PlacementGizmo = GetWorld()->SpawnActor<AFF_BuildPlacementGizmo>(Params);
    }
    if (!PlacementGizmo) return;
    FVector GroundPoint = LastValidation.PlacementTransform.GetLocation(), GroundNormal = FVector::UpVector;
    PlacementPreview->GetPlacementGridSurface(GroundPoint, GroundNormal);
    PlacementGizmo->SetPreview(GetLocalPlayerController(), LastValidation.PlacementTransform,
        SelectedSpec.FootprintSizeCells, GroundPoint, GroundNormal);
    PlacementGizmo->SetGizmoVisible(true);
}

bool UFF_BuildModeSubsystem::HandlePlacementGizmoInput(APlayerController& Controller, const bool bPointerBlocked)
{
    if (!bDirectPlacementAdjustment || bFindingNearbyPlacement || !PlacementGizmo) return false;
    FFF_BuildGizmoPointerInput Input;
    float X = 0, Y = 0;
    const bool bHasPointer = Controller.GetMousePosition(X, Y);
    int32 Width = 0, Height = 0;
    Controller.GetViewportSize(Width, Height);
    Input.ScreenPosition = FVector2D(X, Y);
    Input.bPrimaryPressed = Controller.WasInputKeyJustPressed(EKeys::LeftMouseButton);
    Input.bPrimaryDown = Controller.IsInputKeyDown(EKeys::LeftMouseButton);
    Input.bPrimaryReleased = Controller.WasInputKeyJustReleased(EKeys::LeftMouseButton);
    Input.bBlockedByUi = bPointerBlocked || !bHasPointer ||
        Width <= 0 || Height <= 0 || X < 0 || Y < 0 || X >= Width || Y >= Height;
    Input.bPaused = bInputSuspendedForPauseMenu;
    const EFF_BuildGizmoHandle PreviousHandle = PlacementGizmo->GetActiveHandle();
    const FFF_BuildGizmoDelta Delta = PlacementGizmo->HandlePointer(Input);
    if (Delta.bStarted)
    {
        EndDragPlacement();
        GizmoStartPlacement = CurrentPlacement;
        bHasGizmoStartPlacement = true;
        GizmoStartYawDegrees = GetPlacementYawDegrees();
        PlacementAssistanceMessage = FText::GetEmpty();
    }
    if (Delta.bCancelled && bHasGizmoStartPlacement)
    {
        CurrentPlacement = GizmoStartPlacement;
        bPreviewWorldDirty = true;
    }
    else if (Delta.bChanged)
    {
        const UFFZSWorldSubsystem* Zones = GetZoneSystem();
        const AFFZSGameplayZone* Zone = Zones ? Zones->FindZone(CurrentZoneId) : nullptr;
        if (Zone)
        {
            const double Step = GetPlacementPositionSnapCm();
            const FVector SnappedDelta(FMath::GridSnap(Delta.WorldTranslation.X, Step),
                FMath::GridSnap(Delta.WorldTranslation.Y, Step), 0);
            const FVector LocalDelta = Zone->GetActorTransform().InverseTransformVector(SnappedDelta);
            const double OriginX = GizmoStartPlacement.OriginCell.X + LocalDelta.X;
            const double OriginY = GizmoStartPlacement.OriginCell.Y + LocalDelta.Y;
            if (OriginX >= MIN_int32 && OriginX <= MAX_int32 && OriginY >= MIN_int32 && OriginY <= MAX_int32)
            {
                CurrentPlacement = GizmoStartPlacement;
                CurrentPlacement.OriginCell = FIntPoint(FMath::RoundToInt(OriginX), FMath::RoundToInt(OriginY));
                if (PreviousHandle == EFF_BuildGizmoHandle::RotateYaw)
                {
                    const bool bFree = Controller.IsInputKeyDown(EKeys::LeftShift) || Controller.IsInputKeyDown(EKeys::RightShift);
                    const float Yaw = bFree ? GizmoStartYawDegrees + Delta.YawDegrees
                        : FMath::GridSnap(GizmoStartYawDegrees + Delta.YawDegrees, GetPlacementRotationSnapDegrees());
                    SetPlacementYawDegrees(Yaw);
                }
                bPreviewWorldDirty = true;
            }
        }
    }
    if (Delta.bEnded || Delta.bCancelled)
    {
        bHasGizmoStartPlacement = false;
        // Releasing a handle must never become the start of continuous placement.
        bSuppressWorldClickUntilReleased = true;
        RefreshCurrentPlacementValidation();
    }
    return Delta.bConsumed;
}
