// FF_BuildToolsWidgetTests.cpp
// 역할: 건설 도구의 동작과 저장된 화면 연결을 자동 검증한다.

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildModeGuide.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/TextBlock.h"
#include "FF_FFZSTypes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsDragPlacementGapInputTest,
    "FestaFiesta.Building.BuildTools.DragPlacementGapInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolsDragPlacementGapInputTest::RunTest(const FString& Parameters)
{
    int32 GapCm = 123;
    for (const FString Invalid : {TEXT(""), TEXT("-1"), TEXT("1001"), TEXT("999999999999"),
        TEXT("1.5"), TEXT("NaN"), TEXT("1e2"), TEXT("50cm"), TEXT("1,000")})
    {
        TestFalse(Invalid + TEXT(" is not a whole centimeter spacing in range"),
            FFBuildToolsModel::TryParseDragPlacementGapText(Invalid, GapCm));
        TestEqual(TEXT("Invalid edits preserve the last valid spacing"), GapCm, 123);
    }
    TestTrue(TEXT("Zero explicitly permits touching facilities"),
        FFBuildToolsModel::TryParseDragPlacementGapText(TEXT("0"), GapCm));
    TestEqual(TEXT("Zero is preserved"), GapCm, 0);
    TestTrue(TEXT("Whitespace is accepted around a valid number"),
        FFBuildToolsModel::TryParseDragPlacementGapText(TEXT(" 250 "), GapCm));
    TestEqual(TEXT("Spacing is expressed in centimeters"), GapCm, 250);
    TestTrue(TEXT("Ten meters is accepted"), FFBuildToolsModel::TryParseDragPlacementGapText(TEXT("1000"), GapCm));
    TestEqual(TEXT("Upper bound is preserved"), GapCm, 1000);
    const FString Help = UFF_BuildModeGuide::MakePlacementHelpText(false, true, 250).ToString();
    TestTrue(TEXT("Drag help reports the actual user spacing and stroke undo"),
        Help.Contains(TEXT("250cm")) && Help.Contains(TEXT("Ctrl+Z")) && Help.Contains(TEXT("함께 취소")));
    TestFalse(TEXT("Moving help does not promise drag installation"),
        UFF_BuildModeGuide::MakePlacementHelpText(true, true, 250).ToString().Contains(TEXT("여유 250cm")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsRequiredSpaceTest,
    "FestaFiesta.Building.BuildTools.RequiredSpace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsQueueSettingsInputTest,
    "FestaFiesta.Building.BuildTools.QueueSettingsInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolsQueueSettingsInputTest::RunTest(const FString& Parameters)
{
    int32 Teams = 17;
    float Spacing = 200.f, Width = 600.f;
    for (const FString Invalid : {TEXT(""), TEXT("0"), TEXT("513"), TEXT("1.5"), TEXT("-1"), TEXT("1e2"), TEXT("12팀")})
    {
        TestFalse(Invalid + TEXT(" cannot change the team limit"),
            FFBuildToolsModel::TryParseQueueSettingsText(Invalid, TEXT("115"), TEXT("600"), Teams, Spacing, Width));
        TestEqual(TEXT("Invalid team text preserves capacity"), Teams, 17);
        TestEqual(TEXT("Invalid team text preserves spacing"), Spacing, 200.f);
        TestEqual(TEXT("Invalid team text preserves width"), Width, 600.f);
    }
    for (const FString Invalid : {TEXT(""), TEXT("70"), TEXT("89.9"), TEXT("1000.1"), TEXT("NaN"), TEXT("inf"), TEXT("1e2"), TEXT("115cm"), TEXT("1,000")})
        TestFalse(Invalid + TEXT(" cannot change slot spacing"),
            FFBuildToolsModel::TryParseQueueSettingsText(TEXT("12"), Invalid, TEXT("600"), Teams, Spacing, Width));
    for (const FString Invalid : {TEXT(""), TEXT("99.9"), TEXT("1000.1"), TEXT("NaN"), TEXT("-100"), TEXT("600..5")})
        TestFalse(Invalid + TEXT(" cannot change lane width"),
            FFBuildToolsModel::TryParseQueueSettingsText(TEXT("12"), TEXT("115"), Invalid, Teams, Spacing, Width));
    TestEqual(TEXT("A late field failure leaves the entire edit unchanged"), Teams, 17);
    TestEqual(TEXT("A late field failure never partially updates slot spacing"), Spacing, 200.f);
    TestTrue(TEXT("Decimal centimeters and whitespace are supported"),
        FFBuildToolsModel::TryParseQueueSettingsText(TEXT(" 24 "), TEXT(" 125.5 "), TEXT(" 650.25 "), Teams, Spacing, Width));
    TestEqual(TEXT("Capacity remains an integer team count"), Teams, 24);
    TestEqual(TEXT("Decimal slot spacing is retained"), Spacing, 125.5f);
    TestEqual(TEXT("Decimal lane width is retained"), Width, 650.25f);
    TestTrue(TEXT("The full explicit range is supported"),
        FFBuildToolsModel::TryParseQueueSettingsText(TEXT("512"), TEXT("90"), TEXT("1000"), Teams, Spacing, Width));
    TestTrue(TEXT("Build help points players to the editable queue settings"),
        UFF_BuildModeGuide::MakeBuildOverviewHelpText().ToString().Contains(TEXT("대기열 설정")));
    return true;
}

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsContextPingClearanceTest,
    "FestaFiesta.Building.BuildTools.ContextPingPreviewClearance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolsContextPingClearanceTest::RunTest(const FString& Parameters)
{
    // The old marker offset lands directly on the queue to the facility's right.
    const FVector2D Marker(600, 350);
    const FVector2D PingSize(280, 40);
    const FBox2D QueueAndFootprint(FVector2D(560, 240), FVector2D(990, 420));
    const FBox2D OldPing(Marker + FVector2D(52, -64), Marker + FVector2D(52, -64) + PingSize);
    TestTrue(TEXT("Fixture reproduces the queue obscured by the old label"), OldPing.Intersect(QueueAndFootprint));
    const TArray<FBox2D> Panels = {FBox2D(FVector2D(20, 92), FVector2D(490, 520)),
        FBox2D(FVector2D(490, 420), FVector2D(1000, 620))};
    for (const FVector2D Size : {FVector2D(1280, 720), FVector2D(1920, 1080)})
    {
        FVector2D Position;
        if (TestTrue(TEXT("A long queue retains a clear label outside its reservation"),
            FFBuildToolsModel::ResolveContextPingPosition(Marker, PingSize, Size, QueueAndFootprint, Panels, 52, Position)))
        {
            const FBox2D Ping(Position, Position + PingSize);
            TestFalse(TEXT("Label leaves the entire queue and facility visible"), Ping.Intersect(QueueAndFootprint.ExpandBy(12)));
            for (const FBox2D& Panel : Panels)
                TestFalse(TEXT("Label does not move onto another HUD surface"), Ping.Intersect(Panel));
            TestTrue(TEXT("Label stays within the usable viewport"), Position.X >= 24 && Position.Y >= 40 &&
                Ping.Max.X <= Size.X - 24 && Ping.Max.Y <= Size.Y - 100);
        }
    }
    // Rotation, zoom and viewport edges can project a much wider reservation.
    FVector2D Position;
    const FBox2D WidePreview(FVector2D(650, 80), FVector2D(1260, 500));
    TestTrue(TEXT("A preview at the screen edge still offers a clear side"),
        FFBuildToolsModel::ResolveContextPingPosition(FVector2D(1240, 320), PingSize, FVector2D(1280, 720),
            WidePreview, {}, 100, Position));
    TestFalse(TEXT("Clamping at the viewport edge never pushes the label onto the preview"),
        FBox2D(Position, Position + PingSize).Intersect(WidePreview.ExpandBy(12)));
    TestFalse(TEXT("Hide the floating label when the preview occupies the whole usable viewport"),
        FFBuildToolsModel::ResolveContextPingPosition(Marker, PingSize, FVector2D(1280, 720),
            FBox2D(FVector2D::ZeroVector, FVector2D(1280, 720)), {}, 52, Position));
    return true;
}

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
    (void)Parameters;
    auto* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, FFBuildUIAssetPaths::BuildToolsObject);
    if (!TestNotNull(TEXT("Saved production build tools"), Blueprint) ||
        !TestNotNull(TEXT("Designer widget tree"), Blueprint ? Blueprint->WidgetTree.Get() : nullptr)) return false;
    TestTrue(TEXT("The saved screen retains its native controller"), Blueprint->GeneratedClass &&
        Blueprint->GeneratedClass->IsChildOf(UFF_BuildToolsWidget::StaticClass()));
    UWidgetTree* Tree = Blueprint->WidgetTree;
    TestNotNull(TEXT("The active icon toolbar is an editable child Widget Blueprint"),
        Cast<UUserWidget>(Tree->FindWidget(TEXT("Widget_ToolbarFeatureIcons"))));
    TestNotNull(TEXT("The floating toolbar retains its own pointer surface"),
        Cast<UBorder>(Tree->FindWidget(TEXT("Border_BuildTopBar"))));
    TestNotNull(TEXT("Build back navigation remains independently authored"),
        Cast<UBorder>(Tree->FindWidget(TEXT("Border_BuildBack"))));
    // 설치는 월드 클릭으로 확정한다. 예전 확인·목록·도움말 도크를 다시 요구하지 않는다.
    for (const TCHAR* Retired : {TEXT("Border_Tools"), TEXT("Button_Confirm"), TEXT("Button_Catalog"),
        TEXT("Button_Help"), TEXT("Button_Facts"), TEXT("Button_Radial"), TEXT("Panel_Radial"), TEXT("Panel_Details")})
        TestNull(FString(TEXT("Retired duplicate action is absent: ")) + Retired, Tree->FindWidget(Retired));
    const USizeBox* Top = Cast<USizeBox>(Tree->FindWidget(TEXT("Size_BuildTopBar")));
    const UCanvasPanelSlot* TopSlot = Top ? Cast<UCanvasPanelSlot>(Top->Slot) : nullptr;
    if (TestNotNull(TEXT("Floating toolbar has Designer-owned placement"), TopSlot))
        TestTrue(TEXT("The icon strip stays centered at the upper edge"),
            TopSlot->GetAnchors().Minimum.Equals(FVector2D(.5f, 0.f)) &&
            TopSlot->GetAlignment().Equals(FVector2D(.5f, 0.f)) && TopSlot->GetPosition().Y >= 0.f);
    const USizeBox* Info = Cast<USizeBox>(Tree->FindWidget(TEXT("Size_PlacementInfo")));
    const UCanvasPanelSlot* InfoSlot = Info ? Cast<UCanvasPanelSlot>(Info->Slot) : nullptr;
    if (TestNotNull(TEXT("Placement information has an independent canvas surface"), InfoSlot))
        TestTrue(TEXT("Placement information leaves persistent bottom navigation clear"),
            InfoSlot->GetAnchors().Minimum.Equals(FVector2D(1.f, 1.f)) && InfoSlot->GetPosition().Y <= -60.f);
    for (const TCHAR* Name : {TEXT("Text_FacilityName"), TEXT("Text_Cost"), TEXT("Text_Footprint"),
        TEXT("Text_Legend"), TEXT("Text_PlacementSynergySummary"), TEXT("Text_PlacementAdvice")})
        TestNotNull(FString(TEXT("Current placement information binding: ")) + Name, Cast<UTextBlock>(Tree->FindWidget(Name)));
    const UWidget* Settings = Tree->FindWidget(TEXT("Panel_BuildSettings"));
    TestTrue(TEXT("Settings start dismissed in the saved screen"), Settings && Settings->GetVisibility() == ESlateVisibility::Collapsed);
    for (const TCHAR* Name : {TEXT("Button_BuildBack"), TEXT("Button_BuildSettingsClose"),
        TEXT("Button_SettingsGrid"), TEXT("Button_SettingsSnapMeter"), TEXT("Button_SettingsSnapPrecision"),
        TEXT("Button_SettingsSnapFree"), TEXT("Button_SettingsDirectAdjustment"), TEXT("Button_SettingsFacilityRanges"),
        TEXT("Button_SettingsZoomToCursor"), TEXT("Button_SettingsEdgeScroll"), TEXT("Button_DirectAdjustment"),
        TEXT("Button_RotateLeft"), TEXT("Button_RotateRight"), TEXT("Button_FreeRotateLeft"), TEXT("Button_FreeRotateRight"),
        TEXT("Button_NudgeLeft"), TEXT("Button_NudgeRight"), TEXT("Button_NudgeUp"), TEXT("Button_NudgeDown")})
    {
        const auto* Button = Cast<UFF_BuildToolsActionButton>(Tree->FindWidget(Name));
        if (TestNotNull(FString(TEXT("Native explicit action: ")) + Name, Button))
            TestFalse(TEXT("World editing buttons yield keyboard controls back to the game"), Button->GetIsFocusable());
    }
    TestNotNull(TEXT("Settings retain precise angle editing"), Cast<UEditableTextBox>(Tree->FindWidget(TEXT("Editable_Angle"))));
    TestNotNull(TEXT("Settings retain a scrollable body"), Cast<UScrollBox>(Tree->FindWidget(TEXT("Scroll_BuildSettings"))));
    TestNotNull(TEXT("The catalog has its compact instruction surface"), Cast<USizeBox>(Tree->FindWidget(TEXT("Size_ApprovedQuickKeys"))));
    TestNotNull(TEXT("Full instructions remain available outside catalog browsing"), Cast<UBorder>(Tree->FindWidget(TEXT("Border_PlacementHelp"))));
    TestNotNull(TEXT("Full instructions retain scrolling for short viewports"), Cast<UScrollBox>(Tree->FindWidget(TEXT("Scroll_BuildHelp"))));
    for (const TCHAR* Name : {TEXT("Text_SettingsGrid"), TEXT("Text_SettingsFacilityRanges"),
        TEXT("Text_SettingsDirectAdjustment"), TEXT("Text_SettingsZoomToCursor"), TEXT("Text_SettingsEdgeScroll")})
    {
        const auto* Label = Cast<UTextBlock>(Tree->FindWidget(Name));
        TestTrue(FString(TEXT("Current Designer toggle state: ")) + Name,
            Label && (Label->GetText().ToString() == TEXT("켜짐") || Label->GetText().ToString() == TEXT("꺼짐")));
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
    for (int32 Index = 1; Index <= 7; ++Index)
    {
        auto* Caption = Cast<UTextBlock>(Widget->GetWidgetFromName(
            FName(*FString::Printf(TEXT("Label_PearlTool%d"), Index))));
        if (TestNotNull(TEXT("Authored toolbar caption"), Caption))
            TestEqual(TEXT("Toolbar captions do not occupy the icon-only strip"), Caption->GetVisibility(), ESlateVisibility::Collapsed);
    }
    auto* GuideCaption = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_FeatureGuide")));
    if (TestNotNull(TEXT("Feature guide caption"), GuideCaption))
        TestEqual(TEXT("Feature guide keeps only its icon in the strip"), GuideCaption->GetVisibility(), ESlateVisibility::Collapsed);
    if (UWidget* Undo = Widget->GetWidgetFromName(TEXT("Button_BuildHelpUndo")))
        TestTrue(TEXT("Undo remains available in its own shortcut strip"), Undo->GetParent() &&
            Undo->GetParent()->GetName().StartsWith(TEXT("Border_A5_BuildHelpRows_")));
    Widget->ShowFeatureSummary(TEXT("BuildGrid"));
    auto* Summary = Cast<USizeBox>(Widget->GetWidgetFromName(TEXT("Size_A5_FeatureSummary")));
    auto* SummaryTitle = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_A5_FeatureSummaryTitle")));
    auto* SummaryBody = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_A5_FeatureSummaryBody")));
    if (TestNotNull(TEXT("Non-interactive toolbar explanation"), Summary) &&
        TestNotNull(TEXT("Toolbar explanation title"), SummaryTitle) &&
        TestNotNull(TEXT("Toolbar explanation body"), SummaryBody))
    {
        TestEqual(TEXT("Explanation never consumes mouse input"), Summary->GetVisibility(), ESlateVisibility::HitTestInvisible);
        TestFalse(TEXT("Explanation contains a title and description"), SummaryTitle->GetText().IsEmpty() || SummaryBody->GetText().IsEmpty());
        const FText GridTitle = SummaryTitle->GetText();
        Widget->ShowFeatureSummary(TEXT("BuildSnap"));
        TestFalse(TEXT("A different toolbar action replaces the description"), SummaryTitle->GetText().EqualTo(GridTitle));
        Widget->HideFeatureSummary();
        TestEqual(TEXT("Explanation can be dismissed"), Summary->GetVisibility(), ESlateVisibility::Collapsed);
    }
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
        TEXT("Button_SettingsZoomToCursor"),
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
    Widget->ToggleHelp();
    TestTrue(TEXT("Help opens independently beneath the left summary"), Open(TEXT("Border_PlacementHelp")));
    TestTrue(TEXT("Visible instructions retain the same toolbar toggle"), Widget->bHelpExpanded);
    Widget->ToggleHelp();
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
    TestFalse(TEXT("Closing instructions clears their expanded intent"), Widget->bHelpExpanded);
    Widget->ToggleHelp();
    Widget->ToggleRadialMenu();
    MenuRemainsRemoved();
    TestTrue(TEXT("A legacy menu call leaves the left-hand instructions readable"), Open(TEXT("Border_PlacementHelp")));
    // 세부 정보 호환 함수는 남아 있지만 제거된 버튼을 다시 만들지는 않는다.
    Widget->ToggleFacts();
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
    Widget->ToggleFacts();
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
