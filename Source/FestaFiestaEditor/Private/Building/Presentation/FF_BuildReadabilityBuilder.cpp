// 기존 버튼과 데이터 연결을 유지하며 건축 화면의 글자 여백과 스크롤 영역을 보정한다.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Building/Presentation/FF_BuildTypography.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace FFBuildReadability
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_READABILITY");
constexpr const TCHAR* Version = TEXT("FF.BuildReadability.20260929.v2");

template<class T> T* Find(UWidgetTree& Tree, const TCHAR* Name)
{ return Cast<T>(Tree.FindWidget(Name)); }

void Height(UWidgetTree& Tree, const TCHAR* Name, float Value)
{ if (auto* Size = Find<USizeBox>(Tree, Name)) Size->SetHeightOverride(Value); }

void Label(UWidgetTree& Tree, const TCHAR* Name, int32 Size, bool bWrap = false,
    FFBuildTypography::ERole Role = FFBuildTypography::ERole::Label)
{
    if (auto* Text = Find<UTextBlock>(Tree, Name))
    {
        if (auto* Common = Cast<UCommonTextBlock>(Text)) Common->SetStyle(nullptr);
        // Typography migration already chose the role/typeface. Keep that role
        // when adjusting size, so all cards and the final font audit agree.
        FSlateFontInfo Font = Text->GetFont();
        if (!Font.FontObject) Font = FFBuildTypography::Make(Role, Size);
        Font.Size = Size;
        Text->SetFont(Font);
        Text->SetAutoWrapText(bWrap);
        Text->SetWrapTextAt(0.f); // use the allocated width, never an older fixed text width
        Text->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
        Text->SetTextOverflowPolicy(bWrap ? ETextOverflowPolicy::Clip : ETextOverflowPolicy::Ellipsis);
        Text->SetLineHeightPercentage(1.1f);
        Text->SetClipping(EWidgetClipping::ClipToBounds);
    }
}

void Scroll(UWidgetTree& Tree, const TCHAR* Name)
{
    if (auto* Box = Find<UScrollBox>(Tree, Name))
    {
        Box->SetScrollBarVisibility(ESlateVisibility::Visible);
        Box->SetAnimateWheelScrolling(true);
        Box->SetClipping(EWidgetClipping::ClipToBounds);
    }
}

