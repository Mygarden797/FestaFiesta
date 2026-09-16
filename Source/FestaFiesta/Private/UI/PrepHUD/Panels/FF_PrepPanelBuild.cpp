// LightTeal PrepHUD build panel runtime presentation.

#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"

#include "Building/FF_BuildCatalogEconomyReadModel.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildCatalogModel.h"
#include "Festival/FF_RequiredFacilitiesSubsystem.h"
#include "TimerManager.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildFavoritesSubsystem.h"
#include "Building/Presentation/FF_BuildCategoryIcons.h"
#include "Building/Presentation/FF_BuildModeGuide.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSWorldSubsystem.h"
#include "UI/FF_UILog.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationModel.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepBuild"

namespace FFPrepBuildPanel
{
	constexpr int32 AllCategoryChoiceId = INDEX_NONE;
}

UFF_PrepPanelBuild::UFF_PrepPanelBuild(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Dragging a virtualized list must scroll the list, not move the whole docked panel.
	bAllowDragToMove = false;
}

void UFF_PrepPanelBuild::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	PresentationModel = NewObject<UFF_PrepBuildPresentationModel>(this);
	if (Button_FavoritesOnly)
	{
		FavoritesFilterStyle = Button_FavoritesOnly->GetStyle();
		bHasFavoritesFilterStyle = true;
	}
	if (!HorizontalBox_Tools && WidgetTree)
	{
		// 최초 생성 WBP는 이 host가 designer variable이 아니었다. 팀 에셋을 다시 저장하지 않고
		// 이름 계약으로 안전하게 복구하며, 신규 생성분은 builder에서 variable로 만든다.
		HorizontalBox_Tools = Cast<UHorizontalBox>(
			WidgetTree->FindWidget(TEXT("HorizontalBox_Tools")));
	}
	if (Button_FilterAll && Button_FilterBooth)
	{
		LegacySelectedCategoryStyle = Button_FilterAll->GetStyle();
		LegacyIdleCategoryStyle = Button_FilterBooth->GetStyle();
		bHasLegacyCategoryStyles = true;
	}
	BuildCategoryFilterSurface();
}

void UFF_PrepPanelBuild::NativeConstruct()
{
	Super::NativeConstruct();
	LastResponsiveListWidth = -1.0f;
	LastResponsiveColumnCount = 0;
	if (Button_PreviousPage) Button_PreviousPage->OnClicked.AddUniqueDynamic(this, &ThisClass::PreviousCatalogPage);
	if (Button_NextPage) Button_NextPage->OnClicked.AddUniqueDynamic(this, &ThisClass::NextCatalogPage);

	if (TileView_Facilities)
	{
		// UListView applies scrollbar visibility to its live Slate widget, not a saved
		// property. NativeConstruct runs after Slate creation, including viewport reattach.
		TileView_Facilities->SetScrollbarVisibility(bUsePagination
			? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		// NativeConstruct can run again on the same panel instance after a viewport reattach.
		TileView_Facilities->OnItemClicked().RemoveAll(this);
		TileView_Facilities->OnItemClicked().AddUObject(this, &ThisClass::HandleTileClicked);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] TileView_Facilities가 없어 시설을 표시할 수 없다."), *GetName());
	}

	if (Button_ReturnToCatalog)
	{
		Button_ReturnToCatalog->OnClicked.AddUniqueDynamic(
			this, &ThisClass::HandleReturnToCatalogClicked);
	}
	if (Button_ExitBuildMode)
	{
		Button_ExitBuildMode->OnClicked.AddUniqueDynamic(
			this, &ThisClass::HandleExitBuildModeClicked);
	}
	if (Button_InspectFacilities)
	{
		Button_InspectFacilities->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleInspectFacilitiesClicked);
	}
	if (Button_FilterAll)
	{
		Button_FilterAll->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFilterAllClicked);
	}
	if (Button_FilterBooth)
	{
		Button_FilterBooth->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFilterBoothClicked);
	}
	if (Button_FilterPerformance)
	{
		Button_FilterPerformance->OnClicked.AddUniqueDynamic(
			this, &ThisClass::HandleFilterPerformanceClicked);
	}
	if (Search_Facilities)
	{
		Search_Facilities->OnTextChanged.AddUniqueDynamic(
			this, &ThisClass::HandleSearchTextChanged);
	}
	if (Button_FavoritesOnly) { Button_FavoritesOnly->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFavoritesOnlyClicked); }
	if (Button_ResetFilters) { Button_ResetFilters->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResetFiltersClicked); }
	for (UFF_PrepChoiceButton* Choice : CategoryChoices)
	{
		if (Choice)
		{
			Choice->OnHudTabButtonClicked.BindUObject(
				this, &ThisClass::HandleCategoryChoiceClicked);
		}
	}
	RefreshCategoryChoiceSelection();
	RefreshMinorCategorySurface();
}

void UFF_PrepPanelBuild::NativeDestruct()
{
	if (Button_PreviousPage) Button_PreviousPage->OnClicked.RemoveDynamic(this, &ThisClass::PreviousCatalogPage);
	if (Button_NextPage) Button_NextPage->OnClicked.RemoveDynamic(this, &ThisClass::NextCatalogPage);
	if (Button_FavoritesOnly) { Button_FavoritesOnly->OnClicked.RemoveDynamic(this, &ThisClass::HandleFavoritesOnlyClicked); }
	if (Button_ResetFilters) { Button_ResetFilters->OnClicked.RemoveDynamic(this, &ThisClass::HandleResetFiltersClicked); }
	// World teardown can bypass the bar's normal close callback. Keep opt-in and delegates balanced.
	EndPanelSession();

	if (TileView_Facilities)
	{
		TileView_Facilities->OnItemClicked().RemoveAll(this);
	}
	if (Button_ReturnToCatalog)
	{
		Button_ReturnToCatalog->OnClicked.RemoveDynamic(
			this, &ThisClass::HandleReturnToCatalogClicked);
	}
	if (Button_ExitBuildMode)
	{
		Button_ExitBuildMode->OnClicked.RemoveDynamic(
			this, &ThisClass::HandleExitBuildModeClicked);
	}
	if (Button_InspectFacilities)
	{
		Button_InspectFacilities->OnClicked.RemoveDynamic(this, &ThisClass::HandleInspectFacilitiesClicked);
	}
	if (Button_FilterAll)
	{
		Button_FilterAll->OnClicked.RemoveDynamic(this, &ThisClass::HandleFilterAllClicked);
	}
	if (Button_FilterBooth)
	{
		Button_FilterBooth->OnClicked.RemoveDynamic(this, &ThisClass::HandleFilterBoothClicked);
	}
	if (Button_FilterPerformance)
	{
		Button_FilterPerformance->OnClicked.RemoveDynamic(
			this, &ThisClass::HandleFilterPerformanceClicked);
	}
	if (Search_Facilities)
	{
		Search_Facilities->OnTextChanged.RemoveDynamic(
			this, &ThisClass::HandleSearchTextChanged);
	}
	for (UFF_PrepChoiceButton* Choice : CategoryChoices)
	{
		if (Choice)
		{
			Choice->OnHudTabButtonClicked.Unbind();
		}
	}

	for (UFF_PrepChoiceButton* Choice : MinorChoices)
	{
		if (Choice)
		{
			Choice->OnHudTabButtonClicked.Unbind();
		}
	}
	Super::NativeDestruct();
}

void UFF_PrepPanelBuild::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bUsePagination || !TileView_Facilities || !TileView_Facilities->IsVisible()) return;
	const float Width = TileView_Facilities->GetCachedGeometry().GetLocalSize().X;
	const int32 Columns = FMath::Max(1, CardsPerPage);
	if (!FMath::IsFinite(Width) || Width <= 0.0f ||
		(FMath::IsNearlyEqual(Width, LastResponsiveListWidth, 0.5f) && Columns == LastResponsiveColumnCount)) return;
	LastResponsiveListWidth = Width;
	LastResponsiveColumnCount = Columns;
	// UTileView adds entry padding to SetEntryWidth. Reserve the full authored spacing
	// and two Slate units of rounding tolerance so the last card never wraps to row two.
	const float EntryWidth = FMath::Max(1.0f,
		(Width - 2.0f) / Columns - TileView_Facilities->GetHorizontalEntrySpacing());
	TileView_Facilities->SetEntryWidth(EntryWidth);
}

