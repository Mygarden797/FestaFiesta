// Mouse strokes produce bounded, deduplicated install commands. No preview actors,
// timers, asset loads or global actor scans are created for individual samples.
#include "Building/FF_BuildModeSubsystem.h"

#include "FF_BuildDragPlacementPath.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"

namespace FFBuildDragPlacement
{
	bool IsSpatialFailure(const EFFZSPlacementFailure Reason)
	{
		switch (Reason)
		{
		case EFFZSPlacementFailure::PlacementCollision:
		case EFFZSPlacementFailure::InsufficientClearance:
		case EFFZSPlacementFailure::QueueFootprintBlocked:
		case EFFZSPlacementFailure::OutsideZone:
		case EFFZSPlacementFailure::ZoneNotFound:
		case EFFZSPlacementFailure::CategoryNotAllowed:
		case EFFZSPlacementFailure::InvalidGround:
		case EFFZSPlacementFailure::ExcessSlope:
		case EFFZSPlacementFailure::HeightVarianceExceeded:
		case EFFZSPlacementFailure::ProtectedRouteBlocked:
		case EFFZSPlacementFailure::RequiredRouteDisconnected:
		case EFFZSPlacementFailure::AccessibilityRouteBlocked:
			return true;
		default:
			return false;
		}
	}
}

struct FFF_BuildDragPlacementSession
{
	static constexpr int32 MaxCellsPerFrame = 8;
	static constexpr int32 MaxStrokeCells = 4096;
	static constexpr int32 MaxPendingTargets = 128;
	static constexpr double FrameBudgetSeconds = 0.002;
	FFF_BuildDragPlacementPath Path;
	TSet<FIntPoint> AttemptedCells;
	FIntPoint Targets[MaxPendingTargets];
	int32 TargetHead = 0;
	int32 TargetCount = 0;
	FIntPoint LastTarget = FIntPoint::ZeroValue;
	FTransform LatticeToWorld;
	FIntPoint Footprint;
	FVector2D PitchCm;
	TWeakObjectPtr<UFFZSFacilityDefinition> Definition;
	FGuid TransactionId = FGuid::NewGuid();
	FName ZoneHint;
	bool bHasShownSpatialFailure = false;
};

bool UFF_BuildModeSubsystem::IsDragPlacementEnabled() const
{
	const UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
	const UFFZSFacilityDefinition* Definition = Catalog
		? Catalog->FindDefinition(SelectedFacilityDefinitionId) : nullptr;
	return State == EFF_BuildModeState::Placing && Definition && Definition->bEnableDragPlacement;
}

void UFF_BuildModeSubsystem::HandlePlacementPrimaryPress(const bool bButtonDown)
{
	// Every press uses the established single-install/move path, including a quick
	// press+release in one frame. A held stroke can also start on an occupied road,
	// skip that already-checked origin and continue into the next available position.
	const bool bCanExtend = bButtonDown && IsDragPlacementEnabled();
	const FName PressedDefinitionId = SelectedFacilityDefinitionId;
	const TWeakObjectPtr<AFFZSPlacementPreviewActor> PressedPreview(PlacementPreview.Get());
	const bool bInstalled = CommitPlacement();
	if (!bCanExtend || !IsDragPlacementEnabled() ||
		SelectedFacilityDefinitionId != PressedDefinitionId || PlacementPreview != PressedPreview.Get())
	{
		return;
	}
	if (!bInstalled)
	{
		// Only fresh spatial validation failures are skippable. Money, limits and
		// execution failures end the press and cannot silently resume after recovery.
		if (LastValidation.bIsValid || LastValidation.Reasons.IsEmpty()) { return; }
		for (const EFFZSPlacementFailure Reason : LastValidation.Reasons)
		{
			if (!FFBuildDragPlacement::IsSpatialFailure(Reason)) { return; }
		}
	}
	// The press already processed the origin. Never buy or reject it a second time.
	BeginDragPlacement(true);
	if (DragPlacementSession && !bInstalled) { DragPlacementSession->bHasShownSpatialFailure = true; }
}

