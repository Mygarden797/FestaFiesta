// Explicit saved-widget migration. Runtime only binds data and adapts the authored layout.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/StackBox.h"
#include "Components/StackBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Editor.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace FFFacilityCodexGlassBuilder
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_CODEX_GLASS_THEME");
constexpr const TCHAR* Version = TEXT("FF.FacilityCodex.GlassTheme20260921.1");
constexpr const TCHAR* ScrollbarVersion = TEXT("FF.FacilityCodex.GlassScrollbars20260921.1");
constexpr const TCHAR* CardPath = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexCard");
constexpr const TCHAR* ScreenPath = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/WBP_FF_FacilityCodexScreen");

template<class T> static T* Get(UWidgetTree& Tree, const TCHAR* Name)
{ return Cast<T>(Tree.FindWidget(FName(Name))); }

static FLinearColor Color(uint8 R, uint8 G, uint8 B, float A = 1.f)
{ FLinearColor Result = FLinearColor::FromSRGBColor(FColor(R, G, B)); Result.A = A; return Result; }
static const FLinearColor Cream = Color(250, 247, 227), Mint = Color(172, 224, 206),
    Muted = Color(190, 213, 205), Gold = Color(247, 215, 153), Ink = Color(26, 67, 62);

static UFont* FontAsset(const TCHAR* Name)
{
    return LoadObject<UFont>(nullptr, *(FString(TEXT("/Game/Asset/Font/")) + Name + TEXT(".") + Name));
}
static FSlateFontInfo Font(UFont* Asset, int32 Size)
{
    FSlateFontInfo Result;
    Result.FontObject = Asset;
    Result.Size = Size;
    Result.TypefaceFontName = Asset && Asset->GetInternalCompositeFont().DefaultTypeface.Fonts.Num() > 0
        ? Asset->GetInternalCompositeFont().DefaultTypeface.Fonts[0].Name : NAME_None;
    return Result;
}
static UMaterialInterface* Material(const TCHAR* Role)
{
    const FString Name = FString(TEXT("MI_FF_CodexGlass_")) + Role;
    return LoadObject<UMaterialInterface>(nullptr,
        *(FString(TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Materials/")) + Name + TEXT(".") + Name));
}
static FSlateBrush Rounded(FLinearColor Fill, FLinearColor Line, float Radius = 10.f, float Width = 1.f)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(Fill);
    Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Line), Width);
    return Brush;
}
static void Surface(UBorder* Border, const TCHAR* Role)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.SetResourceObject(Material(Role));
    Brush.ImageSize = FVector2D::ZeroVector;
    Brush.TintColor = FSlateColor(FLinearColor::White);
    Border->SetBrush(Brush);
    Border->SetBrushColor(FLinearColor::White);
}
static void Label(UWidgetTree& Tree, const TCHAR* Name, UFont* Asset, int32 Size, FLinearColor Tint)
{
    if (auto* Text = Get<UTextBlock>(Tree, Name))
    {
        Text->SetFont(Font(Asset, Size));
        Text->SetColorAndOpacity(FSlateColor(Tint));
        Text->SetShadowOffset(FVector2D::ZeroVector);
    }
}
static UVerticalBoxSlot* V(UVerticalBox* Parent, UWidget* Child, FMargin Padding = FMargin(0), bool Fill = false)
{
    Child->RemoveFromParent();
    auto* Slot = Parent->AddChildToVerticalBox(Child);
    Slot->SetPadding(Padding);
    Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    return Slot;
}
static UStackBoxSlot* S(UStackBox* Parent, UWidget* Child, FMargin Padding = FMargin(0), bool Fill = false)
{
    Child->RemoveFromParent();
    auto* Slot = Parent->AddChildToStackBox(Child);
    Slot->SetPadding(Padding);
    Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    return Slot;
}
static void O(UOverlay* Parent, UWidget* Child)
{
    Child->RemoveFromParent();
    auto* Slot = Parent->AddChildToOverlay(Child);
    Slot->SetHorizontalAlignment(HAlign_Fill);
    Slot->SetVerticalAlignment(VAlign_Fill);
}
static void SetDefaultColor(UWidgetBlueprint& BP, const TCHAR* Name, FLinearColor Value)
{
    UObject* Defaults = BP.GeneratedClass ? BP.GeneratedClass->GetDefaultObject() : nullptr;
    if (auto* Property = Defaults ? FindFProperty<FStructProperty>(Defaults->GetClass(), Name) : nullptr)
    {
        Defaults->Modify();
        *Property->ContainerPtrToValuePtr<FLinearColor>(Defaults) = Value;
    }
}
static void SetDefaultFloat(UWidgetBlueprint& BP, const TCHAR* Name, float Value)
{
    UObject* Defaults = BP.GeneratedClass ? BP.GeneratedClass->GetDefaultObject() : nullptr;
    if (auto* Property = Defaults ? FindFProperty<FFloatProperty>(Defaults->GetClass(), Name) : nullptr)
    {
        Defaults->Modify();
        Property->SetPropertyValue_InContainer(Defaults, Value);
    }
}
static void StyleButton(UButton* Button, UFont* Body, bool Catalog = false)
{
    if (!Button) return;
    FButtonStyle Style = Button->GetStyle();
    Style.Normal = Rounded(Catalog ? Color(130, 213, 185, .055f) : Color(25, 75, 70, .58f), Color(154, 217, 196, .28f));
    Style.Hovered = Rounded(Color(82, 148, 131, .74f), Color(222, 231, 188, .9f));
    Style.Pressed = Rounded(Color(36, 100, 86, .86f), Gold);
    Style.Disabled = Rounded(Color(83, 137, 113, .90f), Color(228, 227, 172, .8f));
    Style.NormalPadding = Style.PressedPadding = Catalog ? FMargin(0) : FMargin(16, 7);
    Button->SetStyle(Style);
    Button->SetCursor(EMouseCursor::Hand);
    if (auto* Text = Cast<UTextBlock>(Button->GetContent()))
    {
        Text->SetFont(Font(Body, 14));
        Text->SetColorAndOpacity(FSlateColor(Cream));
        Text->SetAutoWrapText(false);
    }
}
static bool EditCard(UWidgetBlueprint& BP, UFont* Body, UFont* Medium)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Bounds = Get<USizeBox>(Tree, TEXT("Size_Card"));
    auto* BodyBorder = Get<UBorder>(Tree, TEXT("Border_Card"));
    auto* Selection = Get<UBorder>(Tree, TEXT("Border_Selection"));
    auto* Reward = Get<USizeBox>(Tree, TEXT("Size_RewardState"));
    if (!Bounds || !BodyBorder || !Selection || !Reward || !Get<USizeBox>(Tree, TEXT("Size_Name"))) return false;
    Bounds->SetHeightOverride(236.f);
    BodyBorder->SetPadding(FMargin(12.f));
    Surface(BodyBorder, TEXT("Card"));
    Surface(Selection, TEXT("Selection"));
    Get<USizeBox>(Tree, TEXT("Size_Name"))->SetHeightOverride(48.f);
    Tree.ForEachWidget([Body](UWidget* Widget)
    {
        if (auto* Text = Cast<UTextBlock>(Widget))
        {
            auto Info = Font(Body, Text->GetFont().Size);
            Text->SetFont(Info);
        }
    });
    Label(Tree, TEXT("Text_Name"), Medium, 16, Cream);
    Label(Tree, TEXT("Text_Bonus"), Medium, 16, Gold);
    Label(Tree, TEXT("Text_Recipe"), Body, 12, Muted);
    Label(Tree, TEXT("Text_State"), Body, 11, Mint);
    Label(Tree, TEXT("Text_Plus"), Medium, 21, Mint);
    for (const TCHAR* Side : {TEXT("Target"), TEXT("Partner")})
    {
        const FString SizeName = FString(TEXT("Size_Thumb")) + Side;
        const FString BorderName = FString(TEXT("Border_Thumb")) + Side;
        if (auto* Size = Get<USizeBox>(Tree, *SizeName)) Size->SetHeightOverride(84.f);
        if (auto* Tray = Get<UBorder>(Tree, *BorderName))
        {
            Tray->SetPadding(FMargin(4));
            Tray->SetBrush(Rounded(Color(198, 227, 217, .85f), Color(210, 241, 224, .26f), 8.f, 0.f));
            Tray->SetBrushColor(FLinearColor::White);
        }
    }
    if (auto* ImageRow = Tree.FindWidget(TEXT("Horizontal_RecipeImages")))
        if (auto* Slot = Cast<UVerticalBoxSlot>(ImageRow->Slot)) Slot->SetPadding(FMargin(0, 0, 0, 10));
    if (auto* State = Get<UBorder>(Tree, TEXT("Border_State")))
    {
        State->SetPadding(FMargin(7, 2));
        State->SetBrush(Rounded(Color(30, 99, 80, .62f), Color(145, 217, 180, .25f), 5.f, .6f));
        State->SetBrushColor(FLinearColor::White);
        if (auto* Slot = Cast<UVerticalBoxSlot>(State->Slot)) Slot->SetPadding(FMargin(0, 6, 0, 0));
    }
    // Keep all three reward-state surfaces at the already established 72 x 32 bounds.
    SetDefaultColor(BP, TEXT("UndiscoveredTitleColor"), Color(225, 234, 228));
    SetDefaultColor(BP, TEXT("UndiscoveredBodyColor"), Color(177, 200, 191));
    SetDefaultColor(BP, TEXT("UndiscoveredOutlineColor"), Color(38, 74, 69, .75f));
    return true;
}

