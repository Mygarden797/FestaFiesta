// FF_FacilityRevenuePearlBuilder.cpp
// Repaint saved ranking widgets with the construction pearl palette and keep the ledger bindings.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ListViewBase.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FacilityRevenue/FF_FacilityRevenueRanking.h"
#include "UObject/MetaData.h"
#include "WidgetBlueprint.h"

namespace FFFacilityRevenuePearlBuilder
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_FACILITY_REVENUE_PEARL");
constexpr const TCHAR* Version = TEXT("FF.FacilityRevenue.PearlV2");

template <typename T>
T* Find(UWidgetTree& Tree, const TCHAR* Name)
{
    return Cast<T>(Tree.FindWidget(Name));
}

void Text(UWidgetTree& Tree, const TCHAR* Name, const int32 Size,
    const FLinearColor Color, const FName Weight = TEXT("SemiBold"))
{
    if (UTextBlock* Label = Find<UTextBlock>(Tree, Name))
        StyleText(*Label, Size, Color, Weight);
}

void Border(UWidgetTree& Tree, const TCHAR* Name, const FSlateBrush& Brush,
    const FMargin Padding)
{
    if (UBorder* Surface = Find<UBorder>(Tree, Name))
    {
        Surface->SetBrush(Brush);
        Surface->SetBrushColor(FLinearColor::White);
        Surface->SetPadding(Padding);
    }
}

void Button(UWidgetTree& Tree, const TCHAR* Name, const bool bPrimary,
    const bool bPeriod = false)
{
    UButton* Control = Find<UButton>(Tree, Name);
    if (!Control) return;
    FButtonStyle Style = Control->GetStyle();
    Style.Normal = bPrimary
        ? FFBuildPearl::Rounded(FFBuildPearl::Charcoal, 18.f)
        : FFBuildPearl::Rounded(FFBuildPearl::Color(249, 249, 255, .97f),
            18.f, FFBuildPearl::Line, 1.f);
    Style.Hovered = bPrimary
        ? FFBuildPearl::Rounded(FFBuildPearl::Color(82, 83, 112), 18.f,
            FFBuildPearl::Cyan, 1.f)
        : FFBuildPearl::Rounded(FFBuildPearl::Color(228, 245, 255), 18.f,
            FFBuildPearl::Cyan, 1.f);
    Style.Pressed = FFBuildPearl::Rounded(FFBuildPearl::Cyan, 18.f);
    // UpdatePeriod uses Disabled as the selected tab brush while keeping the button active.
    Style.Disabled = bPeriod
        ? FFBuildPearl::Rounded(FFBuildPearl::Charcoal, 18.f)
        : FFBuildPearl::Rounded(FFBuildPearl::Color(177, 181, 201, .84f), 18.f);
    Style.NormalPadding = Style.PressedPadding = FMargin(11.f, 5.f);
    Control->SetStyle(Style);
    Control->SetBackgroundColor(FLinearColor::White);
}

bool Row(UWidgetBlueprint&, UWidgetTree& Tree)
{
    USizeBox* Bounds = Find<USizeBox>(Tree, TEXT("Size_Row"));
    USizeBox* AmountColumn = Find<USizeBox>(Tree, TEXT("Size_AuroraRevenueAmount"));
    UTextBlock* Amount = Find<UTextBlock>(Tree, TEXT("Text_Amount"));
    UTextBlock* Rank = Find<UTextBlock>(Tree, TEXT("Text_Rank"));
    if (!Bounds || !AmountColumn || !Amount || !Rank ||
        !Find<UBorder>(Tree, TEXT("Border_Row"))) return false;
    Bounds->SetHeightOverride(88.f);
    AmountColumn->SetWidthOverride(146.f);
    if (USizeBox* RankBounds = Find<USizeBox>(Tree, TEXT("Size_Rank")))
        RankBounds->SetWidthOverride(32.f);
    Border(Tree, TEXT("Border_Row"), FFBuildPearl::MaterialBrush(TEXT("Card")),
        FMargin(12.f, 9.f));
    Border(Tree, TEXT("Border_Thumbnail"),
        FFBuildPearl::Rounded(FFBuildPearl::Color(234, 241, 250, .96f), 14.f,
            FFBuildPearl::Line, 1.f), FMargin(3.f));
    Text(Tree, TEXT("Text_Rank"), 21, FFBuildPearl::Color(58, 136, 180), TEXT("Bold"));
    Text(Tree, TEXT("Text_Name"), 17, FFBuildPearl::Ink, TEXT("Bold"));
    Text(Tree, TEXT("Text_Location"), 12, FFBuildPearl::Muted, TEXT("Regular"));
    Text(Tree, TEXT("Text_Amount"), 18, FFBuildPearl::Ink, TEXT("Bold"));
    Text(Tree, TEXT("Text_Share"), 12, FFBuildPearl::Muted);
    Text(Tree, TEXT("Text_ThumbnailFallback"), 12, FFBuildPearl::Muted);
    Rank->SetJustification(ETextJustify::Center);
    Amount->SetJustification(ETextJustify::Right);
    Amount->SetAutoWrapText(false);
    if (UTextBlock* Share = Find<UTextBlock>(Tree, TEXT("Text_Share")))
    {
        Share->SetJustification(ETextJustify::Right);
        Share->SetAutoWrapText(false);
    }
    if (UProgressBar* Bar = Find<UProgressBar>(Tree, TEXT("Bar_Revenue")))
    {
        Bar->SetFillColorAndOpacity(FFBuildPearl::Color(75, 170, 214));
        FProgressBarStyle Style = Bar->GetWidgetStyle();
        Style.BackgroundImage = FFBuildPearl::Rounded(FFBuildPearl::Line, 5.f);
        Style.FillImage = FFBuildPearl::Rounded(FLinearColor::White, 5.f);
        Bar->SetWidgetStyle(Style);
    }
    return true;
}

