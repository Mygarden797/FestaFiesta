// Group edits keep their originals intact until the authority accepts the whole batch.
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildMultiSelectionWidget.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "FFBuildGroupEditing"

namespace
{
	const FString LayoutSlot = TEXT("FestaFiesta_BuildLayouts_v1");
	FVector RotateOffset(const FVector& Offset, const float Degrees)
	{
		return FQuat(FVector::UpVector, FMath::DegreesToRadians(Degrees)).RotateVector(Offset);
	}
}

void UFF_BuildModeSubsystem::SetGroupEditingStatus(const FText& Message, const bool bError)
{
	GroupEditingStatus = Message;
	OnBuildMultiSelectionChanged.Broadcast();
	if (bError) ShowStatus(Message.ToString(), true, 3.f);
}

bool UFF_BuildModeSubsystem::CaptureSelectedGroup(TArray<FFF_BuildLayoutEntry>& OutEntries,
	FVector& OutCenter) const
{
	const TArray<AFFZSFacilityActor*> Selected = GetMultiSelectedFacilities();
	OutEntries.Reset();
	OutCenter = FVector::ZeroVector;
	if (Selected.IsEmpty() || Selected.Num() > UFF_BuildLayoutPresetSave::MaxFacilities) return false;
	for (const AFFZSFacilityActor* Facility : Selected) OutCenter += Facility->GetActorLocation();
	OutCenter /= Selected.Num();
	for (const AFFZSFacilityActor* Facility : Selected)
	{
		FFF_BuildLayoutEntry& Entry = OutEntries.AddDefaulted_GetRef();
		Entry.Spec = Facility->GetBaseFacilitySpec();
		Entry.RelativeCenter = Facility->GetActorLocation() - OutCenter;
		Entry.RelativeCenter.Z = 0.0;
		Entry.WorldYawDegrees = Facility->GetActorRotation().Yaw;
	}
	return true;
}

bool UFF_BuildModeSubsystem::StartGroupPlacement(const bool bDuplicate)
{
	if (!bMultiSelectionActive || State != EFF_BuildModeState::BuildMode ||
		bInputSuspendedForPauseMenu || !CanReconfigure()) return false;
	CancelGroupPlacement();
	if (!CaptureSelectedGroup(GroupEntries, GroupTargetCenter))
	{
		SetGroupEditingStatus(LOCTEXT("SelectGroup", "시설을 1~128개 선택하세요."), true);
		return false;
	}
	bGroupDuplicate = bDuplicate;
	GroupSourceIds.Reset();
	for (const AFFZSFacilityActor* Facility : GetMultiSelectedFacilities())
		GroupSourceIds.Add(bDuplicate ? FGuid::NewGuid() : Facility->GetFacilityInstanceId());
	GroupSourceRevision = GetZoneSystem()->GetWorldRevision();
	GroupRotationDegrees = 0.f;
	bMultiSelectionPointerArmed = false;
	bMultiSelectionMarqueeVisible = false;
	if (MultiSelectionWidget) MultiSelectionWidget->SetMarquee(false, FVector2D::ZeroVector, FVector2D::ZeroVector);
	CreateGroupPreviews();
	NotifyBuildToolsInteraction();
	RefreshGroupPlacement(GroupTargetCenter);
	return true;
}

bool UFF_BuildModeSubsystem::BeginGroupMove() { return StartGroupPlacement(false); }
bool UFF_BuildModeSubsystem::BeginGroupDuplicate() { return StartGroupPlacement(true); }

void UFF_BuildModeSubsystem::CreateGroupPreviews()
{
	if (!GetWorld()) return;
	for (int32 Index = 0; Index < GroupEntries.Num(); ++Index)
	{
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AFFZSPlacementPreviewActor* Preview = GetWorld()->SpawnActor<AFFZSPlacementPreviewActor>(Spawn);
		if (Preview) Preview->SetAutoRefreshEnabled(false);
		GroupPreviews.Add(Preview);
	}
}

