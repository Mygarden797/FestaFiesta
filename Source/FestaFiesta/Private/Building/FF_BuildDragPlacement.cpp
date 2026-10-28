// FF_BuildDragPlacement.cpp
// 역할: 드래그 경로를 반투명 미리보기로 표시하고 버튼을 놓을 때 한 번에 설치한다.

// Holding creates only bounded preview data. Two instanced meshes represent the entire
// stroke; the authority receives one atomic batch only after the real pointer release.
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/FF_BuildingSettings.h"

#include "FF_BuildDragPlacementPath.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "FF_FFZSBuildPresentationSettings.h"
#include "FF_FFZSBuildRangeComponent.h"

enum class EFFBuildDragPreviewState : uint8
{
	Pending,
	Valid,
	Invalid
};

struct FFF_BuildDragPlacementSession
{
	// Matches ExecuteConstructionBatch's public atomic-edit limit.
	static constexpr int32 MaxStrokeCells = 128;
	static constexpr int32 MaxPendingTargets = 128;
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
	FFFZSFacilitySpec DefinitionSpec;
	TArray<FFFZSCommand> Commands;
	TArray<int32> CommandPreviewIndices;
	int32 ValidationIndex = 0;
	TArray<FFFZSCommand> PreviewAcceptedCommands;
	int64 PreviewValidationRevision = INDEX_NONE;
	double NextPreviewRevalidationTime = 0.0;
	TArray<FTransform> PreviewTransforms;
	TArray<EFFBuildDragPreviewState> PreviewStates;
	TWeakObjectPtr<AActor> GhostActor;
	TWeakObjectPtr<UInstancedStaticMeshComponent> GhostInstances;
	TWeakObjectPtr<UInstancedStaticMeshComponent> InvalidGhostInstances;
	TWeakObjectPtr<UMaterialInstanceDynamic> InvalidGhostMaterial;
	TWeakObjectPtr<ULineBatchComponent> GhostOutlines;
	FTransform VisualRelativeTransform;
	int32 RenderedCount = 0;
	FGuid TransactionId = FGuid::NewGuid();
	FName ZoneHint;
	EFFZSQueueLayout QueueLayout = EFFZSQueueLayout::Authored;
	FFFZSQueueSettings QueueSettings;
};

namespace FFBuildDragPlacement
{
	FTransform VisibleInstanceTransform(const FFF_BuildDragPlacementSession& Stroke, int32 PreviewIndex, bool bInvalidLayer)
	{
		FTransform Transform = Stroke.VisualRelativeTransform * Stroke.PreviewTransforms[PreviewIndex];
		if ((Stroke.PreviewStates[PreviewIndex] == EFFBuildDragPreviewState::Invalid) != bInvalidLayer)
			Transform.SetScale3D(FVector::ZeroVector);
		return Transform;
	}

	void UpdateGhostInstance(FFF_BuildDragPlacementSession& Stroke, int32 PreviewIndex)
	{
		if (PreviewIndex >= Stroke.RenderedCount) return;
		if (UInstancedStaticMeshComponent* Instances = Stroke.GhostInstances.Get())
			Instances->UpdateInstanceTransform(PreviewIndex, VisibleInstanceTransform(Stroke, PreviewIndex, false), true, true, true);
		if (UInstancedStaticMeshComponent* InvalidInstances = Stroke.InvalidGhostInstances.Get())
			InvalidInstances->UpdateInstanceTransform(PreviewIndex, VisibleInstanceTransform(Stroke, PreviewIndex, true), true, true, true);
	}

