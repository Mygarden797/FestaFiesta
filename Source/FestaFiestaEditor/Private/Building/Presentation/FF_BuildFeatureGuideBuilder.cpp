// Add the feature guide beside the current authored help button without rebuilding the toolbar.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Building/Presentation/FF_BuildFeatureGuide.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildTypography.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFBuildFeatureGuide"

namespace FFBuildFeatureGuideBuilder
{
using namespace FFUIWbpBuilder;
constexpr const TCHAR* Prefix = TEXT("FF_BUILD_FEATURE_GUIDE");
constexpr const TCHAR* PreviousVersion = TEXT("FF.BuildFeatureGuide.20261008.v1");
constexpr const TCHAR* Version = TEXT("FF.BuildFeatureGuide.20261008.v2");

const TCHAR* const NewNames[] = {
    TEXT("Size_FeatureGuideToolbar"), TEXT("Button_FeatureGuide"), TEXT("Text_FeatureGuide"),
    TEXT("Column_FeatureGuideToolbar"), TEXT("Size_FeatureGuideToolbarIcon"),
    TEXT("Border_FeatureGuideToolbarIcon"), TEXT("Text_FeatureGuideToolbarIcon"),
    TEXT("Panel_FeatureGuide"), TEXT("Size_FeatureGuideCard"), TEXT("Border_FeatureGuideCard"),
    TEXT("Column_FeatureGuide"), TEXT("Row_FeatureGuideHeader"),
    TEXT("Text_FeatureGuideHeading"), TEXT("Button_FeatureGuideClose"), TEXT("Text_FeatureGuideClose"),
    TEXT("Combo_FeatureGuideTopic"), TEXT("Text_FeatureGuideTitle"), TEXT("Scroll_FeatureGuideBody"),
    TEXT("Text_FeatureGuideBody"), TEXT("Text_FeatureGuideHint"), TEXT("Size_FeatureGuideConfirm"),
    TEXT("Button_FeatureGuideConfirm"), TEXT("Text_FeatureGuideConfirm")
};

template<class T> T* Get(UWidgetTree& Tree, const TCHAR* Name)
{
    return Cast<T>(Tree.FindWidget(Name));
}

UWidget* ToolbarCell(UHorizontalBox& Row, UWidget& Button)
{
    UWidget* Cell = &Button;
    while (Cell && Cell->GetParent() != &Row) Cell = Cell->GetParent();
    return Cell;
}

UTextBlock* Text(UWidgetBlueprint& BP, const TCHAR* Name, const FText& Value,
    FFBuildTypography::ERole Role, int32 Size, FLinearColor Ink = FFBuildPearl::Ink,
    bool bVariable = false)
{
    auto* Label = MakeWidget<UTextBlock>(BP, *BP.WidgetTree, Name, bVariable);
    Label->SetText(Value);
    Label->SetFont(FFBuildTypography::Make(Role, Size));
    Label->SetColorAndOpacity(FSlateColor(Ink));
    Label->SetAutoWrapText(true);
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    return Label;
}

UFF_BuildToolsActionButton* Button(UWidgetBlueprint& BP, const TCHAR* Name,
    const TCHAR* LabelName, const FText& Caption, bool bPrimary = false)
{
    auto* Result = MakeWidget<UFF_BuildToolsActionButton>(BP, *BP.WidgetTree, Name, true);
    Result->ConfigureFocusable(true);
    FButtonStyle Style;
    const FLinearColor Teal = FFBuildPearl::Color(45, 118, 129);
    Style.Normal = FFBuildPearl::Rounded(bPrimary ? Teal : FFBuildPearl::Color(239, 243, 250),
        10.f, bPrimary ? Teal : FFBuildPearl::Line, 1.f);
    Style.Hovered = FFBuildPearl::Rounded(bPrimary ? FFBuildPearl::Color(34, 98, 108) :
        FFBuildPearl::Color(219, 237, 247), 10.f, Teal, 1.f);
    Style.Pressed = FFBuildPearl::Rounded(bPrimary ? FFBuildPearl::Color(27, 84, 93) :
        FFBuildPearl::Color(200, 226, 239), 10.f, Teal, 1.f);
    Style.Disabled = Style.Normal;
    Style.NormalPadding = Style.PressedPadding = FMargin(20.f, 10.f);
    Result->SetStyle(Style);
    Result->SetCursor(EMouseCursor::Hand);
    auto* Label = Text(BP, LabelName, Caption, FFBuildTypography::ERole::Label,
        16, bPrimary ? FLinearColor::White : FFBuildPearl::Ink);
    Label->SetAutoWrapText(false);
    Label->SetJustification(ETextJustify::Center);
    auto* Slot = CastChecked<UButtonSlot>(Result->AddChild(Label));
    Slot->SetHorizontalAlignment(HAlign_Center);
    Slot->SetVerticalAlignment(VAlign_Center);
    return Result;
}

void StyleCombo(UComboBoxString& Combo)
{
    const FLinearColor Light = FFBuildPearl::Color(244, 247, 253);
    const FLinearColor Highlight = FFBuildPearl::Color(219, 239, 247);
    FComboBoxStyle Style = Combo.GetWidgetStyle();
    auto& Face = Style.ComboButtonStyle.ButtonStyle;
    Face.Normal = FFBuildPearl::Rounded(Light, 10.f, FFBuildPearl::Line, 1.f);
    Face.Hovered = Face.Pressed = FFBuildPearl::Rounded(Highlight, 10.f, FFBuildPearl::Cyan, 1.f);
    Face.Disabled = Face.Normal;
    Face.NormalForeground = Face.HoveredForeground = Face.PressedForeground = FFBuildPearl::Ink;
    Style.ComboButtonStyle.MenuBorderBrush = FFBuildPearl::Rounded(Light, 10.f, FFBuildPearl::Line, 1.f);
    Style.ComboButtonStyle.MenuBorderPadding = FMargin(6.f);
    Style.ComboButtonStyle.DownArrowImage.TintColor = FFBuildPearl::Ink;
    Combo.SetWidgetStyle(Style);
    Combo.SetContentPadding(FMargin(14.f, 10.f));
    Combo.SetMaxListHeight(260.f);
    FTableRowStyle Item = Combo.GetItemStyle();
    Item.EvenRowBackgroundBrush = Item.OddRowBackgroundBrush = FFBuildPearl::Rounded(Light, 6.f);
    Item.EvenRowBackgroundHoveredBrush = Item.OddRowBackgroundHoveredBrush = FFBuildPearl::Rounded(Highlight, 6.f);
    Item.ActiveBrush = Item.InactiveBrush = Item.ActiveHoveredBrush = Item.InactiveHoveredBrush =
        FFBuildPearl::Rounded(Highlight, 6.f);
    Item.TextColor = Item.SelectedTextColor = FFBuildPearl::Ink;
    Combo.SetItemStyle(Item);
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Combo.Font = FFBuildTypography::Make(FFBuildTypography::ERole::Body, 17);
    Combo.ForegroundColor = FFBuildPearl::Ink;
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

UBorder* MakeGuide(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    auto* Panel = MakeWidget<UBorder>(BP, Tree, TEXT("Panel_FeatureGuide"), true);
    Panel->SetBrush(FFBuildPearl::Rounded(FLinearColor(0.f, 0.f, 0.f, .22f), 0.f));
    Panel->SetBrushColor(FLinearColor::White);
    Panel->SetPadding(FMargin(0.f));
    Panel->SetHorizontalAlignment(HAlign_Center);
    Panel->SetVerticalAlignment(VAlign_Center);
    Panel->SetVisibility(ESlateVisibility::Collapsed);
    Panel->bHiddenInDesigner = true;
    auto* CardSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_FeatureGuideCard"));
    CardSize->SetWidthOverride(640.f);
    CardSize->SetHeightOverride(620.f);
    Panel->AddChild(CardSize);
    auto* Card = MakeWidget<UBorder>(BP, Tree, TEXT("Border_FeatureGuideCard"));
    Card->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(253, 254, 255), 20.f,
        FFBuildPearl::Color(188, 207, 222), 1.f));
    Card->SetBrushColor(FLinearColor::White);
    Card->SetPadding(FMargin(24.f));
    CardSize->AddChild(Card);
    auto* Column = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_FeatureGuide"));
    Card->AddChild(Column);
    auto* Header = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_FeatureGuideHeader"));
    Column->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
    auto* Heading = Text(BP, TEXT("Text_FeatureGuideHeading"), LOCTEXT("Heading", "건축 기능 안내"),
        FFBuildTypography::ERole::Heading, 24);
    auto* HeadingSlot = Header->AddChildToHorizontalBox(Heading);
    HeadingSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    HeadingSlot->SetVerticalAlignment(VAlign_Center);
    auto* CloseSlot = Header->AddChildToHorizontalBox(Button(BP, TEXT("Button_FeatureGuideClose"),
        TEXT("Text_FeatureGuideClose"), LOCTEXT("Close", "닫기")));
    CloseSlot->SetVerticalAlignment(VAlign_Center);
    auto* Topic = MakeWidget<UComboBoxString>(BP, Tree, TEXT("Combo_FeatureGuideTopic"), true);
    StyleCombo(*Topic);
    Topic->SetToolTipText(LOCTEXT("PickTopic", "설명을 다시 확인할 기능을 선택하세요."));
    Column->AddChildToVerticalBox(Topic)->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
    auto* Title = Text(BP, TEXT("Text_FeatureGuideTitle"), LOCTEXT("Title", "시설 조명"),
        FFBuildTypography::ERole::Heading, 22, FFBuildPearl::Ink, true);
    Column->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
    auto* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Scroll_FeatureGuideBody"));
    Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
    auto* ScrollSlot = Column->AddChildToVerticalBox(Scroll);
    ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ScrollSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    auto* Body = Text(BP, TEXT("Text_FeatureGuideBody"), LOCTEXT("Body", "기능을 선택해 사용 방법을 확인하세요."),
        FFBuildTypography::ERole::Body, 18, FFBuildPearl::Ink, true);
    Body->SetWrapTextAt(568.f);
    Scroll->AddChild(Body);
    auto* Hint = Text(BP, TEXT("Text_FeatureGuideHint"), LOCTEXT("Hint", "상단의 안내에서 언제든 다시 확인할 수 있어요."),
        FFBuildTypography::ERole::Body, 15, FFBuildPearl::Muted, true);
    Column->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    auto* ConfirmSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_FeatureGuideConfirm"));
    ConfirmSize->SetMinDesiredHeight(46.f);
    ConfirmSize->AddChild(Button(BP, TEXT("Button_FeatureGuideConfirm"),
        TEXT("Text_FeatureGuideConfirm"), LOCTEXT("Confirm", "확인"), true));
    Column->AddChildToVerticalBox(ConfirmSize);
    return Panel;
}

