// Exercises drag input through the real installation, occupancy and economy boundaries.
// Only cursor sampling and initial selection are seeded; no install result is mocked.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetBundleData.h"
#include "Building/FF_BuildCatalogModel.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/FF_ZoneSystemEconomyBridgeSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"

namespace FFBuildDragIntegrationTest
{
    // Restore process-wide settings even if fixture creation or an assertion fails.
    class FScopedSettings
    {
    public:
        FScopedSettings()
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

        ~FScopedSettings()
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

    FFFZSFacilitySpec MakeRoadSpec(const FName Id, const FIntPoint Size)
    {
        FFFZSFacilitySpec Spec;
        Spec.FacilityDefinitionId = Id;
        Spec.DisplayName = FText::FromString(Id.ToString());
        Spec.Category = EFFZSFacilityCategory::Road;
        Spec.FootprintSizeCells = Size;
        Spec.ClearanceCells = 0;
        Spec.QueueRules.MaxWaitingAgents = 0;
        Spec.QueueRules.ServiceCapacity = 0;
        Spec.PlacementObstacleHeightCm = 8.0f;
        Spec.InstallCostKey = TEXT("Cost.Road.Default");
        return Spec;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildDragPlacementTest,
    "FestaFiesta.Building.BuildMode.DragPlacement.AuthorityAndLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildDragPlacementTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace FFBuildDragIntegrationTest;
    FScopedSettings Settings;
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game))
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }

    UWorld* World = Fixture.GetTestWorld();
    UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
    UFF_BuildModeSubsystem* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    UFF_BuildCatalogSubsystem* Catalog = Game ? Game->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
    UFF_FestivalProgressionSubsystem* Economy = Game
        ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
    UAssetManager* Assets = UAssetManager::GetIfInitialized();
    if (!TestNotNull(TEXT("Game world"), World) || !TestNotNull(TEXT("Game instance"), Game) ||
        !TestNotNull(TEXT("Real build subsystem"), Build) || !TestNotNull(TEXT("Real zone authority"), Zones) ||
        !TestNotNull(TEXT("Real catalog"), Catalog) || !TestNotNull(TEXT("Real economy"), Economy) ||
        !TestNotNull(TEXT("Asset manager"), Assets) ||
        !TestNotNull(TEXT("Install economy policy"), World->GetSubsystem<UFF_ZoneSystemEconomyBridgeSubsystem>()))
    {
        return false;
    }

    // GameInstance initialization starts production discovery immediately. Invalidate
    // its callbacks before cancellation, exactly as RefreshCatalog does, so a late
    // async completion cannot replace the isolated four-definition test catalog.
    FFBuildCatalogModel::BeginNextRequest(Catalog->RequestGeneration);
    if (Catalog->ActiveLoadHandle.IsValid() && !Catalog->ActiveLoadHandle->HasLoadCompleted())
    {
        Catalog->ActiveLoadHandle->CancelHandle();
    }
    Catalog->ActiveLoadHandle.Reset();
    Catalog->RequestedAssetIds.Reset();

    ON_SCOPE_EXIT
    {
        Build->EndDragPlacement();
        Build->ClearPauseMenuInputSuspension();
        Build->DestroyPlacementPreview();
        Build->DismissPlacementFailureFeedback(true);
        Build->State = EFF_BuildModeState::Closed;
    };

    // Exercise the native presenter in a headless authority fixture. Designer WBP
    // creation and player-screen attachment have their own presentation tests.
    Build->PlacementFailureFeedback = CreateWidget<UFF_BuildPlacementFailureFeedback>(
        World, UFF_BuildPlacementFailureFeedback::StaticClass());
    if (!TestNotNull(TEXT("Real failure presenter exists without a viewport"), Build->PlacementFailureFeedback.Get()))
    {
        return false;
    }

    constexpr int32 InstallCost = 100;
    TStrongObjectPtr<UFF_FestivalDefinition> Festival(NewObject<UFF_FestivalDefinition>(Game));
    TStrongObjectPtr<UDataTable> Costs(NewObject<UDataTable>(Game));
    Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
    FFF_FacilityCostRow CostRow;
    CostRow.InstallCost = InstallCost;
    Costs->AddRow(TEXT("Cost.Road.Default"), CostRow);
    Festival->FestivalId = TEXT("BuildDragPlacementAutomation");
    Festival->FacilityCostTable = Costs.Get();
    Festival->InitialGold = 100000;
    Festival->DailyVenueRentGold = 0;
    Economy->StartFestival(Festival.Get());

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("Fixture zone"), Zone) ||
        !TestTrue(TEXT("Accepted zone preset"), Zone->ApplyAcceptedZonePreset(ZoneId)) ||
        !TestTrue(TEXT("Zone registers with the authority"), Zones->RegisterZone(Zone)))
    {
        return false;
    }

    Build->SpawnPlacementPreview();
    if (!TestNotNull(TEXT("One shared preview actor for the whole placement session"), Build->PlacementPreview.Get()))
    {
        return false;
    }

    TStrongObjectPtr<UFFZSFacilityDefinition> Road(NewObject<UFFZSFacilityDefinition>());
    TStrongObjectPtr<UFFZSFacilityDefinition> Rectangle(NewObject<UFFZSFacilityDefinition>());
    TStrongObjectPtr<UFFZSFacilityDefinition> Single(NewObject<UFFZSFacilityDefinition>());
    TStrongObjectPtr<UFFZSFacilityDefinition> Limited(NewObject<UFFZSFacilityDefinition>());
    Road->Spec = MakeRoadSpec(TEXT("Road_Surface_DragTest"), FIntPoint(400, 400));
    Road->bEnableDragPlacement = true;
    Rectangle->Spec = MakeRoadSpec(TEXT("Road_Surface_RectangleTest"), FIntPoint(400, 200));
    Rectangle->bEnableDragPlacement = true;
    Single->Spec = MakeRoadSpec(TEXT("Road_Surface_SingleTest"), FIntPoint(400, 400));
    Limited->Spec = MakeRoadSpec(TEXT("Road_Surface_LimitedTest"), FIntPoint(400, 400));
    Limited->Spec.MaxInstancesPerWorld = 2;
    Limited->bEnableDragPlacement = true;

    // Use a private dynamic type so catalog validation runs without changing production assets.
    const FPrimaryAssetType TestType(*FString::Printf(TEXT("FFBuildDragTest_%s"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Assets->RemovePrimaryAssetType(TestType); };
    for (UFFZSFacilityDefinition* Definition : { Road.Get(), Rectangle.Get(), Single.Get(), Limited.Get() })
    {
        const FPrimaryAssetId AssetId(TestType, Definition->Spec.FacilityDefinitionId);
        if (!TestTrue(TEXT("Transient definition registers"), Assets->AddDynamicAsset(
            AssetId, FSoftObjectPath(Definition), FAssetBundleData())))
        {
            return false;
        }
        Catalog->RequestedAssetIds.Add(AssetId);
    }
    Catalog->RebuildCatalogFromLoadedAssets();
    if (!TestEqual(TEXT("All authored definitions pass production catalog validation"), Catalog->GetEntriesView().Num(), 4))
    {
        return false;
    }

    const auto SelectAt = [&](UFFZSFacilityDefinition* Definition, const FIntPoint Origin, const int32 Yaw = 0)
    {
        Build->EndDragPlacement();
        Build->State = EFF_BuildModeState::Placing;
        Build->SelectedFacilityDefinitionId = Definition->Spec.FacilityDefinitionId;
        Build->SelectedSpec = Definition->Spec;
        Build->CurrentZoneId = ZoneId;
        Build->CurrentPlacement = FFFZSGridPlacement();
        Build->CurrentPlacement.OriginCell = Origin;
        Build->CurrentPlacement.YawCentidegrees = Yaw;
        Build->bHasPlacementTarget = true;
    };
    const auto CursorAt = [&](const FIntPoint Origin)
    {
        const FVector2D Offset = FFBuildModeModel::ResolveRotatedFootprintCenterOffset(
            Build->SelectedSpec.FootprintSizeCells,
            UFFZSGridLibrary::YawCentidegreesToDegrees(Build->CurrentPlacement.YawCentidegrees));
        return Zone->GetActorTransform().TransformPosition(FVector(
            static_cast<double>(Origin.X) + Offset.X, static_cast<double>(Origin.Y) + Offset.Y, 0.0));
    };
    const auto Drain = [&]()
    {
        // No wall-clock assumption: every pass may do as little as one authority command.
        for (int32 Pass = 0; Pass < 64; ++Pass)
        {
            Build->ProcessDragPlacement();
        }
    };
    const auto Count = [&](const UFFZSFacilityDefinition* Definition)
    {
        return Zones->CountFacilitiesByDefinitionId(Definition->Spec.FacilityDefinitionId);
    };

    SelectAt(Single.Get(), FIntPoint(3000, 1000));
    TestFalse(TEXT("Existing definitions remain single-click by default"), Build->IsDragPlacementEnabled());
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(4200, 1000)));
    Drain();
    TestEqual(TEXT("Opted-out definition never installs through drag"), Count(Single.Get()), 0);

    // Exercise a genuine PlayerInput press edge, including a click released before
    // the next frame. The isolated driver world has no viewport: the delivered
    // click must use the existing valid preview without a Slate hit-path veto.
    // Keeping the controller outside the authority world preserves the explicitly
    // seeded cursor sample while CommitPlacement still validates and purchases.
    {
        FTestWorldWrapper InputFixture;
        if (!InputFixture.CreateTestWorld(EWorldType::EditorPreview))
        {
            InputFixture.ForwardErrorMessages(this);
            return false;
        }
        APlayerController* InputController = InputFixture.GetTestWorld()->SpawnActor<APlayerController>();
        if (!TestNotNull(TEXT("Real input controller"), InputController))
        {
            return false;
        }
        InputController->PlayerInput = NewObject<UPlayerInput>(InputController);
        const TSharedPtr<SWidget> PreviousKeyboardFocus = FSlateApplication::IsInitialized()
            ? FSlateApplication::Get().GetKeyboardFocusedWidget() : nullptr;
        if (FSlateApplication::IsInitialized())
        {
            FSlateApplication::Get().ClearKeyboardFocus();
        }
        ON_SCOPE_EXIT
        {
            if (FSlateApplication::IsInitialized())
            {
                if (PreviousKeyboardFocus.IsValid())
                {
                    FSlateApplication::Get().SetKeyboardFocus(PreviousKeyboardFocus);
                }
                else
                {
                    FSlateApplication::Get().ClearKeyboardFocus();
                }
            }
        };
        TArray<UInputComponent*> InputStack;
        constexpr float InputDeltaSeconds = 1.0f / 60.0f;
        const auto QuickClick = [&]()
        {
            InputController->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(
                EKeys::LeftMouseButton, IE_Pressed, 1.0f));
            InputController->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(
                EKeys::LeftMouseButton, IE_Released, 0.0f));
            InputController->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
            TestTrue(TEXT("Short click retains its press edge"),
                InputController->WasInputKeyJustPressed(EKeys::LeftMouseButton));
            TestFalse(TEXT("Short click is already released when the frame runs"),
                InputController->IsInputKeyDown(EKeys::LeftMouseButton));
            Build->RefreshCurrentPlacementValidation();
            TestTrue(TEXT("Click preview passes actual placement policies"), Build->LastValidation.bIsValid);
            Build->HandleInput(*InputController, InputDeltaSeconds);
            InputController->PlayerInput->ProcessInputStack(InputStack, InputDeltaSeconds, false);
        };

        const int32 GoldBeforeClick = Economy->GetGold();
        const int64 RevisionBeforeClick = Zones->GetWorldRevision();
        Build->bSuppressWorldClickUntilReleased = true;
        QuickClick();
        TestEqual(TEXT("The catalog-opening click remains suppressed"), Count(Single.Get()), 0);
        TestEqual(TEXT("A suppressed click never charges gold"), Economy->GetGold(), GoldBeforeClick);
        QuickClick();
        TestEqual(TEXT("Quick left click installs an opted-out facility"), Count(Single.Get()), 1);
        TestEqual(TEXT("Quick click commits exactly one authority command"),
            Zones->GetWorldRevision(), RevisionBeforeClick + 1);
        TestEqual(TEXT("Quick click charges exactly once"), Economy->GetGold(), GoldBeforeClick - InstallCost);
        TestFalse(TEXT("Released click never starts a drag session"), Build->DragPlacementSession.IsValid());
        Build->UpdateDragPlacement(CursorAt(FIntPoint(4200, 1000)));
        Drain();
        TestEqual(TEXT("Moving after a released click cannot buy another facility"), Count(Single.Get()), 1);
    }

    SelectAt(Single.Get(), FIntPoint(3400, 1000));
    Build->RefreshCurrentPlacementValidation();
    Build->HandlePlacementPrimaryPress(true);
    TestEqual(TEXT("Holding left click still places one opted-out facility immediately"), Count(Single.Get()), 2);
    TestFalse(TEXT("An opted-out facility never starts a held purchase session"), Build->DragPlacementSession.IsValid());
    Build->UpdateDragPlacement(CursorAt(FIntPoint(4600, 1000)));
    Drain();
    TestEqual(TEXT("Holding and moving respects the facility data-asset opt-out"), Count(Single.Get()), 2);

    SelectAt(Road.Get(), FIntPoint(1000, 1000));
    Build->State = EFF_BuildModeState::Moving;
    Build->BeginDragPlacement();
    Drain();
    TestEqual(TEXT("Moving an existing facility cannot begin a purchase stroke"), Count(Road.Get()), 0);

    SelectAt(Road.Get(), FIntPoint(1000, 1000));
    TestTrue(TEXT("Data-asset opt-in enables drag"), Build->IsDragPlacementEnabled());
    const int32 GoldBeforeLine = Economy->GetGold();
    const int64 RevisionBeforeLine = Zones->GetWorldRevision();
    Build->RefreshCurrentPlacementValidation();
    Build->HandlePlacementPrimaryPress(true);
    TestEqual(TEXT("Press installs the first tile exactly once"), Count(Road.Get()), 1);
    TestTrue(TEXT("Holding left click starts the opted-in drag session"), Build->DragPlacementSession.IsValid());
    for (int32 Frame = 0; Frame < 16; ++Frame)
    {
        Build->UpdateDragPlacement(CursorAt(FIntPoint(1000, 1000)));
        Build->ProcessDragPlacement();
    }
    TestEqual(TEXT("Holding still never repeats the initial authority commit"),
        Zones->GetWorldRevision(), RevisionBeforeLine + 1);
    TestEqual(TEXT("Holding still never repeats the initial charge"), Economy->GetGold(), GoldBeforeLine - InstallCost);
    Build->UpdateDragPlacement(CursorAt(FIntPoint(2200, 1000)));
    Drain();
    TestEqual(TEXT("Fast cursor movement fills all four footprint-spaced tiles"), Count(Road.Get()), 4);
    TestEqual(TEXT("Every tile is a real authority commit"), Zones->GetWorldRevision(), RevisionBeforeLine + 4);
    TestEqual(TEXT("Every successful tile pays exactly one install cost"), Economy->GetGold(), GoldBeforeLine - 4 * InstallCost);
    TSet<FGuid> LineIds;
    for (int32 X = 1000; X <= 2200; X += 400)
    {
        AFFZSFacilityActor* Tile = Zones->FindFacilityAtWorldPoint(CursorAt(FIntPoint(X, 1000)), ZoneId);
        if (TestNotNull(TEXT("Each interpolated tile is independently registered"), Tile))
        {
            TestEqual(TEXT("Installed footprint uses centimetre coordinates"), Tile->GetGridPlacement().OriginCell, FIntPoint(X, 1000));
            LineIds.Add(Tile->GetFacilityInstanceId());
        }
    }
    TestEqual(TEXT("Separate tiles have separate stable instance IDs"), LineIds.Num(), 4);
    Build->UpdateDragPlacement(CursorAt(FIntPoint(1000, 1000)));
    Drain();
    TestEqual(TEXT("Backtracking does not add duplicate purchases"), Economy->GetGold(), GoldBeforeLine - 4 * InstallCost);
    TestEqual(TEXT("Backtracking does not change authority revision"), Zones->GetWorldRevision(), RevisionBeforeLine + 4);

    SelectAt(Road.Get(), FIntPoint(4200, 1000));
    Build->RefreshCurrentPlacementValidation();
    const int32 GoldBeforeRoadClick = Economy->GetGold();
    const int64 RevisionBeforeRoadClick = Zones->GetWorldRevision();
    Build->HandlePlacementPrimaryPress(false);
    TestEqual(TEXT("Quick click also installs an opted-in road"), Count(Road.Get()), 5);
    TestEqual(TEXT("Opted-in quick click charges one tile"), Economy->GetGold(), GoldBeforeRoadClick - InstallCost);
    TestEqual(TEXT("Opted-in quick click commits one tile"), Zones->GetWorldRevision(), RevisionBeforeRoadClick + 1);
    TestFalse(TEXT("Opted-in quick click does not outlive the released button"), Build->DragPlacementSession.IsValid());
    Build->UpdateDragPlacement(CursorAt(FIntPoint(5000, 1000)));
    Drain();
    TestEqual(TEXT("An opted-in road cannot keep buying after a quick click"), Count(Road.Get()), 5);

    SelectAt(Road.Get(), FIntPoint(4200, 1000));
    Build->RefreshCurrentPlacementValidation();
    TestFalse(TEXT("An occupied initial position fails actual preview validation"), Build->LastValidation.bIsValid);
    Build->HandlePlacementPrimaryPress(true);
    TestTrue(TEXT("Holding on existing road can start an extension into empty ground"), Build->DragPlacementSession.IsValid());
    TestEqual(TEXT("The occupied initial tile is never charged again"), Economy->GetGold(), GoldBeforeRoadClick - InstallCost);
    TestEqual(TEXT("The occupied initial tile makes no authority commit"), Zones->GetWorldRevision(), RevisionBeforeRoadClick + 1);
    Build->UpdateDragPlacement(CursorAt(FIntPoint(5000, 1000)));
    Drain();
    TestEqual(TEXT("Holding from existing road installs the next two empty tiles"), Count(Road.Get()), 7);
    TestEqual(TEXT("Extending existing road charges only the two new tiles"), Economy->GetGold(), GoldBeforeRoadClick - 3 * InstallCost);
    TestEqual(TEXT("Extending existing road commits only the two new tiles"), Zones->GetWorldRevision(), RevisionBeforeRoadClick + 3);
    Build->UpdateDragPlacement(CursorAt(FIntPoint(4200, 1000)));
    Drain();
    TestEqual(TEXT("Returning to the initially occupied tile never retries or charges it"),
        Economy->GetGold(), GoldBeforeRoadClick - 3 * InstallCost);

    // The frame cap is contractual; the soft time budget may stop a pass earlier.
    SelectAt(Road.Get(), FIntPoint(1000, 2200));
    const int32 BeforeBudget = Count(Road.Get());
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(6200, 2200)));
    const int32 BeforePass = Count(Road.Get());
    Build->ProcessDragPlacement();
    const int32 AfterPass = Count(Road.Get());
    TestTrue(TEXT("One frame advances but never exceeds eight authority installs"),
        AfterPass > BeforePass && AfterPass - BeforePass <= 8);
    TestTrue(TEXT("Long cursor jumps retain work for later frames"), AfterPass - BeforeBudget < 14);
    Drain();
    TestEqual(TEXT("Bounded processing eventually fills the complete line"), Count(Road.Get()) - BeforeBudget, 14);

    // Failed positions are remembered for this stroke, even if occupancy changes later.
    FFFZSGridPlacement BlockedPlacement;
    BlockedPlacement.OriginCell = FIntPoint(1400, 3400);
    const FFFZSCommand BlockerCommand = UFFZSGridLibrary::MakeInstallCommandFromDefinition(
        Road.Get(), ZoneId, BlockedPlacement, Zones->GetWorldRevision());
    if (!TestTrue(TEXT("Occupancy fixture installs"), Zones->ExecuteCommand(BlockerCommand).bSucceeded))
    {
        return false;
    }
    SelectAt(Road.Get(), FIntPoint(1000, 3400));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(1800, 3400)));
    Drain();
    TestNotNull(TEXT("An occupied tile does not block the next valid tile"),
        Zones->FindFacilityAtWorldPoint(CursorAt(FIntPoint(1800, 3400)), ZoneId));
    FFFZSCommand RemoveBlocker;
    RemoveBlocker.CommandId = FGuid::NewGuid();
    RemoveBlocker.TransactionId = FGuid::NewGuid();
    RemoveBlocker.Type = EFFZSCommandType::Remove;
    RemoveBlocker.FacilityInstanceId = BlockerCommand.FacilityInstanceId;
    RemoveBlocker.ExpectedWorldRevision = Zones->GetWorldRevision();
    if (!TestTrue(TEXT("Previously blocked position becomes available"), Zones->ExecuteCommand(RemoveBlocker).bSucceeded))
    {
        return false;
    }
    const int64 RevisionAfterRemoval = Zones->GetWorldRevision();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(1400, 3400)));
    Drain();
    TestEqual(TEXT("The same stroke does not retry a previously failed position"), Zones->GetWorldRevision(), RevisionAfterRemoval);
    SelectAt(Road.Get(), FIntPoint(1400, 3400));
    Build->BeginDragPlacement();
    TestEqual(TEXT("A fresh press clears failed-position history"), Zones->GetWorldRevision(), RevisionAfterRemoval + 1);

    SelectAt(Rectangle.Get(), FIntPoint(1000, 4600));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(1000, 5200)));
    Drain();
    TestEqual(TEXT("Rectangular footprints use their own 200cm vertical spacing"), Count(Rectangle.Get()), 4);
    SelectAt(Rectangle.Get(), FIntPoint(3000, 4600), 9000);
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(3600, 4600)));
    Drain();
    TestEqual(TEXT("A quarter turn rotates footprint spacing with the tiles"), Count(Rectangle.Get()), 8);

    // Independent cm rounding used to shorten every fourth 45-degree step to
    // 398.808cm. A four-metre tile then collided with its immediate predecessor.
    SelectAt(Road.Get(), FIntPoint(5000, 3400), 4500);
    const int32 BeforeOblique = Count(Road.Get());
    const FVector ObliqueStart = CursorAt(FIntPoint(5000, 3400));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(ObliqueStart + FRotator(0.0, 45.0, 0.0).RotateVector(FVector(4000.0, 0.0, 0.0)));
    Drain();
    TestEqual(TEXT("A 45-degree line installs all tiles despite integer-origin rounding"), Count(Road.Get()) - BeforeOblique, 11);

    SelectAt(Road.Get(), FIntPoint(1000, 5800));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(6200, 5800)));
    Build->EndDragPlacement();
    const int64 RevisionAfterRelease = Zones->GetWorldRevision();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(7000, 5800)));
    Drain();
    TestEqual(TEXT("Release discards queued work and prevents later ghost installs"), Zones->GetWorldRevision(), RevisionAfterRelease);

    SelectAt(Road.Get(), FIntPoint(1000, 7000));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(6200, 7000)));
    TestTrue(TEXT("Pause acquires the build input suspension"), Build->SuspendInputForPauseMenu());
    const int64 RevisionAtPause = Zones->GetWorldRevision();
    Drain();
    TestEqual(TEXT("Pause stops pending purchases immediately"), Zones->GetWorldRevision(), RevisionAtPause);
    Build->RestoreInputAfterPauseMenu();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(7000, 7000)));
    Drain();
    TestEqual(TEXT("Resume requires a new press instead of resuming the old stroke"), Zones->GetWorldRevision(), RevisionAtPause);

    SelectAt(Road.Get(), FIntPoint(3000, 7000));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(6200, 7000)));
    const int64 RevisionBeforeRotation = Zones->GetWorldRevision();
    Build->SetPlacementYawDegrees(90.0f);
    Drain();
    TestEqual(TEXT("Changing direction cancels queued purchases at the old angle"), Zones->GetWorldRevision(), RevisionBeforeRotation);

    SelectAt(Road.Get(), FIntPoint(4600, 7000));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(6200, 7000)));
    const int64 RevisionBeforeReset = Zones->GetWorldRevision();
    Build->ResetPlacementSelection();
    Drain();
    TestEqual(TEXT("Selection reset discards all pending stroke work"), Zones->GetWorldRevision(), RevisionBeforeReset);

    SelectAt(Limited.Get(), FIntPoint(1000, 8200));
    const int32 GoldBeforeLimit = Economy->GetGold();
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(2200, 8200)));
    Drain();
    TestEqual(TEXT("Drag obeys the authored world instance limit"), Count(Limited.Get()), 2);
    TestEqual(TEXT("Build-limit rejection never charges for rejected tiles"), Economy->GetGold(), GoldBeforeLimit - 2 * InstallCost);
    TestEqual(TEXT("Build-limit feedback reports the actual authority failure"),
        Build->PlacementFailureFeedback->GetFailureTitle().ToString(),
        UFF_BuildPlacementFailureFeedback::MakeFailureTitle(EFFZSPlacementFailure::BuildLimitReached, false).ToString());
    SelectAt(Limited.Get(), FIntPoint(3000, 8200));
    Build->RefreshCurrentPlacementValidation();
    TestFalse(TEXT("A reached build limit invalidates the initial preview"), Build->LastValidation.bIsValid);
    TestTrue(TEXT("The initial preview reports the actual build-limit failure"),
        Build->LastValidation.Reasons.Contains(EFFZSPlacementFailure::BuildLimitReached));
    Build->HandlePlacementPrimaryPress(true);
    TestFalse(TEXT("A build-limit failure cannot start a held purchase session"), Build->DragPlacementSession.IsValid());
    Build->UpdateDragPlacement(CursorAt(FIntPoint(4200, 8200)));
    Drain();
    TestEqual(TEXT("Holding after an initial build-limit failure never purchases"), Count(Limited.Get()), 2);
    TestEqual(TEXT("An initial build-limit failure never charges gold"), Economy->GetGold(), GoldBeforeLimit - 2 * InstallCost);

    SelectAt(Road.Get(), FIntPoint(1000, 9000));
    TestTrue(TEXT("Fixture leaves enough money for exactly two tiles"),
        Economy->TrySpend(Economy->GetGold() - 2 * InstallCost, EFF_SpendReason::Promotion));
    const int32 BeforeMoney = Count(Road.Get());
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(2200, 9000)));
    Drain();
    TestEqual(TEXT("Insufficient funds stop at the last affordable tile"), Count(Road.Get()) - BeforeMoney, 2);
    TestEqual(TEXT("The real economy never overspends"), Economy->GetGold(), 0);
    TestEqual(TEXT("Money rejection reaches the real failure presenter"),
        Build->PlacementFailureFeedback->GetFailureType(), EFF_BuildPlacementFailureType::InsufficientGold);
    Economy->AddRevenue(10 * InstallCost, EFF_EarnReason::MinigameReward);
    Build->UpdateDragPlacement(CursorAt(FIntPoint(2600, 9000)));
    Drain();
    TestEqual(TEXT("New income does not silently restart the cancelled stroke"), Count(Road.Get()) - BeforeMoney, 2);
    TestEqual(TEXT("New income remains untouched until a new press"), Economy->GetGold(), 10 * InstallCost);

    // Economic rejection on the initial click must not leave an armed stroke
    // which can unexpectedly spend money received while the button stays held.
    TestTrue(TEXT("Fixture exhausts funds before the initial click"),
        Economy->TrySpend(Economy->GetGold(), EFF_SpendReason::Promotion));
    SelectAt(Road.Get(), FIntPoint(3000, 9000));
    Build->RefreshCurrentPlacementValidation();
    TestFalse(TEXT("Insufficient funds invalidate the initial preview"), Build->LastValidation.bIsValid);
    TestTrue(TEXT("The initial preview reports the actual insufficient-funds failure"),
        Build->LastValidation.Reasons.Contains(EFFZSPlacementFailure::InsufficientFunds));
    const int64 RevisionBeforeUnaffordableClick = Zones->GetWorldRevision();
    Build->HandlePlacementPrimaryPress(true);
    TestFalse(TEXT("An unaffordable initial click never arms a held purchase session"), Build->DragPlacementSession.IsValid());
    TestEqual(TEXT("An unaffordable initial click never commits"), Zones->GetWorldRevision(), RevisionBeforeUnaffordableClick);
    Economy->AddRevenue(3 * InstallCost, EFF_EarnReason::MinigameReward);
    Build->UpdateDragPlacement(CursorAt(FIntPoint(4200, 9000)));
    Drain();
    TestEqual(TEXT("Income after an unaffordable initial click requires a fresh press"),
        Zones->GetWorldRevision(), RevisionBeforeUnaffordableClick);
    TestEqual(TEXT("Holding after an unaffordable initial click leaves new income untouched"),
        Economy->GetGold(), 3 * InstallCost);
    return true;
}

#endif
