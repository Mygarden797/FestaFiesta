// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_BuildReferenceFacilityCodexBuilder.cpp
// Applies the build reference's floating catalog and detail composition to the saved codex WBPs.

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/StackBox.h"
#include "Components/StackBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Viewport.h"
#include "Editor.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace FFBuildReferenceFacilityCodex
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_REFERENCE_FACILITY_CODEX");
constexpr const TCHAR* Version = TEXT("FF.BuildReference.FacilityCodex.20260923.v1");
constexpr const TCHAR* PolishVersion = TEXT("FF.BuildReference.FacilityCodex.20260923.v2");
constexpr const TCHAR* IngredientVersion = TEXT("FF.BuildReference.FacilityCodex.20260923.v3");
constexpr const TCHAR* FixedDetailVersion = TEXT("FF.BuildReference.FacilityCodex.20260923.v4");
constexpr const TCHAR* CompactDetailVersion = TEXT("FF.BuildReference.FacilityCodex.20260923.v5");
template <typename T> T* Get(UWidgetTree& Tree, const TCHAR* Name)
{
    return Cast<T>(Tree.FindWidget(FName(Name)));
}
static FLinearColor Color(uint8 R, uint8 G, uint8 B, uint8 A = 255)
{
    return FLinearColor::FromSRGBColor(FColor(R, G, B, A));
}
static FSlateBrush Rounded(FLinearColor Fill, FLinearColor Line, float Radius = 10.f, float Width = 1.f)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(Fill);
    Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Line), Width);
    return Brush;
}
static void Surface(UBorder* Border, FLinearColor Fill, FLinearColor Line, float Radius, float Width = 1.f)
{
    if (!Border) return;
    Border->SetBrush(Rounded(Fill, Line, Radius, Width));
    Border->SetBrushColor(FLinearColor::White);
}
static UFont* FontAsset()
{
    return LoadObject<UFont>(nullptr, *MakeObjectPath(GetUiFontPackage()));
}
static void Text(UWidgetTree& Tree, const TCHAR* Name, UFont* Font, int32 Size, FLinearColor Tint)
{
    if (auto* Label = Get<UTextBlock>(Tree, Name))
    {
        FSlateFontInfo Info = Label->GetFont();
        Info.FontObject = Font;
        Info.TypefaceFontName = TEXT("Regular");
        if (Size > 0) Info.Size = Size;
        Label->SetFont(Info);
        Label->SetColorAndOpacity(FSlateColor(Tint));
        Label->SetShadowOffset(FVector2D::ZeroVector);
    }
}
static void DefaultColor(UWidgetBlueprint& BP, const TCHAR* Name, FLinearColor Value)
{
    UObject* Defaults = BP.GeneratedClass ? BP.GeneratedClass->GetDefaultObject() : nullptr;
    if (auto* Property = Defaults ? FindFProperty<FStructProperty>(Defaults->GetClass(), Name) : nullptr)
    {
        Defaults->Modify();
        *Property->ContainerPtrToValuePtr<FLinearColor>(Defaults) = Value;
    }
}
static void DefaultFloat(UWidgetBlueprint& BP, const TCHAR* Name, float Value)
{
    UObject* Defaults = BP.GeneratedClass ? BP.GeneratedClass->GetDefaultObject() : nullptr;
    if (auto* Property = Defaults ? FindFProperty<FFloatProperty>(Defaults->GetClass(), Name) : nullptr)
    {
        Defaults->Modify();
        Property->SetPropertyValue_InContainer(Defaults, Value);
    }
}
static void Navigation(UWidgetTree& Tree, const TCHAR* Name)
{
    auto* Button = Get<UButton>(Tree, Name);
    if (!Button) return;
    const auto& P = UFF_UIPalette::Get()->InGame;
    FButtonStyle Style = Button->GetStyle();
    Style.Normal = Rounded(Color(250, 252, 249, 233), P.Line, 8.f);
    Style.Hovered = Rounded(P.Hover, P.Teal, 8.f, 1.5f);
    Style.Pressed = Rounded(P.TealSoft, P.TealDeep, 8.f, 1.5f);
    // UFF_FacilityCodexScreen uses Disabled as the still-enabled selected fill.
    Style.Disabled = Rounded(P.TealDeep, P.TealDeep, 8.f, 1.5f);
    Style.NormalPadding = Style.PressedPadding = FMargin(12, 5);
    Button->SetStyle(Style);
    if (auto* Label = Cast<UTextBlock>(Button->GetContent()))
        Label->SetColorAndOpacity(FSlateColor(P.Ink));
}
static bool ScreenContract(UWidgetBlueprint& BP)
{
    if (!BP.WidgetTree || !BP.ParentClass || !BP.ParentClass->IsChildOf(UFF_FacilityCodexScreen::StaticClass())) return false;
    auto& Tree = *BP.WidgetTree;
    if (BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1"))
        return Get<UCanvasPanel>(Tree, TEXT("Canvas_CodexLayout")) &&
            Get<UBorder>(Tree, TEXT("Border_CodexDock")) &&
            Get<USizeBox>(Tree, TEXT("Size_Detail"));
    auto* Paper = Get<UBorder>(Tree, TEXT("Border_Paper"));
    auto* Column = Get<UVerticalBox>(Tree, TEXT("Vertical_Screen"));
    auto* Main = Get<UStackBox>(Tree, TEXT("Stack_Main"));
    auto* Browse = Get<USizeBox>(Tree, TEXT("Size_Browse"));
    auto* Detail = Get<USizeBox>(Tree, TEXT("Size_Detail"));
    if (!Paper || !Column || Paper->GetContent() != Column || !Main || Main->GetChildrenCount() != 2 ||
        !Browse || !Detail || Main->GetChildAt(0) != Browse || Main->GetChildAt(1) != Detail ||
        Tree.FindWidget(TEXT("Canvas_CodexLayout"))) return false;
    for (const TCHAR* Name : {TEXT("Border_InputShield"), TEXT("Border_Header"), TEXT("Border_Body"),
        TEXT("Panel_Detail"), TEXT("Border_Showcase"), TEXT("Border_Bonus"), TEXT("Border_Summary"),
        TEXT("BackgroundBlur_Glass")})
        if (!Get<UBorder>(Tree, Name) && FCString::Strcmp(Name, TEXT("BackgroundBlur_Glass")) != 0) return false;
    return Get<UBackgroundBlur>(Tree, TEXT("BackgroundBlur_Glass")) &&
        Get<UVerticalBox>(Tree, TEXT("Vertical_ShowcaseDetail")) &&
        Get<UTextBlock>(Tree, TEXT("Text_DetailStatus")) &&
        Get<UButton>(Tree, TEXT("Button_Close")) &&
        Get<USizeBox>(Tree, TEXT("Size_Showcase"));
}
static bool CardContract(UWidgetBlueprint& BP)
{
    if (!BP.WidgetTree || !BP.ParentClass || !BP.ParentClass->IsChildOf(UFF_FacilityCodexCard::StaticClass())) return false;
    auto& Tree = *BP.WidgetTree;
    return Get<USizeBox>(Tree, TEXT("Size_Card")) && Get<USizeBox>(Tree, TEXT("Size_Name")) &&
        Get<UBorder>(Tree, TEXT("Border_Card")) && Get<UBorder>(Tree, TEXT("Border_Selection")) &&
        Get<UBorder>(Tree, TEXT("Border_ThumbTarget")) && Get<UBorder>(Tree, TEXT("Border_ThumbPartner")) &&
        Get<USizeBox>(Tree, TEXT("Size_ThumbTarget")) && Get<USizeBox>(Tree, TEXT("Size_ThumbPartner")) &&
        Get<USizeBox>(Tree, TEXT("Size_RewardState"));
}
static bool EditScreen(UWidgetBlueprint& BP, UFont* Font)
{
    auto& Tree = *BP.WidgetTree;
    const auto& P = UFF_UIPalette::Get()->InGame;
    auto* Paper = Get<UBorder>(Tree, TEXT("Border_Paper"));
    auto* Column = Get<UVerticalBox>(Tree, TEXT("Vertical_Screen"));
    auto* Detail = Get<USizeBox>(Tree, TEXT("Size_Detail"));
    auto* Main = Get<UStackBox>(Tree, TEXT("Stack_Main"));
    auto* Browse = Get<USizeBox>(Tree, TEXT("Size_Browse"));
    auto* ShowcaseColumn = Get<UVerticalBox>(Tree, TEXT("Vertical_ShowcaseDetail"));
    auto* Status = Get<UTextBlock>(Tree, TEXT("Text_DetailStatus"));
    auto* Close = Get<UButton>(Tree, TEXT("Button_Close"));
    UWidget* CloseContainer = Get<USizeBox>(Tree, TEXT("Size_Refined_Button_Close"));
    if (!CloseContainer || Close->GetParent() != CloseContainer) CloseContainer = Close;

    // The input shield and presenter stay full viewport. Only the painted content
    // becomes two independent canvas layers, so the running festival remains visible.
    Column->RemoveFromParent();
    Paper->ClearChildren();
    auto* Canvas = MakeWidget<UCanvasPanel>(BP, Tree, TEXT("Canvas_CodexLayout"), true);
    Paper->AddChild(Canvas);
    auto* Dock = MakeWidget<UBorder>(BP, Tree, TEXT("Border_CodexDock"), true);
    Surface(Dock, Color(247, 248, 246, 235), Color(216, 229, 219, 240), 22.f, 1.3f);
    Dock->SetPadding(FMargin(0));
    Dock->SetClipping(EWidgetClipping::ClipToBounds);
    Dock->AddChild(Column);
    auto* DockSlot = Canvas->AddChildToCanvas(Dock);
    DockSlot->SetPosition(FVector2D(24.f, 596.f));
    DockSlot->SetSize(FVector2D(1440.f, 460.f));
    DockSlot->SetZOrder(0);

    Detail->RemoveFromParent();
    Detail->ClearWidthOverride(); Detail->ClearHeightOverride();
    auto* DetailSlot = Canvas->AddChildToCanvas(Detail);
    DetailSlot->SetPosition(FVector2D(1482.f, 24.f));
    DetailSlot->SetSize(FVector2D(414.f, 730.f));
    DetailSlot->SetZOrder(1);
    Browse->ClearWidthOverride(); Browse->ClearHeightOverride();
    if (auto* Slot = Cast<UStackBoxSlot>(Browse->Slot))
    {
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Slot->SetPadding(FMargin(0));
    }
    Main->SetOrientation(Orient_Horizontal);

    // Put close at the detail's top edge, where the reference places dismissal.
    // The existing button and its binding are moved intact, including its 40px wrapper.
    CloseContainer->RemoveFromParent();
    Status->RemoveFromParent();
    auto* Top = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Horizontal_CodexDetailTop"));
    auto* StatusSlot = Top->AddChildToHorizontalBox(Status);
    StatusSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    StatusSlot->SetVerticalAlignment(VAlign_Center);
    auto* CloseSlot = Top->AddChildToHorizontalBox(CloseContainer);
    CloseSlot->SetVerticalAlignment(VAlign_Center);
    ShowcaseColumn->AddChildToVerticalBox(Top)->SetPadding(FMargin(0, 0, 0, 4));
    ShowcaseColumn->ShiftChild(0, Top);

    Surface(Paper, FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f);
    Surface(Get<UBorder>(Tree, TEXT("Border_InputShield")), FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f);
    Surface(Get<UBorder>(Tree, TEXT("Border_Header")), FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f);
    Surface(Get<UBorder>(Tree, TEXT("Border_Body")), FLinearColor::Transparent, FLinearColor::Transparent, 0.f, 0.f);
    Surface(Get<UBorder>(Tree, TEXT("Border_Summary")), Color(232, 241, 235, 218), P.Line, 9.f);
    Surface(Get<UBorder>(Tree, TEXT("Panel_Detail")), Color(250, 251, 249, 250), Color(202, 218, 208), 21.f, 1.3f);
    Surface(Get<UBorder>(Tree, TEXT("Border_Showcase")), Color(226, 240, 232, 244), Color(188, 215, 199), 14.f);
    Surface(Get<UBorder>(Tree, TEXT("Border_Bonus")), Color(229, 241, 233, 250), Color(190, 215, 197), 10.f);
    Surface(Get<UBorder>(Tree, TEXT("Border_ShowcaseRewardInfo")), Color(246, 249, 245, 251), Color(203, 220, 207), 10.f);
    if (auto* Blur = Get<UBackgroundBlur>(Tree, TEXT("BackgroundBlur_Glass")))
    {
        Blur->SetBlurStrength(2.f);
        Blur->SetLowQualityFallbackBrush(Rounded(Color(247, 248, 246, 24), FLinearColor::Transparent, 0.f, 0.f));
    }
    Get<UBorder>(Tree, TEXT("Border_Header"))->SetPadding(FMargin(20, 8));
    Get<UBorder>(Tree, TEXT("Border_Body"))->SetPadding(FMargin(18, 3, 18, 14));
    Get<UBorder>(Tree, TEXT("Panel_Detail"))->SetPadding(FMargin(16, 13));
    if (auto* Header = Get<UVerticalBox>(Tree, TEXT("Vertical_Heading"))) Header->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Text(Tree, TEXT("Text_Title"), Font, 23, P.TealDeep);
    Text(Tree, TEXT("Text_Intro"), Font, 11, P.InkMuted);
    Text(Tree, TEXT("Text_Summary"), Font, 13, P.Ink);
    Text(Tree, TEXT("Text_Filter"), Font, 13, P.Ink);
    Text(Tree, TEXT("Text_CollectionRemaining"), Font, 11, P.InkMuted);
    Text(Tree, TEXT("Text_VisualLegend"), Font, 11, P.InkMuted);
    Text(Tree, TEXT("Text_DetailStatus"), Font, 12, P.TealDeep);
    Text(Tree, TEXT("Text_DetailTitle"), Font, 23, P.Ink);
    Text(Tree, TEXT("Text_PreviewFacilityName"), Font, 17, P.Ink);
    Text(Tree, TEXT("Text_PreviewCategory"), Font, 12, P.InkMuted);
    Text(Tree, TEXT("Text_DetailRewardAmount"), Font, 22, Color(150, 87, 21));
    Text(Tree, TEXT("Text_Bonus"), Font, 18, P.TealDeep);
    for (const TCHAR* Name : {TEXT("Text_TargetName"), TEXT("Text_PartnerName"), TEXT("Text_Distance"), TEXT("Text_Applied")})
        Text(Tree, Name, Font, 0, P.Ink);
    for (const TCHAR* Name : {TEXT("Text_Condition"), TEXT("Text_Discovery"), TEXT("Text_ShowcaseDistance"), TEXT("Text_PreviewHint")})
        Text(Tree, Name, Font, 0, P.InkMuted);
    Tree.ForEachWidget([Font](UWidget* Widget)
    {
        if (auto* Label = Cast<UTextBlock>(Widget))
        {
            FSlateFontInfo Info = Label->GetFont();
            Info.FontObject = Font;
            Info.TypefaceFontName = TEXT("Regular");
            Label->SetFont(Info);
        }
    });

    for (const TCHAR* Name : {TEXT("Button_All"), TEXT("Button_Discovered"), TEXT("Button_Undiscovered"),
        TEXT("Button_Active"), TEXT("Button_RewardReady"), TEXT("Button_DetailRecipe"),
        TEXT("Button_DetailPlacement"), TEXT("Button_DetailEffect"), TEXT("Button_PreviewTarget"),
        TEXT("Button_PreviewPartner")}) Navigation(Tree, Name);
    DefaultColor(BP, TEXT("SelectedFilterTextColor"), P.White);
    DefaultColor(BP, TEXT("InactiveFilterTextColor"), P.Ink);
    DefaultFloat(BP, TEXT("MinimumCardWidth"), 174.f);
    if (auto* Search = Get<UEditableTextBox>(Tree, TEXT("Search_Entries")))
    {
        FEditableTextBoxStyle Style = Search->GetWidgetStyle();
        Style.TextStyle.Font.FontObject = Font;
        Style.TextStyle.Font.TypefaceFontName = TEXT("Regular");
        Style.TextStyle.Font.Size = 14;
        Style.TextStyle.SetColorAndOpacity(P.Ink);
        Style.SetBackgroundImageNormal(Rounded(Color(255, 255, 255, 245), P.Line, 8.f))
            .SetBackgroundImageHovered(Rounded(P.White, P.Teal, 8.f, 1.5f))
            .SetBackgroundImageFocused(Rounded(P.White, P.TealDeep, 8.f, 2.f))
            .SetForegroundColor(P.Ink).SetFocusedForegroundColor(P.Ink);
        Search->SetWidgetStyle(Style);
    }
    if (auto* Viewport = Get<UViewport>(Tree, TEXT("Viewport_Showcase")))
        Viewport->SetBackgroundColor(Color(229, 241, 233));
    if (auto* Tiles = Get<UFF_FacilityCodexTileView>(Tree, TEXT("TileView_Entries")))
    {
        Tiles->SetEntryHeight(214.f);
        Tiles->SetHorizontalEntrySpacing(8.f);
        Tiles->SetVerticalEntrySpacing(8.f);
    }
    for (const TCHAR* Name : {TEXT("Size_ThumbTarget"), TEXT("Size_ThumbPartner")})
        if (auto* Thumb = Get<USizeBox>(Tree, Name))
        {
            Thumb->SetWidthOverride(66.f);
            Thumb->SetHeightOverride(64.f);
        }
    return true;
}
static bool EditCard(UWidgetBlueprint& BP, UFont* Font)
{
    auto& Tree = *BP.WidgetTree;
    const auto& P = UFF_UIPalette::Get()->InGame;
    Get<USizeBox>(Tree, TEXT("Size_Card"))->SetHeightOverride(206.f);
    Get<USizeBox>(Tree, TEXT("Size_Name"))->SetHeightOverride(52.f);
    for (const TCHAR* Side : {TEXT("Target"), TEXT("Partner")})
    {
        Get<USizeBox>(Tree, *(FString(TEXT("Size_Thumb")) + Side))->SetHeightOverride(68.f);
        auto* Tray = Get<UBorder>(Tree, *(FString(TEXT("Border_Thumb")) + Side));
        Surface(Tray, Color(228, 240, 232, 255), Color(188, 212, 197), 8.f);
        Tray->SetPadding(FMargin(2));
    }
    // The existing card material stays in place so discovered, undiscovered,
    // selection and reward effects continue to use their authored shaders.
    Get<UBorder>(Tree, TEXT("Border_Card"))->SetPadding(FMargin(8));
    if (auto* Images = Tree.FindWidget(TEXT("Horizontal_RecipeImages")))
        if (auto* Slot = Cast<UVerticalBoxSlot>(Images->Slot)) Slot->SetPadding(FMargin(0, 0, 0, 5));
    auto* Selection = Get<UBorder>(Tree, TEXT("Border_Selection"));
    Surface(Selection, FLinearColor::Transparent, P.TealDeep, 11.f, 2.5f);
    auto* State = Get<UBorder>(Tree, TEXT("Border_State"));
    Surface(State, Color(223, 237, 226, 244), Color(179, 205, 186), 6.f);
    State->SetPadding(FMargin(6, 2));
    if (auto* Slot = Cast<UVerticalBoxSlot>(State->Slot)) Slot->SetPadding(FMargin(0, 4, 0, 0));
    Tree.ForEachWidget([Font](UWidget* Widget)
    {
        if (auto* Label = Cast<UTextBlock>(Widget))
        {
            FSlateFontInfo Info = Label->GetFont();
            Info.FontObject = Font;
            Info.TypefaceFontName = TEXT("Regular");
            Label->SetFont(Info);
        }
    });
    Text(Tree, TEXT("Text_Name"), Font, 15, P.Ink);
    Text(Tree, TEXT("Text_Bonus"), Font, 16, P.TealDeep);
    Text(Tree, TEXT("Text_Plus"), Font, 20, P.TealDeep);
    Text(Tree, TEXT("Text_State"), Font, 11, P.TealDeep);
    Text(Tree, TEXT("Text_Recipe"), Font, 12, P.InkMuted);
    DefaultColor(BP, TEXT("UndiscoveredTitleColor"), P.Ink);
    DefaultColor(BP, TEXT("UndiscoveredBodyColor"), P.InkMuted);
    DefaultColor(BP, TEXT("UndiscoveredOutlineColor"), Color(203, 216, 207));
    DefaultColor(BP, TEXT("UndiscoveredBackgroundColor"), Color(240, 245, 241, 251));
    return true;
}
static bool Save(UWidgetBlueprint& BP, bool bScreen, UFont* Font)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Version) == TEXT("1")) return true;
    BP.Modify(); BP.WidgetTree->Modify();
    if (!(bScreen ? EditScreen(BP, Font) : EditCard(BP, Font))) return false;
    Metadata.SetValue(&BP, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Version);
    return false;
}
static bool PolishScreen(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, PolishVersion) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Canvas = Get<UCanvasPanel>(Tree, TEXT("Canvas_CodexLayout"));
    auto* Column = Get<UVerticalBox>(Tree, TEXT("Vertical_ShowcaseDetail"));
    auto* Scroll = Get<UScrollBox>(Tree, TEXT("Scroll_DetailGlass"));
    auto* Reward = Get<USizeBox>(Tree, TEXT("Size_ShowcaseRewardInfo"));
    auto* Close = Get<UButton>(Tree, TEXT("Button_Close"));
    UWidget* CloseContainer = Get<USizeBox>(Tree, TEXT("Size_Refined_Button_Close"));
    if (!CloseContainer || (Close && Close->GetParent() != CloseContainer)) CloseContainer = Close;
    if (!Canvas || !Column || !Scroll || Scroll->GetParent() != Column || !Reward || !CloseContainer) return false;
    BP.Modify(); Tree.Modify();

    // The detail is hidden for empty search results. Dismissal must remain
    // reachable above the canvas regardless of that data state.
    CloseContainer->RemoveFromParent();
    auto* CloseSlot = Canvas->AddChildToCanvas(CloseContainer);
    CloseSlot->SetPosition(FVector2D(1804.f, 36.f));
    CloseSlot->SetSize(FVector2D(78.f, 40.f));
    CloseSlot->SetZOrder(2);

    // Keep the reward claim in a fixed, full-width footer. The existing
    // Switcher_Detail and its facility-catalog links stay in the scroll area.
    Reward->RemoveFromParent();
    Reward->ClearWidthOverride();
    auto* RewardSlot = Column->AddChildToVerticalBox(Reward);
    RewardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
    RewardSlot->SetPadding(FMargin(0, 7, 0, 0));
    Column->ShiftChild(Column->GetChildIndex(Scroll) + 1, Reward);
    if (auto* Panel = Get<UBorder>(Tree, TEXT("Border_ShowcaseRewardInfo")))
        Panel->SetPadding(FMargin(10, 8));
    Metadata.SetValue(&BP, PolishVersion, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, PolishVersion);
    return false;
}
static bool PinIngredients(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, IngredientVersion) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Column = Get<UVerticalBox>(Tree, TEXT("Vertical_ShowcaseDetail"));
    auto* Scroll = Get<UScrollBox>(Tree, TEXT("Scroll_DetailGlass"));
    auto* Ingredients = Get<UHorizontalBox>(Tree, TEXT("Horizontal_Ingredients"));
    if (!Column || !Scroll || Scroll->GetParent() != Column || !Ingredients) return false;
    BP.Modify(); Tree.Modify();
    // Facility names and pictures are catalog links. Keep both visible on every
    // detail page, including at 720p with the reward fixed at the card bottom.
    Ingredients->RemoveFromParent();
    auto* Slot = Column->AddChildToVerticalBox(Ingredients);
    Slot->SetPadding(FMargin(0, 3, 0, 7));
    Slot->SetHorizontalAlignment(HAlign_Left);
    Column->ShiftChild(Column->GetChildIndex(Scroll), Ingredients);
    Metadata.SetValue(&BP, IngredientVersion, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, IngredientVersion);
    return false;
}
static bool FixDetailViewport(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, FixedDetailVersion) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Column = Get<UVerticalBox>(Tree, TEXT("Vertical_ShowcaseDetail"));
    auto* ScrollColumn = Get<UVerticalBox>(Tree, TEXT("Vertical_ShowcaseScroll"));
    auto* Scroll = Get<UScrollBox>(Tree, TEXT("Scroll_DetailGlass"));
    auto* Dock = Get<UBorder>(Tree, TEXT("Border_CodexDock"));
    auto* PreviewHeader = Get<UHorizontalBox>(Tree, TEXT("Horizontal_PreviewHeader"));
    auto* Hero = Get<USizeBox>(Tree, TEXT("Size_Showcase"));
    auto* Rotation = Get<UHorizontalBox>(Tree, TEXT("Horizontal_PreviewRotation"));
    auto* Tabs = Get<UHorizontalBox>(Tree, TEXT("Horizontal_DetailTabs"));
    if (!Column || !ScrollColumn || !Scroll || !Dock || Scroll->GetParent() != Column ||
        !PreviewHeader || !Hero || !Rotation || !Tabs ||
        PreviewHeader->GetParent() != ScrollColumn || Hero->GetParent() != ScrollColumn ||
        Rotation->GetParent() != ScrollColumn || Tabs->GetParent() != ScrollColumn) return false;
    BP.Modify(); Tree.Modify();
    Surface(Dock, Color(247, 248, 246, 218), Color(216, 229, 219, 240), 22.f, 1.3f);
    // The reward footer is fixed at the card bottom. Keep the preview and page
    // tabs ahead of the fill-sized scroll view so both remain visible at 720p.
    for (UWidget* Widget : {static_cast<UWidget*>(PreviewHeader), static_cast<UWidget*>(Hero),
        static_cast<UWidget*>(Rotation), static_cast<UWidget*>(Tabs)})
    {
        Widget->RemoveFromParent();
        auto* Slot = Column->AddChildToVerticalBox(Widget);
        Slot->SetPadding(Widget == PreviewHeader ? FMargin(0, 0, 0, 7) :
            Widget == Hero ? FMargin(0, 0, 0, 6) : FMargin(0, 0, 0, 5));
        Column->ShiftChild(Column->GetChildIndex(Scroll), Widget);
    }
    if (auto* Text = Get<UTextBlock>(Tree, TEXT("Text_ShowcaseDistance")))
        if (auto* Slot = Cast<UVerticalBoxSlot>(Text->Slot)) Slot->SetPadding(FMargin(0, 0, 0, 5));
    if (auto* Text = Get<UTextBlock>(Tree, TEXT("Text_DetailRewardAmount")))
        if (auto* Slot = Cast<UVerticalBoxSlot>(Text->Slot)) Slot->SetPadding(FMargin(0, 0, 0, 5));
    Metadata.SetValue(&BP, FixedDetailVersion, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, FixedDetailVersion);
    return false;
}
static bool ReserveDetailTextSpace(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, CompactDetailVersion) == TEXT("1")) return true;
    auto& Tree = *BP.WidgetTree;
    auto* Column = Get<UVerticalBox>(Tree, TEXT("Vertical_ShowcaseDetail"));
    auto* Scroll = Get<UScrollBox>(Tree, TEXT("Scroll_DetailGlass"));
    auto* Target = Get<USizeBox>(Tree, TEXT("Size_ThumbTarget"));
    auto* Partner = Get<USizeBox>(Tree, TEXT("Size_ThumbPartner"));
    if (!Column || !Scroll || Scroll->GetParent() != Column || !Target || !Partner) return false;
    BP.Modify(); Tree.Modify();
    // Reclaim only ornamental gaps; keep the 40px tab targets, large preview,
    // facility links and reward action at their authored sizes.
    Target->SetHeightOverride(56.f);
    Partner->SetHeightOverride(56.f);
    for (const TPair<const TCHAR*, float>& Item : {
        TPair<const TCHAR*, float>(TEXT("Text_DetailStatus"), 2.f),
        TPair<const TCHAR*, float>(TEXT("Text_DetailTitle"), 8.f),
        TPair<const TCHAR*, float>(TEXT("Horizontal_PreviewHeader"), 4.f),
        TPair<const TCHAR*, float>(TEXT("Size_Showcase"), 4.f),
        TPair<const TCHAR*, float>(TEXT("Horizontal_PreviewRotation"), 2.f),
        TPair<const TCHAR*, float>(TEXT("Horizontal_DetailTabs"), 4.f)})
    {
        if (UWidget* Widget = Tree.FindWidget(FName(Item.Key)))
            if (auto* Slot = Cast<UVerticalBoxSlot>(Widget->Slot))
                Slot->SetPadding(FMargin(0, 0, 0, Item.Value));
    }
    Metadata.SetValue(&BP, CompactDetailVersion, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, CompactDetailVersion);
    return false;
}
static void Run()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: requires an idle Entry editor."), Prefix);
        return;
    }
    UFont* Font = FontAsset();
    auto* Screen = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityCodexScreenPackage);
    auto* Card = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityCodexCardPackage);
    if (!Font || !Screen || !Card || !ScreenContract(*Screen) || !CardContract(*Card))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: missing font or incompatible saved codex widget contract; no asset changed."), Prefix);
        return;
    }
    const bool bSaved = Save(*Card, false, Font) && Save(*Screen, true, Font) &&
        PolishScreen(*Screen) && PinIngredients(*Screen) && FixDetailViewport(*Screen) &&
        ReserveDetailTextSpace(*Screen);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_%s"), Prefix, bSaved ? TEXT("PASS") : TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyBuildReferenceFacilityCodex"),
    TEXT("Lay out the existing codex as a translucent bottom catalog and floating detail card."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
