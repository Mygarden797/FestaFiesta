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
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
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
    if (!TestNotNull(TEXT("Run FF.ApplyBuildReferenceTools after the Clarity migration"), Blueprint) ||
        !TestNotNull(TEXT("Toolbar has a designer widget tree"), Blueprint ? Blueprint->WidgetTree.Get() : nullptr))
    {
        return false;
    }
    TestTrue(TEXT("The WBP uses the native controller contract"), Blueprint->GeneratedClass &&
        Blueprint->GeneratedClass->IsChildOf(UFF_BuildToolsWidget::StaticClass()));
    UWidgetTree* Tree = Blueprint->WidgetTree;
    TestNotNull(TEXT("The placement dock remains an independent pointer surface"),
        Cast<UBorder>(Tree->FindWidget(TEXT("Border_Tools"))));
    TestNotNull(TEXT("Catalog browsing and placement share an authored top toolbar"),
        Cast<UBorder>(Tree->FindWidget(TEXT("Border_BuildTopBar"))));
    TestNotNull(TEXT("Build mode has an authored upper-left back action"),
        Cast<UBorder>(Tree->FindWidget(TEXT("Border_BuildBack"))));
    UBorder* Settings = Cast<UBorder>(Tree->FindWidget(TEXT("Panel_BuildSettings")));
    if (TestNotNull(TEXT("Build settings have their own authored drawer"), Settings))
        TestTrue(TEXT("The settings drawer starts closed"), Settings->GetVisibility() == ESlateVisibility::Collapsed);
    const FName ButtonNames[] = { TEXT("Button_Confirm"), TEXT("Button_Catalog"),
        TEXT("Button_Help"), TEXT("Button_Facts"), TEXT("Button_DirectAdjustment"),
        TEXT("Button_FindNearby"), TEXT("Button_NextPlacementProblem"),
        TEXT("Button_Grid"), TEXT("Button_SnapMeter"), TEXT("Button_SnapPrecision"),
        TEXT("Button_SnapFree"), TEXT("Button_BuildSettings"), TEXT("Button_BuildSettingsClose"),
        TEXT("Button_MultiSelect"), TEXT("Button_SnapCycle"), TEXT("Button_ToolbarRanges"),
        TEXT("Button_ToolbarCatalog"), TEXT("Button_ToolbarHelp"),
        TEXT("Button_BuildBack"), TEXT("Button_SettingsGrid"),
        TEXT("Button_SettingsSnapMeter"), TEXT("Button_SettingsSnapPrecision"),
        TEXT("Button_SettingsSnapFree"), TEXT("Button_SettingsDirectAdjustment"),
        TEXT("Button_SettingsFacilityRanges"), TEXT("Button_SettingsSynergyPreview"),
        TEXT("Button_SettingsZoomToCursor"), TEXT("Button_SettingsEdgeScroll"),
        TEXT("Button_RotateLeft"), TEXT("Button_RotateRight"),
        TEXT("Button_FreeRotateLeft"), TEXT("Button_FreeRotateRight"),
        TEXT("Button_NudgeLeft"), TEXT("Button_NudgeUp"),
        TEXT("Button_NudgeDown"), TEXT("Button_NudgeRight") };
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
        TEXT("Button_Details") })
        TestNull(*FString::Printf(TEXT("Retired radial menu has no remaining Designer control: %s"), *Name.ToString()),
            Tree->FindWidget(Name));
    TestNotNull(TEXT("The settings drawer retains precise angle editing"),
        Cast<UEditableTextBox>(Tree->FindWidget(TEXT("Editable_Angle"))));
    TestNotNull(TEXT("The settings drawer explains rotation shortcuts"),
        Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_RotationHint"))));
    TestNotNull(TEXT("Mode has a state-driven text binding"), Tree->FindWidget(TEXT("Text_Mode")));
    TestNotNull(TEXT("Status has a state-driven text binding"), Tree->FindWidget(TEXT("Text_Legend")));
    const UTextBlock* CatalogAction = Cast<UTextBlock>(Tree->FindWidget(TEXT("Label_Catalog")));
    const UTextBlock* DirectAction = Cast<UTextBlock>(Tree->FindWidget(TEXT("Label_DirectAdjustment")));
    TestTrue(TEXT("Catalog action names the action without a keycap"),
        CatalogAction && CatalogAction->GetText().ToString() == TEXT("시설 목록"));
    TestTrue(TEXT("Direct placement action names the action without a keycap"),
        DirectAction && DirectAction->GetText().ToString() == TEXT("직접 조정"));
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
        TestTrue(TEXT("Placement actions dock to the lower right, clear of the placement center"),
            Slot->GetAnchors().Minimum.Equals(FVector2D(1, 1)) && Slot->GetAlignment().Equals(FVector2D(1, 1)));
        TestTrue(TEXT("Tool dock reserves space above the persistent bottom navigation"), Slot->GetPosition().Y <= -80.f);
    }
    const USizeBox* TopSize = Cast<USizeBox>(Tree->FindWidget(TEXT("Size_BuildTopBar")));
    const UCanvasPanelSlot* TopSlot = TopSize ? Cast<UCanvasPanelSlot>(TopSize->Slot) : nullptr;
    if (TestNotNull(TEXT("Top toolbar has an editable canvas placement"), TopSlot))
    {
        TestTrue(TEXT("Top toolbar is centered along the upper edge"),
            TopSlot->GetAnchors().Minimum.Equals(FVector2D(0.5f, 0.f)) &&
            TopSlot->GetAlignment().Equals(FVector2D(0.5f, 0.f)));
        TestTrue(TEXT("Top toolbar leaves a small margin from the edge"), TopSlot->GetPosition().Y >= 12.f);
    }
    TestTrue(TEXT("Seven pictogram buttons have a generous floating surface"),
        TopSize && TopSize->GetWidthOverride() >= 580.f);
    TestNotNull(TEXT("One real gold currency value lives in the floating toolbar"),
        Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_ToolbarGold"))));
    TestNotNull(TEXT("The toolbar uses the project gold badge"),
        Cast<UImage>(Tree->FindWidget(TEXT("Image_BuildToolbarGold"))));
    const USizeBox* GoldIcon = Cast<USizeBox>(Tree->FindWidget(TEXT("Size_BuildToolbarGoldIcon")));
    TestTrue(TEXT("Currency artwork is large yet bounded"), GoldIcon &&
        GoldIcon->GetWidthOverride() >= 34.f && GoldIcon->GetWidthOverride() <= 38.f &&
        GoldIcon->GetHeightOverride() >= 34.f && GoldIcon->GetHeightOverride() <= 38.f);
    const UBorder* GoldBadge = Cast<UBorder>(Tree->FindWidget(TEXT("Border_BuildToolbarGold")));
    const UCanvasPanelSlot* GoldSlot = GoldBadge ? Cast<UCanvasPanelSlot>(GoldBadge->Slot) : nullptr;
    TestTrue(TEXT("One independent currency badge is sized for icon, name and value"),
        GoldSlot && GoldSlot->GetSize().X >= 210.f && GoldSlot->GetSize().Y >= 54.f);
    TestNotNull(TEXT("Currency badge names the resource"),
        Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_ToolbarGoldCaption"))));
    const UHorizontalBox* Keycaps = Cast<UHorizontalBox>(Tree->FindWidget(TEXT("Row_BuildToolbarKeycaps")));
    TestTrue(TEXT("Function key captions are removed from the visible toolbar"),
        Keycaps && Keycaps->GetVisibility() == ESlateVisibility::Collapsed);
    for (int32 Index = 1; Index <= 7; ++Index)
    {
        TestNotNull(*FString::Printf(TEXT("Toolbar action %d uses a packaged texture"), Index),
            Cast<UImage>(Tree->FindWidget(FName(*FString::Printf(TEXT("Image_ToolbarF%d"), Index)))));
    }
    const USizeBox* SettingsSize = Cast<USizeBox>(Tree->FindWidget(TEXT("Size_BuildSettings")));
    const UCanvasPanelSlot* SettingsSlot = SettingsSize ? Cast<UCanvasPanelSlot>(SettingsSize->Slot) : nullptr;
    if (TestNotNull(TEXT("Settings drawer has an independent canvas placement"), SettingsSlot))
        TestTrue(TEXT("Settings drawer opens from the upper left"),
            SettingsSlot->GetAnchors().Minimum.Equals(FVector2D::ZeroVector) &&
            SettingsSlot->GetPosition().X >= 0.f && SettingsSlot->GetPosition().Y >= 0.f);
    TestNotNull(TEXT("The cost and queue summary has a distinct pointer-safe panel"),
        Cast<UBorder>(Tree->FindWidget(TEXT("Border_PlacementInfo"))));
    TestNotNull(TEXT("Dismissible help is authored as a world overlay"),
        Cast<UBorder>(Tree->FindWidget(TEXT("Border_PlacementHelp"))));
    const USizeBox* HelpSize = Cast<USizeBox>(Tree->FindWidget(TEXT("Size_BuildWorldHelp")));
    TestNotNull(TEXT("World shortcuts have their own canvas placement"),
        HelpSize ? Cast<UCanvasPanelSlot>(HelpSize->Slot) : nullptr);
    const UTextBlock* LegacyHelp = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_ControlHelp")));
    TestTrue(TEXT("Letter-by-letter help is retained only as a hidden compatibility binding"),
        LegacyHelp && LegacyHelp->GetVisibility() == ESlateVisibility::Collapsed);
    for (const FName Name : {FName(TEXT("Size_BuildHelpMouseIcon")),
        FName(TEXT("Size_BuildHelpArrowsIcon")), FName(TEXT("Size_BuildHelpRotateIcon")),
        FName(TEXT("Size_BuildHelpGridIcon")), FName(TEXT("Size_BuildHelpCatalogIcon")),
        FName(TEXT("Size_BuildHelpPauseIcon"))})
        TestNotNull(*FString::Printf(TEXT("Icon guide contains %s"), *Name.ToString()),
            Cast<USizeBox>(Tree->FindWidget(Name)));
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
    };
    UBorder* Settings = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Panel_BuildSettings")));
    if (!TestNotNull(TEXT("Settings drawer instance"), Settings)) return false;
    MenuRemainsRemoved();
    TestFalse(TEXT("A closed settings drawer does not capture auxiliary input"), Widget->IsAuxiliaryPanelOpen());
    for (FName Name : {TEXT("Button_BuildBack"), TEXT("Button_BuildSettings"), TEXT("Button_BuildSettingsClose"),
        TEXT("Button_MultiSelect"), TEXT("Button_SnapCycle"), TEXT("Button_ToolbarRanges"),
        TEXT("Button_ToolbarCatalog"), TEXT("Button_ToolbarHelp"),
        TEXT("Button_SettingsGrid"), TEXT("Button_SettingsSnapMeter"),
        TEXT("Button_SettingsSnapPrecision"), TEXT("Button_SettingsSnapFree"),
        TEXT("Button_SettingsDirectAdjustment"), TEXT("Button_SettingsFacilityRanges"),
        TEXT("Button_SettingsSynergyPreview"), TEXT("Button_SettingsZoomToCursor"),
        TEXT("Button_SettingsEdgeScroll")})
    {
        UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(Name));
        if (TestNotNull(*Name.ToString(), Button))
            TestTrue(*FString::Printf(TEXT("The new settings action is connected: %s"), *Name.ToString()),
                Button->OnClicked.IsBound());
    }
    // This playerless fixture has no active build session. Exercise the independent
    // drawer surface without starting placement or bypassing the runtime session guard.
    Settings->SetVisibility(ESlateVisibility::Visible);
    TestTrue(TEXT("An open settings drawer reserves auxiliary input"), Widget->IsAuxiliaryPanelOpen());
    Widget->ToggleRadialMenu();
    MenuRemainsRemoved();
    TestTrue(TEXT("Legacy radial calls leave the settings drawer open"), Settings->IsVisible());
    Settings->SetVisibility(ESlateVisibility::Collapsed);
    TestFalse(TEXT("Closing settings releases auxiliary input"), Widget->IsAuxiliaryPanelOpen());
    Widget->ToggleRadialMenu();
    Widget->ToggleRadialMenu();
    MenuRemainsRemoved();
    const FKeyEvent RemovedToolKey(EKeys::R, FModifierKeysState(), 0, false, 0, 0);
    TestFalse(TEXT("R is no longer consumed by a removed menu entry"),
        Slate->OnPreviewKeyDown(FGeometry(), RemovedToolKey).IsEventHandled());
    MenuRemainsRemoved();
    TestFalse(TEXT("Function keys do not act without an active build session"), Widget->HandleToolbarKey(EKeys::F5));
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
