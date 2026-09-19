#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"

#include "Building/Codex/FF_FacilityCodexSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/StackBox.h"
#include "Components/StackBoxSlot.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexPresentation.h"

#define LOCTEXT_NAMESPACE "FFFacilityCodexScreen"

void UFF_FacilityCodexScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	if (Button_Close) Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClose);
	if (Button_All) Button_All->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAll);
	if (Button_Discovered) Button_Discovered->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDiscovered);
	if (Button_Undiscovered) Button_Undiscovered->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUndiscovered);
	if (Button_ClearFacility) Button_ClearFacility->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClearFacility);
	if (Search_Entries) Search_Entries->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleSearchChanged);
	if (TileView_Entries) TileView_Entries->OnItemSelectionChanged().AddUObject(this, &ThisClass::HandleSelectionChanged);
}

void UFF_FacilityCodexScreen::NativeConstruct()
{
	Super::NativeConstruct();
	if (!bPreviewData)
	{
		Codex = GetWorld() ? GetWorld()->GetSubsystem<UFF_FacilityCodexSubsystem>() : nullptr;
		if (Codex)
		{
			Codex->Refresh();
			Codex->OnCodexChanged.AddUniqueDynamic(this, &ThisClass::RefreshFromSubsystem);
		}
		RefreshFromSubsystem();
	}
}

void UFF_FacilityCodexScreen::NativeDestruct()
{
	if (Codex) Codex->OnCodexChanged.RemoveDynamic(this, &ThisClass::RefreshFromSubsystem);
	Codex = nullptr;
	// Also release presenter-owned input/build leases when another screen removes this widget.
	OnCloseRequested.Broadcast();
	Super::NativeDestruct();
}

void UFF_FacilityCodexScreen::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	// Layout-only resize handling. Gameplay data still refreshes exclusively through events.
	UpdateResponsiveLayout();
}

void UFF_FacilityCodexScreen::UpdateResponsiveLayout()
{
	if (!Stack_Main || !Size_Detail || Stack_Main->GetChildrenCount() != 2) return;
	UStackBoxSlot* GridSlot = Cast<UStackBoxSlot>(Stack_Main->GetChildAt(0)->Slot);
	if (!GridSlot) return;
	if (!bCapturedResponsiveLayout)
	{
		bAuthoredDetailWidth = Size_Detail->IsWidthOverride();
		bAuthoredDetailHeight = Size_Detail->IsHeightOverride();
		AuthoredDetailWidth = Size_Detail->GetWidthOverride();
		AuthoredDetailHeight = Size_Detail->GetHeightOverride();
		AuthoredGridPadding = GridSlot->GetPadding();
		bCapturedResponsiveLayout = true;
	}
	const float Width = Stack_Main->GetCachedGeometry().GetLocalSize().X;
	if (Width <= 1.f) return;
	const float DetailWidth = bAuthoredDetailWidth ? AuthoredDetailWidth : Size_Detail->GetDesiredSize().X;
	const float Spacing = TileView_Entries ? TileView_Entries->GetHorizontalEntrySpacing() : 0.f;
	const bool bCompact = bEnableResponsiveDetail && Width <
		6.f * (MinimumCardWidth + Spacing) + DetailWidth + AuthoredGridPadding.Left + AuthoredGridPadding.Right + 20.f;
	if (bCompact == bCompactDetail) return;
	bCompactDetail = bCompact;
	Stack_Main->SetOrientation(bCompact ? Orient_Vertical : Orient_Horizontal);
	if (bCompact)
	{
		Size_Detail->ClearWidthOverride();
		Size_Detail->SetHeightOverride(CompactDetailHeight);
		GridSlot->SetPadding(FMargin(0, 0, 0, CompactDetailGap));
	}
	else
	{
		if (bAuthoredDetailWidth) Size_Detail->SetWidthOverride(AuthoredDetailWidth);
		else Size_Detail->ClearWidthOverride();
		if (bAuthoredDetailHeight) Size_Detail->SetHeightOverride(AuthoredDetailHeight);
		else Size_Detail->ClearHeightOverride();
		GridSlot->SetPadding(AuthoredGridPadding);
	}
}

FReply UFF_FacilityCodexScreen::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		HandleClose();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

FReply UFF_FacilityCodexScreen::NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&)
{
	return FReply::Handled();
}

void UFF_FacilityCodexScreen::SetFacilityFilter(FName FacilityId)
{
	Filter.FacilityDefinitionId = FacilityId;
	RefreshFilter();
}

void UFF_FacilityCodexScreen::SetDiscoveryFilter(EFF_FacilityCodexFilterState State)
{
	Filter.State = State;
	RefreshFilter();
}

void UFF_FacilityCodexScreen::SetSearchText(const FText& SearchText)
{
	Filter.SearchText = SearchText.ToString();
	if (Search_Entries && !Search_Entries->GetText().EqualTo(SearchText)) Search_Entries->SetText(SearchText);
	// UEditableTextBox::SetText does not broadcast user edit events.
	RefreshFilter();
}

