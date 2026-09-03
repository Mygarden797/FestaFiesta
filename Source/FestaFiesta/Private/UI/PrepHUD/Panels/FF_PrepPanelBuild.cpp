// LightTeal PrepHUD build panel runtime presentation.

#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"

#include "Building/FF_BuildCatalogEconomyReadModel.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
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

	if (TileView_Facilities)
	{
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
	for (UFF_PrepChoiceButton* Choice : CategoryChoices)
	{
		if (Choice)
		{
			Choice->OnHudTabButtonClicked.BindUObject(
				this, &ThisClass::HandleCategoryChoiceClicked);
		}
	}
	RefreshCategoryChoiceSelection();
}

void UFF_PrepPanelBuild::NativeDestruct()
{
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

	Super::NativeDestruct();
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
	Filter = FFF_PrepBuildFilter();
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
}

void UFF_PrepPanelBuild::HandleCatalogChanged()
{
	RefreshPricedEntries();
	ReconcilePresentation();
}

void UFF_PrepPanelBuild::HandleCatalogThumbnailsChanged()
{
	// Item identity is unchanged; recycle only the visible entry widgets so scroll position is preserved.
	if (TileView_Facilities)
	{
		TileView_Facilities->RegenerateAllEntries();
	}
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
	if (TileView_Facilities)
	{
		TileView_Facilities->RegenerateAllEntries();
	}
	ApplyPresentation(false);
}

void UFF_PrepPanelBuild::HandleBuildModeStateChanged(
	const EFF_BuildModeState PreviousState,
	const EFF_BuildModeState NewState)
{
	UE_LOG(LogFestaFiestaUI, Verbose,
		TEXT("[%s] 배치 표현 상태 전환: %s -> %s"),
		*GetName(), *UEnum::GetValueAsString(PreviousState), *UEnum::GetValueAsString(NewState));

	if (!bPanelOpen || bClosingPanel)
	{
		return;
	}

	if (NewState == EFF_BuildModeState::CatalogOpen)
	{
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
	Filter.SearchText = NewText.ToString();
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
	Filter.bFilterByCategory = bEnabled;
	Filter.Category = Category;
	RefreshCategoryChoiceSelection();
	ReconcilePresentation();
}

void UFF_PrepPanelBuild::BuildCategoryFilterSurface()
{
	const int32 ExpectedChoiceCount =
		1 + FFF_PrepBuildPresentationRules::GetCategoryOrder().Num();
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
	RuntimeCategoryBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("HorizontalBox_CategoryFilters_Runtime"));
	if (!RuntimeCategoryScroll || !RuntimeCategoryBox)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 카테고리 스크롤 위젯을 만들지 못해 기존 버튼을 유지한다."), *GetName());
		return;
	}

	RuntimeCategoryScroll->SetOrientation(Orient_Horizontal);
	RuntimeCategoryScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	RuntimeCategoryScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
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
	CategoryBoxSlot->SetHorizontalAlignment(HAlign_Left);
	CategoryBoxSlot->SetVerticalAlignment(VAlign_Center);

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
	ScrollSlot->SetVerticalAlignment(VAlign_Center);
	ScrollSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
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

		UHorizontalBoxSlot* ChoiceSlot = RuntimeCategoryBox->AddChildToHorizontalBox(Choice);
		if (!ChoiceSlot)
		{
			return false;
		}

		Choice->ConfigureChoice(ChoiceId, Label);
		Choice->OnHudTabButtonClicked.BindUObject(
			this, &ThisClass::HandleCategoryChoiceClicked);
		ChoiceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		ChoiceSlot->SetVerticalAlignment(VAlign_Center);
		ChoiceSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
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

	PresentationModel->Reconcile(
		LoadState, PricedEntries, Filter, Gold, bHasGold, FailureText);
	ApplyPresentation(true);
}

