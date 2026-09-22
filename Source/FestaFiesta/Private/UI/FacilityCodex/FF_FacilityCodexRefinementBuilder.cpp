// FF_FacilityCodexRefinementBuilder.cpp
// 역할: 도감 목록의 열 구성과 시각 요소를 부분 수정한다.

// Additive, explicit visual refinement of the saved six-column codex.
#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

namespace FFFacilityCodexRefinement
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_CLARITY_CODEX_REFINEMENT");
constexpr const TCHAR* Version = TEXT("FF.BuildClarity.CodexRefinement20260921.1");
constexpr const TCHAR* TooltipVersion = TEXT("FF.BuildClarity.CodexRefinement20260921.2");
constexpr const TCHAR* TitleFitVersion = TEXT("FF.BuildClarity.CodexRefinement20260921.3");
template<typename T> static T* Get(UWidgetTree& Tree, const TCHAR* Name) { return Cast<T>(Tree.FindWidget(FName(Name))); }
static void Font(UWidgetTree& Tree, const TCHAR* Name, int32 Size)
{ if (auto* W = Get<UTextBlock>(Tree,Name)) { auto F = W->GetFont(); F.Size = Size; W->SetFont(F); } }
static void VSpace(UWidget* W, FMargin Padding)
{ if (auto* S = W ? Cast<UVerticalBoxSlot>(W->Slot) : nullptr) S->SetPadding(Padding); }

static bool Card(UWidgetBlueprint&, UWidgetTree& Tree)
{
    auto* Bounds = Get<USizeBox>(Tree,TEXT("Size_Card"));
    auto* Column = Get<UVerticalBox>(Tree,TEXT("Vertical_Card"));
    auto* Bonus = Get<UTextBlock>(Tree,TEXT("Text_Bonus"));
    auto* State = Get<UBorder>(Tree,TEXT("Border_State"));
    if (!Bounds || !Column || !Bonus || !State) return false;
    Bounds->SetHeightOverride(190);
    if (auto* Name = Get<USizeBox>(Tree,TEXT("Size_Name"))) Name->SetHeightOverride(42);
    for (const TCHAR* Name : {TEXT("Size_ThumbTarget"),TEXT("Size_ThumbPartner")})
        if (auto* Thumb = Get<USizeBox>(Tree,Name)) Thumb->SetHeightOverride(60);
    for (const TCHAR* Name : {TEXT("Border_ThumbTarget"),TEXT("Border_ThumbPartner")})
        if (auto* Thumb = Get<UBorder>(Tree,Name)) Thumb->SetPadding(FMargin(4));
    VSpace(Tree.FindWidget(TEXT("Horizontal_RecipeImages")),FMargin(0,0,0,8));
    // Bonus and state each own the full card width. A long state can no longer squeeze
    // '+10%' into a second line or cover the bonus. Keep the retired summary attached.
    Bonus->RemoveFromParent(); State->RemoveFromParent();
    Column->AddChildToVerticalBox(Bonus);
    auto* StateSlot = Column->AddChildToVerticalBox(State);
    StateSlot->SetPadding(FMargin(0,5,0,0)); StateSlot->SetHorizontalAlignment(HAlign_Left);
    if (auto* Retired = Tree.FindWidget(TEXT("Horizontal_AuroraCardSummary"))) Retired->SetVisibility(ESlateVisibility::Collapsed);
    State->SetPadding(FMargin(7,3));
    Font(Tree,TEXT("Text_Name"),15); Font(Tree,TEXT("Text_Bonus"),16); Font(Tree,TEXT("Text_State"),11);
    Bonus->SetAutoWrapText(false); Bonus->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    if (auto* Text = Get<UTextBlock>(Tree,TEXT("Text_State"))) Text->SetAutoWrapText(false);
    if (auto* Outline = Get<UBorder>(Tree,TEXT("Border_Selection")))
    { FSlateBrush Brush = Outline->Background; Brush.OutlineSettings.Width = 2.f; Outline->SetBrush(Brush); }
    return true;
}

static bool Height40(UWidgetBlueprint& BP, UWidgetTree& Tree, UWidget* Widget)
{
    if (!Widget) return false;
    const FName Name(TEXT("Size_Refined_") + Widget->GetName());
    if (Tree.FindWidget(Name)) return true;
    auto* Parent = Cast<UHorizontalBox>(Widget->GetParent());
    auto* Original = Cast<UHorizontalBoxSlot>(Widget->Slot);
    if (!Parent || !Original) return false;
    const int32 Index = Parent->GetChildIndex(Widget);
    const FMargin Padding = Original->GetPadding();
    const FSlateChildSize Size = Original->GetSize();
    Widget->RemoveFromParent();
    auto* Bounds = MakeWidget<USizeBox>(BP,Tree,Name);
    Bounds->SetHeightOverride(40);
    auto* Slot = Parent->AddChildToHorizontalBox(Bounds);
    Slot->SetPadding(Padding); Slot->SetSize(Size); Slot->SetVerticalAlignment(VAlign_Center);
    Parent->ShiftChild(Index,Bounds); Bounds->AddChild(Widget);
    return true;
}