	void RefreshGhosts(FFF_BuildDragPlacementSession& Stroke, AFFZSPlacementPreviewActor* Source)
	{
		UInstancedStaticMeshComponent* Instances = Stroke.GhostInstances.Get();
		UInstancedStaticMeshComponent* InvalidInstances = Stroke.InvalidGhostInstances.Get();
		UStaticMeshComponent* Mesh = Source ? Source->PreviewMesh.Get() : nullptr;
		if (!Instances || !InvalidInstances || !Mesh || !Mesh->GetStaticMesh()) return;
		const FTransform Relative = Mesh->GetRelativeTransform();
		if (Instances->GetStaticMesh() != Mesh->GetStaticMesh() || !Relative.Equals(Stroke.VisualRelativeTransform))
		{
			Instances->SetStaticMesh(Mesh->GetStaticMesh());
			InvalidInstances->SetStaticMesh(Mesh->GetStaticMesh());
			Instances->ClearInstances();
			InvalidInstances->ClearInstances();
			Stroke.RenderedCount = 0;
			Stroke.VisualRelativeTransform = Relative;
		}
		for (int32 SlotIndex = 0; SlotIndex < Mesh->GetNumMaterials(); ++SlotIndex)
		{
			if (Instances->GetMaterial(SlotIndex) != Mesh->GetMaterial(SlotIndex))
				Instances->SetMaterial(SlotIndex, Mesh->GetMaterial(SlotIndex));
			if (UMaterialInstanceDynamic* InvalidMaterial = Stroke.InvalidGhostMaterial.Get();
				InvalidMaterial && InvalidInstances->GetMaterial(SlotIndex) != InvalidMaterial)
				InvalidInstances->SetMaterial(SlotIndex, InvalidMaterial);
		}
		for (; Stroke.RenderedCount < Stroke.PreviewTransforms.Num(); ++Stroke.RenderedCount)
		{
			// Stable paired indices avoid instance-removal swaps while validation catches up.
			Instances->AddInstance(VisibleInstanceTransform(Stroke, Stroke.RenderedCount, false), true);
			InvalidInstances->AddInstance(VisibleInstanceTransform(Stroke, Stroke.RenderedCount, true), true);
		}
		// The endpoint is already one of the staged instances. Keep its range feedback,
		// but avoid drawing two overlapping transparent copies of the same facility.
		Mesh->SetVisibility(false);
	}

	void AppendOutline(FFF_BuildDragPlacementSession& Stroke, int32 PreviewIndex,
		const FTransform& Transform, bool bValid, bool bPending = false)
	{
		ULineBatchComponent* Lines = Stroke.GhostOutlines.Get();
		if (!Lines) return;
		const UFFZSBuildPresentationSettings& Style = *GetDefault<UFFZSBuildPresentationSettings>();
		const uint32 BatchId = static_cast<uint32>(PreviewIndex + 1);
		Lines->ClearBatch(BatchId);
		const FLinearColor Color = bPending ? FLinearColor(0.72f, 0.84f, 1.0f)
			: (bValid ? Style.ValidPlacementColor : Style.InvalidPlacementColor);
		const FVector Half(Stroke.Footprint.X * 0.5, Stroke.Footprint.Y * 0.5, 0.0);
		const FVector Corners[] = {
			FVector(-Half.X, -Half.Y, 2.0), FVector(Half.X, -Half.Y, 2.0),
			FVector(Half.X, Half.Y, 2.0), FVector(-Half.X, Half.Y, 2.0)};
		for (int32 Index = 0; Index < 4; ++Index)
			Lines->DrawLine(Transform.TransformPosition(Corners[Index]),
				Transform.TransformPosition(Corners[(Index + 1) % 4]),
				Color, 0, 2.0f, -1.0f, BatchId);
	}
}

bool UFF_BuildModeSubsystem::IsDragPlacementEnabled() const
{
	const UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
	const UFFZSFacilityDefinition* Definition = Catalog
		? Catalog->FindDefinition(SelectedFacilityDefinitionId) : nullptr;
	return State == EFF_BuildModeState::Placing && Definition && Definition->bEnableDragPlacement;
}

void UFF_BuildModeSubsystem::SetDragPlacementGapCm(const int32 GapCm)
{
	if (bInputSuspendedForPauseMenu || bIsExitingBuildMode) return;
	const int32 NewGap = FMath::Clamp(GapCm, 0, MaxDragPlacementGapCm);
	if (DragPlacementGapCm == NewGap) return;
	NotifyBuildToolsInteraction();
	DragPlacementGapCm = NewGap;
	RefreshBuildModeGuide();
	OnBuildToolsChanged.Broadcast();
	ShowStatus(FString::Printf(TEXT("시설 사이 간격 %dcm · 다음 드래그부터 적용됩니다."), NewGap));
}

