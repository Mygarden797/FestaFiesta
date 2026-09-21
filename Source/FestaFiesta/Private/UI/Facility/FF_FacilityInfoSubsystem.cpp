// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoSubsystem.h"

#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "Building/FF_FacilityUpgradeSubsystem.h"
#include "Building/FF_FacilityUpgradeTypes.h"
#include "Building/FF_FacilityNamingLibrary.h"
#include "Building/Synergy/FF_FacilitySynergySubsystem.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Components/BoxComponent.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "Economy/FF_ProgramSchedulingSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "FF_FFZSCollision.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Layout/Margin.h"
#include "InputCoreTypes.h"
#include "Minigame/FF_MinigameCatalogSubsystem.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityPaidProgramPicker.h"
#include "UI/FacilityRevenue/FF_FacilityRevenueRanking.h"
#include "Building/Presentation/FF_BuildFavoritesSubsystem.h"
#include "UI/Facility/FF_FacilityInfoSelection.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

#define LOCTEXT_NAMESPACE "FFFacilityInfoSubsystem"

DEFINE_LOG_CATEGORY_STATIC(LogFestaFiestaFacilityInfo, Log, All);

namespace
{
	constexpr float FacilitySelectionTraceDistanceCm = 500000.0f;
	constexpr float FacilitySelectionOcclusionEndToleranceCm = 2.0f;
	constexpr int32 MaxFacilitySelectionCandidates = 32;

	FText FormatIntegerWithSuffix(const int32 Value, const FText& Suffix)
	{
		return FText::Format(LOCTEXT("IntegerSuffixFormat", "{0}{1}"), FText::AsNumber(Value), Suffix);
	}

	FText FormatOccupancy(const int32 Occupied, const int32 Capacity)
	{
		return FText::Format(
			LOCTEXT("OccupancyFormat", "{0} / {1}팀"),
			FText::AsNumber(Occupied),
			FText::AsNumber(Capacity));
	}

	FText FormatInstanceLimit(
		const int32 CurrentInstances,
		const int32 MaxInstances,
		const bool bLimitReached)
	{
		if (MaxInstances <= 0)
		{
			return FText::Format(
				LOCTEXT("UnlimitedInstancesFormat", "현재 {0} / 제한 없음"),
				FText::AsNumber(CurrentInstances));
		}

		if (bLimitReached)
		{
			return FText::Format(
				LOCTEXT("ReachedInstanceLimitFormat", "현재 {0} / 최대 {1} · 한도 도달"),
				FText::AsNumber(CurrentInstances),
				FText::AsNumber(MaxInstances));
		}

		return FText::Format(
			LOCTEXT("AvailableInstanceLimitFormat", "현재 {0} / 최대 {1} · {2}개 남음"),
			FText::AsNumber(CurrentInstances),
			FText::AsNumber(MaxInstances),
			FText::AsNumber(FMath::Max(0, MaxInstances - CurrentInstances)));
	}
}

void UFF_FacilityInfoSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Super::Initialize(Collection);

	if (UFFZSWorldSubsystem* ZoneSystem = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>())
	{
		ZoneSystem->OnWorldChanged.AddUniqueDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.AddUniqueDynamic(
			this, &ThisClass::HandleWorldSnapshotRestored);
		ZoneSystem->OnFacilityLightingChanged.AddUniqueDynamic(this, &ThisClass::HandleFacilityLightingChanged);
	}
	EnsureProgramReadModelBindings();
}

void UFF_FacilityInfoSubsystem::Deinitialize()
{
	ClearLiveSelectionIndicators();
	ReleasePickControllerBinding();
	if (BoundProgression)
	{
		BoundProgression->OnScheduledProgramsChanged.RemoveDynamic(
			this, &ThisClass::HandleScheduledProgramsChanged);
		BoundProgression->OnFacilityRevenueChanged.RemoveDynamic(
			this, &ThisClass::HandleFacilityRevenueChanged);
		BoundProgression = nullptr;
	}
	if (BoundStage)
	{
		BoundStage->OnStageChanged.RemoveDynamic(this, &ThisClass::HandleStageChanged);
		BoundStage = nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		if (UFFZSWorldSubsystem* ZoneSystem = World->GetSubsystem<UFFZSWorldSubsystem>())
		{
			ZoneSystem->OnWorldChanged.RemoveAll(this);
			ZoneSystem->OnWorldSnapshotRestored.RemoveAll(this);
			ZoneSystem->OnFacilityLightingChanged.RemoveAll(this);
		}
	}

	ReleaseThumbnailSession();
	ClosePaidProgramPicker();
	bRevenueRankingPresentationOpen = false;
	bPauseMenuPresentationSuppressed = false;
	bPanelWasVisibleBeforePause = false;
	bPaidProgramPickerWasVisibleBeforePause = false;
	PauseMenuSuppressedFacilityId.Invalidate();
	PauseMenuSuppressedContext = EFF_FacilityInfoContext::None;
	SelectedFacilityId.Invalidate();
	CachedSynergyFacilityId.Invalidate();
	CachedSynergyEntries.Reset();
	CachedSynergySummary = FText::GetEmpty();
	SelectionContext = EFF_FacilityInfoContext::None;
	if (Panel)
	{
		Panel->OnFavoriteRequested.RemoveAll(this);
		Panel->OnMoveRequested.RemoveAll(this);
		Panel->OnRemoveRequested.RemoveAll(this);
		Panel->OnProgramScheduleRequested.RemoveAll(this);
		Panel->OnUpgradeRequested.RemoveAll(this);
		Panel->CancelLightingEdit();
        Panel->OnLightingRollbackRequested.RemoveAll(this);
        Panel->OnLightingRequested.RemoveAll(this);
		Panel->OnLightingResetRequested.RemoveAll(this);
		Panel->OnMinigameRequested.RemoveAll(this);
		Panel->OnCloseRequested.RemoveAll(this);
		Panel->OnRevenueRankingRequested.RemoveAll(this);
		Panel->RemoveFromParent();
		Panel = nullptr;
	}
	OwningController.Reset();
	Super::Deinitialize();
}

bool UFF_FacilityInfoSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UFF_FacilityInfoSubsystem::Tick(const float DeltaTime)
{
	if (bPauseMenuPresentationSuppressed)
	{
		// Pause 중에는 높은 Viewport layer 표면과 hover/selection 갱신을 모두 멈춘다.
		return;
	}

	// The camera controller already owns click timing and input gating. Bind to its public pick
	// signal here so Camera source files stay untouched and controller replacement is tolerated.
	EnsurePickControllerBinding();
	EnsureProgramReadModelBindings();
	UpdateLiveSelectionIndicators(DeltaTime);
	if (!HasSelection())
	{
		RefreshAccumulatorSeconds = 0.0f;
		return;
	}

	RefreshAccumulatorSeconds += FMath::Max(0.0f, DeltaTime);
	if (RefreshAccumulatorSeconds < RefreshIntervalSeconds)
	{
		return;
	}
	RefreshAccumulatorSeconds = FMath::Fmod(RefreshAccumulatorSeconds, RefreshIntervalSeconds);
	RefreshPanel();
}

void UFF_FacilityInfoSubsystem::EnsureProgramReadModelBindings()
{
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
		: nullptr;
	if (BoundProgression != Progression)
	{
		if (BoundProgression)
		{
			BoundProgression->OnScheduledProgramsChanged.RemoveDynamic(
				this, &ThisClass::HandleScheduledProgramsChanged);
			BoundProgression->OnFacilityRevenueChanged.RemoveDynamic(
				this, &ThisClass::HandleFacilityRevenueChanged);
		}
		BoundProgression = Progression;
		if (BoundProgression)
		{
			BoundProgression->OnScheduledProgramsChanged.AddUniqueDynamic(
				this, &ThisClass::HandleScheduledProgramsChanged);
			BoundProgression->OnFacilityRevenueChanged.AddUniqueDynamic(
				this, &ThisClass::HandleFacilityRevenueChanged);
		}
	}

	UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	if (BoundStage != Stage)
	{
		if (BoundStage)
		{
			BoundStage->OnStageChanged.RemoveDynamic(this, &ThisClass::HandleStageChanged);
		}
		BoundStage = Stage;
		if (BoundStage)
		{
			BoundStage->OnStageChanged.AddUniqueDynamic(this, &ThisClass::HandleStageChanged);
		}
	}
}

TStatId UFF_FacilityInfoSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_FacilityInfoSubsystem, STATGROUP_Tickables);
}

FSoftClassPath UFF_FacilityInfoSubsystem::GetDefaultPanelClassPath()
{
	return FSoftClassPath(TEXT(
		"/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityInfoPanel."
		"WBP_FF_FacilityInfoPanel_C"));
}

bool UFF_FacilityInfoSubsystem::PrimePanel(APlayerController* InOwningController)
{
	return IsValid(InOwningController) && InOwningController->GetLocalPlayer() &&
		EnsurePanel(*InOwningController);
}

