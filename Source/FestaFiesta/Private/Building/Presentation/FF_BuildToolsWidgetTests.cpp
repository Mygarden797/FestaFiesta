// FF_BuildToolsWidgetTests.cpp
// 역할: 건설 도구의 동작과 저장된 화면 연결을 자동 검증한다.

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/TextBlock.h"
#include "FF_FFZSTypes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsRequiredSpaceTest,
    "FestaFiesta.Building.BuildTools.RequiredSpace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolsRequiredSpaceTest::RunTest(const FString& Parameters)
{
    FFFZSFacilitySpec Spec;
    Spec.FootprintSizeCells = FIntPoint(320, 197);
    Spec.ClearanceCells = 75;
    Spec.QueueLayout = EFFZSQueueLayout::UShape;
    // Bent reservation: the inner gap is deliberately not part of the queue.
    Spec.QueueReservedRects = {FFFZSCellRect(FIntPoint(-100, -200), FIntPoint(0, 0)),
        FFFZSCellRect(FIntPoint(0, -200), FIntPoint(400, -100))};
    FText Footprint, Queue;
    FFBuildToolsModel::FormatRequiredSpace(Spec, Footprint, Queue);
    TestEqual(TEXT("Legacy clearance does not add a safety margin to the facility dimensions"),
        Footprint.ToString(), FString(TEXT("시설 크기  3.2 × 1.97 m")));
    TestTrue(TEXT("Queue bounds include negative offsets and the whole bend"),
        Queue.ToString().Contains(TEXT("5 × 2 m")) && Queue.ToString().Contains(TEXT("U자형")));
    Spec.QueueReservedRects.Reset();
    Spec.QueueReservedCells = {FIntPoint(-50, -25), FIntPoint(49, 24)};
    FFBuildToolsModel::FormatRequiredSpace(Spec, Footprint, Queue);
    TestTrue(TEXT("Legacy cell maxima are exclusive and include one centimeter"), Queue.ToString().Contains(TEXT("1 × 0.5 m")));
    Spec.QueueReservedCells.Reset();
    FFBuildToolsModel::FormatRequiredSpace(Spec, Footprint, Queue);
    TestTrue(TEXT("Missing geometry is never presented as zero required space"), Queue.ToString().Contains(TEXT("정보 없음")));
    Spec.Category = EFFZSFacilityCategory::Road;
    FFBuildToolsModel::FormatRequiredSpace(Spec, Footprint, Queue);
    TestEqual(TEXT("Passive facilities explicitly show no queue"), Queue.ToString(), FString(TEXT("대기열 공간 없음")));
    TestEqual(TEXT("Passive facilities also show only their physical footprint"),
        Footprint.ToString(), FString(TEXT("시설 크기  3.2 × 1.97 m")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsHelpClearanceTest,
    "FestaFiesta.Building.BuildTools.HelpCatalogClearance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsAudienceSpaceTest,
    "FestaFiesta.Building.BuildTools.AudienceSpace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolsAudienceSpaceTest::RunTest(const FString& Parameters)
{
    FFFZSFacilitySpec Spec;
    TestTrue(TEXT("Ordinary facilities do not reserve a misleading audience row"),
        FFBuildToolsModel::FormatAudienceSpace(Spec).IsEmpty());
    Spec.Audience.bEnabled = true;
    Spec.Audience.Columns = 3;
    Spec.Audience.Rows = 2;
    Spec.Audience.ColumnSpacingCm = 120.f;
    Spec.Audience.RowSpacingCm = 150.f;
    Spec.Audience.CenterAisleWidthCm = 200.f;
    Spec.Audience.PerimeterMarginCm = 180.f;
    Spec.Audience.FirstSlotLocalCm = FVector(-450, 700, 0);
    const FString Audience = FFBuildToolsModel::FormatAudienceSpace(Spec).ToString();
    TestTrue(TEXT("Audience bounds include the center aisle and perimeter, and show seat capacity"),
        Audience.Contains(TEXT("8 × 5.1 m")) && Audience.Contains(TEXT("정원 6명")));
    Spec.Audience.Rows = 0;
    TestTrue(TEXT("Malformed audience geometry is explicit instead of silently showing zero space"),
        FFBuildToolsModel::FormatAudienceSpace(Spec).ToString().Contains(TEXT("정보 없음")));
    const FString Problems = FFBuildToolsModel::FormatProblemStatus(FText::FromString(TEXT("관객석 경계 문제")), 0, 4).ToString();
    TestTrue(TEXT("Problem navigation cannot be mistaken for boundary coverage"),
        Problems.Contains(TEXT("배치 문제 4개 · 1번째")) && !Problems.Contains(TEXT("1/4")));
    TestEqual(TEXT("A single problem is not decorated with a counter"),
        FFBuildToolsModel::FormatProblemStatus(FText::FromString(TEXT("한 문제")), 0, 1).ToString(), FString(TEXT("한 문제")));
    return true;
}

bool FFFBuildToolsHelpClearanceTest::RunTest(const FString& Parameters)
{
    const FVector2D SharedColumn = FFBuildToolsModel::ResolveHelpVerticalBounds(560.f, 980.f, 525.f, 488.f);
    TestTrue(TEXT("A long queue guide stays below the measured combination card and within the HUD"),
        SharedColumn.X >= 488.f && SharedColumn.X + SharedColumn.Y <= 980.f && SharedColumn.Y < 560.f);
    // The catalog reserves its complete 326px tray plus the persistent navigation,
    // and resized/dragged trays can supply their measured top edge instead.
    for (const float ViewHeight : {540.f, 720.f, 900.f, 1080.f})
        for (const float ContentHeight : {240.f, 330.f, 470.f})
        {
            const float CatalogTop = ViewHeight - 326.f - 84.f;
            const FVector2D Bounds = FFBuildToolsModel::ResolveHelpVerticalBounds(ContentHeight, CatalogTop - 16.f);
            TestTrue(TEXT("Help and catalog retain a 16px gap even when content needs scrolling"),
                Bounds.X + Bounds.Y <= CatalogTop - 16.f + .1f);
            TestTrue(TEXT("Help never expands beyond its content or above the viewport"),
                Bounds.X >= 0.f && Bounds.Y >= 0.f && Bounds.Y <= ContentHeight);
            if (CatalogTop >= 120.f)
                TestTrue(TEXT("The top toolbar keeps its own clear space"), Bounds.X >= 104.f);
        }
    return true;
}

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
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
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
        TEXT("Button_BuildHelpUndo"), TEXT("Button_BuildHelpRedo"),
        TEXT("Button_FindNearby"), TEXT("Button_NextPlacementProblem"),
        TEXT("Button_Grid"), TEXT("Button_BuildSettings"), TEXT("Button_BuildSettingsClose"),
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
    for (const TCHAR* Name : {TEXT("Text_ToolbarGold"), TEXT("Image_BuildToolbarGold"),
        TEXT("Border_BuildToolbarGold"), TEXT("Text_ToolbarGoldCaption"),
        TEXT("Row_RetiredToolbarSnap"), TEXT("Row_BuildToolbarKeycaps"),
        TEXT("Button_SnapMeter"), TEXT("Button_SnapPrecision"), TEXT("Button_SnapFree")})
        TestNull(*FString::Printf(TEXT("Obsolete presentation is removed from Designer: %s"), Name), Tree->FindWidget(Name));
    TestNotNull(TEXT("Placement cost remains available after removing the redundant top gold badge"), Tree->FindWidget(TEXT("Text_Cost")));
    for (int32 Index = 1; Index <= 7; ++Index)
    {
        const FName CanvasName = Index == 1 ? FName(TEXT("Canvas_BuildToolbarFacilityPlus"))
            : FName(*FString::Printf(TEXT("Canvas_BuildToolbarLine%d"), Index));
        const UCanvasPanel* Icon = Cast<UCanvasPanel>(Tree->FindWidget(CanvasName));
        TestTrue(*FString::Printf(TEXT("Toolbar action %d uses editable simple vector strokes"), Index),
            Icon && Icon->GetChildrenCount() >= 2 && Icon->GetChildrenCount() <= 12);
        TestNull(TEXT("Complex raster toolbar artwork is retired"),
            Tree->FindWidget(FName(*FString::Printf(TEXT("Image_ToolbarF%d"), Index))));
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
    TestNotNull(TEXT("Short viewports can scroll the guide inside the available space"),
        Cast<UScrollBox>(Tree->FindWidget(TEXT("Scroll_BuildHelp"))));
    for (const TCHAR* Key : {TEXT("Q"), TEXT("E"), TEXT("W"), TEXT("A"), TEXT("S"),
        TEXT("D"), TEXT("Ctrl"), TEXT("G"), TEXT("B"), TEXT("Esc"),
        TEXT("Z"), TEXT("Y"), TEXT("N")})
    {
        const FString Name = FString(TEXT("Text_BuildHelpKey_")) + Key;
        const UTextBlock* KeyText = Cast<UTextBlock>(Tree->FindWidget(FName(*Name)));
        TestTrue(*FString::Printf(TEXT("The guide shows the actual %s keyboard key in a readable keycap"), Key),
            KeyText && KeyText->GetText().ToString() == Key && KeyText->GetFont().Size >= 14);
    }
    const TCHAR* QueueKeys[] = {TEXT("7"), TEXT("8"), TEXT("9"), TEXT("0")};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        // Existing Designer names are stable; their displayed shortcuts are migrated.
        const FName Name(*FString::Printf(TEXT("Text_BuildHelpKey_%d"), Index + 1));
        const UTextBlock* KeyText = Cast<UTextBlock>(Tree->FindWidget(Name));
        TestTrue(*FString::Printf(TEXT("Queue shortcut %s is shown in the preserved keycap"), QueueKeys[Index]),
            KeyText && KeyText->GetText().ToString() == QueueKeys[Index] && KeyText->GetFont().Size >= 14);
    }
    const UTextBlock* QueueHint = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_BuildHelpQueueHint")));
    const auto* NearbyHelp = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_BuildHelpNearby")));
    TestTrue(TEXT("The N keycap explains nearby movement in the placement guide"),
        NearbyHelp && NearbyHelp->GetText().ToString().Contains(TEXT("가까운 빈자리")));
    const auto* NearbyButton = Cast<UButton>(Tree->FindWidget(TEXT("Button_FindNearby")));
    const auto* NearbyLabel = NearbyButton ? Cast<UTextBlock>(NearbyButton->GetContent()) : nullptr;
    TestTrue(TEXT("Nearby button exposes the N shortcut before runtime refresh"),
        NearbyLabel && NearbyLabel->GetText().ToString().Contains(TEXT("[N]")));
    TestTrue(TEXT("Saved queue mapping matches the active number shortcuts"),
        QueueHint && QueueHint->GetText().ToString() == TEXT("7 기본 · 8 좌측 · 9 우측 · 0 U자"));
    for (const TCHAR* Name : {TEXT("Text_BuildHelpUndo"), TEXT("Text_BuildHelpRedo"), TEXT("Text_BuildHelpQueue"), TEXT("Text_BuildHelpQueueHint")})
        TestNotNull(*FString::Printf(TEXT("Editing guidance belongs to the existing help: %s"), Name), Tree->FindWidget(Name));
    const UWidget* QueueHelp = Tree->FindWidget(TEXT("Row_BuildHelpQueue"));
    TestTrue(TEXT("Designer placement preview exposes the editable queue guide"), QueueHelp &&
        QueueHelp->GetVisibility() == ESlateVisibility::SelfHitTestInvisible && !QueueHelp->bHiddenInDesigner);
    TestNull(TEXT("Obsolete letter-by-letter help is removed instead of archived"), Tree->FindWidget(TEXT("Text_ControlHelp")));
    for (const FName Name : {FName(TEXT("Size_BuildHelpMouseIcon")),
        FName(TEXT("Size_BuildHelpArrowsIcon")), FName(TEXT("Size_BuildHelpRotateIcon")),
        FName(TEXT("Size_BuildHelpGridIcon")), FName(TEXT("Size_BuildHelpCatalogIcon")),
        FName(TEXT("Size_BuildHelpPauseIcon"))})
        TestNotNull(*FString::Printf(TEXT("Icon guide contains %s"), *Name.ToString()),
            Cast<USizeBox>(Tree->FindWidget(Name)));
    for (const FName Name : { FName(TEXT("Text_Queue")), FName(TEXT("Text_Footprint")), FName(TEXT("Text_Audience")), FName(TEXT("Text_Balance")) })
        TestNotNull(*Name.ToString(), Cast<UTextBlock>(Tree->FindWidget(Name)));
    for (const TCHAR* Suffix : {TEXT("Audience"), TEXT("Boundary")})
        TestNotNull(TEXT("Saved WBP labels audience and the construction boundary separately"),
            Tree->FindWidget(FName(*(FString(TEXT("Text_PlacementSpaceLegend")) + Suffix))));
    const auto* AudienceLegend = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_PlacementSpaceLegendAudience")));
    const auto* FacilityLegend = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_PlacementSpaceLegendFacility")));
    TestTrue(TEXT("Audience and facility use visibly distinct authored legend colors"),
        AudienceLegend && FacilityLegend && AudienceLegend->GetColorAndOpacity() != FacilityLegend->GetColorAndOpacity());
    for (const FName Name : {TEXT("Text_PlacementAdvice"), TEXT("Text_PlacementObstruction"), TEXT("Text_AssistanceMessage")})
        TestNotNull(*Name.ToString(), Cast<UTextBlock>(Tree->FindWidget(Name)));
    for (const FName Name : {TEXT("Button_DirectAdjustment"), TEXT("Button_FindNearby"), TEXT("Button_NextPlacementProblem")})
        TestNotNull(*Name.ToString(), Cast<UButton>(Tree->FindWidget(Name)));
    for (const FName Name : {TEXT("Panel_PlacementFacts")})
    {
        UWidget* OptionalPanel = Tree->FindWidget(Name);
        TestTrue(*FString::Printf(TEXT("%s starts collapsed outside a placement session"), *Name.ToString()),
            OptionalPanel && OptionalPanel->GetVisibility() == ESlateVisibility::Collapsed);
    }
    const UWidget* DesignerHelp = Tree->FindWidget(TEXT("Border_PlacementHelp"));
    TestTrue(TEXT("Saved placement Designer shows its editable help surface"), DesignerHelp &&
        DesignerHelp->GetVisibility() == ESlateVisibility::SelfHitTestInvisible && !DesignerHelp->bHiddenInDesigner);
    for (const TCHAR* Name : {TEXT("Text_SettingsGrid"), TEXT("Text_SettingsFacilityRanges"),
        TEXT("Text_SettingsSynergyPreview"), TEXT("Text_SettingsDirectAdjustment"),
        TEXT("Text_SettingsZoomToCursor"), TEXT("Text_SettingsEdgeScroll")})
    {
        const auto* Caption = Cast<UTextBlock>(Tree->FindWidget(Name));
        const FString Value = Caption ? Caption->GetText().ToString() : FString();
        TestTrue(*FString::Printf(TEXT("Saved Designer toggle uses the compact runtime state: %s"), Name),
            Caption && (Value == TEXT("켜짐") || Value == TEXT("꺼짐")));
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
    for (const TCHAR* Name : {TEXT("Border_PlacementSpaceLegendClearance"), TEXT("Text_PlacementSpaceLegendClearance")})
        if (UWidget* Legacy = Widget->GetWidgetFromName(Name))
            TestEqual(TEXT("Legacy safety margin legend is not displayed"), Legacy->GetVisibility(), ESlateVisibility::Collapsed);
    if (auto* Legend = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_RangeLegend"))))
        TestFalse(TEXT("Range legend no longer advertises a safety margin"), Legend->GetText().ToString().Contains(TEXT("안전 여유")));
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
    TestFalse(TEXT("Runtime outside a placement session hides the authored preview help"), Open(TEXT("Border_PlacementHelp")));
    for (FName Name : {TEXT("Button_BuildBack"), TEXT("Button_BuildSettings"), TEXT("Button_BuildSettingsClose"),
        TEXT("Button_MultiSelect"), TEXT("Button_SnapCycle"), TEXT("Button_ToolbarRanges"),
        TEXT("Button_ToolbarCatalog"), TEXT("Button_ToolbarHelp"),
        TEXT("Button_BuildHelpUndo"), TEXT("Button_BuildHelpRedo"),
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
    for (FName Name : {TEXT("Button_BuildHelpUndo"), TEXT("Button_BuildHelpRedo")})
        if (const UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(Name)))
            TestFalse(TEXT("History actions remain disabled without an active editable session"), Button->GetIsEnabled());
    TestFalse(TEXT("Queue keys remain contextual without a queue-capable target"), Open(TEXT("Row_BuildHelpQueue")));
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
    if (auto* HelpBounds = Cast<USizeBox>(Widget->GetWidgetFromName(TEXT("Size_BuildWorldHelp"))))
    {
        // NativeTick keeps this height after the guide is closed. The empty SBox
        // must not remain a hit-test target above the world or catalog.
        HelpBounds->SetHeightOverride(240.f);
        const TSharedRef<SWidget> HelpSlate = HelpBounds->TakeWidget();
        TestFalse(TEXT("Closed help's retained layout bounds never intercept pointer input"),
            HelpSlate->GetVisibility().IsHitTestVisible());
        TestTrue(TEXT("Reopening the guide still permits its scroll surface to receive input"),
            HelpSlate->GetVisibility().AreChildrenHitTestVisible());
    }
    else AddError(TEXT("Help has independently authored layout bounds"));
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