void Catalog(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    Height(Tree, TEXT("Size_ClarityCatalogDock"), 378.f);
    Height(Tree, TEXT("Size_SubcategoryHeader"), 42.f);
    if (auto* Tiles = Find<UFF_PrepBuildTileView>(Tree, TEXT("TileView_Facilities")))
        Tiles->SetEntryHeight(264.f);
    // The subcategory selector yields space to search and ranking. Its contents
    // already scroll horizontally, so long category names cannot push actions out.
    if (auto* Size = Find<USizeBox>(Tree, TEXT("Size_SubcategoryViewport")))
    {
        Size->ClearWidthOverride(); Size->ClearMinDesiredWidth();
        if (auto* Slot = Cast<UHorizontalBoxSlot>(Size->Slot))
            Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
    Label(Tree, TEXT("Text_RevenueRanking"), 15);
    Label(Tree, TEXT("Text_ItemCount"), 14);
    if (auto* Search = Find<UEditableTextBox>(Tree, TEXT("Search_Facilities")))
        Search->SetMinDesiredWidth(100.f);
}

void CatalogCard(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    Height(Tree, TEXT("SizeBox_Card"), 264.f);
    Height(Tree, TEXT("Size_PearlFacilityName"), 96.f);
    Label(Tree, TEXT("Text_Name"), 15, true);
    if (auto* Name = Find<UTextBlock>(Tree, TEXT("Text_Name")))
        Name->SetWrapTextAt(158.f); // virtualized entries have a fixed 184px pitch
    Label(Tree, TEXT("Text_Cost"), 17, false, FFBuildTypography::ERole::Number);
    Label(Tree, TEXT("Text_State"), 14, true);
    for (const TCHAR* Name : {TEXT("Text_RequiredMissing"), TEXT("Text_RequiredSatisfied"), TEXT("Text_ProgramBadge")})
        Label(Tree, Name, 14);
    // State copy sits inside the thumbnail; clip to its bounds instead of painting
    // across the facility name when a disabled reason wraps to several lines.
    if (auto* Thumbnail = Find<UOverlay>(Tree, TEXT("Overlay_Thumbnail")))
        Thumbnail->SetClipping(EWidgetClipping::ClipToBounds);
}

bool Codex(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    auto* Row = Find<UHorizontalBox>(Tree, TEXT("Row_CollectionProgress"));
    auto* Track = Find<UOverlay>(Tree, TEXT("Overlay_CollectionProgressTrack"));
    if (!Row || !Track) return false;
    // Keep all reward milestones reachable at narrower widths. Scrolling the
    // complete track preserves alignment between progress and reward thresholds.
    UWidget* RewardViewport = Tree.FindWidget(TEXT("Scroll_ReadableMilestones"));
    if (!RewardViewport)
    {
        Track->RemoveFromParent();
        auto* RewardScroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Scroll_ReadableMilestones"));
        RewardViewport = RewardScroll;
        RewardScroll->SetOrientation(Orient_Horizontal);
        RewardScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
        RewardScroll->SetAnimateWheelScrolling(true);
        auto* TrackSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_ReadableMilestoneTrack"));
        TrackSize->SetMinDesiredWidth(520.f);
        TrackSize->AddChild(Track);
        RewardScroll->AddChild(TrackSize);
        auto* Slot = Row->AddChildToHorizontalBox(RewardScroll);
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Slot->SetPadding(FMargin(12, 0, 0, 0));
    }
    RewardViewport->SetClipping(EWidgetClipping::ClipToBounds);
    for (const TCHAR* Name : {TEXT("Text_DetailTitle"), TEXT("Text_PreviewFacilityName")})
        Label(Tree, Name, 24, true, FFBuildTypography::ERole::Heading);
    for (const TCHAR* Name : {TEXT("Text_TargetName"), TEXT("Text_PartnerName"), TEXT("Text_Bonus"),
        TEXT("Text_Distance"), TEXT("Text_Condition"), TEXT("Text_Applied"), TEXT("Text_DetailDescription")})
        Label(Tree, Name, 16, true);
    for (const TCHAR* Name : {TEXT("Text_DetailClaimReward"), TEXT("Text_DetailRewardClaimed"), TEXT("Text_DetailRewardLocked")})
        Label(Tree, Name, 18, true);
    // Tabs remain one readable line; their horizontal slots share the available row.
    for (const TCHAR* Name : {TEXT("Text_All"), TEXT("Text_Discovered"), TEXT("Text_Undiscovered"), TEXT("Text_Active"), TEXT("Text_RewardReady")})
        Label(Tree, Name, 15);
    for (const TCHAR* Name : {TEXT("Scroll_DetailGlass"), TEXT("Scroll_Detail"), TEXT("Scroll_CollectionCategories")}) Scroll(Tree, Name);
    if (auto* Tiles = Find<UFF_FacilityCodexTileView>(Tree, TEXT("TileView_Entries")))
        Tiles->SetEntryHeight(296.f);
    return true;
}

void CodexCard(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    Height(Tree, TEXT("Size_Card"), 280.f);
    Height(Tree, TEXT("Size_Name"), 64.f);
    Label(Tree, TEXT("Text_Name"), 16, true);
    Label(Tree, TEXT("Text_Bonus"), 14, false);
    Label(Tree, TEXT("Text_State"), 14, true);
    Label(Tree, TEXT("Text_ClaimReward"), 14);
    if (auto* State = Find<UTextBlock>(Tree, TEXT("Text_State")))
        if (auto* Slot = Cast<UHorizontalBoxSlot>(State->Slot))
            Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    if (auto* Card = Find<UBorder>(Tree, TEXT("Border_Card")))
        Card->SetClipping(EWidgetClipping::ClipToBounds);
}

void Inspector(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    // Short tabs share one row, returning a full row of height to the upgrade
    // comparison. The longer program caption gets more width, not smaller text.
    if (auto* Navigation = Find<UVerticalBox>(Tree, TEXT("Panel_InspectorNavigation")))
    {
        auto* Tabs = Find<UHorizontalBox>(Tree, TEXT("Row_ReadableInspectorTabs"));
        if (!Tabs)
        {
            Tabs = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_ReadableInspectorTabs"));
            Navigation->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(6, 4, 6, 8));
        }
        for (const TCHAR* Name : {TEXT("Button_TabOverview"), TEXT("Button_TabSynergy"),
            TEXT("Button_TabOperation"), TEXT("Button_TabUpgrade"), TEXT("Button_TabRevenue"),
            TEXT("Button_TabPrograms"), TEXT("Button_TabLighting")})
            if (auto* Button = Find<UButton>(Tree, Name))
            {
                if (Button->GetParent() != Tabs) { Button->RemoveFromParent(); Tabs->AddChildToHorizontalBox(Button); }
                auto* Slot = CastChecked<UHorizontalBoxSlot>(Button->Slot);
                FSlateChildSize Width(ESlateSizeRule::Fill);
                Width.Value = FCString::Strcmp(Name, TEXT("Button_TabPrograms")) == 0 ? 1.6f : 1.f;
                Slot->SetSize(Width); Slot->SetPadding(FMargin(2, 0));
                auto Style = Button->GetStyle(); Style.NormalPadding = Style.PressedPadding = FMargin(4, 7);
                Button->SetStyle(Style);
            }
        for (const TCHAR* Name : {TEXT("Panel_InspectorTabs"), TEXT("Clarity_SecondaryTabs")})
            if (auto* LegacyRow = Tree.FindWidget(Name)) LegacyRow->SetVisibility(ESlateVisibility::Collapsed);
    }
    Label(Tree, TEXT("Text_FacilityName"), 21, true, FFBuildTypography::ERole::Heading);
    Label(Tree, TEXT("Text_Category"), 14, true);
    Label(Tree, TEXT("Text_LevelSummary"), 14, true);
    Label(Tree, TEXT("Text_FacilityRevenueStatus"), 14, true);
    Label(Tree, TEXT("Text_SynergySummary"), 14, true);
    Tree.ForEachWidget([](UWidget* Widget)
    {
        auto* Text = Cast<UTextBlock>(Widget);
        if (!Text) return;
        const FString Name = Text->GetName();
        if (Name.StartsWith(TEXT("Clarity_Label_")) || Name.StartsWith(TEXT("V2_Upgrade")))
        {
            Text->SetAutoWrapText(true);
            Text->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
            Text->SetClipping(EWidgetClipping::ClipToBounds);
        }
    });
    Scroll(Tree, TEXT("ScrollBox_Details"));
}