static bool EditScreen(UWidgetBlueprint& BP, UFont* Body, UFont* Medium, UFont* Display)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Root = Get<UBorder>(Tree, TEXT("Border_InputShield"));
    auto* Dialog = Get<USizeBox>(Tree, TEXT("Size_Dialog"));
    auto* Paper = Get<UBorder>(Tree, TEXT("Border_Paper"));
    auto* Header = Get<UBorder>(Tree, TEXT("Border_Header"));
    auto* BodyBorder = Get<UBorder>(Tree, TEXT("Border_Body"));
    auto* Heading = Get<UVerticalBox>(Tree, TEXT("Vertical_Heading"));
    auto* Detail = Get<UBorder>(Tree, TEXT("Panel_Detail"));
    auto* DetailBounds = Get<USizeBox>(Tree, TEXT("Size_Detail"));
    auto* Main = Get<UStackBox>(Tree, TEXT("Stack_Main"));
    auto* Tiles = Get<UFF_FacilityCodexTileView>(Tree, TEXT("TileView_Entries"));
    auto* DetailTabs = Get<UHorizontalBox>(Tree, TEXT("Horizontal_DetailTabs"));
    auto* Pages = Get<UWidgetSwitcher>(Tree, TEXT("Switcher_Detail"));
    auto* Ingredients = Get<UHorizontalBox>(Tree, TEXT("Horizontal_Ingredients"));
    auto* Bonus = Get<UBorder>(Tree, TEXT("Border_Bonus"));
    auto* DetailTitle = Get<UTextBlock>(Tree, TEXT("Text_DetailTitle"));
    if (!Root || !Dialog || !Paper || !Header || !BodyBorder || !Heading || !Detail || !DetailBounds || !Main ||
        !Tiles || !DetailTabs || !Pages || Pages->GetChildrenCount() != 3 || !Ingredients || !Bonus || !DetailTitle ||
        !Get<UTextBlock>(Tree, TEXT("Text_Intro")) || Tree.FindWidget(TEXT("BackgroundBlur_Glass"))) return false;

    Tree.ForEachWidget([Body](UWidget* Widget)
    {
        if (auto* Text = Cast<UTextBlock>(Widget))
        {
            Text->SetFont(Font(Body, Text->GetFont().Size));
            Text->SetColorAndOpacity(FSlateColor(Cream));
            Text->SetShadowOffset(FVector2D::ZeroVector);
        }
    });
    Root->SetPadding(FMargin(0));
    Root->SetHorizontalAlignment(HAlign_Fill);
    Root->SetVerticalAlignment(VAlign_Fill);
    Dialog->SetWidthOverride(1920.f);
    Dialog->SetHeightOverride(1080.f);
    Paper->RemoveFromParent();
    auto* Glass = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_GlassScreen"));
    Dialog->AddChild(Glass);
    auto* Blur = MakeWidget<UBackgroundBlur>(BP, Tree, TEXT("BackgroundBlur_Glass"));
    Blur->SetBlurStrength(14.f);
    Blur->SetApplyAlphaToBlur(false);
    Blur->SetLowQualityFallbackBrush(Rounded(Color(9, 39, 39, .66f), FLinearColor::Transparent, 0.f, 0.f));
    Blur->SetVisibility(ESlateVisibility::HitTestInvisible);
    O(Glass, Blur);
    O(Glass, Paper);
    Surface(Paper, TEXT("Screen")); Paper->SetPadding(FMargin(0));
    Surface(Header, TEXT("Header")); Header->SetPadding(FMargin(32, 16));
    BodyBorder->SetPadding(FMargin(32, 18, 32, 24));
    BodyBorder->SetBrush(Rounded(FLinearColor::Transparent, FLinearColor::Transparent, 0, 0));
    BodyBorder->SetBrushColor(FLinearColor::White);
    auto* Intro = Get<UTextBlock>(Tree, TEXT("Text_Intro"));
    Intro->SetText(FText::FromString(TEXT("FESTA FIESTA  /  COLLECTION")));
    Intro->SetVisibility(ESlateVisibility::HitTestInvisible);
    Intro->SetAutoWrapText(false);
    Intro->RemoveFromParent();
    Heading->AddChildToVerticalBox(Intro)->SetPadding(FMargin(0, 0, 0, 3));
    Heading->ShiftChild(0, Intro);
    if (auto* Title = Get<UTextBlock>(Tree, TEXT("Text_Title")))
    {
        Title->SetText(FText::FromString(TEXT("축제 조합 도감")));
        Title->SetAutoWrapText(false);
    }
    Label(Tree, TEXT("Text_Title"), Display, 32, Cream);
    Label(Tree, TEXT("Text_Intro"), Medium, 12, Mint);
    Label(Tree, TEXT("Text_Summary"), Medium, 14, Cream);
    Label(Tree, TEXT("Text_Filter"), Medium, 14, Cream);
    Label(Tree, TEXT("Text_VisualLegend"), Body, 12, Muted);
    Label(Tree, TEXT("Text_Empty"), Medium, 20, Cream);
    if (auto* ProgressWidth = Get<USizeBox>(Tree, TEXT("Size_CollectionProgress"))) ProgressWidth->SetWidthOverride(340.f);
    if (auto* ProgressHeight = Get<USizeBox>(Tree, TEXT("Size_DiscoveryProgress"))) ProgressHeight->SetHeightOverride(8.f);
    if (auto* Progress = Get<UProgressBar>(Tree, TEXT("Progress_Discovery")))
    {
        FProgressBarStyle Style;
        Style.BackgroundImage = Rounded(Color(12, 45, 42, .68f), Color(170, 222, 198, .22f), 4.f, .5f);
        Style.FillImage = Rounded(Gold, Color(251, 241, 203), 4.f, 0.f);
        Progress->SetWidgetStyle(Style);
        Progress->SetFillColorAndOpacity(FLinearColor::White);
    }
    for (const TCHAR* Name : {TEXT("Button_All"), TEXT("Button_Discovered"), TEXT("Button_Undiscovered"),
        TEXT("Button_Active"), TEXT("Button_ClearFacility"), TEXT("Button_Close"),
        TEXT("Button_DetailRecipe"), TEXT("Button_DetailPlacement"), TEXT("Button_DetailEffect")})
        StyleButton(Get<UButton>(Tree, Name), Medium);
    for (const TCHAR* Name : {TEXT("Button_TargetCatalog"), TEXT("Button_PartnerCatalog")})
        StyleButton(Get<UButton>(Tree, Name), Medium, true);
    if (auto* Search = Get<UEditableTextBox>(Tree, TEXT("Search_Entries")))
    {
        FEditableTextBoxStyle Style = Search->GetWidgetStyle();
        Style.TextStyle.Font = Font(Body, 14);
        Style.TextStyle.ColorAndOpacity = FSlateColor(Cream);
        Style.SetBackgroundImageNormal(Rounded(Color(15, 51, 49, .48f), Color(175, 224, 205, .30f)))
            .SetBackgroundImageHovered(Rounded(Color(27, 76, 66, .64f), Mint))
            .SetBackgroundImageFocused(Rounded(Color(22, 64, 57, .78f), Gold))
            .SetForegroundColor(Cream).SetFocusedForegroundColor(Cream).SetPadding(FMargin(16, 7));
        Search->SetWidgetStyle(Style);
    }
    if (auto* Toolbar = Tree.FindWidget(TEXT("Horizontal_Toolbar")))
        if (auto* Slot = Cast<UVerticalBoxSlot>(Toolbar->Slot)) Slot->SetPadding(FMargin(0, 0, 0, 12));
    if (auto* Filter = Tree.FindWidget(TEXT("Horizontal_Filter")))
        if (auto* Slot = Cast<UVerticalBoxSlot>(Filter->Slot)) Slot->SetPadding(FMargin(0, 0, 0, 16));
    if (auto* Footer = Tree.FindWidget(TEXT("Text_Footer"))) Footer->SetVisibility(ESlateVisibility::Collapsed);
    SetDefaultColor(BP, TEXT("SelectedFilterTextColor"), Cream);
    SetDefaultColor(BP, TEXT("InactiveFilterTextColor"), Color(210, 232, 221));
    SetDefaultFloat(BP, TEXT("MinimumCardWidth"), 224.f);
    SetDefaultFloat(BP, TEXT("CompactDetailHeight"), 220.f);
    SetDefaultFloat(BP, TEXT("CompactDetailGap"), 20.f);

    Main->SetOrientation(Orient_Horizontal);
    CastChecked<UStackBoxSlot>(Main->GetChildAt(0)->Slot)->SetPadding(FMargin(0, 0, 24, 0));
    DetailBounds->SetWidthOverride(420.f); DetailBounds->ClearHeightOverride();
    Tiles->ColumnCount = 5;
    Tiles->SetEntryHeight(250.f);
    Tiles->SetHorizontalEntrySpacing(14.f); Tiles->SetVerticalEntrySpacing(14.f);
    Surface(Detail, TEXT("Detail")); Detail->SetPadding(FMargin(20, 16));
    Surface(Bonus, TEXT("Inset")); Bonus->SetPadding(FMargin(16, 14));
    Label(Tree, TEXT("Text_DetailTitle"), Medium, 22, Cream);
    Label(Tree, TEXT("Text_TargetName"), Medium, 14, Cream);
    Label(Tree, TEXT("Text_PartnerName"), Medium, 14, Cream);
    Label(Tree, TEXT("Text_DetailPlus"), Medium, 24, Mint);
    Label(Tree, TEXT("Text_Bonus"), Medium, 21, Gold);
    Label(Tree, TEXT("Text_Distance"), Medium, 15, Cream);
    Label(Tree, TEXT("Text_Condition"), Body, 14, Muted);
    Label(Tree, TEXT("Text_Discovery"), Body, 13, Mint);
    Label(Tree, TEXT("Text_Applied"), Body, 15, Cream);
    for (const TCHAR* Side : {TEXT("Target"), TEXT("Partner")})
    {
        if (auto* Size = Get<USizeBox>(Tree, *(FString(TEXT("Size_Thumb")) + Side)))
        { Size->SetWidthOverride(154.f); Size->SetHeightOverride(90.f); }
        if (auto* Tray = Get<UBorder>(Tree, *(FString(TEXT("Border_Thumb")) + Side)))
        {
            Tray->SetPadding(FMargin(6));
            Tray->SetBrush(Rounded(Color(198, 228, 219, .88f), Color(220, 240, 223, .35f), 10.f, .6f));
            Tray->SetBrushColor(FLinearColor::White);
        }
    }

    // Retain every action and live page, rearranging their authored containers into an inspector.
    UWidget* RetiredColumn = Detail->GetContent();
    UWidget* RetiredRecipe = Pages->GetChildAt(0);
    auto* Column = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Vertical_GlassDetail"));
    auto* HeadingStack = MakeWidget<UStackBox>(BP, Tree, TEXT("Stack_GlassDetailHeading"), true);
    HeadingStack->SetOrientation(Orient_Vertical);
    auto* TitleColumn = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Vertical_GlassDetailTitle"));
    auto* Kicker = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_GlassDetailKicker"));
    Kicker->SetText(FText::FromString(TEXT("함께 만드는 축제")));
    Kicker->SetFont(Font(Body, 12)); Kicker->SetColorAndOpacity(FSlateColor(Mint));
    Kicker->SetVisibility(ESlateVisibility::HitTestInvisible);
    V(TitleColumn, Kicker, FMargin(0, 0, 0, 4));
    V(TitleColumn, DetailTitle);
    S(HeadingStack, TitleColumn, FMargin(0, 0, 0, 18));
    S(HeadingStack, DetailTabs);
    V(Column, HeadingStack, FMargin(0, 0, 0, 18));
    auto* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Scroll_DetailGlass"));
    Scroll->SetAllowOverscroll(false); Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
    V(Column, Scroll, FMargin(0), true);
    Pages->RemoveFromParent(); Scroll->AddChild(Pages);
    auto* Recipe = MakeWidget<UStackBox>(BP, Tree, TEXT("Stack_GlassRecipe"), true);
    Recipe->SetOrientation(Orient_Vertical);
    S(Recipe, Ingredients, FMargin(0, 4, 0, 24));
    S(Recipe, Bonus);
    Pages->RemoveChild(RetiredRecipe);
    Pages->AddChild(Recipe); Pages->ShiftChild(0, Recipe); Pages->SetActiveWidgetIndex(0);
    Detail->ClearChildren(); Detail->AddChild(Column);
    RetiredColumn->SetVisibility(ESlateVisibility::Collapsed); V(Column, RetiredColumn);
    RetiredRecipe->SetVisibility(ESlateVisibility::Collapsed); V(Column, RetiredRecipe);
    return true;
}

