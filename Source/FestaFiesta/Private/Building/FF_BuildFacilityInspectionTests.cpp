#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "Widgets/SOverlay.h"

namespace
{
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
    FString PlayerError;
    ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(*FString::Printf(TEXT("Real local player: %s"), *PlayerError), Player) ||
        !TestNotNull(TEXT("Local controller"), Controller))
    {
        return false;
    }
    Controller->SetPlayer(Player);
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
    if (!TestNotNull(TEXT("Festival progression"), Progression))
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
    const FTransform FirstTransform = First->GetActorTransform();
    const FFFZSGridPlacement FirstPlacement = First->GetGridPlacement();
    // The friend seam seeds only the starting input state. Selection, WBP creation, policy
    // routing and registry checks all run through production public methods below.
    Build->State = EFF_BuildModeState::BuildMode;
    Build->bBuildSessionActive = true;
    if (!TestTrue(TEXT("Inspecting installed facility succeeds"), Build->InspectFacility(First)))
    {
        return false;
    }
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
    TestTrue(TEXT("Repeated inspection keeps the same facility selected"), Build->InspectFacility(First));
    TestEqual(TEXT("Repeated inspection never removes the source"), Zones->FindFacility(First->GetFacilityInstanceId()), First);
    TestTrue(TEXT("Selecting another installed facility succeeds"), Build->InspectFacility(Second));
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
    TestTrue(TEXT("Catalog inspection transitions through the existing build workspace"), Build->InspectFacility(First));
    TestEqual(TEXT("Catalog inspection ends in BuildMode"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
    TestEqual(TEXT("Catalog inspection opens requested existing instance"), Info->GetSelectedFacilityId(), First->GetFacilityInstanceId());
    TestEqual(TEXT("Every inspection leaves authority revision unchanged"), Zones->GetWorldRevision(), RevisionBeforeInspection);
    TestTrue(TEXT("Inspection never moves the source actor"), First->GetActorTransform().Equals(FirstTransform));
    TestEqual(TEXT("Inspection never changes source placement origin"), First->GetGridPlacement().OriginCell, FirstPlacement.OriginCell);
    TestEqual(TEXT("Inspection never changes source placement yaw"), First->GetGridPlacement().YawCentidegrees, FirstPlacement.YawCentidegrees);
    Info->ClearSelection();
    Build->State = EFF_BuildModeState::Closed;
    Build->bBuildSessionActive = false;
    return true;
}

#endif