bool Verify(UWidgetBlueprint& BP, const float CardHeight = 620.f)
{
    auto& Tree = *BP.WidgetTree;
    auto* Row = Get<UHorizontalBox>(Tree, TEXT("Row_BuildReferenceTopBar"));
    auto* Help = Get<UButton>(Tree, TEXT("Button_ToolbarHelp"));
    auto* Cell = Get<USizeBox>(Tree, TEXT("Size_FeatureGuideToolbar"));
    auto* Open = Get<UButton>(Tree, TEXT("Button_FeatureGuide"));
    auto* Panel = Get<UBorder>(Tree, TEXT("Panel_FeatureGuide"));
    auto* PanelSlot = Panel ? Cast<UCanvasPanelSlot>(Panel->Slot) : nullptr;
    auto* CardSize = Get<USizeBox>(Tree, TEXT("Size_FeatureGuideCard"));
    for (const TCHAR* Name : NewNames) if (!Tree.FindWidget(Name)) return false;
    return Row && Help && Cell && Open && Cell->GetContent() == Open &&
        Cell->GetParent() == Row && Row->GetChildIndex(Cell) == Row->GetChildIndex(ToolbarCell(*Row, *Help)) + 1 &&
        PanelSlot && PanelSlot->GetAnchors() == FAnchors(0.f, 0.f, 1.f, 1.f) && PanelSlot->GetZOrder() >= 100 &&
        CardSize && CardSize->GetWidthOverride() == 640.f && CardSize->GetHeightOverride() == CardHeight &&
        Panel->GetVisibility() == ESlateVisibility::Collapsed &&
        Get<UComboBoxString>(Tree, TEXT("Combo_FeatureGuideTopic")) &&
        Get<UTextBlock>(Tree, TEXT("Text_FeatureGuideTitle")) && Get<UTextBlock>(Tree, TEXT("Text_FeatureGuideBody")) &&
        Get<UTextBlock>(Tree, TEXT("Text_FeatureGuideHint")) &&
        Get<UButton>(Tree, TEXT("Button_FeatureGuideClose")) && Get<UButton>(Tree, TEXT("Button_FeatureGuideConfirm"));
}