int32 UFF_PrepPanelBuild::GetCatalogPageCount() const
{
	const int32 Count = PresentationModel ? PresentationModel->GetVisibleTiles().Num() : 0;
	if (Count == 0) return 0;
	return bUsePagination ? FMath::DivideAndRoundUp(Count, FMath::Max(1, CardsPerPage)) : 1;
}

int32 UFF_PrepPanelBuild::GetCatalogPageIndex() const
{
	return FMath::Clamp(CatalogPageIndex, 0, FMath::Max(0, GetCatalogPageCount() - 1));
}

TArray<UObject*> UFF_PrepPanelBuild::GetCatalogPageItems() const
{
	TArray<UObject*> PageItems;
	if (!PresentationModel) return PageItems;
	const TArray<TObjectPtr<UFF_PrepBuildTileData>>& FilteredItems = PresentationModel->GetVisibleTiles();
	const int32 Start = bUsePagination ? GetCatalogPageIndex() * FMath::Max(1, CardsPerPage) : 0;
	const int32 Count = bUsePagination
		? FMath::Min(FMath::Max(1, CardsPerPage), FilteredItems.Num() - Start) : FilteredItems.Num();
	PageItems.Reserve(Count);
	for (int32 Index = Start; Index < Start + Count; ++Index) PageItems.Add(FilteredItems[Index]);
	return PageItems;
}

bool UFF_PrepPanelBuild::CanGoToPreviousPage() const
{
	return bUsePagination && GetCatalogPageCount() > 0 && GetCatalogPageIndex() > 0;
}

bool UFF_PrepPanelBuild::CanGoToNextPage() const
{
	return bUsePagination && GetCatalogPageIndex() + 1 < GetCatalogPageCount();
}

void UFF_PrepPanelBuild::SetCatalogPage(const int32 PageIndex)
{
	const int32 NewPage = FMath::Clamp(PageIndex, 0, FMath::Max(0, GetCatalogPageCount() - 1));
	if (CatalogPageIndex == NewPage) return;
	CatalogPageIndex = NewPage;
	ApplyPresentation(true);
	if (TileView_Facilities) TileView_Facilities->ScrollToTop();
}

void UFF_PrepPanelBuild::NextCatalogPage()
{
	if (CanGoToNextPage()) SetCatalogPage(GetCatalogPageIndex() + 1);
}

void UFF_PrepPanelBuild::PreviousCatalogPage()
{
	if (CanGoToPreviousPage()) SetCatalogPage(GetCatalogPageIndex() - 1);
}

void UFF_PrepPanelBuild::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	if (bPanelOpen)
	{
		return;
	}

	bPanelOpen = true;
	bClosingPanel = false;
	// The HUD reuses this panel: retain the last major/minor category on reopen.
	// Search and favorites remain scoped to each browsing session.
	Filter.SearchText.Reset();
	Filter.bFavoritesOnly = false;
	Filter.FavoriteIds.Reset();
	CatalogPageIndex = 0;
	RefreshCategoryChoiceSelection();
	if (Search_Facilities)
	{
		Search_Facilities->SetText(FText::GetEmpty());
	}

	BindExternalSources();
	if (!BuildModeSubsystem)
	{
		if (Text_Status)
		{
			Text_Status->SetText(LOCTEXT("BuildModeUnavailable", "이 월드에서는 배치 기능을 사용할 수 없습니다."));
		}
		RequestClose();
		return;
	}

	// Explicit opt-in preserves the legacy full-screen catalog for every caller except this panel.
	BuildModeSubsystem->SetExternalCatalogPresentationActive(true);
	BuildModeSubsystem->OpenCatalog();
	if (!BuildModeSubsystem ||
		BuildModeSubsystem->GetBuildModeState() != EFF_BuildModeState::CatalogOpen)
	{
		// Policy/zone refusal keeps BuildMode out of CatalogOpen. Do not leave a selected,
		// header-only tab whose two content surfaces are both collapsed.
		RequestClose();
		return;
	}

	RevealSynergyRecommendations();
	RefreshPricedEntries();
	ReconcilePresentation();
	ApplyBuildModeState(BuildModeSubsystem->GetBuildModeState());
}

void UFF_PrepPanelBuild::NativeOnPanelClosed()
{
	EndPanelSession();
	Super::NativeOnPanelClosed();
}

void UFF_PrepPanelBuild::EndPanelSession()
{
	if (!bPanelOpen && !CatalogSubsystem && !EconomyReadModel && !BuildModeSubsystem && !ZoneSystem)
	{
		return;
	}

	bClosingPanel = true;
	if (BuildModeSubsystem)
	{
		// Closing the Build tab means leaving its world interaction session as well.
		if (BuildModeSubsystem->GetBuildModeState() != EFF_BuildModeState::Closed)
		{
			BuildModeSubsystem->ExitBuildMode();
		}
		BuildModeSubsystem->SetExternalCatalogPresentationActive(false);
	}

	UnbindExternalSources();
	PricedEntries.Reset();
	if (TileView_Facilities)
	{
		TileView_Facilities->ClearListItems();
	}
	bPanelOpen = false;
	bClosingPanel = false;
}

void UFF_PrepPanelBuild::BindExternalSources()
{
	UGameInstance* GameInstance = GetGameInstance();
	CatalogSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFF_BuildCatalogSubsystem>()
		: nullptr;
	EconomyReadModel = GameInstance
		? GameInstance->GetSubsystem<UFF_BuildCatalogEconomyReadModel>()
		: nullptr;
	BuildModeSubsystem = GetWorld()
		? GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>()
		: nullptr;
	ZoneSystem = GetWorld()
		? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr;
	FavoritesSubsystem = GameInstance ? GameInstance->GetSubsystem<UFF_BuildFavoritesSubsystem>() : nullptr;
	RequiredFacilitiesSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UFF_RequiredFacilitiesSubsystem>() : nullptr;
	bRequiredFacilitiesDirty = true;
	if (RequiredFacilitiesSubsystem)
	{
		RequiredFacilitiesSubsystem->OnRequiredFacilitiesChanged.AddUniqueDynamic(
			this, &ThisClass::HandleRequiredFacilitiesChanged);
	}
	if (FavoritesSubsystem)
	{
		FavoritesSubsystem->OnFavoritesChanged.RemoveAll(this);
		FavoritesSubsystem->OnFavoritesChanged.AddUObject(this, &ThisClass::HandleFavoritesChanged);
		Filter.FavoriteIds = FavoritesSubsystem->GetFavoriteIds();
	}

	if (CatalogSubsystem)
	{
		CatalogSubsystem->OnCatalogChanged.AddUniqueDynamic(this, &ThisClass::HandleCatalogChanged);
		CatalogSubsystem->OnCatalogThumbnailsChanged.AddUniqueDynamic(
			this, &ThisClass::HandleCatalogThumbnailsChanged);
	}
	if (EconomyReadModel)
	{
		EconomyReadModel->OnGoldChanged.AddUniqueDynamic(this, &ThisClass::HandleGoldChanged);
	}
	if (BuildModeSubsystem)
	{
		BuildModeSubsystem->OnBuildModeStateChanged.AddUniqueDynamic(
			this, &ThisClass::HandleBuildModeStateChanged);
		BuildModeSubsystem->OnBuildCatalogFocusRestoreRequested.AddUniqueDynamic(
			this, &ThisClass::HandleBuildCatalogFocusRestoreRequested);
		BuildModeSubsystem->OnSynergyCatalogContextChanged.AddUniqueDynamic(
			this, &ThisClass::HandleSynergyCatalogContextChanged);
		BuildModeSubsystem->OnBuildCatalogFacilityFocusRequested.AddUniqueDynamic(
			this, &ThisClass::HandleCatalogFacilityFocusRequested);
		PendingCatalogFocusId = BuildModeSubsystem->GetPendingCatalogFacilityId();
		// The inspector may have been opened before this panel existed.
		bRevealSynergyOnCatalogOpen = !BuildModeSubsystem->GetSynergyCatalogFacilityIds().IsEmpty();
	}
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.AddUniqueDynamic(this, &ThisClass::HandleZoneWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.AddUniqueDynamic(
			this, &ThisClass::HandleZoneWorldSnapshotRestored);
	}
}

