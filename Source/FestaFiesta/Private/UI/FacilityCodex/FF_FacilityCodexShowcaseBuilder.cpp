// Explicit saved-WBP migration: collection browser, large facility showcase, and live detail/reward panel.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
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
#include "UI/FacilityCodex/FF_FacilityCodexImage.h"
#include "UI/FacilityCodex/FF_FacilityCodexPreview.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace FFFacilityCodexShowcaseBuilder
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_CODEX_SHOWCASE");
constexpr const TCHAR* Version = TEXT("FF.FacilityCodex.Showcase20260921.1");
constexpr const TCHAR* PolishVersion = TEXT("FF.FacilityCodex.Showcase20260921.2");
constexpr const TCHAR* RootPath = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/");
template<class T> static T* Get(UWidgetTree& Tree, const TCHAR* Name)
{ return Cast<T>(Tree.FindWidget(FName(Name))); }
template<class T> static T* Require(UWidgetTree& Tree, const TCHAR* Name)
{
    T* Widget = Get<T>(Tree, Name);
    if (!Widget) UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: required source widget %s (%s) is missing or has the wrong class."),
        Prefix, Name, *T::StaticClass()->GetName());
    return Widget;
}
static FLinearColor Color(uint8 R, uint8 G, uint8 B, float A = 1.f)
{ FLinearColor Value = FLinearColor::FromSRGBColor(FColor(R, G, B)); Value.A = A; return Value; }
static const FLinearColor Cream = Color(250, 247, 227), Mint = Color(172, 224, 206),
    Muted = Color(188, 212, 204), Gold = Color(247, 215, 153);

static UFont* LoadFont(const TCHAR* Name)
{ return LoadObject<UFont>(nullptr, *(FString(TEXT("/Game/Asset/Font/")) + Name + TEXT(".") + Name)); }
static FSlateFontInfo Font(UFont* Object, int32 Size)
{
    FSlateFontInfo Value;
    Value.FontObject = Object; Value.Size = Size;
    Value.TypefaceFontName = Object->GetInternalCompositeFont().DefaultTypeface.Fonts.IsEmpty()
        ? NAME_None : Object->GetInternalCompositeFont().DefaultTypeface.Fonts[0].Name;
    return Value;
}
static FSlateBrush Rounded(FLinearColor Fill, FLinearColor Line, float Radius = 10.f)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox; Brush.TintColor = FSlateColor(Fill);
    Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Line), 1.f);
    return Brush;
}
static UMaterialInterface* Material(const TCHAR* Name)
{ return LoadObject<UMaterialInterface>(nullptr, *(FString(RootPath) + TEXT("Materials/") + Name + TEXT(".") + Name)); }
static void Surface(UBorder* Border, UMaterialInterface* Material)
{
    FSlateBrush Brush; Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.SetResourceObject(Material); Brush.ImageSize = FVector2D::ZeroVector; Brush.TintColor = FSlateColor(FLinearColor::White);
    Border->SetBrush(Brush); Border->SetBrushColor(FLinearColor::White);
}
static UVerticalBoxSlot* V(UVerticalBox* Parent, UWidget* Child, FMargin Padding = FMargin(0), bool Fill = false)
{
    Child->RemoveFromParent(); auto* Slot = Parent->AddChildToVerticalBox(Child);
    Slot->SetPadding(Padding); Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    return Slot;
}
static UHorizontalBoxSlot* H(UHorizontalBox* Parent, UWidget* Child, FMargin Padding = FMargin(0), bool Fill = false)
{
    Child->RemoveFromParent(); auto* Slot = Parent->AddChildToHorizontalBox(Child);
    Slot->SetPadding(Padding); Slot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    Slot->SetVerticalAlignment(VAlign_Center); return Slot;
}
static UOverlaySlot* O(UOverlay* Parent, UWidget* Child)
{
    Child->RemoveFromParent(); auto* Slot = Parent->AddChildToOverlay(Child);
    Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill); return Slot;
}
static UTextBlock* Text(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, const TCHAR* Caption,
    UFont* FontObject, int32 Size, FLinearColor Tint, bool Variable = false)
{
    auto* Label = MakeWidget<UTextBlock>(BP, Tree, Name, Variable);
    Label->SetText(FText::FromString(Caption)); Label->SetFont(Font(FontObject, Size));
    Label->SetColorAndOpacity(FSlateColor(Tint)); Label->SetAutoWrapText(true);
    Label->SetVisibility(ESlateVisibility::HitTestInvisible); return Label;
}
static UButton* Button(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, const TCHAR* Caption,
    const FButtonStyle& Style, UFont* Medium)
{
    auto* Result = MakeWidget<UButton>(BP, Tree, Name, true); Result->SetStyle(Style); Result->SetCursor(EMouseCursor::Hand);
    auto* CaptionText = Text(BP, Tree, *(FString(Name) + TEXT("_Label")), Caption, Medium, 13, Cream);
    CaptionText->SetAutoWrapText(false); CaptionText->SetJustification(ETextJustify::Center); Result->AddChild(CaptionText);
    return Result;
}
static USizeBox* Bounds(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UWidget* Child,
    float Width, float Height, bool Variable = false)
{
    auto* Box = MakeWidget<USizeBox>(BP, Tree, Name, Variable);
    if (Width > 0.f) Box->SetWidthOverride(Width); if (Height > 0.f) Box->SetHeightOverride(Height);
    Child->RemoveFromParent(); Box->AddChild(Child); return Box;
}

