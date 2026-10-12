// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// Adds a compact top build toolbar and a contextual settings drawer to the saved UMG widget.
// The existing Clarity tree, bindings, placement summary and confirm controls are preserved.
#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Texture2D.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "FFBuildReferenceTools"

namespace FFBuildReferenceTools
{
using namespace FFUIWbpBuilder;

constexpr const TCHAR* VersionKey = TEXT("FF.BuildReferenceTools.V1");
constexpr const TCHAR* PresentationKey = TEXT("FF.BuildReferenceTools.PresentationV2");
constexpr const TCHAR* NavigationSettingsKey = TEXT("FF.BuildReferenceTools.NavigationSettingsV3");
constexpr const TCHAR* PearlToolbarKey = TEXT("FF.BuildReferenceTools.PearlToolbarV4");
constexpr const TCHAR* PearlPolishKey = TEXT("FF.BuildReferenceTools.PearlPolishV5");
constexpr const TCHAR* PearlReadableCardsKey = TEXT("FF.BuildReferenceTools.PearlReadableCardsV6");
constexpr const TCHAR* IconHelpKey = TEXT("FF.BuildReferenceTools.IconHelpV7");
constexpr const TCHAR* ActionLanguageKey = TEXT("FF.BuildReferenceTools.ActionLanguageV8");
constexpr const TCHAR* KeyboardHelpKey = TEXT("FF.BuildReferenceTools.KeyboardHelpV9");
constexpr const TCHAR* ConstructionHelpKey = TEXT("FF.BuildReferenceTools.ConstructionHelpV10");
constexpr const TCHAR* QueueShortcutKey = TEXT("FF.BuildReferenceTools.QueueShortcutV11");
constexpr const TCHAR* NearbyShortcutKey = TEXT("FF.BuildNearbyShortcut.20261001.v1");
const FLinearColor White = FLinearColor::White;

FSlateBrush Surface(const FLinearColor Color, const float Radius, const FLinearColor Outline = FLinearColor::Transparent)
{
    FSlateBrush Brush;
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.TintColor = FSlateColor(Color);
    Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
    Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
    Brush.OutlineSettings.Color = FSlateColor(Outline);
    Brush.OutlineSettings.Width = Outline.A > 0.0f ? 1.0f : 0.0f;
    return Brush;
}

UTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, const FName Name, const FText Value,
    const int32 Size, const FLinearColor Color, const bool bVariable = false)
{
    UTextBlock* Text = MakeWidget<UTextBlock>(BP, Tree, Name, bVariable);
    StyleText(*Text, Size, Color, TEXT("SemiBold"));
    Text->SetText(Value);
    Text->SetVisibility(ESlateVisibility::HitTestInvisible);
    return Text;
}

void AddRow(UHorizontalBox& Row, UWidget& Child, const float Right = 6.0f, const bool bFill = false)
{
    UHorizontalBoxSlot* Slot = Row.AddChildToHorizontalBox(&Child);
    Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    Slot->SetVerticalAlignment(VAlign_Center);
    Slot->SetPadding(FMargin(0.0f, 0.0f, Right, 0.0f));
}

void AddColumn(UVerticalBox& Column, UWidget& Child, const float Bottom = 8.0f)
{
    UVerticalBoxSlot* Slot = Column.AddChildToVerticalBox(&Child);
    Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Bottom));
}

UFF_BuildToolsActionButton* Action(UWidgetBlueprint& BP, UWidgetTree& Tree, const FName Name,
    const FName LabelName, const FText Caption, const bool bOnDark)
{
    const auto& P = UFF_UIPalette::Get()->InGame;
    UFF_BuildToolsActionButton* Button = MakeWidget<UFF_BuildToolsActionButton>(BP, Tree, Name, true);
    FButtonStyle Style;
    Style.Normal = Surface(bOnDark ? P.TealDeep : P.PaperSunken, 8.0f,
        bOnDark ? P.Teal : P.Line);
    Style.Hovered = Surface(bOnDark ? P.Teal : P.Hover, 8.0f, P.Teal);
    Style.Pressed = Surface(P.Teal, 8.0f, P.Teal);
    Style.Disabled = Surface(bOnDark ? P.TealDeep : P.PaperSunken, 8.0f);
    Style.NormalPadding = Style.PressedPadding = FMargin(10.0f, 7.0f);
    Button->SetStyle(Style);
    Button->bUseSelectionStyle = true;
    Button->SelectionStyle = Style;
    Button->SelectionStyle.Normal = Surface(bOnDark ? P.Teal : P.TealDeep, 8.0f, P.TealSoft);
    Button->SelectionStyle.Hovered = Surface(P.Teal, 8.0f, P.TealSoft);
    Button->SelectionTextColor = FSlateColor(White);
    UTextBlock* Text = Label(BP, Tree, LabelName, Caption, 14, bOnDark ? White : P.Ink, true);
    UButtonSlot* Slot = CastChecked<UButtonSlot>(Button->AddChild(Text));
    Slot->SetHorizontalAlignment(HAlign_Center);
    Slot->SetVerticalAlignment(VAlign_Center);
    return Button;
}

UWidget* FixedButton(UWidgetBlueprint& BP, UWidgetTree& Tree, const FName Name,
    const FName TextName, const FText Caption, const bool bOnDark, const float Width)
{
    UFF_BuildToolsActionButton* Button = Action(BP, Tree, Name, TextName, Caption, bOnDark);
    USizeBox* Size = MakeWidget<USizeBox>(BP, Tree, FName(*(FString(TEXT("Size_")) + Name.ToString())));
    Size->SetWidthOverride(Width);
    Size->AddChild(Button);
    return Size;
}

bool ApplyLayout(UWidgetBlueprint& BP)
{
    UWidgetTree& Tree = *BP.WidgetTree;
    UCanvasPanel* Canvas = Cast<UCanvasPanel>(Tree.FindWidget(TEXT("Canvas_BuildClarity")));
    if (!Canvas || !Tree.FindWidget(TEXT("Border_Tools")) ||
        !Tree.FindWidget(TEXT("Border_PlacementInfo"))) return false;
    if (Tree.FindWidget(TEXT("Border_BuildTopBar"))) return true;

    const auto& P = UFF_UIPalette::Get()->InGame;
    BP.Modify();
    Tree.Modify();

    UHorizontalBox* BarRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_BuildReferenceTopBar"));
    AddRow(*BarRow, *Label(BP, Tree, TEXT("Label_BuildMode"), LOCTEXT("BuildMode", "건설"), 17, White), 14.0f);
    UBorder* Divider = MakeWidget<UBorder>(BP, Tree, TEXT("Border_BuildReferenceDivider"));
    Divider->SetBrushColor(P.TealSoft);
    USizeBox* DividerSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildReferenceDivider"));
    DividerSize->SetWidthOverride(1.0f);
    DividerSize->SetHeightOverride(28.0f);
    DividerSize->AddChild(Divider);
    AddRow(*BarRow, *DividerSize, 10.0f);
    AddRow(*BarRow, *FixedButton(BP, Tree, TEXT("Button_Grid"), TEXT("Text_Grid"),
        LOCTEXT("Grid", "격자 켜짐"), true, 98.0f), 8.0f);
    AddRow(*BarRow, *FixedButton(BP, Tree, TEXT("Button_SnapMeter"), TEXT("Text_SnapMeter"),
        LOCTEXT("Meter", "1 m"), true, 64.0f), 4.0f);
    AddRow(*BarRow, *FixedButton(BP, Tree, TEXT("Button_SnapPrecision"), TEXT("Text_SnapPrecision"),
        LOCTEXT("Precision", "25 cm"), true, 76.0f), 4.0f);
    AddRow(*BarRow, *FixedButton(BP, Tree, TEXT("Button_SnapFree"), TEXT("Text_SnapFree"),
        LOCTEXT("Free", "1 cm"), true, 64.0f), 12.0f);
    AddRow(*BarRow, *FixedButton(BP, Tree, TEXT("Button_BuildSettings"), TEXT("Label_BuildSettings"),
        LOCTEXT("Settings", "세부 설정"), true, 104.0f), 0.0f);
    UBorder* TopBar = MakeWidget<UBorder>(BP, Tree, TEXT("Border_BuildTopBar"), true);
    TopBar->SetBrush(Surface(P.TealDeep, 14.0f, P.Teal));
    TopBar->SetBrushColor(White);
    TopBar->SetPadding(FMargin(14.0f, 8.0f));
    TopBar->AddChild(BarRow);
    USizeBox* BarSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildTopBar"));
    BarSize->SetWidthOverride(580.0f);
    BarSize->AddChild(TopBar);
    UCanvasPanelSlot* BarSlot = Canvas->AddChildToCanvas(BarSize);
    BarSlot->SetAnchors(FAnchors(0.5f, 0.0f));
    BarSlot->SetAlignment(FVector2D(0.5f, 0.0f));
    BarSlot->SetPosition(FVector2D(0.0f, 20.0f));
    BarSlot->SetAutoSize(true);
    BarSlot->SetZOrder(10);

    UVerticalBox* SettingsBody = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_BuildSettings"));
    UHorizontalBox* SettingsHeader = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_BuildSettingsHeader"));
    AddRow(*SettingsHeader, *Label(BP, Tree, TEXT("Label_BuildSettingsTitle"),
        LOCTEXT("SettingsTitle", "배치 설정"), 20, P.Ink), 0.0f, true);
    AddRow(*SettingsHeader, *FixedButton(BP, Tree, TEXT("Button_BuildSettingsClose"),
        TEXT("Label_BuildSettingsClose"), LOCTEXT("Close", "닫기 ×"), false, 76.0f), 0.0f);
    AddColumn(*SettingsBody, *SettingsHeader, 14.0f);
    AddColumn(*SettingsBody, *Label(BP, Tree, TEXT("Label_RotationSection"),
        LOCTEXT("Rotation", "시설 회전"), 13, P.InkMuted), 5.0f);
    UHorizontalBox* Rotation = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_BuildSettingsRotation"));
    AddRow(*Rotation, *FixedButton(BP, Tree, TEXT("Button_RotateLeft"), TEXT("Label_RotateLeft"),
        LOCTEXT("RotateLeft", "Q 왼쪽"), false, 82.0f), 6.0f);
    UEditableTextBox* Angle = MakeWidget<UEditableTextBox>(BP, Tree, TEXT("Editable_Angle"), true);
    Angle->SetText(FText::FromString(TEXT("0.00")));
    Angle->SetHintText(LOCTEXT("AngleHint", "각도"));
    Angle->SetJustification(ETextJustify::Center);
    FEditableTextBoxStyle AngleStyle = Angle->GetWidgetStyle();
    AngleStyle.BackgroundImageNormal = Surface(P.White, 8.0f, P.Line);
    AngleStyle.BackgroundImageHovered = Surface(P.White, 8.0f, P.Teal);
    AngleStyle.BackgroundImageFocused = Surface(P.White, 8.0f, P.TealDeep);
    AngleStyle.ForegroundColor = FSlateColor(P.Ink);
    AngleStyle.Padding = FMargin(8.0f, 7.0f);
    Angle->SetWidgetStyle(AngleStyle);
    USizeBox* AngleSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildSettingsAngle"));
    AngleSize->SetWidthOverride(82.0f);
    AngleSize->AddChild(Angle);
    AddRow(*Rotation, *AngleSize, 6.0f);
    AddRow(*Rotation, *FixedButton(BP, Tree, TEXT("Button_RotateRight"), TEXT("Label_RotateRight"),
        LOCTEXT("RotateRight", "오른쪽 E"), false, 82.0f), 0.0f);
    AddColumn(*SettingsBody, *Rotation, 8.0f);
    UTextBlock* RotationHint = Label(BP, Tree, TEXT("Text_RotationHint"),
        LOCTEXT("RotationHint", "Q / E 45° 회전 · Shift + Q / E 자유 회전"), 12, P.InkMuted, true);
    RotationHint->SetAutoWrapText(true);
    AddColumn(*SettingsBody, *RotationHint, 14.0f);
    AddColumn(*SettingsBody, *Label(BP, Tree, TEXT("Label_FineRotation"),
        LOCTEXT("FineRotation", "누르는 동안 자유 회전"), 13, P.InkMuted), 5.0f);
    UHorizontalBox* FreeRotation = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_BuildSettingsFreeRotation"));
    AddRow(*FreeRotation, *FixedButton(BP, Tree, TEXT("Button_FreeRotateLeft"),
        TEXT("Label_FreeRotateLeft"), LOCTEXT("FreeLeft", "Shift + Q"), false, 132.0f), 8.0f);
    AddRow(*FreeRotation, *FixedButton(BP, Tree, TEXT("Button_FreeRotateRight"),
        TEXT("Label_FreeRotateRight"), LOCTEXT("FreeRight", "Shift + E"), false, 132.0f), 0.0f);
    AddColumn(*SettingsBody, *FreeRotation, 14.0f);
    AddColumn(*SettingsBody, *Label(BP, Tree, TEXT("Label_FineMove"),
        LOCTEXT("FineMove", "구역 기준 미세 이동"), 13, P.InkMuted), 5.0f);
    UHorizontalBox* Nudge = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_BuildSettingsNudge"));
    AddRow(*Nudge, *FixedButton(BP, Tree, TEXT("Button_NudgeLeft"), TEXT("Label_NudgeLeft"),
        FText::FromString(TEXT("−X")), false, 62.0f), 5.0f);
    AddRow(*Nudge, *FixedButton(BP, Tree, TEXT("Button_NudgeRight"), TEXT("Label_NudgeRight"),
        FText::FromString(TEXT("+X")), false, 62.0f), 5.0f);
    AddRow(*Nudge, *FixedButton(BP, Tree, TEXT("Button_NudgeDown"), TEXT("Label_NudgeDown"),
        FText::FromString(TEXT("−Y")), false, 62.0f), 5.0f);
    AddRow(*Nudge, *FixedButton(BP, Tree, TEXT("Button_NudgeUp"), TEXT("Label_NudgeUp"),
        FText::FromString(TEXT("+Y")), false, 62.0f), 0.0f);
    AddColumn(*SettingsBody, *Nudge, 12.0f);
    UTextBlock* Help = Label(BP, Tree, TEXT("Label_BuildSettingsHelp"),
        LOCTEXT("SettingsHelp", "시설을 고르면 회전과 미세 이동을 사용할 수 있습니다. 격자와 이동 간격은 위 도구에서 조절하세요."),
        12, P.InkMuted);
    Help->SetAutoWrapText(true);
    Help->SetWrapTextAt(310.0f);
    AddColumn(*SettingsBody, *Help, 0.0f);
    UBorder* Settings = MakeWidget<UBorder>(BP, Tree, TEXT("Panel_BuildSettings"), true);
    Settings->SetBrush(Surface(P.Paper, 12.0f, P.LineStrong));
    Settings->SetBrushColor(White);
    Settings->SetPadding(FMargin(18.0f, 16.0f));
    Settings->SetVisibility(ESlateVisibility::Collapsed);
    UScrollBox* SettingsScroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Scroll_BuildSettings"));
    SettingsScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
    SettingsScroll->AddChild(SettingsBody);
    Settings->AddChild(SettingsScroll);
    USizeBox* SettingsSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildSettings"));
    SettingsSize->SetWidthOverride(368.0f);
    SettingsSize->SetHeightOverride(280.0f);
    SettingsSize->AddChild(Settings);
    UCanvasPanelSlot* SettingsSlot = Canvas->AddChildToCanvas(SettingsSize);
    SettingsSlot->SetAnchors(FAnchors(0.0f, 0.0f));
    SettingsSlot->SetPosition(FVector2D(28.0f, 82.0f));
    SettingsSlot->SetAutoSize(true);
    SettingsSlot->SetZOrder(11);
    return true;
}

