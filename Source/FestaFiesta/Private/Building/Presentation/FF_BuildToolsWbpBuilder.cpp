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
#include "UObject/StrongObjectPtr.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "FFBuildToolsWbpBuilder"

namespace FFBuildToolsWbpBuilder
{
    using namespace FFUIWbpBuilder;
    const FString ToolsFolder(TEXT("/Game/FestaFiesta/UI/Building"));
    const FString ToolsPackage = ToolsFolder / TEXT("WBP_FF_BuildTools");
    const TCHAR* ApprovedLayoutKey = TEXT("FFApprovedBuildToolsLayoutVersion");
    const TCHAR* ApprovedLayoutVersion = TEXT("2");

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
        StyleText(*Text, 12, UFF_UIPalette::Get()->InGame.InkMuted, TEXT("Medium"));
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
        Button->bUseSelectionStyle = bLabelVariable;
        Button->SelectionStyle = Style;
        Button->SelectionStyle.Normal = MakeBrush(Palette.TealDeep, 5.0f, Palette.TealDeep, 1.0f);
        Button->SelectionStyle.Hovered = MakeBrush(Palette.Teal, 5.0f, Palette.TealDeep, 1.0f);
        Button->SelectionStyle.Pressed = MakeBrush(Palette.TealDeep, 5.0f, Palette.TealDeep, 1.0f);
        Button->SelectionTextColor = FSlateColor(Palette.White);
        Button->SetToolTipText(Tooltip);
        Text->SetText(Label);
        StyleText(*Text, 13, Palette.Ink, TEXT("Medium"));
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
        UHorizontalBox* Rotation = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Row_Rotation"));
        UHorizontalBox* Status = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("Row_PlacementStatus"));
        UTextBlock* Mode = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Mode"), true);
        UTextBlock* Legend = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Legend"), true);
        Tree.RootWidget = Root;
        Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        Size->SetWidthOverride(1000.0f);
        Size->SetHeightOverride(210.0f);
        Size->AddChild(Panel);
        Panel->SetBrush(MakeBrush(Palette.Paper, 10.0f, Palette.LineStrong, 1.0f));
        Panel->SetBrushColor(FLinearColor::White);
        Panel->SetPadding(FMargin(18.0f, 12.0f));
        Panel->SetVisibility(ESlateVisibility::Visible);
        Panel->AddChild(Content);
        UCanvasPanelSlot* Position = Root->AddChildToCanvas(Size);
        Position->SetAnchors(FAnchors(0.5f, 1.0f));
        Position->SetAlignment(FVector2D(0.5, 1.0));
        Position->SetPosition(FVector2D(0.0, -70.0));
        Position->SetAutoSize(true);

        Mode->SetText(LOCTEXT("Mode", "시설 배치"));
        StyleText(*Mode, 13, Palette.TealDeep, TEXT("SemiBold"));
        Legend->SetText(LOCTEXT("Legend", "배치 가능 · 초록색 영역에 설치할 수 있어요"));
        StyleText(*Legend, 12, Palette.TealDeep, TEXT("Regular"));
        Legend->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        Mode->SetVisibility(ESlateVisibility::HitTestInvisible);
        AddToRow(*Header, *Mode, 16.0f);
        UTextBlock* FacilityName = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_FacilityName"), true);
        FacilityName->SetText(LOCTEXT("FacilityExample", "바다 공방"));
        StyleText(*FacilityName, 17, Palette.Ink, TEXT("SemiBold"));
        FacilityName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        AddToRow(*Header, *FacilityName, 12.0f, true);
        UTextBlock* Cost = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Cost"), true);
        Cost->SetText(LOCTEXT("CostExample", "설치 300 Gold"));
        StyleText(*Cost, 13, Palette.TealDeep, TEXT("SemiBold"));
        AddToRow(*Header, *Cost, 18.0f);
        AddButton(Blueprint, Tree, *Header, TEXT("Button_Catalog"), TEXT("Label_Catalog"),
            LOCTEXT("Catalog", "시설 목록  B"), LOCTEXT("CatalogTip", "현재 배치 또는 이동을 취소하고 시설 목록으로 돌아갑니다"), 112.0f);
        AddButton(Blueprint, Tree, *Header, TEXT("Button_Confirm"), TEXT("Text_Confirm"),
            LOCTEXT("Confirm", "배치 확정"), LOCTEXT("ConfirmTip", "현재 조정한 위치와 각도로 설치 또는 이동을 확정합니다 [Enter]"),
            112.0f, true, 32.0f);
        UVerticalBoxSlot* HeaderSlot = Content->AddChildToVerticalBox(Header);
        HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
        AddToRow(*Status, *Legend, 0.0f, true);
        AddButton(Blueprint, Tree, *Status, TEXT("Button_Grid"), TEXT("Text_Grid"),
            LOCTEXT("Grid", "격자 켜짐"), LOCTEXT("GridTip", "빈 건축 공간의 격자 표시/숨기기 [G]"), 96.0f, true, 24.0f);
        Content->AddChildToVerticalBox(Status)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
        Content->AddChildToVerticalBox(Controls)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

        AddLabel(Blueprint, Tree, *Controls, TEXT("Label_Snap"), LOCTEXT("Snap", "정밀 이동"));
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_SnapMeter"), TEXT("Text_SnapMeter"),
            LOCTEXT("Meter", "1m"), LOCTEXT("MeterTip", "1m 간격으로 가지런하게 배치합니다"), 54.0f, true);
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_SnapPrecision"), TEXT("Text_SnapPrecision"),
            LOCTEXT("Precision", "25cm"), LOCTEXT("PrecisionTip", "25cm 간격으로 세밀하게 배치합니다 [Ctrl을 누르는 동안]"), 62.0f, true);
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_SnapFree"), TEXT("Text_SnapFree"),
            LOCTEXT("Free", "1cm"), LOCTEXT("FreeTip", "1cm 단위로 자유롭게 배치합니다 [Alt를 누르는 동안]"), 54.0f, true);
        UTextBlock* ActiveSnap = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_ActiveSnap"), true);
        ActiveSnap->SetText(LOCTEXT("ActiveSnapExample", "현재 25cm"));
        StyleText(*ActiveSnap, 12, Palette.TealDeep, TEXT("SemiBold"));
        AddToRow(*Controls, *ActiveSnap, 12.0f, true);
        AddDivider(Blueprint, Tree, *Controls, TEXT("Divider_Nudge"));
        AddLabel(Blueprint, Tree, *Controls, TEXT("Label_Nudge"), LOCTEXT("Nudge", "구역 X / Y"));
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_NudgeLeft"), TEXT("Text_NudgeLeft"),
            FText::FromString(TEXT("−X")), LOCTEXT("NudgeLeftTip", "현재 맞춤 간격만큼 구역의 X축 음의 방향으로 이동합니다"), 46.0f);
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_NudgeRight"), TEXT("Text_NudgeRight"),
            FText::FromString(TEXT("+X")), LOCTEXT("NudgeRightTip", "현재 맞춤 간격만큼 구역의 X축 양의 방향으로 이동합니다"), 46.0f);
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_NudgeDown"), TEXT("Text_NudgeDown"),
            FText::FromString(TEXT("−Y")), LOCTEXT("NudgeDownTip", "현재 맞춤 간격만큼 구역의 Y축 음의 방향으로 이동합니다"), 46.0f);
        AddButton(Blueprint, Tree, *Controls, TEXT("Button_NudgeUp"), TEXT("Text_NudgeUp"),
            FText::FromString(TEXT("+Y")), LOCTEXT("NudgeUpTip", "현재 맞춤 간격만큼 구역의 Y축 양의 방향으로 이동합니다"), 46.0f);
        Content->AddChildToVerticalBox(Rotation)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
        AddLabel(Blueprint, Tree, *Rotation, TEXT("Label_Rotation"), LOCTEXT("Rotation", "현재 각도"));
        AddButton(Blueprint, Tree, *Rotation, TEXT("Button_RotateLeft"), TEXT("Text_RotateLeft"),
            LOCTEXT("RotateLeftV2", "Q 왼쪽"), LOCTEXT("RotateLeftTip", "왼쪽 방향의 다음 45도 맞춤 각도로 회전합니다 [Q]"), 76.0f);
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
        AddToRow(*Rotation, *AngleSize, 1.0f);
        AddLabel(Blueprint, Tree, *Rotation, TEXT("Label_Degrees"), FText::FromString(TEXT("°")));
        AddButton(Blueprint, Tree, *Rotation, TEXT("Button_RotateRight"), TEXT("Text_RotateRight"),
            LOCTEXT("RotateRightV2", "E 오른쪽"), LOCTEXT("RotateRightTip", "오른쪽 방향의 다음 45도 맞춤 각도로 회전합니다 [E]"), 76.0f);
        AddDivider(Blueprint, Tree, *Rotation, TEXT("Divider_FreeRotation"));
        AddButton(Blueprint, Tree, *Rotation, TEXT("Button_FreeRotateLeft"), TEXT("Label_FreeRotateLeft"),
            LOCTEXT("FreeRotateLeftV2", "Shift + Q"), LOCTEXT("FreeRotateLeftTip", "버튼을 누르고 있는 동안 왼쪽으로 자유 회전합니다. 키보드는 Shift를 먼저 누른 채 Q를 누르세요."), 124.0f);
        AddButton(Blueprint, Tree, *Rotation, TEXT("Button_FreeRotateRight"), TEXT("Label_FreeRotateRight"),
            LOCTEXT("FreeRotateRightV2", "Shift + E"), LOCTEXT("FreeRotateRightTip", "버튼을 누르고 있는 동안 오른쪽으로 자유 회전합니다. 키보드는 Shift를 먼저 누른 채 E를 누르세요."), 124.0f);
        AddLabel(Blueprint, Tree, *Rotation, TEXT("Label_FreeHold"), LOCTEXT("FreeHold", "누르는 동안 자유 회전"));
        UTextBlock* Hint = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_RotationHint"), true);
        Hint->SetText(LOCTEXT("RotationHint", "Q / E  45° 맞춤  ·  Shift + Q / E  누르는 동안 자유 회전"));
        StyleText(*Hint, 11, Palette.InkMuted, TEXT("Regular"));
        Content->AddChildToVerticalBox(Hint);
        UTextBlock* PrecisionHint = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_PrecisionHint"));
        PrecisionHint->SetText(LOCTEXT("PrecisionHint", "Ctrl 누르는 동안 25cm  ·  Alt 누르는 동안 1cm  ·  미세 조정 후 이 패널에서 배치를 확정하세요"));
        StyleText(*PrecisionHint, 11, Palette.InkMuted, TEXT("Regular"));
        Content->AddChildToVerticalBox(PrecisionHint)->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
        Blueprint.GetOutermost()->GetMetaData().SetValue(&Blueprint, ApprovedLayoutKey, ApprovedLayoutVersion);
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

    void RefreshBuildToolsUMG()
    {
        UWidgetBlueprint* Blueprint = LoadExistingBlueprint(ToolsPackage);
        if (!Blueprint)
        {
            BuildToolsUMG();
            return;
        }
        if (!Blueprint->WidgetTree || Blueprint->ParentClass != UFF_BuildToolsWidget::StaticClass())
        {
            UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_TOOLS_UMG_FAIL: unexpected toolbar parent/tree"));
            return;
        }
        if (Blueprint->GetOutermost()->GetMetaData().GetValue(Blueprint, ApprovedLayoutKey) == ApprovedLayoutVersion)
        {
            UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_TOOLS_UMG_REUSE: approved Designer layout preserved"));
            return;
        }
        if (Blueprint->GetOutermost()->GetMetaData().GetValue(Blueprint, ApprovedLayoutKey) == TEXT("1"))
        {
            // A text-only repair for the known v1 font fallback. Preserve all Designer
            // edits, including custom labels and button widths, without replacing the tree.
            struct FLabelRepair { FName Name; const TCHAR* OldText; FText NewText; FName SizeName; };
            const FLabelRepair Repairs[] = {
                { TEXT("Text_RotateLeft"), TEXT("Q  ↶"), LOCTEXT("RotateLeftV2", "Q 왼쪽"), TEXT("Size_Button_RotateLeft") },
                { TEXT("Text_RotateRight"), TEXT("E  ↷"), LOCTEXT("RotateRightV2", "E 오른쪽"), TEXT("Size_Button_RotateRight") },
                { TEXT("Label_FreeRotateLeft"), TEXT("Shift + Q  ↶"), LOCTEXT("FreeRotateLeftV2", "Shift + Q"), NAME_None },
                { TEXT("Label_FreeRotateRight"), TEXT("Shift + E  ↷"), LOCTEXT("FreeRotateRightV2", "Shift + E"), NAME_None }
            };
            TArray<TPair<UTextBlock*, FText>> PreviousLabels;
            TArray<USizeBox*> ResizedButtons;
            for (const FLabelRepair& Repair : Repairs)
            {
                UTextBlock* Label = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(Repair.Name));
                if (!Label || Label->GetText().ToString() != Repair.OldText) { continue; }
                PreviousLabels.Emplace(Label, Label->GetText());
                Label->SetText(Repair.NewText);
                USizeBox* ButtonSize = Repair.SizeName.IsNone() ? nullptr :
                    Cast<USizeBox>(Blueprint->WidgetTree->FindWidget(Repair.SizeName));
                if (ButtonSize && ButtonSize->IsWidthOverride() && FMath::IsNearlyEqual(ButtonSize->GetWidthOverride(), 54.0f))
                {
                    ButtonSize->SetWidthOverride(76.0f);
                    ResizedButtons.Add(ButtonSize);
                }
            }
            Blueprint->GetOutermost()->GetMetaData().SetValue(Blueprint, ApprovedLayoutKey, ApprovedLayoutVersion);
            if (!SaveNewBlueprint(TEXT("FF_BUILD_TOOLS_UMG"), *Blueprint))
            {
                for (const auto& Previous : PreviousLabels) { Previous.Key->SetText(Previous.Value); }
                for (USizeBox* ButtonSize : ResizedButtons) { ButtonSize->SetWidthOverride(54.0f); }
                Blueprint->GetOutermost()->GetMetaData().SetValue(Blueprint, ApprovedLayoutKey, TEXT("1"));
                SaveNewBlueprint(TEXT("FF_BUILD_TOOLS_UMG"), *Blueprint);
                return;
            }
            UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_TOOLS_UMG_MIGRATE_V2: compatible key labels saved; Designer layout preserved"));
            return;
        }
        // This explicit migration touches only the approved building toolbar. Normal
        // create-if-missing remains non-destructive; later Designer edits stay authoritative.
        TStrongObjectPtr<UWidgetTree> PreviousTree(Blueprint->WidgetTree);
        const auto PreviousGuids = Blueprint->WidgetVariableNameToGuidMap;
        const FString PreviousLayoutVersion = Blueprint->GetOutermost()->GetMetaData().GetValue(Blueprint, ApprovedLayoutKey);
        PreviousTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
        Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, TEXT("WidgetTree"), RF_Transactional);
        Blueprint->WidgetVariableNameToGuidMap.Reset();
        if (!BuildLayout(*Blueprint, *Blueprint->WidgetTree) || !SaveNewBlueprint(TEXT("FF_BUILD_TOOLS_UMG"), *Blueprint))
        {
            Blueprint->WidgetTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
            PreviousTree->Rename(TEXT("WidgetTree"), Blueprint, REN_DontCreateRedirectors | REN_NonTransactional);
            Blueprint->WidgetTree = PreviousTree.Get();
            Blueprint->WidgetVariableNameToGuidMap = PreviousGuids;
            Blueprint->GetOutermost()->GetMetaData().SetValue(Blueprint, ApprovedLayoutKey, *PreviousLayoutVersion);
            SaveNewBlueprint(TEXT("FF_BUILD_TOOLS_UMG"), *Blueprint);
            return;
        }
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_TOOLS_UMG_DONE: approved placement toolbar saved"));
    }
    FAutoConsoleCommand RefreshBuildToolsCommand(TEXT("FF.RefreshBuildToolsUMG"),
        TEXT("Migrates the build-tools Designer layout once; preserves subsequent Designer edits and other team UI."),
        FConsoleCommandDelegate::CreateStatic(&RefreshBuildToolsUMG));
}

#undef LOCTEXT_NAMESPACE
#endif