void UFF_BuildModeSubsystem::BeginDragPlacement(const bool bOriginAlreadyProcessed)
{
	EndDragPlacement();
	UFFZSWorldSubsystem* Zones = GetZoneSystem();
	UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
	UFFZSFacilityDefinition* Definition = Catalog ? Catalog->FindDefinition(SelectedFacilityDefinitionId) : nullptr;
	const AFFZSGameplayZone* Zone = Zones ? Zones->FindZone(CurrentZoneId) : nullptr;
	TArray<FText> Errors;
	if (bInputSuspendedForPauseMenu || !IsDragPlacementEnabled() || !bHasPlacementTarget ||
		!Zone || !PlacementPreview || !CanReconfigure() || !Definition || !Definition->IsDefinitionValid(Errors))
	{
		return;
	}

	const float LocalYaw = UFFZSGridLibrary::YawCentidegreesToDegrees(
		UFFZSGridLibrary::GetEffectiveYawCentidegrees(CurrentPlacement));
	const FVector2D Offset = FFBuildModeModel::ResolveRotatedFootprintCenterOffset(
		Definition->Spec.FootprintSizeCells, LocalYaw);
	const FVector2D Center = FVector2D(CurrentPlacement.OriginCell) + Offset;
	DragPlacementSession = MakeShared<FFF_BuildDragPlacementSession>();
	FFF_BuildDragPlacementSession& Stroke = *DragPlacementSession;
	Stroke.Footprint = Definition->Spec.FootprintSizeCells; // Authority cells are 1 cm.
	// Origins are integer centimetres. At arbitrary yaw, independent rounding of
	// two neighbors can lose up to sqrt(2) cm along a separating axis. Derive the
	// guard from that precision, never relax collision rules or expose a magic gap.
	const bool bCardinal = FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(LocalYaw), 90.0f), 0.0001f);
	const double RoundingGuardCm = bCardinal ? 0.0 : UE_SQRT_2;
	Stroke.PitchCm = FVector2D(Stroke.Footprint) + FVector2D(RoundingGuardCm);
	Stroke.Definition = Definition;
	Stroke.ZoneHint = CurrentZoneId;
	Stroke.LatticeToWorld = FTransform(FRotator(0.0, LocalYaw + Zone->GetActorRotation().Yaw, 0.0),
		Zone->GetActorTransform().TransformPosition(FVector(Center, 0.0)));
	Stroke.AttemptedCells.Reserve(256);
	Stroke.Path.Reset(FIntPoint::ZeroValue);
	if (bOriginAlreadyProcessed)
	{
		Stroke.AttemptedCells.Add(FIntPoint::ZeroValue);
	}
	else
	{
		TryInstallDragCell(FIntPoint::ZeroValue);
	}
}

void UFF_BuildModeSubsystem::EndDragPlacement()
{
	DragPlacementSession.Reset();
}

bool UFF_BuildModeSubsystem::ResolveDragCursorCell(const FVector& CursorWorld, FIntPoint& OutCell) const
{
	if (!DragPlacementSession || CursorWorld.ContainsNaN()) { return false; }
	const FFF_BuildDragPlacementSession& Stroke = *DragPlacementSession;
	const FVector Local = Stroke.LatticeToWorld.InverseTransformPosition(CursorWorld);
	const double X = Local.X / Stroke.PitchCm.X;
	const double Y = Local.Y / Stroke.PitchCm.Y;
	if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) ||
		X <= MIN_int32 || X >= MAX_int32 || Y <= MIN_int32 || Y >= MAX_int32) { return false; }
	OutCell = FIntPoint(FMath::RoundToInt(X), FMath::RoundToInt(Y));
	return true;
}

void UFF_BuildModeSubsystem::UpdateDragPlacement(const FVector& CursorWorld)
{
	FIntPoint Target;
	if (!ResolveDragCursorCell(CursorWorld, Target)) { EndDragPlacement(); return; }
	FFF_BuildDragPlacementSession& Stroke = *DragPlacementSession;
	if (Target == Stroke.LastTarget) { return; }
	// Retain turns while processing catches up. Never silently replace a queued bend
	// with a straight shortcut across land that the player did not paint.
	if (Stroke.TargetCount == Stroke.MaxPendingTargets)
	{
		EndDragPlacement();
		ShowStatus(TEXT("드래그가 너무 빨라 설치를 멈췄습니다. 버튼을 놓고 다시 이어 주세요."), true);
		return;
	}
	Stroke.Targets[(Stroke.TargetHead + Stroke.TargetCount) % Stroke.MaxPendingTargets] = Target;
	++Stroke.TargetCount;
	Stroke.LastTarget = Target;
}

bool UFF_BuildModeSubsystem::ResolveDragPlacement(const FIntPoint& Cell, FName& OutZoneId,
	FFFZSGridPlacement& OutPlacement) const
{
	const UFFZSWorldSubsystem* Zones = GetZoneSystem();
	if (!DragPlacementSession || !Zones) { return false; }
	const FFF_BuildDragPlacementSession& Stroke = *DragPlacementSession;
	const FVector CenterWorld = Stroke.LatticeToWorld.TransformPosition(FVector(
		static_cast<double>(Cell.X) * Stroke.PitchCm.X,
		static_cast<double>(Cell.Y) * Stroke.PitchCm.Y, 0.0));
	OutZoneId = Zones->ResolveZoneAtWorldLocation(CenterWorld, Stroke.ZoneHint);
	const AFFZSGameplayZone* Zone = Zones->FindZone(OutZoneId);
	if (!Zone) { return false; }
	const FVector CenterLocal = Zone->GetActorTransform().InverseTransformPosition(CenterWorld);
	OutPlacement = FFFZSGridPlacement{};
	OutPlacement.YawCentidegrees = UFFZSGridLibrary::DegreesToYawCentidegrees(
		Stroke.LatticeToWorld.Rotator().Yaw - Zone->GetActorRotation().Yaw);
	const FVector2D Offset = FFBuildModeModel::ResolveRotatedFootprintCenterOffset(Stroke.Footprint,
		UFFZSGridLibrary::YawCentidegreesToDegrees(OutPlacement.YawCentidegrees));
	const FVector2D Origin = FVector2D(CenterLocal.X, CenterLocal.Y) - Offset;
	if (Origin.X <= MIN_int32 || Origin.X >= MAX_int32 ||
		Origin.Y <= MIN_int32 || Origin.Y >= MAX_int32) { return false; }
	OutPlacement.OriginCell = FIntPoint(FMath::RoundToInt(Origin.X), FMath::RoundToInt(Origin.Y));
	return true;
}