bool RefinePresentation(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, PresentationKey) == TEXT("1")) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Bar = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildTopBar")));
    auto* SettingsLabel = Cast<UTextBlock>(Tree.FindWidget(TEXT("Label_BuildSettings")));
    auto* LeftLabel = Cast<UTextBlock>(Tree.FindWidget(TEXT("Label_RotateLeft")));
    auto* RightLabel = Cast<UTextBlock>(Tree.FindWidget(TEXT("Label_RotateRight")));
    if (!Bar || !SettingsLabel || !LeftLabel || !RightLabel) return false;
    BP.Modify();
    Tree.Modify();
    Bar->SetWidthOverride(580.0f);
    SettingsLabel->SetText(LOCTEXT("SettingsV2", "세부 설정"));
    LeftLabel->SetText(LOCTEXT("RotateLeftV2", "Q 왼쪽"));
    RightLabel->SetText(LOCTEXT("RotateRightV2", "오른쪽 E"));
    Metadata.SetValue(&BP, PresentationKey, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_TOOLS_REFINEMENT"), BP)) return true;
    Metadata.RemoveValue(&BP, PresentationKey);
    return false;
}

bool AddNavigationAndProjectSettings(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, NavigationSettingsKey) == TEXT("1")) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Canvas = Cast<UCanvasPanel>(Tree.FindWidget(TEXT("Canvas_BuildClarity")));
    auto* SettingsBody = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Column_BuildSettings")));
    auto* SettingsSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildSettings")));
    if (!Canvas || !SettingsBody || !SettingsSize || Tree.FindWidget(TEXT("Border_BuildBack")) ||
        Tree.FindWidget(TEXT("Button_SettingsGrid"))) return false;
    const auto& P = UFF_UIPalette::Get()->InGame;
    BP.Modify();
    Tree.Modify();

    UBorder* Back = MakeWidget<UBorder>(BP, Tree, TEXT("Border_BuildBack"), true);
    Back->SetBrush(Surface(P.TealDeep, 12.0f, P.Teal));
    Back->SetPadding(FMargin(0));
    Back->AddChild(FixedButton(BP, Tree, TEXT("Button_BuildBack"), TEXT("Text_BuildBack"),
        LOCTEXT("BackBuild", "← 건설"), true, 128.0f));
    UCanvasPanelSlot* BackSlot = Canvas->AddChildToCanvas(Back);
    BackSlot->SetAnchors(FAnchors(0.0f, 0.0f));
    BackSlot->SetPosition(FVector2D(28.0f, 20.0f));
    BackSlot->SetAutoSize(true);
    BackSlot->SetZOrder(12);
	// Show the reference's tall settings sheet at 720p while retaining scrolling
	// for smaller viewports and the original rotation/nudge controls below.
    SettingsSize->SetHeightOverride(600.0f);
    SettingsSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    // Keep the existing angle, free-rotation and zone nudge controls below the
    // project-specific session and camera options. All controls call real systems.
    int32 InsertIndex = 1; // after the authored drawer title/close row
    auto Insert = [&](UWidget& Child, const float Bottom)
    {
        AddColumn(*SettingsBody, Child, Bottom);
        SettingsBody->ShiftChild(InsertIndex++, &Child);
    };
    Insert(*Label(BP, Tree, TEXT("Label_SettingsPlacement"),
        LOCTEXT("PlacementSection", "배치 설정"), 14, P.InkMuted), 5.0f);
    Insert(*FixedButton(BP, Tree, TEXT("Button_SettingsGrid"), TEXT("Text_SettingsGrid"),
        LOCTEXT("SettingsGrid", "건축 격자 켜짐"), false, 300.0f), 8.0f);
    Insert(*Label(BP, Tree, TEXT("Label_SettingsSnap"),
        LOCTEXT("SnapSection", "위치 맞춤 간격"), 13, P.InkMuted), 5.0f);
    UHorizontalBox* SnapRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_SettingsSnap"));
    AddRow(*SnapRow, *FixedButton(BP, Tree, TEXT("Button_SettingsSnapMeter"),
        TEXT("Text_SettingsSnapMeter"), LOCTEXT("SnapMeter", "1 m"), false, 90.0f), 8.0f);
    AddRow(*SnapRow, *FixedButton(BP, Tree, TEXT("Button_SettingsSnapPrecision"),
        TEXT("Text_SettingsSnapPrecision"), LOCTEXT("SnapPrecision", "25 cm"), false, 98.0f), 8.0f);
    AddRow(*SnapRow, *FixedButton(BP, Tree, TEXT("Button_SettingsSnapFree"),
        TEXT("Text_SettingsSnapFree"), LOCTEXT("SnapFree", "1 cm"), false, 90.0f), 0.0f);
    Insert(*SnapRow, 8.0f);
    Insert(*FixedButton(BP, Tree, TEXT("Button_SettingsDirectAdjustment"),
        TEXT("Text_SettingsDirectAdjustment"), LOCTEXT("SettingsDirect", "직접 조정 꺼짐"),
        false, 300.0f), 12.0f);
    Insert(*Label(BP, Tree, TEXT("Label_SettingsRanges"),
        LOCTEXT("RangesSection", "범위 표시"), 14, P.InkMuted), 5.0f);
    UWidget* Ranges = FixedButton(BP, Tree, TEXT("Button_SettingsFacilityRanges"),
        TEXT("Text_SettingsFacilityRanges"), LOCTEXT("Ranges", "시설 범위 켜짐"), false, 300.0f);
    Ranges->SetToolTipText(LOCTEXT("RangesTip", "설치된 시설의 점유·안전·대기 범위를 표시합니다."));
    Insert(*Ranges, 7.0f);
    UWidget* Synergy = FixedButton(BP, Tree, TEXT("Button_SettingsSynergyPreview"),
        TEXT("Text_SettingsSynergyPreview"), LOCTEXT("Synergy", "조합 미리보기 켜짐"), false, 300.0f);
    Synergy->SetToolTipText(LOCTEXT("SynergyTip", "배치 중 주변 시설과의 조합 효과를 표시합니다."));
    Insert(*Synergy, 12.0f);
    Insert(*Label(BP, Tree, TEXT("Label_SettingsCamera"),
        LOCTEXT("CameraSection", "카메라"), 14, P.InkMuted), 5.0f);
    Insert(*FixedButton(BP, Tree, TEXT("Button_SettingsZoomToCursor"),
        TEXT("Text_SettingsZoomToCursor"), LOCTEXT("ZoomToCursor", "커서 기준 확대 켜짐"),
        false, 300.0f), 7.0f);
    Insert(*FixedButton(BP, Tree, TEXT("Button_SettingsEdgeScroll"),
        TEXT("Text_SettingsEdgeScroll"), LOCTEXT("EdgeScroll", "가장자리 이동 켜짐"),
        false, 300.0f), 14.0f);

    Metadata.SetValue(&BP, NavigationSettingsKey, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_TOOLS_SETTINGS"), BP)) return true;
    Metadata.RemoveValue(&BP, NavigationSettingsKey);
    return false;
}

void StyleToolbarAction(UFF_BuildToolsActionButton& Button, UTextBlock& Icon, const FText& Glyph,
    const FText& Tip)
{
    FButtonStyle Style;
    Style.Normal = FFBuildPearl::Rounded(FFBuildPearl::Color(91, 89, 116, .35f), 22.f);
    Style.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(152, 190, 218, .34f), 22.f,
        FFBuildPearl::Cyan, 1.f);
    Style.Pressed = FFBuildPearl::Rounded(FFBuildPearl::Cyan, 22.f);
    Style.Disabled = FFBuildPearl::Rounded(FFBuildPearl::Color(91, 89, 116, .18f), 22.f);
    Style.NormalPadding = Style.PressedPadding = FMargin(0.f);
    Button.SetStyle(Style);
    Button.bUseSelectionStyle = true;
    Button.SelectionStyle = Style;
    Button.SelectionStyle.Normal = FFBuildPearl::Rounded(FFBuildPearl::Color(73, 75, 101, .98f),
        22.f, FFBuildPearl::Cyan, 2.f);
    Button.SelectionStyle.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(92, 98, 128, .98f),
        22.f, FFBuildPearl::Cyan, 2.f);
    Button.SelectionTextColor = FSlateColor(FLinearColor::White);
    Button.SetToolTipText(Tip);
    StyleText(Icon, 24, FLinearColor::White, TEXT("SemiBold"));
    Icon.SetText(Glyph);
    Icon.SetJustification(ETextJustify::Center);
}

void AttachToolbarTexture(UWidgetBlueprint& BP, UWidgetTree& Tree,
    UFF_BuildToolsActionButton& Button, UTextBlock& RetainedCaption, UHorizontalBox& Hidden,
    const FName ImageName, const TCHAR* TexturePath)
{
    UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TexturePath);
    if (!Texture) return; // Retain the authored caption if a packaged icon is unavailable.
    RetainedCaption.RemoveFromParent();
    RetainedCaption.SetVisibility(ESlateVisibility::Collapsed);
    RetainedCaption.SetText(FText::GetEmpty());
    AddRow(Hidden, RetainedCaption, 0.f);
    auto* Image = MakeWidget<UImage>(BP, Tree, ImageName);
    Image->SetBrushFromTexture(Texture, true);
    Image->SetColorAndOpacity(FLinearColor::White);
    auto* Bounds = MakeWidget<USizeBox>(BP, Tree,
        FName(*(FString(TEXT("Size_")) + ImageName.ToString())));
    Bounds->SetWidthOverride(25.f);
    Bounds->SetHeightOverride(25.f);
    Bounds->AddChild(Image);
    auto* Slot = CastChecked<UButtonSlot>(Button.AddChild(Bounds));
    Slot->SetHorizontalAlignment(HAlign_Center);
    Slot->SetVerticalAlignment(VAlign_Center);
}

void StyleDrawerAction(UFF_BuildToolsActionButton& Button, UTextBlock* Caption, const bool bToggle)
{
    FButtonStyle Style;
    Style.Normal = FFBuildPearl::Rounded(FFBuildPearl::Color(245, 246, 253, .92f), 18.f,
        FFBuildPearl::Line, 1.f);
    Style.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(226, 241, 252, .95f), 18.f,
        FFBuildPearl::Cyan, 1.f);
    Style.Pressed = FFBuildPearl::Rounded(FFBuildPearl::Cyan, 18.f);
    Style.Disabled = FFBuildPearl::Rounded(FFBuildPearl::Color(232, 234, 242, .58f), 18.f);
    Style.NormalPadding = Style.PressedPadding = FMargin(8.f, 6.f);
    Button.SetStyle(Style);
    Button.bUseSelectionStyle = true;
    Button.SelectionStyle = Style;
    Button.SelectionStyle.Normal = FFBuildPearl::Rounded(FFBuildPearl::Color(173, 226, 250), 18.f,
        FFBuildPearl::Color(117, 194, 232), 1.f);
    Button.SelectionStyle.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Cyan, 18.f);
    Button.SelectionTextColor = FSlateColor(FFBuildPearl::Ink);
    if (Caption)
    {
        StyleText(*Caption, bToggle ? 13 : 14, FFBuildPearl::Ink, TEXT("SemiBold"));
        Caption->SetJustification(ETextJustify::Center);
    }
}

