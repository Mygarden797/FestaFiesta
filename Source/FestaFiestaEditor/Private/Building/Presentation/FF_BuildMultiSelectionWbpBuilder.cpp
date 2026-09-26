// Editable construction group workspace. Only the explicit update command restyles saved assets.
#include "CoreMinimal.h"
#if WITH_EDITOR
#include "Building/Presentation/FF_BuildMultiSelectionWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Building/Presentation/FF_BuildTypography.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UI/Core/FF_BuildPearlStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"

#define LOCTEXT_NAMESPACE "FFBuildMultiSelectionWbpBuilder"
namespace FFBuildMultiSelectionWbpBuilder
{
using namespace FFUIWbpBuilder;
const FString Folder(FFBuildUIAssetPaths::ControlsFolder);
const FString Package(Folder / TEXT("WBP_FF_BuildMultiSelection"));
const FString CardPackage(Folder / TEXT("WBP_FF_BuildSelectedFacilityCard"));
const FName VersionKey(TEXT("FF.MultiSelection.EditableWorkspaceVersion"));
const FString Version(TEXT("1"));
const FLinearColor Teal = FFBuildPearl::Color(39, 108, 100);
const FLinearColor SoftTeal = FFBuildPearl::Color(226, 244, 239);
const FLinearColor Light = FFBuildPearl::Color(251, 253, 252);
const FLinearColor Danger = FFBuildPearl::Color(149, 78, 84);

template <typename T>
T* Get(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, bool bVariable = false)
{
    if (UWidget* Existing = Tree.FindWidget(Name))
    {
        T* Typed = Cast<T>(Existing);
        if (!Typed) UE_LOG(LogFFUIWbpBuilder, Error, TEXT("FF_BUILD_MULTI_UMG: incompatible widget type: %s"), *Name.ToString());
        if (Typed)
        {
            Typed->Modify();
            Typed->bIsVariable |= bVariable;
            // Existing widgets already own stable GUIDs used by Designer/event graph references.
            if (!BP.WidgetVariableNameToGuidMap.Contains(Name)) BP.OnVariableAdded(Name);
        }
        return Typed;
    }
    return MakeWidget<T>(BP, Tree, Name, bVariable);
}
void InRow(UHorizontalBox& Row, UWidget& Child, float Padding = 8.f, bool bFill = false)
{
    Child.RemoveFromParent();
    UHorizontalBoxSlot* Slot = Row.AddChildToHorizontalBox(&Child);
    Slot->SetPadding(FMargin(0.f, 0.f, Padding, 0.f));
    Slot->SetVerticalAlignment(VAlign_Center);
    Slot->SetSize(FSlateChildSize(bFill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
}
void InColumn(UVerticalBox& Column, UWidget& Child, float BottomPadding = 0.f)
{
    Child.RemoveFromParent();
    Column.AddChildToVerticalBox(&Child)->SetPadding(FMargin(0.f, 0.f, 0.f, BottomPadding));
}
UTextBlock* Label(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, const FText& Text,
    int32 Size = 14, FLinearColor Color = FFBuildPearl::Ink, bool bVariable = false)
{
    UTextBlock* Result = Get<UTextBlock>(BP, Tree, Name, bVariable);
    if (!Result) return nullptr;
    Result->SetFont(FFBuildTypography::Make(FFBuildTypography::ERole::Label, Size));
    Result->SetText(Text);
    Result->SetColorAndOpacity(Color);
    Result->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    Result->SetVisibility(ESlateVisibility::HitTestInvisible);
    return Result;
}
UButton* Action(UWidgetBlueprint& BP, UWidgetTree& Tree, FName Name, const FText& Caption, int32 Tone = 0)
{
    UButton* Button = Get<UButton>(BP, Tree, Name, true);
    if (!Button) return nullptr;
    const bool bPrimary = Tone == 1;
    const FLinearColor Fill = bPrimary ? Teal : Tone == 2 ? SoftTeal : Light;
    const FLinearColor Ink = bPrimary ? FLinearColor::White : Tone == 3 ? Danger : FFBuildPearl::Ink;
    FButtonStyle Style;
    Style.Normal = FFBuildPearl::Rounded(Fill, 12.f, bPrimary ? Teal : FFBuildPearl::Line, 1.f);
    Style.Hovered = FFBuildPearl::Rounded(bPrimary ? FFBuildPearl::Color(49, 128, 117) : SoftTeal, 12.f, Teal, 1.f);
    Style.Pressed = FFBuildPearl::Rounded(bPrimary ? FFBuildPearl::Color(30, 88, 80) : FFBuildPearl::Color(209, 232, 224), 12.f, Teal, 1.f);
    Style.Disabled = FFBuildPearl::Rounded(FFBuildPearl::Color(241, 244, 244), 12.f, FFBuildPearl::Line, 1.f);
    Style.NormalForeground = Ink;
    Style.HoveredForeground = Ink;
    Style.PressedForeground = Ink;
    Style.DisabledForeground = FFBuildPearl::Muted;
    Style.NormalPadding = Style.PressedPadding = FMargin(14.f, 9.f);
    Button->SetStyle(Style);
    UTextBlock* Text = Cast<UTextBlock>(Button->GetContent());
    if (!Text)
    {
        // Preserve custom content/event objects when upgrading an already customized button.
        if (Button->GetContent()) return Button;
        Text = Label(BP, Tree, FName(*(TEXT("Text_") + Name.ToString())), Caption);
        Button->AddChild(Text);
    }
    Text->SetText(Caption);
    Text->SetFont(FFBuildTypography::Make(FFBuildTypography::ERole::Label, 14));
    Text->SetColorAndOpacity(FSlateColor::UseForeground());
    Text->SetVisibility(ESlateVisibility::HitTestInvisible);
    UButtonSlot* Slot = CastChecked<UButtonSlot>(Text->Slot);
    Slot->SetHorizontalAlignment(HAlign_Center);
    Slot->SetVerticalAlignment(VAlign_Center);
    return Button;
}

bool BuildCard(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    USizeBox* Size = Get<USizeBox>(BP, Tree, TEXT("Size_SelectedFacilityCard"));
    UBorder* Surface = Get<UBorder>(BP, Tree, TEXT("Border_SelectedFacilityCard"));
    UHorizontalBox* Row = Get<UHorizontalBox>(BP, Tree, TEXT("Row_FacilityCard"));
    USizeBox* IconSize = Get<USizeBox>(BP, Tree, TEXT("Size_FacilityThumbnail"));
    UBorder* IconBackground = Get<UBorder>(BP, Tree, TEXT("Border_FacilityThumbnail"));
    UImage* Icon = Get<UImage>(BP, Tree, TEXT("Image_FacilityThumbnail"), true);
    UVerticalBox* Copy = Get<UVerticalBox>(BP, Tree, TEXT("Column_FacilityCardCopy"));
    Tree.RootWidget = Size;
    Size->SetWidthOverride(196.f); Size->SetHeightOverride(58.f);
    Size->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Size->SetContent(Surface);
    Surface->SetBrush(FFBuildPearl::Rounded(Light, 12.f, FFBuildPearl::Color(199, 226, 216), 1.f));
    Surface->SetPadding(FMargin(8.f)); Surface->SetContent(Row);
    IconSize->SetWidthOverride(40.f); IconSize->SetHeightOverride(40.f); IconSize->SetContent(IconBackground);
    IconBackground->SetBrush(FFBuildPearl::Rounded(SoftTeal, 8.f)); IconBackground->SetPadding(FMargin(5.f)); IconBackground->SetContent(Icon);
    Icon->SetColorAndOpacity(FLinearColor::White); Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    InRow(*Row, *IconSize); InRow(*Row, *Copy, 0.f, true);
    InColumn(*Copy, *Label(BP, Tree, TEXT("Text_FacilityIndex"), LOCTEXT("CardIndex", "01 · 선택한 시설"), 11, Teal, true), 2.f);
    InColumn(*Copy, *Label(BP, Tree, TEXT("Text_FacilityName"), LOCTEXT("CardName", "먹거리 부스"), 14, FFBuildPearl::Ink, true));
    return true;
}

void StylePresetFields(UEditableTextBox& Name, UComboBoxString& Combo)
{
    FEditableTextBoxStyle Edit = Name.GetWidgetStyle();
    Edit.SetFont(FFBuildTypography::Make(FFBuildTypography::ERole::Body, 14));
    Edit.BackgroundImageNormal = FFBuildPearl::Rounded(Light, 10.f, FFBuildPearl::Line, 1.f);
    Edit.BackgroundImageHovered = FFBuildPearl::Rounded(Light, 10.f, Teal, 1.f);
    Edit.BackgroundImageFocused = FFBuildPearl::Rounded(Light, 10.f, Teal, 2.f);
    Edit.BackgroundImageReadOnly = FFBuildPearl::Rounded(SoftTeal, 10.f);
    Edit.ForegroundColor = FFBuildPearl::Ink;
    Edit.FocusedForegroundColor = FFBuildPearl::Ink;
    Edit.Padding = FMargin(12.f, 9.f);
    Name.SetWidgetStyle(Edit);
    Name.SetHintText(LOCTEXT("PresetHint", "예: 먹거리 거리"));
    Name.SetToolTipText(LOCTEXT("PresetNameTip", "1~40자. 같은 이름으로 저장하면 해당 프리셋을 덮어씁니다."));
    FComboBoxStyle ComboStyle = Combo.GetWidgetStyle();
    FButtonStyle Button;
    Button.Normal = FFBuildPearl::Rounded(Light, 10.f, FFBuildPearl::Line, 1.f);
    Button.Hovered = FFBuildPearl::Rounded(SoftTeal, 10.f, Teal, 1.f);
    Button.Pressed = FFBuildPearl::Rounded(SoftTeal, 10.f, Teal, 2.f);
    Button.Disabled = FFBuildPearl::Rounded(FFBuildPearl::Color(241, 244, 244), 10.f, FFBuildPearl::Line, 1.f);
    Button.NormalForeground = Button.HoveredForeground = Button.PressedForeground = FFBuildPearl::Ink;
    Button.DisabledForeground = FFBuildPearl::Muted;
    ComboStyle.ComboButtonStyle.ButtonStyle = Button;
    ComboStyle.ComboButtonStyle.MenuBorderBrush = FFBuildPearl::Rounded(Light, 12.f, FFBuildPearl::Line, 1.f);
    ComboStyle.ComboButtonStyle.MenuBorderPadding = FMargin(6.f);
    ComboStyle.ComboButtonStyle.DownArrowImage.TintColor = Teal;
    Combo.SetWidgetStyle(ComboStyle);
    Combo.SetContentPadding(FMargin(12.f, 9.f));
    Combo.SetMaxListHeight(220.f);
    Combo.SetToolTipText(LOCTEXT("PickPreset", "저장된 배치 프리셋을 선택하세요."));
    FTableRowStyle Item = Combo.GetItemStyle();
    Item.EvenRowBackgroundBrush = Item.OddRowBackgroundBrush = FFBuildPearl::Rounded(Light, 7.f);
    Item.EvenRowBackgroundHoveredBrush = Item.OddRowBackgroundHoveredBrush = FFBuildPearl::Rounded(SoftTeal, 7.f);
    Item.ActiveBrush = Item.InactiveBrush = FFBuildPearl::Rounded(SoftTeal, 7.f);
    Item.ActiveHoveredBrush = Item.InactiveHoveredBrush = FFBuildPearl::Rounded(FFBuildPearl::Color(209, 232, 224), 7.f);
    Item.SelectorFocusedBrush = FFBuildPearl::Rounded(FLinearColor::Transparent, 7.f, Teal, 1.f);
    Item.TextColor = Item.SelectedTextColor = FFBuildPearl::Ink;
    Combo.SetItemStyle(Item);
    // These properties are construction-only in UMG; author them in the asset, never in runtime refresh.
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Combo.Font = FFBuildTypography::Make(FFBuildTypography::ERole::Body, 14);
    Combo.ForegroundColor = FFBuildPearl::Ink;
    Combo.ScrollBarStyle.NormalThumbImage = FFBuildPearl::Rounded(FFBuildPearl::Color(164, 200, 186), 3.f);
    Combo.ScrollBarStyle.HoveredThumbImage = Combo.ScrollBarStyle.DraggedThumbImage = FFBuildPearl::Rounded(Teal, 3.f);
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool BuildLayout(UWidgetBlueprint& BP, UWidgetTree& Tree)
{
    UCanvasPanel* Canvas = Get<UCanvasPanel>(BP, Tree, TEXT("Canvas_MultiSelection"));
    USizeBox* Width = Get<USizeBox>(BP, Tree, TEXT("Size_MultiSelection"));
    UBorder* Panel = Get<UBorder>(BP, Tree, TEXT("Border_MultiSelection"), true);
    UVerticalBox* Body = Get<UVerticalBox>(BP, Tree, TEXT("Column_MultiSelection"));
    UHorizontalBox* Header = Get<UHorizontalBox>(BP, Tree, TEXT("Row_MultiSelectionHeader"));
    USizeBox* CardSize = Get<USizeBox>(BP, Tree, TEXT("Size_MultiSelectionCards"));
    UScrollBox* Scroll = Get<UScrollBox>(BP, Tree, TEXT("Scroll_SelectedFacilities"));
    UHorizontalBox* Cards = Get<UHorizontalBox>(BP, Tree, TEXT("Row_SelectedFacilities"), true);
    UOverlay* CardArea = Get<UOverlay>(BP, Tree, TEXT("Overlay_SelectionCards"));
    if (!Canvas || !Width || !Panel || !Body || !Header || !CardSize || !Scroll || !Cards || !CardArea) return false;
    Tree.RootWidget = Canvas; Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Width->SetWidthOverride(1000.f); Width->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Width->RemoveFromParent(); Width->SetContent(Panel);
    UCanvasPanelSlot* Position = Canvas->AddChildToCanvas(Width);
    Position->SetAnchors(FAnchors(.5f, 1.f)); Position->SetAlignment(FVector2D(.5f, 1.f));
    Position->SetPosition(FVector2D(0.f, -108.f)); Position->SetAutoSize(true);
    Panel->SetBrush(FFBuildPearl::MaterialBrush(TEXT("Panel"))); Panel->SetBrushColor(FLinearColor::White);
    Panel->SetPadding(FMargin(20.f, 16.f)); Panel->SetContent(Body);
    InColumn(*Body, *Header, 12.f);
    InRow(*Header, *Label(BP, Tree, TEXT("Text_MultiSelectionTitle"), LOCTEXT("Title", "다중 선택"), 20), 12.f);
    InRow(*Header, *Label(BP, Tree, TEXT("Text_MultiSelectionCount"), LOCTEXT("Count", "선택 0개"), 14, Teal, true), 0.f);
    InRow(*Header, *Get<USpacer>(BP, Tree, TEXT("Spacer_MultiSelectionHeader")), 0.f, true);
    InRow(*Header, *Action(BP, Tree, TEXT("Button_InspectSelection"), LOCTEXT("Inspect", "상세 보기")));
    InRow(*Header, *Action(BP, Tree, TEXT("Button_ClearSelection"), LOCTEXT("Clear", "전체 해제")));
    InRow(*Header, *Action(BP, Tree, TEXT("Button_ExitMultiSelection"), LOCTEXT("Exit", "선택 종료")), 0.f);
    CardSize->SetHeightOverride(62.f); CardSize->SetContent(CardArea);
    Scroll->RemoveFromParent(); CardArea->AddChildToOverlay(Scroll)->SetHorizontalAlignment(HAlign_Fill);
    Scroll->SetOrientation(Orient_Horizontal); Scroll->SetScrollBarVisibility(ESlateVisibility::Collapsed); Scroll->SetAllowOverscroll(false);
    Cards->RemoveFromParent(); Scroll->AddChild(Cards);
    UTextBlock* Hint = Label(BP, Tree, TEXT("Text_MultiSelectionDesignerHint"), LOCTEXT("Empty", "시설을 클릭하거나 드래그해 함께 편집할 시설을 선택하세요."), 14, FFBuildPearl::Muted, true);
    Hint->RemoveFromParent(); UOverlaySlot* HintSlot = CardArea->AddChildToOverlay(Hint); HintSlot->SetVerticalAlignment(VAlign_Center);
    InColumn(*Body, *CardSize, 12.f);
    UHorizontalBox* Actions = Get<UHorizontalBox>(BP, Tree, TEXT("Row_GroupActions"));
    InColumn(*Body, *Actions, 14.f);
    InRow(*Actions, *Action(BP, Tree, TEXT("Button_MoveGroup"), LOCTEXT("Move", "묶음 이동"), 2));
    InRow(*Actions, *Action(BP, Tree, TEXT("Button_DuplicateGroup"), LOCTEXT("Duplicate", "복제")));
    InRow(*Actions, *Action(BP, Tree, TEXT("Button_RemoveGroup"), LOCTEXT("Remove", "묶음 철거"), 3));
    InRow(*Actions, *Get<USpacer>(BP, Tree, TEXT("Spacer_GroupActions")), 0.f, true);
    InRow(*Actions, *Action(BP, Tree, TEXT("Button_RotateLeft"), LOCTEXT("Left", "왼쪽 회전 Q")));
    InRow(*Actions, *Action(BP, Tree, TEXT("Button_RotateRight"), LOCTEXT("Right", "오른쪽 회전 E")));
    InRow(*Actions, *Action(BP, Tree, TEXT("Button_CancelGroup"), LOCTEXT("Cancel", "취소")));
    InRow(*Actions, *Action(BP, Tree, TEXT("Button_ApplyGroup"), LOCTEXT("Apply", "배치 확정"), 1), 0.f);
    UBorder* PresetWell = Get<UBorder>(BP, Tree, TEXT("Border_PresetWorkspace"));
    PresetWell->SetBrush(FFBuildPearl::Rounded(FFBuildPearl::Color(238, 247, 243), 14.f, FFBuildPearl::Color(214, 231, 222), 1.f));
    PresetWell->SetPadding(FMargin(14.f, 12.f));
    UVerticalBox* PresetBody = Get<UVerticalBox>(BP, Tree, TEXT("Column_PresetWorkspace"));
    PresetWell->SetContent(PresetBody); InColumn(*Body, *PresetWell, 10.f);
    UHorizontalBox* PresetHeading = Get<UHorizontalBox>(BP, Tree, TEXT("Row_PresetHeading"));
    InRow(*PresetHeading, *Label(BP, Tree, TEXT("Text_PresetHeading"), LOCTEXT("Presets", "배치 프리셋"), 14, Teal), 12.f);
    InRow(*PresetHeading, *Label(BP, Tree, TEXT("Text_PresetHint"), LOCTEXT("PresetHintCopy", "완성한 공간을 저장하고 다시 사용하세요"), 12, FFBuildPearl::Muted), 0.f);
    InColumn(*PresetBody, *PresetHeading, 8.f);
    UHorizontalBox* Presets = Get<UHorizontalBox>(BP, Tree, TEXT("Row_PresetControls"));
    InColumn(*PresetBody, *Presets);
    UEditableTextBox* Name = Get<UEditableTextBox>(BP, Tree, TEXT("Edit_PresetName"), true);
    UComboBoxString* Combo = Get<UComboBoxString>(BP, Tree, TEXT("Combo_Presets"), true);
    StylePresetFields(*Name, *Combo);
    InRow(*Presets, *Name, 8.f, true);
    InRow(*Presets, *Action(BP, Tree, TEXT("Button_SavePreset"), LOCTEXT("Save", "이름으로 저장")), 20.f);
    InRow(*Presets, *Combo, 8.f, true);
    InRow(*Presets, *Action(BP, Tree, TEXT("Button_LoadPreset"), LOCTEXT("Load", "불러와 배치"), 2));
    InRow(*Presets, *Action(BP, Tree, TEXT("Button_DeletePreset"), LOCTEXT("Delete", "프리셋 삭제")), 0.f);
    UTextBlock* Status = Label(BP, Tree, TEXT("Text_GroupEditingStatus"),
        LOCTEXT("Help", "묶음 이동 · 복제 후 Q/E로 회전하세요. 새로 복제하거나 불러올 때는 설치비가 적용됩니다."), 12, FFBuildPearl::Muted, true);
    Status->SetAutoWrapText(true); InColumn(*Body, *Status);
    return true;
}

bool ValidateContract(const UWidgetBlueprint& BP)
{
    if (!BP.GeneratedClass || !BP.GeneratedClass->IsChildOf(UFF_BuildMultiSelectionWidget::StaticClass()) || !BP.WidgetTree) return false;
    for (const TCHAR* Name : {TEXT("Button_MoveGroup"), TEXT("Button_DuplicateGroup"), TEXT("Button_RemoveGroup"), TEXT("Button_RotateLeft"),
        TEXT("Button_RotateRight"), TEXT("Button_ApplyGroup"), TEXT("Button_CancelGroup"), TEXT("Button_SavePreset"), TEXT("Button_LoadPreset"), TEXT("Button_DeletePreset")})
        if (!Cast<UButton>(BP.WidgetTree->FindWidget(Name))) return false;
    return Cast<UEditableTextBox>(BP.WidgetTree->FindWidget(TEXT("Edit_PresetName"))) &&
        Cast<UComboBoxString>(BP.WidgetTree->FindWidget(TEXT("Combo_Presets"))) &&
        Cast<UTextBlock>(BP.WidgetTree->FindWidget(TEXT("Text_GroupEditingStatus")));
}
void FinishDefaults(UWidgetBlueprint& BP, UWidgetBlueprint& Card)
{
    if (auto* Defaults = Cast<UFF_BuildMultiSelectionWidget>(BP.GeneratedClass->GetDefaultObject()))
    {
        Defaults->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        Defaults->SelectedFacilityCardClass = TSoftClassPtr<UUserWidget>(Card.GeneratedClass);
        BP.GetOutermost()->GetMetaData().SetValue(&BP, VersionKey, *Version);
        SaveAssetPackage(TEXT("FF_BUILD_MULTI_UMG"), BP);
    }
}
void CreateWidgetAsset()
{
    if (LoadExistingBlueprint(Package))
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_MULTI_UMG_REUSE: saved Designer layout preserved; explicit update: FF.UpdateBuildMultiSelectionWidget"));
        return;
    }
    UWidgetBlueprint* Card = EnsureWidgetBlueprint(TEXT("FF_BUILD_MULTI_CARD_UMG"), CardPackage, Folder, *UUserWidget::StaticClass(), BuildCard);
    UWidgetBlueprint* BP = EnsureWidgetBlueprint(TEXT("FF_BUILD_MULTI_UMG"), Package, Folder, *UFF_BuildMultiSelectionWidget::StaticClass(), BuildLayout);
    if (BP && Card && ValidateContract(*BP)) FinishDefaults(*BP, *Card);
    else UE_LOG(LogFFUIWbpBuilder, Warning, TEXT("FF_BUILD_MULTI_UMG: existing layout needs explicit FF.UpdateBuildMultiSelectionWidget; no existing layout was replaced."));
}
void UpdateWidgetAsset()
{
    UWidgetBlueprint* BP = LoadExistingBlueprint(Package);
    if (!BP) { CreateWidgetAsset(); return; }
    if (BP->GetOutermost()->GetMetaData().GetValue(BP, VersionKey) == Version)
    {
        UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_MULTI_UMG_REUSE: editable workspace already current; manual Designer changes preserved."));
        return;
    }
    UWidgetBlueprint* Card = EnsureWidgetBlueprint(TEXT("FF_BUILD_MULTI_CARD_UMG"), CardPackage, Folder, *UUserWidget::StaticClass(), BuildCard);
    if (!BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(UFF_BuildMultiSelectionWidget::StaticClass()) || !BP->WidgetTree) return;
    BP->Modify(); BP->WidgetTree->Modify();
    // Reuses all known named widget objects, preserving button event bindings and unrelated objects/graphs.
    if (!BuildLayout(*BP, *BP->WidgetTree) || !SaveNewBlueprint(TEXT("FF_BUILD_MULTI_UMG"), *BP) || !ValidateContract(*BP) || !Card) return;
    FinishDefaults(*BP, *Card);
    UE_LOG(LogFFUIWbpBuilder, Display, TEXT("FF_BUILD_MULTI_UMG_UPDATED: %s; card template: %s"), *Package, *CardPackage);
}
FAutoConsoleCommand CreateWidgetCommand(TEXT("FF.CreateBuildMultiSelectionWidget"),
    TEXT("Creates missing editable multi-selection/card WBPs. Preserves existing Designer layouts."), FConsoleCommandDelegate::CreateStatic(&CreateWidgetAsset));
FAutoConsoleCommand UpdateWidgetCommand(TEXT("FF.UpdateBuildMultiSelectionWidget"),
    TEXT("Updates named multi-selection controls to the editable pearl/teal workspace, preserving objects and event graphs."), FConsoleCommandDelegate::CreateStatic(&UpdateWidgetAsset));
}
#undef LOCTEXT_NAMESPACE
#endif