void UFF_BuildModeSubsystem::CancelGroupPlacement()
{
	const bool bWasActive = IsGroupPlacementActive();
	for (AFFZSPlacementPreviewActor* Preview : GroupPreviews) if (IsValid(Preview)) Preview->Destroy();
	GroupPreviews.Reset();
	GroupEntries.Reset();
	GroupSourceIds.Reset();
	GroupCommands.Reset();
	bGroupHasTarget = false;
	bGroupTargetValid = false;
	GroupRotationDegrees = 0.f;
	NextGroupPreviewTime = 0.0;
	if (bWasActive) GroupEditingStatus = LOCTEXT("PreviewEnded", "묶음 미리보기를 종료했습니다.");
	OnBuildMultiSelectionChanged.Broadcast();
}

bool UFF_BuildModeSubsystem::RefreshGroupPlacement(const FVector& TargetCenter)
{
	UFFZSWorldSubsystem* Zones = GetZoneSystem();
	if (!Zones || !IsGroupPlacementActive() || TargetCenter.ContainsNaN()) return false;
	GroupCommands.Reset();
	GroupTargetCenter = TargetCenter;
	const double Snap = GetPlacementPositionSnapCm();
	GroupTargetCenter.X = FMath::GridSnap(GroupTargetCenter.X, Snap);
	GroupTargetCenter.Y = FMath::GridSnap(GroupTargetCenter.Y, Snap);
	bGroupHasTarget = true;
	const FGuid TransactionId = FGuid::NewGuid();
	TArray<FFFZSFacilitySpec> EffectiveSpecs;
	for (int32 Index = 0; Index < GroupEntries.Num(); ++Index)
	{
		const FFF_BuildLayoutEntry& Entry = GroupEntries[Index];
		const AFFZSFacilityActor* Source = bGroupDuplicate ? nullptr : Zones->FindFacility(GroupSourceIds[Index]);
		if (!bGroupDuplicate && !Source) { bGroupHasTarget = false; break; }
		const FFFZSFacilitySpec Effective = Source ? Source->GetFacilitySpec()
			: FFZSFacilityUpgradeRules::BuildEffectiveSpec(Entry.Spec, 1, 1.f);
		const FVector Center = GroupTargetCenter + RotateOffset(Entry.RelativeCenter, GroupRotationDegrees);
		const FName ZoneId = Zones->ResolveZoneAtWorldLocation(Center);
		const AFFZSGameplayZone* Zone = Zones->FindZone(ZoneId);
		if (!Zone) { bGroupHasTarget = false; break; }
		FFFZSCommand& Command = GroupCommands.AddDefaulted_GetRef();
		Command.CommandId = FGuid::NewGuid();
		Command.TransactionId = TransactionId;
		Command.Type = bGroupDuplicate ? EFFZSCommandType::Install : EFFZSCommandType::Move;
		Command.FacilityInstanceId = GroupSourceIds[Index];
		Command.FacilitySpec = bGroupDuplicate ? Entry.Spec : Effective;
		Command.ZoneId = ZoneId;
		// Snap the group pivot once; snapping each facility would distort the saved spacing.
		Command.Placement = FFBuildModeModel::ResolveWorldSnappedPlacement(Center,
			Entry.WorldYawDegrees + GroupRotationDegrees, Zone->GetActorTransform(), Effective.FootprintSizeCells, 1);
		Command.ExpectedWorldRevision = Zones->GetWorldRevision();
		EffectiveSpecs.Add(Effective);
	}
	FText Reason;
	bGroupTargetValid = bGroupHasTarget && Zones->ValidateConstructionBatch(GroupCommands, Reason);
	if (!bGroupHasTarget) Reason = LOCTEXT("OutsideZone", "묶음 전체를 건축 가능한 땅 위에 놓아 주세요.");
	for (int32 Index = 0; Index < GroupPreviews.Num(); ++Index)
	{
		AFFZSPlacementPreviewActor* Preview = GroupPreviews[Index];
		if (!Preview) continue;
		Preview->SetActorHiddenInGame(!GroupCommands.IsValidIndex(Index));
		if (!GroupCommands.IsValidIndex(Index)) continue;
		FFFZSCommand DisplayCommand = GroupCommands[Index];
		DisplayCommand.FacilitySpec = EffectiveSpecs[Index];
		FFFZSPlacementValidation Display;
		Display.bIsValid = bGroupTargetValid;
		Display.Summary = Reason;
		Display.PlacementTransform = Zones->BuildPlacementTransform(*Zones->FindZone(DisplayCommand.ZoneId),
			DisplayCommand.FacilitySpec, DisplayCommand.Placement);
		UFFZSGridLibrary::BuildPlacementOrientedRegions(DisplayCommand.FacilitySpec, DisplayCommand.Placement,
			Display.FootprintOrientedRects, Display.ClearanceOrientedRects, Display.QueueOrientedRects, Display.ReservedOrientedRects);
		Preview->ApplyBatchPreviewResult(DisplayCommand, Display);
	}
	const FText Status = bGroupTargetValid
		? LOCTEXT("Ready", "배치 가능 · 클릭/Enter/확정: 적용 · Q/E: 회전 · 우클릭/취소: 돌아가기")
		: Reason;
	if (!GroupEditingStatus.EqualTo(Status)) SetGroupEditingStatus(Status);
	NextGroupPreviewTime = GetWorld()->GetTimeSeconds() + 0.1;
	return bGroupTargetValid;
}

