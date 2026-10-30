// One-time A5 migration: keep all existing named controls and move only their slots.
#if WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildMultiSelectionWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "WidgetBlueprint.h"

namespace FFBuildMultiA5Designer
{
using namespace FFUIWbpBuilder;
static UHorizontalBoxSlot* Add(UHorizontalBox& Row, UWidget& Child, float Gap = 0, bool Fill = false)
{
    auto* ChildSlot = Row.AddChildToHorizontalBox(&Child);
    ChildSlot->SetPadding(FMargin(0, 0, Gap, 0));
    ChildSlot->SetVerticalAlignment(VAlign_Center);
    ChildSlot->SetSize(FSlateChildSize(Fill ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
    return ChildSlot;
}
static bool Apply(UWidgetBlueprint& BP)
{
    UWidgetTree* Tree = BP.WidgetTree;
    if (!Tree || !BP.ParentClass || !BP.ParentClass->IsChildOf(UFF_BuildMultiSelectionWidget::StaticClass())) return false;
    if (Tree->FindWidget(TEXT("Column_A5_SelectedFacilities"))) return true;
    auto* Bounds = Cast<USizeBox>(Tree->FindWidget(TEXT("Size_MultiSelection")));
    auto* BoundsSlot = Bounds ? Cast<UCanvasPanelSlot>(Bounds->Slot) : nullptr;
    auto* Header = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("Row_MultiSelectionHeader")));
    auto* Actions = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("Row_GroupActions")));
    auto* Presets = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("Row_PresetControls")));
    auto* CardsSize = Cast<USizeBox>(Tree->FindWidget(TEXT("Size_MultiSelectionCards")));
    auto* Scroll = Cast<UScrollBox>(Tree->FindWidget(TEXT("Scroll_SelectedFacilities")));
    auto* Cards = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("Row_SelectedFacilities")));
    if (!Bounds || !BoundsSlot || !Header || !Actions || !Presets || !CardsSize || !Scroll || !Cards ||
        Cards->GetParent() != Scroll || Cards->GetChildrenCount() || Header->GetChildrenCount() != 6 ||
        Actions->GetChildrenCount() != 8 || Presets->GetChildrenCount() != 5) return false;
    const auto Child = [Tree](UHorizontalBox* Parent, const TCHAR* Name) -> UWidget*
    {
        UWidget* Result = Tree->FindWidget(Name);
        return Result && Result->GetParent() == Parent ? Result : nullptr;
    };
    UWidget* Title = Child(Header,TEXT("Text_MultiSelectionTitle"));
    UWidget* Count = Child(Header,TEXT("Text_MultiSelectionCount"));
    UWidget* Spacer = Child(Header,TEXT("Spacer_MultiSelectionHeader"));
    UWidget* Inspect = Child(Header,TEXT("Button_InspectSelection"));
    UWidget* Clear = Child(Header,TEXT("Button_ClearSelection"));
    UWidget* Exit = Child(Header,TEXT("Button_ExitMultiSelection"));
    UWidget* Move = Child(Actions,TEXT("Button_MoveGroup"));
    UWidget* Duplicate = Child(Actions,TEXT("Button_DuplicateGroup"));
    UWidget* Remove = Child(Actions,TEXT("Button_RemoveGroup"));
    UWidget* GroupSpacer = Child(Actions,TEXT("Spacer_GroupActions"));
    UWidget* Left = Child(Actions,TEXT("Button_RotateLeft"));
    UWidget* Right = Child(Actions,TEXT("Button_RotateRight"));
    UWidget* Cancel = Child(Actions,TEXT("Button_CancelGroup"));
    UWidget* Confirm = Child(Actions,TEXT("Button_ApplyGroup"));
    UWidget* Name = Child(Presets,TEXT("Edit_PresetName"));
    UWidget* Save = Child(Presets,TEXT("Button_SavePreset"));
    UWidget* Combo = Child(Presets,TEXT("Combo_Presets"));
    UWidget* Load = Child(Presets,TEXT("Button_LoadPreset"));
    UWidget* Delete = Child(Presets,TEXT("Button_DeletePreset"));
    for (UWidget* Required : {Title, Count, Spacer, Inspect, Clear, Exit, Move, Duplicate, Remove,
        GroupSpacer, Left, Right, Cancel, Confirm, Name, Save, Combo, Load, Delete}) if (!Required) return false;
    BP.Modify(); Tree->Modify(); Header->Modify(); Actions->Modify(); Presets->Modify(); Cards->Modify();
    const auto Stack = [&BP,Tree](UHorizontalBox& Original, const TCHAR* Name)
    {
        for (UWidget* ChildWidget : Original.GetAllChildren()) { ChildWidget->Modify(); ChildWidget->RemoveFromParent(); }
        auto* Result = MakeWidget<UVerticalBox>(BP, *Tree, Name); Add(Original, *Result, 0, true); return Result;
    };
    const auto Line = [&BP,Tree](UVerticalBox& Parent, const TCHAR* Name)
    {
        auto* Result = MakeWidget<UHorizontalBox>(BP, *Tree, Name);
        Parent.AddChildToVerticalBox(Result)->SetPadding(FMargin(0,0,0,5)); return Result;
    };
    auto* HeaderStack=Stack(*Header,TEXT("Column_A5_MultiHeader"));
    auto* HeaderLine=Line(*HeaderStack,TEXT("Row_A5_MultiTitle"));
    Add(*HeaderLine,*Title,8); Add(*HeaderLine,*Count); Add(*HeaderLine,*Spacer,0,true); Add(*HeaderLine,*Exit);
    auto* HeaderButtons=Line(*HeaderStack,TEXT("Row_A5_MultiButtons")); Add(*HeaderButtons,*Inspect,5,true); Add(*HeaderButtons,*Clear,0,true);
    auto* GroupStack=Stack(*Actions,TEXT("Column_A5_GroupActions"));
    auto* GroupLine=Line(*GroupStack,TEXT("Row_A5_GroupActions")); Add(*GroupLine,*Move,5,true); Add(*GroupLine,*Duplicate,5,true); Add(*GroupLine,*Remove,0,true);
    GroupSpacer->SetVisibility(ESlateVisibility::Collapsed); Add(*GroupLine,*GroupSpacer);
    auto* Rotation=Line(*GroupStack,TEXT("Row_A5_GroupRotation")); Add(*Rotation,*Left,5,true); Add(*Rotation,*Right,0,true);
    auto* ConfirmRow=Line(*GroupStack,TEXT("Row_A5_GroupConfirm")); Add(*ConfirmRow,*Cancel,5,true); Add(*ConfirmRow,*Confirm,0,true);
    auto* PresetStack=Stack(*Presets,TEXT("Column_A5_PresetControls"));
    auto* SaveLine=Line(*PresetStack,TEXT("Row_A5_PresetSave")); Add(*SaveLine,*Name,5,true); Add(*SaveLine,*Save,0,true);
    auto* PickLine=Line(*PresetStack,TEXT("Row_A5_PresetPick")); Add(*PickLine,*Combo,0,true);
    auto* LoadLine=Line(*PresetStack,TEXT("Row_A5_PresetLoad")); Add(*LoadLine,*Load,5,true); Add(*LoadLine,*Delete,0,true);
    if (auto* Hint=Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_PresetHint"))))
    {
        Hint->Modify(); Hint->SetAutoWrapText(true);
        if (auto* HintSlot=Cast<UHorizontalBoxSlot>(Hint->Slot)) HintSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    }
    auto* SelectionList=MakeWidget<UVerticalBox>(BP,*Tree,TEXT("Column_A5_SelectedFacilities"));
    Add(*Cards,*SelectionList,0,true);
    Scroll->Modify(); Scroll->SetOrientation(Orient_Vertical); Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
    CardsSize->Modify(); CardsSize->SetHeightOverride(156.f);
    Bounds->Modify(); Bounds->SetWidthOverride(390.f); BoundsSlot->Modify();
    BoundsSlot->SetAnchors(FAnchors(1,1)); BoundsSlot->SetAlignment(FVector2D(1,1));
    BoundsSlot->SetPosition(FVector2D(-24,-40)); BoundsSlot->SetAutoSize(true);
    return SaveNewBlueprint(TEXT("FF_BUILD_MULTI_A5_DESIGNER"),BP);
}
static void Run()
{
    const FString Package=FString(FFBuildUIAssetPaths::ControlsFolder)/TEXT("WBP_FF_BuildMultiSelection");
    UWidgetBlueprint* Blueprint=LoadExistingBlueprint(Package);
    const bool Success=Blueprint && Apply(*Blueprint);
    UE_LOG(LogFFUIWbpBuilder,Display,TEXT("FF_BUILD_MULTI_A5_DESIGNER_%s"),Success?TEXT("PASS"):TEXT("FAIL"));
}
static FAutoConsoleCommand Command(TEXT("FF.ApplyBuildMultiA5DesignerLayout"),
    TEXT("Save the approved multi-selection layout to its WBP while preserving existing controls and bindings."),FConsoleCommandDelegate::CreateStatic(&Run));
}
#endif