void UFF_PrepPanelBuild::UnbindExternalSources()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(RequiredFacilitiesRefreshTimer); }
	if (RequiredFacilitiesSubsystem)
	{
		RequiredFacilitiesSubsystem->OnRequiredFacilitiesChanged.RemoveDynamic(
			this, &ThisClass::HandleRequiredFacilitiesChanged);
	}
	RequiredFacilitiesSubsystem = nullptr;
	RequiredFacilitiesSnapshot = FFF_RequiredFacilitiesStatus();
	bRequiredFacilitiesDirty = true;
	if (FavoritesSubsystem) { FavoritesSubsystem->OnFavoritesChanged.RemoveAll(this); }
	FavoritesSubsystem = nullptr;
	if (CatalogSubsystem)
	{
		CatalogSubsystem->OnCatalogChanged.RemoveDynamic(this, &ThisClass::HandleCatalogChanged);
		CatalogSubsystem->OnCatalogThumbnailsChanged.RemoveDynamic(
			this, &ThisClass::HandleCatalogThumbnailsChanged);
	}
	if (EconomyReadModel)
	{
		EconomyReadModel->OnGoldChanged.RemoveDynamic(this, &ThisClass::HandleGoldChanged);
	}
	if (BuildModeSubsystem)
	{
		BuildModeSubsystem->OnBuildModeStateChanged.RemoveDynamic(
			this, &ThisClass::HandleBuildModeStateChanged);
		BuildModeSubsystem->OnBuildCatalogFocusRestoreRequested.RemoveDynamic(
			this, &ThisClass::HandleBuildCatalogFocusRestoreRequested);
		BuildModeSubsystem->OnSynergyCatalogContextChanged.RemoveDynamic(
			this, &ThisClass::HandleSynergyCatalogContextChanged);
		BuildModeSubsystem->OnBuildCatalogFacilityFocusRequested.RemoveDynamic(
			this, &ThisClass::HandleCatalogFacilityFocusRequested);
	}
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.RemoveDynamic(this, &ThisClass::HandleZoneWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.RemoveDynamic(
			this, &ThisClass::HandleZoneWorldSnapshotRestored);
	}

	CatalogSubsystem = nullptr;
	EconomyReadModel = nullptr;
	BuildModeSubsystem = nullptr;
	ZoneSystem = nullptr;
	bRevealSynergyOnCatalogOpen = false;
	PendingCatalogFocusId = NAME_None;
}

void UFF_PrepPanelBuild::HandleCatalogChanged()
{
	RefreshPricedEntries();
	ReconcilePresentation();
}

void UFF_PrepPanelBuild::HandleCatalogThumbnailsChanged()
{
	// Rebind the displayed rows in place. Destroying pooled rows during scrolling briefly
	// exposes empty entries and needlessly recomputes their desired size.
	RefreshVisibleTiles();
	ApplyPresentation(false);
}

void UFF_PrepPanelBuild::HandleGoldChanged(const int32 NewGold)
{
	if (!PresentationModel)
	{
		return;
	}

	const bool bHasGold = EconomyReadModel && EconomyReadModel->HasAuthoritativeGold();
	PresentationModel->RefreshGold(NewGold, bHasGold);
	RefreshVisibleTiles();
	ApplyPresentation(false);
}

void UFF_PrepPanelBuild::HandleBuildModeStateChanged(
	const EFF_BuildModeState PreviousState,
	const EFF_BuildModeState NewState)
{
	UE_LOG(LogFestaFiestaUI, Verbose,
		TEXT("[%s] 배치 표현 상태 전환: %s -> %s"),
		*GetName(), *UEnum::GetValueAsString(PreviousState), *UEnum::GetValueAsString(NewState));

	if (NewState != EFF_BuildModeState::CatalogOpen) { PendingCatalogFocusId = NAME_None; }
	if (!bPanelOpen || bClosingPanel)
	{
		return;
	}

	if (NewState == EFF_BuildModeState::CatalogOpen)
	{
		bRequiredFacilitiesDirty = true;
		RevealSynergyRecommendations();
		// 설치·철거 뒤 목록으로 돌아올 때 현재 등록 수를 새 스냅샷으로 투영합니다.
		RefreshPricedEntries();
		ReconcilePresentation();
	}
	ApplyBuildModeState(NewState);
	if (NewState == EFF_BuildModeState::Closed)
	{
		// 명시적 배치 종료와 stage-policy 종료는 BuildMode에서 오며, ESC/F5 Pause는 이 panel 상태를 보존한다.
		RequestClose();
	}
}

FReply UFF_PrepPanelBuild::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const UFF_FestivalSettingsDeveloperSettings* Settings =
		UFF_FestivalSettingsDeveloperSettings::Get();
	const EFF_BuildModeState CurrentState = BuildModeSubsystem
		? BuildModeSubsystem->GetBuildModeState()
		: EFF_BuildModeState::Closed;
	if (Settings && FFPrepBuildPauseModel::ShouldHandlePauseMenuKey(
		InKeyEvent.GetKey(),
		InKeyEvent.IsRepeat(),
		bPanelOpen,
		bClosingPanel,
		CurrentState,
		Settings->OpenCloseKeys))
	{
		// Preview 단계에서 child 검색창보다 먼저 받아 ESC/F5가 텍스트 편집 동작으로 소실되지 않게 한다.
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UFF_FestivalSettingsSubsystem* SettingsSubsystem =
				GameInstance->GetSubsystem<UFF_FestivalSettingsSubsystem>())
			{
				SettingsSubsystem->OpenPauseMenu(GetOwningPlayer());
			}
		}
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UFF_PrepPanelBuild::HandleSynergyCatalogContextChanged()
{
	if (!bPanelOpen || bClosingPanel || !BuildModeSubsystem) { return; }
	bRevealSynergyOnCatalogOpen = !BuildModeSubsystem->GetSynergyCatalogFacilityIds().IsEmpty();
	if (BuildModeSubsystem->GetBuildModeState() == EFF_BuildModeState::CatalogOpen)
	{
		RevealSynergyRecommendations();
		ReconcilePresentation();
	}
}

void UFF_PrepPanelBuild::HandleCatalogFacilityFocusRequested(const FName FacilityDefinitionId)
{
	if (!bPanelOpen || bClosingPanel || !BuildModeSubsystem ||
		BuildModeSubsystem->GetBuildModeState() != EFF_BuildModeState::CatalogOpen) { return; }
	RequestFacilityFocus(FacilityDefinitionId);
}

void UFF_PrepPanelBuild::RequestFacilityFocus(const FName FacilityDefinitionId)
{
	if (FacilityDefinitionId.IsNone()) { return; }
	PendingCatalogFocusId = FacilityDefinitionId;
	ReconcilePresentation();
}

void UFF_PrepPanelBuild::HandleRequiredFacilitiesChanged()
{
	bRequiredFacilitiesDirty = true;
	// Coalesce this notification with Install/Remove's existing catalog update.
	// Hidden placement sessions only invalidate; browsing refreshes once on return.
	if (bPanelOpen && !bClosingPanel && BuildModeSubsystem && GetWorld() &&
		BuildModeSubsystem->GetBuildModeState() == EFF_BuildModeState::CatalogOpen &&
		!RequiredFacilitiesRefreshTimer.IsValid())
	{
		RequiredFacilitiesRefreshTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(
			this, &ThisClass::RefreshRequiredFacilitiesPresentation);
	}
}

void UFF_PrepPanelBuild::RefreshRequiredFacilitiesPresentation()
{
	RequiredFacilitiesRefreshTimer.Invalidate();
	if (bRequiredFacilitiesDirty && bPanelOpen && !bClosingPanel && BuildModeSubsystem &&
		BuildModeSubsystem->GetBuildModeState() == EFF_BuildModeState::CatalogOpen)
	{
		ReconcilePresentation();
	}
}

void UFF_PrepPanelBuild::ApplyRequiredFacilitiesSnapshot()
{
	const bool bCatalogVisible = bPanelOpen && BuildModeSubsystem &&
		BuildModeSubsystem->GetBuildModeState() == EFF_BuildModeState::CatalogOpen;
	if (bRequiredFacilitiesDirty && (bCatalogVisible || !RequiredFacilitiesSubsystem))
	{
		RequiredFacilitiesSnapshot = RequiredFacilitiesSubsystem
			? RequiredFacilitiesSubsystem->GetRequiredFacilitiesStatus() : FFF_RequiredFacilitiesStatus();
		bRequiredFacilitiesDirty = false;
	}
	FFBuildCatalogModel::ApplyRequiredFacilities(PricedEntries, RequiredFacilitiesSnapshot);
}

