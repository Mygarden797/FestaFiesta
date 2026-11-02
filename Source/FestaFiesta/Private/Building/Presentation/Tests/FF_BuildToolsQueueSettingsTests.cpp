#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Input/HittestGrid.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "RenderingThread.h"
#include "Rendering/SlateRenderer.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"
#include "Widgets/SVirtualWindow.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Layout/SDPIScaler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsQueueSettingsTest,
    "FestaFiesta.Building.BuildTools.QueueSettingsLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolsQueueSettingsTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    auto* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
    auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
    auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildToolsWidget>(nullptr,
        FFBuildUIAssetPaths::BuildToolsClass));
    if (!TestNotNull(TEXT("Build subsystem"), Build) || !TestNotNull(TEXT("Zone authority"), Zones) ||
        !TestNotNull(TEXT("Saved production toolbar"), Class)) return false;
    const bool PreviousPlayerless = Class->bCanCallInitializedWithoutPlayerContext;
    Class->bCanCallInitializedWithoutPlayerContext = true;
    ON_SCOPE_EXIT { Class->bCanCallInitializedWithoutPlayerContext = PreviousPlayerless; };
    TStrongObjectPtr<UFF_BuildToolsWidget> Widget(CreateWidget<UFF_BuildToolsWidget>(World, Class));
    if (!TestNotNull(TEXT("Production toolbar instance"), Widget.Get())) return false;
    const TSharedRef<SWidget> Slate = Widget->TakeWidget();
    Widget->FeatureDiscovery.Reset();
    Build->BuildToolsWidget = Widget.Get();
    Zones->SetChangePhase(EFFZSChangePhase::InitialSetup);
    Build->State = EFF_BuildModeState::BuildMode;
    Build->bBuildSessionActive = true;
    ON_SCOPE_EXIT
    {
        Build->BuildToolsWidget = nullptr;
        Build->State = EFF_BuildModeState::Closed;
        Build->bBuildSessionActive = false;
        Build->MovingFacility.Reset();
        Build->MovingFacilityInstanceId.Invalidate();
        Build->ResetPlacementSelection();
    };
    Widget->RefreshTools();
    auto* Teams = Cast<UEditableTextBox>(Widget->GetWidgetFromName(TEXT("Editable_QueueMaxTeams")));
    auto* Spacing = Cast<UEditableTextBox>(Widget->GetWidgetFromName(TEXT("Editable_QueueSlotSpacing")));
    auto* Width = Cast<UEditableTextBox>(Widget->GetWidgetFromName(TEXT("Editable_QueueLaneWidth")));
    auto* Apply = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_ApplyQueueSettings")));
    auto* Reset = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_ResetAllQueues")));
    auto* Summary = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_QueueSettingsSummary")));
    if (!TestNotNull(TEXT("Runtime team input"), Teams) || !TestNotNull(TEXT("Runtime spacing input"), Spacing) ||
        !TestNotNull(TEXT("Runtime width input"), Width) || !TestNotNull(TEXT("Atomic apply action"), Apply) ||
        !TestNotNull(TEXT("Common defaults action"), Reset) || !TestNotNull(TEXT("Queue summary"), Summary)) return false;
    TestTrue(TEXT("All numeric inputs have native commit bindings"), Teams->OnTextCommitted.IsBound() &&
        Spacing->OnTextCommitted.IsBound() && Width->OnTextCommitted.IsBound());
    TestTrue(TEXT("Both actions have native bindings"), Apply->OnClicked.IsBound() && Reset->OnClicked.IsBound());
    TestFalse(TEXT("Per-facility settings require a queue facility"), Teams->GetIsEnabled());
    TestTrue(TEXT("Common defaults remain available without selecting a facility"), Reset->GetIsEnabled());
    bool bInsideScroll = false;
    for (const UWidget* Parent = Teams->GetParent(); Parent; Parent = Parent->GetParent())
        if (Parent->IsA<UScrollBox>()) bInsideScroll = true;
    TestTrue(TEXT("New controls remain reachable in the settings scroll at small viewport heights"), bInsideScroll);
    auto* Body = Cast<UVerticalBox>(Widget->WidgetTree->FindWidget(TEXT("Column_BuildSettings")));
    if (!TestNotNull(TEXT("Settings content column"), Body)) return false;
    const int32 ChildCount = Body->GetChildrenCount();
    TestTrue(TEXT("The compatibility helper is idempotent"), FFBuildToolsModel::EnsureQueueSettingsControls(*Widget->WidgetTree));
    TestEqual(TEXT("Calling compatibility again does not create another section"),
        Body->GetChildrenCount(), ChildCount);

    AFFZSFacilityActor* Facility = World->SpawnActor<AFFZSFacilityActor>();
    const FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec();
    if (!TestNotNull(TEXT("Draft source facility"), Facility) || !TestTrue(TEXT("Source initializes"),
        Facility->InitializeFacility(FGuid::NewGuid(), Spec, FFZSTest::ProjectZoneId(), FFFZSGridPlacement(), FTransform::Identity))) return false;
    Build->State = EFF_BuildModeState::Moving;
    Build->MovingFacility = Facility;
    Build->MovingFacilityInstanceId = Facility->GetFacilityInstanceId();
    Build->SelectedSpec = Facility->GetFacilitySpec();
    Build->PendingQueueLayout = Facility->GetQueueLayout();
    Build->PendingQueueSettings = Facility->GetBaseFacilitySpec().QueueSettings;
    Build->bHasPlacementTarget = false;
    Widget->RefreshTools();
    Widget->ToggleBuildSettings();
    const TSharedPtr<SWidget> PreviousFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
    const TSharedRef<SVirtualWindow> Window = SNew(SVirtualWindow).Size(FVector2D(1280, 720));
    Window->SetContent(Slate);
    FSlateApplication::Get().RegisterVirtualWindow(Window);
    Window->SlatePrepass();
    ON_SCOPE_EXIT
    {
        FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
        FSlateApplication::Get().UnregisterVirtualWindow(Window);
        if (PreviousFocus) FSlateApplication::Get().SetKeyboardFocus(PreviousFocus);
    };
    for (UEditableTextBox* Input : {Teams, Spacing, Width})
    {
        FSlateApplication::Get().SetKeyboardFocus(Input->TakeWidget(), EFocusCause::SetDirectly);
        TestTrue(TEXT("Every queue input owns real Slate text focus"), Input->HasKeyboardFocus() || Input->HasFocusedDescendants());
        TestTrue(TEXT("Focused queue text freezes placement and participates in camera text guards"), Widget->IsEditingText());
    }
    TestTrue(TEXT("Moving an existing queue exposes the same settings inputs"), Teams->GetIsEnabled() && Apply->GetIsEnabled());
    TestTrue(TEXT("Legacy width is identified as a proposed value"), Summary->GetText().ToString().Contains(TEXT("제안값")));
    const int64 Revision = Zones->GetWorldRevision();
    const auto Type = [](UEditableTextBox* Input, const TCHAR* Text)
    {
        Input->SetText(FText::FromString(Text));
        Input->OnTextChanged.Broadcast(Input->GetText());
    };
    Type(Teams, TEXT("24"));
    Teams->OnTextCommitted.Broadcast(Teams->GetText(), ETextCommit::OnUserMovedFocus);
    Widget->RefreshTools();
    TestEqual(TEXT("Moving between fields keeps the pending capacity buffer"), Teams->GetText().ToString(), FString(TEXT("24")));
    TestEqual(TEXT("Focus loss does not partially apply capacity"), Build->GetEditingQueueSettings().MaxWaitingTeams, Spec.QueueRules.MaxWaitingAgents);
    Type(Spacing, TEXT("125.5"));
    Type(Width, TEXT("650.25"));
    Width->OnTextCommitted.Broadcast(Width->GetText(), ETextCommit::OnEnter);
    const FFFZSQueueSettings Applied = Build->GetEditingQueueSettings();
    TestTrue(TEXT("Enter enables the common configurable queue"), Applied.bEnabled);
    TestEqual(TEXT("Enter applies the team capacity"), Applied.MaxWaitingTeams, 24);
    TestEqual(TEXT("Enter applies decimal centimeter spacing"), Applied.SlotSpacingCm, 125.5f);
    TestEqual(TEXT("Enter applies decimal lane width"), Applied.LaneWidthCm, 650.25f);
    TestEqual(TEXT("Editing a moving draft leaves authority untouched"), Zones->GetWorldRevision(), Revision);
    TestFalse(TEXT("Moving draft leaves the original facility unchanged"), Facility->GetBaseFacilitySpec().QueueSettings.bEnabled);
    TestTrue(TEXT("Current length is displayed after applying settings"), Summary->GetText().ToString().Contains(TEXT("현재 줄 길이")));
    TestTrue(TEXT("Enter cannot fall through to world confirmation"), Build->bSuppressPlacementConfirmUntilReleased);
    Type(Teams, TEXT("99"));
    Type(Width, TEXT("99"));
    Apply->OnClicked.Broadcast();
    TestTrue(TEXT("A malformed width cannot partially update the capacity"), Build->GetEditingQueueSettings() == Applied);
    const FKeyEvent Escape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
    TestTrue(TEXT("Escape is consumed by the queue editor before pause or world input"),
        Widget->NativeOnPreviewKeyDown(FGeometry(), Escape).IsEventHandled());
    TestEqual(TEXT("Esc restores capacity alongside the cancelled field"), Teams->GetText().ToString(), FString(TEXT("24")));
    TestEqual(TEXT("Esc restores the exact previous width"), Width->GetText().ToString(), FString(TEXT("650.25")));
    if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        // Render the real Game/Moving state, including its additive runtime controls.
        // The ordinary authored preview fixture intentionally has no active build session.
        auto* SettingsScroll = Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("Scroll_BuildSettings")));
        if (!TestNotNull(TEXT("Queue settings use the production settings scroll"), SettingsScroll)) return false;
        FSlateApplication::Get().ClearKeyboardFocus();
        Window->SetContent(SNullWidget::NullWidget);
        TSet<UMaterialInterface*> QueueControlMaterials;
        const auto IncludeBrush = [&](const FSlateBrush& Brush)
        {
            if (auto* Material = Cast<UMaterialInterface>(Brush.GetResourceObject())) QueueControlMaterials.Add(Material);
        };
        Widget->WidgetTree->ForEachWidget([&](UWidget* Child)
        {
            if (const auto* Button = Cast<UButton>(Child))
            {
                IncludeBrush(Button->GetStyle().Normal);
                IncludeBrush(Button->GetStyle().Disabled);
            }
            if (const auto* Border = Cast<UBorder>(Child)) IncludeBrush(Border->Background);
        });
        for (UMaterialInterface* Material : QueueControlMaterials) Material->EnsureIsComplete();
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        FlushRenderingCommands();
        const FString QueueCaptureDirectory = FPaths::ProjectSavedDir() / TEXT("Automation/BuildableGrid");
        IFileManager::Get().MakeDirectory(*QueueCaptureDirectory, true);
        const auto QueueBounds = [](const UWidget* Control)
            { return Control->GetPaintSpaceGeometry().GetRenderBoundingRect(); };
        const auto QueueContains = [](const FSlateRect& Outer, const FSlateRect& Inner)
        {
            return Inner.Left >= Outer.Left - 1.f && Inner.Right <= Outer.Right + 1.f &&
                Inner.Top >= Outer.Top - 1.f && Inner.Bottom <= Outer.Bottom + 1.f;
        };
        const auto QueueOverlaps = [](const FSlateRect& A, const FSlateRect& B)
        {
            return A.Left < B.Right - .5f && A.Right > B.Left + .5f && A.Top < B.Bottom - .5f && A.Bottom > B.Top + .5f;
        };
        for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
        {
            const FVector2D QueueCaptureSize(Resolution.X, Resolution.Y);
            const float QueueDpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
            const TSharedRef<SVirtualWindow> QueueRenderWindow = SNew(SVirtualWindow).Size(QueueCaptureSize);
            QueueRenderWindow->SetContent(SNew(SDPIScaler).DPIScale(QueueDpi)[Slate]);
            FWidgetRenderer QueueRenderer(false, true);
            TStrongObjectPtr<UTextureRenderTarget2D> QueueRenderTarget(
                FWidgetRenderer::CreateTargetFor(QueueCaptureSize, TF_Bilinear, true));
            if (!TestNotNull(TEXT("Queue settings real RHI target"), QueueRenderTarget.Get())) return false;
            FHittestGrid QueueHitGrid;
            SettingsScroll->ScrollToEnd();
            for (int32 Frame = 0; Frame < 4; ++Frame)
            {
                Widget->ForceLayoutPrepass();
                QueueRenderer.DrawWindow(QueueRenderTarget.Get(), QueueHitGrid, QueueRenderWindow, 1.f,
                    QueueCaptureSize, .016f, false);
            }
            const FString QueueContext = FString::Printf(TEXT("Queue settings %dp"), Resolution.Y);
            const FSlateRect QueueScrollClip = QueueBounds(SettingsScroll);
            TestTrue(QueueContext + TEXT(" settings scroll stays inside the viewport"),
                QueueContains(FSlateRect(0.f, 0.f, Resolution.X, Resolution.Y), QueueScrollClip));
            TestTrue(QueueContext + TEXT(" scroll reaches the added section"), SettingsScroll->GetScrollOffset() > 0.f);
            const TArray<UWidget*> QueueControls = {Teams, Spacing, Width, Apply, Reset};
            for (UWidget* Control : QueueControls)
            {
                const FString QueueLabel = QueueContext + TEXT(": ") + Control->GetName();
                TestTrue(QueueLabel + TEXT(" is visible and enabled"), Control->IsVisible() && Control->GetIsEnabled());
                TestTrue(QueueLabel + TEXT(" fits completely in the scrolled clip"), QueueContains(QueueScrollClip, QueueBounds(Control)));
                TestTrue(QueueLabel + TEXT(" has a usable target"), Control->GetCachedGeometry().GetLocalSize().X >= 30.f &&
                    Control->GetCachedGeometry().GetLocalSize().Y >= 20.f);
                if (const auto* Button = Cast<UButton>(Control); Button && Button->GetContent())
                    TestTrue(QueueLabel + TEXT(" contains its caption"), QueueContains(QueueBounds(Control), QueueBounds(Button->GetContent())));
            }
            for (int32 A = 0; A < QueueControls.Num(); ++A)
                for (int32 B = A + 1; B < QueueControls.Num(); ++B)
                    TestFalse(QueueContext + TEXT(" numeric inputs and actions do not overlap"),
                        QueueOverlaps(QueueBounds(QueueControls[A]), QueueBounds(QueueControls[B])));
            for (const UEditableTextBox* Input : {Teams, Spacing, Width})
            {
                const FEditableTextBoxStyle& QueueFieldStyle = Input->GetWidgetStyle();
                const float QueueTextWidth = Input->GetCachedGeometry().GetLocalSize().X -
                    QueueFieldStyle.Padding.Left - QueueFieldStyle.Padding.Right - 4.f;
                for (const TCHAR* Sample : {TEXT("125.5"), TEXT("650.25"), TEXT("1000")})
                {
                    const FVector2D QueueMeasuredText = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(
                        FText::FromString(Sample), QueueFieldStyle.TextStyle.Font);
                    TestTrue(FString::Printf(TEXT("%s: %s shows every digit of %s (available %.2f, measured %.2f)"),
                        *QueueContext, *Input->GetName(), Sample, QueueTextWidth, QueueMeasuredText.X),
                        QueueTextWidth + 1.f >= QueueMeasuredText.X);
                }
            }
            TestTrue(QueueContext + TEXT(" automatic length explanation is fully visible"), QueueContains(QueueScrollClip, QueueBounds(Summary)));
            TArray<FColor> QueuePixels;
            FReadSurfaceDataFlags QueueReadFlags;
            QueueReadFlags.SetLinearToGamma(false);
            if (TestTrue(QueueContext + TEXT(" renders a complete image"), QueueRenderTarget->GameThread_GetRenderTargetResource()->ReadPixels(
                QueuePixels, QueueReadFlags) && QueuePixels.Num() == Resolution.X * Resolution.Y))
            {
                TArray64<uint8> QueuePng;
                FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, QueuePixels, QueuePng);
                const FString QueueCapturePath = QueueCaptureDirectory / FString::Printf(TEXT("BuildTools_QueueSettingsEnd_%d.png"), Resolution.Y);
                TestTrue(QueueContext + TEXT(" image is saved"), FFileHelper::SaveArrayToFile(QueuePng, *QueueCapturePath));
                AddInfo(FString::Printf(TEXT("Saved %s"), *FPaths::ConvertRelativePathToFull(QueueCapturePath)));
            }
            QueueRenderWindow->SetContent(SNullWidget::NullWidget);
        }
        Window->SetContent(Slate);
    }
    Build->bInputSuspendedForPauseMenu = true;
    Widget->RefreshTools();
    TestFalse(TEXT("Paused edits disable queue settings"), Teams->GetIsEnabled() || Apply->GetIsEnabled() || Reset->GetIsEnabled());
    Build->bInputSuspendedForPauseMenu = false;
    return true;
}

#endif