void UFF_FacilityCodexScreen::RefreshFromSubsystem()
{
	if (bPreviewData) return;
	if (Codex) ApplyEntries(Codex->GetEntriesView());
	else ApplyEntries({});
}

#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
void UFF_FacilityCodexScreen::SetPreviewEntries(const TArray<FFF_FacilityCodexEntry>& Entries)
{
	bPreviewData = true;
	if (Codex) Codex->OnCodexChanged.RemoveDynamic(this, &ThisClass::RefreshFromSubsystem);
	Codex = nullptr;
	ApplyEntries(Entries);
}
#endif

void UFF_FacilityCodexScreen::ApplyEntries(const TArray<FFF_FacilityCodexEntry>& Entries)
{
	TSet<FName> CurrentIds;
	AllItems.Reset(Entries.Num());
	for (const FFF_FacilityCodexEntry& Entry : Entries)
	{
		if (Entry.RuleId.IsNone() || CurrentIds.Contains(Entry.RuleId)) continue;
		CurrentIds.Add(Entry.RuleId);
		TObjectPtr<UFF_FacilityCodexItem>& Item = ItemsByRule.FindOrAdd(Entry.RuleId);
		if (!Item) Item = NewObject<UFF_FacilityCodexItem>(this);
		Item->Entry = Entry;
		AllItems.Add(Item);
	}
	for (auto It = ItemsByRule.CreateIterator(); It; ++It)
	{
		if (!CurrentIds.Contains(It.Key())) It.RemoveCurrent();
	}
	Summary = FFF_FacilityCodexModel::Summarize(Entries);
	RefreshFilter();
}