void UFF_PrepPanelBuild::RevealSynergyRecommendations()
{
	if (!bRevealSynergyOnCatalogOpen) { return; }
	bRevealSynergyOnCatalogOpen = false;
	// A newly inspected facility starts on all categories so cross-category partners are visible.
	// Later category/search/favorites choices remain under the player's control.
	Filter.bFilterByCategory = false;
	Filter.MinorToken = NAME_None;
	Filter.SearchText.Reset();
	Filter.bFavoritesOnly = false;
	CatalogPageIndex = 0;
	if (Search_Facilities) { Search_Facilities->SetText(FText::GetEmpty()); }
	RefreshCategoryChoiceSelection();
	RefreshMinorCategorySurface();
	if (TileView_Facilities) { TileView_Facilities->ScrollToTop(); }
}

void UFF_PrepPanelBuild::HandleBuildCatalogFocusRestoreRequested()
{
	if (!bPanelOpen || bClosingPanel || !BuildModeSubsystem ||
		BuildModeSubsystem->GetBuildModeState() != EFF_BuildModeState::CatalogOpen)
	{
		return;
	}

	// TileView 재생성으로 exact Slate entry가 사라진 경우에도 키보드 탐색의 안정된 시작점을 제공한다.
	FocusSelectedCategoryChoice();
}

void UFF_PrepPanelBuild::HandleZoneWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	if ((Event.Type != EFFZSCommandType::Install && Event.Type != EFFZSCommandType::Remove) ||
		!bPanelOpen || bClosingPanel || !BuildModeSubsystem ||
		BuildModeSubsystem->GetBuildModeState() != EFF_BuildModeState::CatalogOpen)
	{
		return;
	}

	// 카탈로그가 보이는 동안 수량이 바뀌는 설치·철거만 즉시 다시 투영한다.
	RefreshPricedEntries();
	ReconcilePresentation();
}

void UFF_PrepPanelBuild::HandleZoneWorldSnapshotRestored()
{
	if (!bPanelOpen || bClosingPanel || !BuildModeSubsystem ||
		BuildModeSubsystem->GetBuildModeState() != EFF_BuildModeState::CatalogOpen)
	{
		return;
	}

	RefreshPricedEntries();
	ReconcilePresentation();
}

void UFF_PrepPanelBuild::HandleTileClicked(UObject* ItemObject)
{
	UFF_PrepBuildTileData* Tile = Cast<UFF_PrepBuildTileData>(ItemObject);
	if (!Tile || !BuildModeSubsystem ||
		BuildModeSubsystem->GetBuildModeState() != EFF_BuildModeState::CatalogOpen)
	{
		return;
	}

	if (!Tile->IsAffordable())
	{
		if (Text_Status)
		{
			const FText Reason = Tile->GetStateText();
			Text_Status->SetText(Reason.IsEmpty()
				? LOCTEXT("UnavailableTile", "지금은 이 시설을 선택할 수 없습니다.")
				: Reason);
		}
		if (TileView_Facilities)
		{
			TileView_Facilities->SetSelectedItem(nullptr);
		}
		return;
	}

	if (!BuildModeSubsystem->StartPlacement(Tile->GetFacilityDefinitionId()))
	{
		// 클릭 직전 다른 설치가 상한을 채운 경우 새 권위 수량을 다시 투영해 일반 오류 대신 정확한 원인을 남깁니다.
		RefreshPricedEntries();
		ReconcilePresentation();
		if (Text_Status)
		{
			const UFF_PrepBuildTileData* RefreshedTile = PresentationModel
				? PresentationModel->FindTile(Tile->GetFacilityDefinitionId())
				: nullptr;
			const FText Reason = RefreshedTile ? RefreshedTile->GetStateText() : FText::GetEmpty();
			Text_Status->SetText(Reason.IsEmpty()
				? LOCTEXT(
					"PlacementStartFailed", "배치를 시작하지 못했습니다. 현재 단계와 건축 가능 구역을 확인하세요.")
				: Reason);
		}
		if (TileView_Facilities)
		{
			TileView_Facilities->SetSelectedItem(nullptr);
		}
	}
}

void UFF_PrepPanelBuild::HandleReturnToCatalogClicked()
{
	if (BuildModeSubsystem && BuildModeSubsystem->GetBuildModeState() != EFF_BuildModeState::Closed)
	{
		BuildModeSubsystem->ReturnToCatalog();
	}
}

void UFF_PrepPanelBuild::HandleExitBuildModeClicked()
{
	if (BuildModeSubsystem)
	{
		BuildModeSubsystem->ExitBuildMode();
	}
}

void UFF_PrepPanelBuild::HandleFilterAllClicked()
{
	SetCategoryFilter(false, EFFZSFacilityCategory::Booth);
}

void UFF_PrepPanelBuild::HandleFilterBoothClicked()
{
	SetCategoryFilter(true, EFFZSFacilityCategory::Booth);
}

void UFF_PrepPanelBuild::HandleFilterPerformanceClicked()
{
	SetCategoryFilter(true, EFFZSFacilityCategory::Performance);
}

void UFF_PrepPanelBuild::HandleSearchTextChanged(const FText& NewText)
{
	if (bApplyingCatalogFocus) { return; }
	if (Filter.SearchText != NewText.ToString()) CatalogPageIndex = 0;
	Filter.SearchText = NewText.ToString();
	if (TileView_Facilities)
	{
		TileView_Facilities->ScrollToTop();
	}
	ReconcilePresentation();
}

void UFF_PrepPanelBuild::HandleFavoritesChanged()
{
	Filter.FavoriteIds = FavoritesSubsystem ? FavoritesSubsystem->GetFavoriteIds() : TSet<FName>();
	ReconcilePresentation();
	// Removing a focused favorite can release that virtualized entry. Keep keyboard navigation in the panel.
	if (Filter.bFavoritesOnly && Button_FavoritesOnly) { Button_FavoritesOnly->SetFocus(); }
}

void UFF_PrepPanelBuild::HandleFavoritesOnlyClicked()
{
	Filter.bFavoritesOnly = !Filter.bFavoritesOnly;
	CatalogPageIndex = 0;
	if (TileView_Facilities) { TileView_Facilities->ScrollToTop(); }
	ReconcilePresentation();
}

void UFF_PrepPanelBuild::HandleResetFiltersClicked()
{
	CatalogPageIndex = 0;
	Filter.bFilterByCategory = false;
	Filter.MinorToken = NAME_None;
	Filter.SearchText.Reset();
	if (Filter.FavoriteIds.IsEmpty()) { Filter.bFavoritesOnly = false; }
	if (Search_Facilities) { Search_Facilities->SetText(FText::GetEmpty()); }
	RefreshCategoryChoiceSelection();
	RefreshMinorCategorySurface();
	ReconcilePresentation();
}

void UFF_PrepPanelBuild::HandleCategoryChoiceClicked(const int32 ChoiceId)
{
	if (ChoiceId == FFPrepBuildPanel::AllCategoryChoiceId)
	{
		SetCategoryFilter(false, EFFZSFacilityCategory::Booth);
		return;
	}

	for (const EFFZSFacilityCategory Category :
		FFF_PrepBuildPresentationRules::GetCategoryOrder())
	{
		if (static_cast<int32>(Category) == ChoiceId)
		{
			SetCategoryFilter(true, Category);
			return;
		}
	}

	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[%s] 알 수 없는 시설 카테고리 선택 ID를 무시한다: %d"), *GetName(), ChoiceId);
}

void UFF_PrepPanelBuild::SetCategoryFilter(
	const bool bEnabled,
	const EFFZSFacilityCategory Category)
{
	if (Filter.bFilterByCategory != bEnabled || (bEnabled && Filter.Category != Category) || !Filter.MinorToken.IsNone())
		CatalogPageIndex = 0;
	Filter.bFilterByCategory = bEnabled;
	Filter.Category = Category;
	Filter.MinorToken = NAME_None;
	if (ScrollBox_Subcategories) ScrollBox_Subcategories->ScrollToStart();
	RefreshCategoryChoiceSelection();
	RefreshMinorCategorySurface();
	if (TileView_Facilities)
	{
		TileView_Facilities->ScrollToTop();
	}
	ReconcilePresentation();
}

void UFF_PrepPanelBuild::HandleInspectFacilitiesClicked()
{
	if (BuildModeSubsystem && BuildModeSubsystem->GetBuildModeState() == EFF_BuildModeState::CatalogOpen)
	{
		BuildModeSubsystem->NotifyBuildToolsInteraction();
		BuildModeSubsystem->EnterBuildMode();
	}
}

