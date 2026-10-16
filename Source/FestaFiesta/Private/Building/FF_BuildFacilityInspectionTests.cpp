// FF_BuildFacilityInspectionTests.cpp
// 역할: BuildFacilityInspection 기능의 동작 계약을 자동 검사한다 (검사 경로: FestaFiesta.Building.BuildMode.FacilityInspection).

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_ProjectZoneTestFixture.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_RequiredFacilitiesSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Components/BoxComponent.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Staff/FF_StaffFacilitySubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelCheck.h"
#include "UnrealClient.h"
#include "Widgets/SOverlay.h"

namespace
{
    // Only the platform cursor and viewport dimensions are supplied by this adapter.
    // Camera deprojection, collision tracing, registry lookup and the inspector remain real.
    class FInspectionInputViewport final : public FViewport
    {
    public:
        explicit FInspectionInputViewport(FViewportClient* Client) : FViewport(Client) {}
        virtual FIntPoint GetSizeXY() const override { return FIntPoint(512, 512); }
        virtual void* GetWindow() override { return nullptr; }
        virtual void MoveWindow(int32, int32, int32, int32) override {}
        virtual void Destroy() override {}
        virtual bool SetUserFocus(bool) override { return true; }
        virtual bool KeyState(FKey) const override { return false; }
        virtual int32 GetMouseX() const override { return Cursor.X; }
        virtual int32 GetMouseY() const override { return Cursor.Y; }
        virtual void GetMousePos(FIntPoint& Position, bool = true) override { Position = Cursor; }
        virtual void SetMouse(int32 X, int32 Y) override { Cursor = FIntPoint(X, Y); }
        virtual void ProcessInput(float) override {}
        virtual FVector2D VirtualDesktopPixelToViewport(FIntPoint Point) const override
        {
            return FVector2D(Point) / FVector2D(GetSizeXY());
        }
        virtual FIntPoint ViewportToVirtualDesktopPixel(FVector2D Point) const override
        {
            return FIntPoint(FMath::RoundToInt(Point.X * 512), FMath::RoundToInt(Point.Y * 512));
        }
        virtual void InvalidateDisplay() override {}
        virtual FViewportFrame* GetViewportFrame() override { return nullptr; }
    private:
        FIntPoint Cursor = FIntPoint(256, 256);
    };