static bool EditCard(UWidgetBlueprint& BP, UFont* Medium)
{
    auto& Tree = *BP.WidgetTree;
    auto* Card = Require<USizeBox>(Tree, TEXT("Size_Card")); auto* Name = Require<USizeBox>(Tree, TEXT("Size_Name"));
    auto* Reward = Require<USizeBox>(Tree, TEXT("Size_RewardState"));
    auto* Border = Require<UBorder>(Tree, TEXT("Border_Card"));
    auto* TargetSize = Require<USizeBox>(Tree, TEXT("Size_ThumbTarget"));
    auto* PartnerSize = Require<USizeBox>(Tree, TEXT("Size_ThumbPartner"));
    auto* NameText = Require<UTextBlock>(Tree, TEXT("Text_Name"));
    auto* BonusText = Require<UTextBlock>(Tree, TEXT("Text_Bonus"));
    auto* Images = Require<UHorizontalBox>(Tree, TEXT("Horizontal_RecipeImages"));
    auto* State = Require<UBorder>(Tree, TEXT("Border_State"));
    if (!Card || !Name || !Reward || !Border || !TargetSize || !PartnerSize || !NameText || !BonusText || !Images || !State ||
        Reward->GetWidthOverride() != 72.f || Reward->GetHeightOverride() != 32.f) return false;
    Card->SetHeightOverride(202.f); Name->SetHeightOverride(58.f);
    Border->SetPadding(FMargin(10));
    TargetSize->SetHeightOverride(60.f); PartnerSize->SetHeightOverride(60.f);
    NameText->SetFont(Font(Medium, 15)); BonusText->SetFont(Font(Medium, 14));
    if (auto* Slot = Cast<UVerticalBoxSlot>(Images->Slot)) Slot->SetPadding(FMargin(0, 0, 0, 6));
    State->SetPadding(FMargin(5, 1));
    if (auto* Slot = Cast<UVerticalBoxSlot>(State->Slot)) Slot->SetPadding(FMargin(0, 3, 0, 0));
    return true;
}