void UFF_PrepPanelBuild::HandleMinorChoiceClicked(const int32 ChoiceId)
{
	if (!MinorTokens.IsValidIndex(ChoiceId))
	{
		return;
	}
	if (Filter.MinorToken != MinorTokens[ChoiceId]) CatalogPageIndex = 0;
	Filter.MinorToken = MinorTokens[ChoiceId];
	for (UFF_PrepChoiceButton* Choice : MinorChoices)
	{
		if (Choice)
		{
			const bool bSelected = Choice->GetChoiceId() == ChoiceId;
			Choice->SetSelected(bSelected);
			if (bSelected && ScrollBox_Subcategories)
				ScrollBox_Subcategories->ScrollWidgetIntoView(Choice, false, EDescendantScrollDestination::IntoView, 4.0f);
		}
	}
	if (TileView_Facilities)
	{
		TileView_Facilities->ScrollToTop();
	}
	ReconcilePresentation();
}

void UFF_PrepPanelBuild::RefreshMinorCategorySurface()
{
	if (!HorizontalBox_Subcategories)
	{
		return;
	}
	UClass* ChoiceClass = ResolveWidgetClass(CategoryChoiceClass, CategoryChoiceFallbackPath);
	if (!ChoiceClass || !ChoiceClass->IsChildOf(UFF_PrepChoiceButton::StaticClass()))
	{
		return;
	}

	TArray<FName> AvailableTokens;
	for (const FFF_BuildCatalogEntry& Entry : PricedEntries)
	{
		if (Filter.bFilterByCategory && Entry.Category == Filter.Category &&
			!Entry.MinorToken.IsNone())
		{
			AvailableTokens.AddUnique(Entry.MinorToken);
		}
	}
	AvailableTokens.Sort([](const FName A, const FName B)
	{
		return FFF_PrepBuildPresentationRules::ResolveMinorDisplayName(A).ToString() <
			FFF_PrepBuildPresentationRules::ResolveMinorDisplayName(B).ToString();
	});
	AvailableTokens.Insert(NAME_None, 0);
	// A reconstructed or loading panel has no snapshot yet; it cannot invalidate
	// the remembered subcategory until the catalog has finished loading.
	if (CatalogSubsystem && CatalogSubsystem->GetLoadState() == EFF_BuildCatalogLoadState::Ready &&
		!AvailableTokens.Contains(Filter.MinorToken))
	{
		Filter.MinorToken = NAME_None;
		CatalogPageIndex = 0;
	}

	// Catalog changes are infrequent. Rebuild chips only when their keys change; gold and
	// thumbnail notifications keep the list's pooled entries and current scroll position.
	if (AvailableTokens != MinorTokens || MinorChoices.IsEmpty())
	{
		if (ScrollBox_Subcategories) ScrollBox_Subcategories->ScrollToStart();
		for (UFF_PrepChoiceButton* Choice : MinorChoices)
		{
			if (Choice) { Choice->OnHudTabButtonClicked.Unbind(); }
		}
		HorizontalBox_Subcategories->ClearChildren();
		MinorChoices.Reset();
		MinorTokens = MoveTemp(AvailableTokens);
		for (int32 Index = 0; Index < MinorTokens.Num(); ++Index)
		{
			UFF_PrepChoiceButton* Choice = CreateWidget<UFF_PrepChoiceButton>(this, ChoiceClass);
			if (!Choice) { continue; }
			Choice->ConfigureChoice(Index, Index == 0
				? (Filter.bFilterByCategory ? LOCTEXT("AllMinorCategories", "전체 종류") : LOCTEXT("AllFacilities", "전체 시설"))
				: FFF_PrepBuildPresentationRules::ResolveMinorDisplayName(MinorTokens[Index]));
			if (UHorizontalBoxSlot* ChoiceSlot = HorizontalBox_Subcategories->AddChildToHorizontalBox(Choice))
			{
				ChoiceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				ChoiceSlot->SetVerticalAlignment(VAlign_Center);
				ChoiceSlot->SetPadding(FMargin(0, 0, 6, 0));
			}
			MinorChoices.Add(Choice);
		}
	}
	for (UFF_PrepChoiceButton* Choice : MinorChoices)
	{
		if (Choice)
		{
			Choice->OnHudTabButtonClicked.BindUObject(this, &ThisClass::HandleMinorChoiceClicked);
			if (Choice->GetChoiceId() == 0)
				Choice->ConfigureChoice(0, Filter.bFilterByCategory
					? LOCTEXT("AllMinorCategories", "전체 종류") : LOCTEXT("AllFacilities", "전체 시설"));
			Choice->SetSelected(MinorTokens.IsValidIndex(Choice->GetChoiceId()) &&
				MinorTokens[Choice->GetChoiceId()] == Filter.MinorToken);
		}
	}
}