bool Migrate(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, Version) == TEXT("1"))
    {
        if (!Verify(BP)) return false;
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: current guide preserved; no save"), Prefix);
        return true;
    }
    if (Metadata.GetValue(&BP, PreviousVersion) == TEXT("1"))
    {
        // The first guide is already authored. Increase only its card height so longer
        // Korean explanations are readable without replacing any content, style or slot.
        if (!Verify(BP, 480.f)) return false;
        auto* CardSize = Get<USizeBox>(*BP.WidgetTree, TEXT("Size_FeatureGuideCard"));
        BP.Modify();
        CardSize->Modify();
        CardSize->SetHeightOverride(620.f);
        if (!Verify(BP)) return false;
        Metadata.SetValue(&BP, Version, TEXT("1"));
        if (SaveNewBlueprint(Prefix, BP)) return true;
        Metadata.RemoveValue(&BP, Version);
        return false;
    }
    auto& Tree = *BP.WidgetTree;
    auto* Canvas = Get<UCanvasPanel>(Tree, TEXT("Canvas_BuildClarity"));
    auto* Row = Get<UHorizontalBox>(Tree, TEXT("Row_BuildReferenceTopBar"));
    auto* TopSize = Get<USizeBox>(Tree, TEXT("Size_BuildTopBar"));
    auto* Help = Get<UButton>(Tree, TEXT("Button_ToolbarHelp"));
    auto* HelpLabel = Get<UTextBlock>(Tree, TEXT("Label_PearlTool7"));
    auto* HelpCell = Row && Help ? Cast<USizeBox>(ToolbarCell(*Row, *Help)) : nullptr;
    auto* HelpSlot = HelpCell ? Cast<UHorizontalBoxSlot>(HelpCell->Slot) : nullptr;
    if (!Canvas || !Row || !TopSize || !HelpCell || !HelpSlot || !HelpLabel ||
        !TopSize->GetWidthOverride() || !HelpCell->GetWidthOverride())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: current toolbar contract missing; no changes"), Prefix);
        return false;
    }
    for (const TCHAR* Name : NewNames)
        if (Tree.FindWidget(Name))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: unversioned widget %s; no changes"), Prefix, Name);
            return false;
        }
    BP.Modify(); Tree.Modify(); Row->Modify(); TopSize->Modify(); Canvas->Modify();
    auto* Cell = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_FeatureGuideToolbar"));
    Cell->SetWidthOverride(HelpCell->GetWidthOverride());
    Cell->SetHeightOverride(HelpCell->GetHeightOverride());
    auto* Open = Button(BP, TEXT("Button_FeatureGuide"), TEXT("Text_FeatureGuide"), LOCTEXT("Open", "안내"));
    Open->ConfigureFocusable(false);
    // The new control inherits the user's current toolbar palette, not an obsolete builder's palette.
    Open->SetStyle(Help->GetStyle());
    Open->SetBackgroundColor(Help->GetBackgroundColor());
    Open->SetColorAndOpacity(Help->GetColorAndOpacity());
    // This new button is not attached to the root tree yet; retain its content directly.
    auto* Caption = CastChecked<UTextBlock>(Open->GetContent());
    Caption->SetFont(HelpLabel->GetFont());
    Caption->SetColorAndOpacity(HelpLabel->GetColorAndOpacity());
    Open->ClearChildren();
    auto* ToolbarColumn = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_FeatureGuideToolbar"));
    ToolbarColumn->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* IconBounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_FeatureGuideToolbarIcon"));
    IconBounds->SetWidthOverride(32.f);
    IconBounds->SetHeightOverride(32.f);
    auto* IconSlot = ToolbarColumn->AddChildToVerticalBox(IconBounds);
    IconSlot->SetHorizontalAlignment(HAlign_Center);
    IconSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
    auto* Icon = MakeWidget<UBorder>(BP, Tree, TEXT("Border_FeatureGuideToolbarIcon"));
    Icon->SetBrush(FFBuildPearl::Rounded(FLinearColor::Transparent, 16.f,
        HelpLabel->GetColorAndOpacity().GetSpecifiedColor(), 2.f));
    Icon->SetPadding(FMargin(0.f));
    Icon->SetHorizontalAlignment(HAlign_Center);
    Icon->SetVerticalAlignment(VAlign_Center);
    IconBounds->AddChild(Icon);
    auto* Mark = Text(BP, TEXT("Text_FeatureGuideToolbarIcon"), FText::FromString(TEXT("i")),
        FFBuildTypography::ERole::Heading, 20, HelpLabel->GetColorAndOpacity().GetSpecifiedColor());
    Mark->SetAutoWrapText(false);
    Icon->AddChild(Mark);
    ToolbarColumn->AddChildToVerticalBox(Caption);
    auto* OpenSlot = CastChecked<UButtonSlot>(Open->AddChild(ToolbarColumn));
    OpenSlot->SetHorizontalAlignment(HAlign_Center);
    OpenSlot->SetVerticalAlignment(VAlign_Center);
    Open->SetToolTipText(LOCTEXT("OpenTip", "시설 조명, 다중 선택, 조합 도감 등 건축 기능의 사용 방법을 다시 확인합니다."));
    Cell->AddChild(Open);
    auto* CellSlot = Row->AddChildToHorizontalBox(Cell);
    FMargin CellPadding = HelpSlot->GetPadding();
    CellPadding.Left += 8.f;
    CellSlot->SetPadding(CellPadding);
    CellSlot->SetHorizontalAlignment(HelpSlot->GetHorizontalAlignment());
    CellSlot->SetVerticalAlignment(HelpSlot->GetVerticalAlignment());
    CellSlot->SetSize(HelpSlot->GetSize());
    Row->ShiftChild(Row->GetChildIndex(HelpCell) + 1, Cell);
    const FMargin Padding = CellSlot->GetPadding();
    TopSize->SetWidthOverride(TopSize->GetWidthOverride() + Cell->GetWidthOverride() + Padding.Left + Padding.Right);

    auto* PanelSlot = Canvas->AddChildToCanvas(MakeGuide(BP));
    PanelSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    PanelSlot->SetOffsets(FMargin(0.f));
    PanelSlot->SetAutoSize(false);
    PanelSlot->SetZOrder(100);
    if (!Verify(BP)) return false;
    Metadata.SetValue(&BP, Version, TEXT("1"));
    if (SaveNewBlueprint(Prefix, BP)) return true;
    Metadata.RemoveValue(&BP, Version);
    return false;
}

