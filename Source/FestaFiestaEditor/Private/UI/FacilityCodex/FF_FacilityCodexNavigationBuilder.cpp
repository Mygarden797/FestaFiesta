// 도감의 별도 닫기 버튼과 화면 폭을 채우는 수집 보상 영역을 저장된 WBP에 적용한다.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Animation/WidgetAnimation.h"
#include "Building/Presentation/FF_BuildTypography.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Building/Codex/FF_FacilityCodexSettings.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/FacilityCodex/FF_FacilityCodexCard.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace FFBuildReadability
{
bool Codex(UWidgetBlueprint& BP);
void CodexCard(UWidgetBlueprint& BP);
}

namespace FFCodexNavigation
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Version = TEXT("FF.Codex.NavigationRewards.20260929.v4");
constexpr const TCHAR* RewardPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Controls/WBP_FF_CodexCollectionReward");
constexpr const TCHAR* CategoryPackage = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/FacilityCodex/Controls/WBP_FF_CodexCollectionCategory");

bool CanRemoveRetiredBranches(UWidgetBlueprint& BP, std::initializer_list<const TCHAR*> Names, bool bIncludeChildren = true)
{
    if (BP.GetOutermost()->GetMetaData().GetValue(&BP, Version) == TEXT("1")) return true;
    if (!BP.WidgetTree || !BP.ParentClass) return false;
    TSet<UWidget*> Removed;
    TSet<FName> TargetNames;
    const auto AddTarget = [&Removed, &TargetNames](UWidget* Widget)
    {
        Removed.Add(Widget);
        TargetNames.Add(Widget->GetFName());
        if (Widget->Slot) TargetNames.Add(Widget->Slot->GetFName());
    };
    for (const TCHAR* Name : Names)
        if (UWidget* Branch = BP.WidgetTree->FindWidget(Name))
        {
            // ForWidgetAndChildren visits descendants, but not Branch itself.
            AddTarget(Branch);
            if (bIncludeChildren) UWidgetTree::ForWidgetAndChildren(Branch, AddTarget);
        }
    for (UWidget* Widget : Removed)
    {
        if (FBlueprintEditorUtils::IsVariableUsed(&BP, Widget->GetFName()))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_CODEX_NAVIGATION_FAIL %s: retired widget %s is used by a Blueprint graph; nothing was changed"),
                *BP.GetName(), *Widget->GetName());
            return false;
        }
        const FProperty* Binding = FindFProperty<FProperty>(BP.ParentClass.Get(), Widget->GetFName());
        if (Binding && Binding->HasMetaData(TEXT("BindWidget")) && !Binding->HasMetaData(TEXT("BindWidgetOptional")))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_CODEX_NAVIGATION_FAIL %s: retired widget %s is a required native binding; nothing was changed"),
                *BP.GetName(), *Widget->GetName());
            return false;
        }
    }
    for (const UWidgetAnimation* Animation : BP.Animations)
    {
        if (!Animation) continue;
        for (const FWidgetAnimationBinding& Binding : Animation->GetBindings())
        {
            if (TargetNames.Contains(Binding.WidgetName) || TargetNames.Contains(Binding.SlotWidgetName))
            {
                UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_CODEX_NAVIGATION_FAIL %s: animation %s targets retired widget/slot %s/%s; nothing was changed"),
                    *BP.GetName(), *Animation->GetName(), *Binding.WidgetName.ToString(), *Binding.SlotWidgetName.ToString());
                return false;
            }
        }
    }
    return true;
}

// Only named retired branches are removed. Conditional reward/empty/thumbnail
// widgets stay in the tree, including their original bindings and appearance.
// Apply preflights every affected asset before this mutation helper is called.
void RemoveRetiredBranch(UWidgetBlueprint& BP, const TCHAR* Name)
{
    UWidget* Branch = BP.WidgetTree->FindWidget(Name);
    if (!Branch) return;
    TArray<UWidget*> Removed;
    Removed.Add(Branch);
    UWidgetTree::ForWidgetAndChildren(Branch, [&Removed](UWidget* Widget) { Removed.AddUnique(Widget); });
    Branch->RemoveFromParent();
    for (int32 Index = Removed.Num() - 1; Index >= 0; --Index)
    {
        UWidget* Widget = Removed[Index];
        BP.OnVariableRemoved(Widget->GetFName());
        BP.WidgetTree->RemoveWidget(Widget);
        Widget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
    }
}