// Copy the authored reward art, text and brushes without sharing UWidget pointers between WBPs.
static USizeBox* CopyRewards(UWidgetBlueprint& BP, UWidgetTree& Tree, UWidgetTree& Source)
{
    auto* Bounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_DetailRewardState"), true);
    Bounds->SetWidthOverride(72.f); Bounds->SetHeightOverride(32.f);
    auto* Overlay = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_DetailRewardState")); Bounds->AddChild(Overlay);
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const TCHAR* Suffix = Index == 0 ? TEXT("ClaimReward") : Index == 1 ? TEXT("RewardClaimed") : TEXT("RewardLocked");
        const FString SourceName = FString(Index == 0 ? TEXT("Button_") : TEXT("Border_")) + Suffix;
        const FString NewName = FString(Index == 0 ? TEXT("Button_Detail") : TEXT("Border_Detail")) + Suffix;
        auto* Row = MakeWidget<UHorizontalBox>(BP, Tree, *(FString(TEXT("Horizontal_Detail")) + Suffix));
        Row->SetVisibility(ESlateVisibility::HitTestInvisible);
        if (Index == 0)
        {
            auto* Original = Get<UButton>(Source, *SourceName);
            auto* Action = MakeWidget<UButton>(BP, Tree, *NewName, true);
            Action->SetStyle(Original->GetStyle()); Action->SetCursor(EMouseCursor::Hand);
            auto* Slot = CastChecked<UButtonSlot>(Action->AddChild(Row)); Slot->SetPadding(FMargin(6, 4));
            Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center);
            O(Overlay, Action); Action->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            auto* Original = Get<UBorder>(Source, *SourceName);
            auto* Badge = MakeWidget<UBorder>(BP, Tree, *NewName, true);
            Badge->SetBrush(Original->Background); Badge->SetBrushColor(Original->GetBrushColor());
            Badge->SetPadding(Original->GetPadding()); Badge->SetHorizontalAlignment(HAlign_Center); Badge->SetVerticalAlignment(VAlign_Center);
            Badge->AddChild(Row); O(Overlay, Badge); Badge->SetVisibility(ESlateVisibility::Collapsed);
        }
        auto* Icon = MakeWidget<UImage>(BP, Tree, *(FString(TEXT("Image_Detail")) + Suffix));
        auto* OriginalIcon = Get<UImage>(Source, *(FString(TEXT("Image_")) + Suffix));
        Icon->SetBrush(OriginalIcon->GetBrush()); Icon->SetColorAndOpacity(OriginalIcon->GetColorAndOpacity());
        Icon->SetDesiredSizeOverride(FVector2D(24)); Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
        H(Row, Icon);
        auto* OriginalLabel = Get<UTextBlock>(Source, *(FString(TEXT("Text_")) + Suffix));
        auto* Label = MakeWidget<UTextBlock>(BP, Tree, *(FString(TEXT("Text_Detail")) + Suffix));
        Label->SetFont(OriginalLabel->GetFont()); Label->SetText(OriginalLabel->GetText());
        Label->SetColorAndOpacity(OriginalLabel->GetColorAndOpacity()); Label->SetAutoWrapText(false);
        Label->SetShadowColorAndOpacity(OriginalLabel->GetShadowColorAndOpacity()); Label->SetShadowOffset(OriginalLabel->GetShadowOffset());
        Label->SetVisibility(ESlateVisibility::HitTestInvisible); H(Row, Label, FMargin(3, 0, 0, 0));
    }
    return Bounds;
}

