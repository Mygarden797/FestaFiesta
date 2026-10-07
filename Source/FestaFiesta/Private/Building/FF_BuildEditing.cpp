#include "Building/FF_BuildModeSubsystem.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSQueueGeometry.h"
#include "FF_FFZSWorldSubsystem.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"

bool UFF_BuildModeSubsystem::UndoBuildEdit()
{
	if (bInputSuspendedForPauseMenu || bIsExitingBuildMode || !CanReconfigure() ||
		!FFBuildModeModel::IsBuildSessionActive(State)) return false;
	NotifyBuildToolsInteraction();
	CancelGroupPlacement();
	EndDragPlacement();
	if (State == EFF_BuildModeState::Moving) CancelMovingToBuildMode(TEXT("이동 미리보기를 취소했습니다."));
	FText Error;
	UFFZSWorldSubsystem* Zones = GetZoneSystem();
	const bool bSuccess = Zones && Zones->UndoConstruction(Error);
	ShowStatus(bSuccess ? TEXT("건축 작업을 되돌렸습니다.") : Error.ToString(), !bSuccess);
	OnBuildToolsChanged.Broadcast();
	return bSuccess;
}

bool UFF_BuildModeSubsystem::RedoBuildEdit()
{
	if (bInputSuspendedForPauseMenu || bIsExitingBuildMode || !CanReconfigure() ||
		!FFBuildModeModel::IsBuildSessionActive(State)) return false;
	NotifyBuildToolsInteraction();
	CancelGroupPlacement();
	EndDragPlacement();
	if (State == EFF_BuildModeState::Moving) CancelMovingToBuildMode(TEXT("이동 미리보기를 취소했습니다."));
	FText Error;
	UFFZSWorldSubsystem* Zones = GetZoneSystem();
	const bool bSuccess = Zones && Zones->RedoConstruction(Error);
	ShowStatus(bSuccess ? TEXT("건축 작업을 다시 실행했습니다.") : Error.ToString(), !bSuccess);
	OnBuildToolsChanged.Broadcast();
	return bSuccess;
}

bool UFF_BuildModeSubsystem::SetSelectedFacilityQueueLayout(const EFFZSQueueLayout Layout)
{
	if (!CanChooseQueueLayout()) return false;
	if (FFBuildModeModel::IsPreviewActive(State))
	{
		FFFZSFacilitySpec Base;
		int32 Level = 1;
		float Size = 1.0f;
		if (State == EFF_BuildModeState::Moving)
		{
			const AFFZSFacilityActor* Facility = MovingFacility.Get();
			if (!Facility) return false;
			Base = Facility->GetBaseFacilitySpec();
			Level = Facility->GetUpgradeLevel();
			Size = Facility->GetUpgradeSizeMultiplier();
		}
		else
		{
			const UFF_BuildCatalogSubsystem* Catalog = GetCatalogSubsystem();
			const UFFZSFacilityDefinition* Definition = Catalog ? Catalog->FindDefinition(SelectedFacilityDefinitionId) : nullptr;
			if (!Definition) return false;
			Base = Definition->Spec;
		}
		Base.QueueLayout = EFFZSQueueLayout::Authored;
		FFFZSFacilitySpec Candidate = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Base, Level, Size);
		Candidate.QueueLayout = Layout;
		FText Error;
		if (!FFFZSQueueGeometry::ApplySelectedLayout(Candidate, Error))
		{
			ShowStatus(Error.ToString(), true);
			return false;
		}
		// Finish the current paint stroke before changing its queue footprint. Already
		// committed facilities stay intact; pending targets cannot inherit a new shape.
		EndDragPlacement();
		CancelNearbyPlacementSearch();
		PendingQueueLayout = Layout;
		SelectedSpec = MoveTemp(Candidate);
		bPreviewWorldDirty = true;
		bSuppressWorldClickUntilReleased = true;
		bSuppressPlacementConfirmUntilReleased = true;
		NotifyBuildToolsInteraction();
		if (bHasPlacementTarget) UpdatePlacementFromCursor(true);
		if (bHasPlacementTarget && !LastValidation.bIsValid)
			ShowStatus(FString::Printf(TEXT("현재 위치에 대기열을 놓을 수 없습니다: %s"), *LastValidation.Summary.ToString()), true);
		else
			ShowStatus(State == EFF_BuildModeState::Moving
				? TEXT("대기열 미리보기를 변경했습니다. 이동을 확정하면 함께 적용됩니다. 우클릭으로 취소할 수 있습니다.")
				: TEXT("대기열 미리보기를 변경했습니다. 배치를 확정하면 적용됩니다."));
		OnBuildToolsChanged.Broadcast();
		return true;
	}
	UFF_FacilityInfoSubsystem* Info = GetFacilityInfoSubsystem();
	AFFZSFacilityActor* Facility = Info ? Info->GetSelectedFacility() : nullptr;
	UFFZSWorldSubsystem* Zones = GetZoneSystem();
	if (!Facility || !Zones) return false;
	NotifyBuildToolsInteraction();
	FText Error;
	const bool bSuccess = Zones->SetFacilityQueueLayout(Facility->GetFacilityInstanceId(), Layout, Error);
	ShowStatus(bSuccess ? TEXT("대기열 형태를 변경했습니다.") : Error.ToString(), !bSuccess);
	OnBuildToolsChanged.Broadcast();
	return bSuccess;
}

bool UFF_BuildModeSubsystem::CanChooseQueueLayout() const
{
	if (bInputSuspendedForPauseMenu || bIsExitingBuildMode || bMultiSelectionActive ||
		IsGroupPlacementActive() || !CanReconfigure()) return false;
	if (FFBuildModeModel::IsPreviewActive(State))
		return SelectedSpec.UsesServiceQueue() && SelectedSpec.QueueRules.MaxWaitingAgents > 0 &&
			(State != EFF_BuildModeState::Moving || (MovingFacility.IsValid() && MovingFacility->CanMoveOrRemove()));
	const UFF_FacilityInfoSubsystem* Info = GetFacilityInfoSubsystem();
	const AFFZSFacilityActor* Facility = Info ? Info->GetSelectedFacility() : nullptr;
	return State == EFF_BuildModeState::BuildMode && Facility && Facility->CanMoveOrRemove() &&
		Facility->GetFacilitySpec().UsesServiceQueue() && Facility->GetFacilitySpec().QueueRules.MaxWaitingAgents > 0;
}

EFFZSQueueLayout UFF_BuildModeSubsystem::GetEditingQueueLayout() const
{
	if (FFBuildModeModel::IsPreviewActive(State)) return PendingQueueLayout;
	const UFF_FacilityInfoSubsystem* Info = GetFacilityInfoSubsystem();
	const AFFZSFacilityActor* Facility = Info ? Info->GetSelectedFacility() : nullptr;
	return Facility ? Facility->GetQueueLayout() : EFFZSQueueLayout::Authored;
}

FText UFF_BuildModeSubsystem::GetQueueLayoutKeyHint() const
{
	if (!CanChooseQueueLayout()) return FText::GetEmpty();
	const TCHAR* Name = TEXT("기본형");
	switch (GetEditingQueueLayout())
	{
	case EFFZSQueueLayout::Left: Name = TEXT("좌측형"); break;
	case EFFZSQueueLayout::Right: Name = TEXT("우측형"); break;
	case EFFZSQueueLayout::UShape: Name = TEXT("U자형"); break;
	default: break;
	}
	return FText::FromString(FString::Printf(TEXT("현재: %s%s"), Name,
		FFBuildModeModel::IsPreviewActive(State) ? TEXT(" · 배치 확정 시 적용") : TEXT("")));
}