bool ApplyPearlToolbarAndDrawer(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, PearlToolbarKey) == TEXT("1")) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Canvas = Cast<UCanvasPanel>(Tree.FindWidget(TEXT("Canvas_BuildClarity")));
    auto* Bar = Cast<UBorder>(Tree.FindWidget(TEXT("Border_BuildTopBar")));
    auto* BarSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildTopBar")));
    auto* BarRow = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_BuildReferenceTopBar")));
    auto* Heading = Cast<UTextBlock>(Tree.FindWidget(TEXT("Label_BuildMode")));
    auto* Back = Cast<UBorder>(Tree.FindWidget(TEXT("Border_BuildBack")));
    auto* BackButton = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(TEXT("Button_BuildBack")));
    auto* BackSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_Button_BuildBack")));
    auto* Settings = Cast<UBorder>(Tree.FindWidget(TEXT("Panel_BuildSettings")));
    auto* SettingsSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildSettings")));
    auto* SettingsBody = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Column_BuildSettings")));
    auto* Help = Cast<UBorder>(Tree.FindWidget(TEXT("Border_PlacementHelp")));
    auto* HelpText = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ControlHelp")));
    auto* Grid = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(TEXT("Button_Grid")));
    auto* GridText = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Grid")));
    auto* SettingsButton = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(TEXT("Button_BuildSettings")));
    auto* SettingsIcon = Cast<UTextBlock>(Tree.FindWidget(TEXT("Label_BuildSettings")));
    if (!Canvas || !Bar || !BarSize || !BarRow || !Heading || !Back || !BackButton || !BackSize ||
        !Settings || !SettingsSize || !SettingsBody || !Help || !HelpText || !Grid || !GridText ||
        !SettingsButton || !SettingsIcon || Tree.FindWidget(TEXT("Button_MultiSelect"))) return false;

    BP.Modify();
    Tree.Modify();
    // The named native widgets stay in the tree. Three direct snap buttons become
    // hidden compatibility controls; the visible F3 icon cycles their same modes.
    auto* OldSnap = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_RetiredToolbarSnap"));
    OldSnap->SetVisibility(ESlateVisibility::Collapsed);
    for (const TCHAR* Name : {TEXT("Size_Button_SnapMeter"), TEXT("Size_Button_SnapPrecision"),
        TEXT("Size_Button_SnapFree")})
    {
        if (UWidget* Widget = Tree.FindWidget(Name))
        {
            Widget->RemoveFromParent();
            AddRow(*OldSnap, *Widget, 0.f);
        }
    }
    Heading->RemoveFromParent();
    if (UWidget* Divider = Tree.FindWidget(TEXT("Size_BuildReferenceDivider")))
    {
        Divider->RemoveFromParent();
        AddRow(*OldSnap, *Divider, 0.f);
    }
    UWidget* GridCell = Tree.FindWidget(TEXT("Size_Button_Grid"));
    UWidget* SettingsCell = Tree.FindWidget(TEXT("Size_Button_BuildSettings"));
    if (!GridCell || !SettingsCell) return false;
    GridCell->RemoveFromParent();
    SettingsCell->RemoveFromParent();
    BarRow->ClearChildren();
    Bar->SetBrush(FFBuildPearl::MaterialBrush(TEXT("Dark")));
    Bar->SetBrushColor(FLinearColor::White);
    Bar->SetPadding(FMargin(10.f, 7.f));
    BarSize->SetWidthOverride(394.f);
    if (auto* Slot = Cast<UCanvasPanelSlot>(BarSize->Slot)) Slot->SetPosition(FVector2D(0.f, 18.f));

    auto MakeIconCell = [&](const TCHAR* ButtonName, const TCHAR* TextName, const FText& Glyph,
        const FText& Tip) -> USizeBox*
    {
        auto* Button = Action(BP, Tree, ButtonName, TextName, Glyph, true);
        auto* Caption = CastChecked<UTextBlock>(Button->GetContent());
        StyleToolbarAction(*Button, *Caption, Glyph, Tip);
        auto* Cell = MakeWidget<USizeBox>(BP, Tree, FName(*(FString(TEXT("Size_")) + ButtonName)));
        Cell->SetWidthOverride(49.f);
        Cell->SetHeightOverride(43.f);
        Cell->AddChild(Button);
        return Cell;
    };
    auto SizeIcon = [](UWidget& Cell)
    {
        if (auto* Size = Cast<USizeBox>(&Cell))
        {
            Size->SetWidthOverride(49.f);
            Size->SetHeightOverride(43.f);
        }
    };
    StyleToolbarAction(*Grid, *GridText, LOCTEXT("GridIcon", "▦"),
        LOCTEXT("GridTip", "건축 격자 표시 · F2"));
    StyleToolbarAction(*SettingsButton, *SettingsIcon, LOCTEXT("SettingsIcon", "⚙"),
        LOCTEXT("SettingsTip", "건축 설정 · F5"));
    SizeIcon(*GridCell); SizeIcon(*SettingsCell);
    UWidget* Icons[] = {
        MakeIconCell(TEXT("Button_MultiSelect"), TEXT("Text_MultiSelect"), LOCTEXT("MultiIcon", "▧"),
            LOCTEXT("MultiTip", "시설 다중 선택 · F1")),
        GridCell,
        MakeIconCell(TEXT("Button_SnapCycle"), TEXT("Text_SnapCycle"), LOCTEXT("SnapIcon", "⌗"),
            LOCTEXT("SnapTip", "위치 맞춤 간격 · F3")),
        MakeIconCell(TEXT("Button_ToolbarRanges"), TEXT("Text_ToolbarRanges"), LOCTEXT("RangesIcon", "◎"),
            LOCTEXT("RangesTip", "시설 범위 표시 · F4")),
        SettingsCell,
        MakeIconCell(TEXT("Button_ToolbarCatalog"), TEXT("Text_ToolbarCatalog"), LOCTEXT("CatalogIcon", "▤"),
            LOCTEXT("CatalogTip", "시설 목록 · F6")),
        MakeIconCell(TEXT("Button_ToolbarHelp"), TEXT("Text_ToolbarHelp"), LOCTEXT("HelpIcon", "?"),
            LOCTEXT("HelpTip", "건축 조작 안내 · F7"))
    };
    struct FToolbarIcon { const TCHAR* Button; const TCHAR* Text; const TCHAR* Image; const TCHAR* Path; };
    const FToolbarIcon Textures[] = {
        {TEXT("Button_MultiSelect"), TEXT("Text_MultiSelect"), TEXT("Image_ToolbarF1"),
            TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions/T_FF_LH_Icon_AssignStaff_White_256.T_FF_LH_Icon_AssignStaff_White_256")},
        {TEXT("Button_Grid"), TEXT("Text_Grid"), TEXT("Image_ToolbarF2"),
            TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_Density_White_256.T_FF_LH_Icon_Density_White_256")},
        {TEXT("Button_SnapCycle"), TEXT("Text_SnapCycle"), TEXT("Image_ToolbarF3"),
            TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions/T_FF_LH_Icon_Locate_White_256.T_FF_LH_Icon_Locate_White_256")},
        {TEXT("Button_ToolbarRanges"), TEXT("Text_ToolbarRanges"), TEXT("Image_ToolbarF4"),
            TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Metrics/T_FF_LH_Icon_Safety_White_256.T_FF_LH_Icon_Safety_White_256")},
        {TEXT("Button_BuildSettings"), TEXT("Label_BuildSettings"), TEXT("Image_ToolbarF5"),
            TEXT("/Game/FestaFiesta/UI/Title/CleanCasual/Icons/T_FF_Title_CleanCasual_Icon_Settings_White_256.T_FF_Title_CleanCasual_Icon_Settings_White_256")},
        {TEXT("Button_ToolbarCatalog"), TEXT("Text_ToolbarCatalog"), TEXT("Image_ToolbarF6"),
            TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_Facilities_White_256.T_FF_LH_Icon_Facilities_White_256")},
        {TEXT("Button_ToolbarHelp"), TEXT("Text_ToolbarHelp"), TEXT("Image_ToolbarF7"),
            TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Facilities/T_FF_LH_Icon_FacilityInfo_White_256.T_FF_LH_Icon_FacilityInfo_White_256")}
    };
    for (const FToolbarIcon& Icon : Textures)
        if (auto* Button = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(Icon.Button)))
            if (auto* Caption = Cast<UTextBlock>(Tree.FindWidget(Icon.Text)))
                AttachToolbarTexture(BP, Tree, *Button, *Caption, *OldSnap, Icon.Image, Icon.Path);
    for (UWidget* Icon : Icons) AddRow(*BarRow, *Icon, 4.f);
    auto* GoldLine = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_BuildToolbarGold"));
    auto* GoldImage = MakeWidget<UImage>(BP, Tree, TEXT("Image_BuildToolbarGold"));
    if (auto* Texture = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Metrics/T_FF_LH_Icon_Gold_White_256.T_FF_LH_Icon_Gold_White_256")))
        GoldImage->SetBrushFromTexture(Texture, true);
    GoldImage->SetColorAndOpacity(FLinearColor::White);
    auto* GoldIconSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildToolbarGoldIcon"));
    GoldIconSize->SetWidthOverride(25.f);
    GoldIconSize->SetHeightOverride(25.f);
    GoldIconSize->AddChild(GoldImage);
    AddRow(*GoldLine, *GoldIconSize, 4.f);
    AddRow(*GoldLine, *Label(BP, Tree, TEXT("Text_ToolbarGold"), FText::FromString(TEXT("—")),
        15, FLinearColor::White, true), 0.f);
    auto* GoldPill = MakeWidget<UBorder>(BP, Tree, TEXT("Border_BuildToolbarGold"), true);
    GoldPill->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(82, 79, 107, .92f), 22.f,
        FFBuildPearl::Color(160, 163, 189, .45f), 1.f));
    GoldPill->SetPadding(FMargin(8.f, 7.f));
    GoldPill->AddChild(GoldLine);
    GoldPill->SetToolTipText(LOCTEXT("GoldTip", "현재 보유 골드"));
    auto* GoldSlot = Canvas->AddChildToCanvas(GoldPill);
    GoldSlot->SetAnchors(FAnchors(1.f, 0.f));
    GoldSlot->SetAlignment(FVector2D(1.f, 0.f));
    GoldSlot->SetPosition(FVector2D(-32.f, 27.f));
    GoldSlot->SetAutoSize(true);
    GoldSlot->SetZOrder(12);

    // Small keycaps sit beneath the floating charcoal pill, aligned with each icon.
    auto* Keycaps = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_BuildToolbarKeycaps"));
    for (int32 Index = 1; Index <= 7; ++Index)
    {
        auto* Key = MakeWidget<UBorder>(BP, Tree, FName(*FString::Printf(TEXT("Border_BuildKeyF%d"), Index)));
        Key->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(249, 249, 255, .94f), 5.f,
            FFBuildPearl::Line, 1.f));
        Key->SetPadding(FMargin(4.f, 1.f));
        Key->AddChild(Label(BP, Tree, FName(*FString::Printf(TEXT("Label_BuildKeyF%d"), Index)),
            FText::FromString(FString::Printf(TEXT("F%d"), Index)), 10, FFBuildPearl::Ink));
        auto* Cell = MakeWidget<USizeBox>(BP, Tree, FName(*FString::Printf(TEXT("Size_BuildKeyF%d"), Index)));
        Cell->SetWidthOverride(49.f);
        Cell->SetHeightOverride(20.f);
        auto* KeySlot = CastChecked<USizeBoxSlot>(Cell->AddChild(Key));
        KeySlot->SetHorizontalAlignment(HAlign_Center);
        AddRow(*Keycaps, *Cell, 4.f);
    }
    Bar->RemoveFromParent();
    auto* FloatingStack = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_BuildFloatingToolbar"));
    AddColumn(*FloatingStack, *Bar, 2.f);
    AddColumn(*FloatingStack, *Keycaps, 0.f);
    AddColumn(*FloatingStack, *OldSnap, 0.f);
    BarSize->AddChild(FloatingStack);

    // Navigation is a circular world control; the title sits beside it instead of
    // consuming one of the seven tool slots.
    Back->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Charcoal, 30.f,
        FFBuildPearl::Color(149, 218, 250, .62f), 1.f));
    Back->SetPadding(FMargin(0.f));
    BackSize->SetWidthOverride(55.f);
    BackSize->SetHeightOverride(55.f);
    if (auto* BackText = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_BuildBack"))))
        StyleToolbarAction(*BackButton, *BackText, LOCTEXT("BackIcon", "←"),
            LOCTEXT("BackTip", "이전 건축 화면"));
    {
        FButtonStyle Style = BackButton->GetStyle();
        Style.Normal = FFBuildPearl::Rounded(FLinearColor::Transparent, 29.f);
        Style.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(152, 219, 250, .25f), 29.f);
        Style.Pressed = FFBuildPearl::Rounded(FFBuildPearl::Cyan, 29.f);
        BackButton->SetStyle(Style);
    }
    if (auto* Slot = Cast<UCanvasPanelSlot>(Back->Slot)) Slot->SetPosition(FVector2D(27.f, 21.f));
    StyleText(*Heading, 25, FLinearColor::White, TEXT("Bold"));
    Heading->SetShadowOffset(FVector2D(1.f, 2.f));
    Heading->SetShadowColorAndOpacity(FFBuildPearl::Color(31, 30, 44, .8f));
    auto* HeadingSlot = Canvas->AddChildToCanvas(Heading);
    HeadingSlot->SetAnchors(FAnchors(0.f, 0.f));
    HeadingSlot->SetPosition(FVector2D(94.f, 28.f));
    HeadingSlot->SetAutoSize(true);
    HeadingSlot->SetZOrder(12);

    Settings->SetBrush(FFBuildPearl::MaterialBrush(TEXT("Panel")));
    Settings->SetBrushColor(FLinearColor::White);
    Settings->SetPadding(FMargin(23.f, 19.f));
    SettingsSize->SetWidthOverride(430.f);
    if (auto* Slot = Cast<UCanvasPanelSlot>(SettingsSize->Slot)) Slot->SetPosition(FVector2D(25.f, 101.f));
    if (auto* Title = Cast<UTextBlock>(Tree.FindWidget(TEXT("Label_BuildSettingsTitle"))))
    {
        StyleText(*Title, 26, FFBuildPearl::Ink, TEXT("Bold"));
        Title->SetText(LOCTEXT("PearlSettingsTitle", "건축 설정"));
    }
    if (auto* Close = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(TEXT("Button_BuildSettingsClose"))))
    {
        if (auto* Caption = Cast<UTextBlock>(Close->GetContent()))
        {
            StyleDrawerAction(*Close, Caption, false);
            Caption->SetText(LOCTEXT("PearlClose", "×"));
            StyleText(*Caption, 22, FFBuildPearl::Ink, TEXT("Bold"));
        }
        if (auto* Size = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_Button_BuildSettingsClose"))))
        { Size->SetWidthOverride(40.f); Size->SetHeightOverride(40.f); }
    }
    for (const TCHAR* Name : {TEXT("Label_SettingsPlacement"), TEXT("Label_SettingsRanges"),
        TEXT("Label_SettingsCamera"), TEXT("Label_RotationSection"), TEXT("Label_FineRotation"),
        TEXT("Label_FineMove"), TEXT("Label_SettingsSnap")})
        if (auto* Section = Cast<UTextBlock>(Tree.FindWidget(Name)))
            StyleText(*Section, 14, FFBuildPearl::Muted, TEXT("SemiBold"));
    struct FToggleRow { const TCHAR* Button; const TCHAR* Caption; const TCHAR* Label; };
    const FToggleRow ToggleRows[] = {
        {TEXT("Button_SettingsGrid"), TEXT("건축 격자"), TEXT("Row_PearlSettingsGrid")},
        {TEXT("Button_SettingsDirectAdjustment"), TEXT("직접 위치 조정"), TEXT("Row_PearlSettingsDirect")},
        {TEXT("Button_SettingsFacilityRanges"), TEXT("시설 점유·안전·대기 범위"), TEXT("Row_PearlSettingsRanges")},
        {TEXT("Button_SettingsSynergyPreview"), TEXT("조합 미리보기"), TEXT("Row_PearlSettingsSynergy")},
        {TEXT("Button_SettingsZoomToCursor"), TEXT("커서 기준 확대"), TEXT("Row_PearlSettingsZoom")},
        {TEXT("Button_SettingsEdgeScroll"), TEXT("가장자리 카메라 이동"), TEXT("Row_PearlSettingsEdge")}
    };
    for (const FToggleRow& Row : ToggleRows)
    {
        const FString SizeName = FString(TEXT("Size_")) + Row.Button;
        auto* Size = Cast<USizeBox>(Tree.FindWidget(*SizeName));
        auto* Button = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(Row.Button));
        if (!Size || !Button || Size->GetParent() != SettingsBody) return false;
        const int32 Index = SettingsBody->GetChildIndex(Size);
        Size->RemoveFromParent();
        Size->SetWidthOverride(91.f);
        Size->SetHeightOverride(34.f);
        StyleDrawerAction(*Button, Cast<UTextBlock>(Button->GetContent()), true);
        auto* LabelText = Label(BP, Tree, FName(*(FString(Row.Label) + TEXT("Caption"))),
            FText::FromString(Row.Caption), 15, FFBuildPearl::Ink);
        LabelText->SetAutoWrapText(true);
        auto* Horizontal = MakeWidget<UHorizontalBox>(BP, Tree, Row.Label);
        AddRow(*Horizontal, *LabelText, 8.f, true);
        AddRow(*Horizontal, *Size, 0.f);
        AddColumn(*SettingsBody, *Horizontal, 8.f);
        SettingsBody->ShiftChild(Index, Horizontal);
    }
    for (const TCHAR* Name : {TEXT("Button_SettingsSnapMeter"), TEXT("Button_SettingsSnapPrecision"),
        TEXT("Button_SettingsSnapFree"), TEXT("Button_RotateLeft"), TEXT("Button_RotateRight"),
        TEXT("Button_FreeRotateLeft"), TEXT("Button_FreeRotateRight"), TEXT("Button_NudgeLeft"),
        TEXT("Button_NudgeRight"), TEXT("Button_NudgeDown"), TEXT("Button_NudgeUp")})
        if (auto* Button = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(Name)))
            StyleDrawerAction(*Button, Cast<UTextBlock>(Button->GetContent()), false);
    if (auto* Angle = Cast<UEditableTextBox>(Tree.FindWidget(TEXT("Editable_Angle"))))
    {
        FEditableTextBoxStyle Style = Angle->GetWidgetStyle();
        Style.BackgroundImageNormal = FFBuildPearl::Rounded(FFBuildPearl::Color(250, 250, 255),
            16.f, FFBuildPearl::Line, 1.f);
        Style.BackgroundImageHovered = FFBuildPearl::Rounded(FLinearColor::White, 16.f,
            FFBuildPearl::Cyan, 1.f);
        Style.BackgroundImageFocused = FFBuildPearl::Rounded(FLinearColor::White, 16.f,
            FFBuildPearl::Cyan, 2.f);
        Angle->SetWidgetStyle(Style);
    }

    // The reference keeps shortcuts in the world layer, not inside a white card.
    Help->RemoveFromParent();
    Help->SetBrush(FFBuildPearl::Rounded(FLinearColor::Transparent, 0.f));
    Help->SetBrushColor(FLinearColor::Transparent);
    Help->SetPadding(FMargin(0.f));
    StyleText(*HelpText, 16, FLinearColor::White, TEXT("SemiBold"));
    HelpText->SetShadowOffset(FVector2D(1.f, 2.f));
    HelpText->SetShadowColorAndOpacity(FFBuildPearl::Color(25, 25, 36, .95f));
    HelpText->SetWrapTextAt(330.f);
    HelpText->SetLineHeightPercentage(1.3f);
    auto* HelpSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildWorldHelp"), true);
    HelpSize->SetWidthOverride(340.f);
    HelpSize->AddChild(Help);
    auto* HelpSlot = Canvas->AddChildToCanvas(HelpSize);
    HelpSlot->SetAnchors(FAnchors(0.f, 0.f));
    HelpSlot->SetPosition(FVector2D(32.f, 525.f));
    HelpSlot->SetAutoSize(true);
    HelpSlot->SetZOrder(9);

    Metadata.SetValue(&BP, PearlToolbarKey, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_TOOLS_PEARL_V4"), BP)) return true;
    Metadata.RemoveValue(&BP, PearlToolbarKey);
    return false;
}

