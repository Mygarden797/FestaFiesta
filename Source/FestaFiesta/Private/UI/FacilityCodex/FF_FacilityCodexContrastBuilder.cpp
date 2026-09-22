// FF_FacilityCodexContrastBuilder.cpp
// 역할: 도감의 대비와 글자 가독성을 조정한다.

// A bounded saved-WBP style update; discovery, economy and layout remain data-driven.
#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Viewport.h"
#include "Editor.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace FFFacilityCodexContrast
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Version = TEXT("FF.FacilityCodex.White20260921.3");
constexpr const TCHAR* Prefix = TEXT("FF_CODEX_CONTRAST");
constexpr const TCHAR* Root = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/");
template<class T> T* Get(UWidgetTree& Tree, const TCHAR* Name) { return Cast<T>(Tree.FindWidget(Name)); }
static FLinearColor Color(uint8 R, uint8 G, uint8 B, uint8 A = 255)
{ return FLinearColor::FromSRGBColor(FColor(R, G, B, A)); }
static FSlateBrush Rounded(FLinearColor Fill, FLinearColor Line, float Width = 1.f, float Radius = 9.f)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = Fill;
    Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Line), Width);
    return Brush;
}
static void Text(UWidgetTree& Tree, const TCHAR* Name, FLinearColor Tint)
{ if (auto* Label = Get<UTextBlock>(Tree, Name)) Label->SetColorAndOpacity(FSlateColor(Tint)); }
static void DefaultColor(UWidgetBlueprint& BP, const TCHAR* Name, FLinearColor Tint)
{
    UObject* CDO = BP.GeneratedClass ? BP.GeneratedClass->GetDefaultObject() : nullptr;
    if (auto* Property = CDO ? FindFProperty<FStructProperty>(CDO->GetClass(), Name) : nullptr)
    { CDO->Modify(); *Property->ContainerPtrToValuePtr<FLinearColor>(CDO) = Tint; }
}
static const FLinearColor Forest = Color(32, 80, 57), Cream = Color(245, 255, 232),
    Mint = Color(166, 231, 187), Quiet = Color(188, 210, 195), Gold = Color(247, 215, 153);

