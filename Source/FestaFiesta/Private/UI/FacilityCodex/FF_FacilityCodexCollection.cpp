#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"

#include "Building/Codex/FF_FacilityCodexSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
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
// Cards occupy equal width columns. Put each reward threshold at its card's center,
// then interpolate discoveries between those anchors so the track reaches the
// visible reward at the same count that enables it.
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

bool UFF_FacilityCodexScreen::UpdateCollectionLayout(const FVector2D& V)
{
	if (!Column_CollectionCategories) return false;
	if (Size_Dialog)
	{
		if (!FMath::IsNearlyEqual(Size_Dialog->GetWidthOverride(),V.X)) Size_Dialog->SetWidthOverride(V.X);
		if (!FMath::IsNearlyEqual(Size_Dialog->GetHeightOverride(),V.Y)) Size_Dialog->SetHeightOverride(V.Y);
	}
	auto Place = [this](const TCHAR* Name, FVector2D P, FVector2D S)
	{
		UWidget* W=GetWidgetFromName(Name);
		if(auto* CanvasSlot=W?Cast<UCanvasPanelSlot>(W->Slot):nullptr)
		{
			if(!CanvasSlot->GetPosition().Equals(P,.5)) CanvasSlot->SetPosition(P);
			if(!CanvasSlot->GetSize().Equals(S,.5)) CanvasSlot->SetSize(S);
		}
	};
	const float Side=FMath::Clamp(V.X*.035f,24.f,64.f);
	const float NavWidth=FMath::Clamp(V.X*.15f,174.f,240.f);
	const float Top=118.f;
	const float Footer=148.f;
	const float GridX=Side+NavWidth+18.f;
	Place(TEXT("Size_Refined_Button_Close"),{Side,24},{330,48});
	Place(TEXT("Horizontal_Toolbar"),{V.X*.44f,26},{V.X*.56f-Side,44});
	Place(TEXT("Size_CollectionSearch"),{Side,84},{NavWidth,36});
	Place(TEXT("Scroll_CollectionCategories"),{Side,136},{NavWidth,V.Y-172});
	Place(TEXT("Size_Browse"),{GridX,Top},{V.X-GridX-Side,FMath::Max(120.f,V.Y-Top-Footer-12)});
	Place(TEXT("Row_CollectionProgress"),{GridX,V.Y-Footer},{V.X-GridX-Side,120});
	const float DetailX=V.X*.61f;
	Place(TEXT("Size_Showcase"),{Side+54,V.Y*.23f},{V.X*.52f-Side,V.Y*.56f});
	Place(TEXT("Horizontal_PreviewRotation"),{V.X*.20f,V.Y*.81f},{V.X*.30f,40});
	Place(TEXT("Size_Detail"),{DetailX,112},{V.X-DetailX-Side,V.Y-160});
	Place(TEXT("Button_CollectionPrevious"),{Side,V.Y*.48f},{46,62});
	Place(TEXT("Button_CollectionNext"),{V.X-Side-46,V.Y*.48f},{46,62});
	if (TileView_Entries)
	{
		const float Width=TileView_Entries->GetCachedGeometry().GetLocalSize().X;
		if (Width>1.f) TileView_Entries->SetResponsiveColumns(FMath::FloorToInt(Width/190.f));
	}
	return true;
}

