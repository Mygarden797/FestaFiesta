// Explicit, versioned migration of the existing authored codex. No runtime tree construction.
#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/StackBox.h"
#include "Components/StackBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace FFFacilityCodexClarity
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_CLARITY_CODEX");
constexpr const TCHAR* Version = TEXT("FF.BuildClarity.Codex20260920.1");
static FLinearColor Color(uint8 R, uint8 G, uint8 B)
{ return FLinearColor::FromSRGBColor(FColor(R, G, B)); }
static const FLinearColor Ink = Color(32, 76, 54), Muted = Color(94, 119, 102),
    Paper = Color(242, 247, 238), Surface = Color(252, 254, 249),
    Sage = Color(226, 236, 221), Line = Color(195, 212, 191), Forest = Color(42, 87, 65);

static FSlateBrush Rounded(FLinearColor Fill, float Radius = 8.f)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(Fill);
    Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Line), 1.f);
    return Brush;
}

static FSlateBrush PanelBrush(const TCHAR* Role, FLinearColor Fallback)
{
    const FString Name = FString(TEXT("MI_UI_BuildClarity_")) + Role;
    const FString Path = FString(TEXT("/Game/FestaFiesta/UI/Building/Clarity/")) + Name + TEXT(".") + Name;
    FSlateBrush Brush = Rounded(Fallback, 12.f);
    if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Path))
    {
        Brush = FSlateBrush();
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.SetResourceObject(Material);
        Brush.ImageSize = FVector2D(128, 48);
        Brush.TintColor = FSlateColor(FLinearColor::White);
    }
    return Brush;
}

static void Font(UWidgetTree& Tree, FName Name, int32 Size, bool bMuted = false)
{
    if (UTextBlock* Text = Cast<UTextBlock>(Tree.FindWidget(Name)))
    {
        FSlateFontInfo Info = Text->GetFont();
        Info.Size = Size;
        Text->SetFont(Info);
        Text->SetColorAndOpacity(FSlateColor(bMuted ? Muted : Ink));
    }
}

static void SurfaceBrush(UBorder* Border, const FSlateBrush& Brush)
{
    if (!Border) return;
    Border->SetBrush(Brush);
    Border->SetBrushColor(FLinearColor::White);
}

static UHorizontalBoxSlot* H(UHorizontalBox* Parent, UWidget* Child, bool bFill = false, FMargin Padding = FMargin(0))
{
    Child->RemoveFromParent();
    UHorizontalBoxSlot* Slot = Parent->AddChildToHorizontalBox(Child);
    Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    Slot->SetVerticalAlignment(VAlign_Center);
    Slot->SetPadding(Padding);
    return Slot;
}

static UVerticalBoxSlot* V(UVerticalBox* Parent, UWidget* Child, bool bFill = false, FMargin Padding = FMargin(0))
{
    Child->RemoveFromParent();
    UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child);
    Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    Slot->SetPadding(Padding);
    return Slot;
}

static void StyleButton(UButton& Button)
{
    FButtonStyle Style = Button.GetStyle();
    Style.Normal = Rounded(Surface, 6);
    Style.Hovered = Rounded(Sage, 6);
    Style.Pressed = Style.Disabled = Rounded(Forest, 6);
    Style.NormalPadding = Style.PressedPadding = FMargin(14, 7);
    Button.SetStyle(Style);
    if (UTextBlock* Text = Cast<UTextBlock>(Button.GetContent()))
    {
        FSlateFontInfo FontInfo = Text->GetFont();
        FontInfo.Size = 14;
        Text->SetFont(FontInfo);
        Text->SetAutoWrapText(false);
        Text->SetColorAndOpacity(FSlateColor(Ink));
    }
}

static UButton* Button(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, const TCHAR* Caption)
{
    UButton* Result = MakeWidget<UButton>(BP, Tree, Name, true);
    UTextBlock* Label = MakeWidget<UTextBlock>(BP, Tree, FName(Name.ToString() + TEXT("_Label")));
    StyleText(*Label, 14, Ink, TEXT("Medium"));
    Label->SetText(FText::FromString(Caption));
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    Result->AddChild(Label);
    StyleButton(*Result);
    return Result;
}