void UFF_BuildModeSubsystem::HandlePlacementPrimaryPress(const bool bButtonDown)
{
	EndDragPlacement();
	// A same-frame click has already been released and can use the single-install
	// path. A held press must never spend money or create an authoritative facility.
	if (!bButtonDown || !IsDragPlacementEnabled())
	{
		CommitPlacement();
		return;
	}
	if (!bDirectPlacementAdjustment) UpdatePlacementFromCursor(true);
	BeginDragPlacement();
}

void UFF_BuildModeSubsystem::BeginDragPlacement(const bool bOriginAlreadyProcessed, FGuid TransactionId)
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
	// This token groups the future batch only. Do not register an authority stroke
	// while the player is still reviewing a purely local preview.
	if (!TransactionId.IsValid()) TransactionId = FGuid::NewGuid();

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
	Stroke.PitchCm = FVector2D(Stroke.Footprint) + FVector2D(RoundingGuardCm + GetDragPlacementGapCm());
	Stroke.TransactionId = TransactionId;
	Stroke.Definition = Definition;
	Stroke.DefinitionSpec = Definition->Spec;
	Stroke.QueueLayout = PendingQueueLayout;
	Stroke.QueueSettings = PendingQueueSettings;
	Stroke.ZoneHint = CurrentZoneId;
	Stroke.LatticeToWorld = FTransform(FRotator(0.0, LocalYaw + Zone->GetActorRotation().Yaw, 0.0),
		Zone->GetActorTransform().TransformPosition(FVector(Center, 0.0)));
	Stroke.AttemptedCells.Reserve(Stroke.MaxStrokeCells);
	Stroke.Commands.Reserve(Stroke.MaxStrokeCells);
	Stroke.CommandPreviewIndices.Reserve(Stroke.MaxStrokeCells);
	Stroke.PreviewAcceptedCommands.Reserve(Stroke.MaxStrokeCells);
	Stroke.PreviewTransforms.Reserve(Stroke.MaxStrokeCells);
	Stroke.PreviewStates.Reserve(Stroke.MaxStrokeCells);
	Stroke.Path.Reset(FIntPoint::ZeroValue);
	UWorld* World = GetWorld();
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Ghost = World ? World->SpawnActor<AActor>(Spawn) : nullptr;
	if (!Ghost) { EndDragPlacement(); return; }
	Stroke.GhostActor = Ghost;
	UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(Ghost, NAME_None, RF_Transient);
	Ghost->AddInstanceComponent(Instances);
	Ghost->SetRootComponent(Instances);
	Instances->SetMobility(EComponentMobility::Movable);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetGenerateOverlapEvents(false);
	Instances->SetCanEverAffectNavigation(false);
	Instances->SetCastShadow(false);
	Instances->SetReceivesDecals(false);
	Instances->bDisallowNanite = true;
	Instances->RegisterComponent();
	Stroke.GhostInstances = Instances;
	UInstancedStaticMeshComponent* InvalidInstances = NewObject<UInstancedStaticMeshComponent>(Ghost, NAME_None, RF_Transient);
	Ghost->AddInstanceComponent(InvalidInstances);
	InvalidInstances->SetupAttachment(Instances);
	InvalidInstances->SetMobility(EComponentMobility::Movable);
	InvalidInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InvalidInstances->SetGenerateOverlapEvents(false);
	InvalidInstances->SetCanEverAffectNavigation(false);
	InvalidInstances->SetCastShadow(false);
	InvalidInstances->SetReceivesDecals(false);
	InvalidInstances->bDisallowNanite = true;
	const UFFZSBuildPresentationSettings& Style = *GetDefault<UFFZSBuildPresentationSettings>();
	const TSoftObjectPtr<UMaterialInterface>& InvalidMaterialReference = PlacementPreview->PlacementFeedbackMaterial.IsNull()
		? Style.DefaultPlacementFeedbackMaterial : PlacementPreview->PlacementFeedbackMaterial;
	if (UMaterialInterface* Material = InvalidMaterialReference.LoadSynchronous())
	{
		UMaterialInstanceDynamic* InvalidMaterial = UMaterialInstanceDynamic::Create(Material, InvalidInstances);
		InvalidMaterial->SetVectorParameterValue(TEXT("StatusColor"), Style.InvalidPlacementColor);
		InvalidMaterial->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp(Style.InvalidPlacementOpacity, 0.0f, 1.0f));
		// The component owns this material reference even before the asynchronous mesh arrives.
		InvalidInstances->SetMaterial(0, InvalidMaterial);
		Stroke.InvalidGhostMaterial = InvalidMaterial;
	}
	InvalidInstances->RegisterComponent();
	Stroke.InvalidGhostInstances = InvalidInstances;
	ULineBatchComponent* Outlines = NewObject<UFFZSBuildRangeComponent>(Ghost, NAME_None, RF_Transient);
	Ghost->AddInstanceComponent(Outlines);
	Outlines->SetupAttachment(Instances);
	Outlines->SetCanEverAffectNavigation(false);
	Outlines->RegisterComponent();
	Stroke.GhostOutlines = Outlines;
	// Compatibility flag means a legacy caller has already handled the origin;
	// new input always starts with false so the first facility is staged as well.
	if (bOriginAlreadyProcessed) Stroke.AttemptedCells.Add(FIntPoint::ZeroValue);
	else TryInstallDragCell(FIntPoint::ZeroValue);
	if (DragPlacementSession) FFBuildDragPlacement::RefreshGhosts(*DragPlacementSession, PlacementPreview);
}