void UFF_FacilityCodexScreen::RefreshFilter()
{
	if (bApplyingFilter) return;
	TGuardValue<bool> Guard(bApplyingFilter, true);
	TArray<TObjectPtr<UFF_FacilityCodexItem>> NewVisible;
	for (UFF_FacilityCodexItem* Item : AllItems)
	{
		if (Item && FFF_FacilityCodexModel::MatchesFilter(Item->Entry, Filter)) NewVisible.Add(Item);
	}
	const bool bMembershipChanged = NewVisible != VisibleItems;
	VisibleItems = MoveTemp(NewVisible);
	UFF_FacilityCodexItem* Selected = ItemsByRule.FindRef(SelectedRuleId);
	if (!VisibleItems.Contains(Selected)) Selected = VisibleItems.IsEmpty() ? nullptr : VisibleItems[0].Get();
	SelectedRuleId = Selected ? Selected->Entry.RuleId : NAME_None;
	if (TileView_Entries)
	{
		if (bMembershipChanged)
		{
			TArray<UObject*> Objects;
			Objects.Reserve(VisibleItems.Num());
			for (UFF_FacilityCodexItem* Item : VisibleItems) Objects.Add(Item);
			TileView_Entries->SetListItems(Objects);
			TileView_Entries->ScrollToTop();
		}
		TileView_Entries->SetSelectedItem(Selected);
		for (UUserWidget* Widget : TileView_Entries->GetDisplayedEntryWidgets())
		{
			if (auto* Card = Cast<UFF_FacilityCodexCard>(Widget)) Card->RefreshPresentation();
		}
	}
	if (Button_All) Button_All->SetIsEnabled(Filter.State != EFF_FacilityCodexFilterState::All);
	if (Button_Discovered) Button_Discovered->SetIsEnabled(Filter.State != EFF_FacilityCodexFilterState::Discovered);
	if (Button_Undiscovered) Button_Undiscovered->SetIsEnabled(Filter.State != EFF_FacilityCodexFilterState::Undiscovered);
	if (Button_ClearFacility) Button_ClearFacility->SetVisibility(Filter.FacilityDefinitionId.IsNone()
		? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (Text_Summary) Text_Summary->SetText(FText::Format(LOCTEXT("Summary", "발견 {0} / {1}    ·    효과 적용 중 {2}"),
		FText::AsNumber(Summary.DiscoveredCount), FText::AsNumber(Summary.TotalCount), FText::AsNumber(Summary.EffectAppliedCount)));
	FText FacilityName = LOCTEXT("AllFacilities", "모든 시설");
	if (!Filter.FacilityDefinitionId.IsNone())
	{
		FacilityName = LOCTEXT("SelectedFacility", "선택한 시설");
		for (const UFF_FacilityCodexItem* Item : AllItems)
		{
			if (Item->Entry.TargetFacilityId == Filter.FacilityDefinitionId) { FacilityName = Item->Entry.TargetName; break; }
			if (Item->Entry.PartnerFacilityId == Filter.FacilityDefinitionId) { FacilityName = Item->Entry.PartnerName; break; }
		}
	}
	if (Text_Filter) Text_Filter->SetText(FText::Format(LOCTEXT("Filter", "{0} · {1}개 조합"), FacilityName, FText::AsNumber(VisibleItems.Num())));
	if (Text_Empty)
	{
		Text_Empty->SetVisibility(VisibleItems.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Text_Empty->SetText(AllItems.IsEmpty() ? LOCTEXT("NoRules", "등록된 시설 조합이 없습니다.")
			: LOCTEXT("NoMatches", "조건에 맞는 조합이 없습니다.\n검색어나 필터를 바꿔 보세요."));
	}
	RefreshDetail();
}

void UFF_FacilityCodexScreen::HandleSelectionChanged(UObject* SelectedItem)
{
	if (bApplyingFilter) return;
	const auto* Item = Cast<UFF_FacilityCodexItem>(SelectedItem);
	SelectedRuleId = Item ? Item->Entry.RuleId : NAME_None;
	RefreshDetail();
}

void UFF_FacilityCodexScreen::RefreshDetail()
{
	using namespace FFFacilityCodexPresentation;
	const UFF_FacilityCodexItem* Item = ItemsByRule.FindRef(SelectedRuleId);
	if (Panel_Detail) Panel_Detail->SetVisibility(Item ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	if (!Item)
	{
		Thumbnail(Image_Target, Text_TargetFallback, {}, ShownTarget);
		Thumbnail(Image_Partner, Text_PartnerFallback, {}, ShownPartner);
		return;
	}
	const FFF_FacilityCodexEntry& Entry = Item->Entry;
	if (Text_DetailTitle) Text_DetailTitle->SetText(Entry.DisplayName);
	if (Text_TargetName) Text_TargetName->SetText(Entry.TargetName);
	if (Text_PartnerName) Text_PartnerName->SetText(Entry.PartnerName);
	if (Text_Bonus) Text_Bonus->SetText(FText::Format(LOCTEXT("DetailBonus", "{0}\n수익 +{1}%"), Entry.TargetName, Number(Entry.ConfiguredBonusPercent)));
	if (Text_Distance) Text_Distance->SetText(FText::Format(LOCTEXT("Distance", "시설 중심 사이 {0}m 이내에 배치"), Number(Entry.MaxDistanceMeters)));
	if (Text_Discovery) Text_Discovery->SetText(Entry.bDiscovered ? LOCTEXT("Found", "발견 완료 · 두 시설의 설치가 기록되었어요") : LOCTEXT("NotFound", "미발견 · 조합에 필요한 두 시설을 설치해 보세요"));
	FText Condition;
	if (Entry.bConditionMet)
	{
		Condition = FText::Format(LOCTEXT("Met", "현재 조건 충족: {0}개 시설"), FText::AsNumber(Entry.ConditionMetCount));
	}
	else if (!Entry.bTargetInstalled || !Entry.bPartnerInstalled)
	{
		Condition = LOCTEXT("Install", "현재 조건 미충족: 필요한 시설을 설치해 주세요");
	}
	else if (Entry.ClosestDistanceMeters >= 0.f)
	{
		Condition = FText::Format(LOCTEXT("Far", "현재 가장 가까운 거리 {0}m · 더 가까이 배치해 주세요"), Number(Entry.ClosestDistanceMeters));
	}
	else if (Entry.bHasInstalledPair) Condition = LOCTEXT("Placement", "두 시설 설치 완료 · 현재 배치 상태를 확인해 주세요");
	else Condition = LOCTEXT("Pair", "현재 조건 미충족: 서로 다른 두 시설이 필요해요");
	if (Text_Condition) Text_Condition->SetText(Condition);
	if (Text_Applied) Text_Applied->SetText(Entry.bEffectApplied
		? FText::Format(LOCTEXT("EffectApplied", "현재 {0}개 시설에 적용 중 · 최대 +{1}%\n같은 그룹의 더 높은 효과와 수익 상한에 따라 실제 보너스가 달라집니다."),
			FText::AsNumber(Entry.EffectAppliedCount), Number(Entry.AppliedBonusPercent))
		: Entry.bConditionMet ? LOCTEXT("EffectCapped", "배치 조건은 충족했어요. 더 높은 같은 그룹 효과 또는 수익 상한으로 현재 추가 적용은 없어요.")
		: LOCTEXT("NoEffect", "현재 적용 효과 없음 · 배치 조건을 충족하면 자동으로 적용됩니다."));
	Thumbnail(Image_Target, Text_TargetFallback, Entry.TargetThumbnail, ShownTarget);
	Thumbnail(Image_Partner, Text_PartnerFallback, Entry.PartnerThumbnail, ShownPartner);
}

void UFF_FacilityCodexScreen::HandleClose() { OnCloseRequested.Broadcast(); }
void UFF_FacilityCodexScreen::HandleAll() { SetDiscoveryFilter(EFF_FacilityCodexFilterState::All); }
void UFF_FacilityCodexScreen::HandleDiscovered() { SetDiscoveryFilter(EFF_FacilityCodexFilterState::Discovered); }
void UFF_FacilityCodexScreen::HandleUndiscovered() { SetDiscoveryFilter(EFF_FacilityCodexFilterState::Undiscovered); }
void UFF_FacilityCodexScreen::HandleClearFacility() { SetFacilityFilter(NAME_None); }
void UFF_FacilityCodexScreen::HandleSearchChanged(const FText& Text) { Filter.SearchText = Text.ToString(); RefreshFilter(); }

#undef LOCTEXT_NAMESPACE