void UFF_PrepPanelBuild::ApplyPresentation(const bool bReplaceListItems)
{
	if (!PresentationModel)
	{
		return;
	}

	const FFF_PrepBuildPresentationSnapshot Snapshot = PresentationModel->GetSnapshot();
	if (TileView_Facilities)
	{
		if (bReplaceListItems)
		{
			TileView_Facilities->SetListItems(PresentationModel->GetListItems());
		}
		TileView_Facilities->SetVisibility(
			Snapshot.bCanInteract ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Text_ItemCount)
	{
		Text_ItemCount->SetText(FText::Format(
			LOCTEXT("ItemCount", "시설 {0} / {1}"),
			FText::AsNumber(Snapshot.VisibleItemCount),
			FText::AsNumber(Snapshot.TotalItemCount)));
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
		if (Snapshot.SurfaceState == EFF_PrepBuildSurfaceState::Ready)
		{
			const EFF_BuildCatalogThumbnailLoadState ThumbnailState = CatalogSubsystem
				? CatalogSubsystem->GetThumbnailLoadState()
				: EFF_BuildCatalogThumbnailLoadState::Failed;
			Status = (ThumbnailState == EFF_BuildCatalogThumbnailLoadState::Loading ||
				ThumbnailState == EFF_BuildCatalogThumbnailLoadState::WaitingForCatalog)
				? LOCTEXT("ThumbnailsLoading", "시설 이미지를 불러오는 중입니다. 목록은 먼저 선택할 수 있습니다.")
				: LOCTEXT("CatalogReady", "[B]키를 누르면 건축 모드로 전환됩니다 ");
		}
		Text_Status->SetText(Status);
	}
}

void UFF_PrepPanelBuild::ApplyBuildModeState(const EFF_BuildModeState State)
{
	const bool bShowCatalog = State == EFF_BuildModeState::CatalogOpen;
	const bool bShowCompact = State == EFF_BuildModeState::BuildMode ||
		State == EFF_BuildModeState::Placing || State == EFF_BuildModeState::Moving;

	if (Panel_Catalog)
	{
		Panel_Catalog->SetVisibility(bShowCatalog ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Panel_PlacementCompact)
	{
		Panel_PlacementCompact->SetVisibility(
			bShowCompact ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 제목·보유 금화는 배치 중에 볼 일이 없다 — 목록에서 이미 보고 들어왔다.
	if (Box_Header)
	{
		Box_Header->SetVisibility(
			bShowCompact ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	// 상하 여백을 0 으로. 남겨 두면 흰 판이 탭 줄 위로 튀어나와 초록 칩이 아래에 붙어 보인다.
	if (Border_PanelRoot)
	{
		Border_PanelRoot->SetPadding(bShowCompact
			? FMargin(18.f, 0.f)
			: FMargin(18.f, 14.f));
	}
	if (Text_PlacementGuide)
	{
		FText Guide;
		switch (State)
		{
		case EFF_BuildModeState::Placing: 
			// 🔴 **한 줄로 쓴다.** 네 줄로 쌓으면 그만큼 지면을 가리는데, 배치 중에는
			//   지면이 보여야 시설을 놓을 수 있다 (2026-09-03 백종태 요청).
			Guide = LOCTEXT("PlacingGuide",
				"[좌클릭] 설치·철거·이동 · [Q/E] 45° 회전 · [Shift+Q/E] 정밀 회전 · [우클릭] 목록");
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
		Layout.Size.Y = bShowCompact ? CompactHeight : CatalogHeight;

		// ★ 자리도 같이 옮긴다. 배치 중에는 바 **위**에 얹어 탭 줄과 겹치지 않게 한다 —
		//   겹치면 흰 띠 안에 초록 칩이 들어가고, 띠를 키울수록 칩이 아래로 밀린다.
		Layout.Offset.Y = bShowCompact ? CompactOffsetY : 0.0;

		Bar->SetOpenPanelLayout(Layout);
	}

	if (bShowCatalog)
	{
		FocusSelectedCategoryChoice();
	}
}

#undef LOCTEXT_NAMESPACE