bool UFF_FacilityInfoSubsystem::EnsurePanel(APlayerController& InOwningController)
{
	if (Panel)
	{
		if (Panel->GetOwningPlayer() == &InOwningController && Panel->IsInViewport())
		{
			return true;
		}

		// Seamless controller replacement is uncommon in this single-player project, but keeping
		// the panel bound to a stale LocalPlayer would route input and viewport ownership incorrectly.
		Panel->OnFavoriteRequested.RemoveAll(this);
		Panel->OnMoveRequested.RemoveAll(this);
		Panel->OnRemoveRequested.RemoveAll(this);
		Panel->OnProgramScheduleRequested.RemoveAll(this);
		Panel->OnUpgradeRequested.RemoveAll(this);
		Panel->CancelLightingEdit();
        Panel->OnLightingRollbackRequested.RemoveAll(this);
        Panel->OnLightingRequested.RemoveAll(this);
		Panel->OnLightingResetRequested.RemoveAll(this);
		Panel->OnMinigameRequested.RemoveAll(this);
		Panel->OnCloseRequested.RemoveAll(this);
		Panel->OnRevenueRankingRequested.RemoveAll(this);
		Panel->RemoveFromParent();
		Panel = nullptr;
		ClosePaidProgramPicker();
		OwningController.Reset();
	}
	if (!InOwningController.GetLocalPlayer())
	{
		return false;
	}

	UClass* PanelClass = GetDefaultPanelClassPath().TryLoadClass<UFF_FacilityInfoPanel>();
	if (!PanelClass || !PanelClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass()))
	{
		UE_LOG(LogFestaFiestaFacilityInfo, Error,
			TEXT("시설 정보 WBP를 불러오지 못했습니다: %s. 에디터에서 FF.BuildFacilityInfoUMG를 실행하세요."),
			*GetDefaultPanelClassPath().ToString());
		return false;
	}

	Panel = CreateWidget<UFF_FacilityInfoPanel>(&InOwningController, PanelClass);
	if (!Panel)
	{
		UE_LOG(LogFestaFiestaFacilityInfo, Error, TEXT("시설 정보 패널 인스턴스 생성 실패."));
		return false;
	}

	Panel->OnMoveRequested.AddUObject(this, &ThisClass::HandlePanelMoveRequested);
	Panel->OnFavoriteRequested.AddUObject(this, &ThisClass::HandlePanelFavoriteRequested);
	Panel->OnRemoveRequested.AddUObject(this, &ThisClass::HandlePanelRemoveRequested);
	Panel->OnProgramScheduleRequested.AddUObject(
		this, &ThisClass::HandlePanelProgramScheduleRequested);
	Panel->OnUpgradeRequested.AddUObject(this, &ThisClass::HandlePanelUpgradeRequested);
	Panel->OnLightingRequested.AddUObject(this, &ThisClass::HandlePanelLightingRequested);
	Panel->OnLightingRollbackRequested.AddUObject(this, &ThisClass::HandlePanelLightingRollbackRequested);
	Panel->OnLightingResetRequested.AddUObject(this, &ThisClass::HandlePanelLightingResetRequested);
	Panel->OnMinigameRequested.AddUObject(this, &ThisClass::HandlePanelMinigameRequested);
	Panel->OnCloseRequested.AddUObject(this, &ThisClass::HandlePanelCloseRequested);
	Panel->OnRevenueRankingRequested.AddUObject(this, &ThisClass::HandlePanelRevenueRankingRequested);
	// RootLayout and its stage HUDs use AddToViewport. AddToPlayerScreen belongs to UE's lower
	// Player game layer, so its local Z-order can never rise above a newly-created PrepHUD in the
	// Viewport layer (UE 5.7: Player=100, Viewport=200). Keep this card in the same global layer.
	Panel->AddToViewport(PanelZOrder);
	if (!Panel->IsInViewport())
	{
		UE_LOG(LogFestaFiestaFacilityInfo, Error,
			TEXT("시설 정보 패널을 Viewport 화면에 추가하지 못했습니다."));
		Panel->OnFavoriteRequested.RemoveAll(this);
		Panel->OnMoveRequested.RemoveAll(this);
		Panel->OnRemoveRequested.RemoveAll(this);
		Panel->OnProgramScheduleRequested.RemoveAll(this);
		Panel->OnUpgradeRequested.RemoveAll(this);
		Panel->CancelLightingEdit();
        Panel->OnLightingRollbackRequested.RemoveAll(this);
        Panel->OnLightingRequested.RemoveAll(this);
		Panel->OnLightingResetRequested.RemoveAll(this);
		Panel->OnMinigameRequested.RemoveAll(this);
		Panel->OnCloseRequested.RemoveAll(this);
		Panel->OnRevenueRankingRequested.RemoveAll(this);
		Panel = nullptr;
		return false;
	}
	Panel->SetPanelOpen(false);
	OwningController = &InOwningController;
	return true;
}

bool UFF_FacilityInfoSubsystem::EnsurePaidProgramPicker(APlayerController& InOwningController)
{
	if (PaidProgramPicker)
	{
		if (PaidProgramPicker->GetOwningPlayer() == &InOwningController &&
			PaidProgramPicker->IsInViewport())
		{
			return true;
		}
		ClosePaidProgramPicker();
	}
	if (!InOwningController.GetLocalPlayer())
	{
		return false;
	}

	UClass* PickerClass =
		GetDefaultPaidProgramPickerClassPath().TryLoadClass<UFF_FacilityPaidProgramPicker>();
	if (!PickerClass || !PickerClass->IsChildOf(UFF_FacilityPaidProgramPicker::StaticClass()))
	{
		UE_LOG(LogFestaFiestaFacilityInfo, Error,
			TEXT("유료 프로그램 Picker WBP를 불러오지 못했습니다: %s. FF.BuildFacilityInfoUMG를 실행하세요."),
			*GetDefaultPaidProgramPickerClassPath().ToString());
		return false;
	}

	PaidProgramPicker = CreateWidget<UFF_FacilityPaidProgramPicker>(&InOwningController, PickerClass);
	if (!PaidProgramPicker)
	{
		return false;
	}
	PaidProgramPicker->OnConfirmed.AddUObject(
		this, &ThisClass::HandlePaidProgramConfirmed);
	PaidProgramPicker->OnCloseRequested.AddUObject(
		this, &ThisClass::HandlePaidProgramPickerCloseRequested);
	// The picker replaces the card as the active surface and must share its global viewport layer.
	PaidProgramPicker->AddToViewport(PaidProgramPickerZOrder);
	if (!PaidProgramPicker->IsInViewport())
	{
		ClosePaidProgramPicker();
		return false;
	}
	PaidProgramPicker->SetPickerOpen(false);
	return true;
}

void UFF_FacilityInfoSubsystem::ClosePaidProgramPicker()
{
	if (!PaidProgramPicker)
	{
		return;
	}
	PaidProgramPicker->OnConfirmed.RemoveAll(this);
	PaidProgramPicker->OnCloseRequested.RemoveAll(this);
	PaidProgramPicker->RemoveFromParent();
	PaidProgramPicker = nullptr;
	if (Panel && HasSelection())
	{
		// Picker의 의미 상태가 닫혀도 Pause Hub 뒤의 높은 Z-order 카드가 다시 나타나면 안 된다.
		if (bPauseMenuPresentationSuppressed)
		{
			bPaidProgramPickerWasVisibleBeforePause = false;
			bPanelWasVisibleBeforePause = true;
			Panel->SetPanelOpen(false);
		}
		else
		{
			Panel->SetPanelOpen(!bRevenueRankingPresentationOpen);
		}
	}
}

void UFF_FacilityInfoSubsystem::SetPaidProgramPickerOpen(const bool bOpen)
{
	if (!PaidProgramPicker)
	{
		return;
	}

	// These are two views of the same selected facility. Keeping one presentation owner visible
	// avoids both the right-side card overlap and stale actions behind the confirmation surface.
	if (bPauseMenuPresentationSuppressed)
	{
		// suppressed 동안 발생한 권위 수명주기 변경은 복원 대상에 반영하되 실제 표면은 계속 숨긴다.
		bPaidProgramPickerWasVisibleBeforePause = bOpen;
		bPanelWasVisibleBeforePause = !bOpen && HasSelection();
		PaidProgramPicker->SetPickerOpen(false);
		if (Panel)
		{
			Panel->SetPanelOpen(false);
		}
		return;
	}

	PaidProgramPicker->SetPickerOpen(bOpen);
	if (Panel)
	{
		Panel->SetPanelOpen(!bOpen && HasSelection() && !bRevenueRankingPresentationOpen);
	}
}

bool UFF_FacilityInfoSubsystem::SelectFacility(
	APlayerController* InOwningController,
	AFFZSFacilityActor* Facility,
	const EFF_FacilityInfoContext Context)
{
	if (bPauseMenuPresentationSuppressed)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!IsValid(InOwningController) || !InOwningController->GetLocalPlayer() ||
		!IsValid(Facility) || !ZoneSystem ||
		Context == EFF_FacilityInfoContext::None || !IsContextCurrentlyAllowed(Context))
	{
		return false;
	}

	const FGuid FacilityId = Facility->GetFacilityInstanceId();
	if (!FacilityId.IsValid() || ZoneSystem->FindFacility(FacilityId) != Facility || !EnsurePanel(*InOwningController))
	{
		return false;
	}

	const bool bSelectionChanged = SelectedFacilityId != FacilityId || SelectionContext != Context;
	if (bSelectionChanged)
	{
		ClosePaidProgramPicker();
	}
	SelectedFacilityId = FacilityId;
	SelectionContext = Context;
	OwningController = InOwningController;
	FeedbackText = FText::GetEmpty();
	bFeedbackIsError = false;
	RefreshAccumulatorSeconds = 0.0f;
	AcquireThumbnailSession();
	RefreshPanel();

	// Refresh can clear the selection synchronously if a world-change listener removed or
	// replaced the actor while the panel was being projected. Never resurrect that stale card.
	if (!Panel || SelectedFacilityId != FacilityId || SelectionContext != Context)
	{
		return false;
	}
	const bool bPickerOpen = PaidProgramPicker &&
		PaidProgramPicker->GetVisibility() != ESlateVisibility::Collapsed;
	Panel->SetPanelOpen(!bPickerOpen && !bRevenueRankingPresentationOpen);
	if (Context == EFF_FacilityInfoContext::Live)
	{
		ApplyLiveSelectionIndicatorTargets(SelectionIndicatorHoverFacility.Get(), Facility);
	}
	else
	{
		ClearLiveSelectionIndicators();
	}
	if (bSelectionChanged)
	{
		OnSelectionChanged.Broadcast(FacilityId, Context);
	}
	return true;
}

bool UFF_FacilityInfoSubsystem::SelectFacilityAtWorldPoint(
	APlayerController* InOwningController,
	const FVector& WorldPoint,
	const EFF_FacilityInfoContext Context,
	const FName ZoneId)
{
	UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	AFFZSFacilityActor* Facility = ZoneSystem
		? ZoneSystem->FindFacilityAtWorldPoint(WorldPoint, ZoneId)
		: nullptr;
	return Facility && SelectFacility(InOwningController, Facility, Context);
}

void UFF_FacilityInfoSubsystem::ClearSelection()
{
	const bool bHadSelection = HasSelection();
	ClosePaidProgramPicker();
	SelectedFacilityId.Invalidate();
	CachedSynergyFacilityId.Invalidate();
	CachedSynergyEntries.Reset();
	CachedSynergySummary = FText::GetEmpty();
	SelectionContext = EFF_FacilityInfoContext::None;
	if (bPauseMenuPresentationSuppressed)
	{
		bPanelWasVisibleBeforePause = false;
		bPaidProgramPickerWasVisibleBeforePause = false;
	}
	if (UFF_FacilityRevenueRankingSubsystem* Ranking = GetWorld()
		? GetWorld()->GetSubsystem<UFF_FacilityRevenueRankingSubsystem>() : nullptr)
	{
		if (Ranking->IsOpen()) Ranking->Close();
	}
	bRevenueRankingPresentationOpen = false;
	FeedbackText = FText::GetEmpty();
	bFeedbackIsError = false;
	RefreshAccumulatorSeconds = 0.0f;
	ApplyLiveSelectionIndicatorTargets(SelectionIndicatorHoverFacility.Get(), nullptr);
	ReleaseThumbnailSession();
	if (Panel)
	{
		// Clear the view snapshot and image brush before collapsing so the last selected thumbnail
		// is not retained for the lifetime of the world.
		Panel->ApplyViewData(FFF_FacilityInfoViewData());
		Panel->SetPanelOpen(false);
	}
	if (bHadSelection)
	{
		OnSelectionChanged.Broadcast(FGuid(), EFF_FacilityInfoContext::None);
	}
}