void UFF_BuildModeSubsystem::EndDragPlacement()
{
	const TSharedPtr<FFF_BuildDragPlacementSession> Stroke = DragPlacementSession;
	DragPlacementSession.Reset();
	PendingDragPlacementTransactionId.Invalidate();
	if (Stroke)
	{
		if (AActor* Ghost = Stroke->GhostActor.Get()) Ghost->Destroy();
		if (PlacementPreview && PlacementPreview->PreviewMesh) PlacementPreview->PreviewMesh->SetVisibility(true);
	}
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
	// A straight stroke can produce hundreds of cursor samples while a single
	// installation is expensive. Extending its queued endpoint preserves every
	// rasterized cell without letting redundant samples fill the bounded queue.
	if (Stroke.TargetCount > 0)
	{
		const int32 Tail = (Stroke.TargetHead + Stroke.TargetCount - 1) % Stroke.MaxPendingTargets;
		const FIntPoint Previous = Stroke.TargetCount > 1
			? Stroke.Targets[(Stroke.TargetHead + Stroke.TargetCount - 2) % Stroke.MaxPendingTargets]
			: Stroke.Path.GetTargetCell();
		if (FFF_BuildDragPlacementPath::CanExtendCardinalSegment(Previous, Stroke.Targets[Tail], Target))
		{
			Stroke.Targets[Tail] = Target;
			Stroke.LastTarget = Target;
			return;
		}
	}
	// Retain turns while processing catches up. Never silently replace a queued bend
	// with a straight shortcut across land that the player did not paint.
	if (Stroke.TargetCount == Stroke.MaxPendingTargets)
	{
		EndDragPlacement();
		ShowStatus(TEXT("드래그 경로가 너무 복잡해 이번 미리보기를 취소했습니다. 버튼을 놓고 다시 시작해 주세요."), true);
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
	const TSharedPtr<FFF_BuildDragPlacementSession> Stroke = DragPlacementSession;
	UFFZSWorldSubsystem* Zones = GetZoneSystem();
	UFFZSFacilityDefinition* Definition = Stroke ? Stroke->Definition.Get() : nullptr;
	if (!Stroke || !Zones || !Definition || bInputSuspendedForPauseMenu || !IsDragPlacementEnabled() ||
		!CanReconfigure() || !FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(&Definition->Spec, &Stroke->DefinitionSpec, 0))
	{
		EndDragPlacement();
		return false;
	}
	if (Stroke->AttemptedCells.Contains(Cell)) return false;
	if (Stroke->AttemptedCells.Num() >= Stroke->MaxStrokeCells)
	{
		EndDragPlacement();
		ShowStatus(TEXT("한 번에 최대 128개까지 미리보기로 설치할 수 있습니다. 이번 드래그는 취소되었습니다."), true);
		return false;
	}
	Stroke->AttemptedCells.Add(Cell);
	FName ZoneId;
	FFFZSGridPlacement Placement;
	FTransform PreviewTransform;
	const bool bResolved = ResolveDragPlacement(Cell, ZoneId, Placement);
	if (bResolved)
	{
		FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommandFromDefinition(
			Definition, ZoneId, Placement, Zones->GetWorldRevision());
		Command.TransactionId = Stroke->TransactionId;
		Command.FacilitySpec.QueueLayout = Stroke->QueueLayout;
		Command.FacilitySpec.QueueSettings = Stroke->QueueSettings;
		// Produce every visible position immediately. Expensive ground/collision/policy
		// checks run separately under the per-frame budget and correct these transforms.
		PreviewTransform = Zones->BuildPlacementTransform(*Zones->FindZone(ZoneId), Command.FacilitySpec, Placement);
		FVector GroundPoint, GroundNormal;
		if (PlacementPreview && PlacementPreview->GetPlacementGridSurface(GroundPoint, GroundNormal) &&
			GroundNormal.Z > UE_SMALL_NUMBER)
		{
			FVector Location = PreviewTransform.GetLocation();
			Location.Z = GroundPoint.Z - ((Location.X - GroundPoint.X) * GroundNormal.X +
				(Location.Y - GroundPoint.Y) * GroundNormal.Y) / GroundNormal.Z;
			PreviewTransform.SetLocation(Location);
		}
		Stroke->CommandPreviewIndices.Add(Stroke->PreviewTransforms.Num());
		Stroke->Commands.Add(MoveTemp(Command));
		Stroke->ZoneHint = ZoneId;
	}
	else
	{
		// An unresolved position remains visibly red and is excluded from release.
		PreviewTransform = Stroke->LatticeToWorld;
		PreviewTransform.SetLocation(Stroke->LatticeToWorld.TransformPosition(FVector(
			Cell.X * Stroke->PitchCm.X, Cell.Y * Stroke->PitchCm.Y, 0.0)));
	}
	const int32 PreviewIndex = Stroke->PreviewTransforms.Add(PreviewTransform);
	Stroke->PreviewStates.Add(bResolved ? EFFBuildDragPreviewState::Pending : EFFBuildDragPreviewState::Invalid);
	FFBuildDragPlacement::AppendOutline(*Stroke, PreviewIndex, PreviewTransform, false, bResolved);
	FFBuildDragPlacement::RefreshGhosts(*Stroke, PlacementPreview);
	return bResolved;
}

bool UFF_BuildModeSubsystem::CommitDragPlacement()
{
	const TSharedPtr<FFF_BuildDragPlacementSession> Stroke = DragPlacementSession;
	if (!Stroke || bInputSuspendedForPauseMenu || !CanReconfigure() || !IsDragPlacementEnabled())
	{
		EndDragPlacement();
		return false;
	}
	const UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
	const UFFZSFacilityDefinition* Definition = Stroke->Definition.Get();
	if (!Definition || !Catalog || Catalog->FindDefinition(SelectedFacilityDefinitionId) != Definition ||
		!FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(&Definition->Spec, &Stroke->DefinitionSpec, 0))
	{
		EndDragPlacement();
		ShowStatus(TEXT("시설 정보가 변경되어 이번 미리보기를 취소했습니다. 다시 드래그해 주세요."), true);
		return false;
	}
	// Geometry is never deferred behind expensive validation: the input path has
	// already displayed all staged positions. Flush only any final release sample.
	ProcessDragPlacement();
	if (DragPlacementSession != Stroke) return false;
	UFFZSWorldSubsystem* Zones = GetZoneSystem();
	if (!Zones) { EndDragPlacement(); return false; }
	const int64 ReleaseRevision = Zones->GetWorldRevision();
	TArray<FFFZSCommand> Candidates;
	Candidates.Reserve(Stroke->Commands.Num());
	FText FailureMessage = FText::FromString(TEXT("드래그한 범위에 설치 가능한 위치가 없습니다."));
	EFFZSPlacementFailure FailureReason = EFFZSPlacementFailure::OutsideZone;
	for (FFFZSCommand& Command : Stroke->Commands)
	{
		Command.ExpectedWorldRevision = ReleaseRevision;
		const FFFZSPlacementValidation Validation = Zones->ValidatePlacement(Command);
		if (DragPlacementSession != Stroke) return false;
		if (Validation.bIsValid) Candidates.Add(Command);
		else
		{
			if (!Validation.Summary.IsEmpty()) FailureMessage = Validation.Summary;
			if (!Validation.Reasons.IsEmpty()) FailureReason = Validation.Reasons[0];
		}
	}
	TArray<FFFZSCommand> Accepted;
	FText BatchFailure;
	if (!Candidates.IsEmpty())
	{
		// Most strokes are entirely valid. Validate the full candidate set once,
		// including aggregate money/limits and collisions among proposed facilities.
		const bool bWholeBatchValid = Zones->ValidateConstructionBatch(Candidates, BatchFailure);
		if (DragPlacementSession != Stroke) return false;
		if (bWholeBatchValid) Accepted = MoveTemp(Candidates);
		else
		{
			// Keep draw order deterministic when only part of the path is affordable,
			// reaches a facility limit, or overlaps another candidate's queue/clearance.
			Accepted.Reserve(Candidates.Num());
			for (const FFFZSCommand& Candidate : Candidates)
			{
				Accepted.Add(Candidate);
				FText CandidateFailure;
				const bool bCanKeep = Zones->ValidateConstructionBatch(Accepted, CandidateFailure);
				if (DragPlacementSession != Stroke) return false;
				if (!bCanKeep)
				{
					Accepted.Pop();
					if (!CandidateFailure.IsEmpty()) BatchFailure = CandidateFailure;
				}
			}
		}
	}
	if (Accepted.IsEmpty())
	{
		EndDragPlacement();
		if (!BatchFailure.IsEmpty())
		{
			FailureMessage = BatchFailure;
			FailureReason = EFFZSPlacementFailure::ExternalPolicyRejected;
		}
		PresentPlacementFailure(FailureReason, FailureMessage);
		return false;
	}
	const int32 InstalledCount = Accepted.Num();
	const int32 SkippedCount = Stroke->PreviewTransforms.Num() - InstalledCount;
	const FName CommittingDefinitionId = SelectedFacilityDefinitionId;
	const TWeakObjectPtr<AFFZSPlacementPreviewActor> CommittingPreview(PlacementPreview.Get());
	EndDragPlacement();
	// Authority revalidates every target and aggregate cost/limits, rolls back any
	// failure and stores precisely one construction history entry for this batch.
	const FFFZSCommandResult Result = Zones->ExecuteConstructionBatch(Accepted);
	if (State != EFF_BuildModeState::Placing || SelectedFacilityDefinitionId != CommittingDefinitionId ||
		PlacementPreview != CommittingPreview.Get()) return Result.bSucceeded;
	bPreviewWorldDirty = true;
	ResetPlacementAssistance();
	UpdatePlacementFromCursor(true);
	if (Result.bSucceeded)
	{
		DismissPlacementFailureFeedback();
		ShowStatus(SkippedCount > 0
			? FString::Printf(TEXT("시설 %d개 설치 · 설치 불가 %d개 건너뜀 · Ctrl+Z로 이번 설치 실행 취소"), InstalledCount, SkippedCount)
			: FString::Printf(TEXT("시설 %d개 설치 완료 · Ctrl+Z로 이번 드래그 전체 실행 취소"), InstalledCount));
	}
	else PresentPlacementFailure(Result.FailureReason, Result.Message);
	return Result.bSucceeded;
}

void UFF_BuildModeSubsystem::ProcessDragPlacement()
{
	const TSharedPtr<FFF_BuildDragPlacementSession> Stroke = DragPlacementSession;
	if (!Stroke) return;
	// Cheap geometry follows the full input path in this frame. The candidate cap
	// bounds allocations, while the step cap also bounds repeated retracing work.
	constexpr int32 MaxPathSteps = FFF_BuildDragPlacementSession::MaxStrokeCells *
		FFF_BuildDragPlacementSession::MaxPendingTargets;
	for (int32 Step = 0; DragPlacementSession == Stroke && Step < MaxPathSteps; ++Step)
	{
		if (!Stroke->Path.HasPendingCells())
		{
			if (Stroke->TargetCount == 0) break;
			Stroke->Path.SetTarget(Stroke->Targets[Stroke->TargetHead]);
			Stroke->TargetHead = (Stroke->TargetHead + 1) % Stroke->MaxPendingTargets;
			--Stroke->TargetCount;
		}
		FIntPoint Cell;
		if (Stroke->Path.TryStep(Cell)) TryInstallDragCell(Cell);
	}
	if (DragPlacementSession != Stroke) return;
	if (Stroke->Path.HasPendingCells() || Stroke->TargetCount > 0)
	{
		EndDragPlacement();
		ShowStatus(TEXT("드래그 범위가 너무 길어 취소되었습니다. 범위를 나누어 설치해 주세요."), true);
		return;
	}
	FFBuildDragPlacement::RefreshGhosts(*Stroke, PlacementPreview);
	UFFZSWorldSubsystem* Zones = GetZoneSystem();
	if (!Zones) { EndDragPlacement(); return; }
	const double ValidationStartTime = FPlatformTime::Seconds();
	const int64 PreviewRevision = Zones->GetWorldRevision();
	if (Stroke->PreviewValidationRevision != PreviewRevision ||
		(Stroke->ValidationIndex >= Stroke->Commands.Num() && ValidationStartTime >= Stroke->NextPreviewRevalidationTime))
	{
		// Registry changes refresh immediately; money/policy changes can happen without
		// a placement revision, so completed cycles also refresh twice per second.
		// Retain old colors until each replacement result arrives to avoid flickering.
		Stroke->ValidationIndex = 0;
		Stroke->PreviewAcceptedCommands.Reset();
		Stroke->PreviewValidationRevision = PreviewRevision;
		Stroke->NextPreviewRevalidationTime = ValidationStartTime + 0.5;
	}
	const UFF_BuildingSettings* Settings = GetDefault<UFF_BuildingSettings>();
	const double Deadline = ValidationStartTime + Settings->GetWorkBudgetSeconds();
	const int32 MaxValidationsPerFrame = Settings->GetDragPlacementsPerFrame();
	for (int32 Count = 0; Count < MaxValidationsPerFrame && Stroke->ValidationIndex < Stroke->Commands.Num(); ++Count)
	{
		const int32 CommandIndex = Stroke->ValidationIndex++;
		FFFZSCommand& Command = Stroke->Commands[CommandIndex];
		Command.ExpectedWorldRevision = Zones->GetWorldRevision();
		const FFFZSPlacementValidation Validation = Zones->ValidatePlacement(Command);
		if (DragPlacementSession != Stroke) return;
		bool bAcceptedForPreview = Validation.bIsValid;
		if (bAcceptedForPreview)
		{
			// Match release's draw-order selection: a clear location can still exceed
			// the combined budget/limit or overlap a previous candidate's queue space.
			Stroke->PreviewAcceptedCommands.Add(Command);
			FText BatchFailure;
			bAcceptedForPreview = Zones->ValidateConstructionBatch(Stroke->PreviewAcceptedCommands, BatchFailure);
			if (DragPlacementSession != Stroke) return;
			if (!bAcceptedForPreview) Stroke->PreviewAcceptedCommands.Pop();
		}
		const int32 PreviewIndex = Stroke->CommandPreviewIndices[CommandIndex];
		// Early definition/zone failures leave an identity transform. Preserve the
		// cursor-derived pose so a rejected ghost never jumps to the world origin.
		if (!Validation.FootprintOrientedRects.IsEmpty() && !Validation.PlacementTransform.ContainsNaN())
			Stroke->PreviewTransforms[PreviewIndex] = Validation.PlacementTransform;
		Stroke->PreviewStates[PreviewIndex] = bAcceptedForPreview ? EFFBuildDragPreviewState::Valid : EFFBuildDragPreviewState::Invalid;
		FFBuildDragPlacement::UpdateGhostInstance(*Stroke, PreviewIndex);
		FFBuildDragPlacement::AppendOutline(*Stroke, PreviewIndex, Stroke->PreviewTransforms[PreviewIndex], bAcceptedForPreview);
		if (Stroke->ValidationIndex >= Stroke->Commands.Num())
			Stroke->NextPreviewRevalidationTime = FPlatformTime::Seconds() + 0.5;
		if (FPlatformTime::Seconds() >= Deadline) break;
	}
}

void UFF_BuildModeSubsystem::RefreshDragPlacementPreview(const bool bForceRefresh, const bool bValidate,
	const FHitResult* GroundHit)
{
	if (!DragPlacementSession || !PlacementPreview) { return; }
	FFBuildDragPlacement::RefreshGhosts(*DragPlacementSession, PlacementPreview);
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
	if (!bHasPlacementTarget) { return; }
	// CurrentPlacement follows input at frame rate; the command still identifies the
	// last fully validated target. Comparing only CurrentPlacement would suppress the
	// later periodic validation after the lightweight presentation has already moved.
	const bool bNeedsValidation = bChanged || PlacementPreview->HasUnvalidatedPreviewTransform() ||
		PlacementPreview->PreviewCommand.ZoneId != CurrentZoneId ||
		!(PlacementPreview->PreviewCommand.Placement == CurrentPlacement);
	if (bValidate && (bNeedsValidation || bForceRefresh ||
		FPlatformTime::Seconds() - LastPreviewValidationTimeSeconds >= 0.5))
	{
		RefreshCurrentPlacementValidation();
		if (DragPlacementSession) FFBuildDragPlacement::RefreshGhosts(*DragPlacementSession, PlacementPreview);
		return;
	}
	if (!bValidate && (bChanged || (!PlacementPreview->HasUnvalidatedPreviewTransform() &&
		(bNeedsValidation || bPreviewWorldDirty))))
	{
		const UFFZSWorldSubsystem* Zones = GetZoneSystem();
		const AFFZSGameplayZone* Zone = Zones ? Zones->FindZone(CurrentZoneId) : nullptr;
		if (!Zone) { return; }
		FTransform TargetTransform = Zones->BuildPlacementTransform(*Zone, SelectedSpec, CurrentPlacement);
		FVector GroundPoint, GroundNormal;
		if (PlacementPreview->GetPlacementGridSurface(GroundPoint, GroundNormal) && GroundNormal.Z > UE_SMALL_NUMBER)
		{
			const AFFZSFacilityActor* Surface = GroundHit ? Cast<AFFZSFacilityActor>(GroundHit->GetActor()) : nullptr;
			if (SelectedSpec.IsGroundSurface() && Surface && Surface->GetFacilitySpec().IsGroundSurface() &&
				Zones->FindFacility(Surface->GetFacilityInstanceId()) == Surface)
			{
				// Cursor traces can hit a pavement mesh's top. Authority ground checks ignore
				// that surface; use its installed base to avoid bouncing by its visual height
				// on the next validation. Keep the actual grid surface unchanged.
				GroundPoint.Z = Surface->GetActorLocation().Z;
				GroundNormal = FVector::UpVector;
			}
			// Project the snapped center onto the traced floor plane. This is visual only;
			// authoritative ground sampling still supplies the final placement height.
			FVector Location = TargetTransform.GetLocation();
			Location.Z = GroundPoint.Z - ((Location.X - GroundPoint.X) * GroundNormal.X +
				(Location.Y - GroundPoint.Y) * GroundNormal.Y) / GroundNormal.Z;
			TargetTransform.SetLocation(Location);
		}
		PlacementPreview->SetUnvalidatedPreviewTransform(TargetTransform);
	}
}
