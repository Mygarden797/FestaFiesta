#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Building/FF_BuildModeSubsystem.h"
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
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Components/BoxComponent.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
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
    Festival->DailyVenueRentGold = 0;
    Progression->StartFestival(Festival);

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("Fixture zone"), Zone) ||
        !TestTrue(TEXT("Zone preset"), Zone->ApplyAcceptedZonePreset(ZoneId)) ||
        !TestTrue(TEXT("Zone registers"), Zones->RegisterZone(Zone)))
    {
        return false;
    }
    const auto InstallFixture = [&](const int32 X) -> AFFZSFacilityActor*
    {
        FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
        Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_Fixture");
        Spec.InstallCostKey = NAME_None;
        FFFZSGridPlacement Placement;
        Placement.OriginCell = FIntPoint(X, 1000);
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
    TestEqual(TEXT("Existing info subsystem selects the installed instance"), Info->GetSelectedFacilityId(), First->GetFacilityInstanceId());
    TestEqual(TEXT("Inspector uses build context"), Info->GetSelectionContext(), EFF_FacilityInfoContext::Build);
    TArray<UUserWidget*> Panels;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Panels, UFF_FacilityInfoPanel::StaticClass(), false);
    UFF_FacilityInfoPanel* Panel = Panels.Num() == 1 ? Cast<UFF_FacilityInfoPanel>(Panels[0]) : nullptr;
    if (TestNotNull(TEXT("The existing facility info WBP is the only inspector"), Panel))
    {
        TestTrue(TEXT("Existing panel is attached to its real viewport"), Panel->IsInViewport());
        TestTrue(TEXT("Existing panel is open"), Panel->GetVisibility() != ESlateVisibility::Collapsed &&
            Panel->GetVisibility() != ESlateVisibility::Hidden);
        TestEqual(TEXT("Panel projects selected instance data"), Panel->GetCurrentViewData().FacilityInstanceId,
            First->GetFacilityInstanceId());
    }
    QuickClick();
    TestEqual(TEXT("Repeated raw clicks keep the same facility selected"), Info->GetSelectedFacilityId(), First->GetFacilityInstanceId());
    TestEqual(TEXT("Repeated inspection never removes the source"), Zones->FindFacility(First->GetFacilityInstanceId()), First);
    AimAt(Second->SelectionProxy->GetComponentLocation());
    QuickClick();
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
    TestEqual(TEXT("Catalog inspection ends in BuildMode"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("Catalog inspection opens requested existing instance"), Info->GetSelectedFacilityId(), First->GetFacilityInstanceId());
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
    TestEqual(TEXT("Clicking an installed facility from the normal preparation screen enters inspection"),
        Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("The first click from the normal preparation screen opens that facility card"),
        Info->GetSelectedFacilityId(), First->GetFacilityInstanceId());

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
    Info->ClearSelection();
    Build->State = EFF_BuildModeState::Closed;
    Build->bBuildSessionActive = false;
    return true;
}

#endif