void Apply()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: requires idle Entry editor"), Prefix);
        return;
    }
    auto* BP = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildToolsPackage);
    if (BP && BP->GetOutermost()->IsDirty())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: save or discard unsaved BuildTools edits before migration"), Prefix);
        return;
    }
    if (!BP || !BP->WidgetTree || BP->ParentClass != UFF_BuildToolsWidget::StaticClass() || !Migrate(*BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: saved BuildTools migration failed"), Prefix);
        return;
    }
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_PASS: guide and toolbar entry ready"), Prefix);
}

FAutoConsoleCommand Command(TEXT("FF.MigrateBuildFeatureGuide"),
    TEXT("Add the feature guide to the saved BuildTools WBP, preserving existing authored widgets."),
    FConsoleCommandDelegate::CreateStatic(&Apply));

namespace FeatureBadges
{
constexpr const TCHAR* BadgePrefix = TEXT("FF_BUILD_FEATURE_BADGES");
constexpr const TCHAR* PreviousBadgeVersion = TEXT("FF.BuildFeatureBadges.20261008.v1");
constexpr const TCHAR* BadgeVersion = TEXT("FF.BuildFeatureBadges.20261009.v2");

FName BadgeName(const FFBuildFeatureGuide::FTopic& Topic)
{
    return FName(*FString::Printf(TEXT("Badge_%s"), *Topic.Id.ToString()));
}

FName MarkName(const FFBuildFeatureGuide::FTopic& Topic)
{
    return FName(*FString::Printf(TEXT("Text_Badge_%s"), *Topic.Id.ToString()));
}

UCanvasPanel* IconCanvas(UWidgetTree& Tree, const FFBuildFeatureGuide::FTopic& Topic)
{
    const auto* Bounds = Cast<USizeBox>(Tree.FindWidget(Topic.ToolbarIcon));
    return Bounds ? Cast<UCanvasPanel>(Bounds->GetContent()) : nullptr;
}

bool VerifyBadges(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    int32 Count = 0;
    for (const auto& Topic : FFBuildFeatureGuide::Topics())
    {
        if (Topic.ToolbarButton.IsNone()) continue;
        const auto* Canvas = IconCanvas(Tree, Topic);
        const auto* Badge = Cast<UBorder>(Tree.FindWidget(BadgeName(Topic)));
        const auto* Mark = Cast<UTextBlock>(Tree.FindWidget(MarkName(Topic)));
        const auto* Slot = Badge ? Cast<UCanvasPanelSlot>(Badge->Slot) : nullptr;
        if (!Canvas || !Badge || !Mark || !Slot || Badge->GetParent() != Canvas ||
            Badge->GetContent() != Mark || !Mark->GetText().IsEmpty() ||
            Badge->Background.DrawAs != ESlateBrushDrawType::RoundedBox ||
            !Badge->Background.TintColor.GetSpecifiedColor().Equals(FLinearColor::Red) ||
            Badge->Background.OutlineSettings.Width != 0.f ||
            Badge->GetVisibility() != ESlateVisibility::Collapsed ||
            Mark->GetVisibility() != ESlateVisibility::Collapsed ||
            !Slot->GetPosition().Equals(FVector2D(32.f, -3.f)) ||
            !Slot->GetSize().Equals(FVector2D(18.f, 18.f)) || Slot->GetZOrder() < 10)
            return false;
        ++Count;
    }
    return Count == 7;
}

bool MigrateBadges(UWidgetBlueprint& BP)
{
    auto& Tree = *BP.WidgetTree;
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, BadgeVersion) == TEXT("1"))
    {
        if (!VerifyBadges(BP)) return false;
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_REUSE: existing badges preserved; no save"), BadgePrefix);
        return true;
    }
    const bool bUpgradeExisting = Metadata.GetValue(&BP, PreviousBadgeVersion) == TEXT("1");
    // Validate every target before updating badges or adding siblings to the artwork.
    for (const auto& Topic : FFBuildFeatureGuide::Topics())
    {
        if (Topic.ToolbarButton.IsNone()) continue;
        const auto* Badge = Cast<UBorder>(Tree.FindWidget(BadgeName(Topic)));
        const auto* Mark = Cast<UTextBlock>(Tree.FindWidget(MarkName(Topic)));
        const bool bValidExisting = Badge && Mark && Badge->GetContent() == Mark &&
            Badge->GetParent() == IconCanvas(Tree, Topic) && Cast<UCanvasPanelSlot>(Badge->Slot);
        if (!IconCanvas(Tree, Topic) || !Cast<UButton>(Tree.FindWidget(Topic.ToolbarButton)) ||
            (bUpgradeExisting ? !bValidExisting :
                (Tree.FindWidget(BadgeName(Topic)) || Tree.FindWidget(MarkName(Topic)))))
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: missing icon canvas or unversioned badge for %s; no changes"),
                BadgePrefix, *Topic.Id.ToString());
            return false;
        }
    }
    BP.Modify();
    Tree.Modify();
    for (const auto& Topic : FFBuildFeatureGuide::Topics())
    {
        if (Topic.ToolbarButton.IsNone()) continue;
        auto* Canvas = IconCanvas(Tree, Topic);
        Canvas->Modify();
        auto* Badge = bUpgradeExisting ? Cast<UBorder>(Tree.FindWidget(BadgeName(Topic))) :
            MakeWidget<UBorder>(BP, Tree, BadgeName(Topic));
        Badge->Modify();
        Badge->SetBrush(FFBuildPearl::Rounded(FLinearColor::Red, 9.f,
            FLinearColor::Transparent, 0.f));
        Badge->SetBrushColor(FLinearColor::White);
        Badge->SetPadding(FMargin(0.f));
        Badge->SetHorizontalAlignment(HAlign_Center);
        Badge->SetVerticalAlignment(VAlign_Center);
        Badge->SetVisibility(ESlateVisibility::Collapsed);
        auto* Mark = bUpgradeExisting ? Cast<UTextBlock>(Tree.FindWidget(MarkName(Topic))) :
            Text(BP, *MarkName(Topic).ToString(), FText::GetEmpty(),
                FFBuildTypography::ERole::Heading, 14, FLinearColor::White);
        Mark->Modify();
        Mark->SetText(FText::GetEmpty());
        Mark->SetVisibility(ESlateVisibility::Collapsed);
        Mark->SetAutoWrapText(false);
        Mark->SetJustification(ETextJustify::Center);
        if (!bUpgradeExisting) Badge->AddChild(Mark);
        auto* Slot = bUpgradeExisting ? Cast<UCanvasPanelSlot>(Badge->Slot) : Canvas->AddChildToCanvas(Badge);
        Slot->Modify();
        Slot->SetPosition(FVector2D(32.f, -3.f));
        Slot->SetSize(FVector2D(18.f, 18.f));
        Slot->SetZOrder(10);
    }
    if (!VerifyBadges(BP)) return false;
    Metadata.SetValue(&BP, BadgeVersion, TEXT("1"));
    if (SaveNewBlueprint(BadgePrefix, BP)) return true;
    Metadata.RemoveValue(&BP, BadgeVersion);
    return false;
}

void ApplyBadges()
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->GetOutermost()->GetName() != TEXT("/Engine/Maps/Entry"))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: requires idle Entry editor"), BadgePrefix);
        return;
    }
    auto* BP = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildToolsPackage);
    if (BP && BP->GetOutermost()->IsDirty())
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: save or discard unsaved BuildTools edits before migration"), BadgePrefix);
        return;
    }
    if (!BP || !BP->WidgetTree || BP->ParentClass != UFF_BuildToolsWidget::StaticClass() || !MigrateBadges(*BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("%s_FAIL: saved BuildTools badge migration failed"), BadgePrefix);
        return;
    }
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_PASS: seven first-use badges ready; original icons preserved"), BadgePrefix);
}

FAutoConsoleCommand BadgeCommand(TEXT("FF.MigrateBuildFeatureBadges"),
    TEXT("Apply first-use red dot badges to existing toolbar icon canvases without changing their artwork."),
    FConsoleCommandDelegate::CreateStatic(&ApplyBadges));
}
}

#undef LOCTEXT_NAMESPACE
#endif