void UFF_FacilityInfoSubsystem::ClearSelectionIfMatches(
	const FGuid FacilityInstanceId,
	const EFF_FacilityInfoContext Context)
{
	if (SelectedFacilityId == FacilityInstanceId && SelectionContext == Context)
	{
		ClearSelection();
	}
}

AFFZSFacilityActor* UFF_FacilityInfoSubsystem::GetSelectedFacility() const
{
	const UWorld* World = GetWorld();
	const UFFZSWorldSubsystem* ZoneSystem = World
		? World->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr;
	return ZoneSystem && SelectedFacilityId.IsValid()
		? ZoneSystem->FindFacility(SelectedFacilityId)
		: nullptr;
}

bool UFF_FacilityInfoSubsystem::IsPointerOverPanel() const
{
	if (bPauseMenuPresentationSuppressed)
	{
		return false;
	}
	const UFF_FacilityRevenueRankingSubsystem* Ranking = GetWorld()
		? GetWorld()->GetSubsystem<UFF_FacilityRevenueRankingSubsystem>() : nullptr;
	return (Panel && Panel->IsPointerOverInteractivePanel()) ||
		(PaidProgramPicker && PaidProgramPicker->IsPointerOverInteractivePanel()) ||
		(Ranking && Ranking->IsPointerOverPanel());
}

bool UFF_FacilityInfoSubsystem::IsLightingTextInputFocused() const
{ return Panel && Panel->IsLightingTextInputFocused(); }

void UFF_FacilityInfoSubsystem::SetRevenueRankingPresentationOpen(const bool bOpen)
{
	bRevenueRankingPresentationOpen = bOpen;
	const bool bPickerOpen = PaidProgramPicker && PaidProgramPicker->GetVisibility() != ESlateVisibility::Collapsed;
	if (Panel) Panel->SetPanelOpen(!bOpen && !bPauseMenuPresentationSuppressed && !bPickerOpen &&
		HasSelection() && IsContextCurrentlyAllowed(SelectionContext));
}

void UFF_FacilityInfoSubsystem::HandlePanelRevenueRankingRequested()
{
	if (!bPauseMenuPresentationSuppressed && !bRevenueRankingPresentationOpen && Panel && HasSelection() &&
		Panel->GetCurrentViewData().FacilityInstanceId == SelectedFacilityId && IsContextCurrentlyAllowed(SelectionContext))
		OnRevenueRankingRequested.Broadcast();
}

void UFF_FacilityInfoSubsystem::SetPauseMenuPresentationSuppressed(const bool bSuppressed)
{
	if (bPauseMenuPresentationSuppressed == bSuppressed)
	{
		return;
	}

	if (bSuppressed)
	{
		// Close the replacement presentation before recording the inspector surface to restore.
		if (UFF_FacilityRevenueRankingSubsystem* Ranking = GetWorld()
			? GetWorld()->GetSubsystem<UFF_FacilityRevenueRankingSubsystem>() : nullptr)
		{
			if (Ranking->IsOpen()) Ranking->Close();
		}
		const auto IsPresented = [](const ESlateVisibility Visibility)
		{
			return Visibility != ESlateVisibility::Collapsed &&
				Visibility != ESlateVisibility::Hidden;
		};
		bPanelWasVisibleBeforePause = Panel && IsPresented(Panel->GetVisibility());
		bPaidProgramPickerWasVisibleBeforePause =
			PaidProgramPicker && IsPresented(PaidProgramPicker->GetVisibility());
		PauseMenuSuppressedFacilityId = SelectedFacilityId;
		PauseMenuSuppressedContext = SelectionContext;
		bPauseMenuPresentationSuppressed = true;
		if (Panel)
		{
			Panel->SetPanelOpen(false);
		}
		if (PaidProgramPicker)
		{
			PaidProgramPicker->SetPickerOpen(false);
		}
		return;
	}

	const bool bSelectionUnchanged = PauseMenuSuppressedFacilityId.IsValid() &&
		SelectedFacilityId == PauseMenuSuppressedFacilityId &&
		SelectionContext == PauseMenuSuppressedContext;
	const bool bRestorePicker = bSelectionUnchanged &&
		bPaidProgramPickerWasVisibleBeforePause && PaidProgramPicker;
	const bool bRestorePanel = bSelectionUnchanged && !bRestorePicker &&
		(bPanelWasVisibleBeforePause ||
			(bPaidProgramPickerWasVisibleBeforePause && !PaidProgramPicker));

	bPauseMenuPresentationSuppressed = false;
	if (PaidProgramPicker)
	{
		PaidProgramPicker->SetPickerOpen(bRestorePicker);
	}
	if (Panel)
	{
		Panel->SetPanelOpen(bRestorePanel && !bRevenueRankingPresentationOpen);
	}
	bPanelWasVisibleBeforePause = false;
	bPaidProgramPickerWasVisibleBeforePause = false;
	PauseMenuSuppressedFacilityId.Invalidate();
	PauseMenuSuppressedContext = EFF_FacilityInfoContext::None;
}

void UFF_FacilityInfoSubsystem::SetFeedback(const FText& Feedback, const bool bIsError)
{
	if (!HasSelection())
	{
		return;
	}
	FeedbackText = Feedback;
	bFeedbackIsError = bIsError;
	RefreshPanel();
}