void UFF_BuildModeSubsystem::RotateGroupPlacement(const float DeltaDegrees)
{
	if (!IsGroupPlacementActive() || !FMath::IsFinite(DeltaDegrees) || bInputSuspendedForPauseMenu) return;
	GroupRotationDegrees = FRotator::NormalizeAxis(GroupRotationDegrees + DeltaDegrees);
	RefreshGroupPlacement(GroupTargetCenter);
}

bool UFF_BuildModeSubsystem::HandleGroupPlacementInput(APlayerController& Controller, const bool bBlockedByUi)
{
	if (!IsGroupPlacementActive()) return false;
	if (!CanReconfigure() || !GetZoneSystem() || GetZoneSystem()->GetWorldRevision() != GroupSourceRevision)
	{
		CancelGroupPlacement();
		SetGroupEditingStatus(LOCTEXT("Changed", "시설 상태가 바뀌어 묶음 미리보기를 취소했습니다. 다시 선택해 주세요."));
		return true;
	}
	if (bBlockedByUi) return true;
	if (Controller.WasInputKeyJustPressed(EKeys::RightMouseButton))
	{
		CancelGroupPlacement();
		SetGroupEditingStatus(LOCTEXT("Cancelled", "묶음 편집을 취소했습니다."));
		return true;
	}
	if (Controller.WasInputKeyJustPressed(EKeys::Q)) RotateGroupPlacement(-GetPlacementRotationSnapDegrees());
	if (Controller.WasInputKeyJustPressed(EKeys::E)) RotateGroupPlacement(GetPlacementRotationSnapDegrees());
	const bool bCommit = Controller.WasInputKeyJustPressed(EKeys::LeftMouseButton) ||
		Controller.WasInputKeyJustPressed(EKeys::Enter);
	if (bCommit || GetWorld()->GetTimeSeconds() >= NextGroupPreviewTime)
	{
		FHitResult Ground;
		if (TraceGroundUnderCursor(Controller, Ground)) RefreshGroupPlacement(Ground.ImpactPoint);
		else
		{
			bGroupHasTarget = bGroupTargetValid = false;
			for (AFFZSPlacementPreviewActor* Preview : GroupPreviews) if (Preview) Preview->SetActorHiddenInGame(true);
			SetGroupEditingStatus(LOCTEXT("PointGround", "배치할 땅을 가리켜 주세요."));
		}
	}
	if (bCommit) CommitGroupPlacement();
	return true;
}