bool Panel(UWidgetBlueprint&, UWidgetTree& Tree)
{
    USizeBox* Frame = Find<USizeBox>(Tree, TEXT("Size_ClarityFrame"));
    UBorder* Surface = Find<UBorder>(Tree, TEXT("Border_Panel"));
    UTextBlock* Total = Find<UTextBlock>(Tree, TEXT("Text_Total"));
    if (!Frame || !Surface || !Total ||
        !Find<UButton>(Tree, TEXT("Button_Today"))) return false;
    Frame->SetWidthOverride(600.f);
    if (UListViewBase* List = Find<UListViewBase>(Tree, TEXT("List_Ranking")))
        List->SetScrollbarVisibility(ESlateVisibility::Collapsed);
    Border(Tree, TEXT("Border_Panel"), FFBuildPearl::MaterialBrush(TEXT("Card")),
        FMargin(20.f, 18.f));
    Border(Tree, TEXT("Border_Total"),
        FFBuildPearl::Rounded(FFBuildPearl::Color(232, 245, 255, .96f), 20.f,
            FFBuildPearl::Cyan, 1.f), FMargin(17.f, 13.f));
    Border(Tree, TEXT("Border_FirstRevenue"),
        FFBuildPearl::Rounded(FFBuildPearl::Color(242, 247, 255, .95f), 17.f,
            FFBuildPearl::Line, 1.f), FMargin(13.f, 9.f));
    Text(Tree, TEXT("Text_Title"), 25, FFBuildPearl::Ink, TEXT("Bold"));
    Text(Tree, TEXT("Text_TotalLabel"), 13, FFBuildPearl::Muted);
    Text(Tree, TEXT("Text_Total"), 30, FFBuildPearl::Ink, TEXT("Bold"));
    Text(Tree, TEXT("Text_Count"), 13, FFBuildPearl::Muted);
    Text(Tree, TEXT("Text_Empty"), 15, FFBuildPearl::Muted, TEXT("Regular"));
    Text(Tree, TEXT("Text_FirstRevenueDescription"), 13, FFBuildPearl::Muted, TEXT("Regular"));
    Total->SetJustification(ETextJustify::Right);
    Total->SetAutoWrapText(false);
    Button(Tree, TEXT("Button_Close"), false);
    Button(Tree, TEXT("Button_Today"), false, true);
    Button(Tree, TEXT("Button_Total"), false, true);
    Button(Tree, TEXT("Button_Inspect"), true);
    Text(Tree, TEXT("Button_Close_Label"), 14, FFBuildPearl::Ink);
    Text(Tree, TEXT("Button_Today_Label"), 15, FFBuildPearl::Ink);
    Text(Tree, TEXT("Button_Total_Label"), 15, FFBuildPearl::Ink);
    Text(Tree, TEXT("Button_Inspect_Label"), 16, FLinearColor::White, TEXT("Bold"));
    return true;
}

bool ApplyOne(const TCHAR* Package, UClass& Expected,
    bool (*Edit)(UWidgetBlueprint&, UWidgetTree&))
{
    UWidgetBlueprint* BP = LoadExistingBlueprint(Package);
    if (!BP || !BP->WidgetTree || !BP->GeneratedClass ||
        !BP->GeneratedClass->IsChildOf(&Expected))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_CONTRACT_FAIL %s"), Prefix, Package);
        return false;
    }
    auto& Metadata = BP->GetOutermost()->GetMetaData();
    if (Metadata.GetValue(BP, Version) == TEXT("1")) return true;
    BP->Modify();
    BP->WidgetTree->Modify();
    if (!Edit(*BP, *BP->WidgetTree))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_LAYOUT_FAIL %s"), Prefix, Package);
        return false;
    }
    Metadata.SetValue(BP, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, *BP)) return true;
    Metadata.RemoveValue(BP, Version);
    return false;
}

void Apply()
{
    const bool bRow = ApplyOne(FFBuildUIAssetPaths::FacilityRevenueRankRowPackage,
        *UFF_FacilityRevenueRankRow::StaticClass(), Row);
    const bool bPanel = ApplyOne(FFBuildUIAssetPaths::FacilityRevenueRankingPackage,
        *UFF_FacilityRevenueRankingPanel::StaticClass(), Panel);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_COMPLETE_%s (row=%d, panel=%d)"),
        Prefix, bRow && bPanel ? TEXT("PASS") : TEXT("FAIL"), bRow, bPanel);
}

FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityRevenuePearlUI"),
    TEXT("Restyle saved facility revenue ranking and row WBP assets with pearl surfaces and aligned Gold values."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}

#endif