static FButtonStyle NavigationStyle(const FButtonStyle& Source)
{
    const auto& Palette = UFF_UIPalette::Get()->InGame;
    FButtonStyle Style = Source;
    Style.Normal = Rounded(Palette.PaperSunken, Palette.LineStrong, 1.4f);
    Style.Hovered = Rounded(Cream, Palette.Teal, 2.f);
    Style.Pressed = Rounded(Palette.TealSoft, Palette.TealDeep, 2.f);
    // The screen maps this authored brush to its selected, still-enabled state.
    Style.Disabled = Rounded(Forest, Mint, 2.4f);
    return Style;
}
static void NavigationButton(UButton* Button)
{
    Button->SetStyle(NavigationStyle(Button->GetStyle()));
    if (auto* Label = Cast<UTextBlock>(Button->GetContent()))
        Label->SetColorAndOpacity(FSlateColor(UFF_UIPalette::Get()->InGame.Ink));
}
static void QuietReward(UWidgetTree& Tree, bool Detail)
{
    const TCHAR* BorderName = Detail ? TEXT("Border_DetailRewardLocked") : TEXT("Border_RewardLocked");
    auto* Locked = Get<UBorder>(Tree, BorderName);
    Locked->SetBrush(Rounded(Color(40, 65, 56), Color(104, 132, 114), 1.f, 7.5f));
    Locked->SetBrushColor(FLinearColor::White);
    Text(Tree, Detail ? TEXT("Text_DetailRewardLocked") : TEXT("Text_RewardLocked"), Quiet);
    // Preserve the gift, lock caption and exact hit/visual bounds. Only the idle surface recedes.
    Text(Tree, Detail ? TEXT("Text_DetailClaimReward") : TEXT("Text_ClaimReward"), Cream);
}
static bool Contract(UWidgetBlueprint& BP, bool Screen)
{
    if (!BP.WidgetTree || !BP.GeneratedClass ||
        BP.GetOutermost()->GetMetaData().GetValue(&BP, TEXT("FF.FacilityCodex.Showcase20260921.2")) != TEXT("1")) return false;
    auto& Tree = *BP.WidgetTree;
    if (Screen)
    {
        for (const TCHAR* Name : {TEXT("Button_All"), TEXT("Button_Discovered"), TEXT("Button_Undiscovered"),
            TEXT("Button_Active"), TEXT("Button_RewardReady"), TEXT("Button_ClearFacility"), TEXT("Button_Close"),
            TEXT("Button_DetailRecipe"), TEXT("Button_DetailPlacement"), TEXT("Button_DetailEffect"),
            TEXT("Button_PreviewTarget"), TEXT("Button_PreviewPartner"), TEXT("Button_RotatePreviewLeft"),
            TEXT("Button_RotatePreviewRight"), TEXT("Button_ResetPreview")})
            if (!Get<UButton>(Tree, Name)) return false;
        return Get<UComboBoxString>(Tree, TEXT("Combo_Category")) &&
            Get<UEditableTextBox>(Tree, TEXT("Search_Entries")) &&
            Get<UBorder>(Tree, TEXT("Border_DetailRewardLocked"));
    }
    return Get<UBorder>(Tree, TEXT("Border_RewardLocked")) && Get<UBorder>(Tree, TEXT("Border_State")) &&
        Get<UBorder>(Tree, TEXT("Border_ThumbTarget")) && Get<UBorder>(Tree, TEXT("Border_ThumbPartner"));
}
static void ScreenStyle(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    const auto& Palette = UFF_UIPalette::Get()->InGame;
    for (const TCHAR* Name : {TEXT("Button_All"), TEXT("Button_Discovered"), TEXT("Button_Undiscovered"),
        TEXT("Button_Active"), TEXT("Button_RewardReady"), TEXT("Button_ClearFacility"), TEXT("Button_Close"),
        TEXT("Button_DetailRecipe"), TEXT("Button_DetailPlacement"), TEXT("Button_DetailEffect"),
        TEXT("Button_PreviewTarget"), TEXT("Button_PreviewPartner"), TEXT("Button_RotatePreviewLeft"),
        TEXT("Button_RotatePreviewRight"), TEXT("Button_ResetPreview")})
        NavigationButton(Get<UButton>(Tree, Name));
    // Rotation and close are actions, not selection tabs. Their disabled state
    // must stay quiet and readable instead of borrowing the selected-tab ring.
    for (const TCHAR* Name : {TEXT("Button_Close"), TEXT("Button_ClearFacility"),
        TEXT("Button_RotatePreviewLeft"), TEXT("Button_RotatePreviewRight"), TEXT("Button_ResetPreview")})
    {
        auto* Button = Get<UButton>(Tree, Name);
        auto ActionStyle = Button->GetStyle();
        ActionStyle.Disabled = Rounded(Palette.TealSoft, Palette.LineStrong, 1.f);
        Button->SetStyle(ActionStyle);
    }
    DefaultColor(BP, TEXT("SelectedFilterTextColor"), Cream);
    DefaultColor(BP, TEXT("InactiveFilterTextColor"), Palette.Ink);

    auto* Combo = Get<UComboBoxString>(Tree, TEXT("Combo_Category"));
    auto Style = Combo->GetWidgetStyle();
    Style.ComboButtonStyle.ButtonStyle = NavigationStyle(Style.ComboButtonStyle.ButtonStyle);
    Style.ComboButtonStyle.ButtonStyle.Normal = Rounded(Palette.Paper, Palette.Teal, 2.f);
    Style.ComboButtonStyle.MenuBorderBrush = Rounded(Palette.Paper, Palette.TealDeep, 2.f);
    Style.ComboButtonStyle.DownArrowImage.TintColor = Palette.TealDeep;
    Combo->SetWidgetStyle(Style);
    auto Row = Combo->GetItemStyle();
    Row.TextColor = Palette.Ink; Row.SelectedTextColor = Cream;
    Row.ActiveBrush = Row.ActiveHoveredBrush = Row.InactiveBrush = Row.InactiveHoveredBrush = Rounded(Forest, Mint, 2.f);
    Row.EvenRowBackgroundBrush = Row.OddRowBackgroundBrush = Rounded(Palette.Paper, FLinearColor::Transparent, 0.f);
    Row.EvenRowBackgroundHoveredBrush = Row.OddRowBackgroundHoveredBrush = Rounded(Palette.TealSoft, Palette.Teal, 1.f);
    Combo->SetItemStyle(Row);
    // These ComboBox properties are construction-only, so author them before Slate is built.
    if (auto* Property = FindFProperty<FStructProperty>(Combo->GetClass(), TEXT("ForegroundColor")))
        *Property->ContainerPtrToValuePtr<FSlateColor>(Combo) = FSlateColor(Palette.Ink);
    if (auto* Property = FindFProperty<FStructProperty>(Combo->GetClass(), TEXT("Font")))
        Property->ContainerPtrToValuePtr<FSlateFontInfo>(Combo)->Size = 14;

    auto* Search = Get<UEditableTextBox>(Tree, TEXT("Search_Entries"));
    auto SearchStyle = Search->GetWidgetStyle();
    SearchStyle.SetBackgroundImageNormal(Rounded(Color(20, 47, 39), Palette.Teal, 1.3f))
        .SetBackgroundImageHovered(Rounded(Color(26, 60, 47), Mint, 1.5f))
        .SetBackgroundImageFocused(Rounded(Color(26, 60, 47), Gold, 2.f))
        .SetForegroundColor(Cream).SetFocusedForegroundColor(Cream);
    SearchStyle.TextStyle.SetColorAndOpacity(Cream);
    Search->SetWidgetStyle(SearchStyle);
    Text(Tree, TEXT("Text_Title"), Cream);
    Text(Tree, TEXT("Text_DetailTitle"), Cream);
    Text(Tree, TEXT("Text_PreviewFacilityName"), Cream);
    Text(Tree, TEXT("Text_Filter"), Cream);
    Text(Tree, TEXT("Text_Summary"), Cream);
    Text(Tree, TEXT("Text_PreviewCategory"), Mint);
    Text(Tree, TEXT("Text_DetailStatus"), Mint);
    Text(Tree, TEXT("Text_CollectionRemaining"), Quiet);
    Text(Tree, TEXT("Text_VisualLegend"), Quiet);
    Text(Tree, TEXT("Text_DetailRewardAmount"), Gold);
    Text(Tree, TEXT("Text_Bonus"), Cream);
    QuietReward(Tree, true);
}
static void CardStyle(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    const auto& Palette = UFF_UIPalette::Get()->InGame;
    for (const TCHAR* Name : {TEXT("Border_ThumbTarget"), TEXT("Border_ThumbPartner")})
    {
        auto* Tray = Get<UBorder>(Tree, Name);
        Tray->SetBrush(Rounded(Color(214, 226, 216), Palette.LineStrong, .8f, 7.f));
        Tray->SetBrushColor(FLinearColor::White);
    }
    auto* State = Get<UBorder>(Tree, TEXT("Border_State"));
    State->SetBrush(Rounded(Forest, Mint, .8f, 5.f));
    State->SetBrushColor(FLinearColor::White);
    DefaultColor(BP, TEXT("UndiscoveredTitleColor"), Color(226, 238, 229));
    DefaultColor(BP, TEXT("UndiscoveredBodyColor"), Quiet);
    DefaultColor(BP, TEXT("UndiscoveredOutlineColor"), Color(40, 65, 56));
    Text(Tree, TEXT("Text_Name"), Cream);
    Text(Tree, TEXT("Text_State"), Cream);
    Text(Tree, TEXT("Text_Bonus"), Cream);
    QuietReward(Tree, false);
}
static void PearlStyle(UWidgetBlueprint& BP, bool Screen)
{
    auto& Tree = *BP.WidgetTree;
    const auto& Palette = UFF_UIPalette::Get()->InGame;
    // The glass shader supplies paper-white surfaces. Forest belongs to active
    // controls and headings; all broad reading surfaces use dark ink.
    const FLinearColor BodyInk = Color(78, 94, 85);
    auto PaperText = [&](const TCHAR* Name, FLinearColor Tint)
    {
        Text(Tree, Name, Tint);
        if (auto* Label = Get<UTextBlock>(Tree, Name)) Label->SetShadowOffset(FVector2D::ZeroVector);
    };
    for (const TCHAR* Name : {TEXT("Border_ThumbTarget"), TEXT("Border_ThumbPartner")})
        if (auto* Tray = Get<UBorder>(Tree, Name))
        {
            Tray->SetBrush(Rounded(Palette.Paper, Palette.LineStrong, .8f, 7.f));
            Tray->SetBrushColor(FLinearColor::White);
        }
    auto* Locked = Get<UBorder>(Tree, Screen ? TEXT("Border_DetailRewardLocked") : TEXT("Border_RewardLocked"));
    Locked->SetBrush(Rounded(Palette.PaperSunken, Palette.LineStrong, 1.f, 7.5f));
    Locked->SetBrushColor(FLinearColor::White);
    PaperText(Screen ? TEXT("Text_DetailRewardLocked") : TEXT("Text_RewardLocked"), BodyInk);
    if (Screen)
    {
        for (const TCHAR* Name : {TEXT("Text_Summary"), TEXT("Text_Filter"), TEXT("Text_Empty")})
            PaperText(Name, Palette.Ink);
        PaperText(TEXT("Text_Title"), Palette.TealDeep);
        PaperText(TEXT("Text_Intro"), Palette.TealDeep);
        for (const TCHAR* Name : {TEXT("Text_CollectionRemaining"), TEXT("Text_VisualLegend"),
            TEXT("Text_ShowcaseFallback")}) PaperText(Name, BodyInk);
        auto* Search = Get<UEditableTextBox>(Tree, TEXT("Search_Entries"));
        auto SearchStyle = Search->GetWidgetStyle();
        SearchStyle.SetBackgroundImageNormal(Rounded(Palette.Paper, Palette.LineStrong, 1.3f))
            .SetBackgroundImageHovered(Rounded(Palette.White, Palette.Teal, 1.5f))
            .SetBackgroundImageFocused(Rounded(Palette.White, Palette.TealDeep, 2.f))
            .SetForegroundColor(Palette.Ink).SetFocusedForegroundColor(Palette.Ink);
        SearchStyle.TextStyle.SetColorAndOpacity(Palette.Ink);
        Search->SetWidgetStyle(SearchStyle);
        // Slate forces native hints to 35% alpha. A passive authored hint keeps
        // the search affordance readable on white without changing input text.
        if (!Tree.FindWidget(TEXT("Overlay_SearchHint")))
        {
            auto* Parent = Search->GetParent();
            auto* Overlay = MakeWidget<UOverlay>(BP, Tree, TEXT("Overlay_SearchHint"));
            Parent->ReplaceChild(Search, Overlay);
            Search->Slot = nullptr; // ReplaceChild transfers the original slot to the overlay.
            Overlay->AddChildToOverlay(Search);
            auto* Hint = MakeWidget<UTextBlock>(BP, Tree, TEXT("Text_SearchHint"));
            Hint->SetText(Search->GetHintText());
            Hint->SetFont(SearchStyle.TextStyle.Font);
            Hint->SetColorAndOpacity(FSlateColor(BodyInk));
            Hint->SetVisibility(ESlateVisibility::HitTestInvisible);
            auto* HintSlot = Overlay->AddChildToOverlay(Hint);
            HintSlot->SetPadding(SearchStyle.Padding);
            HintSlot->SetVerticalAlignment(VAlign_Center);
            Search->SetHintText(FText::GetEmpty());
        }
        if (auto* SearchSlot = Cast<UOverlaySlot>(Search->Slot))
        {
            SearchSlot->SetHorizontalAlignment(HAlign_Fill);
            SearchSlot->SetVerticalAlignment(VAlign_Fill);
        }
        if (auto* Blur = Get<UBackgroundBlur>(Tree, TEXT("BackgroundBlur_Glass")))
            Blur->SetLowQualityFallbackBrush(Rounded(Color(247, 248, 246, 245), Palette.Line, 0.f));
        if (auto* Preview = Get<UViewport>(Tree, TEXT("Viewport_Showcase")))
            Preview->SetBackgroundColor(Color(238, 242, 239));
        if (auto* Progress = Get<UProgressBar>(Tree, TEXT("Progress_Discovery")))
        {
            auto Style = Progress->GetWidgetStyle();
            Style.BackgroundImage = Rounded(Palette.PaperSunken, Palette.LineStrong, .8f, 4.f);
            Style.FillImage = Rounded(Palette.Teal, Palette.TealDeep, .5f, 4.f);
            Progress->SetWidgetStyle(Style);
            Progress->SetFillColorAndOpacity(FLinearColor::White);
        }
        for (const TCHAR* Name : {TEXT("Text_DetailTitle"), TEXT("Text_PreviewFacilityName"),
            TEXT("Text_TargetName"), TEXT("Text_PartnerName"), TEXT("Text_Bonus"),
            TEXT("Text_Distance"), TEXT("Text_Applied")}) PaperText(Name, Palette.Ink);
        for (const TCHAR* Name : {TEXT("Text_PreviewCategory"), TEXT("Text_PreviewHint"),
            TEXT("Text_Condition"), TEXT("Text_ShowcaseDistance"), TEXT("Text_TargetFallback"),
            TEXT("Text_PartnerFallback")}) PaperText(Name, BodyInk);
        for (const TCHAR* Name : {TEXT("Text_DetailStatus"), TEXT("Text_DetailPlus"),
            TEXT("Text_DetailRewardLabel"), TEXT("Text_Discovery")}) PaperText(Name, Palette.TealDeep);
        // Bronze keeps the reward distinct without losing contrast on ivory.
        PaperText(TEXT("Text_DetailRewardAmount"), Color(130, 80, 15));
        for (const TCHAR* Name : {TEXT("Button_TargetCatalog"), TEXT("Button_PartnerCatalog"),
            TEXT("Button_ShowcaseCatalog")})
            if (auto* Button = Get<UButton>(Tree, Name))
            {
                auto Style = Button->GetStyle();
                Style.Normal = Rounded(FLinearColor::Transparent, Palette.LineStrong, .8f);
                Style.Hovered = Rounded(Palette.White, Palette.TealDeep, 1.5f);
                Style.Pressed = Rounded(Palette.Hover, Palette.TealDeep, 2.f);
                Style.Disabled = Rounded(Palette.PaperSunken, Palette.Line, .8f);
                // The large heading remains a quiet link until hovered.
                if (FCString::Strcmp(Name, TEXT("Button_ShowcaseCatalog")) == 0)
                    Style.Normal = Rounded(FLinearColor::Transparent, FLinearColor::Transparent, 0.f);
                Button->SetStyle(Style);
            }
    }
    else
    {
        DefaultColor(BP, TEXT("UndiscoveredTitleColor"), Palette.Ink);
        DefaultColor(BP, TEXT("UndiscoveredBodyColor"), BodyInk);
        DefaultColor(BP, TEXT("UndiscoveredOutlineColor"), Palette.PaperSunken);
        PaperText(TEXT("Text_Name"), Palette.Ink);
        PaperText(TEXT("Text_Bonus"), Palette.TealDeep);
        PaperText(TEXT("Text_Recipe"), BodyInk);
        PaperText(TEXT("Text_Plus"), Palette.TealDeep);
        PaperText(TEXT("Text_TargetFallback"), BodyInk);
        PaperText(TEXT("Text_PartnerFallback"), BodyInk);
        // Discovered badges remain green; undiscovered badges use the authored
        // paper override with dark text. Discovery never dims the whole card.
    }
}
static bool Save(UWidgetBlueprint& BP, bool Screen)
{
    auto& Meta = BP.GetOutermost()->GetMetaData();
    if (Meta.GetValue(&BP, Version) == TEXT("1")) return true;
    BP.Modify(); BP.WidgetTree->Modify();
    if (Screen) ScreenStyle(BP); else CardStyle(BP);
    PearlStyle(BP, Screen);
    Meta.SetValue(&BP, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Meta.RemoveValue(&BP, Version); return false;
}
static void Run()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry")) return;
    auto* Screen = LoadExistingBlueprint(FString(Root) + TEXT("WBP_FF_FacilityCodexScreen"));
    auto* Card = LoadExistingBlueprint(FString(Root) + TEXT("WBP_FF_FacilityCodexCard"));
    if (!Screen || !Card || !Contract(*Screen, true) || !Contract(*Card, false))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: requires the saved showcase widgets."), Prefix); return; }
    const bool bSaved = Save(*Screen, true) && Save(*Card, false);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s"), Prefix, bSaved ? TEXT("PASS") : TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyFacilityCodexContrast"),
    TEXT("Strengthen the saved codex's hierarchy using the project's sage, forest and paper controls."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