bool UFF_FacilityInfoSubsystem::IsContextCurrentlyAllowed(
	const EFF_FacilityInfoContext Context) const
{
	if (Context != EFF_FacilityInfoContext::Live)
	{
		return Context == EFF_FacilityInfoContext::Build;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	return Stage && Stage->CurrentStage == EFF_FestivalStage::Live;
}

void UFF_FacilityInfoSubsystem::ApplySynergyViewData(
	const AFFZSFacilityActor& Facility, FFF_FacilityInfoViewData& OutViewData) const
{
	const UWorld* World = GetWorld();
	const UFF_FacilitySynergySubsystem* Synergies = World ? World->GetSubsystem<UFF_FacilitySynergySubsystem>() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_BuildCatalogSubsystem* Catalog = GameInstance ? GameInstance->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
	const bool bEligible = !Facility.NeedsRelocation() && Facility.GetFacilityState() != EFFZSFacilityState::Planned
		&& Facility.GetFacilityState() != EFFZSFacilityState::Removed;
	const uint64 Revision = Synergies ? Synergies->GetRevision() : 0;
	const uint64 CatalogRevision = Catalog ? Catalog->GetCatalogRevision() : 0;
	const FGuid FacilityId = Facility.GetFacilityInstanceId();
	if (CachedSynergyFacilityId != FacilityId || CachedSynergyRevision != Revision ||
		CachedSynergyCatalogRevision != CatalogRevision || bCachedSynergyEligible != bEligible)
	{
		FFF_FacilitySynergyPreview Preview;
		if (Synergies)
		{
			// Same identity and centre produce an installed-state view, including reverse beneficiaries.
			Synergies->BuildPlacementPreview(Facility.GetFacilitySpec().FacilityDefinitionId,
				Facility.GetActorLocation(), FacilityId, Preview);
		}
		TMap<FName, FText> Names;
		for (const FFF_FacilitySynergyPreviewEntry& Entry : Preview.Entries)
		{
			for (const FName DefinitionId : {Entry.TargetFacilityId, Entry.PartnerFacilityId})
			{
				if (Names.Contains(DefinitionId)) continue;
				Names.Add(DefinitionId, Catalog ? Catalog->ResolveFacilityDisplayName(DefinitionId)
					: LOCTEXT("SynergyNamesLoading", "시설 이름 불러오는 중"));
			}
		}
		// The selected installed facility uses the same authored display name as this inspector's heading.
		if (!Facility.GetFacilitySpec().DisplayName.IsEmpty())
			Names.Add(Facility.GetFacilitySpec().FacilityDefinitionId, Facility.GetFacilitySpec().DisplayName);
		FFFacilityInfoModel::ApplySynergyPreview(Synergies ? &Preview : nullptr, bEligible,
			Synergies ? Synergies->GetBonusPercent(FacilityId) : 0.f, Names, OutViewData);
		CachedSynergyFacilityId = FacilityId;
		CachedSynergyRevision = Revision;
		CachedSynergyCatalogRevision = CatalogRevision;
		bCachedSynergyEligible = bEligible;
		CachedSynergySummary = OutViewData.SynergySummaryText;
		CachedSynergyEntries = OutViewData.SynergyEntries;
		return;
	}
	// Queue/occupancy refreshes copy the prepared presentation instead of repeating spatial evaluation.
	OutViewData.SynergySummaryText = CachedSynergySummary;
	OutViewData.SynergyEntries = CachedSynergyEntries;
}

bool UFF_FacilityInfoSubsystem::BuildViewData(
	AFFZSFacilityActor& Facility,
	FFF_FacilityInfoViewData& OutViewData) const
{
	UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!ZoneSystem || ZoneSystem->FindFacility(Facility.GetFacilityInstanceId()) != &Facility)
	{
		return false;
	}

	OutViewData = FFF_FacilityInfoViewData();
	const FFFZSFacilitySpec& Spec = Facility.GetFacilitySpec();
	const FFFZSFacilityReadModel ReadModel = Facility.GetReadModel();
	OutViewData.FacilityInstanceId = Facility.GetFacilityInstanceId();
	OutViewData.FacilityDefinitionId = Spec.FacilityDefinitionId;
	OutViewData.Context = SelectionContext;
	OutViewData.ContextTitle = SelectionContext == EFF_FacilityInfoContext::Build
		? LOCTEXT("BuildContextTitle", "배치 시설")
		: LOCTEXT("LiveContextTitle", "운영 시설");
	OutViewData.DisplayName = Spec.DisplayName.IsEmpty()
		? LOCTEXT("UnknownFacility", "이름 없는 시설")
		: Spec.DisplayName;
	FFF_FacilityNameParts NameParts;
	FString NamingFailure;
	UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
		Spec.Category, Spec.FacilityDefinitionId, NameParts, NamingFailure);
	OutViewData.CategoryText = FFF_PrepBuildPresentationRules::ResolveCategoryPathText(
		Spec.Category, NameParts.MinorToken);
	OutViewData.ZoneText = Facility.GetZoneId().IsNone()
		? LOCTEXT("UnknownZone", "구역 정보 없음")
		: FText::FromName(Facility.GetZoneId());
	OutViewData.FacilityStateText = FFFacilityInfoModel::GetFacilityStateText(ReadModel.State);
	OutViewData.CurrentInstancesInWorld = Spec.FacilityDefinitionId.IsNone()
		? 0
		: FMath::Max(0, ZoneSystem->CountFacilitiesByDefinitionId(Spec.FacilityDefinitionId));
	OutViewData.MaxInstancesPerWorld = FMath::Max(0, Spec.MaxInstancesPerWorld);
	OutViewData.bInstanceLimitReached = OutViewData.MaxInstancesPerWorld > 0 &&
		OutViewData.CurrentInstancesInWorld >= OutViewData.MaxInstancesPerWorld;
	OutViewData.InstanceLimitText = FormatInstanceLimit(
		OutViewData.CurrentInstancesInWorld,
		OutViewData.MaxInstancesPerWorld,
		OutViewData.bInstanceLimitReached);
	OutViewData.FootprintText = FFFacilityInfoModel::FormatFootprint(Spec.FootprintSizeCells);
	OutViewData.ClearanceText = FFFacilityInfoModel::FormatDistanceMeters(Spec.ClearanceCells);
	OutViewData.QueueCapacityText = FText::Format(
		LOCTEXT("QueueCapacityFormat", "대기 {0}팀 · 동시 이용 {1}팀"),
		FText::AsNumber(Spec.QueueRules.MaxWaitingAgents),
		FText::AsNumber(Spec.QueueRules.ServiceCapacity));
	OutViewData.QueueCapacityValueText=FText::Format(LOCTEXT("QueueTeamsValue","{0}팀"),FText::AsNumber(Spec.QueueRules.MaxWaitingAgents));
	OutViewData.ServiceCapacityValueText=FText::Format(LOCTEXT("QueueTeamsValue","{0}팀"),FText::AsNumber(Spec.QueueRules.ServiceCapacity));
	OutViewData.ServiceDurationText = FText::Format(
		LOCTEXT("ServiceDurationFormat", "팀당 {0}초"),
		FText::AsNumber(Spec.QueueRules.ServiceDurationSeconds));
	const UFF_FacilityUpgradeSubsystem* Upgrades = World
		? World->GetSubsystem<UFF_FacilityUpgradeSubsystem>() : nullptr;
	FFF_FacilityUpgradePreview UpgradePreview;
	const bool bHasUpgradePreview = Upgrades &&
		Upgrades->GetUpgradePreview(OutViewData.FacilityInstanceId, UpgradePreview);
	FFFacilityInfoModel::ApplyUpgradePreview(bHasUpgradePreview ? &UpgradePreview : nullptr, OutViewData);
	ApplySynergyViewData(Facility, OutViewData);
	if (bHasUpgradePreview && UpgradePreview.CurrentServiceDurationSeconds > 0.0f)
	{
		OutViewData.ServiceDurationText = FText::Format(
			LOCTEXT("ServiceDurationFormat", "팀당 {0}초"),
			FText::AsNumber(UpgradePreview.CurrentServiceDurationSeconds));
	}
	if (Spec.Category == EFFZSFacilityCategory::Lighting || Spec.IsGroundSurface())
	{
		OutViewData.QueueCapacityText = LOCTEXT("LightingNoQueue", "대기열 없음");
		OutViewData.QueueCapacityValueText=FText::FromString(TEXT("—"));
		OutViewData.ServiceCapacityValueText=FText::FromString(TEXT("—"));
		OutViewData.ServiceDurationText = LOCTEXT("LightingNoService", "이용 서비스 없음");
	}
	OutViewData.bNeedsRelocation = ReadModel.bNeedsRelocation;
	OutViewData.bHasLightingRig = Facility.HasLightingRig();
	OutViewData.bCanEditLighting = OutViewData.bHasLightingRig &&
		IsContextCurrentlyAllowed(SelectionContext) && !bPauseMenuPresentationSuppressed;
	OutViewData.LightingState = Facility.GetLightingState();
	OutViewData.FeedbackText = FeedbackText;
	OutViewData.bFeedbackIsError = bFeedbackIsError;

	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (const UFF_BuildFavoritesSubsystem* Favorites = GameInstance
		? GameInstance->GetSubsystem<UFF_BuildFavoritesSubsystem>() : nullptr)
	{
		OutViewData.bIsFavorite = Favorites->IsFavorite(Spec.FacilityDefinitionId);
	}
	const UFF_BuildCatalogSubsystem* Catalog = GameInstance
		? GameInstance->GetSubsystem<UFF_BuildCatalogSubsystem>()
		: nullptr;
	FFF_BuildCatalogEntry CatalogEntry;
	const bool bHasCatalogEntry = Catalog && Catalog->FindEntry(Spec.FacilityDefinitionId, CatalogEntry);
	OutViewData.Thumbnail = bHasCatalogEntry ? CatalogEntry.Thumbnail.Get() : nullptr;
	if (!Catalog)
	{
		OutViewData.ThumbnailPlaceholderText =
			LOCTEXT("ThumbnailUnavailable", "시설 이미지 정보 없음");
	}
	else if (!Catalog->IsCatalogReady())
	{
		OutViewData.ThumbnailPlaceholderText =
			Catalog->GetLoadState() == EFF_BuildCatalogLoadState::Failed
				? LOCTEXT("ThumbnailCatalogFailed", "시설 이미지 정보를 불러오지 못했습니다")
				: LOCTEXT("ThumbnailCatalogLoading", "시설 이미지 정보 불러오는 중");
	}
	else if (!bHasCatalogEntry || CatalogEntry.Thumbnail.IsNull())
	{
		OutViewData.ThumbnailPlaceholderText =
			LOCTEXT("ThumbnailNotAuthored", "등록된 시설 이미지 없음");
	}
	else if (OutViewData.Thumbnail)
	{
		OutViewData.ThumbnailPlaceholderText = FText::GetEmpty();
	}
	else
	{
		const EFF_BuildCatalogThumbnailLoadState ThumbnailState = Catalog->GetThumbnailLoadState();
		OutViewData.ThumbnailPlaceholderText =
			ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Failed ||
			ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Ready
				? LOCTEXT("ThumbnailLoadFailed", "시설 이미지를 표시할 수 없습니다")
				: LOCTEXT("ThumbnailLoading", "시설 이미지 불러오는 중");
	}

	// Catalog entries are presentation snapshots and are not guaranteed to carry projected prices.
	// Resolve from the installed actor's frozen Spec, exactly like the demolition economy bridge.
	const UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
		: nullptr;
	const UFF_FestivalDefinition* Festival = Progression && Progression->IsFestivalStarted()
		? Progression->GetDefinition()
		: nullptr;
	int32 InstallCost = 0;
	FName MatchedCostRow = NAME_None;
	FString CostFailureReason;
	const bool bHasInstallCost = Festival && Festival->FacilityCostTable &&
		FFF_FacilityCostResolver::FindInstallCost(
			Spec,
			Festival->FacilityCostTable,
			InstallCost,
			MatchedCostRow,
			CostFailureReason);
	const int32 RecoveryCost = bHasInstallCost
		? FFF_FacilityCostResolver::CalculateDemolitionRefund(
			InstallCost, Festival->FacilityDemolitionRefundRate)
		: 0;
	OutViewData.InstallCostText = FFFacilityInfoModel::FormatGold(bHasInstallCost, InstallCost);
	// A zero-Gold refund is valid for a free facility or a zero refund rate.
	OutViewData.RecoveryCostText = FFFacilityInfoModel::FormatGold(bHasInstallCost, RecoveryCost);

	const UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge = World
		? World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>()
		: nullptr;
	const UFF_QueueSubsystem* QueueSystem = World ? World->GetSubsystem<UFF_QueueSubsystem>() : nullptr;
	const int32 QueueFacilityId = QueueBridge
		? QueueBridge->FindQueueFacilityId(OutViewData.FacilityInstanceId)
		: INDEX_NONE;
	const bool bQueueRegistered = QueueSystem && QueueFacilityId != INDEX_NONE &&
		QueueSystem->IsFacilityRegistered(QueueFacilityId);
	const FFF_QueueState QueueState = bQueueRegistered && QueueBridge
		? QueueBridge->GetZoneFacilityQueueState(OutViewData.FacilityInstanceId)
		: FFF_QueueState();
	const bool bQueueSupported = QueueBridge && QueueSystem;
	const bool bQueueAvailable = bQueueRegistered && QueueState.FacilityId == QueueFacilityId;
	const FText MissingQueueMetric = bQueueSupported
		? LOCTEXT("PendingQueueMetric", "준비 중")
		: LOCTEXT("UnsupportedQueueMetric", "사용 불가");

	OutViewData.OperationStateText = FFFacilityInfoModel::GetOperationStateText(
		bQueueSupported, bQueueAvailable, QueueState.bIsOpen, QueueState.bAcceptingNew);
	OutViewData.WaitingPartiesText = bQueueAvailable
		? FormatIntegerWithSuffix(QueueState.WaitingPartyCount, LOCTEXT("TeamSuffix", "팀"))
		: MissingQueueMetric;
	OutViewData.OccupancyText = bQueueAvailable
		? FormatOccupancy(QueueState.WaitingPartyCount, QueueState.MaxCapacityParties)
		: MissingQueueMetric;
	OutViewData.RemainingServiceText = bQueueAvailable
		? FormatIntegerWithSuffix(QueueState.RemainingServicePartyCount, LOCTEXT("TeamSuffix", "팀"))
		: MissingQueueMetric;
	const float ExpectedWaitSeconds = bQueueAvailable && QueueState.bIsOpen
		? QueueSystem->GetExpectedWaitSeconds(QueueFacilityId, 1)
		: 0.0f;
	OutViewData.ExpectedWaitText = bQueueAvailable
		? FFFacilityInfoModel::FormatExpectedWait(QueueState.bIsOpen, ExpectedWaitSeconds)
		: MissingQueueMetric;
	OutViewData.ThroughputText = bQueueAvailable
		? FFFacilityInfoModel::FormatThroughput(true, QueueState.GetServiceRatePerMinute())
		: MissingQueueMetric;
	if (Spec.Category == EFFZSFacilityCategory::Lighting || Spec.IsGroundSurface())
	{
		OutViewData.OperationStateText = Spec.IsGroundSurface()
			? (Spec.AllowsFacilityPlacementOnTop()
				? LOCTEXT("TileSurface", "통행 · 시설 설치 가능")
				: LOCTEXT("RoadSurfaceWithoutFacilities", "통행 전용 · 시설 설치 불가"))
			: (Facility.GetLightingState().bEnabled
				? LOCTEXT("LightingOn", "조명 켜짐") : LOCTEXT("LightingOff", "조명 꺼짐"));
		const FText NotApplicable = LOCTEXT("LightingMetricNotApplicable", "해당 없음");
		OutViewData.WaitingPartiesText = NotApplicable;
		OutViewData.OccupancyText = NotApplicable;
		OutViewData.RemainingServiceText = NotApplicable;
		OutViewData.ExpectedWaitText = NotApplicable;
		OutViewData.ThroughputText = NotApplicable;
	}

	const bool bReconfigurePhase = ZoneSystem->GetChangePhase() == EFFZSChangePhase::InitialSetup ||
		ZoneSystem->GetChangePhase() == EFFZSChangePhase::MorningCheck;
	const bool bQueueEmpty = !bQueueAvailable || QueueState.OccupiedMemberCount <= 0;
	const bool bCanModify = SelectionContext == EFF_FacilityInfoContext::Build &&
		bReconfigurePhase && bQueueEmpty && Facility.CanMoveOrRemove();
	OutViewData.bCanMove = bCanModify;
	OutViewData.bCanRemove = bCanModify;

	// Paid-program state is flattened here so WBP bindings never have to join progression cards,
	// reservation handles, facility identity, and stage policy on their own.
	const UFF_ProgramReservationSubsystem* ProgramReservations = GameInstance
		? GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>()
		: nullptr;
	const UFF_ProgramSchedulingSubsystem* ProgramScheduling = GetWorld()
		? GetWorld()->GetSubsystem<UFF_ProgramSchedulingSubsystem>()
		: nullptr;
	const TArray<FFF_ProgramCard> ProgramCatalog = Progression
		? Progression->GetProgramCatalog()
		: TArray<FFF_ProgramCard>();
	TArray<FString> ScheduledProgramSummaries;
	if (ProgramScheduling && ProgramReservations)
	{
		for (const FFF_ScheduledProgram& Scheduled :
			ProgramScheduling->GetScheduledProgramsForFacility(OutViewData.FacilityInstanceId))
		{
			FFF_ProgramReservation Reservation;
			if (!ProgramReservations->FindReservation(Scheduled.ReservationId, Reservation))
			{
				continue;
			}

			const FFF_ProgramCard* Card = ProgramCatalog.FindByPredicate(
				[ProgramId = Scheduled.ProgramId](const FFF_ProgramCard& Candidate)
				{
					return Candidate.ProgramId == ProgramId;
				});
			const FText ProgramName = Card && !Card->DisplayName.IsEmpty()
				? Card->DisplayName
				: FText::FromName(Scheduled.ProgramId);
			ScheduledProgramSummaries.Add(
				FFFacilityInfoModel::FormatScheduledProgramSummary(
					ProgramName,
					Reservation.StartMinutes,
					Reservation.EndMinutes,
					Card != nullptr,
					Card ? Card->CostGold : 0).ToString());
		}
	}

	OutViewData.bHasScheduledProgram = ScheduledProgramSummaries.Num() > 0;
	OutViewData.ScheduledProgramSummaryText = OutViewData.bHasScheduledProgram
		? FText::FromString(FString::Join(ScheduledProgramSummaries, TEXT("\n")))
		: FText::GetEmpty();
	OutViewData.ProgramStatusText = OutViewData.bHasScheduledProgram
		? (ScheduledProgramSummaries.Num() == 1
			? LOCTEXT("OneProgramScheduled", "오늘 편성됨")
			: FText::Format(
				LOCTEXT("ManyProgramsScheduled", "오늘 {0}개 편성됨"),
				FText::AsNumber(ScheduledProgramSummaries.Num())))
		: LOCTEXT("NoProgramScheduled", "오늘 편성 없음");
	OutViewData.bShowProgramSection =
		SelectionContext == EFF_FacilityInfoContext::Build || OutViewData.bHasScheduledProgram;
	OutViewData.bShowProgramScheduleAction = SelectionContext == EFF_FacilityInfoContext::Build;
	OutViewData.ProgramScheduleActionText = OutViewData.bHasScheduledProgram
		? LOCTEXT("AddPaidProgram", "유료 프로그램 추가 편성")
		: LOCTEXT("SchedulePaidProgram", "유료 프로그램 편성");

	if (OutViewData.bShowProgramScheduleAction)
	{
		if (!ProgramScheduling || !Progression)
		{
			OutViewData.ProgramScheduleDisabledReasonText =
				LOCTEXT("ProgramSchedulingUnavailable", "프로그램 편성 정보를 준비 중입니다.");
		}
		else
		{
			bool bHasCompatibleCard = false;
			EFF_PaidProgramScheduleResult FirstBlockedResult =
				EFF_PaidProgramScheduleResult::UnknownProgram;

			for (const FFF_ProgramCard& Card : ProgramCatalog)
			{
				const EFF_PaidProgramScheduleResult Result =
					ProgramScheduling->CanScheduleProgramAtFacility(
						Card.ProgramId, OutViewData.FacilityInstanceId);
				if (Result == EFF_PaidProgramScheduleResult::FacilityTypeMismatch ||
					Result == EFF_PaidProgramScheduleResult::UnknownProgram)
				{
					continue;
				}

				bHasCompatibleCard = true;
				if (Result == EFF_PaidProgramScheduleResult::Success ||
					Result == EFF_PaidProgramScheduleResult::InsufficientGold ||
					Result == EFF_PaidProgramScheduleResult::AlreadyScheduled ||
					Result == EFF_PaidProgramScheduleResult::FacilityTimeConflict)
				{
					// The CTA opens a read-only preview even when this particular card cannot be
					// purchased now. Only the picker's separate confirmation button is gated by Success.
					OutViewData.bCanScheduleProgram = true;
					if (Result == EFF_PaidProgramScheduleResult::Success)
					{
						break;
					}
				}
				FirstBlockedResult = Result;
			}

			if (!OutViewData.bCanScheduleProgram)
			{
				if (!bHasCompatibleCard)
				{
					OutViewData.ProgramScheduleDisabledReasonText = LOCTEXT(
						"NoCompatiblePaidProgram",
						"이 시설에서 열 수 있는 유료 프로그램이 없습니다.");
				}
				else
				{
					OutViewData.ProgramScheduleDisabledReasonText =
						UFF_ProgramSchedulingSubsystem::GetResultText(FirstBlockedResult);
				}
			}
		}
	}

	// This installed facility's actual earnings. The ledger's per-definition rows merge booths of
	// the same type; the panel reads the per-instance book so two identical booths stay distinct.
	FFF_FacilityInstanceRevenue InstanceRevenue;
	const bool bEconomyAvailable = Progression && Progression->IsFestivalStarted();
	if (bEconomyAvailable)
	{
		Progression->GetFacilityInstanceRevenue(OutViewData.FacilityInstanceId, InstanceRevenue);
	}
	const bool bGeneratesDirectRevenue =
		(bHasUpgradePreview && UpgradePreview.bRevenueAvailable && UpgradePreview.CurrentRevenuePerUseGold > 0) ||
		OutViewData.bHasScheduledProgram;
	FFFacilityInfoModel::ApplyFacilityRevenue(
		bEconomyAvailable ? &InstanceRevenue : nullptr,
		!(Spec.Category == EFFZSFacilityCategory::Lighting || Spec.IsGroundSurface()),
		bGeneratesDirectRevenue,
		bEconomyAvailable ? Progression->GetTodayRevenue() : 0,
		OutViewData);

	const UFF_MinigameCatalogSubsystem* MinigameCatalog = GameInstance
		? GameInstance->GetSubsystem<UFF_MinigameCatalogSubsystem>()
		: nullptr;
	const UFF_MinigameDefinition* MinigameDefinition = MinigameCatalog
		? MinigameCatalog->FindDefinitionForFacility(Spec.FacilityDefinitionId)
		: nullptr;
	OutViewData.bHasMinigame = MinigameDefinition != nullptr;
	OutViewData.bCanStartMinigame =
		SelectionContext == EFF_FacilityInfoContext::Live &&
		MinigameDefinition &&
		ReadModel.State == EFFZSFacilityState::Active &&
		!ReadModel.bNeedsRelocation;
	OutViewData.MinigameActionText = MinigameDefinition
		? FText::Format(
			LOCTEXT("StartNamedMinigame", "{0} 시작"),
			MinigameDefinition->DisplayName)
		: FText::GetEmpty();
	return true;
}