bool UFF_BuildModeSubsystem::CommitGroupPlacement()
{
	if (!IsGroupPlacementActive() || !bGroupHasTarget || bInputSuspendedForPauseMenu || !CanReconfigure()) return false;
	UFFZSWorldSubsystem* Zones = GetZoneSystem();
	if (!Zones || Zones->GetWorldRevision() != GroupSourceRevision || !RefreshGroupPlacement(GroupTargetCenter))
	{
		ShowStatus(GroupEditingStatus.ToString(), true);
		return false;
	}
	const TArray<FGuid> ResultIds = GroupSourceIds;
	bGroupCommitting = true;
	const FFFZSCommandResult Result = Zones->ExecuteConstructionBatch(GroupCommands);
	bGroupCommitting = false;
	if (!Result.bSucceeded) { SetGroupEditingStatus(Result.Message, true); return false; }
	CancelGroupPlacement();
	ClearMultiSelection();
	for (const FGuid& Id : ResultIds) AddMultiSelectedFacility(Zones->FindFacility(Id));
	SetGroupEditingStatus(LOCTEXT("Applied", "묶음 배치를 적용했습니다. 실행 취소로 한 번에 되돌릴 수 있습니다."));
	return true;
}

bool UFF_BuildModeSubsystem::RemoveSelectedGroup()
{
	if (!bMultiSelectionActive || bInputSuspendedForPauseMenu || !CanReconfigure() || IsGroupPlacementActive()) return false;
	const TArray<AFFZSFacilityActor*> Selected = GetMultiSelectedFacilities();
	UFFZSWorldSubsystem* Zones = GetZoneSystem();
	if (!Zones || Selected.IsEmpty()) return false;
	TArray<FFFZSCommand> Commands;
	const FGuid Transaction = FGuid::NewGuid();
	for (const AFFZSFacilityActor* Facility : Selected)
	{
		FFFZSCommand& Command = Commands.AddDefaulted_GetRef();
		Command.CommandId = FGuid::NewGuid();
		Command.TransactionId = Transaction;
		Command.ExpectedWorldRevision = Zones->GetWorldRevision();
		Command.Type = EFFZSCommandType::Remove;
		Command.FacilityInstanceId = Facility->GetFacilityInstanceId();
	}
	const FFFZSCommandResult Result = Zones->ExecuteConstructionBatch(Commands);
	if (!Result.bSucceeded) { SetGroupEditingStatus(Result.Message, true); return false; }
	ClearMultiSelection();
	SetGroupEditingStatus(LOCTEXT("Removed", "선택한 시설을 철거했습니다. 실행 취소로 시설과 환급금을 함께 되돌릴 수 있습니다."));
	return true;
}

bool UFF_BuildModeSubsystem::EnsureLayoutPresetsLoaded()
{
	if (LayoutPresets) return true;
	if (UGameplayStatics::DoesSaveGameExist(LayoutSlot, 0))
	{
		LayoutPresets = Cast<UFF_BuildLayoutPresetSave>(UGameplayStatics::LoadGameFromSlot(LayoutSlot, 0));
		if (!LayoutPresets || LayoutPresets->Version != UFF_BuildLayoutPresetSave::CurrentVersion ||
			LayoutPresets->Presets.Num() > UFF_BuildLayoutPresetSave::MaxPresets)
		{
			LayoutPresets = nullptr;
			ShowStatus(TEXT("배치 프리셋 저장 파일을 읽을 수 없습니다. 기존 파일은 유지됩니다."), true);
			return false;
		}
	}
	else LayoutPresets = NewObject<UFF_BuildLayoutPresetSave>(this);
	return true;
}

TArray<FString> UFF_BuildModeSubsystem::GetGroupPresetNames()
{
	TArray<FString> Names;
	if (EnsureLayoutPresetsLoaded())
		for (const FFF_BuildLayoutPreset& Preset : LayoutPresets->Presets) Names.Add(Preset.Name);
	return Names;
}