static bool Apply(const TCHAR* Path, bool Card, UFont* Body, UFont* Medium, UFont* Display)
{
    auto* BP = LoadExistingBlueprint(Path);
    if (!BP || !BP->WidgetTree || !BP->ParentClass || !BP->ParentClass->IsChildOf(
        Card ? UFF_FacilityCodexCard::StaticClass() : UFF_FacilityCodexScreen::StaticClass())) return false;
    auto& Meta = BP->GetOutermost()->GetMetaData();
    if (Meta.GetValue(BP, Version) == TEXT("1")) return true;
    BP->Modify(); BP->WidgetTree->Modify();
    if (!(Card ? EditCard(*BP, Body, Medium) : EditScreen(*BP, Body, Medium, Display))) return false;
    Meta.SetValue(BP, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, *BP)) return true;
    Meta.RemoveValue(BP, Version);
    return false;
}
static void Run()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: requires an idle Entry editor."), Prefix); return; }
    UFont* Body = FontAsset(TEXT("NEXON_Lv2_Gothic_OTF_Font"));
    UFont* Medium = FontAsset(TEXT("NEXON_Lv2_Gothic_OTF_Medium_Font"));
    UFont* Display = FontAsset(TEXT("NEXON_Kart_Gothic_Kor_OTF_Bold_Font"));
    if (!Body || !Medium || !Display)
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: required project fonts are missing."), Prefix); return; }
    for (const TCHAR* Role : {TEXT("Screen"), TEXT("Header"), TEXT("Card"), TEXT("Selection"), TEXT("Detail"), TEXT("Inset")})
        if (!Material(Role))
        { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: create glass UI materials first (%s)."), Prefix, Role); return; }
    const bool Saved = Apply(CardPath, true, Body, Medium, Display) && Apply(ScreenPath, false, Body, Medium, Display);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s"), Prefix, Saved ? TEXT("PASS") : TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityCodexGlassTheme"),
    TEXT("Apply the full-screen waterfront glass collection, project typography, responsive grid and inspector to the saved codex."),
    FConsoleCommandDelegate::CreateStatic(&Run));