bool ApplyPearlPolish(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, PearlPolishKey) == TEXT("1")) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* BarSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildTopBar")));
    auto* Hidden = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_RetiredToolbarSnap")));
    auto* Gold = Cast<UBorder>(Tree.FindWidget(TEXT("Border_BuildToolbarGold")));
    auto* GoldImage = Cast<UImage>(Tree.FindWidget(TEXT("Image_BuildToolbarGold")));
    auto* Back = Cast<UBorder>(Tree.FindWidget(TEXT("Border_BuildBack")));
    auto* BackButton = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(TEXT("Button_BuildBack")));
    auto* Info = Cast<UBorder>(Tree.FindWidget(TEXT("Border_PlacementInfo")));
    auto* InfoSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_PlacementInfo")));
    auto* Summary = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Column_ClaritySummary")));
    auto* Dock = Cast<UBorder>(Tree.FindWidget(TEXT("Border_Tools")));
    if (!BarSize || !Hidden || !Gold || !GoldImage || !Back || !BackButton ||
        !Info || !InfoSize || !Summary || !Dock) return false;
    BP.Modify(); Tree.Modify();

    // The first pearl pass used glyphs outside the project's Korean font and
    // an unconstrained 256px coin. Keep every named text binding in a collapsed
    // compatibility row, and give each action a bounded, packaged icon texture.
    BarSize->SetWidthOverride(394.f);
    struct FToolbarIcon { const TCHAR* Button; const TCHAR* Text; const TCHAR* Image; const TCHAR* Path; };
    const FToolbarIcon Textures[] = {
        {TEXT("Button_MultiSelect"), TEXT("Text_MultiSelect"), TEXT("Image_ToolbarF1"),
            TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions/T_FF_LH_Icon_AssignStaff_White_256.T_FF_LH_Icon_AssignStaff_White_256")},
        {TEXT("Button_Grid"), TEXT("Text_Grid"), TEXT("Image_ToolbarF2"),
            TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_Density_White_256.T_FF_LH_Icon_Density_White_256")},
        {TEXT("Button_SnapCycle"), TEXT("Text_SnapCycle"), TEXT("Image_ToolbarF3"),
            TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions/T_FF_LH_Icon_Locate_White_256.T_FF_LH_Icon_Locate_White_256")},
        {TEXT("Button_ToolbarRanges"), TEXT("Text_ToolbarRanges"), TEXT("Image_ToolbarF4"),
            TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Metrics/T_FF_LH_Icon_Safety_White_256.T_FF_LH_Icon_Safety_White_256")},
        {TEXT("Button_BuildSettings"), TEXT("Label_BuildSettings"), TEXT("Image_ToolbarF5"),
            TEXT("/Game/FestaFiesta/UI/Title/CleanCasual/Icons/T_FF_Title_CleanCasual_Icon_Settings_White_256.T_FF_Title_CleanCasual_Icon_Settings_White_256")},
        {TEXT("Button_ToolbarCatalog"), TEXT("Text_ToolbarCatalog"), TEXT("Image_ToolbarF6"),
            TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_Facilities_White_256.T_FF_LH_Icon_Facilities_White_256")},
        {TEXT("Button_ToolbarHelp"), TEXT("Text_ToolbarHelp"), TEXT("Image_ToolbarF7"),
            TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Facilities/T_FF_LH_Icon_FacilityInfo_White_256.T_FF_LH_Icon_FacilityInfo_White_256")}
    };
    for (const FToolbarIcon& Icon : Textures)
    {
        auto* Button = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(Icon.Button));
        auto* Caption = Cast<UTextBlock>(Tree.FindWidget(Icon.Text));
        if (!Button || !Caption) return false;
        if (!Tree.FindWidget(Icon.Image) && Button->GetContent() == Caption)
            AttachToolbarTexture(BP, Tree, *Button, *Caption, *Hidden, Icon.Image, Icon.Path);
        FButtonStyle ToolbarStyle = Button->GetStyle();
        ToolbarStyle.Normal = FFBuildPearl::Rounded(FFBuildPearl::Color(91, 89, 116, .35f), 22.f);
        ToolbarStyle.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(152, 190, 218, .34f),
            22.f, FFBuildPearl::Cyan, 1.f);
        ToolbarStyle.Pressed = FFBuildPearl::Rounded(FFBuildPearl::Cyan, 22.f);
        ToolbarStyle.Disabled = FFBuildPearl::Rounded(FFBuildPearl::Color(91, 89, 116, .18f), 22.f);
        Button->SetStyle(ToolbarStyle);
        Button->SelectionStyle.Normal = FFBuildPearl::Rounded(FFBuildPearl::Color(73, 75, 101, .98f),
            22.f, FFBuildPearl::Cyan, 2.f);
        Button->SelectionStyle.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(92, 98, 128, .98f),
            22.f, FFBuildPearl::Cyan, 2.f);
        Button->SelectionTextColor = FSlateColor(FLinearColor::White);
    }
    if (auto* Texture = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Metrics/T_FF_LH_Icon_Gold_White_256.T_FF_LH_Icon_Gold_White_256")))
        GoldImage->SetBrushFromTexture(Texture, true);
    GoldImage->SetColorAndOpacity(FLinearColor::White);
    if (!Tree.FindWidget(TEXT("Size_BuildToolbarGoldIcon")))
    {
        auto* GoldRow = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_BuildToolbarGold")));
        if (!GoldRow) return false;
        const int32 Index = GoldRow->GetChildIndex(GoldImage);
        GoldImage->RemoveFromParent();
        auto* Bounds = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildToolbarGoldIcon"));
        Bounds->SetWidthOverride(25.f); Bounds->SetHeightOverride(25.f);
        Bounds->AddChild(GoldImage);
        AddRow(*GoldRow, *Bounds, 4.f);
        GoldRow->ShiftChild(FMath::Max(0, Index), Bounds);
    }
    if (auto* Slot = Cast<UCanvasPanelSlot>(Gold->Slot))
    {
        Slot->SetAnchors(FAnchors(1.f, 0.f));
        Slot->SetAlignment(FVector2D(1.f, 0.f));
        Slot->SetPosition(FVector2D(-32.f, 27.f));
        Slot->SetAutoSize(true);
    }
    Gold->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(82, 79, 107, .92f), 22.f,
        FFBuildPearl::Color(160, 163, 189, .45f), 1.f));
    for (int32 Index = 1; Index <= 7; ++Index)
        if (auto* Key = Cast<UBorder>(Tree.FindWidget(
            FName(*FString::Printf(TEXT("Border_BuildKeyF%d"), Index)))))
            Key->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(249, 249, 255, .94f),
                5.f, FFBuildPearl::Line, 1.f));
    for (const TCHAR* Name : {TEXT("Button_BuildSettingsClose"),
        TEXT("Button_SettingsGrid"), TEXT("Button_SettingsDirectAdjustment"),
        TEXT("Button_SettingsFacilityRanges"), TEXT("Button_SettingsSynergyPreview"),
        TEXT("Button_SettingsZoomToCursor"), TEXT("Button_SettingsEdgeScroll"),
        TEXT("Button_SettingsSnapMeter"), TEXT("Button_SettingsSnapPrecision"),
        TEXT("Button_SettingsSnapFree"), TEXT("Button_RotateLeft"),
        TEXT("Button_RotateRight"), TEXT("Button_FreeRotateLeft"),
        TEXT("Button_FreeRotateRight"), TEXT("Button_NudgeLeft"),
        TEXT("Button_NudgeRight"), TEXT("Button_NudgeDown"), TEXT("Button_NudgeUp")})
        if (auto* Button = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(Name)))
            StyleDrawerAction(*Button, Cast<UTextBlock>(Button->GetContent()),
                FCString::Strncmp(Name, TEXT("Button_Settings"), 15) == 0);
    if (auto* Close = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(TEXT("Button_BuildSettingsClose"))))
        if (auto* Caption = Cast<UTextBlock>(Close->GetContent()))
            StyleText(*Caption, 22, FFBuildPearl::Ink, TEXT("Bold"));
    if (auto* Angle = Cast<UEditableTextBox>(Tree.FindWidget(TEXT("Editable_Angle"))))
    {
        FEditableTextBoxStyle AngleStyle = Angle->GetWidgetStyle();
        AngleStyle.BackgroundImageNormal = FFBuildPearl::Rounded(
            FFBuildPearl::Color(250, 250, 255), 16.f, FFBuildPearl::Line, 1.f);
        AngleStyle.BackgroundImageHovered = FFBuildPearl::Rounded(
            FLinearColor::White, 16.f, FFBuildPearl::Cyan, 1.f);
        AngleStyle.BackgroundImageFocused = FFBuildPearl::Rounded(
            FLinearColor::White, 16.f, FFBuildPearl::Cyan, 2.f);
        Angle->SetWidgetStyle(AngleStyle);
    }
    Back->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Charcoal, 30.f,
        FFBuildPearl::Color(149, 218, 250, .62f), 1.f));
    {
        FButtonStyle Style = BackButton->GetStyle();
        Style.Normal = FFBuildPearl::Rounded(FLinearColor::Transparent, 29.f);
        Style.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(152, 219, 250, .25f), 29.f);
        Style.Pressed = FFBuildPearl::Rounded(FFBuildPearl::Cyan, 29.f);
        BackButton->SetStyle(Style);
    }

    // The placement summary becomes the right inspector: live name and mode in
    // a blue hero, real price/status/facts in a quiet pearl body below it.
    Info->SetBrush(FFBuildPearl::MaterialBrush(TEXT("Panel")));
    Info->SetBrushColor(FLinearColor::White);
    Info->SetPadding(FMargin(0.f, 0.f, 0.f, 11.f));
    if (auto* Slot = Cast<UCanvasPanelSlot>(InfoSize->Slot))
    {
        Slot->SetAnchors(FAnchors(1.f, 0.f));
        Slot->SetAlignment(FVector2D(1.f, 0.f));
        Slot->SetPosition(FVector2D(-32.f, 100.f));
    }
    if (!Tree.FindWidget(TEXT("Border_BuildPlacementHero")))
    {
        auto* ModeRow = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_ClarityMode")));
        auto* Name = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_FacilityName")));
        if (!ModeRow || !Name) return false;
        ModeRow->RemoveFromParent(); Name->RemoveFromParent();
        auto* HeroColumn = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_BuildPlacementHero"));
        AddColumn(*HeroColumn, *ModeRow, 7.f);
        AddColumn(*HeroColumn, *Name, 0.f);
        auto* HeroRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_BuildPlacementHero"));
        AddRow(*HeroRow, *HeroColumn, 8.f, true);
        auto* Thumbnail = MakeWidget<UImage>(BP, Tree, TEXT("Image_PlacementFacility"), true);
        Thumbnail->SetVisibility(ESlateVisibility::Collapsed);
        auto* ThumbnailFrame = MakeWidget<UBorder>(BP, Tree, TEXT("Border_PlacementFacilityThumb"));
        ThumbnailFrame->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(238, 248, 255, .38f),
            16.f, FFBuildPearl::Color(248, 253, 255, .8f), 1.f));
        ThumbnailFrame->SetPadding(FMargin(4.f));
        ThumbnailFrame->AddChild(Thumbnail);
        auto* ThumbnailSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_PlacementFacilityThumb"));
        ThumbnailSize->SetWidthOverride(96.f);
        ThumbnailSize->SetHeightOverride(92.f);
        ThumbnailSize->AddChild(ThumbnailFrame);
        AddRow(*HeroRow, *ThumbnailSize, 0.f);
        auto* Hero = MakeWidget<UBorder>(BP, Tree, TEXT("Border_BuildPlacementHero"));
        Hero->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(73, 129, 198, .97f), 20.f));
        Hero->SetPadding(FMargin(16.f, 12.f));
        Hero->AddChild(HeroRow);
        Summary->InsertChildAt(0, Hero);
        if (auto* Slot = Cast<UVerticalBoxSlot>(Hero->Slot)) Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 7.f));
    }
    if (auto* Mode = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_Mode"))))
        StyleText(*Mode, 14, FLinearColor::White, TEXT("SemiBold"));
    if (auto* Name = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_FacilityName"))))
        StyleText(*Name, 23, FLinearColor::White, TEXT("Bold"));
    if (auto* Facts = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(TEXT("Button_Facts"))))
    {
        FButtonStyle Style = Facts->GetStyle();
        Style.Normal = FFBuildPearl::Rounded(FFBuildPearl::Color(245, 249, 255, .2f), 16.f);
        Style.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(245, 249, 255, .34f), 16.f);
        Style.Pressed = Style.Hovered;
        Facts->SetStyle(Style);
        if (auto* Caption = Cast<UTextBlock>(Facts->GetContent()))
            StyleText(*Caption, 13, FLinearColor::White, TEXT("SemiBold"));
    }
    for (const TCHAR* Name : {TEXT("Text_Cost"), TEXT("Text_Balance"), TEXT("Text_Legend"),
        TEXT("Text_Footprint"), TEXT("Text_Queue"), TEXT("Text_RangeLegend"),
        TEXT("Text_PlacementAdvice"), TEXT("Text_PlacementObstruction"),
        TEXT("Text_AssistanceMessage")})
        if (auto* Caption = Cast<UTextBlock>(Tree.FindWidget(Name)))
            StyleText(*Caption, FCString::Strcmp(Name, TEXT("Text_Cost")) == 0 ? 19 : 14,
                FCString::Strcmp(Name, TEXT("Text_Cost")) == 0 ? FFBuildPearl::Ink : FFBuildPearl::Muted,
                TEXT("SemiBold"));
    for (int32 Index = 0; Index < Summary->GetChildrenCount(); ++Index)
        if (auto* Child = Summary->GetChildAt(Index))
            if (Child->GetFName() != TEXT("Border_BuildPlacementHero"))
                if (auto* Slot = Cast<UVerticalBoxSlot>(Child->Slot))
                    Slot->SetPadding(FMargin(15.f, 3.f, 15.f, 4.f));

    Dock->SetBrush(FFBuildPearl::MaterialBrush(TEXT("Panel")));
    Dock->SetBrushColor(FLinearColor::White);
    Dock->SetPadding(FMargin(14.f, 10.f));
    for (const TCHAR* Name : {TEXT("Button_Catalog"), TEXT("Button_Help"),
        TEXT("Button_DirectAdjustment"), TEXT("Button_FindNearby"),
        TEXT("Button_NextPlacementProblem")})
        if (auto* Button = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(Name)))
        {
            StyleDrawerAction(*Button, Cast<UTextBlock>(Button->GetContent()), false);
            if (FCString::Strcmp(Name, TEXT("Button_FindNearby")) == 0)
            {
                if (auto* Caption = Cast<UTextBlock>(Button->GetContent()))
                    Caption->SetText(LOCTEXT("NearbyShortcutButton", "[N] 가까운 빈자리"));
                Button->SetToolTipText(LOCTEXT("NearbyShortcutTooltip",
                    "N 또는 클릭으로 가까운 빈자리로 이동합니다. Enter로 배치를 확정하세요."));
            }
        }
    if (auto* Confirm = Cast<UFF_BuildToolsActionButton>(Tree.FindWidget(TEXT("Button_Confirm"))))
    {
        FButtonStyle Style = Confirm->GetStyle();
        Style.Normal = FFBuildPearl::Rounded(FFBuildPearl::Charcoal, 20.f);
        Style.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(83, 82, 111), 20.f,
            FFBuildPearl::Cyan, 1.f);
        Style.Pressed = Style.Hovered;
        Style.Disabled = FFBuildPearl::Rounded(FFBuildPearl::Color(153, 154, 171, .65f), 20.f);
        Confirm->SetStyle(Style);
        Confirm->SelectionStyle = Style;
        Confirm->SelectionStyle.Normal = FFBuildPearl::Rounded(FFBuildPearl::Charcoal,
            20.f, FFBuildPearl::Cyan, 1.f);
        Confirm->SelectionTextColor = FSlateColor(FLinearColor::White);
        if (auto* Caption = Cast<UTextBlock>(Confirm->GetContent()))
            StyleText(*Caption, 17, FLinearColor::White, TEXT("Bold"));
    }
    if (auto* Snap = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ActiveSnap"))))
        StyleText(*Snap, 14, FFBuildPearl::Muted, TEXT("SemiBold"));
    if (auto* Ping = Cast<UBorder>(Tree.FindWidget(TEXT("Border_ContextPing"))))
    {
        Ping->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(248, 251, 255, .94f),
            18.f, FFBuildPearl::Cyan, 1.f));
        Ping->SetBrushColor(FLinearColor::White);
    }
    if (auto* PingText = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ContextPing"))))
        StyleText(*PingText, 14, FFBuildPearl::Ink, TEXT("SemiBold"));

    Metadata.SetValue(&BP, PearlPolishKey, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_TOOLS_PEARL_V5"), BP)) return true;
    Metadata.RemoveValue(&BP, PearlPolishKey);
    return false;
}

bool ApplyPearlReadableCards(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, PearlReadableCardsKey) == TEXT("1")) return true;
    auto* Settings = Cast<UBorder>(BP.WidgetTree->FindWidget(TEXT("Panel_BuildSettings")));
    auto* Info = Cast<UBorder>(BP.WidgetTree->FindWidget(TEXT("Border_PlacementInfo")));
    if (!Settings || !Info) return false;
    BP.Modify();
    BP.WidgetTree->Modify();
    Settings->SetBrush(FFBuildPearl::MaterialBrush(TEXT("Card")));
    Settings->SetBrushColor(FLinearColor::White);
    Info->SetBrush(FFBuildPearl::MaterialBrush(TEXT("Card")));
    Info->SetBrushColor(FLinearColor::White);
    Metadata.SetValue(&BP, PearlReadableCardsKey, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_TOOLS_PEARL_V6"), BP)) return true;
    Metadata.RemoveValue(&BP, PearlReadableCardsKey);
    return false;
}