static bool Screen(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    auto* Tiles = Get<UFF_FacilityCodexTileView>(Tree,TEXT("TileView_Entries"));
    if (!Tiles || !Tree.FindWidget(TEXT("Size_Dialog"))) return false;
    Tiles->ColumnCount = 6; Tiles->SetEntryHeight(200.f);
    Tiles->SetHorizontalEntrySpacing(8.f); Tiles->SetVerticalEntrySpacing(10.f);
    for (const TCHAR* Name : {TEXT("Button_Close"),TEXT("Button_All"),TEXT("Button_Discovered"),
        TEXT("Button_Undiscovered"),TEXT("Button_Active"),TEXT("Button_ClearFacility"),
        TEXT("Button_DetailRecipe"),TEXT("Button_DetailPlacement"),TEXT("Button_DetailEffect")})
    {
        auto* Button = Get<UButton>(Tree,Name);
        if (!Height40(BP,Tree,Button)) return false;
        FButtonStyle Style = Button->GetStyle();
        Style.NormalPadding = Style.PressedPadding = FMargin(14,6); Button->SetStyle(Style);
        if (auto* Label = Cast<UTextBlock>(Button->GetContent()))
        { auto F = Label->GetFont(); F.Size = 14; Label->SetFont(F); Label->SetAutoWrapText(false); }
    }
    if (!Height40(BP,Tree,Tree.FindWidget(TEXT("Search_Entries")))) return false;
    if (auto* Header = Get<UBorder>(Tree,TEXT("Border_Header"))) Header->SetPadding(FMargin(20,12));
    Font(Tree,TEXT("Text_Title"),22); Font(Tree,TEXT("Text_Summary"),13);
    return true;
}

static bool CardTooltip(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    // Keep the authoring preview consistent with the runtime card: details are
    // already presented beneath the grid, so no repeated-name hover card is needed.
    if (!Tree.FindWidget(TEXT("Size_Card"))) return false;
    Tree.ForEachWidget([](UWidget* Widget) { Widget->SetToolTip(nullptr); Widget->SetToolTipText(FText::GetEmpty()); });
    if (auto* CDO = BP.GeneratedClass ? BP.GeneratedClass->GetDefaultObject<UUserWidget>() : nullptr)
    { CDO->SetToolTip(nullptr); CDO->SetToolTipText(FText::GetEmpty()); }
    return true;
}
static bool CardTitleFit(UWidgetBlueprint&, UWidgetTree& Tree)
{
    auto* Card = Get<USizeBox>(Tree,TEXT("Size_Card"));
    auto* Name = Get<USizeBox>(Tree,TEXT("Size_Name"));
    if (!Card || !Name) return false;
    // Two Korean text lines use more than the former 42px title region at the
    // authored font. Reserve both lines without cutting their lower glyphs.
    Name->SetHeightOverride(54); Card->SetHeightOverride(204);
    return true;
}
static bool ScreenTitleFit(UWidgetBlueprint&, UWidgetTree& Tree)
{
    auto* Tiles = Get<UFF_FacilityCodexTileView>(Tree,TEXT("TileView_Entries"));
    if (!Tiles || !Tree.FindWidget(TEXT("Size_Dialog"))) return false;
    Tiles->ColumnCount = 6; Tiles->SetEntryHeight(214);
    return true;
}
static bool Apply(const TCHAR* Path, UClass* Expected, bool(*Edit)(UWidgetBlueprint&,UWidgetTree&), const TCHAR* AppliedVersion = Version)
{
    auto* BP = LoadExistingBlueprint(Path);
    if (!BP || !BP->WidgetTree || !BP->ParentClass || !BP->ParentClass->IsChildOf(Expected)) return false;
    auto& Meta = BP->GetOutermost()->GetMetaData();
    if (Meta.GetValue(BP,AppliedVersion) == TEXT("1")) return true;
    BP->Modify(); BP->WidgetTree->Modify();
    if (!Edit(*BP,*BP->WidgetTree)) return false;
    Meta.SetValue(BP,AppliedVersion,TEXT("1"));
    if (!SaveNewBlueprint(Prefix,*BP)) { Meta.RemoveValue(BP,AppliedVersion); return false; }
    return true;
}
static void Run()
{
    bool Ok = Apply(FFBuildUIAssetPaths::FacilityCodexCardPackage,UFF_FacilityCodexCard::StaticClass(),Card);
    Ok &= Apply(FFBuildUIAssetPaths::FacilityCodexCardPackage,UFF_FacilityCodexCard::StaticClass(),CardTooltip,TooltipVersion);
    Ok &= Apply(FFBuildUIAssetPaths::FacilityCodexScreenPackage,UFF_FacilityCodexScreen::StaticClass(),Screen);
    Ok &= Apply(FFBuildUIAssetPaths::FacilityCodexCardPackage,UFF_FacilityCodexCard::StaticClass(),CardTitleFit,TitleFitVersion);
    Ok &= Apply(FFBuildUIAssetPaths::FacilityCodexScreenPackage,UFF_FacilityCodexScreen::StaticClass(),ScreenTitleFit,TitleFitVersion);
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("%s_COMPLETE_%s"),Prefix,Ok ? TEXT("PASS") : TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyBuildModeClarityCodexRefinement"),
    TEXT("Refine the saved centered six-column codex once: full-width bonus, clearer thumbnails, and consistent 40px controls."),
    FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