static bool EditScreen(UWidgetBlueprint& BP, UWidgetTree& CardTree, UFont* Body, UFont* Medium)
{
    auto& Tree = *BP.WidgetTree;
    // FindWidget traverses only the attached root. Cache every retained widget before any reparenting:
    // the new detail column is temporarily detached while its saved hierarchy is assembled.
    auto* Main = Require<UStackBox>(Tree, TEXT("Stack_Main"));
    auto* Detail = Require<UBorder>(Tree, TEXT("Panel_Detail"));
    auto* DetailSize = Require<USizeBox>(Tree, TEXT("Size_Detail"));
    auto* DetailTitle = Require<UTextBlock>(Tree, TEXT("Text_DetailTitle"));
    auto* DetailScroll = Require<UScrollBox>(Tree, TEXT("Scroll_DetailGlass"));
    auto* Pages = Require<UWidgetSwitcher>(Tree, TEXT("Switcher_Detail"));
    auto* Tabs = Require<UHorizontalBox>(Tree, TEXT("Horizontal_DetailTabs"));
    auto* Ingredients = Require<UHorizontalBox>(Tree, TEXT("Horizontal_Ingredients"));
    auto* Bonus = Require<UBorder>(Tree, TEXT("Border_Bonus"));
    auto* Toolbar = Require<UHorizontalBox>(Tree, TEXT("Horizontal_Toolbar"));
    auto* Filter = Require<UHorizontalBox>(Tree, TEXT("Horizontal_Filter"));
    auto* Tiles = Require<UFF_FacilityCodexTileView>(Tree, TEXT("TileView_Entries"));
    auto* SourceButton = Require<UButton>(Tree, TEXT("Button_All"));
    auto* BonusText = Require<UTextBlock>(Tree, TEXT("Text_Bonus"));
    auto* TargetSize = Require<USizeBox>(Tree, TEXT("Size_ThumbTarget"));
    auto* PartnerSize = Require<USizeBox>(Tree, TEXT("Size_ThumbPartner"));
    auto* TargetName = Require<UTextBlock>(Tree, TEXT("Text_TargetName"));
    auto* PartnerName = Require<UTextBlock>(Tree, TEXT("Text_PartnerName"));
    auto* Plus = Require<UTextBlock>(Tree, TEXT("Text_DetailPlus"));
    if (!Main || Main->GetChildrenCount() != 2 || !Detail || !DetailSize || !DetailTitle || !DetailScroll ||
        !Pages || Pages->GetChildrenCount() != 3 || !Tabs || !Ingredients || !Bonus || !Toolbar || !Filter || !Tiles || !SourceButton ||
        !BonusText || !TargetSize || !PartnerSize || !TargetName || !PartnerName || !Plus || !Detail->GetContent() ||
        !Cast<UStackBoxSlot>(DetailSize->Slot) || Main->GetChildAt(1) != DetailSize ||
        Tree.FindWidget(TEXT("Size_Browse"))) return false;
    UWidget* OldDetailColumn = Detail->GetContent();
    UWidget* OldRecipe = Pages->GetChildAt(0);
    FButtonStyle ButtonStyle = SourceButton->GetStyle();
    ButtonStyle.NormalPadding = ButtonStyle.PressedPadding = FMargin(12, 5);

    // Category belongs to the secondary row, so the additional filters do not crowd the search field.
    auto* Category = MakeWidget<UComboBoxString>(BP, Tree, TEXT("Combo_Category"), true);
    Category->AddOption(TEXT("모든 시설")); Category->SetSelectedOption(TEXT("모든 시설"));
    Category->SetContentPadding(FMargin(12, 5)); Category->SetMaxListHeight(320.f);
    if (auto* Property = FindFProperty<FStructProperty>(Category->GetClass(), TEXT("Font")))
        *Property->ContainerPtrToValuePtr<FSlateFontInfo>(Category) = Font(Medium, 13);
    if (auto* Property = FindFProperty<FStructProperty>(Category->GetClass(), TEXT("ForegroundColor")))
        *Property->ContainerPtrToValuePtr<FSlateColor>(Category) = FSlateColor(Cream);
    FComboBoxStyle ComboStyle = Category->GetWidgetStyle();
    ComboStyle.ComboButtonStyle.ButtonStyle = ButtonStyle;
    ComboStyle.ComboButtonStyle.MenuBorderBrush = Rounded(Color(18, 56, 51, .97f), Color(182, 213, 169, .7f));
    ComboStyle.ComboButtonStyle.DownArrowImage.TintColor = FSlateColor(Mint);
    Category->SetWidgetStyle(ComboStyle);
    FTableRowStyle RowStyle = Category->GetItemStyle();
    RowStyle.TextColor = FSlateColor(Cream); RowStyle.SelectedTextColor = FSlateColor(Cream);
    RowStyle.ActiveBrush = RowStyle.ActiveHoveredBrush = Rounded(Color(54, 115, 94, .97f), Mint);
    RowStyle.InactiveBrush = RowStyle.InactiveHoveredBrush = RowStyle.ActiveBrush;
    RowStyle.EvenRowBackgroundBrush = RowStyle.OddRowBackgroundBrush = Rounded(Color(17, 55, 49, .97f), FLinearColor::Transparent);
    RowStyle.EvenRowBackgroundHoveredBrush = RowStyle.OddRowBackgroundHoveredBrush = Rounded(Color(44, 93, 79, .98f), Mint);
    Category->SetItemStyle(RowStyle);
    auto* CategorySize = Bounds(BP, Tree, TEXT("Size_Category"), Category, 176.f, 36.f);
    H(Filter, CategorySize, FMargin(0, 0, 16, 0)); Filter->ShiftChild(0, CategorySize);
    auto* Remaining = Text(BP, Tree, TEXT("Text_CollectionRemaining"), TEXT("아직 발견하지 않은 조합 0개"), Body, 12, Muted, true);
    H(Filter, Remaining, FMargin(16, 0, 16, 0)); Filter->ShiftChild(FMath::Min(2, Filter->GetChildrenCount() - 1), Remaining);
    auto* Ready = Button(BP, Tree, TEXT("Button_RewardReady"), TEXT("수령 가능"), ButtonStyle, Medium);
    auto* ReadySize = Bounds(BP, Tree, TEXT("Size_RewardReadyFilter"), Ready, 0, 40);
    H(Toolbar, ReadySize, FMargin(0, 0, 14, 0));
    int32 SearchIndex = Toolbar->GetChildrenCount() - 2;
    for (int32 Index = 0; Index < Toolbar->GetChildrenCount(); ++Index)
        if (Toolbar->GetChildAt(Index)->GetName().Contains(TEXT("Search_Entries"))) { SearchIndex = Index; break; }
    Toolbar->ShiftChild(SearchIndex, ReadySize);

    UWidget* Grid = Main->GetChildAt(0); Grid->RemoveFromParent();
    auto* Browse = Bounds(BP, Tree, TEXT("Size_Browse"), Grid, 680.f, 0.f, true);
    auto* BrowseSlot = Main->AddChildToStackBox(Browse); BrowseSlot->SetPadding(FMargin(0, 0, 24, 0));
    BrowseSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); Main->ShiftChild(0, Browse);
    Main->SetOrientation(Orient_Horizontal);
    DetailSize->ClearWidthOverride(); DetailSize->ClearHeightOverride();
    CastChecked<UStackBoxSlot>(DetailSize->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Tiles->ColumnCount = 2; Tiles->SetEntryHeight(216.f);
    if (auto* Defaults = BP.GeneratedClass ? BP.GeneratedClass->GetDefaultObject() : nullptr)
        if (auto* Property = FindFProperty<FFloatProperty>(Defaults->GetClass(), TEXT("MinimumCardWidth")))
        { Defaults->Modify(); Property->SetPropertyValue_InContainer(Defaults, 200.f); }

    auto* Column = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Vertical_ShowcaseDetail"));
    auto* Status = Text(BP, Tree, TEXT("Text_DetailStatus"), TEXT("미발견 조합"), Medium, 12, Mint, true);
    V(Column, Status, FMargin(0, 0, 0, 5));
    DetailTitle->SetFont(Font(Medium, 24)); DetailTitle->SetColorAndOpacity(FSlateColor(Cream));
    V(Column, DetailTitle, FMargin(0, 0, 0, 14));
    V(Column, DetailScroll, FMargin(0), true);
    auto* ScrollColumn = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Vertical_ShowcaseScroll"));
    Pages->RemoveFromParent(); DetailScroll->ClearChildren(); DetailScroll->AddChild(ScrollColumn);

    auto* PreviewHeader = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Horizontal_PreviewHeader"));
    auto* PreviewLabels = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Vertical_PreviewLabels"));
    V(PreviewLabels, Text(BP, Tree, TEXT("Text_PreviewFacilityName"), TEXT("시설 미리보기"), Medium, 20, Cream, true));
    V(PreviewLabels, Text(BP, Tree, TEXT("Text_PreviewCategory"), TEXT("시설"), Body, 12, Muted, true), FMargin(0, 3, 0, 0));
    auto* PreviewCatalog = MakeWidget<UButton>(BP, Tree, TEXT("Button_ShowcaseCatalog"), true);
    FButtonStyle LinkStyle = ButtonStyle;
    LinkStyle.Normal = Rounded(FLinearColor::Transparent, FLinearColor::Transparent);
    LinkStyle.NormalPadding = LinkStyle.PressedPadding = FMargin(8, 4);
    PreviewCatalog->SetStyle(LinkStyle); PreviewCatalog->SetCursor(EMouseCursor::Hand);
    PreviewCatalog->SetToolTipText(FText::FromString(TEXT("건축 카탈로그에서 이 시설 확인")));
    auto* PreviewCatalogSlot = CastChecked<UButtonSlot>(PreviewCatalog->AddChild(PreviewLabels));
    PreviewCatalogSlot->SetHorizontalAlignment(HAlign_Fill); PreviewCatalogSlot->SetVerticalAlignment(VAlign_Center);
    H(PreviewHeader, PreviewCatalog, FMargin(0, 0, 12, 0), true);
    H(PreviewHeader, Bounds(BP, Tree, TEXT("Size_PreviewTargetButton"),
        Button(BP, Tree, TEXT("Button_PreviewTarget"), TEXT("주 시설"), ButtonStyle, Medium), 0, 36), FMargin(0, 0, 6, 0));
    H(PreviewHeader, Bounds(BP, Tree, TEXT("Size_PreviewPartnerButton"),
        Button(BP, Tree, TEXT("Button_PreviewPartner"), TEXT("함께할 시설"), ButtonStyle, Medium), 0, 36));
    V(ScrollColumn, PreviewHeader, FMargin(0, 0, 0, 12));

    auto* Stage = MakeWidget<UBorder>(BP, Tree, TEXT("Border_Showcase"));
    Surface(Stage, Material(TEXT("MI_FF_CodexShowcase_Stage"))); Stage->SetPadding(FMargin(12));
    auto* Hero = Bounds(BP, Tree, TEXT("Size_Showcase"), Stage, 0, 350.f, true);
    V(ScrollColumn, Hero, FMargin(0, 0, 0, 10));
    auto* Overlay = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_Showcase")); Stage->AddChild(Overlay);
    auto* Fallback = Text(BP, Tree, TEXT("Text_ShowcaseFallback"), TEXT("시설 미리보기"), Medium, 22, Muted, true);
    auto* FallbackSlot = O(Overlay, Fallback); FallbackSlot->SetHorizontalAlignment(HAlign_Center); FallbackSlot->SetVerticalAlignment(VAlign_Center);
    auto* Image = MakeWidget<UFF_FacilityCodexImage>(BP, Tree, TEXT("Image_Showcase"), true);
    Image->DiscoveryMaterial = Material(TEXT("M_FF_Codex_DiscoveryThumbnail")); Image->SetVisibility(ESlateVisibility::Hidden);
    auto* ImageScale = MakeWidget<UScaleBox>(BP, Tree, TEXT("Scale_Showcase")); ImageScale->SetStretch(EStretch::ScaleToFit);
    ImageScale->AddChild(Image); ImageScale->SetVisibility(ESlateVisibility::HitTestInvisible);
    O(Overlay, ImageScale)->SetPadding(FMargin(28, 16));
    auto* Viewport = MakeWidget<UFF_FacilityCodexPreview>(BP, Tree, TEXT("Viewport_Showcase"), true);
    O(Overlay, Viewport); Viewport->SetVisibility(ESlateVisibility::Collapsed);

    auto* Rotation = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Horizontal_PreviewRotation"));
    H(Rotation, Text(BP, Tree, TEXT("Text_PreviewHint"), TEXT("화살표로 시설을 둘러보세요"), Body, 12, Muted, true), FMargin(0, 0, 12, 0), true);
    H(Rotation, Bounds(BP, Tree, TEXT("Size_RotatePreviewLeft"), Button(BP, Tree, TEXT("Button_RotatePreviewLeft"), TEXT("<"), ButtonStyle, Medium), 38, 32), FMargin(0, 0, 5, 0));
    H(Rotation, Bounds(BP, Tree, TEXT("Size_ResetPreview"), Button(BP, Tree, TEXT("Button_ResetPreview"), TEXT("정면"), ButtonStyle, Medium), 64, 32), FMargin(0, 0, 5, 0));
    H(Rotation, Bounds(BP, Tree, TEXT("Size_RotatePreviewRight"), Button(BP, Tree, TEXT("Button_RotatePreviewRight"), TEXT(">"), ButtonStyle, Medium), 38, 32));
    V(ScrollColumn, Rotation, FMargin(0, 0, 0, 16));
    V(ScrollColumn, Tabs, FMargin(0, 0, 0, 14));

    // Existing tab pages stay live. The reward summary beside them is visible on every tab.
    auto* Footer = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Horizontal_ShowcaseFooter"));
    H(Footer, Pages, FMargin(0, 0, 18, 0), true)->SetVerticalAlignment(VAlign_Top);
    auto* Recipe = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Vertical_ShowcaseRecipe"));
    V(Recipe, Ingredients, FMargin(0, 0, 0, 12))->SetHorizontalAlignment(HAlign_Left);
    V(Recipe, Bonus);
    Pages->RemoveChild(OldRecipe); Pages->AddChild(Recipe); Pages->ShiftChild(0, Recipe); Pages->SetActiveWidgetIndex(0);
    Bonus->SetPadding(FMargin(12, 10));
    BonusText->SetFont(Font(Medium, 18));
    for (USizeBox* Size : {TargetSize, PartnerSize})
    { Size->SetWidthOverride(104.f); Size->SetHeightOverride(64.f); }
    TargetName->SetFont(Font(Medium, 13)); PartnerName->SetFont(Font(Medium, 13));
    Plus->SetFont(Font(Medium, 20));
    if (auto* Slot = Cast<UHorizontalBoxSlot>(Plus->Slot)) Slot->SetPadding(FMargin(6, 0));
    auto* RewardPanel = MakeWidget<UBorder>(BP, Tree, TEXT("Border_ShowcaseRewardInfo"));
    Surface(RewardPanel, Material(TEXT("MI_FF_CodexGlass_Inset"))); RewardPanel->SetPadding(FMargin(16, 12));
    auto* RewardColumn = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Vertical_ShowcaseRewardInfo")); RewardPanel->AddChild(RewardColumn);
    V(RewardColumn, Text(BP, Tree, TEXT("Text_ShowcaseDistance"), TEXT("시설을 가까이 배치해 보세요"), Body, 13, Muted, true), FMargin(0, 0, 0, 14));
    V(RewardColumn, Text(BP, Tree, TEXT("Text_DetailRewardLabel"), TEXT("첫 발견 보상"), Medium, 12, Mint), FMargin(0, 0, 0, 4));
    V(RewardColumn, Text(BP, Tree, TEXT("Text_DetailRewardAmount"), TEXT("0 골드"), Medium, 20, Gold, true), FMargin(0, 0, 0, 10));
    V(RewardColumn, CopyRewards(BP, Tree, CardTree))->SetHorizontalAlignment(HAlign_Left);
    H(Footer, Bounds(BP, Tree, TEXT("Size_ShowcaseRewardInfo"), RewardPanel, 280.f, 0, true))->SetVerticalAlignment(VAlign_Top);
    V(ScrollColumn, Footer);
    Detail->ClearChildren(); Detail->AddChild(Column);
    // Keep designer history attached and collapsed, without orphaning variable GUIDs or old optional bindings.
    OldDetailColumn->SetVisibility(ESlateVisibility::Collapsed); V(Column, OldDetailColumn);
    OldRecipe->SetVisibility(ESlateVisibility::Collapsed); V(Column, OldRecipe);
    return true;
}

