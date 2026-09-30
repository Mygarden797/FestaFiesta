// 역할: 시설 조합 도감의 목록·상세 화면 전환과 수집 보상 표시를 담당한다.

#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"

#include "Building/Codex/FF_FacilityCodexSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexPreview.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"

#define LOCTEXT_NAMESPACE "FFCodexCollection"

namespace
{
// 보상을 받을 수 있는 발견 수와 진행 막대가 보상 카드에 도달하는 시점을 맞춘다.
float MilestoneTrackPercent(const TArray<FFF_FacilityCodexCollectionMilestone>& Milestones,
    const int32 DiscoveredCount, const int32 TotalCount)
{
    if (Milestones.IsEmpty() || TotalCount <= 0) return 0.f;
    const int32 Count = FMath::Clamp(DiscoveredCount, 0, TotalCount);
    int32 PreviousCount = 0;
    float PreviousPosition = 0.f;
    for (int32 Index = 0; Index < Milestones.Num(); ++Index)
    {
        const int32 Threshold = Milestones[Index].RequiredDiscoveries;
        const float Position = (float(Index) + .5f) / float(Milestones.Num());
        if (Count <= Threshold)
        {
            const float Fraction = Threshold > PreviousCount
                ? float(Count - PreviousCount) / float(Threshold - PreviousCount) : 1.f;
            return FMath::Lerp(PreviousPosition, Position, FMath::Clamp(Fraction, 0.f, 1.f));
        }
        PreviousCount = Threshold;
        PreviousPosition = Position;
    }
    if (TotalCount <= PreviousCount) return PreviousPosition;
    return FMath::Lerp(PreviousPosition, 1.f,
        FMath::Clamp(float(Count - PreviousCount) / float(TotalCount - PreviousCount), 0.f, 1.f));
}
}