void UFF_FacilityInfoSubsystem::RefreshPanel()
{
	if (!HasSelection() || !Panel)
	{
		return;
	}
	if (!IsContextCurrentlyAllowed(SelectionContext))
	{
		ClearSelection();
		return;
	}

	AFFZSFacilityActor* Facility = GetSelectedFacility();
	FFF_FacilityInfoViewData ViewData;
	if (!Facility || !BuildViewData(*Facility, ViewData))
	{
		ClearSelection();
		return;
	}
	Panel->ApplyViewData(ViewData);
	if (bRevenueRankingPresentationOpen) Panel->SetPanelOpen(false);
	if (!bPauseMenuPresentationSuppressed && PaidProgramPicker && IsValid(Facility)
		&& PaidProgramPicker->GetVisibility() != ESlateVisibility::Collapsed)
	{
		FFF_FacilityPaidProgramPickerViewData PickerViewData;
		if (BuildPaidProgramPickerViewData(*Facility, PickerViewData))
		{
			PaidProgramPicker->RefreshViewData(PickerViewData);
		}
	}
}

void UFF_FacilityInfoSubsystem::AcquireThumbnailSession()
{
	if (bThumbnailSessionAcquired)
	{
		return;
	}
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UFF_BuildCatalogSubsystem* Catalog = GameInstance
		? GameInstance->GetSubsystem<UFF_BuildCatalogSubsystem>()
		: nullptr)
	{
		Catalog->AcquireThumbnailPreload();
		bThumbnailSessionAcquired = true;
	}
}