USizeBox* HelpTextureIcon(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name,
    const TCHAR* TexturePath)
{
    UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, TexturePath);
    if (!Texture) return nullptr;
    auto* Image = MakeWidget<UImage>(BP, Tree, FName(*(FString(Name) + TEXT("Image"))));
    Image->SetBrushFromTexture(Texture, true);
    Image->SetColorAndOpacity(FLinearColor::White);
    auto* Icon = MakeWidget<USizeBox>(BP, Tree, Name);
    Icon->SetWidthOverride(32.f);
    Icon->SetHeightOverride(32.f);
    Icon->AddChild(Image);
    return Icon;
}

USizeBox* HelpMouseIcon(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    auto* Icon = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildHelpMouseIcon"));
    Icon->SetWidthOverride(32.f); Icon->SetHeightOverride(32.f);
    auto* Canvas = MakeWidget<UCanvasPanel>(BP, Tree, TEXT("Canvas_BuildHelpMouseIcon"));
    Icon->AddChild(Canvas);
    auto AddShape = [&](const TCHAR* Name, const FSlateBrush& Brush,
        const FVector2D Position, const FVector2D Size)
    {
        auto* Shape = MakeWidget<UBorder>(BP, Tree, Name);
        Shape->SetBrush(Brush);
        Shape->SetPadding(FMargin(0.f));
        auto* Slot = Canvas->AddChildToCanvas(Shape);
        Slot->SetPosition(Position); Slot->SetSize(Size);
    };
    AddShape(TEXT("Border_BuildHelpMouseBody"),
        FFBuildPearl::Rounded(FFBuildPearl::Color(59, 58, 78, .65f), 11.f,
            FLinearColor::White, 2.f), FVector2D(6.f, 1.f), FVector2D(20.f, 30.f));
    AddShape(TEXT("Border_BuildHelpMouseDivider"),
        FFBuildPearl::Rounded(FLinearColor::White, 1.f),
        FVector2D(15.f, 4.f), FVector2D(2.f, 10.f));
    AddShape(TEXT("Border_BuildHelpMouseClick"),
        FFBuildPearl::Rounded(FFBuildPearl::Cyan, 3.f),
        FVector2D(9.f, 5.f), FVector2D(5.f, 8.f));
    return Icon;
}

