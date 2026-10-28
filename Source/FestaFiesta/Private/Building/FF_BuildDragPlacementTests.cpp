// FF_BuildDragPlacementTests.cpp
// 역할: 드래그 미리보기 중 가능한 위치만 해제 시 한 번에 설치하고 취소하는 동작을 검사한다.

// Exercises deferred drag proposals through the real installation, occupancy and economy boundaries.
// Only cursor sampling and initial selection are seeded; no install result is mocked.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetBundleData.h"
#include "Building/FF_BuildCatalogModel.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "Components/LineBatchComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/FF_ZoneSystemEconomyBridgeSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/HitResult.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_ProjectZoneTestFixture.h"
#include "Tests/FF_FFZSTestFixtures.h"
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
    // async completion cannot replace the isolated test catalog.
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
    Economy->StartFestival(Festival.Get());

    const FName ZoneId(FFProjectZoneTest::ZoneId());
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
    TStrongObjectPtr<UFFZSFacilityDefinition> Small(NewObject<UFFZSFacilityDefinition>());
    TStrongObjectPtr<UFFZSFacilityDefinition> Queued(NewObject<UFFZSFacilityDefinition>());
    Road->Spec = MakeRoadSpec(TEXT("Road_Surface_DragTest"), FIntPoint(400, 400));
    Road->bEnableDragPlacement = true;
    Rectangle->Spec = MakeRoadSpec(TEXT("Road_Surface_RectangleTest"), FIntPoint(400, 200));
    Rectangle->bEnableDragPlacement = true;
    Single->Spec = MakeRoadSpec(TEXT("Road_Surface_SingleTest"), FIntPoint(400, 400));
    Limited->Spec = MakeRoadSpec(TEXT("Road_Surface_LimitedTest"), FIntPoint(400, 400));
    Limited->Spec.MaxInstancesPerWorld = 2;
    Limited->bEnableDragPlacement = true;
    Small->Spec = MakeRoadSpec(TEXT("Road_Surface_LongStrokeTest"), FIntPoint(40, 40));
    Small->bEnableDragPlacement = true;
    Queued->Spec = FFZSTest::MakeValidFacilitySpec();
    Queued->Spec.FacilityDefinitionId = TEXT("Booth_Queued_DragTest");
    Queued->Spec.InstallCostKey = TEXT("Cost.Road.Default");
    Queued->bEnableDragPlacement = true;

    // Use a private dynamic type so catalog validation runs without changing production assets.
    const FPrimaryAssetType TestType(*FString::Printf(TEXT("FFBuildDragTest_%s"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    ON_SCOPE_EXIT { Assets->RemovePrimaryAssetType(TestType); };
    for (UFFZSFacilityDefinition* Definition : { Road.Get(), Rectangle.Get(), Single.Get(), Limited.Get(), Small.Get(), Queued.Get() })
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
    if (!TestEqual(TEXT("All authored definitions pass production catalog validation"), Catalog->GetEntriesView().Num(), 6))
    {
        return false;
    }

    const auto SelectAt = [&](UFFZSFacilityDefinition* Definition, const FIntPoint Origin, const int32 Yaw = 0)
    {
        Build->EndDragPlacement();
        Build->State = EFF_BuildModeState::Placing;
        Build->SelectedFacilityDefinitionId = Definition->Spec.FacilityDefinitionId;
        Build->SelectedSpec = Definition->Spec;
        Build->PendingQueueLayout = Definition->Spec.QueueLayout;
        Build->PendingQueueSettings = Definition->Spec.QueueSettings;
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
        // No wall-clock assumption: every pass may prepare as little as one preview position.
        for (int32 Pass = 0; Pass < 256; ++Pass)
        {
            Build->ProcessDragPlacement();
        }
    };
    const auto Count = [&](const UFFZSFacilityDefinition* Definition)
    {
        return Zones->CountFacilitiesByDefinitionId(Definition->Spec.FacilityDefinitionId);
    };

    TestEqual(TEXT("New placement sessions preserve connected roads by default"), Build->GetDragPlacementGapCm(), 0);
    Build->SetDragPlacementGapCm(-100);
    TestEqual(TEXT("Negative spacing cannot create overlapping pitch"), Build->GetDragPlacementGapCm(), 0);
    Build->SetDragPlacementGapCm(1001);
    TestEqual(TEXT("Spacing is bounded to ten metres"), Build->GetDragPlacementGapCm(), 1000);
    Build->SetDragPlacementGapCm(0);

    SelectAt(Single.Get(), FIntPoint(3000, 1000));
    TestFalse(TEXT("Existing definitions remain single-click by default"), Build->IsDragPlacementEnabled());
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(4200, 1000)));
    Drain();
    TestEqual(TEXT("Opted-out definitions never stage purchases through drag"), Count(Single.Get()), 0);

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
    TestFalse(TEXT("Moving an existing facility cannot start a purchase stroke"), Build->DragPlacementSession.IsValid());
    TestEqual(TEXT("Moving never purchases a facility"), Count(Road.Get()), 0);

    // A held gesture is only a proposal. Nothing in the registry, occupancy,
    // construction history or wallet changes until its one release operation.
    SelectAt(Road.Get(), FIntPoint(1000, 1000));
    const int32 GoldBeforeStroke = Economy->GetGold();
    const int64 RevisionBeforeStroke = Zones->GetWorldRevision();
    Build->RefreshCurrentPlacementValidation();
    Build->HandlePlacementPrimaryPress(true);
    TestTrue(TEXT("Held press starts a preview session"), Build->DragPlacementSession.IsValid());
    TestEqual(TEXT("The first held preview does not purchase its origin"), Count(Road.Get()), 0);
    for (const int32 X : { 1000, 1400, 1800, 2200, 1400, 2200 })
    {
        Build->UpdateDragPlacement(CursorAt(FIntPoint(X, 1000)));
        Drain();
        Build->RefreshDragPlacementPreview(false, false);
    }
    TestEqual(TEXT("All sampled ghosts remain outside the facility registry"), Count(Road.Get()), 0);
    TestEqual(TEXT("Previewing and retracing do not spend money"), Economy->GetGold(), GoldBeforeStroke);
    TestEqual(TEXT("Previewing and retracing do not change world revision"), Zones->GetWorldRevision(), RevisionBeforeStroke);
    TestTrue(TEXT("Release installs the complete deduplicated proposal"), Build->CommitDragPlacement());
    TestFalse(TEXT("Release ends the staged session"), Build->DragPlacementSession.IsValid());
    TestEqual(TEXT("Four preview positions become four facilities"), Count(Road.Get()), 4);
    TestEqual(TEXT("Release charges each distinct preview exactly once"), Economy->GetGold(), GoldBeforeStroke - 4 * InstallCost);
    TestEqual(TEXT("Only release publishes the four construction changes"), Zones->GetWorldRevision(), RevisionBeforeStroke + 4);
    TestEqual(TEXT("Successful release retains continuous placement mode"), Build->GetBuildModeState(), EFF_BuildModeState::Placing);
    TestNotNull(TEXT("The next cursor preview survives release"), Build->PlacementPreview.Get());
    TestFalse(TEXT("A duplicate release cannot install the proposal twice"), Build->CommitDragPlacement());
    TestEqual(TEXT("Duplicate release does not charge twice"), Economy->GetGold(), GoldBeforeStroke - 4 * InstallCost);
    TestTrue(TEXT("One undo removes the entire released proposal"), Build->UndoBuildEdit());
    TestEqual(TEXT("All four facilities are removed by one undo"), Count(Road.Get()), 0);
    TestEqual(TEXT("One undo refunds the complete proposal"), Economy->GetGold(), GoldBeforeStroke);
    TestTrue(TEXT("One redo restores the entire released proposal"), Build->RedoBuildEdit());
    TestEqual(TEXT("Redo restores four facilities"), Count(Road.Get()), 4);
    TestEqual(TEXT("Redo charges the proposal exactly once"), Economy->GetGold(), GoldBeforeStroke - 4 * InstallCost);

    // A press and release delivered in the same input frame stays one click.
    SelectAt(Road.Get(), FIntPoint(4200, 1000));
    Build->RefreshCurrentPlacementValidation();
    Build->HandlePlacementPrimaryPress(false);
    TestEqual(TEXT("A quick click installs one opted-in facility"), Count(Road.Get()), 5);
    TestFalse(TEXT("A quick click leaves no held preview session"), Build->DragPlacementSession.IsValid());
    Build->UpdateDragPlacement(CursorAt(FIntPoint(5000, 1000)));
    Drain();
    TestEqual(TEXT("Moving after quick release cannot install again"), Count(Road.Get()), 5);

    // Releasing before the frame budget drained must include every drawn position.
    SelectAt(Road.Get(), FIntPoint(1000, 2200));
    const int32 BeforeBudget = Count(Road.Get());
    const int32 GoldBeforeBudget = Economy->GetGold();
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(6200, 2200)));
    Build->ProcessDragPlacement();
    TestEqual(TEXT("A bounded preview pass does not install a prefix"), Count(Road.Get()), BeforeBudget);
    TestEqual(TEXT("Preview work cannot reserve or spend wallet money"), Economy->GetGold(), GoldBeforeBudget);
    TestTrue(TEXT("Release drains pending preview positions before the atomic commit"), Build->CommitDragPlacement());
    TestEqual(TEXT("The complete fourteen-tile line is installed together"), Count(Road.Get()) - BeforeBudget, 14);

    // Bends and retracing retain the actual drawn path, not a rectangle or shortcut.
    SelectAt(Road.Get(), FIntPoint(8200, 1000));
    const int32 BeforeBend = Count(Road.Get());
    Build->BeginDragPlacement();
    for (const FIntPoint Position : { FIntPoint(8600, 1000), FIntPoint(9000, 1000),
        FIntPoint(9000, 1400), FIntPoint(9000, 1800), FIntPoint(8600, 1800), FIntPoint(8200, 1800) })
    {
        Build->UpdateDragPlacement(CursorAt(Position));
    }
    Drain();
    TestEqual(TEXT("A bent proposal has no installed prefix"), Count(Road.Get()), BeforeBend);
    TestTrue(TEXT("A complete bent proposal installs on release"), Build->CommitDragPlacement());
    TestEqual(TEXT("Both turns retain all seven drawn positions"), Count(Road.Get()) - BeforeBend, 7);
    TestNull(TEXT("The undrawn interior of a bend stays empty"),
        Zones->FindFacilityAtWorldPoint(CursorAt(FIntPoint(8600, 1400)), ZoneId));

    // An occupied middle preview is skipped, while both valid ends form one
    // construction entry. Undo must never touch the pre-existing obstacle.
    FFFZSGridPlacement BlockedPlacement;
    BlockedPlacement.OriginCell = FIntPoint(1400, 3400);
    const FFFZSCommand BlockerCommand = UFFZSGridLibrary::MakeInstallCommandFromDefinition(
        Road.Get(), ZoneId, BlockedPlacement, Zones->GetWorldRevision());
    if (!TestTrue(TEXT("Occupancy fixture installs"), Zones->ExecuteCommand(BlockerCommand).bSucceeded))
    {
        return false;
    }
    SelectAt(Road.Get(), FIntPoint(1000, 3400));
    const int32 BeforeBlocked = Count(Road.Get());
    const int32 GoldBeforeBlocked = Economy->GetGold();
    const int64 RevisionBeforeBlocked = Zones->GetWorldRevision();
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(1800, 3400)));
    Drain();
    TestEqual(TEXT("Invalid previews do not change registry occupancy"), Count(Road.Get()), BeforeBlocked);
    TestTrue(TEXT("Release skips the occupied middle and installs the valid ends"), Build->CommitDragPlacement());
    TestEqual(TEXT("Both valid ends install despite the blocked middle"), Count(Road.Get()), BeforeBlocked + 2);
    TestEqual(TEXT("The blocked middle contributes no cost"), Economy->GetGold(), GoldBeforeBlocked - 2 * InstallCost);
    TestEqual(TEXT("Only the accepted ends publish revisions"), Zones->GetWorldRevision(), RevisionBeforeBlocked + 2);
    TestNotNull(TEXT("The valid position before the blocker installs"),
        Zones->FindFacilityAtWorldPoint(CursorAt(FIntPoint(1000, 3400)), ZoneId));
    TestNotNull(TEXT("A later valid position still installs after the rejected candidate"),
        Zones->FindFacilityAtWorldPoint(CursorAt(FIntPoint(1800, 3400)), ZoneId));
    TestNotNull(TEXT("The existing obstacle remains unchanged"), Zones->FindFacility(BlockerCommand.FacilityInstanceId));
    TestTrue(TEXT("One undo removes the accepted subset"), Build->UndoBuildEdit());
    TestEqual(TEXT("Undo removes both ends without removing the blocker"), Count(Road.Get()), BeforeBlocked);
    TestEqual(TEXT("Subset undo refunds only accepted positions"), Economy->GetGold(), GoldBeforeBlocked);
    TestNotNull(TEXT("The unrelated blocker survives subset undo"), Zones->FindFacility(BlockerCommand.FacilityInstanceId));
    SelectAt(Road.Get(), FIntPoint(1400, 3400));
    const int64 RevisionBeforeAllBlocked = Zones->GetWorldRevision();
    Build->BeginDragPlacement();
    Drain();
    TestFalse(TEXT("An entirely blocked proposal installs nothing"), Build->CommitDragPlacement());
    TestEqual(TEXT("An entirely blocked proposal preserves the wallet"), Economy->GetGold(), GoldBeforeBlocked);
    TestEqual(TEXT("An entirely blocked proposal publishes no revision"), Zones->GetWorldRevision(), RevisionBeforeAllBlocked);
    TestTrue(TEXT("An empty accepted subset preserves the previous redo entry"), Zones->CanRedoConstruction());
    TestTrue(TEXT("One redo restores only the accepted subset"), Build->RedoBuildEdit());
    TestEqual(TEXT("Subset redo restores both ends without duplicating the blocker"), Count(Road.Get()), BeforeBlocked + 2);
    TestEqual(TEXT("Subset redo charges exactly two facilities"), Economy->GetGold(), GoldBeforeBlocked - 2 * InstallCost);

    // Revalidate against the release-time world, even after all held previews
    // passed validation. Another command can occupy a candidate before release.
    SelectAt(Road.Get(), FIntPoint(5000, 7400));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(5800, 7400)));
    Drain();
    FFFZSGridPlacement LateBlockedPlacement;
    LateBlockedPlacement.OriginCell = FIntPoint(5400, 7400);
    const FFFZSCommand LateBlocker = UFFZSGridLibrary::MakeInstallCommandFromDefinition(
        Road.Get(), ZoneId, LateBlockedPlacement, Zones->GetWorldRevision());
    if (!TestTrue(TEXT("Another command occupies a previously valid preview"), Zones->ExecuteCommand(LateBlocker).bSucceeded))
    {
        return false;
    }
    const int32 BeforeLateRelease = Count(Road.Get());
    const int32 GoldBeforeLateRelease = Economy->GetGold();
    TestTrue(TEXT("Release reselects the remaining valid positions against the current world"), Build->CommitDragPlacement());
    TestEqual(TEXT("The late blocked candidate is skipped while both ends install"), Count(Road.Get()), BeforeLateRelease + 2);
    TestEqual(TEXT("Late revalidation charges only the accepted positions"), Economy->GetGold(), GoldBeforeLateRelease - 2 * InstallCost);
    TestTrue(TEXT("The release-time subset remains one undo entry"), Build->UndoBuildEdit());
    TestEqual(TEXT("Undo removes only the release-time subset"), Count(Road.Get()), BeforeLateRelease);
    TestNotNull(TEXT("The independently installed late blocker survives undo"), Zones->FindFacility(LateBlocker.FacilityInstanceId));

    // Footprint axes, arbitrary rotation and the user's gap survive staging.
    SelectAt(Rectangle.Get(), FIntPoint(1000, 4600));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(1000, 5200)));
    TestTrue(TEXT("A rectangular vertical proposal commits"), Build->CommitDragPlacement());
    TestEqual(TEXT("Rectangular footprints use 200cm vertical spacing"), Count(Rectangle.Get()), 4);
    SelectAt(Rectangle.Get(), FIntPoint(3000, 4600), 9000);
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(3600, 4600)));
    TestTrue(TEXT("A quarter-turned rectangular proposal commits"), Build->CommitDragPlacement());
    TestEqual(TEXT("Quarter turn rotates the spacing with the footprint"), Count(Rectangle.Get()), 8);

    SelectAt(Road.Get(), FIntPoint(5000, 3400), 4500);
    const int32 BeforeOblique = Count(Road.Get());
    const FVector ObliqueStart = CursorAt(FIntPoint(5000, 3400));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(ObliqueStart + FRotator(0.0, 45.0, 0.0).RotateVector(FVector(4000.0, 0.0, 0.0)));
    TestTrue(TEXT("A diagonal proposal avoids centimetre rounding overlap"), Build->CommitDragPlacement());
    TestEqual(TEXT("All eleven diagonal facilities install together"), Count(Road.Get()) - BeforeOblique, 11);

    Build->SetDragPlacementGapCm(150);
    SelectAt(Road.Get(), FIntPoint(1000, 6200));
    const int32 BeforeSpaced = Count(Road.Get());
    const int32 GoldBeforeSpaced = Economy->GetGold();
    Build->HandlePlacementPrimaryPress(true);
    Build->UpdateDragPlacement(CursorAt(FIntPoint(2650, 6200)));
    Drain();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(1550, 6200)));
    Drain();
    TestEqual(TEXT("Spaced and retraced previews remain unpurchased"), Count(Road.Get()), BeforeSpaced);
    TestTrue(TEXT("A regularly spaced proposal commits"), Build->CommitDragPlacement());
    TestEqual(TEXT("Four 400cm footprints retain their 150cm gaps"), Count(Road.Get()) - BeforeSpaced, 4);
    for (const int32 X : { 1000, 1550, 2100, 2650 })
    {
        AFFZSFacilityActor* Facility = Zones->FindFacilityAtWorldPoint(CursorAt(FIntPoint(X, 6200)), ZoneId);
        if (TestNotNull(TEXT("Each spaced preview position is installed"), Facility))
        {
            TestEqual(TEXT("The reviewed gap is the authoritative placement gap"),
                Facility->GetGridPlacement().OriginCell, FIntPoint(X, 6200));
        }
    }
    TestEqual(TEXT("Retracing never duplicates a charge"), Economy->GetGold(), GoldBeforeSpaced - 4 * InstallCost);

    SelectAt(Road.Get(), FIntPoint(1000, 7000));
    const int64 RevisionBeforeGapChange = Zones->GetWorldRevision();
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(2650, 7000)));
    Drain();
    Build->SetDragPlacementGapCm(300);
    TestFalse(TEXT("Changing spacing cancels all previews made with the old gap"), Build->DragPlacementSession.IsValid());
    TestEqual(TEXT("Changing spacing never purchases the old proposal"), Zones->GetWorldRevision(), RevisionBeforeGapChange);
    SelectAt(Road.Get(), FIntPoint(1000, 6600));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(1700, 6600)));
    TestTrue(TEXT("The next gesture commits using the changed gap"), Build->CommitDragPlacement());
    TestEqual(TEXT("A 700cm pitch places exactly two additional facilities"), Count(Road.Get()) - BeforeSpaced, 6);
    TestTrue(TEXT("The later spaced proposal is one separate undo entry"), Build->UndoBuildEdit());
    TestEqual(TEXT("Undo preserves the first proposal"), Count(Road.Get()) - BeforeSpaced, 4);
    TestTrue(TEXT("The first spaced proposal is one undo entry"), Build->UndoBuildEdit());
    TestEqual(TEXT("Two undos remove the two independent proposals"), Count(Road.Get()), BeforeSpaced);
    TestEqual(TEXT("Undo refunds exactly both proposals"), Economy->GetGold(), GoldBeforeSpaced);
    TestTrue(TEXT("The first proposal can be redone"), Build->RedoBuildEdit());
    TestTrue(TEXT("The second proposal can be redone"), Build->RedoBuildEdit());
    TestEqual(TEXT("Redo preserves the two historical spacings"), Count(Road.Get()) - BeforeSpaced, 6);
    Build->SetDragPlacementGapCm(0);

    // Keep previews and the public atomic batch within the same 128-member limit.
    // An oversized gesture must not commit a prefix that the user did not approve.
    SelectAt(Small.Get(), FIntPoint(7000, 8200));
    const int32 GoldBeforeLongStroke = Economy->GetGold();
    const int64 RevisionBeforeLongStroke = Zones->GetWorldRevision();
    Build->BeginDragPlacement();
    for (int32 Row = 0; Row < 10; ++Row)
    {
        const int32 Y = 8200 + Row * 40;
        if (Row > 0)
        {
            Build->UpdateDragPlacement(CursorAt(FIntPoint(Row % 2 == 0 ? 7000 : 7480, Y)));
        }
        Build->UpdateDragPlacement(CursorAt(FIntPoint(Row % 2 == 0 ? 7480 : 7000, Y)));
    }
    Drain();
    TestFalse(TEXT("An oversized 130-position proposal cannot release a partial batch"), Build->CommitDragPlacement());
    TestEqual(TEXT("Oversized proposals install nothing"), Count(Small.Get()), 0);
    TestEqual(TEXT("Oversized proposals spend nothing"), Economy->GetGold(), GoldBeforeLongStroke);
    TestEqual(TEXT("Oversized proposals publish no revision"), Zones->GetWorldRevision(), RevisionBeforeLongStroke);

    SelectAt(Small.Get(), FIntPoint(7000, 8200));
    Build->BeginDragPlacement();
    for (int32 Row = 0; Row < 10; ++Row)
    {
        const int32 Y = 8200 + Row * 40;
        if (Row > 0)
        {
            Build->UpdateDragPlacement(CursorAt(FIntPoint(Row % 2 == 0 ? 7000 : 7480, Y)));
        }
        const int32 X = Row == 9 ? 7080 : (Row % 2 == 0 ? 7480 : 7000);
        Build->UpdateDragPlacement(CursorAt(FIntPoint(X, Y)));
    }
    TestTrue(TEXT("Exactly 128 preview positions can commit as one batch"), Build->CommitDragPlacement());
    TestEqual(TEXT("The complete maximum-size proposal installs"), Count(Small.Get()), 128);
    TestTrue(TEXT("One undo removes the maximum-size proposal"), Build->UndoBuildEdit());
    TestEqual(TEXT("Maximum-size undo leaves no prefix behind"), Count(Small.Get()), 0);
    TestEqual(TEXT("Maximum-size undo returns the exact complete cost"), Economy->GetGold(), GoldBeforeLongStroke);
    TestTrue(TEXT("One redo restores the maximum-size proposal"), Build->RedoBuildEdit());
    TestEqual(TEXT("Maximum-size redo restores every facility"), Count(Small.Get()), 128);

    // Cancellation, UI interruptions, selection changes and pause are cancellation,
    // never implicit release. No deferred proposal may buy after the interruption.
    SelectAt(Road.Get(), FIntPoint(1000, 5800));
    const int64 RevisionBeforeCancel = Zones->GetWorldRevision();
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(6200, 5800)));
    Drain();
    Build->EndDragPlacement();
    TestFalse(TEXT("Cancelled proposals cannot later release"), Build->CommitDragPlacement());
    Build->UpdateDragPlacement(CursorAt(FIntPoint(7000, 5800)));
    Drain();
    TestEqual(TEXT("Cancellation discards every unpurchased preview"), Zones->GetWorldRevision(), RevisionBeforeCancel);

    SelectAt(Road.Get(), FIntPoint(1000, 7000));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(6200, 7000)));
    Drain();
    TestTrue(TEXT("Pause acquires build input suspension"), Build->SuspendInputForPauseMenu());
    TestFalse(TEXT("Pause cancels the held proposal"), Build->DragPlacementSession.IsValid());
    Build->RestoreInputAfterPauseMenu();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(7000, 7000)));
    Drain();
    TestEqual(TEXT("Resume cannot purchase a cancelled proposal"), Zones->GetWorldRevision(), RevisionBeforeCancel);

    SelectAt(Road.Get(), FIntPoint(3000, 7000));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(6200, 7000)));
    Drain();
    Build->SetPlacementYawDegrees(90.0f);
    TestFalse(TEXT("Changing rotation cancels the old proposal"), Build->DragPlacementSession.IsValid());
    TestEqual(TEXT("Rotation does not purchase old-angle previews"), Zones->GetWorldRevision(), RevisionBeforeCancel);
    SelectAt(Road.Get(), FIntPoint(4600, 7000));
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(6200, 7000)));
    Drain();
    Build->ResetPlacementSelection();
    TestFalse(TEXT("Selection reset cancels every preview"), Build->DragPlacementSession.IsValid());
    TestEqual(TEXT("Selection reset does not purchase a prefix"), Zones->GetWorldRevision(), RevisionBeforeCancel);

    // Each candidate is legal against the existing world, but neighboring queues
    // overlap each other. Greedy batch validation must skip the middle two and
    // still accept a later non-overlapping candidate in the same gesture.
    SelectAt(Queued.Get(), FIntPoint(3500, 7000));
    const int32 GoldBeforeQueued = Economy->GetGold();
    for (const int32 Y : { 7000, 7300, 7600, 7900 })
    {
        FFFZSGridPlacement CandidatePlacement;
        CandidatePlacement.OriginCell = FIntPoint(3500, Y);
        const FFFZSCommand Candidate = UFFZSGridLibrary::MakeInstallCommandFromDefinition(
            Queued.Get(), ZoneId, CandidatePlacement, Zones->GetWorldRevision());
        TestTrue(TEXT("Every queued candidate is individually valid before the gesture"),
            Zones->ValidatePlacement(Candidate).bIsValid);
    }
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(3500, 7900)));
    Drain();
    TestEqual(TEXT("Overlapping queue previews remain unpurchased while held"), Count(Queued.Get()), 0);
    TestTrue(TEXT("Release selects a mutually compatible queued subset"), Build->CommitDragPlacement());
    TestEqual(TEXT("The first and later non-overlapping queue facilities install"), Count(Queued.Get()), 2);
    TestNotNull(TEXT("A valid queued candidate after rejected neighbors is retained"),
        Zones->FindFacilityAtWorldPoint(CursorAt(FIntPoint(3500, 7900)), ZoneId));
    TestEqual(TEXT("Rejected overlapping queue candidates are free"), Economy->GetGold(), GoldBeforeQueued - 2 * InstallCost);
    TestTrue(TEXT("The queued subset is one undo entry"), Build->UndoBuildEdit());
    TestEqual(TEXT("Queued subset undo removes every accepted member"), Count(Queued.Get()), 0);
    TestEqual(TEXT("Queued subset undo refunds only the accepted cost"), Economy->GetGold(), GoldBeforeQueued);

    // Aggregate limits and affordability select a draw-order prefix that is
    // still executed as one atomic group, never as separate purchases.
    SelectAt(Limited.Get(), FIntPoint(1000, 8200));
    const int32 GoldBeforeLimit = Economy->GetGold();
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(2200, 8200)));
    Drain();
    TestEqual(TEXT("Previewing beyond an instance limit installs nothing"), Count(Limited.Get()), 0);
    TestTrue(TEXT("Four proposals install only the two allowed by the authored limit"), Build->CommitDragPlacement());
    TestEqual(TEXT("The instance limit is respected across the accepted subset"), Count(Limited.Get()), 2);
    TestEqual(TEXT("Limit-skipped candidates contribute no cost"), Economy->GetGold(), GoldBeforeLimit - 2 * InstallCost);
    TestNotNull(TEXT("The first limit-eligible candidate installs"),
        Zones->FindFacilityAtWorldPoint(CursorAt(FIntPoint(1000, 8200)), ZoneId));
    TestNotNull(TEXT("The second limit-eligible candidate installs"),
        Zones->FindFacilityAtWorldPoint(CursorAt(FIntPoint(1400, 8200)), ZoneId));
    TestTrue(TEXT("The limit-selected subset is one undo entry"), Build->UndoBuildEdit());
    TestEqual(TEXT("One undo removes both limit-selected facilities"), Count(Limited.Get()), 0);
    TestTrue(TEXT("The limit-selected subset is one redo entry"), Build->RedoBuildEdit());
    TestEqual(TEXT("One redo restores both facilities without exceeding the limit"), Count(Limited.Get()), 2);
    SelectAt(Limited.Get(), FIntPoint(2200, 8200));
    const int64 RevisionBeforeFullLimit = Zones->GetWorldRevision();
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(2600, 8200)));
    TestFalse(TEXT("A fully reached limit leaves no accepted candidates"), Build->CommitDragPlacement());
    TestEqual(TEXT("A fully reached limit charges nothing more"), Economy->GetGold(), GoldBeforeLimit - 2 * InstallCost);
    TestEqual(TEXT("A fully reached limit publishes no revision"), Zones->GetWorldRevision(), RevisionBeforeFullLimit);

    SelectAt(Road.Get(), FIntPoint(1000, 9000));
    TestTrue(TEXT("Fixture leaves money for exactly two facilities"),
        Economy->TrySpend(Economy->GetGold() - 2 * InstallCost, EFF_SpendReason::Promotion));
    const int32 BeforeMoney = Count(Road.Get());
    const int64 RevisionBeforeMoney = Zones->GetWorldRevision();
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(2200, 9000)));
    Drain();
    TestEqual(TEXT("Unaffordable previews cannot reserve money"), Economy->GetGold(), 2 * InstallCost);
    TestTrue(TEXT("Four candidates install only the two affordable positions"), Build->CommitDragPlacement());
    TestEqual(TEXT("The accepted subset never exceeds the available funds"), Count(Road.Get()), BeforeMoney + 2);
    TestEqual(TEXT("Only accepted facilities consume the available balance"), Economy->GetGold(), 0);
    TestEqual(TEXT("Only affordable accepted facilities publish revisions"), Zones->GetWorldRevision(), RevisionBeforeMoney + 2);
    TestTrue(TEXT("One undo removes the affordable subset"), Build->UndoBuildEdit());
    TestEqual(TEXT("Affordable-subset undo restores the complete balance"), Economy->GetGold(), 2 * InstallCost);
    TestTrue(TEXT("One redo restores the affordable subset"), Build->RedoBuildEdit());
    TestEqual(TEXT("Affordable-subset redo charges only the accepted members"), Economy->GetGold(), 0);

    // An empty accepted set must not create a construction history entry at all.
    Zones->ClearConstructionHistory();
    SelectAt(Road.Get(), FIntPoint(3000, 9000));
    const int64 RevisionBeforeNoFunds = Zones->GetWorldRevision();
    Build->BeginDragPlacement();
    Build->UpdateDragPlacement(CursorAt(FIntPoint(4200, 9000)));
    Drain();
    TestFalse(TEXT("No funds leaves no accepted candidates"), Build->CommitDragPlacement());
    TestFalse(TEXT("An empty accepted subset creates no undo history"), Zones->CanUndoConstruction());
    TestEqual(TEXT("An empty accepted subset leaves the wallet unchanged"), Economy->GetGold(), 0);
    TestEqual(TEXT("An empty accepted subset leaves the revision unchanged"), Zones->GetWorldRevision(), RevisionBeforeNoFunds);
    TestEqual(TEXT("An entirely unaffordable proposal reports the actual failure"),
        Build->PlacementFailureFeedback->GetFailureType(), EFF_BuildPlacementFailureType::InsufficientGold);
    Economy->AddRevenue(10 * InstallCost, EFF_EarnReason::DailyGoalBonus);
    Build->UpdateDragPlacement(CursorAt(FIntPoint(2600, 9000)));
    Drain();
    TestEqual(TEXT("Later income cannot revive rejected candidates"), Count(Road.Get()), BeforeMoney + 2);
    TestEqual(TEXT("Later income requires a new gesture to spend"), Economy->GetGold(), 10 * InstallCost);
    return true;
}

#endif