// 화면 크기에 맞춰 목록·분류·상세 영역을 배치하고 실제로 달라진 위치만 반영한다.
bool UFF_FacilityCodexScreen::UpdateCollectionLayout(const FVector2D& Viewport)
{
	if (!Column_CollectionCategories) return false;
	if (bUseAuthoredCollectionLayout)
	{
		// Saved Fill slots stretch the reward track. The saved six-column tile
		// view measures its own pitch in Slate, including in Designer without Tick.
		return true;
	}
	if (Size_Dialog)
	{
		if (!FMath::IsNearlyEqual(Size_Dialog->GetWidthOverride(), Viewport.X))
			Size_Dialog->SetWidthOverride(Viewport.X);
		if (!FMath::IsNearlyEqual(Size_Dialog->GetHeightOverride(), Viewport.Y))
			Size_Dialog->SetHeightOverride(Viewport.Y);
	}
	const auto Place = [this](const TCHAR* Name, const FVector2D& Position, const FVector2D& Size)
	{
		UWidget* Widget = GetWidgetFromName(Name);
		if (UCanvasPanelSlot* CanvasSlot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
		{
			if (!CanvasSlot->GetPosition().Equals(Position, .5)) CanvasSlot->SetPosition(Position);
			if (!CanvasSlot->GetSize().Equals(Size, .5)) CanvasSlot->SetSize(Size);
		}
	};
	const float SidePadding = FMath::Clamp(Viewport.X * .025f, 16.f, 48.f);
	const float NavigationWidth = FMath::Clamp(Viewport.X * .15f, 154.f, 220.f);
	// A dedicated filter row never competes with the title/close button. This also
	// leaves enough width for all five Korean captions at the 720p logical size.
	const float TopPadding = 144.f;
	const float FooterHeight = 164.f;
	const float GridX = SidePadding + NavigationWidth + 18.f;
	const float GridWidth = Viewport.X - GridX - SidePadding;
	Place(TEXT("Size_Refined_Button_Close"), {SidePadding, 24}, {330, 48});
	Place(TEXT("Button_ExitCodex"), {Viewport.X - SidePadding - 48.f, 24.f}, {48, 48});
	Place(TEXT("Horizontal_Toolbar"), {GridX, 84}, {GridWidth, 44});
	Place(TEXT("Size_CollectionSearch"), {SidePadding, 84}, {NavigationWidth, 44});
	Place(TEXT("Scroll_CollectionCategories"), {SidePadding, TopPadding}, {NavigationWidth, FMath::Max(80.f, Viewport.Y - TopPadding - 24.f)});
	Place(TEXT("Size_Browse"), {GridX, TopPadding},
		{GridWidth, FMath::Max(120.f, Viewport.Y - TopPadding - FooterHeight - 12)});
	Place(TEXT("Row_CollectionProgress"), {GridX, Viewport.Y - FooterHeight}, {GridWidth, 140});
	// A horizontal scroll box measures its child at its desired width. Fill all
	// available space on wide screens, retaining scrolling only when cards need it.
	if (auto* TrackSize = Cast<USizeBox>(GetWidgetFromName(TEXT("Size_ReadableMilestoneTrack"))))
	{
		const auto* RewardScroll = GetWidgetFromName(TEXT("Scroll_ReadableMilestones"));
		const float AvailableWidth = RewardScroll ? RewardScroll->GetCachedGeometry().GetLocalSize().X : 0.f;
		const float CardWidth = Row_CollectionMilestones ? Row_CollectionMilestones->GetChildrenCount() * 124.f : 0.f;
		const float TrackWidth = FMath::Max(FMath::Max(520.f, CardWidth), AvailableWidth);
		if (!FMath::IsNearlyEqual(TrackSize->GetMinDesiredWidth(), TrackWidth, .5f))
			TrackSize->SetMinDesiredWidth(TrackWidth);
	}
	// Detail navigation belongs above the card: the old next arrow covered the
	// detail text, and a percentage-only detail width made long names unreadable.
	const float DetailWidth = FMath::Clamp(Viewport.X * .42f, 360.f, 620.f);
	const float DetailX = FMath::Max(SidePadding, Viewport.X - SidePadding - DetailWidth);
	const float HeroWidth = FMath::Max(120.f, DetailX - SidePadding - 24.f);
	Place(TEXT("Size_Showcase"), {SidePadding, 112}, {HeroWidth, FMath::Max(140.f, Viewport.Y - 176.f)});
	Place(TEXT("Horizontal_PreviewRotation"), {SidePadding, Viewport.Y - 56.f}, {HeroWidth, 40});
	Place(TEXT("Size_Detail"), {DetailX, 112}, {DetailWidth, FMath::Max(140.f, Viewport.Y - 136.f)});
	Place(TEXT("Button_CollectionPrevious"), {Viewport.X - SidePadding - 240.f, 26.f}, {48, 44});
	Place(TEXT("Button_CollectionNext"), {Viewport.X - SidePadding - 176.f, 26.f}, {48, 44});
	if (TileView_Entries)
	{
		const float Width = TileView_Entries->GetCachedGeometry().GetLocalSize().X;
		if (Width > 1.f) TileView_Entries->SetResponsiveColumns(FMath::Max(1, FMath::FloorToInt(Width / 224.f)));
	}
	return true;
}

// 사이드바는 전체 조합 하나만 표시하고 선택 표시와 발견 개수만 갱신한다.
void UFF_FacilityCodexScreen::RefreshCollectionNavigation()
{
	if (!Column_CollectionCategories || !CollectionCategoryClass) return;
	if (CollectionCategories.Num() != 1)
	{
		if (Column_CollectionCategories->GetChildrenCount() == 1)
			if (auto* Authored = Cast<UFF_PrepChoiceButton>(Column_CollectionCategories->GetChildAt(0)))
				CollectionCategories.Add(Authored);
	}
	if (CollectionCategories.Num() != 1)
	{
		// Legacy category APIs remain available, but the collection has one clear reset action.
		Column_CollectionCategories->ClearChildren();
		CollectionCategories.Reset();
		if (auto* Choice = CreateWidget<UFF_PrepChoiceButton>(this, CollectionCategoryClass))
		{
			Choice->ConfigureChoice(0, LOCTEXT("All", "전체 조합"));
			Choice->OnHudTabButtonClicked.BindUObject(this,&ThisClass::HandleCollectionCategory);
			Column_CollectionCategories->AddChildToVerticalBox(Choice)->SetPadding(FMargin(0,0,0,12));
			CollectionCategories.Add(Choice);
		}
	}
	for (UFF_PrepChoiceButton* Choice : CollectionCategories)
		if (Choice)
		{
			Choice->ConfigureChoice(0, LOCTEXT("All", "전체 조합"));
			Choice->OnHudTabButtonClicked.BindUObject(this, &ThisClass::HandleCollectionCategory);
			Choice->SetSelected(!Filter.bFilterByCategory && Filter.FacilityDefinitionId.IsNone());
		}
	if(auto* Text=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_CollectionCount"))))
		Text->SetText(FText::Format(LOCTEXT("Count","{0} / {1}"),FText::AsNumber(Summary.DiscoveredCount),FText::AsNumber(Summary.TotalCount)));
}

void UFF_FacilityCodexScreen::HandleCollectionCategory(int32 Index)
{
	// The all-combinations entry also clears a facility inspector's launch filter.
	// That old filter row no longer belongs to the full-screen collection layout.
	if (Index == 0) Filter.FacilityDefinitionId = NAME_None;
	SetCategoryFilter(CategoryChoices.IsValidIndex(Index-1),CategoryChoices.IsValidIndex(Index-1)?CategoryChoices[Index-1]:EFFZSFacilityCategory::Booth);
}

// 발견 수로 보상 가능 여부를 표시한다. 실제 골드 지급은 보상 클릭 처리에서 요청한다.
void UFF_FacilityCodexScreen::RefreshCollectionMilestones()
{
	if(!Row_CollectionMilestones || !CollectionMilestoneClass) return;
	const TArray<FFF_FacilityCodexCollectionMilestone> Milestones=Codex?Codex->GetCollectionMilestones():TArray<FFF_FacilityCodexCollectionMilestone>();
	// The Pearl track spans the same width as the evenly spaced reward cards.
	// Legacy screens retain their plain discovered/total progress presentation.
	if (Progress_Discovery && GetWidgetFromName(TEXT("Overlay_CollectionProgressTrack")) && !Milestones.IsEmpty())
		Progress_Discovery->SetPercent(MilestoneTrackPercent(Milestones,Summary.DiscoveredCount,Summary.TotalCount));
	TArray<FName> Ids;
	Ids.Reserve(Milestones.Num());
	for(const auto& M:Milestones) Ids.Add(M.MilestoneId);
	if(Ids!=CollectionMilestoneIds || Row_CollectionMilestones->GetChildrenCount()!=Milestones.Num())
	{
		CollectionMilestones.Reset(); CollectionMilestoneIds=Ids;
		if (Row_CollectionMilestones->GetChildrenCount() == Milestones.Num())
			for (UWidget* Child : Row_CollectionMilestones->GetAllChildren())
				if (auto* Choice = Cast<UFF_PrepChoiceButton>(Child)) CollectionMilestones.Add(Choice);
		const bool bReuseAuthored = CollectionMilestones.Num() == Milestones.Num();
		if (!bReuseAuthored) { Row_CollectionMilestones->ClearChildren(); CollectionMilestones.Reset(); }
		for(int32 I=0;I<Milestones.Num();++I)
		{
			auto* Choice=bReuseAuthored ? CollectionMilestones[I].Get() : CreateWidget<UFF_PrepChoiceButton>(this,CollectionMilestoneClass);
			if(!Choice) continue;
			Choice->ConfigureChoice(I,FText::GetEmpty());
			Choice->OnHudTabButtonClicked.BindUObject(this,&ThisClass::HandleCollectionMilestone);
			if (!bReuseAuthored)
			{
				auto* MilestoneSlot=Row_CollectionMilestones->AddChildToHorizontalBox(Choice);
				MilestoneSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); MilestoneSlot->SetPadding(FMargin(12,0));
				MilestoneSlot->SetHorizontalAlignment(HAlign_Center);
				CollectionMilestones.Add(Choice);
			}
		}
	}
	for(UFF_PrepChoiceButton* C:CollectionMilestones)
	{
		if(!C||!Milestones.IsValidIndex(C->GetChoiceId())) continue;
		const auto& M=Milestones[C->GetChoiceId()];
		const int32 Remaining=FMath::Max(0,M.RequiredDiscoveries-M.DiscoveredCount);
		auto* Amount=Cast<UTextBlock>(C->GetWidgetFromName(TEXT("Text_MilestoneAmount")));
		auto* Status=Cast<UTextBlock>(C->GetWidgetFromName(TEXT("Text_MilestoneStatus")));
		// The new authored card gives threshold, Gold, and action separate visual roles.
		// Older saved widgets still receive their readable two-line fallback.
		C->ConfigureChoice(C->GetChoiceId(),Amount&&Status
			? FText::AsNumber(M.RequiredDiscoveries)
			: FText::Format(LOCTEXT("MilestoneFallback","{0}개 발견\n{1} · {2} G"),
				FText::AsNumber(M.RequiredDiscoveries),M.bClaimed?LOCTEXT("Claimed","수령 완료")
					:M.bCanClaim?LOCTEXT("Claim","받기"):LOCTEXT("Locked","잠김"),FText::AsNumber(M.RewardGold)));
		C->SetSelected(M.bCanClaim);
		if(Amount&&Status)
		{
			Amount->SetText(FText::AsNumber(M.RewardGold));
			Status->SetText(M.bClaimed?LOCTEXT("MilestoneCheck","✓"):FText::GetEmpty());
			Amount->SetColorAndOpacity(FSlateColor(M.bCanClaim?FLinearColor::White
				:M.bClaimed?FFBuildPearl::Color(241,246,253):FFBuildPearl::Color(197,209,224)));
			Status->SetColorAndOpacity(FSlateColor(FFBuildPearl::Color(29,78,94)));
			if(auto* Badge=Cast<UBorder>(C->GetWidgetFromName(TEXT("Border_MilestoneStatus"))))
			{
				Badge->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(221,240,246),10,FFBuildPearl::Cyan,1));
				Badge->SetBrushColor(FLinearColor::White);
				Badge->SetVisibility(M.bClaimed?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
			}
			if(auto* Gold=Cast<UImage>(C->GetWidgetFromName(TEXT("Image_MilestoneGold"))))
				Gold->SetColorAndOpacity(FLinearColor(1.f,1.f,1.f,M.bCanClaim?1.f:M.bClaimed?.85f:.55f));
		}
		// Claimed/locked milestones remain focusable for the progress explanation.
		C->SetToolTipText(FText::Format(LOCTEXT("MilestoneTip","조합 {0}개 발견 시 {1} G · 현재 {2}개{3}"),
			FText::AsNumber(M.RequiredDiscoveries),FText::AsNumber(M.RewardGold),FText::AsNumber(M.DiscoveredCount),
			M.bClaimed?LOCTEXT("Received"," · 수령 완료")
				:M.bCanClaim?LOCTEXT("ReadyTip"," · 클릭하여 받기")
					:FText::Format(LOCTEXT("RemainingTip"," · {0}개 남음"),FText::AsNumber(Remaining))));
	}
}