bool UFF_BuildModeSubsystem::TryInstallDragCell(const FIntPoint& Cell)
{
	// Keep the session alive across synchronous command events. Subscribers can close
	// the catalog, restore a snapshot, or start another selection inside ExecuteCommand.
	const TSharedPtr<FFF_BuildDragPlacementSession> Stroke = DragPlacementSession;
	UFFZSWorldSubsystem* Zones = GetZoneSystem();
	UFFZSFacilityDefinition* Definition = Stroke ? Stroke->Definition.Get() : nullptr;
	if (!Stroke || !Zones || !Definition || bInputSuspendedForPauseMenu || !IsDragPlacementEnabled() ||
		!CanReconfigure() || Definition->Spec.FootprintSizeCells != Stroke->Footprint)
	{
		EndDragPlacement();
		return false;
	}
	if (Stroke->AttemptedCells.Contains(Cell)) { return false; }
	if (Stroke->AttemptedCells.Num() >= Stroke->MaxStrokeCells)
	{
		EndDragPlacement();
		ShowStatus(TEXT("한 번의 드래그 설치 범위에 도달했습니다. 버튼을 놓고 이어 주세요."));
		return false;
	}
	Stroke->AttemptedCells.Add(Cell);
	FName ZoneId;
	FFFZSGridPlacement Placement;
	if (!ResolveDragPlacement(Cell, ZoneId, Placement)) { return false; }
	FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommandFromDefinition(
		Definition, ZoneId, Placement, Zones->GetWorldRevision());
	Command.TransactionId = Stroke->TransactionId;
	const FFFZSCommandResult Result = Zones->ExecuteCommand(Command);
	if (DragPlacementSession != Stroke || State != EFF_BuildModeState::Placing) { return Result.bSucceeded; }
	bPreviewWorldDirty = true;
	if (Result.bSucceeded)
	{
		Stroke->ZoneHint = ZoneId;
		DismissPlacementFailureFeedback();
		return true;
	}
	const bool bLocalObstacle = FFBuildDragPlacement::IsSpatialFailure(Result.FailureReason);
	if (!bLocalObstacle || !Stroke->bHasShownSpatialFailure)
	{
		PresentPlacementFailure(Result.FailureReason, Result.Message);
		Stroke->bHasShownSpatialFailure = true;
	}
	// Money, instance limits and systemic failures cannot be fixed by moving one tile.
	if (!bLocalObstacle) { EndDragPlacement(); }
	return false;
}

void UFF_BuildModeSubsystem::ProcessDragPlacement()
{
	const double Deadline = FPlatformTime::Seconds() + FFF_BuildDragPlacementSession::FrameBudgetSeconds;
	for (int32 Count = 0; DragPlacementSession && Count < FFF_BuildDragPlacementSession::MaxCellsPerFrame; ++Count)
	{
		FFF_BuildDragPlacementSession& Stroke = *DragPlacementSession;
		if (!Stroke.Path.HasPendingCells())
		{
			if (Stroke.TargetCount == 0) { break; }
			Stroke.Path.SetTarget(Stroke.Targets[Stroke.TargetHead]);
			Stroke.TargetHead = (Stroke.TargetHead + 1) % Stroke.MaxPendingTargets;
			--Stroke.TargetCount;
		}
		FIntPoint Cell;
		if (Stroke.Path.TryStep(Cell)) { TryInstallDragCell(Cell); }
		// One authority command is atomic; the time budget is a soft bound between commands.
		if (FPlatformTime::Seconds() >= Deadline) { break; }
	}
}

void UFF_BuildModeSubsystem::RefreshDragPlacementPreview(const bool bForceRefresh)
{
	if (!DragPlacementSession || !PlacementPreview) { return; }
	FName NewZoneId;
	FFFZSGridPlacement NewPlacement;
	const bool bNewTarget = ResolveDragPlacement(DragPlacementSession->LastTarget, NewZoneId, NewPlacement);
	const bool bChanged = bNewTarget != bHasPlacementTarget || NewZoneId != CurrentZoneId ||
		!(NewPlacement == CurrentPlacement);
	bHasPlacementTarget = bNewTarget;
	if (bNewTarget)
	{
		CurrentZoneId = NewZoneId;
		CurrentPlacement = NewPlacement;
	}
	PlacementPreview->SetActorHiddenInGame(!bHasPlacementTarget);
	if (bHasPlacementTarget && (bChanged || bForceRefresh ||
		FPlatformTime::Seconds() - LastPreviewValidationTimeSeconds >= 0.5))
	{
		RefreshCurrentPlacementValidation();
	}
}