void UFF_FacilityInfoSubsystem::ReleaseThumbnailSession()
{
	if (!bThumbnailSessionAcquired)
	{
		return;
	}
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UFF_BuildCatalogSubsystem* Catalog = GameInstance
		? GameInstance->GetSubsystem<UFF_BuildCatalogSubsystem>()
		: nullptr)
	{
		Catalog->ReleaseThumbnailPreload();
	}
	bThumbnailSessionAcquired = false;
}

void UFF_FacilityInfoSubsystem::EnsurePickControllerBinding()
{
	UWorld* World = GetWorld();
	AFF_FestivalPlayerController* Candidate = World
		? Cast<AFF_FestivalPlayerController>(World->GetFirstPlayerController())
		: nullptr;
	if (!Candidate || !Candidate->IsLocalController())
	{
		ReleasePickControllerBinding();
		return;
	}
	if (BoundPickController.Get() == Candidate)
	{
		return;
	}

	ReleasePickControllerBinding();
	Candidate->OnActorPicked.AddUniqueDynamic(this, &ThisClass::HandleActorPicked);
	BoundPickController = Candidate;
	// Prime once after a valid LocalPlayer appears. A first-click SelectFacility call still retries
	// safely if the viewport was not ready at this exact moment.
	PrimePanel(Candidate);
}

void UFF_FacilityInfoSubsystem::ReleasePickControllerBinding()
{
	if (AFF_FestivalPlayerController* Controller = BoundPickController.Get())
	{
		Controller->OnActorPicked.RemoveAll(this);
	}
	BoundPickController.Reset();
	SelectionIndicatorRefreshAccumulatorSeconds = 0.0f;
	ApplyLiveSelectionIndicatorTargets(nullptr, nullptr);
}

AFFZSFacilityActor* UFF_FacilityInfoSubsystem::ResolveFacilityUnderCursor(
	APlayerController& Controller,
	AFFZSFacilityActor* PreferredHoveredFacility) const
{
	FVector RayOrigin = FVector::ZeroVector;
	FVector RayDirection = FVector::ZeroVector;
	if (!Controller.DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return nullptr;
	}
	return ResolveFacilityAlongRay(
		RayOrigin,
		RayDirection,
		PreferredHoveredFacility,
		Controller.GetPawn());
}

AFFZSFacilityActor* UFF_FacilityInfoSubsystem::ResolveFacilityAlongRay(
	const FVector& RayOrigin,
	const FVector& RayDirection,
	AFFZSFacilityActor* PreferredHoveredFacility,
	const AActor* IgnoredActor) const
{
	UWorld* World = GetWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	const FVector NormalizedRayDirection = RayDirection.GetSafeNormal();
	if (!World || !ZoneSystem || NormalizedRayDirection.IsNearlyZero())
	{
		return nullptr;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FF_FacilityInfoPick), false);
	if (IgnoredActor)
	{
		QueryParams.AddIgnoredActor(IgnoredActor);
	}
	const FVector RayEnd = RayOrigin + NormalizedRayDirection * FacilitySelectionTraceDistanceCm;

	auto IsRegisteredCandidate = [ZoneSystem](const AFFZSFacilityActor* Facility)
	{
		return IsValid(Facility) && Facility->SelectionProxy &&
			Facility->SelectionProxy->GetCollisionEnabled() != ECollisionEnabled::NoCollision &&
			Facility->SelectionProxy->GetCollisionResponseToChannel(
				FFZSCollision::FacilitySelectionTraceChannel) == ECR_Block &&
			Facility->GetFacilityInstanceId().IsValid() &&
			ZoneSystem->FindFacility(Facility->GetFacilityInstanceId()) == Facility;
	};
	auto IsExternallyOccluded = [World, &QueryParams, &RayOrigin, &NormalizedRayDirection](
		const AFFZSFacilityActor& Facility,
		const FHitResult& SelectionHit)
	{
		const float HitDistance = FVector::DotProduct(
			SelectionHit.ImpactPoint - RayOrigin,
			NormalizedRayDirection);
		const float OcclusionDistance = FMath::Max(
			0.0f,
			HitDistance - FacilitySelectionOcclusionEndToleranceCm);
		if (OcclusionDistance <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		// 후보 자체의 mesh/helper는 무시한다. 끝점을 proxy 표면보다 2cm 당겨 지형이나
		// Chaos 접촉 오차가 정확한 경계 클릭을 거짓 가림으로 만들지 않게 한다.
		FCollisionQueryParams OcclusionParams = QueryParams;
		OcclusionParams.AddIgnoredActor(&Facility);
		FHitResult OcclusionHit;
		return World->LineTraceSingleByChannel(
			OcclusionHit,
			RayOrigin,
			RayOrigin + NormalizedRayDirection * OcclusionDistance,
			ECC_Visibility,
			OcclusionParams);
	};

	// Hovered wire box means "clicking now selects this facility". The hover sample is throttled,
	// so validate that exact component against the current click ray before the general world query.
	if (IsRegisteredCandidate(PreferredHoveredFacility))
	{
		FHitResult PreferredHit;
		if (PreferredHoveredFacility->SelectionProxy->LineTraceComponent(
				PreferredHit,
				RayOrigin,
				RayEnd,
				QueryParams) &&
			!IsExternallyOccluded(*PreferredHoveredFacility, PreferredHit))
		{
			return PreferredHoveredFacility;
		}
	}

	bool bHitAnySelectionProxy = false;
	FCollisionQueryParams CandidateQueryParams = QueryParams;
	for (int32 CandidateIndex = 0;
		CandidateIndex < MaxFacilitySelectionCandidates;
		++CandidateIndex)
	{
		FHitResult SelectionProxyHit;
		if (!World->LineTraceSingleByChannel(
				SelectionProxyHit,
				RayOrigin,
				RayEnd,
				FFZSCollision::FacilitySelectionTraceChannel,
				CandidateQueryParams))
		{
			break;
		}
		bHitAnySelectionProxy = true;

		AFFZSFacilityActor* ProxyFacility = Cast<AFFZSFacilityActor>(SelectionProxyHit.GetActor());
		const bool bRegisteredSelectionProxy = IsRegisteredCandidate(ProxyFacility) &&
			SelectionProxyHit.GetComponent() == ProxyFacility->SelectionProxy;
		if (bRegisteredSelectionProxy &&
			!IsExternallyOccluded(*ProxyFacility, SelectionProxyHit))
		{
			return ProxyFacility;
		}

		// A stale/invalid/occluded blocking proxy must not hide a valid candidate behind it. Channel
		// MultiTrace also stops at the first Block, so advance explicitly with a bounded ignore loop.
		if (AActor* HitActor = SelectionProxyHit.GetActor())
		{
			CandidateQueryParams.AddIgnoredActor(HitActor);
		}
		else if (UPrimitiveComponent* HitComponent = SelectionProxyHit.GetComponent())
		{
			CandidateQueryParams.AddIgnoredComponent(HitComponent);
		}
		else
		{
			break;
		}
	}
	if (bHitAnySelectionProxy)
	{
		// 전용 proxy가 가려졌거나 비정상이면 뒤 지면으로 관통 선택하지 않는다.
		return nullptr;
	}

	// Compatibility path for installed actors that do not yet expose a selection proxy. The
	// first Visibility blocker must itself be a walkable ground surface, so walls and obstacles
	// cannot be crossed to reach a footprint behind them.
	FHitResult GroundHit;
	if (!World->LineTraceSingleByChannel(
			GroundHit,
			RayOrigin,
			RayEnd,
			ECC_Visibility,
			QueryParams) ||
		GroundHit.ImpactNormal.Z < 0.45f)
	{
		return nullptr;
	}
	return ZoneSystem->FindFacilityAtWorldPoint(GroundHit.ImpactPoint);
}

void UFF_FacilityInfoSubsystem::UpdateLiveSelectionIndicators(const float DeltaTime)
{
	const bool bIsLiveStage = IsContextCurrentlyAllowed(EFF_FacilityInfoContext::Live);
	AFFZSFacilityActor* SelectedFacility = bIsLiveStage &&
		SelectionContext == EFF_FacilityInfoContext::Live
		? GetSelectedFacility()
		: nullptr;
	AFF_FestivalPlayerController* Controller = BoundPickController.Get();
	const bool bPickingAllowed = Controller && Controller->IsPickingAllowed();
	const bool bCursorVisible = Controller && Controller->bShowMouseCursor;
	const bool bPointerOverPanel = IsPointerOverPanel();
	const bool bShouldResolveHover = FFFacilityInfoSelection::ShouldResolveLiveSelectionHover(
		bIsLiveStage,
		bPickingAllowed,
		bCursorVisible,
		bPointerOverPanel);
	if (!bIsLiveStage || !bPickingAllowed || !bCursorVisible)
	{
		SelectionIndicatorRefreshAccumulatorSeconds = 0.0f;
		ApplyLiveSelectionIndicatorTargets(nullptr, nullptr);
		return;
	}
	if (!bShouldResolveHover)
	{
		SelectionIndicatorRefreshAccumulatorSeconds = 0.0f;
		ApplyLiveSelectionIndicatorTargets(nullptr, SelectedFacility);
		return;
	}

	SelectionIndicatorRefreshAccumulatorSeconds += FMath::Max(0.0f, DeltaTime);
	if (SelectionIndicatorRefreshAccumulatorSeconds < SelectionIndicatorRefreshIntervalSeconds)
	{
		ApplyLiveSelectionIndicatorTargets(
			SelectionIndicatorHoverFacility.Get(),
			SelectedFacility);
		return;
	}
	SelectionIndicatorRefreshAccumulatorSeconds = FMath::Fmod(
		SelectionIndicatorRefreshAccumulatorSeconds,
		SelectionIndicatorRefreshIntervalSeconds);
	ApplyLiveSelectionIndicatorTargets(
		ResolveFacilityUnderCursor(*Controller),
		SelectedFacility);
}

void UFF_FacilityInfoSubsystem::ApplyLiveSelectionIndicatorTargets(
	AFFZSFacilityActor* HoveredFacility,
	AFFZSFacilityActor* SelectedFacility)
{
	if (SelectionIndicatorHoverFacility.Get() == HoveredFacility &&
		SelectionIndicatorSelectedFacility.Get() == SelectedFacility)
	{
		return;
	}

	TArray<AFFZSFacilityActor*, TInlineAllocator<4>> Candidates;
	Candidates.AddUnique(SelectionIndicatorHoverFacility.Get());
	Candidates.AddUnique(SelectionIndicatorSelectedFacility.Get());
	Candidates.AddUnique(HoveredFacility);
	Candidates.AddUnique(SelectedFacility);
	Candidates.Remove(nullptr);

	SelectionIndicatorHoverFacility = HoveredFacility;
	SelectionIndicatorSelectedFacility = SelectedFacility;
	for (AFFZSFacilityActor* Candidate : Candidates)
	{
		const EFFZSFacilitySelectionIndicatorState State = Candidate == SelectedFacility
			? EFFZSFacilitySelectionIndicatorState::Selected
			: Candidate == HoveredFacility
				? EFFZSFacilitySelectionIndicatorState::Hovered
				: EFFZSFacilitySelectionIndicatorState::Hidden;
		Candidate->SetSelectionIndicatorState(State);
	}
}