bool SaveVersion(UWidgetBlueprint& BP)
{
    // UMG Designer ignores runtime Visibility. Persist the matching initial
    // preview state so alternative reward states and feedback never stack up.
    BP.WidgetTree->ForEachWidget([](UWidget* Widget)
    {
        const ESlateVisibility Visibility = Widget->GetVisibility();
        Widget->bHiddenInDesigner = Visibility == ESlateVisibility::Collapsed || Visibility == ESlateVisibility::Hidden;
    });
    BP.GetOutermost()->GetMetaData().SetValue(&BP, Version, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_CODEX_NAVIGATION"), BP)) return true;
    BP.GetOutermost()->GetMetaData().RemoveValue(&BP, Version);
    return false;
}

void Anchor(UWidgetTree& Tree, const TCHAR* Name, FAnchors Anchors, FMargin Offsets, FVector2D Alignment = FVector2D::ZeroVector)
{
    UWidget* Widget = Tree.FindWidget(Name);
    if (auto* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
    {
        Slot->SetAnchors(Anchors); Slot->SetAlignment(Alignment); Slot->SetAutoSize(false); Slot->SetOffsets(Offsets);
    }
}

bool PrepareAuxiliaryAssets(UWidgetBlueprint& Card, UWidgetBlueprint& Reward, UWidgetBlueprint& Launcher)
{
    if (Card.GetOutermost()->GetMetaData().GetValue(&Card, Version) != TEXT("1"))
    {
        Card.Modify(); Card.WidgetTree->Modify();
        RemoveRetiredBranch(Card, TEXT("Size_Recipe"));
        RemoveRetiredBranch(Card, TEXT("Horizontal_AuroraCardSummary"));
        if (auto* Bonus = Cast<UTextBlock>(Card.WidgetTree->FindWidget(TEXT("Text_Bonus"))))
        {
            // Auto-wrap inside an auto-sized ScaleBox repeatedly measures its
            // shortest word; that made the percentage stack vertically.
            Bonus->SetAutoWrapText(false);
            Bonus->SetWrapTextAt(0.f);
        }
        if (!SaveVersion(Card)) return false;
    }
    if (Reward.GetOutermost()->GetMetaData().GetValue(&Reward, Version) != TEXT("1"))
    {
        Reward.Modify(); Reward.WidgetTree->Modify();
        RemoveRetiredBranch(Reward, TEXT("Vertical_MilestoneCard"));
        if (!SaveVersion(Reward)) return false;
    }
    if (Launcher.GetOutermost()->GetMetaData().GetValue(&Launcher, Version) != TEXT("1"))
    {
        auto* Bounds = Cast<USizeBox>(Launcher.WidgetTree->FindWidget(TEXT("Size_Launcher")));
        auto* Button = Cast<UButton>(Launcher.WidgetTree->FindWidget(TEXT("Button_OpenCodex")));
        auto* Label = Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr;
        if (!Bounds || !Button || !Label) return false;
        Launcher.Modify(); Launcher.WidgetTree->Modify();
        Bounds->SetWidthOverride(150.f); Bounds->SetHeightOverride(34.f);
        auto* Catalog = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildPanelPackage);
        auto* Tab = Catalog && Catalog->WidgetTree ? Cast<UButton>(Catalog->WidgetTree->FindWidget(TEXT("Button_FilterAll"))) : nullptr;
        if (!Tab)
            if (auto* CategoryStyle = LoadExistingBlueprint(FFBuildUIAssetPaths::CategoryChoicePackage))
                if (CategoryStyle->WidgetTree) Tab = Cast<UButton>(CategoryStyle->WidgetTree->FindWidget(TEXT("Button_Root")));
        auto* RankingLabel = Catalog && Catalog->WidgetTree ? Cast<UTextBlock>(Catalog->WidgetTree->FindWidget(TEXT("Text_RevenueRanking"))) : nullptr;
        auto Style = Tab ? Tab->GetStyle() : Button->GetStyle();
        Style.NormalPadding = Style.PressedPadding = FMargin(10.f, 3.f);
        Button->SetStyle(Style);
        Label->SetText(NSLOCTEXT("FFCodexNavigation", "CatalogEntry", "조합 도감"));
        if (RankingLabel) Label->SetFont(RankingLabel->GetFont());
        Label->SetColorAndOpacity(FSlateColor(FFBuildPearl::Ink));
        Label->SetMinDesiredWidth(90.f); Label->SetJustification(ETextJustify::Center); Label->SetAutoWrapText(false);
        if (auto* Slot = Cast<UButtonSlot>(Label->Slot))
        { Slot->SetPadding(FMargin(0)); Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center); }
        if (!SaveVersion(Launcher)) return false;
    }
    return true;
}