void UFF_PrepPanelBuild::BuildCategoryFilterSurface()
{
	const int32 ExpectedChoiceCount =
		1 + FFF_PrepBuildPresentationRules::GetCategoryOrder().Num();
	if (HorizontalBox_Categories)
	{
		if (CategoryChoices.Num() == ExpectedChoiceCount) { return; }
		UClass* ChoiceClass = ResolveWidgetClass(CategoryChoiceClass, CategoryChoiceFallbackPath);
		if (!ChoiceClass || !ChoiceClass->IsChildOf(UFF_PrepChoiceButton::StaticClass())) { return; }
		const auto AddAuthoredChoice = [this, ChoiceClass](int32 Id, FText Label)
		{
			auto* Choice = CreateWidget<UFF_PrepChoiceButton>(this, ChoiceClass);
			if (!Choice) { return; }
			const bool bAll = Id == FFPrepBuildPanel::AllCategoryChoiceId;
			Choice->ConfigureTab(Id, Label, FFBuildCategoryIcons::Resolve(bAll,
				bAll ? EFFZSFacilityCategory::Booth : static_cast<EFFZSFacilityCategory>(Id)));
			Choice->OnHudTabButtonClicked.BindUObject(this, &ThisClass::HandleCategoryChoiceClicked);
			auto* Slot = HorizontalBox_Categories->AddChildToHorizontalBox(Choice);
			Slot->SetPadding(CategoryItemPadding);
			Slot->SetVerticalAlignment(VAlign_Center);
			CategoryChoices.Add(Choice);
		};
		AddAuthoredChoice(FFPrepBuildPanel::AllCategoryChoiceId, LOCTEXT("FilterAll", "전체"));
		for (const auto Category : FFF_PrepBuildPresentationRules::GetCategoryOrder())
		{
			AddAuthoredChoice(static_cast<int32>(Category), FFF_PrepBuildPresentationRules::ResolveCategoryDisplayName(Category));
		}
		RefreshCategoryChoiceSelection();
		return;
	}
	if (RuntimeCategoryScroll && RuntimeCategoryBox &&
		CategoryChoices.Num() == ExpectedChoiceCount)
	{
		return;
	}

	if (!HorizontalBox_Tools || !WidgetTree)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] HorizontalBox_Tools가 없어 전체 카테고리 칩을 만들 수 없다."), *GetName());
		return;
	}

	UClass* ResolvedChoiceClass = ResolveWidgetClass(
		CategoryChoiceClass, CategoryChoiceFallbackPath);
	if (!ResolvedChoiceClass || !ResolvedChoiceClass->IsChildOf(UFF_PrepChoiceButton::StaticClass()))
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] PrepChoiceButton 클래스를 찾지 못해 기존 카테고리 버튼을 유지한다: %s"),
			*GetName(), *CategoryChoiceFallbackPath);
		return;
	}

	RuntimeCategoryScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(), TEXT("ScrollBox_CategoryFilters_Runtime"));
	RuntimeCategoryBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("VerticalBox_CategoryFilters_Runtime"));
	if (!RuntimeCategoryScroll || !RuntimeCategoryBox)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 카테고리 스크롤 위젯을 만들지 못해 기존 버튼을 유지한다."), *GetName());
		return;
	}

	RuntimeCategoryScroll->SetOrientation(Orient_Vertical);
	RuntimeCategoryScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	RuntimeCategoryScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	RuntimeCategoryScroll->SetAnimateWheelScrolling(true);
	RuntimeCategoryScroll->SetWheelScrollMultiplier(1.0f);
	RuntimeCategoryScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
	RuntimeCategoryScroll->SetNavigationDestination(EDescendantScrollDestination::IntoView);
	UScrollBoxSlot* CategoryBoxSlot =
		Cast<UScrollBoxSlot>(RuntimeCategoryScroll->AddChild(RuntimeCategoryBox));
	if (!CategoryBoxSlot)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 카테고리 행을 ScrollBox에 연결하지 못해 기존 버튼을 유지한다."),
			*GetName());
		RuntimeCategoryScroll = nullptr;
		RuntimeCategoryBox = nullptr;
		return;
	}
	CategoryBoxSlot->SetHorizontalAlignment(HAlign_Fill);
	CategoryBoxSlot->SetVerticalAlignment(VAlign_Top);

	UHorizontalBoxSlot* ScrollSlot = Cast<UHorizontalBoxSlot>(
		HorizontalBox_Tools->InsertChildAt(0, RuntimeCategoryScroll));
	if (!ScrollSlot)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 카테고리 스크롤 행을 Tools에 삽입하지 못했다."), *GetName());
		HorizontalBox_Tools->RemoveChild(RuntimeCategoryScroll);
		RuntimeCategoryScroll = nullptr;
		RuntimeCategoryBox = nullptr;
		return;
	}
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ScrollSlot->SetVerticalAlignment(VAlign_Fill);
	ScrollSlot->SetPadding(FMargin(0.0f));
	if (Search_Facilities)
	{
		Search_Facilities->SetMinDesiredWidth(180.0f);
		if (UHorizontalBoxSlot* SearchSlot = Cast<UHorizontalBoxSlot>(Search_Facilities->Slot))
		{
			SearchSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			SearchSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	const auto AddChoice = [this, ResolvedChoiceClass](
		const int32 ChoiceId, const FText& Label)
	{
		UFF_PrepChoiceButton* Choice =
			CreateWidget<UFF_PrepChoiceButton>(this, ResolvedChoiceClass);
		if (!Choice || !RuntimeCategoryBox)
		{
			return false;
		}

		UVerticalBoxSlot* ChoiceSlot = RuntimeCategoryBox->AddChildToVerticalBox(Choice);
		if (!ChoiceSlot)
		{
			return false;
		}

		Choice->ConfigureChoice(ChoiceId, Label);
		Choice->OnHudTabButtonClicked.BindUObject(
			this, &ThisClass::HandleCategoryChoiceClicked);
		ChoiceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ChoiceSlot->SetHorizontalAlignment(HAlign_Fill);
		ChoiceSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		CategoryChoices.Add(Choice);
		return true;
	};

	bool bBuiltAllCategories = AddChoice(
		FFPrepBuildPanel::AllCategoryChoiceId, LOCTEXT("FilterAll", "전체"));
	for (const EFFZSFacilityCategory Category :
		FFF_PrepBuildPresentationRules::GetCategoryOrder())
	{
		bBuiltAllCategories &= AddChoice(
			static_cast<int32>(Category),
			FFF_PrepBuildPresentationRules::ResolveCategoryDisplayName(Category));
	}
	bBuiltAllCategories &= CategoryChoices.Num() == ExpectedChoiceCount;

	if (!bBuiltAllCategories)
	{
		for (UFF_PrepChoiceButton* Choice : CategoryChoices)
		{
			if (Choice)
			{
				Choice->OnHudTabButtonClicked.Unbind();
			}
		}
		CategoryChoices.Reset();
		HorizontalBox_Tools->RemoveChild(RuntimeCategoryScroll);
		RuntimeCategoryScroll = nullptr;
		RuntimeCategoryBox = nullptr;
		if (Search_Facilities)
		{
			if (UHorizontalBoxSlot* SearchSlot =
				Cast<UHorizontalBoxSlot>(Search_Facilities->Slot))
			{
				SearchSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 전체 카테고리 칩 생성에 실패해 런타임 행을 롤백했다."), *GetName());
		return;
	}

	// 생성된 WBP의 세 정적 버튼은 fail-safe 폴백이다. 전체+7종이 준비된 런타임에서만 접는다.
	for (UButton* LegacyButton :
		{ Button_FilterAll.Get(), Button_FilterBooth.Get(), Button_FilterPerformance.Get() })
	{
		if (LegacyButton)
		{
			LegacyButton->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	RefreshCategoryChoiceSelection();
}

void UFF_PrepPanelBuild::RefreshCategoryChoiceSelection()
{
	const int32 SelectedChoiceId = Filter.bFilterByCategory
		? static_cast<int32>(Filter.Category)
		: FFPrepBuildPanel::AllCategoryChoiceId;
	UFF_PrepChoiceButton* SelectedChoice = nullptr;
	for (UFF_PrepChoiceButton* Choice : CategoryChoices)
	{
		if (Choice)
		{
			const bool bSelected = Choice->GetChoiceId() == SelectedChoiceId;
			Choice->SetSelected(bSelected);
			SelectedChoice = bSelected ? Choice : SelectedChoice;
		}
	}
	if (RuntimeCategoryScroll && SelectedChoice)
	{
		RuntimeCategoryScroll->ScrollWidgetIntoView(
			SelectedChoice, true, EDescendantScrollDestination::IntoView, 8.0f);
	}
	RefreshLegacyCategoryButtonStyles();
}

void UFF_PrepPanelBuild::RefreshLegacyCategoryButtonStyles()
{
	if (!bHasLegacyCategoryStyles)
	{
		return;
	}

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	const auto ApplyState = [this, Palette](UButton* Button, const bool bSelected)
	{
		if (!Button)
		{
			return;
		}

		Button->SetStyle(bSelected ? LegacySelectedCategoryStyle : LegacyIdleCategoryStyle);
		if (Palette)
		{
			if (UTextBlock* Label = Cast<UTextBlock>(Button->GetContent()))
			{
				Label->SetColorAndOpacity(FSlateColor(
					bSelected ? Palette->InGame.White : Palette->InGame.Ink));
			}
		}
	};

	ApplyState(Button_FilterAll, !Filter.bFilterByCategory);
	ApplyState(Button_FilterBooth,
		Filter.bFilterByCategory && Filter.Category == EFFZSFacilityCategory::Booth);
	ApplyState(Button_FilterPerformance,
		Filter.bFilterByCategory && Filter.Category == EFFZSFacilityCategory::Performance);
}

void UFF_PrepPanelBuild::FocusSelectedCategoryChoice()
{
	const int32 SelectedChoiceId = Filter.bFilterByCategory
		? static_cast<int32>(Filter.Category)
		: FFPrepBuildPanel::AllCategoryChoiceId;
	for (UFF_PrepChoiceButton* Choice : CategoryChoices)
	{
		if (!Choice || Choice->GetChoiceId() != SelectedChoiceId)
		{
			continue;
		}

		if (UButton* RootButton = Cast<UButton>(Choice->GetWidgetFromName(TEXT("Button_Root"))))
		{
			RootButton->SetFocus();
		}
		if (RuntimeCategoryScroll)
		{
			RuntimeCategoryScroll->ScrollWidgetIntoView(
				Choice, true, EDescendantScrollDestination::IntoView, 8.0f);
		}
		return;
	}
}

void UFF_PrepPanelBuild::RefreshPricedEntries()
{
	PricedEntries = MakePricedEntries();
	RefreshMinorCategorySurface();
}

TArray<FFF_BuildCatalogEntry> UFF_PrepPanelBuild::MakePricedEntries() const
{
	TArray<FFF_BuildCatalogEntry> Result;
	if (!CatalogSubsystem)
	{
		return Result;
	}

	Result = CatalogSubsystem->GetEntriesView();
	const UFFZSWorldSubsystem* CurrentZoneSystem = ZoneSystem.Get();
	if (!CurrentZoneSystem && GetWorld())
	{
		CurrentZoneSystem = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>();
	}
	for (FFF_BuildCatalogEntry& Entry : Result)
	{
		Entry.CurrentInstanceCount = CurrentZoneSystem
			? CurrentZoneSystem->CountFacilitiesByDefinitionId(Entry.FacilityDefinitionId)
			: 0;
		Entry.bBuildLimitReached = Entry.MaxInstancesPerWorld > 0 &&
			Entry.CurrentInstanceCount >= Entry.MaxInstancesPerWorld;

		if (!EconomyReadModel)
		{
			continue;
		}

		FString FailureReason;
		if (!EconomyReadModel->TryProjectFacilityCosts(Entry, Entry.Definition.Get(), FailureReason))
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("[%s] 시설 '%s' 가격을 투영할 수 없다: %s"),
				*GetName(), *Entry.FacilityDefinitionId.ToString(), *FailureReason);
		}
	}
	return Result;
}

void UFF_PrepPanelBuild::ReconcilePresentation()
{
	if (!PresentationModel)
	{
		return;
	}

	const EFF_BuildCatalogLoadState LoadState = CatalogSubsystem
		? CatalogSubsystem->GetLoadState()
		: EFF_BuildCatalogLoadState::Failed;
	const int32 Gold = EconomyReadModel ? EconomyReadModel->GetGold() : 0;
	const bool bHasGold = EconomyReadModel && EconomyReadModel->HasAuthoritativeGold();
	const FText FailureText = CatalogSubsystem
		? CatalogSubsystem->GetLastError()
		: LOCTEXT("CatalogUnavailable", "시설 카탈로그를 초기화할 수 없습니다.");

	FName FocusId;
	bool bFocusTargetMissing = false;
	const bool bCanFocusCatalog = !BuildModeSubsystem ||
		BuildModeSubsystem->GetBuildModeState() == EFF_BuildModeState::CatalogOpen;
	if (bCanFocusCatalog && !PendingCatalogFocusId.IsNone() && LoadState == EFF_BuildCatalogLoadState::Ready)
	{
		FocusId = PendingCatalogFocusId;
		PendingCatalogFocusId = NAME_None;
		const FFF_BuildCatalogEntry* Target = PricedEntries.FindByPredicate([FocusId](const FFF_BuildCatalogEntry& Entry)
		{
			return Entry.FacilityDefinitionId == FocusId;
		});
		bFocusTargetMissing = !Target;
		if (Target)
		{
			TGuardValue<bool> FocusGuard(bApplyingCatalogFocus, true);
			bRevealSynergyOnCatalogOpen = false;
			Filter.bFilterByCategory = true;
			Filter.Category = Target->Category;
			Filter.MinorToken = Target->MinorToken;
			Filter.SearchText.Reset();
			Filter.bFavoritesOnly = false;
			if (Search_Facilities) { Search_Facilities->SetText(FText::GetEmpty()); }
			RefreshCategoryChoiceSelection();
			RefreshMinorCategorySurface();
		}
	}

	PresentationModel->SetSynergyHighlightIds(BuildModeSubsystem
		? BuildModeSubsystem->GetSynergyCatalogFacilityIds() : TArray<FName>());
	ApplyRequiredFacilitiesSnapshot();
	PresentationModel->Reconcile(
		LoadState, PricedEntries, Filter, Gold, bHasGold, FailureText);
	UFF_PrepBuildTileData* FocusTile = nullptr;
	if (!FocusId.IsNone() && !bFocusTargetMissing)
	{
		const auto& VisibleTiles = PresentationModel->GetVisibleTiles();
		const int32 Index = VisibleTiles.IndexOfByPredicate([FocusId](const UFF_PrepBuildTileData* Tile)
		{
			return Tile && Tile->GetFacilityDefinitionId() == FocusId;
		});
		if (Index != INDEX_NONE)
		{
			CatalogPageIndex = bUsePagination ? Index / FMath::Max(1, CardsPerPage) : 0;
			FocusTile = VisibleTiles[Index];
		}
	}
	ApplyPresentation(true);
	if (!FocusId.IsNone())
	{
		if (FocusTile && TileView_Facilities)
		{
			TileView_Facilities->SetSelectedItem(FocusTile);
			TileView_Facilities->RequestNavigateToItem(FocusTile);
		}
		if (BuildModeSubsystem) { BuildModeSubsystem->AcknowledgeCatalogFacilityFocus(FocusId); }
		if (bFocusTargetMissing && Text_Status)
		{
			Text_Status->SetText(LOCTEXT("CatalogFocusUnavailable", "이 시설은 현재 건축 목록에서 사용할 수 없습니다."));
		}
	}
}

void UFF_PrepPanelBuild::RefreshVisibleTiles()
{
	if (TileView_Facilities)
	{
		for (UUserWidget* Entry : TileView_Facilities->GetDisplayedEntryWidgets())
		{
			if (UFF_PrepBuildTileWidget* Tile = Cast<UFF_PrepBuildTileWidget>(Entry))
			{
				Tile->RefreshItemPresentation();
			}
		}
	}
}

void UFF_PrepPanelBuild::ApplyPresentation(const bool bReplaceListItems)
{
	if (!PresentationModel)
	{
		return;
	}

	const FFF_PrepBuildPresentationSnapshot Snapshot = PresentationModel->GetSnapshot();
	const int32 ClampedPageIndex = GetCatalogPageIndex();
	const bool bPageClamped = ClampedPageIndex != CatalogPageIndex;
	CatalogPageIndex = ClampedPageIndex;
	if (Button_PreviousPage) Button_PreviousPage->SetIsEnabled(Snapshot.bCanInteract && CanGoToPreviousPage());
	if (Button_NextPage) Button_NextPage->SetIsEnabled(Snapshot.bCanInteract && CanGoToNextPage());
	if (Text_PageIndex) Text_PageIndex->SetText(FText::Format(LOCTEXT("CatalogPage", "{0} / {1}"),
		FText::AsNumber(GetCatalogPageCount() == 0 ? 0 : CatalogPageIndex + 1),
		FText::AsNumber(GetCatalogPageCount())));
	if (Text_CategoryBreadcrumb)
	{
		const FText Major = Filter.bFilterByCategory
			? FFF_PrepBuildPresentationRules::ResolveCategoryDisplayName(Filter.Category)
			: LOCTEXT("AllFacilities", "전체 시설");
		Text_CategoryBreadcrumb->SetText(Filter.MinorToken.IsNone() ? Major : FText::Format(
			LOCTEXT("CategoryBreadcrumb", "{0}  /  {1}"), Major,
			FFF_PrepBuildPresentationRules::ResolveMinorDisplayName(Filter.MinorToken)));
	}
	if (Text_PanelTitle)
	{
		Text_PanelTitle->SetText(LOCTEXT("ApprovedBuildTitle", "시설 배치"));
	}
	if (Text_FavoritesOnly)
	{
		Text_FavoritesOnly->SetText(Text_FavoriteCount
			? (Filter.bFavoritesOnly ? LOCTEXT("FavoritesOnlyOn", "★ 즐겨찾기만") : LOCTEXT("FavoritesOnlyOff", "☆ 즐겨찾기만"))
			: FText::Format(Filter.bFavoritesOnly ? LOCTEXT("FavoritesOn", "★ 즐겨찾기만 · {0}") : LOCTEXT("FavoritesOff", "☆ 즐겨찾기만 · {0}"),
				FText::AsNumber(Filter.FavoriteIds.Num())));
	}
	if (Text_FavoriteCount) Text_FavoriteCount->SetText(FText::AsNumber(Filter.FavoriteIds.Num()));
	if (Panel_Empty) Panel_Empty->SetVisibility(Snapshot.SurfaceState == EFF_PrepBuildSurfaceState::Empty
		? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Snapshot.SurfaceState == EFF_PrepBuildSurfaceState::Empty)
	{
		const bool bNoFavorites = Filter.bFavoritesOnly && Filter.FavoriteIds.IsEmpty();
		if (Text_EmptyTitle) Text_EmptyTitle->SetText(bNoFavorites
			? LOCTEXT("NoFavoritesTitle", "즐겨찾기한 시설이 없어요")
			: (Filter.bFavoritesOnly ? LOCTEXT("NoMatchingFavoritesTitle", "조건에 맞는 즐겨찾기가 없어요")
				: LOCTEXT("NoMatchingFacilitiesTitle", "조건에 맞는 시설이 없어요")));
		if (Text_EmptyHint) Text_EmptyHint->SetText(bNoFavorites
			? LOCTEXT("NoFavoritesHint", "자주 쓰는 시설의 ☆을 눌러 모아보세요.")
			: LOCTEXT("NoMatchingFacilitiesHint", "분류나 검색어를 바꾸어 보세요."));
	}
	if (Button_FavoritesOnly && bHasFavoritesFilterStyle)
	{
		FButtonStyle Style = FavoritesFilterStyle;
		if (Filter.bFavoritesOnly)
		{
			Style.Normal = Style.Pressed;
			Style.Hovered = Style.Pressed;
		}
		Button_FavoritesOnly->SetStyle(Style);
	}
	if (Button_ResetFilters)
	{
		Button_ResetFilters->SetVisibility(Snapshot.SurfaceState == EFF_PrepBuildSurfaceState::Empty
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (UTextBlock* Label = Cast<UTextBlock>(Button_ResetFilters->GetContent()))
		{
			Label->SetText(Filter.bFavoritesOnly && Filter.FavoriteIds.IsEmpty()
				? LOCTEXT("ShowAllFacilities", "전체 시설 보기") : LOCTEXT("ClearCategorySearch", "분류·검색 초기화"));
		}
	}
	if (TileView_Facilities)
	{
		if (bReplaceListItems || bPageClamped)
		{
			const TArray<UObject*> PageItems = GetCatalogPageItems();
			TileView_Facilities->SetListItems(PageItems);
			if (PageItems.IsEmpty())
			{
				// A collapsed list cannot run its next Slate generation pass. Release rows
				// only at this final empty-result boundary, before hiding the list.
				TileView_Facilities->RegenerateAllEntries();
			}
			else
			{
				RefreshVisibleTiles();
			}
		}
		TileView_Facilities->SetVisibility(
			Snapshot.bCanInteract ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (bPageClamped) TileView_Facilities->ScrollToTop();
	}

	if (Text_ItemCount)
	{
		Text_ItemCount->SetText(FText::Format(
			Filter.bFavoritesOnly ? LOCTEXT("FavoriteItemCount", "즐겨찾기 {0}종") : LOCTEXT("ItemCount", "시설 {0}종"),
			FText::AsNumber(Snapshot.VisibleItemCount)));
	}
	if (Text_Gold)
	{
		Text_Gold->SetText(EconomyReadModel && EconomyReadModel->HasAuthoritativeGold()
			? FText::Format(LOCTEXT("Gold", "보유 {0} Gold"), FText::AsNumber(EconomyReadModel->GetGold()))
			: LOCTEXT("GoldLoading", "잔액 확인 중"));
	}
	if (Text_Status)
	{
		FText Status = Snapshot.StatusText;
		if (Snapshot.SurfaceState == EFF_PrepBuildSurfaceState::Empty && Filter.bFavoritesOnly)
		{
			Status = Filter.FavoriteIds.IsEmpty()
				? LOCTEXT("NoFavorites", "즐겨찾기한 시설이 없어요. 시설 카드의 ☆을 눌러 모아보세요.")
				: LOCTEXT("NoMatchingFavorites", "조건에 맞는 즐겨찾기가 없어요. 분류나 검색어를 바꾸어 보세요.");
		}
		if (Snapshot.SurfaceState == EFF_PrepBuildSurfaceState::Empty && Panel_Empty)
			Status = LOCTEXT("EmptyCatalogFooter", "☆ 즐겨찾기  ·  B 시설 확인");
		if (Snapshot.SurfaceState == EFF_PrepBuildSurfaceState::Ready)
		{
			const EFF_BuildCatalogThumbnailLoadState ThumbnailState = CatalogSubsystem
				? CatalogSubsystem->GetThumbnailLoadState()
				: EFF_BuildCatalogThumbnailLoadState::Failed;
			Status = (ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Loading ||
				ThumbnailState == EFF_BuildCatalogThumbnailLoadState::WaitingForCatalog)
				? LOCTEXT("ThumbnailsLoading", "시설 이미지를 불러오는 중입니다. 목록은 먼저 선택할 수 있습니다.")
				: LOCTEXT("ApprovedCatalogReady", "카드 선택 → 배치 시작  ·  ☆ 즐겨찾기  ·  B 시설 확인");
		}
		const FText SynergySource = BuildModeSubsystem
			? BuildModeSubsystem->GetSynergyCatalogSourceName() : FText::GetEmpty();
		if (!SynergySource.IsEmpty() && (Snapshot.SurfaceState == EFF_PrepBuildSurfaceState::Ready ||
			Snapshot.SurfaceState == EFF_PrepBuildSurfaceState::Empty))
		{
			int32 VisibleMatches = 0;
			int32 TotalMatches = 0;
			for (const FName Id : BuildModeSubsystem->GetSynergyCatalogFacilityIds())
			{
				if (PresentationModel->FindTile(Id)) { ++TotalMatches; }
			}
			for (const UFF_PrepBuildTileData* Tile : PresentationModel->GetVisibleTiles())
			{
				VisibleMatches += Tile && Tile->IsSynergyHighlighted() ? 1 : 0;
			}
			Status = TotalMatches == 0
				? FText::Format(LOCTEXT("NoSynergyCatalogPartners", "{0} · 목록에 조합 시설이 없습니다."), SynergySource)
				: FText::Format(LOCTEXT("SynergyCatalogPartners", "{0} 조합 {1}/{2}종 강조 · 배치 시 거리 조건 확인"),
					SynergySource, FText::AsNumber(VisibleMatches), FText::AsNumber(TotalMatches));
		}
		Text_Status->SetText(Status);
	}
}

void UFF_PrepPanelBuild::ApplyBuildModeState(const EFF_BuildModeState State)
{
	const bool bShowCatalog = State == EFF_BuildModeState::CatalogOpen;
	const bool bShowCompact = State == EFF_BuildModeState::BuildMode;
	const bool bShowSeparateTools = State == EFF_BuildModeState::Placing || State == EFF_BuildModeState::Moving;

	if (Panel_Catalog)
	{
		Panel_Catalog->SetVisibility(bShowCatalog ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Panel_PlacementCompact)
	{
		Panel_PlacementCompact->SetVisibility(
			bShowCompact ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Catalog header and the separate placement-tools WBP are mutually exclusive.
	if (Box_Header)
	{
		Box_Header->SetVisibility(
			bShowCatalog ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	// The WBP owns padding; C++ changes only which surface is visible.
	if (Border_PanelRoot)
	{
		// Placement tools have their own WBP and viewport host. Do not leave an empty catalog rectangle behind.
		Border_PanelRoot->SetVisibility(bShowSeparateTools ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (Text_PlacementGuide)
	{
		FText Guide;
		switch (State)
		{
		case EFF_BuildModeState::Placing: 
			// 🔴 **한 줄로 쓴다.** 네 줄로 쌓으면 그만큼 지면을 가리는데, 배치 중에는
			//   지면이 보여야 시설을 놓을 수 있다 (2026-09-03 백종태 요청).
			Guide = BuildModeSubsystem && BuildModeSubsystem->IsDragPlacementEnabled()
				? LOCTEXT("DragPlacingGuide", "[좌클릭 누른 채 이동] 연속 설치 · 놓으면 정지 · [Q/E] 회전 · [우클릭] 목록")
				: LOCTEXT("PlacingGuide", "[좌클릭/Enter] 한 개씩 설치 · [Q/E] 회전 · [Shift+Q/E] 부드럽게 회전 · [우클릭] 목록");
			break;
		case EFF_BuildModeState::Moving:
			Guide = LOCTEXT("MovingGuide", "이동 중 · 좌클릭 확정 · 우클릭 취소");
			break;
		case EFF_BuildModeState::BuildMode:
			Guide = LOCTEXT("BuildModeGuide", "건축 모드 · 설치된 시설을 선택하거나 목록으로 돌아가세요.");
			break;
		default:
			Guide = FText::GetEmpty();
			break;
		}
		Text_PlacementGuide->SetText(Guide);
		Text_PlacementGuide->SetToolTipText(bShowSeparateTools
			? UFF_BuildModeGuide::MakePlacementHelpText(State == EFF_BuildModeState::Moving,
				BuildModeSubsystem && BuildModeSubsystem->IsDragPlacementEnabled())
			: UFF_BuildModeGuide::MakeBuildOverviewHelpText());
	}
	// 🔴 **패널 높이도 같이 줄인다.**
	//
	// 목록을 접고 안내 네 줄만 남겨도 호스트 슬롯은 카탈로그 높이(320)를 그대로 쥐고 있어서,
	// 글자 몇 줄 아래로 **빈 흰 판이 크게 남았다** (2026-09-03 백종태 지적).
	// 배치 중에는 지면이 보여야 하는데 그 빈 판이 화면을 가린다.
	//
	// 크기는 바가 소유하므로 여기서 슬롯을 직접 만지지 않고 요청한다 — 직접 만지면
	// 탭을 닫았다 열 때 `ApplyPanelLayout` 이 표 값으로 덮어써서 되돌아간다.
	if (UFF_PrepHudBar* Bar = GetOwningBar())
	{
		FFF_PrepPanelLayout Layout = Bar->GetOpenPanelLayout();

		// 배치 중에는 얇은 띠로, 카탈로그일 때는 원래 크기로.
		Layout.Size.Y = bShowCatalog ? CatalogHeight : CompactHeight;

		// ★ 자리도 같이 옮긴다. 배치 중에는 바 **위**에 얹어 탭 줄과 겹치지 않게 한다 —
		//   겹치면 흰 띠 안에 초록 칩이 들어가고, 띠를 키울수록 칩이 아래로 밀린다.
		Layout.Offset.Y = bShowCatalog ? CatalogOffsetY : CompactOffsetY;

		Bar->SetOpenPanelLayout(Layout);
	}

	if (bShowCatalog)
	{
		FocusSelectedCategoryChoice();
	}
}

#undef LOCTEXT_NAMESPACE