void UFF_FacilityInfoSubsystem::ClearLiveSelectionIndicators()
{
	SelectionIndicatorRefreshAccumulatorSeconds = 0.0f;
	ApplyLiveSelectionIndicatorTargets(nullptr, nullptr);
	SelectionIndicatorHoverFacility.Reset();
	SelectionIndicatorSelectedFacility.Reset();
}

void UFF_FacilityInfoSubsystem::HandlePanelMoveRequested()
{
	if (bPauseMenuPresentationSuppressed)
	{
		return;
	}
	if (SelectionContext == EFF_FacilityInfoContext::Build && SelectedFacilityId.IsValid())
	{
		OnMoveRequested.Broadcast(SelectedFacilityId);
	}
}

void UFF_FacilityInfoSubsystem::HandlePanelFavoriteRequested()
{
	const AFFZSFacilityActor* Facility = GetSelectedFacility();
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFF_BuildFavoritesSubsystem* Favorites = GameInstance
		? GameInstance->GetSubsystem<UFF_BuildFavoritesSubsystem>() : nullptr;
	if (!bPauseMenuPresentationSuppressed && Facility && Favorites)
	{
		Favorites->ToggleFavorite(Facility->GetFacilitySpec().FacilityDefinitionId);
		RefreshPanel();
	}
}

void UFF_FacilityInfoSubsystem::HandlePanelRemoveRequested()
{
	if (bPauseMenuPresentationSuppressed)
	{
		return;
	}
	if (SelectionContext == EFF_FacilityInfoContext::Build && SelectedFacilityId.IsValid())
	{
		OnRemoveRequested.Broadcast(SelectedFacilityId);
	}
}

void UFF_FacilityInfoSubsystem::HandlePanelUpgradeRequested()
{
	if (bPauseMenuPresentationSuppressed || !Panel ||
		Panel->GetVisibility() == ESlateVisibility::Collapsed ||
		SelectionContext != EFF_FacilityInfoContext::Build || !SelectedFacilityId.IsValid())
	{
		return;
	}
	const FFF_FacilityInfoViewData& ViewData = Panel->GetCurrentViewData();
	if (ViewData.FacilityInstanceId != SelectedFacilityId || !ViewData.bShowUpgradeDetails ||
		!ViewData.bShowUpgradeAction || !ViewData.bCanUpgrade)
	{
		return;
	}

	// Copy the reviewed identity/level before the command emits world events and refreshes the UI.
	const FGuid RequestedFacilityId = SelectedFacilityId;
	const int32 ExpectedLevel = ViewData.CurrentUpgradeLevel;
	UFF_FacilityUpgradeSubsystem* Upgrades = GetWorld()
		? GetWorld()->GetSubsystem<UFF_FacilityUpgradeSubsystem>() : nullptr;
	FText FailureReason;
	const bool bSucceeded = Upgrades &&
		Upgrades->TryUpgradeFacility(RequestedFacilityId, ExpectedLevel, FailureReason);
	if (SelectedFacilityId != RequestedFacilityId)
	{
		return;
	}
	SetFeedback(bSucceeded
		? FText::Format(LOCTEXT("UpgradeSucceeded", "Lv.{0} 강화 완료! 손님을 더 빠르게 응대합니다."),
			FText::AsNumber(ExpectedLevel + 1))
		: (FailureReason.IsEmpty()
			? LOCTEXT("UpgradeFailed", "강화할 수 없습니다. 시설 상태를 다시 확인해 주세요.")
			: FailureReason), !bSucceeded);
}

void UFF_FacilityInfoSubsystem::HandleFacilityLightingChanged(const FGuid FacilityInstanceId)
{
	if (FacilityInstanceId == SelectedFacilityId) RefreshPanel();
}

bool UFF_FacilityInfoSubsystem::CanEditSelectedLighting(const FGuid FacilityId) const
{
	if (bPauseMenuPresentationSuppressed || !Panel ||
		Panel->GetVisibility() == ESlateVisibility::Collapsed || !FacilityId.IsValid() ||
		SelectedFacilityId != FacilityId || !IsContextCurrentlyAllowed(SelectionContext)) return false;
	const FFF_FacilityInfoViewData& ViewData = Panel->GetCurrentViewData();
	const AFFZSFacilityActor* Facility = GetSelectedFacility();
	return ViewData.FacilityInstanceId == FacilityId && ViewData.bCanEditLighting &&
		Facility && Facility->HasLightingRig();
}

void UFF_FacilityInfoSubsystem::HandlePanelLightingRequested(
	const FGuid FacilityId, const FFFZSFacilityLightingState& State)
{
	if (!CanEditSelectedLighting(FacilityId)) return;
	UFFZSWorldSubsystem* ZoneSystem = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	FText Failure;
	const bool bSucceeded = ZoneSystem && ZoneSystem->SetFacilityLighting(FacilityId, State, Failure);
	if (SelectedFacilityId != FacilityId) return;
	// World events may refresh synchronously. Always reread after a failed command as well.
	if (!bSucceeded) SetFeedback(Failure.IsEmpty()
		? LOCTEXT("LightingFailed", "조명을 바꿀 수 없습니다. 시설 상태를 확인해 주세요.") : Failure, true);
	else
	{
		FeedbackText = FText::GetEmpty();
		bFeedbackIsError = false;
		RefreshPanel();
	}
}

void UFF_FacilityInfoSubsystem::HandlePanelLightingResetRequested(const FGuid FacilityId)
{
	if (!CanEditSelectedLighting(FacilityId)) return;
	UFFZSWorldSubsystem* ZoneSystem = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	FText Failure;
	const bool bSucceeded = ZoneSystem && ZoneSystem->ResetFacilityLighting(FacilityId, Failure);
	if (SelectedFacilityId != FacilityId) return;
	SetFeedback(bSucceeded ? LOCTEXT("LightingReset", "시설의 기본 조명 설정으로 되돌렸습니다.")
		: (Failure.IsEmpty() ? LOCTEXT("LightingResetFailed", "기본 조명을 복원할 수 없습니다.") : Failure), !bSucceeded);
}

void UFF_FacilityInfoSubsystem::HandlePanelLightingRollbackRequested(const FGuid FacilityId,const FFFZSFacilityLightingState& State)
{
	// Rollback targets the transaction's original instance, even if selection/phase changed.
	// The Zone subsystem rejects removed instances and validates every restored value.
	if(UFFZSWorldSubsystem* ZoneSystem=GetWorld()?GetWorld()->GetSubsystem<UFFZSWorldSubsystem>():nullptr)
	{ FText Failure; ZoneSystem->SetFacilityLighting(FacilityId,State,Failure); }
}

FSoftClassPath UFF_FacilityInfoSubsystem::GetDefaultPaidProgramPickerClassPath()
{
	return FSoftClassPath(TEXT(
		"/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramPicker."
		"WBP_FF_FacilityPaidProgramPicker_C"));
}

void UFF_FacilityInfoSubsystem::HandlePanelProgramScheduleRequested()
{
	if (bPauseMenuPresentationSuppressed || !Panel ||
		SelectionContext != EFF_FacilityInfoContext::Build ||
		!SelectedFacilityId.IsValid())
	{
		return;
	}

	// Stage/program changes and facility removal refresh or clear this immutable snapshot on the
	// game thread. Matching it again prevents a stale button from routing a previous facility ID.
	const FFF_FacilityInfoViewData& ViewData = Panel->GetCurrentViewData();
	if (ViewData.FacilityInstanceId != SelectedFacilityId ||
		!ViewData.bShowProgramScheduleAction || !ViewData.bCanScheduleProgram)
	{
		return;
	}
	// Public observation seam remains available for analytics/tests; this subsystem still owns the
	// actual picker and authoritative command, so listeners cannot bypass the second confirmation.
	OnProgramScheduleRequested.Broadcast(SelectedFacilityId);

	AFFZSFacilityActor* Facility = GetSelectedFacility();
	APlayerController* Controller = OwningController.Get();
	if (!Facility || !Controller || !EnsurePaidProgramPicker(*Controller))
	{
		SetFeedback(LOCTEXT("PaidProgramPickerOpenFailed", "프로그램 선택 화면을 열 수 없습니다."), true);
		return;
	}

	FFF_FacilityPaidProgramPickerViewData PickerViewData;
	if (!BuildPaidProgramPickerViewData(*Facility, PickerViewData))
	{
		SetFeedback(LOCTEXT("PaidProgramPickerDataFailed", "프로그램 정보를 불러올 수 없습니다."), true);
		return;
	}
	PaidProgramPicker->ApplyViewData(PickerViewData);
	SetPaidProgramPickerOpen(true);
}