static FSlateBrush ScrollbarBrush(const FSlateBrush& Authored, FLinearColor Fill, FLinearColor Line, float Width = 0.f)
{
    // Preserve the authored brush image size and all scrollbar metrics. This patch changes paint only.
    FSlateBrush Brush = Authored;
    Brush.SetResourceObject(nullptr);
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(Fill);
    Brush.OutlineSettings = FSlateBrushOutlineSettings(5.f, FSlateColor(Line), Width);
    return Brush;
}

static FScrollBarStyle GlassScrollbars(FScrollBarStyle Style)
{
    const FLinearColor Track = Color(10, 45, 43, .42f);
    Style.VerticalBackgroundImage = ScrollbarBrush(Style.VerticalBackgroundImage, Track, FLinearColor::Transparent);
    Style.HorizontalBackgroundImage = ScrollbarBrush(Style.HorizontalBackgroundImage, Track, FLinearColor::Transparent);
    for (FSlateBrush* Segment : {&Style.VerticalTopSlotImage, &Style.VerticalBottomSlotImage,
        &Style.HorizontalTopSlotImage, &Style.HorizontalBottomSlotImage})
        *Segment = ScrollbarBrush(*Segment, FLinearColor::Transparent, FLinearColor::Transparent);
    Style.NormalThumbImage = ScrollbarBrush(Style.NormalThumbImage, Color(144, 201, 177, .80f), Color(228, 221, 172, .48f), .75f);
    Style.HoveredThumbImage = ScrollbarBrush(Style.HoveredThumbImage, Color(179, 225, 201, .94f), Gold, 1.f);
    Style.DraggedThumbImage = ScrollbarBrush(Style.DraggedThumbImage, Gold, Color(248, 242, 205), 1.f);
    return Style;
}