bool UFF_BuildModeSubsystem::SaveSelectedGroupPreset(const FString& Name)
{
	const FString Trimmed = Name.TrimStartAndEnd();
	if (!bMultiSelectionActive || IsGroupPlacementActive() || bInputSuspendedForPauseMenu || !CanReconfigure()) return false;
	if (Trimmed.IsEmpty() || Trimmed.Len() > 40)
	{
		SetGroupEditingStatus(LOCTEXT("PresetName", "프리셋 이름을 1~40자로 입력하세요."), true);
		return false;
	}
	FFF_BuildLayoutPreset Preset;
	FVector Center;
	Preset.Name = Trimmed;
	if (!CaptureSelectedGroup(Preset.Entries, Center) || !EnsureLayoutPresetsLoaded()) return false;
	const TArray<FFF_BuildLayoutPreset> Previous = LayoutPresets->Presets;
	const int32 Index = LayoutPresets->Presets.IndexOfByPredicate([&](const FFF_BuildLayoutPreset& Item) { return Item.Name.Equals(Trimmed); });
	if (Index != INDEX_NONE) LayoutPresets->Presets[Index] = MoveTemp(Preset);
	else if (LayoutPresets->Presets.Num() < UFF_BuildLayoutPresetSave::MaxPresets) LayoutPresets->Presets.Add(MoveTemp(Preset));
	else { SetGroupEditingStatus(LOCTEXT("PresetLimit", "프리셋은 32개까지 저장할 수 있습니다. 기존 프리셋을 삭제하거나 덮어써 주세요."), true); return false; }
	if (!UGameplayStatics::SaveGameToSlot(LayoutPresets, LayoutSlot, 0))
	{
		LayoutPresets->Presets = Previous;
		SetGroupEditingStatus(LOCTEXT("SaveFailed", "프리셋을 저장하지 못했습니다. 저장 공간을 확인하세요."), true);
		return false;
	}
	SetGroupEditingStatus(LOCTEXT("Saved", "배치 프리셋을 저장했습니다. 다음 실행에서도 사용할 수 있습니다."));
	return true;
}

bool UFF_BuildModeSubsystem::BeginGroupPreset(const FString& Name)
{
	if (!bMultiSelectionActive || State != EFF_BuildModeState::BuildMode || bInputSuspendedForPauseMenu ||
		!CanReconfigure() || !EnsureLayoutPresetsLoaded()) return false;
	const FFF_BuildLayoutPreset* Preset = LayoutPresets->Presets.FindByPredicate([&](const FFF_BuildLayoutPreset& Item) { return Item.Name == Name; });
	if (!Preset || Preset->Entries.IsEmpty() || Preset->Entries.Num() > UFF_BuildLayoutPresetSave::MaxFacilities) return false;
	for (const FFF_BuildLayoutEntry& Entry : Preset->Entries)
		if (Entry.RelativeCenter.ContainsNaN() || !FMath::IsFinite(Entry.WorldYawDegrees) || !UFFZSGridLibrary::HasSafeFacilitySpecCardinality(Entry.Spec)) return false;
	CancelGroupPlacement();
	GroupEntries = Preset->Entries;
	bGroupDuplicate = true;
	GroupSourceRevision = GetZoneSystem()->GetWorldRevision();
	for (int32 Index = 0; Index < GroupEntries.Num(); ++Index) GroupSourceIds.Add(FGuid::NewGuid());
	CreateGroupPreviews();
	for (AFFZSPlacementPreviewActor* Preview : GroupPreviews) if (Preview) Preview->SetActorHiddenInGame(true);
	bMultiSelectionPointerArmed = false;
	NotifyBuildToolsInteraction();
	SetGroupEditingStatus(LOCTEXT("PresetPlacement", "프리셋을 놓을 땅을 가리켜 주세요. 새 시설의 설치비가 적용됩니다."));
	return true;
}

bool UFF_BuildModeSubsystem::DeleteGroupPreset(const FString& Name)
{
	if (bInputSuspendedForPauseMenu || !EnsureLayoutPresetsLoaded()) return false;
	const TArray<FFF_BuildLayoutPreset> Previous = LayoutPresets->Presets;
	if (LayoutPresets->Presets.RemoveAll([&](const FFF_BuildLayoutPreset& Item) { return Item.Name == Name; }) == 0) return false;
	if (!UGameplayStatics::SaveGameToSlot(LayoutPresets, LayoutSlot, 0))
	{
		LayoutPresets->Presets = Previous;
		SetGroupEditingStatus(LOCTEXT("DeleteFailed", "프리셋 삭제를 저장하지 못했습니다."), true);
		return false;
	}
	SetGroupEditingStatus(LOCTEXT("Deleted", "배치 프리셋을 삭제했습니다. 설치된 시설은 그대로 유지됩니다."));
	return true;
}

#undef LOCTEXT_NAMESPACE