// 보상 상태를 다시 확인하고 지급 결과를 표시한다. 지급 도중 화면이 닫히면 갱신을 멈춘다.
void UFF_FacilityCodexScreen::HandleCollectionMilestone(int32 Index)
{
	if (!Codex || !CollectionMilestoneIds.IsValidIndex(Index) || bPreviewData) return;
	const TArray<FFF_FacilityCodexCollectionMilestone> Milestones = Codex->GetCollectionMilestones();
	const FName MilestoneId = CollectionMilestoneIds[Index];
	const FFF_FacilityCodexCollectionMilestone* Milestone = Milestones.FindByPredicate(
		[MilestoneId](const FFF_FacilityCodexCollectionMilestone& Candidate)
		{
			return Candidate.MilestoneId == MilestoneId;
		});
	if (!Milestone || !Milestone->bCanClaim) return;
	const UGameInstance* Game = GetGameInstance();
	const auto* Economy = Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	if (!Economy) return;
	const int32 PaidGold = static_cast<int32>(FMath::Min<int64>(Milestone->RewardGold,
		FMath::Max<int64>(0, int64(MAX_int32) - Economy->GetGold())));
	UFF_FacilityCodexSubsystem* ClaimCodex = Codex;
	const bool bClaimed = ClaimCodex->ClaimCollectionReward(MilestoneId);
	// A wallet listener can close the screen or replace it with editor preview data.
	if (Codex != ClaimCodex || bPreviewData) return;
	if (bClaimed) ShowRewardFeedback(PaidGold);
	RefreshCollectionMilestones();
}

