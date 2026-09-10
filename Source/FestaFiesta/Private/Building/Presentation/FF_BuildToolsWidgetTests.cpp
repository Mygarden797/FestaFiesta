#if WITH_DEV_AUTOMATION_TESTS

#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Components/TextBlock.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsAngleInputTest,
    "FestaFiesta.Building.BuildTools.AngleInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolsAngleInputTest::RunTest(const FString& Parameters)
{
    float Yaw = 42.0f;
    TestTrue(TEXT("Integer degrees are accepted"), FFBuildToolsModel::TryParseYawText(TEXT("90"), Yaw));
    TestEqual(TEXT("Integer degrees are preserved"), Yaw, 90.0f);
    TestTrue(TEXT("Trimmed fractional degrees are accepted"), FFBuildToolsModel::TryParseYawText(TEXT(" 12.5 "), Yaw));
    TestEqual(TEXT("Fractional degrees are preserved"), Yaw, 12.5f);
    TestTrue(TEXT("360 degrees is a valid displayed endpoint"), FFBuildToolsModel::TryParseYawText(TEXT("360"), Yaw));
    TestEqual(TEXT("360 maps to the authority's zero-degree orientation"), Yaw, 0.0f);
    for (const FString& Invalid : { FString(), FString(TEXT(".")), FString(TEXT("-1")),
        FString(TEXT("361")), FString(TEXT("nan")), FString(TEXT("inf")), FString(TEXT("1e3")),
        FString(TEXT("12.5.5")), FString(TEXT("90 degrees")), FString(TEXT("12,5")),
        FString(TEXT("999999999999999999999999999")) })
    {
        Yaw = 42.0f;
        TestFalse(*FString::Printf(TEXT("Malformed/out-of-range input is rejected: %s"), *Invalid),
            FFBuildToolsModel::TryParseYawText(Invalid, Yaw));
        TestEqual(TEXT("Rejected text cannot overwrite a valid rotation"), Yaw, 42.0f);
    }
    const UFF_BuildToolsActionButton* Button = GetDefault<UFF_BuildToolsActionButton>();
    TestFalse(TEXT("Toolbar action buttons cannot retain keyboard focus"), Button->GetIsFocusable());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsDesignerStyleTest,
    "FestaFiesta.Building.BuildTools.DesignerStyleOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolsDesignerStyleTest::RunTest(const FString& Parameters)
{
    UFF_BuildToolsActionButton* Button = NewObject<UFF_BuildToolsActionButton>();
    UTextBlock* Label = NewObject<UTextBlock>();
    FButtonStyle DesignerStyle = Button->GetStyle();
    DesignerStyle.NormalPadding = FMargin(19.0f, 7.0f);
    DesignerStyle.Normal.TintColor = FSlateColor(FLinearColor::Blue);
    Button->SetStyle(DesignerStyle);
    Label->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
    Button->SelectionStyle = DesignerStyle;
    Button->SelectionStyle.Normal.TintColor = FSlateColor(FLinearColor::Green);
    Button->SelectionTextColor = FSlateColor(FLinearColor::White);
    Button->bUseSelectionStyle = true;
    Button->ApplySelection(true, Label);
    TestTrue(TEXT("Selected variant uses the WBP-authored brush"),
        Button->GetStyle().Normal.TintColor == Button->SelectionStyle.Normal.TintColor);
    Button->ApplySelection(false, Label);
    TestTrue(TEXT("Deselection restores the original Designer brush"),
        Button->GetStyle().Normal.TintColor == DesignerStyle.Normal.TintColor);
    TestTrue(TEXT("Refreshing selection preserves Designer padding"), Button->GetStyle().NormalPadding == DesignerStyle.NormalPadding);
    TestTrue(TEXT("Deselection restores the original Designer text color"),
        Label->GetColorAndOpacity() == FSlateColor(FLinearColor::Red));
    return true;
}

#if WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "WidgetBlueprint.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsDesignerContractTest,
    "FestaFiesta.Building.BuildTools.DesignerContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolsDesignerContractTest::RunTest(const FString& Parameters)
{
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr,
        TEXT("/Game/FestaFiesta/UI/Building/WBP_FF_BuildTools.WBP_FF_BuildTools"));
    if (!TestNotNull(TEXT("Run FF.BuildToolsUMG to create the editable toolbar"), Blueprint) ||
        !TestNotNull(TEXT("Toolbar has a designer widget tree"), Blueprint ? Blueprint->WidgetTree.Get() : nullptr))
    {
        return false;
    }
    TestTrue(TEXT("The WBP uses the native controller contract"), Blueprint->GeneratedClass &&
        Blueprint->GeneratedClass->IsChildOf(UFF_BuildToolsWidget::StaticClass()));
    UWidgetTree* Tree = Blueprint->WidgetTree;
    TestNotNull(TEXT("Only the visible toolbar panel receives pointer hits"),
        Cast<UBorder>(Tree->FindWidget(TEXT("Border_Tools"))));
    const FName ButtonNames[] = { TEXT("Button_Confirm"), TEXT("Button_Grid"), TEXT("Button_SnapMeter"),
        TEXT("Button_SnapPrecision"), TEXT("Button_SnapFree"), TEXT("Button_RotateLeft"),
        TEXT("Button_RotateRight"), TEXT("Button_NudgeLeft"), TEXT("Button_NudgeUp"),
        TEXT("Button_NudgeDown"), TEXT("Button_NudgeRight"), TEXT("Button_Catalog"),
        TEXT("Button_FreeRotateLeft"), TEXT("Button_FreeRotateRight") };
    for (const FName Name : ButtonNames)
    {
        const UFF_BuildToolsActionButton* Button = Cast<UFF_BuildToolsActionButton>(Tree->FindWidget(Name));
        TestNotNull(*FString::Printf(TEXT("Native action binding exists: %s"), *Name.ToString()), Button);
        if (Button)
        {
            TestFalse(TEXT("Each button yields keyboard controls back to the game"), Button->GetIsFocusable());
        }
    }
    TestNotNull(TEXT("Angle can be edited through UMG"),
        Cast<UEditableTextBox>(Tree->FindWidget(TEXT("Editable_Angle"))));
    TestNotNull(TEXT("Mode has a state-driven text binding"), Tree->FindWidget(TEXT("Text_Mode")));
    TestNotNull(TEXT("Status has a state-driven text binding"), Tree->FindWidget(TEXT("Text_Legend")));
    for (const FName Name : { FName(TEXT("Text_FacilityName")), FName(TEXT("Text_Cost")),
        FName(TEXT("Text_ActiveSnap")), FName(TEXT("Text_RotationHint")) })
    {
        TestNotNull(*FString::Printf(TEXT("Designer presentation field: %s"), *Name.ToString()),
            Cast<UTextBlock>(Tree->FindWidget(Name)));
    }
    const UFF_BuildToolsActionButton* Snap = Cast<UFF_BuildToolsActionButton>(Tree->FindWidget(TEXT("Button_SnapMeter")));
    TestTrue(TEXT("Selected brushes are authored in the Designer asset"), Snap && Snap->bUseSelectionStyle);
    USizeBox* Size = Cast<USizeBox>(Tree->FindWidget(TEXT("Size_Tools")));
    const UCanvasPanelSlot* Slot = Size ? Cast<UCanvasPanelSlot>(Size->Slot) : nullptr;
    if (TestNotNull(TEXT("Toolbar has an editable canvas placement"), Slot))
    {
        TestTrue(TEXT("Toolbar anchors to bottom center"), Slot->GetAnchors().Minimum.Equals(FVector2D(0.5, 1.0)));
        TestTrue(TEXT("Toolbar reserves space above the existing shared 58px bottom strip"), Slot->GetPosition().Y <= -70.0f);
    }
    return true;
}

#endif
#endif