USizeBox* HelpArrowIcon(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    auto* Icon = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildHelpArrowsIcon"));
    Icon->SetWidthOverride(32.f); Icon->SetHeightOverride(32.f);
    auto* Canvas = MakeWidget<UCanvasPanel>(BP, Tree, TEXT("Canvas_BuildHelpArrowsIcon"));
    Icon->AddChild(Canvas);
    auto* Texture = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions/T_FF_LH_Icon_Next_White_256.T_FF_LH_Icon_Next_White_256"));
    if (!Texture) return nullptr;
    const FVector2D Positions[] = {FVector2D(20.f, 10.f), FVector2D(0.f, 10.f),
        FVector2D(10.f, 0.f), FVector2D(10.f, 20.f)};
    const float Rotations[] = {0.f, 180.f, -90.f, 90.f};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        auto* Arrow = MakeWidget<UImage>(BP, Tree,
            FName(*FString::Printf(TEXT("Image_BuildHelpArrow%d"), Index)));
        Arrow->SetBrushFromTexture(Texture, true);
        Arrow->SetRenderTransformPivot(FVector2D(.5f, .5f));
        Arrow->SetRenderTransformAngle(Rotations[Index]);
        auto* Slot = Canvas->AddChildToCanvas(Arrow);
        Slot->SetPosition(Positions[Index]); Slot->SetSize(FVector2D(12.f, 12.f));
    }
    auto* Hub = MakeWidget<UBorder>(BP, Tree, TEXT("Border_BuildHelpArrowHub"));
    Hub->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Cyan, 4.f));
    auto* HubSlot = Canvas->AddChildToCanvas(Hub);
    HubSlot->SetPosition(FVector2D(13.f, 13.f)); HubSlot->SetSize(FVector2D(6.f, 6.f));
    return Icon;
}

USizeBox* HelpRotateIcon(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    auto* Icon = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildHelpRotateIcon"));
    Icon->SetWidthOverride(32.f); Icon->SetHeightOverride(32.f);
    auto* Canvas = MakeWidget<UCanvasPanel>(BP, Tree, TEXT("Canvas_BuildHelpRotateIcon"));
    Icon->AddChild(Canvas);
    auto* Ring = MakeWidget<UBorder>(BP, Tree, TEXT("Border_BuildHelpRotateRing"));
    Ring->SetBrush(FFBuildPearl::Rounded(FLinearColor::Transparent, 13.f,
        FLinearColor::White, 2.f));
    auto* RingSlot = Canvas->AddChildToCanvas(Ring);
    RingSlot->SetPosition(FVector2D(3.f, 3.f)); RingSlot->SetSize(FVector2D(26.f, 26.f));
    auto* Texture = LoadObject<UTexture2D>(nullptr,
        TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions/T_FF_LH_Icon_Next_White_256.T_FF_LH_Icon_Next_White_256"));
    if (!Texture) return nullptr;
    auto* Arrow = MakeWidget<UImage>(BP, Tree, TEXT("Image_BuildHelpRotateArrow"));
    Arrow->SetBrushFromTexture(Texture, true);
    Arrow->SetColorAndOpacity(FFBuildPearl::Cyan);
    Arrow->SetRenderTransformPivot(FVector2D(.5f, .5f));
    Arrow->SetRenderTransformAngle(125.f);
    auto* ArrowSlot = Canvas->AddChildToCanvas(Arrow);
    ArrowSlot->SetPosition(FVector2D(20.f, 0.f)); ArrowSlot->SetSize(FVector2D(13.f, 13.f));
    return Icon;
}

bool ApplyIconHelp(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, IconHelpKey) == TEXT("1")) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Top = Cast<UBorder>(Tree.FindWidget(TEXT("Border_BuildTopBar")));
    auto* TopSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildTopBar")));
    auto* TopRow = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_BuildReferenceTopBar")));
    auto* Keycaps = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_BuildToolbarKeycaps")));
    auto* Gold = Cast<UBorder>(Tree.FindWidget(TEXT("Border_BuildToolbarGold")));
    auto* GoldRow = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_BuildToolbarGold")));
    auto* GoldIcon = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildToolbarGoldIcon")));
    auto* GoldValue = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ToolbarGold")));
    auto* Help = Cast<UBorder>(Tree.FindWidget(TEXT("Border_PlacementHelp")));
    auto* HelpSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildWorldHelp")));
    auto* LegacyHelp = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_ControlHelp")));
    if (!Top || !TopSize || !TopRow || !Keycaps || !Gold || !GoldRow || !GoldIcon ||
        !GoldValue || !Help || !HelpSize || !LegacyHelp) return false;
    BP.Modify(); Tree.Modify();

    // The seven buttons now read as large pictograms. The keyboard is no longer
    // the primary way to discover or activate the construction tools.
    TopSize->SetWidthOverride(600.f);
    Top->SetPadding(FMargin(10.f, 7.f));
    Keycaps->SetVisibility(ESlateVisibility::Collapsed);
    for (int32 Index = 0; Index < TopRow->GetChildrenCount(); ++Index)
    {
        if (auto* Cell = Cast<USizeBox>(TopRow->GetChildAt(Index)))
        {
            Cell->SetWidthOverride(76.f);
            Cell->SetHeightOverride(54.f);
            if (auto* Slot = Cast<UHorizontalBoxSlot>(Cell->Slot))
                Slot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
        }
        if (auto* Image = Cast<UImage>(Tree.FindWidget(
            FName(*FString::Printf(TEXT("Image_ToolbarF%d"), Index + 1)))))
            if (auto* IconBounds = Cast<USizeBox>(Image->GetParent()))
            {
                IconBounds->SetWidthOverride(32.f);
                IconBounds->SetHeightOverride(32.f);
            }
    }
    const FText Tips[] = {
        LOCTEXT("LargeToolbarMulti", "시설 다중 선택"),
        LOCTEXT("LargeToolbarGrid", "건축 격자 표시"),
        LOCTEXT("LargeToolbarSnap", "위치 맞춤 간격"),
        LOCTEXT("LargeToolbarRanges", "시설 범위 표시"),
        LOCTEXT("LargeToolbarSettings", "건축 설정"),
        LOCTEXT("LargeToolbarCatalog", "시설 목록"),
        LOCTEXT("LargeToolbarHelp", "건축 조작 안내")};
    const TCHAR* Buttons[] = {TEXT("Button_MultiSelect"), TEXT("Button_Grid"),
        TEXT("Button_SnapCycle"), TEXT("Button_ToolbarRanges"), TEXT("Button_BuildSettings"),
        TEXT("Button_ToolbarCatalog"), TEXT("Button_ToolbarHelp")};
    for (int32 Index = 0; Index < 7; ++Index)
        if (auto* Button = Cast<UButton>(Tree.FindWidget(Buttons[Index])))
            Button->SetToolTipText(Tips[Index]);

    // A single, readable currency badge stays separate at the upper right.
    Gold->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(67, 62, 86, .96f), 29.f,
        FFBuildPearl::Color(248, 211, 146, .7f), 1.f));
    Gold->SetPadding(FMargin(13.f, 7.f));
    Gold->SetHorizontalAlignment(HAlign_Center);
    Gold->SetVerticalAlignment(VAlign_Center);
    GoldIcon->SetWidthOverride(36.f); GoldIcon->SetHeightOverride(36.f);
    if (!Tree.FindWidget(TEXT("Text_ToolbarGoldCaption")))
    {
        auto* Caption = Label(BP, Tree, TEXT("Text_ToolbarGoldCaption"),
            LOCTEXT("GoldCaption", "골드"), 14, FFBuildPearl::Color(248, 211, 146));
        AddRow(*GoldRow, *Caption, 8.f);
        GoldRow->ShiftChild(1, Caption);
    }
    StyleText(*GoldValue, 22, FLinearColor::White, TEXT("Bold"));
    if (auto* Slot = Cast<UCanvasPanelSlot>(Gold->Slot))
    {
        Slot->SetAnchors(FAnchors(1.f, 0.f));
        Slot->SetAlignment(FVector2D(1.f, 0.f));
        Slot->SetPosition(FVector2D(-32.f, 20.f));
        Slot->SetSize(FVector2D(220.f, 58.f));
        Slot->SetAutoSize(false);
    }

    // The legacy text binding remains attached but hidden. These rows use project
    // artwork and vector UMG primitives for mouse, motion and rotation.
    LegacyHelp->RemoveFromParent();
    LegacyHelp->SetVisibility(ESlateVisibility::Collapsed);
    HelpSize->SetWidthOverride(350.f);
    auto* HelpColumn = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Column_BuildIconHelp"), true);
    Help->AddChild(HelpColumn);
    auto* Heading = Label(BP, Tree, TEXT("Text_BuildIconHelpHeading"),
        LOCTEXT("IconHelpHeading", "조작 안내"), 17, FLinearColor::White);
    Heading->SetShadowOffset(FVector2D(1.f, 2.f));
    Heading->SetShadowColorAndOpacity(FFBuildPearl::Color(20, 19, 33, .96f));
    AddColumn(*HelpColumn, *Heading, 8.f);
    auto AddHelpRow = [&](const TCHAR* RowName, const TCHAR* LabelName,
        USizeBox* Icon, const FText& Caption) -> bool
    {
        if (!Icon) return false;
        auto* Row = MakeWidget<UHorizontalBox>(BP, Tree, RowName, true);
        AddRow(*Row, *Icon, 10.f);
        auto* Text = Label(BP, Tree, LabelName, Caption, 15, FLinearColor::White, true);
        Text->SetShadowOffset(FVector2D(1.f, 2.f));
        Text->SetShadowColorAndOpacity(FFBuildPearl::Color(20, 19, 33, .96f));
        AddRow(*Row, *Text, 0.f, true);
        AddColumn(*HelpColumn, *Row, 4.f);
        if (Row->GetFName() == TEXT("Row_BuildHelpNearby"))
        {
            Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            Row->bHiddenInDesigner = false;
        }
        return true;
    };
    // The keyboard refinement below fills this lane with the same physical N
    // keycap used by the narrow shortcut migration for already-authored assets.
    auto* NearbyLane = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildHelpNearbyIcon"));
    if (!AddHelpRow(TEXT("Row_BuildHelpPrimary"), TEXT("Text_BuildHelpPrimary"),
            HelpMouseIcon(BP, Tree), LOCTEXT("HelpPrimary", "클릭해 시설 배치")) ||
        !AddHelpRow(TEXT("Row_BuildHelpRotate"), TEXT("Text_BuildHelpRotate"),
            HelpRotateIcon(BP, Tree), LOCTEXT("HelpRotate", "시설 방향 회전")) ||
        !AddHelpRow(TEXT("Row_BuildHelpNearby"), TEXT("Text_BuildHelpNearby"),
            NearbyLane, LOCTEXT("HelpNearby", "가까운 빈자리로 이동")) ||
        !AddHelpRow(TEXT("Row_BuildHelpMove"), TEXT("Text_BuildHelpMove"),
            HelpArrowIcon(BP, Tree), LOCTEXT("HelpMove", "카메라 이동")) ||
        !AddHelpRow(TEXT("Row_BuildHelpPrecision"), TEXT("Text_BuildHelpPrecision"),
            HelpTextureIcon(BP, Tree, TEXT("Size_BuildHelpPrecisionIcon"),
                TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions/T_FF_LH_Icon_Locate_White_256.T_FF_LH_Icon_Locate_White_256")),
            LOCTEXT("HelpPrecision", "정밀 위치 맞춤")) ||
        !AddHelpRow(TEXT("Row_BuildHelpGrid"), TEXT("Text_BuildHelpGrid"),
            HelpTextureIcon(BP, Tree, TEXT("Size_BuildHelpGridIcon"),
                TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_Density_White_256.T_FF_LH_Icon_Density_White_256")),
            LOCTEXT("HelpGrid", "격자 보기")) ||
        !AddHelpRow(TEXT("Row_BuildHelpCatalog"), TEXT("Text_BuildHelpCatalog"),
            HelpTextureIcon(BP, Tree, TEXT("Size_BuildHelpCatalogIcon"),
                TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation/T_FF_LH_Icon_Facilities_White_256.T_FF_LH_Icon_Facilities_White_256")),
            LOCTEXT("HelpCatalog", "시설 목록")) ||
        !AddHelpRow(TEXT("Row_BuildHelpPause"), TEXT("Text_BuildHelpPause"),
            HelpTextureIcon(BP, Tree, TEXT("Size_BuildHelpPauseIcon"),
                TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Actions/T_FF_LH_Icon_Pause_White_256.T_FF_LH_Icon_Pause_White_256")),
            LOCTEXT("HelpPause", "일시정지"))) return false;
    AddColumn(*HelpColumn, *LegacyHelp, 0.f);
    Help->SetPadding(FMargin(0.f));
    Metadata.SetValue(&BP, IconHelpKey, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_TOOLS_ICON_HELP_V7"), BP)) return true;
    Metadata.RemoveValue(&BP, IconHelpKey);
    return false;
}

bool ApplyActionLanguage(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, ActionLanguageKey) == TEXT("1")) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    BP.Modify(); Tree.Modify();
    struct FCaption { const TCHAR* Name; const TCHAR* Value; };
    const FCaption Captions[] = {
        {TEXT("Label_RotateLeft"), TEXT("왼쪽")},
        {TEXT("Label_RotateRight"), TEXT("오른쪽")},
        {TEXT("Label_FreeRotateLeft"), TEXT("자유 좌회전")},
        {TEXT("Label_FreeRotateRight"), TEXT("자유 우회전")},
        {TEXT("Label_NudgeLeft"), TEXT("←")},
        {TEXT("Label_NudgeRight"), TEXT("→")},
        {TEXT("Label_NudgeDown"), TEXT("↓")},
        {TEXT("Label_NudgeUp"), TEXT("↑")},
        {TEXT("Text_RotationHint"), TEXT("45°씩 회전 · 자유 회전 지원")},
        {TEXT("Text_PrecisionHint"), TEXT("정밀 조정 후 배치 확정 버튼을 누르세요")}
    };
    for (const FCaption& Caption : Captions)
        if (auto* Text = Cast<UTextBlock>(Tree.FindWidget(Caption.Name)))
            Text->SetText(FText::FromString(Caption.Value));
    for (const TCHAR* Name : {TEXT("Button_Catalog"), TEXT("Button_DirectAdjustment")})
        if (auto* Button = Cast<UButton>(Tree.FindWidget(Name)))
            if (auto* Text = Cast<UTextBlock>(Button->GetContent()))
                Text->SetText(FCString::Strcmp(Name, TEXT("Button_Catalog")) == 0
                    ? LOCTEXT("CatalogAction", "시설 목록")
                    : LOCTEXT("DirectAction", "직접 조정"));
    if (auto* Confirm = Cast<UButton>(Tree.FindWidget(TEXT("Button_Confirm"))))
        Confirm->SetToolTipText(LOCTEXT("ConfirmActionTip",
            "현재 조정한 위치와 각도로 설치 또는 이동을 확정합니다"));
    Metadata.SetValue(&BP, ActionLanguageKey, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_TOOLS_ACTION_LANGUAGE_V8"), BP)) return true;
    Metadata.RemoveValue(&BP, ActionLanguageKey);
    return false;
}

bool ApplyKeyboardHelp(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, KeyboardHelpKey) == TEXT("1")) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Help = Cast<UBorder>(Tree.FindWidget(TEXT("Border_PlacementHelp")));
    auto* HelpSize = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildWorldHelp")));
    auto* Column = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Column_BuildIconHelp")));
    auto* Heading = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_BuildIconHelpHeading")));
    if (!Help || !HelpSize || !Column || !Heading) return false;
    BP.Modify(); Tree.Modify();
    Column->bIsVariable = true;

    // An inset face and lower lip make these input labels unmistakably physical keys.
    // Toolbar pictograms remain actions; only this guide teaches keyboard bindings.
    auto Keycap = [&](const TCHAR* KeyName, const TCHAR* Caption) -> USizeBox*
    {
        const FString Stem = FString(TEXT("BuildHelpKey_")) + KeyName;
        auto* Size = MakeWidget<USizeBox>(BP, Tree, FName(*(TEXT("Size_") + Stem)));
        Size->SetMinDesiredWidth(25.f);
        Size->SetHeightOverride(28.f);
        auto* Edge = MakeWidget<UBorder>(BP, Tree, FName(*(TEXT("Border_") + Stem)));
        Edge->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(158, 169, 193), 5.f));
        Edge->SetPadding(FMargin(1.f, 1.f, 1.f, 3.f));
        auto* Face = MakeWidget<UBorder>(BP, Tree, FName(*(TEXT("Face_") + Stem)));
        Face->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(247, 249, 255), 4.f));
        Face->SetPadding(FMargin(5.f, 1.f));
        Face->SetHorizontalAlignment(HAlign_Center);
        Face->SetVerticalAlignment(VAlign_Center);
        Face->AddChild(Label(BP, Tree, FName(*(TEXT("Text_") + Stem)),
            FText::FromString(Caption), 14, FFBuildPearl::Ink, true));
        Edge->AddChild(Face);
        Size->AddChild(Edge);
        return Size;
    };
    auto Keys = [&](const TCHAR* IconName, const TArray<TPair<FString, FString>>& KeyList,
        const TCHAR* KeysRowName = nullptr) -> bool
    {
        auto* Icon = Cast<USizeBox>(Tree.FindWidget(IconName));
        if (!Icon) return false;
        // Detaching a UMG child does not remove its Blueprint variable GUID or
        // WidgetTree ownership. Retire the entire former pictogram subtree.
        TArray<UWidget*> Retired;
        UWidgetTree::GetChildWidgets(Icon, Retired);
        Icon->ClearChildren();
        for (int32 Index = Retired.Num() - 1; Index >= 0; --Index)
        {
            UWidget* Widget = Retired[Index];
            BP.OnVariableRemoved(Widget->GetFName());
            Widget->RemoveFromParent();
            Widget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        }
        auto* Row = MakeWidget<UHorizontalBox>(BP, Tree,
            KeysRowName ? FName(KeysRowName) : FName(*(FString(IconName) + TEXT("Keys"))));
        for (const auto& Key : KeyList) AddRow(*Row, *Keycap(*Key.Key, *Key.Value), 4.f);
        Icon->SetWidthOverride(124.f);
        Icon->SetHeightOverride(30.f);
        auto* Slot = CastChecked<USizeBoxSlot>(Icon->AddChild(Row));
        Slot->SetHorizontalAlignment(HAlign_Left);
        Slot->SetVerticalAlignment(VAlign_Center);
        return true;
    };
    if (!Keys(TEXT("Size_BuildHelpRotateIcon"), {{TEXT("Q"), TEXT("Q")}, {TEXT("E"), TEXT("E")}}) ||
        !Keys(TEXT("Size_BuildHelpNearbyIcon"), {{TEXT("N"), TEXT("N")}}, TEXT("Row_BuildHelpNearbyKeys")) ||
        !Keys(TEXT("Size_BuildHelpArrowsIcon"), {{TEXT("W"), TEXT("W")}, {TEXT("A"), TEXT("A")},
            {TEXT("S"), TEXT("S")}, {TEXT("D"), TEXT("D")}}) ||
        !Keys(TEXT("Size_BuildHelpPrecisionIcon"), {{TEXT("Ctrl"), TEXT("Ctrl")}}) ||
        !Keys(TEXT("Size_BuildHelpGridIcon"), {{TEXT("G"), TEXT("G")}}) ||
        !Keys(TEXT("Size_BuildHelpCatalogIcon"), {{TEXT("B"), TEXT("B")}}) ||
        !Keys(TEXT("Size_BuildHelpPauseIcon"), {{TEXT("Esc"), TEXT("Esc")}})) return false;

    auto* Mouse = Cast<USizeBox>(Tree.FindWidget(TEXT("Size_BuildHelpMouseIcon")));
    auto* Primary = Cast<UHorizontalBox>(Tree.FindWidget(TEXT("Row_BuildHelpPrimary")));
    if (!Mouse || !Primary) return false;
    Mouse->RemoveFromParent();
    auto* MouseRow = MakeWidget<UHorizontalBox>(BP, Tree, TEXT("Row_BuildHelpMouseInput"));
    AddRow(*MouseRow, *Mouse, 5.f);
    AddRow(*MouseRow, *Label(BP, Tree, TEXT("Text_BuildHelpMouseInput"),
        LOCTEXT("HelpMouseInput", "좌클릭"), 12, FFBuildPearl::Cyan), 0.f);
    auto* MouseSize = MakeWidget<USizeBox>(BP, Tree, TEXT("Size_BuildHelpMouseInput"));
    MouseSize->SetWidthOverride(124.f);
    MouseSize->AddChild(MouseRow);
    AddRow(*Primary, *MouseSize, 10.f);
    Primary->ShiftChild(0, MouseSize);

    Heading->SetText(LOCTEXT("KeyboardHelpTitle", "조작 안내"));
    StyleText(*Heading, 16, FLinearColor::White, TEXT("SemiBold"));
    auto* Hint = Label(BP, Tree, TEXT("Text_BuildHelpKeyboardHint"),
        LOCTEXT("KeyboardHelpHint", "표시된 키를 누르면 실행됩니다"), 11, FFBuildPearl::Cyan);
    AddColumn(*Column, *Hint, 10.f);
    Column->ShiftChild(1, Hint);
    for (const TCHAR* Name : {TEXT("Text_BuildHelpPrimary"), TEXT("Text_BuildHelpRotate"),
        TEXT("Text_BuildHelpNearby"),
        TEXT("Text_BuildHelpMove"), TEXT("Text_BuildHelpPrecision"), TEXT("Text_BuildHelpGrid"),
        TEXT("Text_BuildHelpCatalog"), TEXT("Text_BuildHelpPause")})
        if (auto* Text = Cast<UTextBlock>(Tree.FindWidget(Name)))
        {
            StyleText(*Text, 13, FLinearColor::White, TEXT("Medium"));
            Text->SetAutoWrapText(true);
            Text->SetShadowOffset(FVector2D::ZeroVector);
        }
    if (auto* Text = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_BuildHelpCatalog"))))
        Text->SetText(LOCTEXT("HelpToggleCatalog", "시설 목록 열기 / 닫기"));
    if (auto* Text = Cast<UTextBlock>(Tree.FindWidget(TEXT("Text_BuildHelpPause"))))
        Text->SetText(LOCTEXT("HelpOptions", "일시정지 · 옵션"));
    Help->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(60, 59, 79, .92f), 16.f,
        FFBuildPearl::Color(219, 222, 237, .45f), 1.f));
    Help->SetBrushColor(FLinearColor::White);
    Help->SetPadding(FMargin(14.f, 12.f));
    Column->RemoveFromParent();
    auto* Scroll = MakeWidget<UScrollBox>(BP, Tree, TEXT("Scroll_BuildHelp"));
    Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
    Scroll->SetClipping(EWidgetClipping::ClipToBounds);
    Scroll->AddChild(Column);
    Help->AddChild(Scroll);
    HelpSize->SetWidthOverride(360.f);
    HelpSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (auto* Slot = Cast<UCanvasPanelSlot>(HelpSize->Slot)) Slot->SetPosition(FVector2D(32.f, 104.f));
    // Freshly rebuilt help satisfies the same one-time shortcut migration, so a
    // later narrow command can reuse it without a conflicting duplicate row.
    if (auto* NearbyButton = Cast<UButton>(Tree.FindWidget(TEXT("Button_FindNearby"))))
        if (auto* Caption = Cast<UTextBlock>(NearbyButton->GetContent()))
            if (Caption->GetText().ToString() == TEXT("[N] 가까운 빈자리"))
                Metadata.SetValue(&BP, NearbyShortcutKey, TEXT("1"));
    Metadata.SetValue(&BP, KeyboardHelpKey, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_TOOLS_KEYBOARD_HELP_V9"), BP)) return true;
    Metadata.RemoveValue(&BP, KeyboardHelpKey);
    Metadata.RemoveValue(&BP, NearbyShortcutKey);
    return false;
}