void UFF_FacilityCodexScreen::HandleCollectionClicked(UObject* Object)
{
	const auto* Item=Cast<UFF_FacilityCodexItem>(Object);
	if (!Item) return;
	// The reward action lives inside a selectable tile. Keep an action click from
	// also triggering the tile's detail navigation if Slate forwards both events.
	if (TileView_Entries)
		for (UUserWidget* Widget : TileView_Entries->GetDisplayedEntryWidgets())
			if (const auto* Card=Cast<UFF_FacilityCodexCard>(Widget))
				if (Card->IsClaimActionUnderCursorFor(Item)) return;
	OpenCombinationDetail(Item->Entry.RuleId);
}
void UFF_FacilityCodexScreen::OpenCombinationDetail(FName RuleId)
{
	if(!Column_CollectionCategories||!ItemsByRule.Contains(RuleId)) return;
	SelectedRuleId=RuleId;
	if (TileView_Entries) TileView_Entries->SetSelectedItem(ItemsByRule.FindRef(RuleId));
	SetCollectionDetailOpen(true);
	SetDetailPage(1);
	RefreshDetail();
	SetKeyboardFocus();
}
void UFF_FacilityCodexScreen::SetCollectionDetailOpen(bool bOpen)
{
	bCollectionDetailOpen=bOpen;
	for(const TCHAR* Name:{TEXT("Horizontal_Toolbar"),TEXT("Size_CollectionSearch"),TEXT("Scroll_CollectionCategories"),TEXT("Size_Browse"),TEXT("Row_CollectionProgress")})
		if(auto* W=GetWidgetFromName(Name)) W->SetVisibility(bOpen?ESlateVisibility::Collapsed:ESlateVisibility::SelfHitTestInvisible);
	for(const TCHAR* Name:{TEXT("Size_Detail"),TEXT("Size_Showcase"),TEXT("Horizontal_PreviewRotation"),TEXT("Button_CollectionPrevious"),TEXT("Button_CollectionNext")})
		if(auto* W=GetWidgetFromName(Name)) W->SetVisibility(bOpen?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
	if(auto* Rotation=GetWidgetFromName(TEXT("Horizontal_PreviewRotation")))Rotation->SetVisibility(ESlateVisibility::Collapsed);
	if(auto* T=Cast<UTextBlock>(Button_Close?Button_Close->GetContent():nullptr))
		T->SetText(bOpen?LOCTEXT("Back","‹   조합 목록"):LOCTEXT("Title","‹   시설 조합 도감"));
	if(!bOpen)
	{
		bShowPreviewModel=false;
		if(Viewport_Showcase) Viewport_Showcase->ResetPreview();
	}
}
// 필터로 남은 항목 안에서만 앞·뒤 조합으로 이동하며 목록 끝에서는 현재 항목을 유지한다.
void UFF_FacilityCodexScreen::NavigateCollectionDetail(int32 Direction)
{
	if (VisibleItems.IsEmpty()) return;
	int32 Index = VisibleItems.IndexOfByPredicate([this](const auto& Item)
	{
		return Item && Item->Entry.RuleId == SelectedRuleId;
	});
	Index = FMath::Clamp(Index + Direction, 0, VisibleItems.Num() - 1);
	OpenCombinationDetail(VisibleItems[Index]->Entry.RuleId);
}
void UFF_FacilityCodexScreen::HandleCollectionPrevious()
{
	NavigateCollectionDetail(-1);
}

void UFF_FacilityCodexScreen::HandleCollectionNext()
{
	NavigateCollectionDetail(1);
}

#undef LOCTEXT_NAMESPACE
