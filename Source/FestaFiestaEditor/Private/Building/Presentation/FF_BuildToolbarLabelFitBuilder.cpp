// Save the requested toolbar hit-target and caption sizes without rebuilding its authored tree.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Blueprint/WidgetTree.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace FFBuildToolbarLabelFit
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Version = TEXT("FF.BuildToolbar.LabelFit.20261012.v1");
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_TOOLBAR_LABEL_FIT");
constexpr float ButtonWidth = 96.f;
constexpr float ButtonHeight = 78.f;

void Apply()
{
    if (GEditor && GEditor->PlayWorld)
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: stop PIE before saving the toolbar"), Prefix);
        return;
    }
    UWidgetBlueprint* BP = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildToolsPackage);
    if (!BP || !BP->WidgetTree || BP->ParentClass != UFF_BuildToolsWidget::StaticClass()) return;
    auto& Metadata = BP->GetOutermost()->GetMetaData();
    if (Metadata.GetValue(BP, Version) == TEXT("1"))
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE"), Prefix);
        return;
    }
    auto& Tree = *BP->WidgetTree;
    auto* Row = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_BuildReferenceTopBar")));
    auto* Top = Cast<UBorder>(Tree.FindWidget(TEXT("Border_BuildTopBar")));
    auto* Bounds = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildTopBar")));
    if (!Row || !Top || !Bounds) return;
    const TCHAR* Names[] = {TEXT("Button_MultiSelect"), TEXT("Button_Grid"), TEXT("Button_SnapCycle"),
        TEXT("Button_ToolbarRanges"), TEXT("Button_BuildSettings"), TEXT("Button_ToolbarCatalog"),
        TEXT("Button_ToolbarHelp"), TEXT("Button_FeatureGuide")};
    TArray<UButton*> Buttons;
    TArray<USizeBox*> Cells;
    TArray<UTextBlock*> Captions;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
    {
        auto* Button = Cast<UButton>(Tree.FindWidget(Names[Index]));
        const FName CaptionName = Index == 7 ? FName(TEXT("Text_FeatureGuide")) :
            FName(*FString::Printf(TEXT("Label_PearlTool%d"), Index + 1));
        auto* Caption = Cast<UTextBlock>(Tree.FindWidget(CaptionName));
        UWidget* Cell = Button;
        while (Cell && Cell->GetParent() && Cell->GetParent() != Row) Cell = Cell->GetParent();
        auto* Size = Cast<USizeBox>(Cell);
        if (!Button || !Button->GetContent() || !Caption || !Size || Size->GetParent() != Row ||
            !Cast<UHorizontalBoxSlot>(Size->Slot) || !Cast<UButtonSlot>(Button->GetContent()->Slot))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: authored toolbar contract missing for %s; no changes"), Prefix, Names[Index]);
            return;
        }
        Buttons.Add(Button); Cells.Add(Size); Captions.Add(Caption);
    }
    BP->Modify(); Tree.Modify(); Bounds->Modify();
    float ToolbarWidth = Top->GetPadding().Left + Top->GetPadding().Right;
    for (int32 Index = 0; Index < Buttons.Num(); ++Index)
    {
        UButton* Button = Buttons[Index];
        USizeBox* Cell = Cells[Index];
        UTextBlock* Caption = Captions[Index];
        Button->Modify(); Cell->Modify(); Caption->Modify();
        Cell->SetWidthOverride(ButtonWidth);
        Cell->SetHeightOverride(ButtonHeight);
        auto* RowSlot = CastChecked<UHorizontalBoxSlot>(Cell->Slot);
        RowSlot->Modify();
        RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        RowSlot->SetHorizontalAlignment(HAlign_Fill);
        RowSlot->SetVerticalAlignment(VAlign_Fill);
        const FMargin Padding = RowSlot->GetPadding();
        ToolbarWidth += ButtonWidth + Padding.Left + Padding.Right;
        if (auto* SizeSlot = Cast<USizeBoxSlot>(Button->Slot))
        {
            SizeSlot->Modify();
            SizeSlot->SetHorizontalAlignment(HAlign_Fill);
            SizeSlot->SetVerticalAlignment(VAlign_Fill);
        }
        auto* ContentSlot = CastChecked<UButtonSlot>(Button->GetContent()->Slot);
        ContentSlot->Modify();
        ContentSlot->SetPadding(FMargin(6.f, 4.f));
        ContentSlot->SetHorizontalAlignment(HAlign_Fill);
        ContentSlot->SetVerticalAlignment(VAlign_Center);
        Button->SetClipping(EWidgetClipping::ClipToBounds);
        FSlateFontInfo Font = Caption->GetFont();
        Font.Size = 13;
        if (auto* Common = Cast<UCommonTextBlock>(Caption)) Common->SetStyle(nullptr);
        Caption->SetFont(Font);
        Caption->SetAutoWrapText(false);
        Caption->SetJustification(ETextJustify::Center);
        Caption->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        Caption->SetClipping(EWidgetClipping::ClipToBounds);
        if (auto* CaptionSlot = Cast<UVerticalBoxSlot>(Caption->Slot))
        {
            CaptionSlot->Modify();
            CaptionSlot->SetHorizontalAlignment(HAlign_Fill);
        }
    }
    Bounds->SetWidthOverride(ToolbarWidth);
    Bounds->ClearHeightOverride();
    Bounds->SetMinDesiredHeight(ButtonHeight + Top->GetPadding().Top + Top->GetPadding().Bottom);
    Metadata.SetValue(BP, Version, TEXT("1"));
    if (!SaveNewBlueprint(Prefix, *BP)) Metadata.RemoveValue(BP, Version);
}

FAutoConsoleCommand Command(TEXT("FF.ApplyBuildToolbarLabelFit"),
    TEXT("Enlarge the eight authored build toolbar buttons and fit their Korean captions without replacing the widget tree."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