// Simple, consistently weighted native UMG artwork stays editable in the saved WBP.
// No raster imports, private fonts, or runtime-created toolbar controls are required.
USizeBox* SimpleToolbarIcon(UWidgetBlueprint& BP, UWidgetTree& Tree, int32 Index)
{
    const FString Stem = FString::Printf(TEXT("BuildToolbarLine%d"), Index);
    auto* Bounds = MakeWidget<USizeBox>(BP, Tree, FName(*(TEXT("Size_") + Stem)));
    Bounds->SetWidthOverride(32.f); Bounds->SetHeightOverride(32.f);
    auto* Canvas = MakeWidget<UCanvasPanel>(BP, Tree,
        Index == 1 ? FName(TEXT("Canvas_BuildToolbarFacilityPlus")) : FName(*(TEXT("Canvas_") + Stem)));
    Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
    Bounds->AddChild(Canvas);
    int32 Part = 0;
    auto Shape = [&](FVector2D Position, FVector2D Size, bool bFilled, float Radius) -> UBorder*
    {
        auto* Border = MakeWidget<UBorder>(BP, Tree, FName(*FString::Printf(TEXT("Border_%s_%d"), *Stem, Part++)));
        Border->SetPadding(FMargin(0.f));
        Border->SetBrush(FFBuildPearl::Rounded(bFilled ? FLinearColor::White : FLinearColor::Transparent,
            Radius, bFilled ? FLinearColor::Transparent : FLinearColor::White, bFilled ? 0.f : 2.f));
        auto* Slot = Canvas->AddChildToCanvas(Border);
        Slot->SetPosition(Position); Slot->SetSize(Size);
        return Border;
    };
    auto Line = [&](FVector2D From, FVector2D To)
    {
        const FVector2D Delta = To - From;
        UBorder* Stroke = Shape(From - FVector2D(0.f, 1.f), FVector2D(Delta.Size(), 2.f), true, 1.f);
        Stroke->SetRenderTransformPivot(FVector2D(0.f, .5f));
        Stroke->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)));
    };
    switch (Index)
    {
    case 1: // Facility plus: roof, facade and door; unmistakably a building instead of a person.
        Line({3.f, 13.f}, {12.f, 5.f}); Line({12.f, 5.f}, {21.f, 13.f});
        Line({5.f, 12.f}, {5.f, 28.f}); Line({5.f, 28.f}, {22.f, 28.f}); Line({22.f, 18.f}, {22.f, 28.f});
        Shape({11.f, 20.f}, {5.f, 8.f}, false, 1.f);
        Line({26.f, 6.f}, {26.f, 16.f}); Line({21.f, 11.f}, {31.f, 11.f});
        break;
    case 2: // A compact four-cell grid.
        Shape({5.f, 5.f}, {22.f, 22.f}, false, 2.f);
        Line({16.f, 5.f}, {16.f, 27.f}); Line({5.f, 16.f}, {27.f, 16.f});
        break;
    case 3: // Position alignment.
        Shape({11.f, 11.f}, {10.f, 10.f}, false, 2.f);
        Line({16.f, 3.f}, {16.f, 8.f}); Line({16.f, 24.f}, {16.f, 29.f});
        Line({3.f, 16.f}, {8.f, 16.f}); Line({24.f, 16.f}, {29.f, 16.f});
        break;
    case 4: // Facility influence radius.
        Shape({4.f, 4.f}, {24.f, 24.f}, false, 12.f);
        Shape({11.f, 11.f}, {10.f, 10.f}, false, 5.f);
        break;
    case 5: // Three adjustable settings sliders.
        Line({5.f, 8.f}, {27.f, 8.f}); Line({5.f, 16.f}, {27.f, 16.f}); Line({5.f, 24.f}, {27.f, 24.f});
        Shape({10.f, 5.f}, {4.f, 6.f}, true, 1.f); Shape({20.f, 13.f}, {4.f, 6.f}, true, 1.f);
        Shape({8.f, 21.f}, {4.f, 6.f}, true, 1.f);
        break;
    case 6: // Facility catalog cards, visually distinct from the add/select building.
        Shape({4.f, 5.f}, {10.f, 9.f}, false, 2.f); Shape({18.f, 5.f}, {10.f, 9.f}, false, 2.f);
        Shape({4.f, 18.f}, {10.f, 9.f}, false, 2.f); Shape({18.f, 18.f}, {10.f, 9.f}, false, 2.f);
        break;
    case 7:
    {
        Shape({3.f, 3.f}, {26.f, 26.f}, false, 13.f);
        auto* Question = Label(BP, Tree, TEXT("Text_BuildToolbarHelpMark"), FText::FromString(TEXT("?")), 21, FLinearColor::White);
        Question->SetJustification(ETextJustify::Center);
        auto* QuestionSlot = Canvas->AddChildToCanvas(Question);
        QuestionSlot->SetPosition({3.f, 0.f}); QuestionSlot->SetSize({26.f, 30.f});
        break;
    }
    default: break;
    }
    return Bounds;
}