bool UFF_FacilityInfoSubsystem::BuildPaidProgramPickerViewData(
	AFFZSFacilityActor& Facility,
	FFF_FacilityPaidProgramPickerViewData& OutViewData,
	const FText& Feedback,
	const bool bIsFeedbackError) const
{
	OutViewData = FFF_FacilityPaidProgramPickerViewData();
	const FGuid FacilityId = Facility.GetFacilityInstanceId();
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
		: nullptr;
	const UFF_ProgramSchedulingSubsystem* Scheduling = World
		? World->GetSubsystem<UFF_ProgramSchedulingSubsystem>()
		: nullptr;
	if (!FacilityId.IsValid() || !Progression || !Scheduling)
	{
		return false;
	}

	OutViewData.FacilityInstanceId = FacilityId;
	OutViewData.AvailableGold = Progression->GetGold();
	OutViewData.FacilityName = Facility.GetFacilitySpec().DisplayName.IsEmpty()
		? FText::FromName(Facility.GetFacilitySpec().FacilityDefinitionId)
		: Facility.GetFacilitySpec().DisplayName;
	OutViewData.FeedbackText = Feedback;
	OutViewData.bFeedbackIsError = bIsFeedbackError;
	for (const FFF_ProgramCard& Card : Progression->GetProgramCatalogForFacility(FacilityId))
	{
		const EFF_PaidProgramScheduleResult Result =
			Scheduling->CanScheduleProgramAtFacility(Card.ProgramId, FacilityId);
		if (Result == EFF_PaidProgramScheduleResult::FacilityTypeMismatch ||
			Result == EFF_PaidProgramScheduleResult::UnknownProgram)
		{
			continue;
		}

		FFF_FacilityPaidProgramCardViewData& CardViewData = OutViewData.Cards.AddDefaulted_GetRef();
		CardViewData.ProgramId = Card.ProgramId;
		CardViewData.DisplayName = Card.DisplayName.IsEmpty()
			? FText::FromName(Card.ProgramId)
			: Card.DisplayName;
		CardViewData.Description = FText::Format(
			LOCTEXT("ProgramCardDescription", "최대 매출 {0} Gold · 정원 모두 참여 시\n당일 한정 프로그램"),
			FText::AsNumber(Card.CalcMaxRevenue()));
		CardViewData.TimeText =
			FFFacilityInfoModel::FormatProgramTimeRange(Card.StartMinutes, Card.EndMinutes);
		CardViewData.CostGold = Card.CostGold;
		CardViewData.CapacityPeople = Card.Capacity;
		CardViewData.TicketFeeGold = Card.FeePerPerson;
		CardViewData.MaxRevenueGold = Card.CalcMaxRevenue();
		CardViewData.CostText = FFFacilityInfoModel::FormatGold(true, Card.CostGold);
		CardViewData.CapacityText = FText::Format(
			LOCTEXT("ProgramCapacity", "정원 {0}명"), FText::AsNumber(Card.Capacity));
		CardViewData.TicketFeeText = FText::Format(
			LOCTEXT("ProgramTicketFee", "참가비 {0} Gold/인"),
			FText::AsNumber(Card.FeePerPerson));
		CardViewData.AvailabilityText =
			UFF_ProgramSchedulingSubsystem::GetResultText(Result);
		CardViewData.bCanSchedule = Result == EFF_PaidProgramScheduleResult::Success;
	}
	return true;
}

void UFF_FacilityInfoSubsystem::HandlePaidProgramConfirmed(
	const FName ProgramId,
	const FGuid FacilityInstanceId)
{
	if (bPauseMenuPresentationSuppressed || !PaidProgramPicker || ProgramId.IsNone() ||
		FacilityInstanceId != SelectedFacilityId ||
		PaidProgramPicker->GetFacilityInstanceId() != FacilityInstanceId)
	{
		return;
	}
	// 아래 권위 명령은 경제/예약 델리게이트를 동기 방송할 수 있다. 수신자가 선택을
	// 닫으면 Picker가 제거되므로, 호출 전 약한 참조를 잡고 반환 뒤 다시 검증한다.
	TWeakObjectPtr<UFF_FacilityPaidProgramPicker> RequestedPicker = PaidProgramPicker;
	UFF_ProgramSchedulingSubsystem* Scheduling = GetWorld()
		? GetWorld()->GetSubsystem<UFF_ProgramSchedulingSubsystem>()
		: nullptr;
	const EFF_PaidProgramScheduleResult Result = Scheduling
		? Scheduling->TryScheduleProgramAtFacility(ProgramId, FacilityInstanceId)
		: EFF_PaidProgramScheduleResult::CommitFailed;
	UFF_FacilityPaidProgramPicker* ActivePicker = RequestedPicker.Get();
	if (!ActivePicker || PaidProgramPicker != ActivePicker ||
		SelectedFacilityId != FacilityInstanceId ||
		ActivePicker->GetFacilityInstanceId() != FacilityInstanceId)
	{
		return;
	}
	if (Result == EFF_PaidProgramScheduleResult::Success)
	{
		const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		const UFF_FestivalProgressionSubsystem* Progression = GameInstance
			? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
		ActivePicker->ShowPurchaseComplete(ProgramId,
			Progression ? Progression->GetGold() : 0);
		SetPaidProgramPickerOpen(true);
		SetFeedback(LOCTEXT("PaidProgramScheduled", "유료 프로그램 구매·편성이 완료되었습니다."), false);
		RefreshPanel();
		return;
	}

	AFFZSFacilityActor* Facility = GetSelectedFacility();
	FFF_FacilityPaidProgramPickerViewData PickerViewData;
	if (Facility && BuildPaidProgramPickerViewData(
		*Facility,
		PickerViewData,
		UFF_ProgramSchedulingSubsystem::GetResultText(Result),
		true))
	{
		ActivePicker->ApplyViewData(PickerViewData);
		SetPaidProgramPickerOpen(true);
	}
}

void UFF_FacilityInfoSubsystem::HandlePaidProgramPickerCloseRequested()
{
	if (bPauseMenuPresentationSuppressed)
	{
		return;
	}
	if (PaidProgramPicker)
	{
		SetPaidProgramPickerOpen(false);
	}
}

void UFF_FacilityInfoSubsystem::HandlePanelMinigameRequested()
{
	if (bPauseMenuPresentationSuppressed)
	{
		return;
	}
	if (SelectionContext == EFF_FacilityInfoContext::Live && SelectedFacilityId.IsValid())
	{
		OnMinigameRequested.Broadcast(SelectedFacilityId);
	}
}

void UFF_FacilityInfoSubsystem::HandlePanelCloseRequested()
{
	if (bPauseMenuPresentationSuppressed)
	{
		return;
	}
	const FGuid ClosingFacilityId = SelectedFacilityId;
	const EFF_FacilityInfoContext ClosingContext = SelectionContext;
	if (ClosingContext == EFF_FacilityInfoContext::Build && ClosingFacilityId.IsValid())
	{
		OnCloseRequested.Broadcast(ClosingFacilityId);
	}
	if (SelectedFacilityId == ClosingFacilityId && SelectionContext == ClosingContext)
	{
		ClearSelection();
	}
}

void UFF_FacilityInfoSubsystem::HandleScheduledProgramsChanged()
{
	if (HasSelection())
	{
		RefreshPanel();
	}
}

void UFF_FacilityInfoSubsystem::HandleFacilityRevenueChanged(const FGuid FacilityInstanceId)
{
	// Several parties can settle in one frame. Instead of re-projecting per payment, pull the
	// next 0.25 s sample forward so the card shows the new amount on the following tick.
	if (HasSelection() && FacilityInstanceId == SelectedFacilityId)
	{
		RefreshAccumulatorSeconds = RefreshIntervalSeconds;
	}
}

void UFF_FacilityInfoSubsystem::HandleStageChanged(
	const EFF_FestivalStage PreviousStage,
	const EFF_FestivalStage NewStage)
{
	(void)PreviousStage;
	(void)NewStage;
	ClosePaidProgramPicker();
	if (HasSelection())
	{
		// RefreshPanel also closes a Live selection immediately when the stage leaves Live.
		RefreshPanel();
	}
}

void UFF_FacilityInfoSubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	if (Event.Type == EFFZSCommandType::Remove)
	{
		const AFFZSFacilityActor* HoveredFacility = SelectionIndicatorHoverFacility.Get();
		if (Event.FacilityInstanceId == SelectedFacilityId)
		{
			ClearLiveSelectionIndicators();
			ClearSelection();
			return;
		}
		if (HoveredFacility &&
			HoveredFacility->GetFacilityInstanceId() == Event.FacilityInstanceId)
		{
			ApplyLiveSelectionIndicatorTargets(
				nullptr,
				SelectionIndicatorSelectedFacility.Get());
		}
		if (HasSelection())
		{
			// Remove events no longer have an actor from which to recover the definition id.
			// Refresh the selected card on the next tick so a shared-definition count cannot go stale.
			RefreshAccumulatorSeconds = RefreshIntervalSeconds;
		}
		return;
	}
	if (Event.FacilityInstanceId != SelectedFacilityId)
	{
		if (Event.Type == EFFZSCommandType::Install && HasSelection())
		{
			UWorld* World = GetWorld();
			UFFZSWorldSubsystem* ZoneSystem = World
				? World->GetSubsystem<UFFZSWorldSubsystem>()
				: nullptr;
			const AFFZSFacilityActor* InstalledFacility = ZoneSystem
				? ZoneSystem->FindFacility(Event.FacilityInstanceId)
				: nullptr;
			const AFFZSFacilityActor* SelectedFacility = GetSelectedFacility();
			if (InstalledFacility && SelectedFacility &&
				InstalledFacility->GetFacilitySpec().FacilityDefinitionId ==
					SelectedFacility->GetFacilitySpec().FacilityDefinitionId)
			{
				RefreshAccumulatorSeconds = RefreshIntervalSeconds;
			}
		}
		return;
	}
	ClosePaidProgramPicker();
	// QueueBridge consumes the same world event. Refresh on the next subsystem tick so all
	// authority adapters have committed their matching read models before the card samples them.
	RefreshAccumulatorSeconds = RefreshIntervalSeconds;
}

void UFF_FacilityInfoSubsystem::HandleWorldSnapshotRestored()
{
	// Snapshot restore replaces actor generations even when a GUID is preserved.
	ClearLiveSelectionIndicators();
	ClearSelection();
}

void UFF_FacilityInfoSubsystem::HandleActorPicked(AActor* PickedActor)
{
	if (bPauseMenuPresentationSuppressed)
	{
		return;
	}
	// The camera delegate is only the already-gated click signal. Facility identity comes from
	// the dedicated selection trace below, so a null or unrelated camera hit is still meaningful.
	(void)PickedActor;
	AFF_FestivalPlayerController* Controller = BoundPickController.Get();
	if (!Controller || IsPointerOverPanel())
	{
		return;
	}
	if (!IsContextCurrentlyAllowed(EFF_FacilityInfoContext::Live))
	{
		if (SelectionContext == EFF_FacilityInfoContext::Live)
		{
			ClearSelection();
		}
		return;
	}

	AFFZSFacilityActor* Facility = ResolveFacilityUnderCursor(
		*Controller,
		SelectionIndicatorHoverFacility.Get());
	ApplyLiveSelectionIndicatorTargets(
		Facility,
		SelectionContext == EFF_FacilityInfoContext::Live
			? GetSelectedFacility()
			: nullptr);
	if (Facility && SelectFacility(Controller, Facility, EFF_FacilityInfoContext::Live))
	{
		return;
	}
	if (SelectionContext == EFF_FacilityInfoContext::Live)
	{
		ClearLiveSelectionIndicators();
		ClearSelection();
	}
}

#undef LOCTEXT_NAMESPACE
