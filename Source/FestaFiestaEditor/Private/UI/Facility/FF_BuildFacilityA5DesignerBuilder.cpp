// One-time, additive migration of the approved facility inspector layout into its WBP.
#if WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "WidgetBlueprint.h"

namespace FFBuildFacilityA5Designer
{
using namespace FFUIWbpBuilder;

static bool Apply(UWidgetBlueprint& Blueprint)
{
    UWidgetTree* Tree = Blueprint.WidgetTree;
    if (!Tree || !Blueprint.ParentClass || !Blueprint.ParentClass->IsChildOf(UFF_FacilityInfoPanel::StaticClass())) return false;
    // The marker makes this migration one-shot, preserving later Designer edits.
    if (Tree->FindWidget(TEXT("A5_InspectorDesignerLayout"))) return true;

    auto* Overview = Cast<UButton>(Tree->FindWidget(TEXT("Button_TabOverview")));
    auto* Synergy = Cast<UButton>(Tree->FindWidget(TEXT("Button_TabSynergy")));
    auto* Operation = Cast<UButton>(Tree->FindWidget(TEXT("Button_TabOperation")));
    auto* Staff = Cast<UButton>(Tree->FindWidget(TEXT("Button_TabStaff")));
    auto* Tabs = Operation ? Cast<UHorizontalBox>(Operation->GetParent()) : nullptr;
    auto* Ranking = Cast<UButton>(Tree->FindWidget(TEXT("Button_RevenueRanking")));
    auto* BuildBorder = Cast<UBorder>(Tree->FindWidget(TEXT("Panel_BuildSection")));
    auto* Basics = BuildBorder ? Cast<UVerticalBox>(BuildBorder->GetContent()) : nullptr;
    if (!Overview || !Synergy || !Operation || !Staff || !Tabs || !Ranking || !Basics ||
        Overview->GetParent() != Tabs || Synergy->GetParent() != Tabs || Staff->GetParent() != Tabs ||
        Tree->FindWidget(TEXT("Button_OverviewRevenueRanking"))) return false;

    TArray<UButton*> Optional;
    for (const TCHAR* Name : {TEXT("Button_TabPrograms"), TEXT("Button_TabUpgrade"),
        TEXT("Button_TabLighting"), TEXT("Button_TabStaff"), TEXT("Button_TabRevenue")})
        if (auto* Button = Cast<UButton>(Tree->FindWidget(Name)))
        {
            if (!Cast<UHorizontalBox>(Button->GetParent())) return false;
            Optional.Add(Button);
        }

    UWidget* Anchor = Tabs;
    UVerticalBox* Column = nullptr;
    while (Anchor && Anchor->GetParent())
    {
        Column = Cast<UVerticalBox>(Anchor->GetParent());
        if (Column) break;
        Anchor = Anchor->GetParent();
    }
    if (!Column || !Anchor) return false;
    UHorizontalBox* Tools = nullptr;
    for (const TCHAR* Name : {TEXT("Reference_AdditionalFacilityPages"), TEXT("Clarity_SecondaryTabs"), TEXT("Panel_InspectorTools")})
        if (UWidget* Candidate = Tree->FindWidget(Name))
        {
            Tools = Cast<UHorizontalBox>(Candidate);
            if (!Tools || Tools == Tabs) return false;
            break;
        }
    for (const TCHAR* Name : {TEXT("Runtime_InspectorSpacer1"), TEXT("Runtime_InspectorSpacer2")})
        if (Tree->FindWidget(Name)) return false;

    Blueprint.Modify(); Tree->Modify(); Tabs->Modify(); Column->Modify(); Basics->Modify();
    if (!Tools) Tools = MakeWidget<UHorizontalBox>(Blueprint, *Tree, TEXT("Panel_InspectorTools"));
    if (!Tools) return false;
    Tools->Modify();
    if (Tools->GetParent() != Column)
    {
        Tools->RemoveFromParent();
        if (auto* RowSlot = Column->AddChildToVerticalBox(Tools)) RowSlot->SetPadding(FMargin(8, 0, 8, 6));
    }
    Column->ShiftChild(Column->GetChildIndex(Anchor) + 1, Tools);
    const UButton* ProgramTab = Cast<UButton>(Tree->FindWidget(TEXT("Button_TabPrograms")));
    const FButtonStyle TabStyle = ProgramTab ? ProgramTab->GetStyle() : Overview->GetStyle();
    for (UButton* Button : Optional)
    {
        Button->Modify();
        if (Button->GetParent() != Tools) { Button->RemoveFromParent(); Tools->AddChildToHorizontalBox(Button); }
        if (auto* TabSlot = Cast<UHorizontalBoxSlot>(Button->Slot))
        {
            TabSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            TabSlot->SetPadding(FMargin(2, 0));
            TabSlot->SetHorizontalAlignment(HAlign_Fill);
            TabSlot->SetVerticalAlignment(VAlign_Fill);
        }
        Button->SetStyle(TabStyle);
    }
    for (int32 Index = 0; Index < Optional.Num(); ++Index) Tools->ShiftChild(Index, Optional[Index]);
    int32 PrimaryIndex = 0;
    for (UButton* Button : {Overview, Synergy, Operation})
    {
        Tabs->ShiftChild(PrimaryIndex++, Button);
        Button->Modify(); Button->SetStyle(TabStyle);
        if (auto* TabSlot = Cast<UHorizontalBoxSlot>(Button->Slot))
        {
            TabSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            TabSlot->SetHorizontalAlignment(HAlign_Fill);
            TabSlot->SetVerticalAlignment(VAlign_Fill);
        }
    }
    for (const TCHAR* Name : {TEXT("Runtime_InspectorSpacer1"), TEXT("Runtime_InspectorSpacer2"), TEXT("A5_InspectorDesignerLayout")})
    {
        auto* Spacer = MakeWidget<USizeBox>(Blueprint, *Tree, Name);
        Spacer->SetVisibility(ESlateVisibility::Collapsed);
        if (auto* SpacerSlot = Tools->AddChildToHorizontalBox(Spacer)) SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }

    auto* OverviewRanking = MakeWidget<UButton>(Blueprint, *Tree, TEXT("Button_OverviewRevenueRanking"), true);
    auto* RankingLabel = MakeWidget<UCommonTextBlock>(Blueprint, *Tree, TEXT("Text_OverviewRevenueRanking"));
    OverviewRanking->SetStyle(Ranking->GetStyle());
    RankingLabel->SetText(FText::FromString(TEXT("시설별 매출 순위 →")));
    if (const auto* Ref = Cast<UCommonTextBlock>(Ranking->GetContent()))
    {
        RankingLabel->SetFont(Ref->GetFont());
        RankingLabel->SetColorAndOpacity(Ref->GetColorAndOpacity());
        RankingLabel->SetJustification(ETextJustify::Center);
    }
    OverviewRanking->AddChild(RankingLabel);
    if (auto* RankingSlot = Basics->AddChildToVerticalBox(OverviewRanking)) RankingSlot->SetPadding(FMargin(0, 6));
    Basics->ShiftChild(0, OverviewRanking);
    OverviewRanking->SetVisibility(ESlateVisibility::Collapsed);
    return SaveNewBlueprint(TEXT("FF_BUILD_FACILITY_A5_DESIGNER"), Blueprint);
}

static void Run()
{
    UWidgetBlueprint* Blueprint = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityInfoPanelPackage);
    const bool bSuccess = Blueprint && Apply(*Blueprint);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_FACILITY_A5_DESIGNER_%s"), bSuccess ? TEXT("PASS") : TEXT("FAIL"));
}

static FAutoConsoleCommand Command(TEXT("FF.ApplyBuildFacilityA5DesignerLayout"),
    TEXT("Move approved inspector tabs and overview ranking into the editable facility WBP; repeat runs preserve Designer edits."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