    class FScopedInspectionSettings
    {
    public:
        FScopedInspectionSettings()
            : Settings(GetMutableDefault<UFFZSDeveloperSettings>())
            , bGround(Settings->bEnableGroundChecks)
            , bCollision(Settings->bEnableWorldCollisionChecks)
            , bNavigation(Settings->bEnableNavigationProjectionChecks)
            , bPolicies(Settings->bRequireProductionPolicyDomains)
            , InitialPhase(Settings->InitialPhase)
        {
            Settings->bEnableGroundChecks = false;
            Settings->bEnableWorldCollisionChecks = false;
            Settings->bEnableNavigationProjectionChecks = false;
            Settings->bRequireProductionPolicyDomains = false;
            Settings->InitialPhase = EFFZSChangePhase::InitialSetup;
        }
        ~FScopedInspectionSettings()
        {
            Settings->bEnableGroundChecks = bGround;
            Settings->bEnableWorldCollisionChecks = bCollision;
            Settings->bEnableNavigationProjectionChecks = bNavigation;
            Settings->bRequireProductionPolicyDomains = bPolicies;
            Settings->InitialPhase = InitialPhase;
        }
    private:
        UFFZSDeveloperSettings* Settings;
        bool bGround, bCollision, bNavigation, bPolicies;
        EFFZSChangePhase InitialPhase;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildFacilityInspectionTest,
    "FestaFiesta.Building.BuildMode.FacilityInspection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildFacilityInspectionTest::RunTest(const FString& Parameters)
{
    FScopedInspectionSettings Settings;
    // The private overlay is a real viewport host for AddToViewport, without opening a
    // window or substituting a test panel for the existing production inspector.
    const TSharedRef<SOverlay> Overlay = SNew(SOverlay);
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game))
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }
    UWorld* World = Fixture.GetTestWorld();
    UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
    UFF_BuildModeSubsystem* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    UFF_FacilityInfoSubsystem* Info = World ? World->GetSubsystem<UFF_FacilityInfoSubsystem>() : nullptr;
    UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game world"), World) || !TestNotNull(TEXT("Game instance"), Game) ||
        !TestNotNull(TEXT("Real build subsystem"), Build) || !TestNotNull(TEXT("Existing info subsystem"), Info) ||
        !TestNotNull(TEXT("Zone subsystem"), Zones))
    {
        return false;
    }
    FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
    if (!TestNotNull(TEXT("Test world context"), Context))
    {
        return false;
    }
    UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
    Context->GameViewport = Viewport;
    Viewport->Init(*Context, Game, false);
    Viewport->SetViewportOverlayWidget(nullptr, Overlay);
    FInspectionInputViewport InputViewport(Viewport);
    Viewport->Viewport = &InputViewport;
    ON_SCOPE_EXIT { Viewport->Viewport = nullptr; };
    FString PlayerError;
    ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
    APlayerController* Controller = World->SpawnActor<AFF_FestivalPlayerController>();
    if (!TestNotNull(*FString::Printf(TEXT("Real local player: %s"), *PlayerError), Player) ||
        !TestNotNull(TEXT("Local controller"), Controller))
    {
        return false;
    }
    Controller->SetPlayer(Player);
    Player->Origin = FVector2D::ZeroVector;
    Player->Size = FVector2D(1.0, 1.0);
    if (!TestFalse(TEXT("Synthetic input viewport has no Slate game viewport widget"),
        Viewport->GetGameViewportWidget().IsValid()))
    {
        return false;
    }
    Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
    const TSharedPtr<SWidget> PreviousKeyboardFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
    FSlateApplication::Get().ClearKeyboardFocus();
    ON_SCOPE_EXIT
    {
        if (PreviousKeyboardFocus.IsValid())
        {
            FSlateApplication::Get().SetKeyboardFocus(PreviousKeyboardFocus);
        }
        else
        {
            FSlateApplication::Get().ClearKeyboardFocus();
        }
    };
    // FTestWorldWrapper deliberately leaves actors uninitialized. The normal
    // AController::PostInitializeComponents registration therefore has not run;
    // mirror that world bookkeeping so production controller lookup is exercised.
    World->AddController(Controller);
    if (!TestEqual(TEXT("World lookup resolves the fixture controller"), World->GetFirstPlayerController(), Controller) ||
        !TestTrue(TEXT("Fixture controller is local"), Controller->IsLocalController()) ||
        !TestEqual(TEXT("Fixture controller owns the real local player"), Controller->GetLocalPlayer(), Player) ||
        !TestEqual(TEXT("Build subsystem resolves the real local controller"), Build->GetLocalPlayerController(), Controller))
    {
        return false;
    }

    UFF_FestivalProgressionSubsystem* Progression = Game->GetSubsystem<UFF_FestivalProgressionSubsystem>();
    UFF_FestivalStageSubsystem* Stage = Game->GetSubsystem<UFF_FestivalStageSubsystem>();
    if (!TestNotNull(TEXT("Festival progression"), Progression) ||
        !TestNotNull(TEXT("Festival stage"), Stage))
    {
        return false;
    }
    UFF_FestivalDefinition* Festival = NewObject<UFF_FestivalDefinition>(Game);
    UDataTable* Costs = NewObject<UDataTable>(Game);
    Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
    FFF_FacilityCostRow CostRow;
    CostRow.InstallCost = 100;
    Costs->AddRow(TEXT("Cost.Booth.Default"), CostRow);
    Festival->FestivalId = TEXT("BuildFacilityInspectionAutomation");
    Festival->FacilityCostTable = Costs;
    Festival->InitialGold = 10000;
    // This fixture adds its own mandatory facility below; unrelated production
    // defaults would send compact guidance to a missing facility's catalog instead.
    Festival->RequiredSetupFacilities.Reset();
    Progression->StartFestival(Festival);

    const FName ZoneId(FFProjectZoneTest::ZoneId());
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("Fixture zone"), Zone) ||
        !TestTrue(TEXT("Zone preset"), Zone->ApplyAcceptedZonePreset(ZoneId)) ||
        !TestTrue(TEXT("Zone registers"), Zones->RegisterZone(Zone)))
    {
        return false;
    }
    // Real visibility collision supplies the ground used by held-drag deprojection.
    AActor* GroundActor = World->SpawnActor<AActor>();
    UBoxComponent* GroundBox = NewObject<UBoxComponent>(GroundActor);
    GroundActor->SetRootComponent(GroundBox);
    GroundActor->AddInstanceComponent(GroundBox);
    GroundBox->SetBoxExtent(FVector(100000.0, 100000.0, 10.0));
    GroundBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    GroundBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    GroundBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GroundBox->RegisterComponent();
    GroundActor->SetActorLocation(FVector(0.0, 0.0, -10.0));
    const auto InstallFixture = [&](const int32 X) -> AFFZSFacilityActor*
    {
        FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
        Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_Fixture");
        Spec.InstallCostKey = NAME_None;
        FFFZSGridPlacement Placement;
        Placement.OriginCell = FIntPoint(X, 1000);
        Placement.YawCentidegrees = X == 1000 ? 3725 : 0;
        const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(
            Spec, ZoneId, Placement, Zones->GetWorldRevision());
        const FFFZSCommandResult Result = Zones->ExecuteCommand(Command);
        TestTrue(*FString::Printf(TEXT("Seed installed facility: %s"), *Result.Message.ToString()), Result.bSucceeded);
        return Result.bSucceeded ? Zones->FindFacility(Command.FacilityInstanceId) : nullptr;
    };
    AFFZSFacilityActor* First = InstallFixture(1000);
    AFFZSFacilityActor* Second = InstallFixture(4000);
    if (!TestNotNull(TEXT("First installed fixture"), First) || !TestNotNull(TEXT("Second installed fixture"), Second))
    {
        return false;
    }
    const int64 RevisionBeforeInspection = Zones->GetWorldRevision();
    const int32 GoldBeforeInspection = Progression->GetGold();
    const FTransform FirstTransform = First->GetActorTransform();
    const FFFZSGridPlacement FirstPlacement = First->GetGridPlacement();
    const auto AimAt = [Controller](const FVector& Target)
    {
        Controller->SetInitialLocationAndRotation(Target + FVector(0.0, 0.0, 2000.0), FRotator(-90.0, 0.0, 0.0));
    };
    TArray<UInputComponent*> InputStack;
    constexpr float InputDeltaSeconds = 1.0f / 60.0f;
    const auto QuickClick = [&]()
    {
        Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(
            EKeys::LeftMouseButton, IE_Pressed, 1.0f));
        Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(
            EKeys::LeftMouseButton, IE_Released, 0.0f));
        Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
        TestTrue(TEXT("Short inspection click retains its pressed edge"),
            Controller->WasInputKeyJustPressed(EKeys::LeftMouseButton));
        TestFalse(TEXT("Inspection accepts a click already released this frame"),
            Controller->IsInputKeyDown(EKeys::LeftMouseButton));
        Build->HandleInput(*Controller, InputDeltaSeconds);
        Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
        // Workspace transitions suppress the opening press until a released frame.
        Build->HandleInput(*Controller, InputDeltaSeconds);
    };
    const auto PressPointer = [&]()
    {
        Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(
            EKeys::LeftMouseButton, IE_Pressed, 1.0f));
        Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
        Build->HandleInput(*Controller, InputDeltaSeconds);
        Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
    };
    const auto HoldPointerAt = [&](const int32 X, const int32 Y)
    {
        InputViewport.SetMouse(X, Y);
        Build->HandleInput(*Controller, InputDeltaSeconds);
        Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
    };
    const auto ReleasePointer = [&]()
    {
        Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(
            EKeys::LeftMouseButton, IE_Released, 0.0f));
        Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
        Build->HandleInput(*Controller, InputDeltaSeconds);
        Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
        Build->HandleInput(*Controller, InputDeltaSeconds);
    };
    const auto CancelMove = [&]()
    {
        Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(
            EKeys::RightMouseButton, IE_Pressed, 1.0f));
        Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(
            EKeys::RightMouseButton, IE_Released, 0.0f));
        Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
        Build->HandleInput(*Controller, InputDeltaSeconds);
        Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
        TestEqual(TEXT("Right click returns a move to world building"),
            Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    };
    const auto PressEnter = [&]()
    {
        // Drain transition suppression on a released frame so this exercises the
        // delivered shortcut, rather than passing only because a prior click blocked it.
        Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
        Build->HandleInput(*Controller, InputDeltaSeconds);
        Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Enter, IE_Pressed, 1.0f));
        Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Enter, IE_Released, 0.0f));
        Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
        Build->HandleInput(*Controller, InputDeltaSeconds);
        Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
    };
    // The friend seam seeds only the workspace state. Raw input then follows the same
    // deprojection, selection proxy, registry and WBP path as a delivered gameplay click.
    Build->State = EFF_BuildModeState::BuildMode;
    Build->bBuildSessionActive = true;
    AimAt(First->SelectionProxy->GetComponentLocation());
    if (!TestEqual(TEXT("The real cursor ray resolves the registered first facility"),
        Info->ResolveFacilityUnderCursor(*Controller), First))
    {
        return false;
    }
    Build->bSuppressWorldClickUntilReleased = true;
    QuickClick();
    TestFalse(TEXT("A suppressed opening click cannot select the world behind UI"),
        Info->GetSelectedFacilityId().IsValid());
    QuickClick();
    TestEqual(TEXT("A world facility click opens information without movement"),
        Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("The first world click selects the installed instance for information"),
        Info->GetSelectedFacilityId(), First->GetFacilityInstanceId());
    TestEqual(TEXT("The first world click opens the build inspector"),
        Info->GetSelectionContext(), EFF_FacilityInfoContext::Build);
    TestNull(TEXT("The first world click does not create a movement preview"), Build->PlacementPreview.Get());
    TestNull(TEXT("The first world click has no movement target"), Build->MovingFacility.Get());
    TestEqual(TEXT("Opening information does not submit a facility command"),
        Zones->GetWorldRevision(), RevisionBeforeInspection);
    TestEqual(TEXT("Opening information preserves the festival balance"),
        Progression->GetGold(), GoldBeforeInspection);
    TestTrue(TEXT("Opening information keeps the original position and arbitrary rotation"),
        First->GetGridPlacement() == FirstPlacement && First->GetActorTransform().Equals(FirstTransform));
    TArray<UUserWidget*> Panels;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Panels, UFF_FacilityInfoPanel::StaticClass(), false);
    UFF_FacilityInfoPanel* Panel = Panels.Num() == 1 ? Cast<UFF_FacilityInfoPanel>(Panels[0]) : nullptr;
    if (!TestNotNull(TEXT("A single world click opens the existing facility info WBP"), Panel)) return false;
    TestTrue(TEXT("The clicked facility information is attached and visible"),
        Panel->IsInViewport() && Panel->IsVisible());
    TestEqual(TEXT("The first click displays the clicked facility's information"),
        Panel->GetCurrentViewData().FacilityInstanceId, First->GetFacilityInstanceId());
    Info->Tick(1.0f);
    TestTrue(TEXT("Periodic information refresh preserves the visible card while moving"), Panel->IsVisible());
    TestEqual(TEXT("Periodic information refresh retains the moving facility"),
        Info->GetSelectedFacilityId(), First->GetFacilityInstanceId());
    InputViewport.SetMouse(300, 300);
    Build->UpdatePlacementFromCursor(true);
    TestEqual(TEXT("Idle pointer motion after inspection never enters movement"),
        Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestTrue(TEXT("Idle pointer motion preserves the authoritative pose"),
        First->GetGridPlacement() == FirstPlacement);
    InputViewport.SetMouse(256, 256);
    QuickClick();
    TestEqual(TEXT("Repeated clicks inspect without movement"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    PressPointer();
    TestTrue(TEXT("A held facility press arms the threshold"), Build->bFacilityDragPending);
    HoldPointerAt(258, 258);
    TestEqual(TEXT("A small click wobble remains inspection"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    ReleasePointer();
    TestFalse(TEXT("Release below the threshold clears the gesture"), Build->bFacilityDragPending);
    TestEqual(TEXT("A small click wobble submits no move command"), Zones->GetWorldRevision(), RevisionBeforeInspection);
    InputViewport.SetMouse(256, 256);
    PressPointer();
    HoldPointerAt(280, 280);
    TestEqual(TEXT("Holding beyond the threshold starts moving"), Build->GetBuildModeState(), EFF_BuildModeState::Moving);
    TestTrue(TEXT("The held movement owns one drag"), Build->bFacilityDragActive);
    TestTrue(TEXT("The drag moves its preview before authority changes"),
        Build->bHasPlacementTarget && !(Build->CurrentPlacement == FirstPlacement));
    TestEqual(TEXT("The held preview leaves authority untouched"), Zones->GetWorldRevision(), RevisionBeforeInspection);
    TestTrue(TEXT("Information remains visible during the held drag"), Panel->IsVisible());

    UWidget* Card = Panel->GetWidgetFromName(TEXT("Border_Panel"));
    if (!TestNotNull(TEXT("The real information card owns pointer hover"), Card)) return false;
    const TSharedRef<SWidget> CardSlate = Card->TakeWidget();
    const FPointerEvent CardPointer;
    CardSlate->OnMouseEnter(Card->GetCachedGeometry(), CardPointer);
    TestTrue(TEXT("Hovering the information card blocks world pointer input"), Info->IsPointerOverPanel());
    ReleasePointer();
    TestEqual(TEXT("Release over information cancels the move"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("Release over information never submits a move command"), Zones->GetWorldRevision(), RevisionBeforeInspection);
    TestTrue(TEXT("A cancelled drag retains the original pose"), First->GetGridPlacement() == FirstPlacement);
    TestTrue(TEXT("A cancelled drag retains the information card"), Panel->IsVisible());
    CardSlate->OnMouseLeave(CardPointer);
    InputViewport.SetMouse(256, 256);
    PressEnter();
    TestEqual(TEXT("Enter while inspecting cannot implicitly demolish its facility"),
        Zones->FindFacility(First->GetFacilityInstanceId()), First);
    TestEqual(TEXT("Enter leaves authority unchanged"), Zones->GetWorldRevision(), RevisionBeforeInspection);

    // Invalid ground and pointer capture loss cannot commit the last valid preview.
    PressPointer();
    HoldPointerAt(288, 270);
    TestTrue(TEXT("Invalid-release regression starts a real drag"), Build->bFacilityDragActive);
    GroundBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ReleasePointer();
    GroundBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TestEqual(TEXT("Releasing without supporting ground cancels"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("Invalid release never commits the stale last-valid pose"), Zones->GetWorldRevision(), RevisionBeforeInspection);
    TestTrue(TEXT("Invalid release retains the original position"), First->GetGridPlacement() == FirstPlacement);
    InputViewport.SetMouse(256, 256);
    PressPointer();
    HoldPointerAt(290, 275);
    InputViewport.SetMouse(600, 600);
    ReleasePointer();
    TestEqual(TEXT("Release outside the viewport cancels"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("Outside release never commits"), Zones->GetWorldRevision(), RevisionBeforeInspection);
    InputViewport.SetMouse(256, 256);
    PressPointer();
    HoldPointerAt(288, 270);
    TestTrue(TEXT("Pause regression starts a real drag"), Build->bFacilityDragActive);
    Build->SuspendInputForPauseMenu();
    TestEqual(TEXT("Opening pause cancels an in-flight held drag"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    ReleasePointer();
    Build->RestoreInputAfterPauseMenu();
    TestFalse(TEXT("Resuming pause cannot resume the old gesture"), Build->bFacilityDragActive || Build->bFacilityDragPending);
    TestEqual(TEXT("Pause never submits the unfinished move"), Zones->GetWorldRevision(), RevisionBeforeInspection);
    InputViewport.SetMouse(256, 256);

    UButton* Move = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Move")));
    if (!TestNotNull(TEXT("The information panel retains its explicit Move action"), Move)) return false;
    TestTrue(TEXT("A movable facility enables its Move action"), Move->GetIsEnabled());
    Move->OnClicked.Broadcast();
    TestEqual(TEXT("The information panel Move action starts a move preview"),
        Build->GetBuildModeState(), EFF_BuildModeState::Moving);
    TestEqual(TEXT("Explicit move targets the inspected registered instance"),
        Build->MovingFacility.Get(), First);
    Info->Tick(1.0f);
    TestTrue(TEXT("The explicit Move button also retains the visible information card"), Panel->IsVisible());
    TestEqual(TEXT("Explicit movement retains its inspected facility"),
        Info->GetSelectedFacilityId(), First->GetFacilityInstanceId());
    TestTrue(TEXT("Explicit move has a valid initial placement target"), Build->bHasPlacementTarget);
    TestTrue(TEXT("The initial move keeps the source position and arbitrary rotation"),
        Build->CurrentPlacement == FirstPlacement);
    if (TestNotNull(TEXT("Explicit move creates a placement preview"), Build->PlacementPreview.Get()))
    {
        TestTrue(TEXT("The initial preview command retains the source placement"),
            Build->PlacementPreview->PreviewCommand.Placement == FirstPlacement);
        TestEqual(TEXT("The initial preview validates a move instead of a new purchase"),
            Build->PlacementPreview->PreviewCommand.Type, EFFZSCommandType::Move);
    }
    TestEqual(TEXT("The Move action only previews and does not commit or charge the player"),
        Zones->GetWorldRevision(), RevisionBeforeInspection);
    TestEqual(TEXT("The Move action preserves the festival balance"),
        Progression->GetGold(), GoldBeforeInspection);
    TestTrue(TEXT("The source remains authoritative throughout the move preview"),
        First->GetActorTransform().Equals(FirstTransform));
    // Even a moving idle cursor must keep the explicitly armed preview anchored.
    InputViewport.SetMouse(310, 300);
    Build->UpdatePlacementFromCursor(true);
    TestTrue(TEXT("Stationary cursor refresh does not jump to the clicked mesh center"),
        Build->CurrentPlacement == FirstPlacement);
    TestTrue(TEXT("Stationary cursor refresh retains a usable original target"), Build->bHasPlacementTarget);
    InputViewport.SetMouse(256, 256);
    PressEnter();
    TestEqual(TEXT("Enter cannot confirm an explicitly armed move"), Zones->GetWorldRevision(), RevisionBeforeInspection);
    CancelMove();
    TestTrue(TEXT("Cancelling an explicit move preserves the source placement"),
        First->GetGridPlacement() == FirstPlacement && First->GetActorTransform().Equals(FirstTransform));
    TestEqual(TEXT("Cancelling a move does not issue an authoritative command"),
        Zones->GetWorldRevision(), RevisionBeforeInspection);

    // Explicit inspection remains available to staff management and other UI shortcuts.
    TestTrue(TEXT("Explicit inspection opens the existing facility card"), Build->InspectFacility(First));
    TestEqual(TEXT("Explicit inspection does not begin an automatic move"),
        Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("Existing info subsystem selects the installed instance"), Info->GetSelectedFacilityId(), First->GetFacilityInstanceId());
    TestEqual(TEXT("Inspector uses build context"), Info->GetSelectionContext(), EFF_FacilityInfoContext::Build);
    if (TestNotNull(TEXT("The existing facility info WBP is the only inspector"), Panel))
    {
        TestTrue(TEXT("Existing panel is attached to its real viewport"), Panel->IsInViewport());
        TestTrue(TEXT("Existing panel is open"), Panel->GetVisibility() != ESlateVisibility::Collapsed &&
            Panel->GetVisibility() != ESlateVisibility::Hidden);
        TestEqual(TEXT("Panel projects selected instance data"), Panel->GetCurrentViewData().FacilityInstanceId,
            First->GetFacilityInstanceId());
    }
    TestTrue(TEXT("Repeated explicit inspection keeps the same facility selected"), Build->InspectFacility(First));
    TestEqual(TEXT("Repeated inspection never removes the source"), Zones->FindFacility(First->GetFacilityInstanceId()), First);
    AimAt(Second->SelectionProxy->GetComponentLocation());
    QuickClick();
    TestEqual(TEXT("Clicking another facility changes only its information"),
        Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("Info selection switches to the other installed instance"), Info->GetSelectedFacilityId(), Second->GetFacilityInstanceId());
    if (Panel)
    {
        TestEqual(TEXT("The same panel now projects the second instance"), Panel->GetCurrentViewData().FacilityInstanceId,
            Second->GetFacilityInstanceId());
    }

    TestFalse(TEXT("Null inspection is rejected"), Build->InspectFacility(nullptr));
    AFFZSFacilityActor* Unregistered = World->SpawnActor<AFFZSFacilityActor>();
    if (TestNotNull(TEXT("Unregistered actor fixture"), Unregistered))
    {
        TestFalse(TEXT("Unregistered facility cannot open an inspector"), Build->InspectFacility(Unregistered));
    }
    for (const EFF_BuildModeState DeniedState : {
        EFF_BuildModeState::Closed, EFF_BuildModeState::Placing, EFF_BuildModeState::Moving })
    {
        Build->State = DeniedState;
        TestFalse(TEXT("Closed/placement/moving input cannot inspect a facility"), Build->InspectFacility(First));
        TestEqual(TEXT("Rejected inspection preserves the existing selection"), Info->GetSelectedFacilityId(), Second->GetFacilityInstanceId());
    }

    Build->State = EFF_BuildModeState::CatalogOpen;
    AimAt(First->SelectionProxy->GetComponentLocation());
    QuickClick();
    TestEqual(TEXT("A world click from the catalog opens information only"),
        Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("Catalog world click selects the requested instance for information"),
        Info->GetSelectedFacilityId(), First->GetFacilityInstanceId());
    TestTrue(TEXT("Catalog world click makes the information panel visible"), Panel->IsVisible());
    TestNull(TEXT("Catalog world click leaves movement inactive"), Build->PlacementPreview.Get());
    TestEqual(TEXT("Every inspection leaves authority revision unchanged"), Zones->GetWorldRevision(), RevisionBeforeInspection);
    TestEqual(TEXT("Every inspection leaves the festival balance unchanged"), Progression->GetGold(), GoldBeforeInspection);
    TestTrue(TEXT("Inspection never moves the source actor"), First->GetActorTransform().Equals(FirstTransform));
    TestEqual(TEXT("Inspection never changes source placement origin"), First->GetGridPlacement().OriginCell, FirstPlacement.OriginCell);
    TestEqual(TEXT("Inspection never changes source placement yaw"), First->GetGridPlacement().YawCentidegrees, FirstPlacement.YawCentidegrees);
    AimAt(FVector(-100000.0, -100000.0, 0.0));
    QuickClick();
    TestFalse(TEXT("Clicking empty world clears the selected facility card"), Info->GetSelectedFacilityId().IsValid());
    TestEqual(TEXT("Empty world click does not submit a facility command"), Zones->GetWorldRevision(), RevisionBeforeInspection);

    Build->ExitBuildMode();
    TestTrue(TEXT("The normal preparation screen allows facility interaction"), Stage->IsReconfigureAllowed());
    QuickClick();
    TestEqual(TEXT("Empty terrain in the normal preparation screen does not open build tools"),
        Build->GetBuildModeState(), EFF_BuildModeState::Closed);
    AimAt(First->SelectionProxy->GetComponentLocation());
    AFF_FestivalPlayerController* FestivalController = CastChecked<AFF_FestivalPlayerController>(Controller);
    FestivalController->SetModalUiActive(true);
    QuickClick();
    TestEqual(TEXT("An active modal prevents opening build tools through an installed facility"),
        Build->GetBuildModeState(), EFF_BuildModeState::Closed);
    TestFalse(TEXT("An active modal prevents selecting the installed facility behind it"),
        Info->GetSelectedFacilityId().IsValid());
    FestivalController->SetModalUiActive(false);
    QuickClick();
    TestEqual(TEXT("Clicking a facility from normal preparation opens information only"),
        Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("The first preparation-screen click selects that facility for information"),
        Info->GetSelectedFacilityId(), First->GetFacilityInstanceId());
    TestTrue(TEXT("Normal preparation click makes the information panel visible"), Panel->IsVisible());
    TestNull(TEXT("Normal preparation click leaves movement inactive"), Build->PlacementPreview.Get());
    Build->ExitBuildMode();
    PressPointer();
    TestTrue(TEXT("The initial preparation press arms dragging after entering build mode"), Build->bFacilityDragPending);
    HoldPointerAt(284, 270);
    TestTrue(TEXT("The same preparation press crosses the drag threshold"), Build->bFacilityDragActive);
    CancelMove();
    ReleasePointer();
    InputViewport.SetMouse(256, 256);
    TestEqual(TEXT("Cancelling the initial preparation gesture never commits"), Zones->GetWorldRevision(), RevisionBeforeInspection);
    Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::M, IE_Pressed, 1.0f));
    Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::M, IE_Released, 0.0f));
    Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
    Build->HandleInput(*Controller, InputDeltaSeconds);
    Controller->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
    TestEqual(TEXT("M starts moving the facility currently inspected"),
        Build->GetBuildModeState(), EFF_BuildModeState::Moving);
    TestEqual(TEXT("The explicit M shortcut targets the inspected facility"),
        Build->MovingFacility.Get(), First);
    TestTrue(TEXT("The explicit M shortcut begins at the existing position and angle"),
        Build->CurrentPlacement == FirstPlacement);
    CancelMove();

    QuickClick();
    Move->OnClicked.Broadcast();
    TestEqual(TEXT("The close regression begins with a visible-card move session"),
        Build->GetBuildModeState(), EFF_BuildModeState::Moving);
    UButton* Close = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Close")));
    if (!TestNotNull(TEXT("The information card retains its Close action"), Close)) return false;
    Close->OnClicked.Broadcast();
    Info->Tick(1.0f);
    TestEqual(TEXT("Closing the information card ends its move session"),
        Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestNull(TEXT("Closing the information card removes the movement ghost"), Build->PlacementPreview.Get());
    TestFalse(TEXT("Closing the information card clears the movement source"), Build->MovingFacility.IsValid());
    TestFalse(TEXT("Closing the information card clears inspector action selection"), Build->PendingRemovalFacility.IsValid());
    TestFalse(TEXT("Closing the information card clears its information selection"), Info->GetSelectedFacilityId().IsValid());
    TestFalse(TEXT("The closed information card is hidden"), Panel->IsVisible());
    TestTrue(TEXT("Closing information never changes the original facility pose"),
        First->GetGridPlacement() == FirstPlacement && First->GetActorTransform().Equals(FirstTransform));
    TestEqual(TEXT("Closing information never submits a movement command"),
        Zones->GetWorldRevision(), RevisionBeforeInspection);

    // Pause uses a lower CommonUI layer than the viewport toolbar. Exercise the
    // real presenter so a settings/economy refresh cannot reveal it above Options.
    UFF_BuildToolsWidget* Tools = Build->BuildToolsWidget;
    if (TestNotNull(TEXT("Build entry creates the production toolbar"), Tools))
    {
        for (const EFF_BuildModeState PausedState : { EFF_BuildModeState::CatalogOpen,
            EFF_BuildModeState::BuildMode, EFF_BuildModeState::Placing, EFF_BuildModeState::Moving })
        {
            Build->State = PausedState;
            Tools->RefreshTools();
            UWidget* Help = Tools->GetWidgetFromName(TEXT("Border_PlacementHelp"));
            const ESlateVisibility HelpBeforePause = Help ? Help->GetVisibility() : ESlateVisibility::Collapsed;
            TestTrue(TEXT("Active build toolbar is visible before Pause"), Tools->IsVisible());
            TestTrue(TEXT("Pause suspends the active build state"), Build->SuspendInputForPauseMenu());
            TestEqual(TEXT("Pause immediately hides the complete build toolbar"),
                Tools->GetVisibility(), ESlateVisibility::Collapsed);
            Build->OnBuildToolsChanged.Broadcast();
            TestEqual(TEXT("Refresh while Options is open keeps the toolbar hidden"),
                Tools->GetVisibility(), ESlateVisibility::Collapsed);
            TestEqual(TEXT("Pause preserves catalog/placement state"), Build->GetBuildModeState(), PausedState);
            Build->ResumeInputAfterPauseMenu();
            TestEqual(TEXT("Toolbar waits for Pause removal before reappearing"),
                Tools->GetVisibility(), ESlateVisibility::Collapsed);
            Build->SuspendInputForPauseMenu();
            TestFalse(TEXT("Reopening Pause cancels the pending resume"), Build->bPauseMenuResumePending);
            Build->RestoreInputAfterPauseMenu();
            TestTrue(TEXT("Resume restores the same toolbar"), Build->BuildToolsWidget == Tools && Tools->IsVisible());
            if (Help) TestEqual(TEXT("Resume preserves the expanded help state"), Help->GetVisibility(), HelpBeforePause);
        }
        Build->State = EFF_BuildModeState::BuildMode;
        Tools->RefreshTools();
    }

    Build->ExitBuildMode();
    Stage->CurrentStage = EFF_FestivalStage::Live;
    QuickClick();
    TestEqual(TEXT("Build input never steals a live-stage facility click"),
        Build->GetBuildModeState(), EFF_BuildModeState::Closed);
    TestFalse(TEXT("Build input cannot open a build-context card during the live stage"),
        Info->GetSelectedFacilityId().IsValid());
    TestEqual(TEXT("Normal preparation and live input leave authority revision unchanged"),
        Zones->GetWorldRevision(), RevisionBeforeInspection);
    TestEqual(TEXT("Normal preparation and live input leave the festival balance unchanged"),
        Progression->GetGold(), GoldBeforeInspection);

    Stage->CurrentStage = EFF_FestivalStage::Setup;

    // Reuse the real viewport/controller fixture to exercise the saved check WBP's
    // click binding, tab handoff, installed-facility selection and staff controls together.
    UFF_RequiredFacilitiesSubsystem* Required = World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>();
    UFF_StaffFacilitySubsystem* Staffing = World->GetSubsystem<UFF_StaffFacilitySubsystem>();
    if (!TestNotNull(TEXT("Shortcut requirement coordinator"), Required) ||
        !TestNotNull(TEXT("Shortcut staffing coordinator"), Staffing)) { return false; }
    Required->OnWorldBeginPlay(*World);
    UFFZSFacilityDefinition* StaffDefinition = NewObject<UFFZSFacilityDefinition>(Festival);
    StaffDefinition->Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::Info);
    StaffDefinition->Spec.FacilityDefinitionId = TEXT("Info_GeneralInfo_CheckShortcutAutomation");
    StaffDefinition->Spec.InstallCostKey = TEXT("Cost.Info.GeneralInfo.CheckShortcutAutomation");
    Costs->AddRow(StaffDefinition->Spec.InstallCostKey, CostRow);
    StaffDefinition->Spec.StaffCapacity = 2;
    FFF_RequiredFacility& Requirement = Festival->RequiredSetupFacilities.AddDefaulted_GetRef();
    Requirement.FacilityDefinition = StaffDefinition;
    Requirement.MinimumCount = 1;
    Requirement.MinimumAssignedStaff = 1;
    FFFZSGridPlacement StaffPlacement;
    StaffPlacement.OriginCell = FIntPoint(1000, 4000);
    const FFFZSCommand StaffInstall = UFFZSGridLibrary::MakeInstallCommand(
        StaffDefinition->Spec, ZoneId, StaffPlacement, Zones->GetWorldRevision());
    const FFFZSCommandResult StaffInstalled = Zones->ExecuteCommand(StaffInstall);
    if (!TestTrue(*FString::Printf(TEXT("Required staff facility installs: %s"),
        *StaffInstalled.Message.ToString()), StaffInstalled.bSucceeded)) { return false; }
    AFFZSFacilityActor* StaffFacility = Zones->FindFacility(StaffInstall.FacilityInstanceId);
    if (!TestNotNull(TEXT("Installed staff shortcut target"), StaffFacility)) { return false; }
    TestEqual(TEXT("Unassigned requirement resolves the installed target"),
        Required->FindStaffAssignmentTarget(), StaffFacility);

    UClass* BarClass = LoadClass<UFF_PrepHudBar>(nullptr, FFBuildUIAssetPaths::PrepHudBarClass);
    if (!TestNotNull(TEXT("Saved preparation HUD class"), BarClass)) { return false; }
    UFF_PrepHudBar* Bar = CreateWidget<UFF_PrepHudBar>(Controller, BarClass);
    if (!TestNotNull(TEXT("Real preparation HUD"), Bar)) { return false; }
    Bar->AddToViewport();
    ON_SCOPE_EXIT { Bar->CloseCurrentPanel(); Bar->RemoveFromParent(); };
    // SelectTab loads the production check WBP and calls InitializePanel with this bar.
    Bar->SelectTab(EFF_PrepHudTab::Check);
    TArray<UUserWidget*> CheckPanels;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, CheckPanels, UFF_PrepPanelCheck::StaticClass(), false);
    UFF_PrepPanelCheck* Check = nullptr;
    for (UUserWidget* Widget : CheckPanels)
    {
        UFF_PrepPanelCheck* Candidate = Cast<UFF_PrepPanelCheck>(Widget);
        if (Candidate && Candidate->GetOwningBar() == Bar) { Check = Candidate; break; }
    }
    if (!TestNotNull(TEXT("HUD initializes the saved check panel"), Check)) { return false; }
    UButton* Guidance = Cast<UButton>(Check->GetWidgetFromName(TEXT("Button_OpenGateReason")));
    auto* Reason = Cast<UTextBlock>(Check->GetWidgetFromName(TEXT("Text_OpenGateReason")));
    auto* ReasonRow = Cast<UHorizontalBox>(Check->GetWidgetFromName(TEXT("HorizontalBox_OpenGateReason")));
    if (!TestNotNull(TEXT("Saved warning has an explicit staff assignment action"), Guidance) ||
        !TestNotNull(TEXT("Warning keeps its own text"), Reason) ||
        !TestNotNull(TEXT("Warning and action share one horizontal row"), ReasonRow)) { return false; }
    UWidget* Attention = Check->GetWidgetFromName(TEXT("Border_RequiredAttention"));
    if (Attention)
    {
        UWidget* Copy = Check->GetWidgetFromName(TEXT("VerticalBox_RequiredAttentionCopy"));
        auto* Content = Cast<UHorizontalBox>(Check->GetWidgetFromName(TEXT("HorizontalBox_RequiredAttentionContent")));
        if (!TestNotNull(TEXT("Saved attention strip has its title and reason column"), Copy) ||
            !TestNotNull(TEXT("Saved attention strip has its horizontal content row"), Content)) { return false; }
        TestTrue(TEXT("The warning reason remains in the attention strip's copy column"),
            Reason->GetParent() == ReasonRow && ReasonRow->GetParent() == Copy);
        TestTrue(TEXT("The action is immediately right of the title and warning column"),
            Content->GetChildAt(0) == Copy && Content->GetChildAt(1) == Guidance);
    }
    else
    {
        TestTrue(TEXT("The action is placed immediately right of the warning text"),
            ReasonRow->GetChildAt(0) == Reason && ReasonRow->GetChildAt(1) == Guidance);
    }
    TestTrue(TEXT("The warning flexes while the action retains its width"),
        CastChecked<UHorizontalBoxSlot>(Reason->Slot)->GetSize().SizeRule == ESlateSizeRule::Fill);
    TestTrue(TEXT("The action occupies only its required width"),
        CastChecked<UHorizontalBoxSlot>(Guidance->Slot)->GetSize().SizeRule == ESlateSizeRule::Automatic);
    TestFalse(TEXT("No link text is appended under the warning"), Reason->GetText().ToString().Contains(TEXT("스태프 배정 위치 보기")));
    TestTrue(TEXT("Unassigned required staff enables the warning shortcut"), Guidance->GetIsEnabled());
    TestEqual(TEXT("Unassigned required staff shows the right-hand shortcut"), Guidance->GetVisibility(), ESlateVisibility::Visible);
    const int64 RevisionBeforeShortcut = Zones->GetWorldRevision();
    const int32 GoldBeforeShortcut = Progression->GetGold();
    Guidance->OnClicked.Broadcast();
    if (Check->UsesCompactLayout())
    {
        TestTrue(TEXT("Compact warning opens the staff tab when only staff assignment is missing"),
            Bar->GetSelection().IsSelected(EFF_PrepHudTab::Staff));
        TestEqual(TEXT("Compact staff guidance does not start a facility move"),
            Build->GetBuildModeState(), EFF_BuildModeState::Closed);
        TestEqual(TEXT("Compact staff guidance leaves facility authority unchanged"),
            Zones->GetWorldRevision(), RevisionBeforeShortcut);
        TestEqual(TEXT("Compact staff guidance does not charge or hire staff"),
            Progression->GetGold(), GoldBeforeShortcut);
        // Explicit facility actions still use the inspector, independently of the
        // compact HUD's staff-tab destination and ordinary world inspection.
        Bar->SelectTab(EFF_PrepHudTab::Build);
        TestTrue(TEXT("An explicit action inspects the installed staff facility"), Build->InspectFacility(StaffFacility));
        TestTrue(TEXT("An explicit action opens the selected facility's staff page"), Info->ShowSelectedStaffPage());
    }
    TestTrue(TEXT("Facility inspection opens the build tab synchronously"), Bar->GetSelection().IsSelected(EFF_PrepHudTab::Build));
    TestEqual(TEXT("Warning click enters facility inspection"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("Warning click selects the required installed Info facility"),
        Info->GetSelectedFacilityId(), StaffInstall.FacilityInstanceId);
    if (!TestNotNull(TEXT("The existing inspector is reused for the shortcut"), Panel)) { return false; }
    TestEqual(TEXT("Warning click opens the staff page directly"), Panel->GetInfoPage(), EFF_FacilityInfoPage::Staff);
    UButton* Assign = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_AssignStaff")));
    UWidget* StaffSection = Panel->GetWidgetFromName(TEXT("Panel_StaffSection"));
    if (!TestNotNull(TEXT("Assignment button remains bound in the staff page"), Assign) ||
        !TestNotNull(TEXT("Staff assignment section"), StaffSection)) { return false; }
    TestTrue(TEXT("Staff section is visible after the warning click"), StaffSection->IsVisible());
    TestTrue(TEXT("The shortcut exposes an enabled assignment action"), Assign->GetIsEnabled());
    TestEqual(TEXT("Following the warning does not mutate facility authority"), Zones->GetWorldRevision(), RevisionBeforeShortcut);
    TestEqual(TEXT("Following the warning does not charge or hire staff"), Progression->GetGold(), GoldBeforeShortcut);
    Assign->OnClicked.Broadcast();
    TestEqual(TEXT("The exposed assignment button assigns one waiting staff member"),
        Staffing->GetAssignedStaffCount(StaffInstall.FacilityInstanceId), 1);
    Bar->SelectTab(EFF_PrepHudTab::Check);
    TestTrue(TEXT("Player can return from assignment to the check panel"), Bar->GetSelection().IsSelected(EFF_PrepHudTab::Check));
    TestFalse(TEXT("Satisfied staffing disables the warning shortcut on reopen"), Guidance->GetIsEnabled());
    TestEqual(TEXT("Satisfied staffing hides the completed assignment shortcut"), Guidance->GetVisibility(), ESlateVisibility::Collapsed);
    if (Check->UsesCompactLayout() && Attention)
    {
        auto* Title = Cast<UTextBlock>(Check->GetWidgetFromName(TEXT("Text_RequiredAttentionTitle")));
        if (!TestNotNull(TEXT("The completed compact attention strip retains its title"), Title)) { return false; }
        TestEqual(TEXT("Satisfied compact preparation is ready"), Bar->GetCtaState(), EFF_PrepCtaState::Ready);
        TestEqual(TEXT("The completed compact attention strip remains visible"),
            Attention->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
        TestTrue(TEXT("The compact completion title remains visible"), Title->IsVisible());
        TestEqual(TEXT("The compact strip shows completion once in its title"),
            Title->GetText().ToString(), FString(TEXT("✓ 필수 준비 완료")));
        TestEqual(TEXT("The compact completion title replaces the duplicated reason"),
            Reason->GetVisibility(), ESlateVisibility::Collapsed);
    }
    else
    {
        TestTrue(TEXT("The reason is separate from the hidden shortcut"), Reason->IsVisible());
    }

    // Isolate the operational-lock case from the preparation HUD fixture above.
    Bar->CloseCurrentPanel();
    Bar->RemoveFromParent();
    Build->ExitBuildMode();
    Stage->CurrentStage = EFF_FestivalStage::Live;
    if (!TestTrue(TEXT("Operational-lock fixture enters its lifecycle phase"),
        Zones->SetChangePhase(EFFZSChangePhase::LiveOperation))) { return false; }
    FFFZSCommand OpenCommand;
    OpenCommand.CommandId = FGuid::NewGuid();
    OpenCommand.TransactionId = FGuid::NewGuid();
    OpenCommand.ExpectedWorldRevision = Zones->GetWorldRevision();
    OpenCommand.Type = EFFZSCommandType::Open;
    OpenCommand.FacilityInstanceId = First->GetFacilityInstanceId();
    const FFFZSCommandResult OpenResult = Zones->ExecuteCommand(OpenCommand);
    if (!TestTrue(*FString::Printf(TEXT("Operational-lock fixture opens: %s"),
        *OpenResult.Message.ToString()), OpenResult.bSucceeded)) { return false; }
    TestFalse(TEXT("An active facility cannot move"), First->CanMoveOrRemove());
    Stage->CurrentStage = EFF_FestivalStage::Setup;
    if (!TestTrue(TEXT("Operational-lock fixture returns to preparation authority"),
        Zones->SetChangePhase(EFFZSChangePhase::InitialSetup))) { return false; }
    const int64 RevisionBeforeLockedClick = Zones->GetWorldRevision();
    const int32 GoldBeforeLockedClick = Progression->GetGold();
    Build->EnterBuildMode();
    AimAt(First->SelectionProxy->GetComponentLocation());
    QuickClick(); // Release the build-entry suppression before the delivered world press.
    QuickClick();
    TestEqual(TEXT("An active facility click opens inspection without starting a move"),
        Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("An active facility remains available for information and operations"),
        Info->GetSelectedFacilityId(), First->GetFacilityInstanceId());
    TestEqual(TEXT("The operational lock never submits a move command"),
        Zones->GetWorldRevision(), RevisionBeforeLockedClick);
    TestEqual(TEXT("The operational lock does not charge the player"),
        Progression->GetGold(), GoldBeforeLockedClick);
    TestTrue(TEXT("The operational lock keeps the original actor transform"),
        First->GetActorTransform().Equals(FirstTransform));
    Build->ExitBuildMode();

    // A real held gesture commits once on release, and keeps the same inspector useful.
    Build->EnterBuildMode();
    AimAt(Second->SelectionProxy->GetComponentLocation());
    InputViewport.SetMouse(256, 256);
    QuickClick();
    QuickClick();
    const FGuid MovingId = Second->GetFacilityInstanceId();
    const FFFZSGridPlacement SecondOrigin = Second->GetGridPlacement();
    const int64 RevisionBeforeCommit = Zones->GetWorldRevision();
    const int32 GoldBeforeCommit = Progression->GetGold();
    PressPointer();
    HoldPointerAt(304, 256);
    if (!TestEqual(TEXT("The second fixture starts a held drag"), Build->GetBuildModeState(), EFF_BuildModeState::Moving)) return false;
    TestEqual(TEXT("Holding the drag does not execute authority"), Zones->GetWorldRevision(), RevisionBeforeCommit);
    TestEqual(TEXT("Dragging retains the original angle"), Build->CurrentPlacement.YawCentidegrees, SecondOrigin.YawCentidegrees);
    const FFFZSGridPlacement ConfirmedPlacement = Build->CurrentPlacement;
    TestFalse(TEXT("The release target differs from the original"), ConfirmedPlacement == SecondOrigin);
    ReleasePointer();
    Second = Zones->FindFacility(MovingId);
    if (!TestNotNull(TEXT("The moved facility remains registered"), Second)) return false;
    Info->Tick(1.0f);
    TestEqual(TEXT("Successful movement returns to facility inspection"),
        Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("Successful movement executes exactly one authority command"),
        Zones->GetWorldRevision(), RevisionBeforeCommit + 1);
    TestEqual(TEXT("Moving an installed facility does not purchase it again"),
        Progression->GetGold(), GoldBeforeCommit);
    TestTrue(TEXT("Authority receives the chosen movement pose"), Second->GetGridPlacement() == ConfirmedPlacement);
    TestEqual(TEXT("Successful movement preserves the selected facility information"),
        Info->GetSelectedFacilityId(), MovingId);
    TestTrue(TEXT("The information card remains visible after successful movement"), Panel->IsVisible());
    TestEqual(TEXT("The information card still projects the moved instance"),
        Panel->GetCurrentViewData().FacilityInstanceId, MovingId);
    TestEqual(TEXT("The moved facility is ready for further inspector actions"),
        Build->PendingRemovalFacility.Get(), Second);
    PressEnter();
    if (!TestEqual(TEXT("Enter after successful movement cannot implicitly demolish its facility"),
        Zones->FindFacility(MovingId), Second)) return false;
    TestEqual(TEXT("Enter after successful movement leaves authority unchanged"),
        Zones->GetWorldRevision(), RevisionBeforeCommit + 1);
    TestEqual(TEXT("Enter after successful movement leaves the festival balance unchanged"),
        Progression->GetGold(), GoldBeforeCommit);
    Move->OnClicked.Broadcast();
    TestEqual(TEXT("Move remains available immediately after successful movement"),
        Build->GetBuildModeState(), EFF_BuildModeState::Moving);
    TestTrue(TEXT("A subsequent move begins at the just-confirmed pose"),
        Build->CurrentPlacement == ConfirmedPlacement);
    Panel->SetInfoPage(EFF_FacilityInfoPage::Overview);
    UEditableTextBox* QueueTeams = Cast<UEditableTextBox>(Panel->GetWidgetFromName(TEXT("Editable_FacilityQueueTeams")));
    UButton* ApplyQueue = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_ApplyFacilityQueueSettings")));
    if (!TestNotNull(TEXT("The retained information card exposes queue capacity"), QueueTeams) ||
        !TestNotNull(TEXT("The retained information card exposes queue Apply"), ApplyQueue)) return false;
    QueueTeams->SetText(FText::FromString(TEXT("5")));
    ApplyQueue->OnClicked.Broadcast();
    Second = Zones->FindFacility(MovingId);
    if (!TestNotNull(TEXT("Queue editing retains the installed facility"), Second)) return false;
    Info->Tick(1.0f);
    TestEqual(TEXT("Queue Apply ends the move preview whose queue snapshot is now stale"),
        Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestNull(TEXT("Queue Apply removes the stale movement ghost"), Build->PlacementPreview.Get());
    TestEqual(TEXT("Queue Apply updates the authoritative team capacity"),
        Second->GetBaseFacilitySpec().QueueSettings.MaxWaitingTeams, 5);
    TestTrue(TEXT("Queue Apply does not move the installed facility"),
        Second->GetGridPlacement() == ConfirmedPlacement);
    TestEqual(TEXT("Queue Apply retains the information selection"), Info->GetSelectedFacilityId(), MovingId);
    TestTrue(TEXT("Queue Apply leaves the information card visible"), Panel->IsVisible());
    TestEqual(TEXT("Queue Apply refreshes the displayed capacity"), QueueTeams->GetText().ToString(), FString(TEXT("5")));
    TestEqual(TEXT("Queue Apply executes one authority command without committing movement"),
        Zones->GetWorldRevision(), RevisionBeforeCommit + 2);
    Move->OnClicked.Broadcast();
    TestEqual(TEXT("Movement remains available after applying queue settings"),
        Build->GetBuildModeState(), EFF_BuildModeState::Moving);
    TestEqual(TEXT("The new movement preview takes the updated queue snapshot"),
        Build->PendingQueueSettings.MaxWaitingTeams, 5);
    UButton* Remove = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_Remove")));
    UButton* ConfirmRemoval = Cast<UButton>(Panel->GetWidgetFromName(TEXT("Button_RemoveConfirm")));
    if (!TestNotNull(TEXT("The retained information card has its Remove action"), Remove) ||
        !TestNotNull(TEXT("Removal retains the existing explicit confirmation"), ConfirmRemoval)) return false;
    Remove->OnClicked.Broadcast();
    TestEqual(TEXT("Choosing removal while moving first shows confirmation"),
        Panel->GetInfoPage(), EFF_FacilityInfoPage::RemoveConfirmation);
    TestNotNull(TEXT("Opening the removal confirmation cannot remove authority"), Zones->FindFacility(MovingId));
    ConfirmRemoval->OnClicked.Broadcast();
    TestEqual(TEXT("Confirmed removal ends the pending move session"),
        Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestNull(TEXT("Confirmed removal destroys the movement ghost"), Build->PlacementPreview.Get());
    TestNull(TEXT("Confirmed removal removes the selected installed facility"), Zones->FindFacility(MovingId));
    TestFalse(TEXT("Confirmed removal clears information selection"), Info->GetSelectedFacilityId().IsValid());
    TestFalse(TEXT("Confirmed removal clears inspector action selection"), Build->PendingRemovalFacility.IsValid());
    TestEqual(TEXT("Confirmed removal executes exactly one further authority command"),
        Zones->GetWorldRevision(), RevisionBeforeCommit + 3);
    Build->ExitBuildMode();

    Info->ClearSelection();
    Build->State = EFF_BuildModeState::Closed;
    Build->bBuildSessionActive = false;
    return true;
}

#endif