static bool Card(UWidgetBlueprint& BP)
{
    if (BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1")) return true;
    UWidgetTree* Tree = BP.WidgetTree;
    if (!Tree || !Cast<USizeBox>(Tree->FindWidget(TEXT("Size_Card")))) return false;
    BP.Modify(); Tree->Modify();
    CastChecked<USizeBox>(Tree->FindWidget(TEXT("Size_Card")))->SetHeightOverride(170.f);
    CastChecked<USizeBox>(Tree->FindWidget(TEXT("Size_Name")))->SetHeightOverride(42.f);
    for (const FName Name : {TEXT("Size_ThumbTarget"), TEXT("Size_ThumbPartner")})
        if (USizeBox* Bounds = Cast<USizeBox>(Tree->FindWidget(Name))) Bounds->SetHeightOverride(44.f);
    if (UBorder* Body = Cast<UBorder>(Tree->FindWidget(TEXT("Border_Card"))))
    {
        SurfaceBrush(Body, Rounded(Surface));
        Body->SetPadding(FMargin(9));
    }
    SurfaceBrush(Cast<UBorder>(Tree->FindWidget(TEXT("Border_State"))), Rounded(Sage, 5));
    Font(*Tree, TEXT("Text_Name"), 15);
    Font(*Tree, TEXT("Text_Bonus"), 16);
    Font(*Tree, TEXT("Text_State"), 12, true);
    for (const FName Name : {TEXT("Text_Name"), TEXT("Text_State"), TEXT("Text_Bonus")})
        if (UTextBlock* Text = Cast<UTextBlock>(Tree->FindWidget(Name))) Text->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    BP.GetOutermost()->GetMetaData().SetValue(&BP, Version, TEXT("1"));
    return SaveNewBlueprint(Prefix, BP);
}

static bool Screen(UWidgetBlueprint& BP)
{
    if (BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1")) return true;
    UWidgetTree* Tree = BP.WidgetTree;
    if (!Tree) return false;
    auto* Root = Cast<UBorder>(Tree->FindWidget(TEXT("Border_InputShield")));
    auto* PaperBorder = Cast<UBorder>(Tree->FindWidget(TEXT("Border_Paper")));
    auto* Main = Cast<UStackBox>(Tree->FindWidget(TEXT("Stack_Main")));
    auto* DetailBounds = Cast<USizeBox>(Tree->FindWidget(TEXT("Size_Detail")));
    auto* Detail = Cast<UBorder>(Tree->FindWidget(TEXT("Panel_Detail")));
    auto* Tiles = Cast<UFF_FacilityCodexTileView>(Tree->FindWidget(TEXT("TileView_Entries")));
    const FName Required[] = {TEXT("Text_DetailTitle"), TEXT("Horizontal_Ingredients"), TEXT("Border_Bonus"),
        TEXT("Text_Distance"), TEXT("Text_Discovery"), TEXT("Text_Condition"), TEXT("Text_Applied")};
    if (!Root || !PaperBorder || !Main || Main->GetChildrenCount() != 2 || !DetailBounds || !Detail || !Tiles) return false;
    for (FName Name : Required) if (!Tree->FindWidget(Name)) return false;
    if (Tree->FindWidget(TEXT("Size_Dialog"))) return false;
    UWidget* RetiredDetail = Detail->GetContent();
    BP.Modify(); Tree->Modify();

    // Retain the production tree and all bindings, replacing only the detail layout seam.
    PaperBorder->RemoveFromParent();
    USizeBox* Dialog = MakeWidget<USizeBox>(BP, *Tree, TEXT("Size_Dialog"), true);
    Dialog->SetWidthOverride(1320.f); Dialog->SetHeightOverride(800.f);
    Root->SetPadding(FMargin(0));
    Root->SetHorizontalAlignment(HAlign_Center); Root->SetVerticalAlignment(VAlign_Center);
    Root->AddChild(Dialog); Dialog->AddChild(PaperBorder);
    SurfaceBrush(PaperBorder, PanelBrush(TEXT("Paper"), Paper));
    if (UBorder* Header = Cast<UBorder>(Tree->FindWidget(TEXT("Border_Header"))))
    {
        SurfaceBrush(Header, PanelBrush(TEXT("Header"), Sage));
        Header->SetPadding(FMargin(20, 13));
    }
    if (UWidget* Intro = Tree->FindWidget(TEXT("Text_Intro"))) Intro->SetVisibility(ESlateVisibility::Collapsed);
    if (UWidget* Footer = Tree->FindWidget(TEXT("Text_Footer"))) Footer->SetVisibility(ESlateVisibility::Collapsed);
    if (UBorder* Body = Cast<UBorder>(Tree->FindWidget(TEXT("Border_Body")))) Body->SetPadding(FMargin(18, 12));
    SurfaceBrush(Cast<UBorder>(Tree->FindWidget(TEXT("Border_Summary"))), Rounded(Sage, 6));
    Font(*Tree, TEXT("Text_Title"), 23);
    Font(*Tree, TEXT("Text_Summary"), 14);
    Font(*Tree, TEXT("Text_Filter"), 13, true);
    if (UHorizontalBox* HeaderRow = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("Horizontal_Header"))))
    {
        UTextBlock* Summary = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Summary")));
        UWidget* Close = Tree->FindWidget(TEXT("Button_Close"));
        if (Summary && Close && Close->GetParent() == HeaderRow)
        {
            Summary->SetAutoWrapText(false);
            H(HeaderRow, Summary, false, FMargin(20, 0, 20, 0));
            HeaderRow->ShiftChild(HeaderRow->GetChildIndex(Close), Summary);
            if (UWidget* EmptySummary = Tree->FindWidget(TEXT("Border_Summary"))) EmptySummary->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    Tree->ForEachWidget([](UWidget* Widget) { if (UButton* Existing = Cast<UButton>(Widget)) StyleButton(*Existing); });
    if (UEditableTextBox* Search = Cast<UEditableTextBox>(Tree->FindWidget(TEXT("Search_Entries"))))
    {
        FEditableTextBoxStyle Style = Search->GetWidgetStyle();
        Style.TextStyle.Font.Size = 14;
        Style.SetBackgroundImageNormal(Rounded(Surface, 6)).SetBackgroundImageHovered(Rounded(Sage, 6))
            .SetBackgroundImageFocused(Rounded(Surface, 6)).SetPadding(FMargin(12, 7));
        Search->SetWidgetStyle(Style);
    }

    Main->SetOrientation(Orient_Vertical);
    CastChecked<UStackBoxSlot>(Main->GetChildAt(0)->Slot)->SetPadding(FMargin(0, 0, 0, 12));
    DetailBounds->ClearWidthOverride(); DetailBounds->SetHeightOverride(184.f);
    Tiles->ColumnCount = 6;
    Tiles->SetEntryHeight(180.f); Tiles->SetHorizontalEntrySpacing(8.f); Tiles->SetVerticalEntrySpacing(10.f);
    SurfaceBrush(Detail, Rounded(Surface, 8)); Detail->SetPadding(FMargin(16, 12));

    // Style the retained source widgets while the original detail subtree is still attached.
    for (const FName Name : {TEXT("Size_ThumbTarget"), TEXT("Size_ThumbPartner")})
        if (USizeBox* Bounds = Cast<USizeBox>(Tree->FindWidget(Name)))
        { Bounds->SetWidthOverride(130.f); Bounds->SetHeightOverride(50.f); }
    Font(*Tree, TEXT("Text_TargetName"), 14);
    Font(*Tree, TEXT("Text_PartnerName"), 14);
    Font(*Tree, TEXT("Text_Bonus"), 18);
    Font(*Tree, TEXT("Text_Distance"), 15);
    Font(*Tree, TEXT("Text_Condition"), 14);
    Font(*Tree, TEXT("Text_Discovery"), 13, true);
    Font(*Tree, TEXT("Text_Applied"), 15);
    SurfaceBrush(Cast<UBorder>(Tree->FindWidget(TEXT("Border_Bonus"))), Rounded(Sage, 6));
    UVerticalBox* DetailColumn = MakeWidget<UVerticalBox>(BP, *Tree, TEXT("Vertical_ClarityDetail"));
    UHorizontalBox* DetailHeader = MakeWidget<UHorizontalBox>(BP, *Tree, TEXT("Horizontal_DetailTabs"));
    Font(*Tree, TEXT("Text_DetailTitle"), 18);
    H(DetailHeader, Tree->FindWidget(TEXT("Text_DetailTitle")), true, FMargin(0, 0, 16, 0));
    H(DetailHeader, Button(BP, *Tree, TEXT("Button_DetailRecipe"), TEXT("조합")), false, FMargin(0, 0, 6, 0));
    H(DetailHeader, Button(BP, *Tree, TEXT("Button_DetailPlacement"), TEXT("배치 조건")), false, FMargin(0, 0, 6, 0));
    H(DetailHeader, Button(BP, *Tree, TEXT("Button_DetailEffect"), TEXT("현재 효과")));
    V(DetailColumn, DetailHeader, false, FMargin(0, 0, 0, 10));
    UWidgetSwitcher* Pages = MakeWidget<UWidgetSwitcher>(BP, *Tree, TEXT("Switcher_Detail"), true);
    V(DetailColumn, Pages, true);
    UHorizontalBox* Recipe = MakeWidget<UHorizontalBox>(BP, *Tree, TEXT("Horizontal_DetailRecipe"));
    Pages->AddChild(Recipe);
    H(Recipe, Tree->FindWidget(TEXT("Horizontal_Ingredients")), true);
    H(Recipe, Tree->FindWidget(TEXT("Border_Bonus")), true, FMargin(18, 0, 0, 0));
    UVerticalBox* Placement = MakeWidget<UVerticalBox>(BP, *Tree, TEXT("Vertical_DetailPlacement"));
    Pages->AddChild(Placement);
    V(Placement, Tree->FindWidget(TEXT("Text_Distance")), false, FMargin(0, 0, 0, 4));
    V(Placement, Tree->FindWidget(TEXT("Text_Condition")), false, FMargin(0, 0, 0, 4));
    V(Placement, Tree->FindWidget(TEXT("Text_Discovery")));
    UVerticalBox* Effect = MakeWidget<UVerticalBox>(BP, *Tree, TEXT("Vertical_DetailEffect"));
    Pages->AddChild(Effect);
    V(Effect, Tree->FindWidget(TEXT("Text_Applied")));
    Pages->SetActiveWidgetIndex(0);
    Detail->ClearChildren(); Detail->AddChild(DetailColumn);
    // Preserve the original scroll composition and any designer-only contents as a
    // collapsed child. Leaving it detached creates orphan widgets and prevents saving.
    if (RetiredDetail)
    {
        RetiredDetail->SetVisibility(ESlateVisibility::Collapsed);
        V(DetailColumn, RetiredDetail);
    }
    BP.GetOutermost()->GetMetaData().SetValue(&BP, Version, TEXT("1"));
    const bool bSaved = SaveNewBlueprint(Prefix, BP);
    // A failed compile/save must never turn a later invocation into a false success.
    if (!bSaved) BP.GetOutermost()->GetMetaData().RemoveValue(&BP, Version);
    return bSaved;
}

static void Run()
{
    UWidgetBlueprint* CardBP = LoadExistingBlueprint(TEXT("/Game/FestaFiesta/UI/FacilityCodex/WBP_FF_FacilityCodexCard"));
    UWidgetBlueprint* ScreenBP = LoadExistingBlueprint(TEXT("/Game/FestaFiesta/UI/FacilityCodex/WBP_FF_FacilityCodexScreen"));
    if (!CardBP || !ScreenBP || !CardBP->ParentClass || !ScreenBP->ParentClass ||
        !CardBP->ParentClass->IsChildOf(UFF_FacilityCodexCard::StaticClass()) ||
        !ScreenBP->ParentClass->IsChildOf(UFF_FacilityCodexScreen::StaticClass())) return;
    const bool bSaved = Card(*CardBP) && Screen(*ScreenBP);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_COMPLETE_%s"), Prefix, bSaved ? TEXT("PASS") : TEXT("FAIL"));
}

static FAutoConsoleCommand Command(TEXT("FF.ApplyBuildModeClarityCodex"),
    TEXT("Apply the centered six-column codex and direct detail tabs once, preserving production widgets and data."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