bool ApplyConstructionEditingHelp(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, ConstructionHelpKey) == TEXT("1")) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    auto* Column = Cast<UVerticalBox>(Tree.FindWidget(TEXT("Column_BuildIconHelp")));
    if (!Column) return false;
    const TCHAR* Buttons[] = {TEXT("Button_MultiSelect"), TEXT("Button_Grid"), TEXT("Button_SnapCycle"),
        TEXT("Button_ToolbarRanges"), TEXT("Button_BuildSettings"), TEXT("Button_ToolbarCatalog"), TEXT("Button_ToolbarHelp")};
    for (const TCHAR* Name : Buttons) if (!Tree.FindWidget(Name)) return false;
    BP.Modify(); Tree.Modify();
    for (int32 Index = 0; Index < 7; ++Index)
    {
        auto* Button = CastChecked<UButton>(Tree.FindWidget(Buttons[Index]));
        UWidget* Old = Button->GetContent();
        TArray<UWidget*> Retired;
        if (Old) { UWidgetTree::GetChildWidgets(Old, Retired); Retired.Add(Old); }
        Button->ClearChildren();
        for (UWidget* Widget : Retired)
        {
            BP.OnVariableRemoved(Widget->GetFName());
            Widget->RemoveFromParent();
            Widget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        }
        auto* Slot = CastChecked<UButtonSlot>(Button->AddChild(SimpleToolbarIcon(BP, Tree, Index + 1)));
        Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center);
    }
    auto Keycap = [&](const TCHAR* Name, const TCHAR* Caption) -> USizeBox*
    {
        const FString Stem = FString(TEXT("BuildHelpKey_")) + Name;
        auto* Size = MakeWidget<USizeBox>(BP, Tree, FName(*(TEXT("Size_") + Stem)));
        Size->SetMinDesiredWidth(25.f); Size->SetHeightOverride(28.f);
        auto* Edge = MakeWidget<UBorder>(BP, Tree, FName(*(TEXT("Border_") + Stem)));
        Edge->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(158, 169, 193), 5.f));
        Edge->SetPadding(FMargin(1.f, 1.f, 1.f, 3.f));
        auto* Face = MakeWidget<UBorder>(BP, Tree, FName(*(TEXT("Face_") + Stem)));
        Face->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(247, 249, 255), 4.f));
        Face->SetPadding(FMargin(5.f, 1.f)); Face->SetHorizontalAlignment(HAlign_Center); Face->SetVerticalAlignment(VAlign_Center);
        Face->AddChild(Label(BP, Tree, FName(*(TEXT("Text_") + Stem)), FText::FromString(Caption), 14, FFBuildPearl::Ink, true));
        Edge->AddChild(Face); Size->AddChild(Edge);
        return Size;
    };
    auto HelpRow = [&](const TCHAR* Stem, const TArray<TPair<FString, FString>>& Keys, const FText& Caption, const TCHAR* CaptionName = nullptr) -> UHorizontalBox*
    {
        auto* Row = MakeWidget<UHorizontalBox>(BP, Tree, FName(*(FString(TEXT("Row_")) + Stem)));
        auto* KeysRow = MakeWidget<UHorizontalBox>(BP, Tree, FName(*(FString(TEXT("Keys_")) + Stem)));
        for (const auto& Key : Keys) AddRow(*KeysRow, *Keycap(*Key.Key, *Key.Value), 4.f);
        auto* Bounds = MakeWidget<USizeBox>(BP, Tree, FName(*(FString(TEXT("Size_")) + Stem)));
        Bounds->SetWidthOverride(124.f); Bounds->SetHeightOverride(30.f); Bounds->AddChild(KeysRow);
        AddRow(*Row, *Bounds, 10.f);
        auto* CaptionWidget = Label(BP, Tree, CaptionName ? FName(CaptionName) : FName(*(FString(TEXT("Text_")) + Stem)), Caption, 13, FLinearColor::White, true);
        CaptionWidget->SetAutoWrapText(true);
        AddRow(*Row, *CaptionWidget, 0.f, true);
        return Row;
    };
    auto HistoryAction = [&](const TCHAR* ButtonName, const TCHAR* Stem, const TCHAR* CtrlName,
        const TCHAR* Letter, const FText& Caption, int32 Position)
    {
        auto* Button = MakeWidget<UFF_BuildToolsActionButton>(BP, Tree, ButtonName, true);
        FButtonStyle Style;
        Style.Normal = FFBuildPearl::Rounded(FLinearColor::Transparent, 5.f);
        Style.Hovered = FFBuildPearl::Rounded(FFBuildPearl::Color(152, 190, 218, .18f), 5.f);
        Style.Pressed = FFBuildPearl::Rounded(FFBuildPearl::Color(152, 190, 218, .28f), 5.f);
        Style.Disabled = Style.Normal;
        Style.NormalPadding = FMargin(0.f); Style.PressedPadding = FMargin(0.f);
        Button->SetStyle(Style);
        Button->SetToolTipText(FText::Format(LOCTEXT("ClickHelpAction", "클릭하거나 표시된 단축키로 {0}"), Caption));
        auto* Row = HelpRow(Stem, {{CtrlName, TEXT("Ctrl")}, {Letter, Letter}}, Caption);
        auto* ButtonSlot = CastChecked<UButtonSlot>(Button->AddChild(Row));
        ButtonSlot->SetPadding(FMargin(0.f));
        ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
        ButtonSlot->SetVerticalAlignment(VAlign_Center);
        if (auto* CaptionWidget = Cast<UTextBlock>(Row->GetChildAt(1))) CaptionWidget->SetAutoWrapText(false);
        AddColumn(*Column, *Button, 4.f);
        Column->ShiftChild(Position, Button);
    };
    HistoryAction(TEXT("Button_BuildHelpUndo"), TEXT("BuildHelpUndo"), TEXT("UndoCtrl"), TEXT("Z"),
        LOCTEXT("HelpUndoAction", "실행 취소"), 2);
    HistoryAction(TEXT("Button_BuildHelpRedo"), TEXT("BuildHelpRedo"), TEXT("RedoCtrl"), TEXT("Y"),
        LOCTEXT("HelpRedoAction", "다시 실행"), 3);
    auto* QueueGroup = MakeWidget<UVerticalBox>(BP, Tree, TEXT("Row_BuildHelpQueue"), true);
    QueueGroup->SetVisibility(ESlateVisibility::Collapsed);
    AddColumn(*QueueGroup, *HelpRow(TEXT("BuildHelpQueueKeys"), {{TEXT("1"), TEXT("7")}, {TEXT("2"), TEXT("8")},
        {TEXT("3"), TEXT("9")}, {TEXT("4"), TEXT("0")}}, LOCTEXT("HelpQueueAction", "대기열 모양 선택"), TEXT("Text_BuildHelpQueue")), 3.f);
    auto* QueueHint = Label(BP, Tree, TEXT("Text_BuildHelpQueueHint"),
        LOCTEXT("QueueDigits", "7 기본 · 8 좌측 · 9 우측 · 0 U자"), 12, FFBuildPearl::Cyan, true);
    QueueHint->SetAutoWrapText(true);
    AddColumn(*QueueGroup, *QueueHint, 5.f);
    AddColumn(*Column, *QueueGroup, 4.f); Column->ShiftChild(5, QueueGroup);
    Metadata.SetValue(&BP, ConstructionHelpKey, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_TOOLS_CONSTRUCTION_HELP_V10"), BP)) return true;
    Metadata.RemoveValue(&BP, ConstructionHelpKey);
    return false;
}

// Text-only migration: preserve every existing widget, binding, font, brush and slot.
bool ApplyQueueShortcutKeys(UWidgetBlueprint& BP)
{
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    if (Metadata.GetValue(&BP, QueueShortcutKey) == TEXT("1")) return true;
    UWidgetTree& Tree = *BP.WidgetTree;
    const TCHAR* Names[] = {TEXT("Text_BuildHelpKey_1"), TEXT("Text_BuildHelpKey_2"),
        TEXT("Text_BuildHelpKey_3"), TEXT("Text_BuildHelpKey_4"), TEXT("Text_BuildHelpQueueHint")};
    const FText Captions[] = {FText::FromString(TEXT("7")), FText::FromString(TEXT("8")),
        FText::FromString(TEXT("9")), FText::FromString(TEXT("0")),
        LOCTEXT("QueueShortcutDigits7890", "7 기본 · 8 좌측 · 9 우측 · 0 U자")};
    TArray<UTextBlock*> Labels;
    for (const TCHAR* Name : Names)
    {
        auto* LabelWidget = Cast<UTextBlock>(Tree.FindWidget(Name));
        if (!LabelWidget) return false;
        Labels.Add(LabelWidget);
    }
    BP.Modify();
    for (int32 Index = 0; Index < Labels.Num(); ++Index)
    {
        Labels[Index]->Modify();
        Labels[Index]->SetText(Captions[Index]);
    }
    Metadata.SetValue(&BP, QueueShortcutKey, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_TOOLS_QUEUE_SHORTCUT_V11"), BP)) return true;
    Metadata.RemoveValue(&BP, QueueShortcutKey);
    return false;
}

bool RemoveTopWallet(UWidgetBlueprint& BP)
{
    constexpr const TCHAR* Key = TEXT("FF.BuildReferenceTools.RemoveTopWalletV12");
    auto& Metadata = BP.GetOutermost()->GetMetaData();
    auto* Gold = Cast<UBorder>(BP.WidgetTree->FindWidget(TEXT("Border_BuildToolbarGold")));
    if (!Gold) return false;
    if (Metadata.GetValue(&BP, Key) == TEXT("1") && Gold->GetVisibility() == ESlateVisibility::Collapsed)
        return true;
    BP.Modify();
    Gold->Modify();
    Gold->SetVisibility(ESlateVisibility::Collapsed);
    Metadata.SetValue(&BP, Key, TEXT("1"));
    if (SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_TOOLS_REMOVE_TOP_WALLET_V12"), BP)) return true;
    Metadata.RemoveValue(&BP, Key);
    return false;
}

void Apply()
{
    UWidgetBlueprint* BP = LoadExistingBlueprint(FFBuildUIAssetPaths::BuildToolsPackage);
    if (!BP || BP->ParentClass != UFF_BuildToolsWidget::StaticClass() || !BP->WidgetTree)
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_REFERENCE_TOOLS_FAIL: expected saved BuildTools WBP"));
        return;
    }
    auto& Metadata = BP->GetOutermost()->GetMetaData();
    if (Metadata.GetValue(BP, VersionKey) == TEXT("1"))
    {
        if (RefinePresentation(*BP) && AddNavigationAndProjectSettings(*BP) &&
            ApplyPearlToolbarAndDrawer(*BP) && ApplyPearlPolish(*BP) &&
            ApplyPearlReadableCards(*BP) && ApplyIconHelp(*BP) &&
            ApplyActionLanguage(*BP) && ApplyKeyboardHelp(*BP) && ApplyConstructionEditingHelp(*BP) &&
            ApplyQueueShortcutKeys(*BP) && RemoveTopWallet(*BP))
        {
            UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_REFERENCE_TOOLS_REUSE"));
        }
        else
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_REFERENCE_TOOLS_FAIL: refinement failed"));
        }
        return;
    }
    if (!ApplyLayout(*BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_REFERENCE_TOOLS_FAIL: Clarity widget tree contract missing"));
        return;
    }
    Metadata.SetValue(BP, VersionKey, TEXT("1"));
    if (!SaveNewBlueprint(TEXT("FF_BUILD_REFERENCE_TOOLS"), *BP))
    {
        Metadata.RemoveValue(BP, VersionKey);
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_REFERENCE_TOOLS_FAIL: save failed"));
    }
    else if (!RefinePresentation(*BP) || !AddNavigationAndProjectSettings(*BP) ||
        !ApplyPearlToolbarAndDrawer(*BP) || !ApplyPearlPolish(*BP) ||
        !ApplyPearlReadableCards(*BP) || !ApplyIconHelp(*BP) ||
        !ApplyActionLanguage(*BP) || !ApplyKeyboardHelp(*BP) || !ApplyConstructionEditingHelp(*BP) ||
        !ApplyQueueShortcutKeys(*BP) || !RemoveTopWallet(*BP))
    {
        UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_REFERENCE_TOOLS_FAIL: refinement failed"));
    }
}

FAutoConsoleCommand Command(TEXT("FF.ApplyBuildReferenceTools"),
    TEXT("Add an interactive build-mode top bar and settings drawer to the saved WBP."),
    FConsoleCommandDelegate::CreateStatic(&Apply));
}

#undef LOCTEXT_NAMESPACE
#endif