void Apply()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_CODEX_NAVIGATION_FAIL requires idle Entry editor")); return; }
    auto* BP = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityCodexScreenPackage);
    auto* Card = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityCodexCardPackage);
    auto* Reward = LoadExistingBlueprint(RewardPackage);
    auto* Category = LoadExistingBlueprint(CategoryPackage);
    auto* Launcher = LoadExistingBlueprint(FFBuildUIAssetPaths::FacilityCodexCatalogLauncherPackage);
    if (!BP || !BP->WidgetTree || !Card || !Reward || !Category || !Launcher || !Category->GeneratedClass) return;
    // Complete all deletion checks before compiling, saving or changing any of
    // the four assets. A designer reference in one asset preserves the whole set.
    if (!CanRemoveRetiredBranches(*BP, {TEXT("Border_CodexDock"), TEXT("BackgroundBlur_Glass"),
            TEXT("Vertical_GlassDetail"), TEXT("Stack_GlassRecipe")}) ||
        !CanRemoveRetiredBranches(*Card, {TEXT("Size_Recipe"), TEXT("Horizontal_AuroraCardSummary")}) ||
        !CanRemoveRetiredBranches(*Reward, {TEXT("Vertical_MilestoneCard")}) ||
        !CanRemoveRetiredBranches(*BP, {TEXT("Scroll_ReadableMilestones")}, false)) return;
    if (!PrepareAuxiliaryAssets(*Card, *Reward, *Launcher))
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_CODEX_NAVIGATION_FAIL auxiliary widgets")); return; }
    if (BP->GetOutermost()->GetMetaData().GetValue(BP, Version) == TEXT("1"))
    { UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_CODEX_NAVIGATION_PASS already applied")); return; }
    auto& Tree = *BP->WidgetTree;
    // Fresh creation reaches this command directly from the modern authoring
    // pipeline. Existing readable WBPs keep their edited sizes and typography.
    if (!Tree.FindWidget(TEXT("Scroll_ReadableMilestones")))
    {
        if (!FFBuildReadability::Codex(*BP)) return;
        FFBuildReadability::CodexCard(*Card);
        if (!SaveVersion(*Card)) return;
    }
    auto* Canvas = Cast<UCanvasPanel>(Tree.FindWidget(TEXT("Canvas_CodexLayout")));
    auto* Track = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_ReadableMilestoneTrack")));
    auto* RewardViewport = Tree.FindWidget(TEXT("Scroll_ReadableMilestones"));
    auto* ProgressRow = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_CollectionProgress")));
    if (!Canvas || !Track || !RewardViewport || !ProgressRow)
    { UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_CODEX_NAVIGATION_FAIL requires readable collection layout")); return; }
    BP->Modify(); Tree.Modify();
    // Four authored reward stages fit the supported screen widths. A Fill
    // container makes the track span its parent in both Designer and gameplay;
    // a horizontal ScrollBox instead forces the child to its 520px desired size.
    if (auto* OldScroll = Cast<UScrollBox>(RewardViewport))
    {
        const int32 RewardIndex = ProgressRow->GetChildIndex(OldScroll);
        Track->RemoveFromParent();
        OldScroll->RemoveFromParent();
        BP->OnVariableRemoved(OldScroll->GetFName());
        Tree.RemoveWidget(OldScroll);
        OldScroll->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        auto* Fill = MakeWidget<USizeBox>(*BP, Tree, TEXT("Scroll_ReadableMilestones"));
        Fill->AddChild(Track);
        auto* FillSlot = ProgressRow->AddChildToHorizontalBox(Fill);
        FillSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        FillSlot->SetPadding(FMargin(12, 0, 0, 0));
        FillSlot->SetHorizontalAlignment(HAlign_Fill); FillSlot->SetVerticalAlignment(VAlign_Fill);
        ProgressRow->ShiftChild(RewardIndex, Fill);
        RewardViewport = Fill;
    }
    for (const TCHAR* Name : {TEXT("Border_CodexDock"), TEXT("BackgroundBlur_Glass"),
        TEXT("Vertical_GlassDetail"), TEXT("Stack_GlassRecipe")}) RemoveRetiredBranch(*BP, Name);
    auto* Exit = Cast<UButton>(Tree.FindWidget(TEXT("Button_ExitCodex")));
    if (!Exit) Exit = MakeWidget<UButton>(*BP, Tree, TEXT("Button_ExitCodex"), true);
    if (Exit->GetParent() != Canvas) { Exit->RemoveFromParent(); Canvas->AddChildToCanvas(Exit); }
    auto* Slot = CastChecked<UCanvasPanelSlot>(Exit->Slot);
    Slot->SetPosition(FVector2D(1824.f, 24.f)); Slot->SetSize(FVector2D(48.f, 48.f)); Slot->SetZOrder(20);
    auto Style = Exit->GetStyle();
    Style.Normal = FFBuildPearl::Rounded(FFBuildPearl::Pearl, 16.f, FFBuildPearl::Line, 1.f);
    Style.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Cyan, 16.f);
    Style.Pressed = FFBuildPearl::Rounded(FFBuildPearl::Color(180, 221, 238), 16.f);
    Style.NormalPadding = Style.PressedPadding = FMargin(0.f);
    Exit->SetStyle(Style); Exit->SetBackgroundColor(FLinearColor::White);
    Exit->SetToolTipText(NSLOCTEXT("FFCodexNavigation", "ExitHint", "도감 닫기 · 이전 화면으로"));
    auto* Label = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ExitCodex")));
    if (!Label) Label = MakeWidget<UTextBlock>(*BP, Tree, TEXT("Text_ExitCodex"));
    Label->SetText(FText::FromString(TEXT("×")));
    Label->SetFont(FFBuildTypography::Make(FFBuildTypography::ERole::Heading, 26));
    Label->SetColorAndOpacity(FSlateColor(FFBuildPearl::Ink));
    Label->SetJustification(ETextJustify::Center); Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (Exit->GetContent() != Label) { Label->RemoveFromParent(); Exit->AddChild(Label); }
    Track->ClearWidthOverride(); Track->ClearMaxDesiredWidth(); Track->ClearMinDesiredWidth();
    if (auto* TrackSlot = Cast<USizeBoxSlot>(Track->Slot))
    { TrackSlot->SetHorizontalAlignment(HAlign_Fill); TrackSlot->SetVerticalAlignment(VAlign_Fill); }
    // Save the actual responsive Canvas layout. These offsets are directly editable
    // in Designer and no longer get replaced by NativeTick.
    Anchor(Tree, TEXT("Size_Refined_Button_Close"), FAnchors(.025f, 0.f), FMargin(0, 24, 330, 48));
    Anchor(Tree, TEXT("Button_ExitCodex"), FAnchors(.975f, 0.f), FMargin(0, 24, 52, 52), FVector2D(1, 0));
    // Give Korean navigation/search a stable readable width; the card area
    // consumes the remaining space through stretch anchors at every resolution.
    Anchor(Tree, TEXT("Horizontal_Toolbar"), FAnchors(.025f, 0.f, .975f, 0.f), FMargin(194, 84, 0, 44));
    Anchor(Tree, TEXT("Size_CollectionSearch"), FAnchors(.025f, 0.f), FMargin(0, 84, 176, 44));
    Anchor(Tree, TEXT("Scroll_CollectionCategories"), FAnchors(.025f, 0.f, .025f, 1.f), FMargin(0, 144, 176, 24));
    Anchor(Tree, TEXT("Size_Browse"), FAnchors(.025f, 0.f, .975f, 1.f), FMargin(194, 144, 0, 220));
    Anchor(Tree, TEXT("Row_CollectionProgress"), FAnchors(.025f, 1.f, .975f, 1.f), FMargin(194, -208, 0, 184));
    Anchor(Tree, TEXT("Size_Detail"), FAnchors(.975f, 0.f, .975f, 1.f), FMargin(0, 112, 620, 24), FVector2D(1, 0));
    Anchor(Tree, TEXT("Size_Showcase"), FAnchors(.025f, 0.f, .975f, 1.f), FMargin(0, 112, 644, 176));
    Anchor(Tree, TEXT("Button_CollectionPrevious"), FAnchors(.975f, 0.f), FMargin(-240, 26, 48, 44));
    Anchor(Tree, TEXT("Button_CollectionNext"), FAnchors(.975f, 0.f), FMargin(-176, 26, 48, 44));
    if (auto* Dialog = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_Dialog"))))
    {
        Dialog->ClearWidthOverride(); Dialog->ClearHeightOverride();
        Dialog->ClearMinDesiredWidth(); Dialog->ClearMinDesiredHeight();
        Dialog->ClearMaxDesiredWidth(); Dialog->ClearMaxDesiredHeight();
    }
    // Clearing the old fixed size is insufficient while an ancestor centers
    // its desired size. Save a continuous fill chain down to the Canvas.
    for (const TCHAR* Name : {TEXT("Border_InputShield"), TEXT("Size_Dialog"),
        TEXT("Overlay_GlassScreen"), TEXT("Border_Paper"), TEXT("Canvas_CodexLayout")})
        if (UWidget* Widget = Tree.FindWidget(Name))
        {
            if (auto* Border = Cast<UBorder>(Widget))
            { Border->SetPadding(FMargin(0)); Border->SetHorizontalAlignment(HAlign_Fill); Border->SetVerticalAlignment(VAlign_Fill); }
            if (auto* BorderSlot = Cast<UBorderSlot>(Widget->Slot))
            { BorderSlot->SetPadding(FMargin(0)); BorderSlot->SetHorizontalAlignment(HAlign_Fill); BorderSlot->SetVerticalAlignment(VAlign_Fill); }
            if (auto* SizeSlot = Cast<USizeBoxSlot>(Widget->Slot))
            { SizeSlot->SetPadding(FMargin(0)); SizeSlot->SetHorizontalAlignment(HAlign_Fill); SizeSlot->SetVerticalAlignment(VAlign_Fill); }
            if (auto* OverlaySlot = Cast<UOverlaySlot>(Widget->Slot))
            { OverlaySlot->SetPadding(FMargin(0)); OverlaySlot->SetHorizontalAlignment(HAlign_Fill); OverlaySlot->SetVerticalAlignment(VAlign_Fill); }
        }
    if (auto* Search = Cast<UEditableTextBox>(Tree.FindWidget(TEXT("Search_Entries"))))
    { Search->SetMinDesiredWidth(0.f); Search->SetHintText(FText::GetEmpty()); Search->SetClipping(EWidgetClipping::ClipToBounds); }
    if (auto* Hint = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_SearchHint"))))
    { Hint->SetText(NSLOCTEXT("FFCodexNavigation", "SearchHint", "시설·조합 검색")); Hint->SetAutoWrapText(false); Hint->SetClipping(EWidgetClipping::ClipToBounds); }
    if (auto* Overlay = Tree.FindWidget(TEXT("Overlay_SearchHint"))) Overlay->SetClipping(EWidgetClipping::ClipToBounds);
    if (auto* SearchBounds = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_Refined_Search_Entries"))))
    { SearchBounds->ClearWidthOverride(); SearchBounds->ClearMinDesiredWidth(); SearchBounds->ClearMaxDesiredWidth(); }
    auto* Categories = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Column_CollectionCategories")));
    auto* Milestones = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_CollectionMilestones")));
    if (!Categories || !Milestones || !Reward->GeneratedClass) return;
    if (auto* MilestoneSlot = Cast<UOverlaySlot>(Milestones->Slot))
    { MilestoneSlot->SetHorizontalAlignment(HAlign_Fill); MilestoneSlot->SetVerticalAlignment(VAlign_Center); }
    if (auto* Tiles = Cast<UFF_FacilityCodexTileView>(Tree.FindWidget(TEXT("TileView_Entries"))))
    {
        Tiles->ColumnCount = 6;
        if (auto* PreviewCount = FindFProperty<FIntProperty>(Tiles->GetClass(), TEXT("NumDesignerPreviewEntries")))
            PreviewCount->SetPropertyValue_InContainer(Tiles, 12);
    }
    if (Categories->GetChildrenCount() == 0)
    {
        auto* All = MakeWidgetOfClass(*BP, Tree, TEXT("Widget_CollectionAll"), Category->GeneratedClass, false);
        Categories->AddChildToVerticalBox(All)->SetPadding(FMargin(0, 0, 0, 12));
    }
    // Saved sample controls expose the real repeated reward template to Designer.
    // Runtime reuses them when the configured count matches; different data can rebuild safely.
    if (Milestones->GetChildrenCount() == 0)
    {
        const int32 Count = GetDefault<UFF_FacilityCodexSettings>()->CollectionRewards.Num();
        for (int32 Index = 0; Index < Count; ++Index)
        {
            auto* RewardWidget = MakeWidgetOfClass(*BP, Tree, *FString::Printf(TEXT("Widget_CollectionReward%d"), Index), Reward->GeneratedClass, false);
            auto* RewardSlot = Milestones->AddChildToHorizontalBox(RewardWidget);
            RewardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); RewardSlot->SetPadding(FMargin(12, 0));
            RewardSlot->SetHorizontalAlignment(HAlign_Center);
        }
    }
    if (auto* Count = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_CollectionCount"))))
        Count->SetText(NSLOCTEXT("FFCodexNavigation", "PreviewCount", "0 / 0"));
    for (const TCHAR* Name : {TEXT("Size_Detail"), TEXT("Size_Showcase"), TEXT("Horizontal_PreviewRotation"),
        TEXT("Button_CollectionPrevious"), TEXT("Button_CollectionNext"), TEXT("Text_Empty"), TEXT("Border_RewardFeedback")})
        if (auto* Widget = Tree.FindWidget(Name)) Widget->SetVisibility(ESlateVisibility::Collapsed);
    if (!SaveVersion(*BP)) return;
    auto* Defaults = Cast<UFF_FacilityCodexScreen>(BP->GeneratedClass->GetDefaultObject());
    auto* AuthoredFlag = FindFProperty<FBoolProperty>(BP->GeneratedClass, TEXT("bUseAuthoredCollectionLayout"));
    if (!Defaults || !AuthoredFlag) return;
    AuthoredFlag->SetPropertyValue_InContainer(Defaults, true);
    Defaults->DesignTimeSize = FVector2D(1920, 1080);
    Defaults->DesignSizeMode = EDesignPreviewSizeMode::Custom;
    if (!SaveAssetPackage(TEXT("FF_CODEX_NAVIGATION"), *BP)) return;
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_CODEX_NAVIGATION_PASS"));
}
FAutoConsoleCommand Command(TEXT("FF.ApplyCodexNavigation"),
    TEXT("Migrate the codex to editable Designer layouts, removing retired branches once and preserving later edits."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
