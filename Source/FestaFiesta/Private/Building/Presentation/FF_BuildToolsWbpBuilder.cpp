// One-time scaffold for the editable LightTeal toolbar. Existing WBP assets are reused;
// subsequent layout changes belong in the Designer, not a destructive regeneration command.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"

#define LOCTEXT_NAMESPACE "FFBuildToolsWbpBuilder"

namespace FFBuildToolsWbpBuilder
{
    using namespace FFUIWbpBuilder;
    const FString ToolsFolder(TEXT("/Game/FestaFiesta/UI/Building"));
    const FString ToolsPackage = ToolsFolder / TEXT("WBP_FF_BuildTools");

    FSlateBrush MakeBrush(const FLinearColor Color, const float Radius,
        const FLinearColor Outline = FLinearColor::Transparent, const float OutlineWidth = 0.0f)
    {
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.TintColor = FSlateColor(Color);
        Brush.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Outline), OutlineWidth);
        return Brush;
    }

    void AddToRow(UHorizontalBox& Row, UWidget& Child, const float RightPadding = 4.0f,
        const bool bFill = false)
    {
        UHorizontalBoxSlot* Slot = Row.AddChildToHorizontalBox(&Child);
        Slot->SetPadding(FMargin(0.0f, 0.0f, RightPadding, 0.0f));
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    }

    UTextBlock* AddLabel(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UHorizontalBox& Row,
        const FName Name, const FText Label)
    {
        UTextBlock* Text = MakeWidget<UTextBlock>(Blueprint, Tree, Name);
        StyleText(*Text, 11, UFF_UIPalette::Get()->InGame.InkMuted, TEXT("Medium"));
        Text->SetText(Label);
        Text->SetVisibility(ESlateVisibility::HitTestInvisible);
        AddToRow(Row, *Text, 7.0f);
        return Text;
    }

    void AddButton(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UHorizontalBox& Row,
        const FName Name, const FName LabelName, const FText Label, const FText Tooltip,
        const float Width, const bool bLabelVariable = false, const float Height = 32.0f)
    {
        const FFF_UIPaletteInGame& Palette = UFF_UIPalette::Get()->InGame;
        UFF_BuildToolsActionButton* Button = MakeWidget<UFF_BuildToolsActionButton>(Blueprint, Tree, Name, true);
        UTextBlock* Text = MakeWidget<UTextBlock>(Blueprint, Tree, LabelName, bLabelVariable);
        USizeBox* Size = MakeWidget<USizeBox>(Blueprint, Tree,
            FName(*FString::Printf(TEXT("Size_%s"), *Name.ToString())));
        Size->SetWidthOverride(Width);
        Size->SetHeightOverride(Height);
        Size->AddChild(Button);
        FButtonStyle Style;
        Style.Normal = MakeBrush(Palette.PaperSunken, 5.0f, Palette.Line, 1.0f);
        Style.Hovered = MakeBrush(Palette.Hover, 5.0f, Palette.Teal, 1.0f);
        Style.Pressed = MakeBrush(Palette.TealSoft, 5.0f, Palette.Teal, 1.0f);
        Style.Disabled = MakeBrush(Palette.PaperSunken, 5.0f, Palette.Line, 1.0f);
        Style.NormalPadding = FMargin(4.0f, 2.0f);
        Style.PressedPadding = Style.NormalPadding;
        Button->SetStyle(Style);
        Button->SetToolTipText(Tooltip);
        Text->SetText(Label);
        StyleText(*Text, 12, Palette.Ink, TEXT("Medium"));
        Text->SetJustification(ETextJustify::Center);
        Text->SetVisibility(ESlateVisibility::HitTestInvisible);
        UButtonSlot* Slot = CastChecked<UButtonSlot>(Button->AddChild(Text));
        Slot->SetHorizontalAlignment(HAlign_Center);
        Slot->SetVerticalAlignment(VAlign_Center);
        AddToRow(Row, *Size);
    }

    void AddDivider(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, UHorizontalBox& Row, const FName Name)
    {
        USizeBox* Size = MakeWidget<USizeBox>(Blueprint, Tree, Name);
        UBorder* Line = MakeWidget<UBorder>(Blueprint, Tree,
            FName(*FString::Printf(TEXT("Border_%s"), *Name.ToString())));
        Size->SetWidthOverride(1.0f);
        Size->SetHeightOverride(22.0f);
        Line->SetBrushColor(UFF_UIPalette::Get()->InGame.Line);
        Line->SetVisibility(ESlateVisibility::HitTestInvisible);
        Size->AddChild(Line);
        UHorizontalBoxSlot* Slot = Row.AddChildToHorizontalBox(Size);
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetPadding(FMargin(8.0f, 0.0f, 12.0f, 0.0f));
    }

    bool BuildLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
    {
        const FFF_UIPaletteInGame& Palette = UFF_UIPalette::Get()->InGame;
        UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Canvas_Root"));
        USizeBox* Size = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Size_Tools"));
        UBorder* Panel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Tools"), true);
        UVerticalBox* Content = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("Vertical_Tools"));
        UHorizontalBox* Header = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Row_Status"));
        UHorizontalBox* Controls = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Row_Controls"));
        UTextBlock* Mode = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Mode"), true);
        UTextBlock* Legend = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Legend"), true);
        Tree.RootWidget = Root;
        Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        Size->SetWidthOverride(800.0f);
        Size->SetHeightOverride(82.0f);
        Size->AddChild(Panel);
        Panel->SetBrush(MakeBrush(Palette.Paper, 10.0f, Palette.LineStrong, 1.0f));
        Panel->SetBrushColor(FLinearColor::White);
        Panel->SetPadding(FMargin(14.0f, 9.0f));
        Panel->SetVisibility(ESlateVisibility::Visible);
        Panel->AddChild(Content);
        UCanvasPanelSlot* Position = Root->AddChildToCanvas(Size);
        Position->SetAnchors(FAnchors(0.5f, 1.0f));
        Position->SetAlignment(FVector2D(0.5, 1.0));
        Position->SetPosition(FVector2D(0.0, -110.0));
        Position->SetAutoSize(true);

        Mode->SetText(LOCTEXT("Mode", "건축 도구"));
        StyleText(*Mode, 13, Palette.TealDeep, TEXT("SemiBold"));
        Legend->SetText(LOCTEXT("Legend", "격자: 빈 공간 · 막힌 곳에는 격자가 없어요"));
        StyleText(*Legend, 11, Palette.InkMuted, TEXT("Regular"));
        Legend->SetJustification(ETextJustify::Right);
        Legend->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        Mode->SetVisibility(ESlateVisibility::HitTestInvisible);
        AddToRow(*Header, *Mode, 16.0f);
        AddToRow(*Header, *Legend, 0.0f, true);
        AddButton(Blueprint, Tree, *Header, TEXT("Button_Confirm"), TEXT("Text_Confirm"),
            LOCTEXT("Confirm", "설치"), LOCTEXT("ConfirmTip", "현재 조정한 위치와 각도로 설치 또는 이동을 확정합니다"),
            88.0f, true, 24.0f);
        UVerticalBoxSlot* HeaderSlot = Content->AddChildToVerticalBox(Header);
        HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 7.0f, 7.0f));
        Content->AddChildToVerticalBox(Controls)->SetHorizontalAlignment(HAlign_Center);

        AddButton(Blueprint, Tree, *Controls, TEXT("Button_Grid"), TEXT("Text_Grid"),
            LOCTEXT("Grid", "격자 켜짐"), LOCTEXT("GridTip", "빈 건축 공간의 격자 표시/숨기기 [G]"), 82.0f, true);
        AddDivider(Blueprint, Tree, *Controls, TEXT("Divider_Snap"));
        AddLabel(Blueprint, Tree, *Controls, TEXT("Label_Snap"), LOCTEXT("Snap", "맞춤"));
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_SnapMeter"), TEXT("Text_SnapMeter"),
            LOCTEXT("Meter", "1m"), LOCTEXT("MeterTip", "1m 간격으로 가지런하게 배치합니다"), 43.0f, true);
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_SnapPrecision"), TEXT("Text_SnapPrecision"),
            LOCTEXT("Precision", "25cm"), LOCTEXT("PrecisionTip", "25cm 간격으로 세밀하게 배치합니다 [Ctrl을 누르는 동안]"), 52.0f, true);
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_SnapFree"), TEXT("Text_SnapFree"),
            LOCTEXT("Free", "자유"), LOCTEXT("FreeTip", "1cm 단위로 자유롭게 배치합니다 [Alt를 누르는 동안]"), 43.0f, true);
        AddDivider(Blueprint, Tree, *Controls, TEXT("Divider_Rotation"));
        AddLabel(Blueprint, Tree, *Controls, TEXT("Label_Rotation"), LOCTEXT("Rotation", "회전"));
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_RotateLeft"), TEXT("Text_RotateLeft"),
            LOCTEXT("RotateLeft", "−45°"), LOCTEXT("RotateLeftTip", "시설을 왼쪽으로 45도 회전합니다"), 45.0f);
        UEditableTextBox* Angle = MakeWidget<UEditableTextBox>(Blueprint, Tree, TEXT("Editable_Angle"), true);
        USizeBox* AngleSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Size_Angle"));
        AngleSize->SetWidthOverride(62.0f);
        AngleSize->SetHeightOverride(32.0f);
        AngleSize->AddChild(Angle);
        FEditableTextBoxStyle AngleStyle = Angle->GetWidgetStyle();
        AngleStyle.BackgroundImageNormal = MakeBrush(Palette.White, 5.0f, Palette.Line, 1.0f);
        AngleStyle.BackgroundImageHovered = MakeBrush(Palette.White, 5.0f, Palette.Teal, 1.0f);
        AngleStyle.BackgroundImageFocused = MakeBrush(Palette.White, 5.0f, Palette.TealDeep, 2.0f);
        AngleStyle.BackgroundImageReadOnly = MakeBrush(Palette.PaperSunken, 5.0f, Palette.Line, 1.0f);
        AngleStyle.ForegroundColor = FSlateColor(Palette.Ink);
        AngleStyle.Padding = FMargin(5.0f, 4.0f);
        AngleStyle.TextStyle.SetFont(Mode->GetFont());
        Angle->SetWidgetStyle(AngleStyle);
        Angle->SetText(FText::FromString(TEXT("0.0")));
        Angle->SetJustification(ETextJustify::Center);
        Angle->SetSelectAllTextWhenFocused(true);
        Angle->SetClearKeyboardFocusOnCommit(false);
        Angle->SetToolTipText(LOCTEXT("AngleTip", "0~360도 입력 후 Enter로 적용합니다. 소수점도 사용할 수 있어요. Esc는 입력 취소입니다."));
        AddToRow(*Controls, *AngleSize, 1.0f);
        AddLabel(Blueprint, Tree, *Controls, TEXT("Label_Degrees"), FText::FromString(TEXT("°")));
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_RotateRight"), TEXT("Text_RotateRight"),
            LOCTEXT("RotateRight", "+45°"), LOCTEXT("RotateRightTip", "시설을 오른쪽으로 45도 회전합니다"), 45.0f);
        AddDivider(Blueprint, Tree, *Controls, TEXT("Divider_Nudge"));
        AddLabel(Blueprint, Tree, *Controls, TEXT("Label_Nudge"), LOCTEXT("Nudge", "이동"));
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_NudgeLeft"), TEXT("Text_NudgeLeft"),
            FText::FromString(TEXT("←")), LOCTEXT("NudgeLeftTip", "현재 맞춤 간격만큼 구역의 왼쪽으로 이동합니다"), 30.0f);
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_NudgeDown"), TEXT("Text_NudgeDown"),
            FText::FromString(TEXT("↓")), LOCTEXT("NudgeDownTip", "현재 맞춤 간격만큼 구역의 아래로 이동합니다"), 30.0f);
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_NudgeUp"), TEXT("Text_NudgeUp"),
            FText::FromString(TEXT("↑")), LOCTEXT("NudgeUpTip", "현재 맞춤 간격만큼 구역의 위로 이동합니다"), 30.0f);
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_NudgeRight"), TEXT("Text_NudgeRight"),
            FText::FromString(TEXT("→")), LOCTEXT("NudgeRightTip", "현재 맞춤 간격만큼 구역의 오른쪽으로 이동합니다"), 30.0f);
        return true;
    }

    void BuildToolsUMG()
    {
        EnsureWidgetBlueprint(TEXT("FF_BUILD_TOOLS_UMG"), ToolsPackage, ToolsFolder,
            *UFF_BuildToolsWidget::StaticClass(), BuildLayout);
    }

    FAutoConsoleCommand BuildToolsCommand(TEXT("FF.BuildToolsUMG"),
        TEXT("Creates the editable LightTeal build toolbar and preserves existing Designer assets."),
        FConsoleCommandDelegate::CreateStatic(&BuildToolsUMG));
}

#undef LOCTEXT_NAMESPACE
#endif
