// FF_BuildToolsWidgetTests.cpp
// 역할: 건설 도구의 동작과 저장된 화면 연결을 자동 검증한다.

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
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
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"
#include "Widgets/SWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsDesignerContractTest,
    "FestaFiesta.Building.BuildTools.DesignerContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolsDesignerContractTest::RunTest(const FString& Parameters)
{
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr,
        FFBuildUIAssetPaths::BuildToolsObject);
    if (!TestNotNull(TEXT("Run FF.ApplyBuildModeClarityTools to migrate the editable toolbar"), Blueprint) ||
        !TestNotNull(TEXT("Toolbar has a designer widget tree"), Blueprint ? Blueprint->WidgetTree.Get() : nullptr))
    {
        return false;
    }
    TestTrue(TEXT("The WBP uses the native controller contract"), Blueprint->GeneratedClass &&
        Blueprint->GeneratedClass->IsChildOf(UFF_BuildToolsWidget::StaticClass()));
    UWidgetTree* Tree = Blueprint->WidgetTree;
    TestNotNull(TEXT("Only the visible toolbar panel receives pointer hits"),
        Cast<UBorder>(Tree->FindWidget(TEXT("Border_Tools"))));
    const FName ButtonNames[] = { TEXT("Button_Confirm"), TEXT("Button_Catalog"),
        TEXT("Button_Help"), TEXT("Button_Facts"), TEXT("Button_DirectAdjustment"),
        TEXT("Button_FindNearby"), TEXT("Button_NextPlacementProblem") };
    for (const FName Name : ButtonNames)
    {
        const UFF_BuildToolsActionButton* Button = Cast<UFF_BuildToolsActionButton>(Tree->FindWidget(Name));
        TestNotNull(*FString::Printf(TEXT("Native action binding exists: %s"), *Name.ToString()), Button);
        if (Button)
        {
            TestFalse(TEXT("Each button yields keyboard controls back to the game"), Button->GetIsFocusable());
        }
    }
    for (const FName Name : { TEXT("Button_Radial"), TEXT("Panel_Radial"), TEXT("Panel_Details"),
        TEXT("Button_Details"), TEXT("Button_Grid"), TEXT("Button_SnapMeter"), TEXT("Button_SnapPrecision"),
        TEXT("Button_SnapFree"), TEXT("Button_RotateLeft"), TEXT("Button_RotateRight"),
        TEXT("Button_NudgeLeft"), TEXT("Button_NudgeUp"), TEXT("Button_NudgeDown"), TEXT("Button_NudgeRight"),
        TEXT("Button_FreeRotateLeft"), TEXT("Button_FreeRotateRight"), TEXT("Editable_Angle"), TEXT("Text_RotationHint") })
        TestNull(*FString::Printf(TEXT("Removed tool menu has no remaining Designer control: %s"), *Name.ToString()),
            Tree->FindWidget(Name));
    TestNotNull(TEXT("Mode has a state-driven text binding"), Tree->FindWidget(TEXT("Text_Mode")));
    TestNotNull(TEXT("Status has a state-driven text binding"), Tree->FindWidget(TEXT("Text_Legend")));
    for (const FName Name : { FName(TEXT("Text_FacilityName")), FName(TEXT("Text_Cost")),
        FName(TEXT("Text_ActiveSnap")) })
    {
        TestNotNull(*FString::Printf(TEXT("Designer presentation field: %s"), *Name.ToString()),
            Cast<UTextBlock>(Tree->FindWidget(Name)));
    }
    const UFF_BuildToolsActionButton* Confirm = Cast<UFF_BuildToolsActionButton>(Tree->FindWidget(TEXT("Button_Confirm")));
    TestTrue(TEXT("The primary action brush remains authored in the Designer asset"), Confirm && Confirm->bUseSelectionStyle);
    USizeBox* Size = Cast<USizeBox>(Tree->FindWidget(TEXT("Size_Tools")));
    const UCanvasPanelSlot* Slot = Size ? Cast<UCanvasPanelSlot>(Size->Slot) : nullptr;
    if (TestNotNull(TEXT("Toolbar has an editable canvas placement"), Slot))
    {
        TestTrue(TEXT("Compact tools dock to the lower right, clear of the placement center"),
            Slot->GetAnchors().Minimum.Equals(FVector2D(1, 1)) && Slot->GetAlignment().Equals(FVector2D(1, 1)));
        TestTrue(TEXT("Tool dock reserves space above the persistent bottom navigation"), Slot->GetPosition().Y <= -80.f);
    }
    TestNotNull(TEXT("The cost and queue summary has a distinct pointer-safe panel"),
        Cast<UBorder>(Tree->FindWidget(TEXT("Border_PlacementInfo"))));
    TestNotNull(TEXT("Dismissible help is authored beneath the left facility summary"),
        Cast<UBorder>(Tree->FindWidget(TEXT("Border_PlacementHelp"))));
    for (const FName Name : { FName(TEXT("Text_Queue")), FName(TEXT("Text_Footprint")), FName(TEXT("Text_Balance")) })
        TestNotNull(*Name.ToString(), Cast<UTextBlock>(Tree->FindWidget(Name)));
    for (const FName Name : {TEXT("Text_PlacementAdvice"), TEXT("Text_PlacementObstruction"), TEXT("Text_AssistanceMessage")})
        TestNotNull(*Name.ToString(), Cast<UTextBlock>(Tree->FindWidget(Name)));
    for (const FName Name : {TEXT("Button_DirectAdjustment"), TEXT("Button_FindNearby"), TEXT("Button_NextPlacementProblem")})
        TestNotNull(*Name.ToString(), Cast<UButton>(Tree->FindWidget(Name)));
    for (const FName Name : {TEXT("Panel_PlacementFacts"), TEXT("Border_PlacementHelp")})
    {
        UWidget* OptionalPanel = Tree->FindWidget(Name);
        TestTrue(*FString::Printf(TEXT("%s starts collapsed outside a placement session"), *Name.ToString()),
            OptionalPanel && OptionalPanel->GetVisibility() == ESlateVisibility::Collapsed);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsAdaptivePanelsTest,
    "FestaFiesta.Building.BuildTools.AdaptivePanels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolsAdaptivePanelsTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildToolsWidget>(nullptr,
        FFBuildUIAssetPaths::BuildToolsClass));
    if (!TestNotNull(TEXT("Playerless editor world"), World) || !TestNotNull(TEXT("Production compact tools"), Class)) return false;
    const bool bPrevious = Class->bCanCallInitializedWithoutPlayerContext;
    Class->bCanCallInitializedWithoutPlayerContext = true;
    TStrongObjectPtr<UFF_BuildToolsWidget> Widget(CreateWidget<UFF_BuildToolsWidget>(World, Class));
    Class->bCanCallInitializedWithoutPlayerContext = bPrevious;
    if (!TestNotNull(TEXT("Adaptive tools instance"), Widget.Get())) return false;
    const TSharedRef<SWidget> Slate = Widget->TakeWidget();
    auto* Data = FindFProperty<FStructProperty>(Class, TEXT("ViewData"));
    if (!TestNotNull(TEXT("Transient presentation fixture seam"), Data)) return false;
    // Seed presentation only: no placement subsystem, player save or purchase is touched.
    Data->ContainerPtrToValuePtr<FFF_BuildToolsViewData>(Widget.Get())->bPreviewActive = true;
    auto Click = [&](FName Name)
    {
        UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(Name));
        if (TestNotNull(*Name.ToString(), Button)) Button->OnClicked.Broadcast();
    };
    auto Open = [&](FName Name)
    {
        UWidget* Panel = Widget->GetWidgetFromName(Name);
        return Panel && Panel->GetVisibility() != ESlateVisibility::Collapsed;
    };
    auto MenuRemainsRemoved = [&]()
    {
        for (FName Name : {TEXT("Button_Radial"), TEXT("Panel_Radial"), TEXT("Panel_Details")})
            TestNull(*FString::Printf(TEXT("Removed tool entry cannot be recreated at runtime: %s"), *Name.ToString()),
                Widget->GetWidgetFromName(Name));
        TestFalse(TEXT("The compatibility query never reports an open tool menu"), Widget->IsRadialMenuOpen());
        TestFalse(TEXT("Removed detail panels cannot capture auxiliary-panel input"), Widget->IsAuxiliaryPanelOpen());
    };
    MenuRemainsRemoved();
    Widget->ToggleRadialMenu();
    Widget->ToggleRadialMenu();
    MenuRemainsRemoved();
    const FKeyEvent RemovedToolKey(EKeys::R, FModifierKeysState(), 0, false, 0, 0);
    TestFalse(TEXT("R is no longer consumed by a removed menu entry"),
        Slate->OnPreviewKeyDown(FGeometry(), RemovedToolKey).IsEventHandled());
    MenuRemainsRemoved();
    Click(TEXT("Button_Help"));
    TestTrue(TEXT("Help opens independently beneath the left summary"), Open(TEXT("Border_PlacementHelp")));
    auto* HelpLabel = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Label_Help")));
    TestTrue(TEXT("Visible help has a clear close action"), HelpLabel && HelpLabel->GetText().ToString() == TEXT("안내 닫기"));
    Click(TEXT("Button_Help"));
    TestFalse(TEXT("The help close action dismisses only the instructions"), Open(TEXT("Border_PlacementHelp")));
    TestTrue(TEXT("Closed instructions can be reopened by the same action"), HelpLabel && HelpLabel->GetText().ToString() == TEXT("조작 안내"));
    Click(TEXT("Button_Help"));
    Widget->ToggleRadialMenu();
    MenuRemainsRemoved();
    TestTrue(TEXT("A legacy menu call leaves the left-hand instructions readable"), Open(TEXT("Border_PlacementHelp")));
    Click(TEXT("Button_Facts"));
    TestTrue(TEXT("Facility dimensions and help remain independently available"),
        Open(TEXT("Panel_PlacementFacts")) && Open(TEXT("Border_PlacementHelp")));
    for (FName Name : {TEXT("Button_DirectAdjustment"), TEXT("Button_FindNearby"), TEXT("Button_NextPlacementProblem")})
    {
        UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(Name));
        if (TestNotNull(*Name.ToString(), Button))
        {
            TestTrue(*FString::Printf(TEXT("Placement assistance retains its native click binding: %s"), *Name.ToString()),
                Button->OnClicked.IsBound());
            Button->OnClicked.Broadcast();
        }
    }
    Widget->ToggleRadialMenu();
    MenuRemainsRemoved();
    TestTrue(TEXT("Compatibility and assistance actions preserve help and facility facts"),
        Open(TEXT("Panel_PlacementFacts")) && Open(TEXT("Border_PlacementHelp")));
    Click(TEXT("Button_Facts"));
    TestFalse(TEXT("Facility dimensions collapse again on the same button"), Open(TEXT("Panel_PlacementFacts")));
    Widget->RefreshTools();
    TestTrue(TEXT("Leaving placement hides the dock"), Widget->GetVisibility() == ESlateVisibility::Collapsed);
    TestFalse(TEXT("Leaving placement collapses its contextual help"), Open(TEXT("Border_PlacementHelp")));
    Widget->ToggleRadialMenu();
    MenuRemainsRemoved();
    return true;
}

#endif
#endif