void UFF_FacilityCodexScreen::RefreshCollectionNavigation()
{
	if (!Column_CollectionCategories || !CollectionCategoryClass) return;
	if (CollectionCategories.Num() != CategoryChoices.Num() + 1)
	{
		// The presenter can set its facility filter before NativeConstruct populates categories.
		Column_CollectionCategories->ClearChildren();
		CollectionCategories.Reset();
		for (int32 I=0;I<=CategoryChoices.Num();++I)
		{
			auto* Choice=CreateWidget<UFF_PrepChoiceButton>(this,CollectionCategoryClass);
			if(!Choice) continue;
			const FText Label=I==0?LOCTEXT("All","전체 조합"):
				FFF_PrepBuildPresentationRules::ResolveCategoryDisplayName(CategoryChoices[I-1]);
			Choice->ConfigureChoice(I,Label);
			Choice->OnHudTabButtonClicked.BindUObject(this,&ThisClass::HandleCollectionCategory);
			Column_CollectionCategories->AddChildToVerticalBox(Choice)->SetPadding(FMargin(0,0,0,12));
			CollectionCategories.Add(Choice);
		}
	}
	const int32 Selected=Filter.bFilterByCategory?CategoryChoices.IndexOfByKey(Filter.Category)+1:0;
	for(UFF_PrepChoiceButton* C:CollectionCategories) if(C) C->SetSelected(C->GetChoiceId()==Selected);
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

void UFF_FacilityCodexScreen::RefreshCollectionMilestones()
{
	if(!Row_CollectionMilestones || !CollectionMilestoneClass) return;
	const TArray<FFF_FacilityCodexCollectionMilestone> Milestones=Codex?Codex->GetCollectionMilestones():TArray<FFF_FacilityCodexCollectionMilestone>();
	// The Pearl track spans the same width as the evenly spaced reward cards.
	// Legacy screens retain their plain discovered/total progress presentation.
	if (Progress_Discovery && GetWidgetFromName(TEXT("Overlay_CollectionProgressTrack")) && !Milestones.IsEmpty())
		Progress_Discovery->SetPercent(MilestoneTrackPercent(Milestones,Summary.DiscoveredCount,Summary.TotalCount));
	TArray<FName> Ids;
	for(const auto& M:Milestones) Ids.Add(M.MilestoneId);
	if(Ids!=CollectionMilestoneIds)
	{
		Row_CollectionMilestones->ClearChildren(); CollectionMilestones.Reset(); CollectionMilestoneIds=Ids;
		for(int32 I=0;I<Milestones.Num();++I)
		{
			auto* Choice=CreateWidget<UFF_PrepChoiceButton>(this,CollectionMilestoneClass);
			if(!Choice) continue;
			Choice->ConfigureChoice(I,FText::GetEmpty());
			Choice->OnHudTabButtonClicked.BindUObject(this,&ThisClass::HandleCollectionMilestone);
			auto* MilestoneSlot=Row_CollectionMilestones->AddChildToHorizontalBox(Choice);
			MilestoneSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); MilestoneSlot->SetPadding(FMargin(12,0));
			MilestoneSlot->SetHorizontalAlignment(HAlign_Center);
			CollectionMilestones.Add(Choice);
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

void UFF_FacilityCodexScreen::HandleCollectionMilestone(int32 Index)
{
	if(!Codex||!CollectionMilestoneIds.IsValidIndex(Index)||bPreviewData) return;
	const auto Milestones=Codex->GetCollectionMilestones();
	const FName Id=CollectionMilestoneIds[Index];
	const auto* M=Milestones.FindByPredicate([Id](const auto& V){return V.MilestoneId==Id;});
	if (!M || !M->bCanClaim) return;
	const UGameInstance* Game = GetGameInstance();
	const auto* Economy = Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	if (!Economy) return;
	const int32 PaidGold = static_cast<int32>(FMath::Min<int64>(M->RewardGold,
		FMath::Max<int64>(0, int64(MAX_int32) - Economy->GetGold())));
	UFF_FacilityCodexSubsystem* ClaimCodex = Codex;
	const bool bClaimed = ClaimCodex->ClaimCollectionReward(Id);
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
void UFF_FacilityCodexScreen::NavigateCollectionDetail(int32 Direction)
{
	if(VisibleItems.IsEmpty()) return;
	int32 Index=VisibleItems.IndexOfByPredicate([this](const auto& I){return I&&I->Entry.RuleId==SelectedRuleId;});
	Index=FMath::Clamp(Index+Direction,0,VisibleItems.Num()-1);
	OpenCombinationDetail(VisibleItems[Index]->Entry.RuleId);
}
void UFF_FacilityCodexScreen::HandleCollectionPrevious(){NavigateCollectionDetail(-1);}
void UFF_FacilityCodexScreen::HandleCollectionNext(){NavigateCollectionDetail(1);}

#undef LOCTEXT_NAMESPACE