static void RunScrollbars()
{
    constexpr const TCHAR* ScrollbarPrefix = TEXT("FF_CODEX_GLASS_SCROLLBARS");
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: requires an idle Entry editor."), ScrollbarPrefix); return; }
    auto* BP = LoadExistingBlueprint(ScreenPath);
    if (!BP || !BP->WidgetTree || !BP->ParentClass || !BP->ParentClass->IsChildOf(UFF_FacilityCodexScreen::StaticClass()))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: codex screen contract is missing."), ScrollbarPrefix); return; }
    auto& Meta = BP->GetOutermost()->GetMetaData();
    if (Meta.GetValue(BP, ScrollbarVersion) == TEXT("1"))
    { UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_PASS: already applied."), ScrollbarPrefix); return; }
    auto* Tiles = Get<UFF_FacilityCodexTileView>(*BP->WidgetTree, TEXT("TileView_Entries"));
    auto* DetailScroll = Get<UScrollBox>(*BP->WidgetTree, TEXT("Scroll_DetailGlass"));
    auto* Property = Tiles ? FindFProperty<FStructProperty>(Tiles->GetClass(), TEXT("ScrollBarStyle")) : nullptr;
    if (Meta.GetValue(BP, Version) != TEXT("1") || !DetailScroll || !Property || Property->Struct != FScrollBarStyle::StaticStruct())
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: apply glass layout before the scrollbar paint patch."), ScrollbarPrefix); return; }
    BP->Modify(); BP->WidgetTree->Modify(); Tiles->Modify(); DetailScroll->Modify();
    auto* TileStyle = Property->ContainerPtrToValuePtr<FScrollBarStyle>(Tiles);
    *TileStyle = GlassScrollbars(*TileStyle);
    DetailScroll->SetWidgetBarStyle(GlassScrollbars(DetailScroll->GetWidgetBarStyle()));
    Meta.SetValue(BP, ScrollbarVersion, TEXT("1"));
    const bool Saved = SaveNewBlueprint(ScrollbarPrefix, *BP);
    if (!Saved) Meta.RemoveValue(BP, ScrollbarVersion);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s"), ScrollbarPrefix, Saved ? TEXT("PASS") : TEXT("FAIL"));
}

static FAutoConsoleCommand ScrollbarCommand(TEXT("FF.ApplyFacilityCodexGlassScrollbars"),
    TEXT("Tint the saved codex scrollbars in mint and champagne without changing the full-screen layout, geometry or input."),
    FConsoleCommandDelegate::CreateStatic(&RunScrollbars));
}
#endif