static bool SourceContract(UWidgetTree& Tree)
{
    for (const TCHAR* Suffix : {TEXT("ClaimReward"), TEXT("RewardClaimed"), TEXT("RewardLocked")})
        if (!Get<UImage>(Tree, *(FString(TEXT("Image_")) + Suffix)) ||
            !Get<UTextBlock>(Tree, *(FString(TEXT("Text_")) + Suffix))) return false;
    return Get<UButton>(Tree, TEXT("Button_ClaimReward")) && Get<UBorder>(Tree, TEXT("Border_RewardClaimed")) &&
        Get<UBorder>(Tree, TEXT("Border_RewardLocked")) && Get<USizeBox>(Tree, TEXT("Size_RewardState"));
}
static bool PolishCard(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    auto* Card = Require<USizeBox>(Tree, TEXT("Size_Card"));
    auto* Name = Require<USizeBox>(Tree, TEXT("Size_Name"));
    if (!Card || !Name) return false;
    Card->SetHeightOverride(202.f); Name->SetHeightOverride(58.f);
    return true;
}
static bool PolishScreen(UWidgetBlueprint& BP, UFont* Medium)
{
    auto& Tree = *BP.WidgetTree;
    auto* Catalog = Require<UButton>(Tree, TEXT("Button_ShowcaseCatalog"));
    auto* FacilityName = Require<UTextBlock>(Tree, TEXT("Text_PreviewFacilityName"));
    auto* Category = Require<UTextBlock>(Tree, TEXT("Text_PreviewCategory"));
    auto* Stage = Require<USizeBox>(Tree, TEXT("Size_Showcase"));
    auto* Viewport = Require<UFF_FacilityCodexPreview>(Tree, TEXT("Viewport_Showcase"));
    auto* Tiles = Require<UFF_FacilityCodexTileView>(Tree, TEXT("TileView_Entries"));
    auto* CatalogSlot = Catalog && Catalog->GetContent() ? Cast<UButtonSlot>(Catalog->GetContent()->Slot) : nullptr;
    const TCHAR* Names[] = {TEXT("Text_DetailStatus"), TEXT("Text_DetailTitle"), TEXT("Horizontal_PreviewHeader"),
        TEXT("Size_Showcase"), TEXT("Horizontal_PreviewRotation"), TEXT("Horizontal_DetailTabs"), TEXT("Horizontal_Ingredients")};
    const FMargin Padding[] = {FMargin(0,0,0,2), FMargin(0,0,0,8), FMargin(0,0,0,8), FMargin(0,0,0,8),
        FMargin(0,0,0,8), FMargin(0,0,0,8), FMargin(0,0,0,8)};
    TArray<UVerticalBoxSlot*> Slots;
    for (const TCHAR* Name : Names)
    {
        auto* Widget = Require<UWidget>(Tree, Name);
        auto* Slot = Widget ? Cast<UVerticalBoxSlot>(Widget->Slot) : nullptr;
        if (!Slot)
        { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: polish requires the existing vertical slot for %s."), Prefix, Name); return false; }
        Slots.Add(Slot);
    }
    if (!Catalog || !FacilityName || !Category || !Stage || !Viewport || !Tiles || !CatalogSlot) return false;
    // A fill-aligned content slot gives the facility name its actual header width, instead of a
    // centered shrink-to-fit label that caches a narrow wrap before the final viewport layout.
    CatalogSlot->SetHorizontalAlignment(HAlign_Fill); CatalogSlot->SetVerticalAlignment(VAlign_Center);
    FButtonStyle Style = Catalog->GetStyle();
    Style.NormalPadding = Style.PressedPadding = FMargin(8, 2); Catalog->SetStyle(Style);
    FacilityName->SetFont(Font(Medium, 18)); FacilityName->SetAutoWrapText(false);
    FacilityName->SetJustification(ETextJustify::Left); FacilityName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    Category->SetAutoWrapText(false); Category->SetJustification(ETextJustify::Left);
    if (auto* Remaining = Get<UTextBlock>(Tree, TEXT("Text_CollectionRemaining"))) Remaining->SetAutoWrapText(false);
    for (int32 Index = 0; Index < Slots.Num(); ++Index) Slots[Index]->SetPadding(Padding[Index]);
    Stage->SetHeightOverride(310.f);
    Viewport->SetBackgroundColor(FLinearColor(.002f, .006f, .008f, 1.f));
    Tiles->SetEntryHeight(216.f);
    return true;
}
static bool SaveVersion(UWidgetBlueprint& BP, const TCHAR* Key = Version)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData(); Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Key); return false;
}
static void Run()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: requires an idle Entry editor."), Prefix); return; }
    auto* Card = LoadExistingBlueprint(FString(RootPath) + TEXT("WBP_FF_FacilityCodexCard"));
    auto* Screen = LoadExistingBlueprint(FString(RootPath) + TEXT("WBP_FF_FacilityCodexScreen"));
    UFont* Body = LoadFont(TEXT("NEXON_Lv2_Gothic_OTF_Font"));
    UFont* Medium = LoadFont(TEXT("NEXON_Lv2_Gothic_OTF_Medium_Font"));
    if (!Card || !Screen || !Card->WidgetTree || !Screen->WidgetTree || !Card->ParentClass || !Screen->ParentClass || !Body || !Medium ||
        !Card->ParentClass->IsChildOf(UFF_FacilityCodexCard::StaticClass()) || !Screen->ParentClass->IsChildOf(UFF_FacilityCodexScreen::StaticClass()) ||
        !SourceContract(*Card->WidgetTree) || !Material(TEXT("MI_FF_CodexShowcase_Stage")) ||
        !Material(TEXT("M_FF_Codex_DiscoveryThumbnail")) || !Material(TEXT("MI_FF_CodexGlass_Inset")))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: saved widget, project font, reward or showcase material contract is missing."), Prefix); return; }
    bool Saved = true;
    if (Card->GetOutermost()->GetMetaData().GetValue(Card, Version) != TEXT("1"))
    { Card->Modify(); Card->WidgetTree->Modify(); Saved = EditCard(*Card, Medium) && SaveVersion(*Card); }
    if (Saved && Screen->GetOutermost()->GetMetaData().GetValue(Screen, Version) != TEXT("1"))
    { Screen->Modify(); Screen->WidgetTree->Modify(); Saved = EditScreen(*Screen, *Card->WidgetTree, Body, Medium) && SaveVersion(*Screen); }
    // Existing v1 screens take only this bounded visual patch; never recreate their controls or layout.
    if (Saved && Card->GetOutermost()->GetMetaData().GetValue(Card, PolishVersion) != TEXT("1"))
    { Card->Modify(); Card->WidgetTree->Modify(); Saved = PolishCard(*Card) && SaveVersion(*Card, PolishVersion); }
    if (Saved && Screen->GetOutermost()->GetMetaData().GetValue(Screen, PolishVersion) != TEXT("1"))
    { Screen->Modify(); Screen->WidgetTree->Modify(); Saved = PolishScreen(*Screen, Medium) && SaveVersion(*Screen, PolishVersion); }
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s"), Prefix, Saved ? TEXT("PASS") : TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityCodexShowcase"),
    TEXT("Apply the reference-inspired collection browser, large facility preview, category filter and detail rewards to the existing saved codex."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