void Tools(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    auto* Hero = Find<UBorder>(Tree, TEXT("Border_BuildPlacementHero"));
    auto* HeroRow = Find<UHorizontalBox>(Tree, TEXT("Row_BuildPlacementHero"));
    auto* ModeRow = Find<UHorizontalBox>(Tree, TEXT("Row_ClarityMode"));
    if (Hero && HeroRow && ModeRow && !Tree.FindWidget(TEXT("Column_ReadablePlacementHeader")))
    {
        // Previously the thumbnail also stole width from the mode/action row.
        // Give that row the complete card width and keep name + thumbnail below.
        ModeRow->RemoveFromParent(); HeroRow->RemoveFromParent();
        auto* Column = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_ReadablePlacementHeader"));
        Hero->AddChild(Column);
        Column->AddChildToVerticalBox(ModeRow)->SetPadding(FMargin(0, 0, 0, 8));
        Column->AddChildToVerticalBox(HeroRow);
        if (auto* Mode = Find<UTextBlock>(Tree, TEXT("Text_Mode")))
            if (auto* Slot = Cast<UHorizontalBoxSlot>(Mode->Slot))
            { Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); Slot->SetPadding(FMargin(0, 0, 8, 0)); }
    }
    Label(Tree, TEXT("Text_FacilityName"), 22, true, FFBuildTypography::ERole::Heading);
    for (const TCHAR* Name : {TEXT("Text_Legend"), TEXT("Text_RangeLegend"), TEXT("Text_PlacementAdvice"),
        TEXT("Text_PlacementObstruction"), TEXT("Text_AssistanceMessage"), TEXT("Text_BuildHelpQueueHint")})
        Label(Tree, Name, 14, true);
    Scroll(Tree, TEXT("Scroll_BuildSettings"));
}

void Apply()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_READABILITY_FAIL requires idle Entry editor")); return; }
    const TCHAR* Paths[] = {FFBuildUIAssetPaths::BuildPanelPackage, FFBuildUIAssetPaths::BuildTilePackage,
        FFBuildUIAssetPaths::FacilityCodexScreenPackage, FFBuildUIAssetPaths::FacilityCodexCardPackage,
        FFBuildUIAssetPaths::FacilityInfoPanelPackage, FFBuildUIAssetPaths::BuildToolsPackage};
    TArray<UWidgetBlueprint*> Blueprints;
    for (const TCHAR* Path : Paths)
    {
        auto* BP = LoadExistingBlueprint(Path);
        if (!BP || !BP->WidgetTree)
        { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_READABILITY_FAIL missing %s"), Path); return; }
        Blueprints.Add(BP);
    }
    if (!Blueprints[2]->WidgetTree->FindWidget(TEXT("Row_CollectionProgress")))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_READABILITY_FAIL requires collection layout")); return; }
    if (Blueprints.ContainsByPredicate([](const UWidgetBlueprint* BP)
        { return BP->GetOutermost()->GetMetaData().GetValue(BP, Version) != TEXT("1"); }) == false)
    { UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_READABILITY_PASS already applied")); return; }
    for (auto* BP : Blueprints) { BP->Modify(); BP->WidgetTree->Modify(); }
    Catalog(*Blueprints[0]); CatalogCard(*Blueprints[1]);
    if (!Codex(*Blueprints[2])) return;
    CodexCard(*Blueprints[3]); Inspector(*Blueprints[4]); Tools(*Blueprints[5]);
    for (auto* BP : Blueprints)
    {
        BP->GetOutermost()->GetMetaData().SetValue(BP, Version, TEXT("1"));
        if (!SaveNewBlueprint(Prefix, *BP))
        {
            BP->GetOutermost()->GetMetaData().RemoveValue(BP, Version);
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_READABILITY_FAIL save %s"), *BP->GetName()); return;
        }
    }
    auto* Defaults = Blueprints[0]->GeneratedClass->GetDefaultObject();
    if (auto* Property = FindFProperty<FFloatProperty>(Defaults->GetClass(), TEXT("CatalogHeight")))
        Property->SetPropertyValue_InContainer(Defaults, 378.f);
    if (!SaveAssetPackage(Prefix, *Blueprints[0])) return;
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_READABILITY_PASS assets=%d"), Blueprints.Num());
}
FAutoConsoleCommand Command(TEXT("FF.ApplyBuildReadability"),
    TEXT("Refine readable text, wrapping and overflow for the current construction UI without replacing actions."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
