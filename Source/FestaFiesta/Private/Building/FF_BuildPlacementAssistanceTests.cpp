// FF_BuildPlacementAssistanceTests.cpp
// 역할: 다음 기능의 정상 동작과 오류 처리를 자동 검사한다: 설치 실패 원인을 안내하고 가까운 유효 위치를 나누어 탐색한다.

// Contracts for bounded suggestions and preview-only placement assistance.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildCatalogModel.h"
#include "Building/FF_BuildingSettings.h"
#include "Building/Presentation/FF_BuildPlacementFailureFeedback.h"
#include "Building/Presentation/FF_BuildPlacementGuidance.h"
#include "Components/SplineComponent.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "FF_FFZSBuildArea.h"
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
#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildPlacementAssistanceOffsetsTest,
    "FestaFiesta.Building.BuildMode.PlacementAssistance.BoundedSnappedCandidates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildPlacementAssistanceOffsetsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    for (const int32 RequestedSnap : { 1, 25, 100, 0, -1, MIN_int32, MAX_int32 })
    {
        const auto Offsets = FFBuildPlacementAssistance::MakeNearbyOffsets(RequestedSnap);
        const FString Context = FString::Printf(TEXT("Snap %d"), RequestedSnap);
        TestTrue(*(Context + TEXT(": a search has a finite, small candidate budget")),
            Offsets.Num() > 0 && Offsets.Num() <= 64);
        TSet<FIntPoint> Seen;
        int64 PreviousDistanceSquared = 0;
        for (const FIntPoint Offset : Offsets)
        {
            const int64 DistanceSquared = static_cast<int64>(Offset.X) * Offset.X + static_cast<int64>(Offset.Y) * Offset.Y;
            TestTrue(*(Context + TEXT(": candidates stay inside the advertised 6m circle")),
                DistanceSquared > 0 && DistanceSquared <= 600LL * 600LL);
            TestTrue(*(Context + TEXT(": nearer candidates are tried before farther ones")),
                DistanceSquared >= PreviousDistanceSquared);
            TestFalse(*(Context + TEXT(": no candidate consumes the validation budget twice")), Seen.Contains(Offset));
            if (RequestedSnap == 1 || RequestedSnap == 25 || RequestedSnap == 100)
                TestTrue(*(Context + TEXT(": suggestions remain on the user's snap lattice")),
                    Offset.X % RequestedSnap == 0 && Offset.Y % RequestedSnap == 0);
            Seen.Add(Offset);
            PreviousDistanceSquared = DistanceSquared;
        }
        TestTrue(*(Context + TEXT(": searches cover both sides of each axis")),
            Offsets.ContainsByPredicate([](FIntPoint P) { return P.X > 0 && P.Y == 0; }) &&
            Offsets.ContainsByPredicate([](FIntPoint P) { return P.X < 0 && P.Y == 0; }) &&
            Offsets.ContainsByPredicate([](FIntPoint P) { return P.X == 0 && P.Y > 0; }) &&
            Offsets.ContainsByPredicate([](FIntPoint P) { return P.X == 0 && P.Y < 0; }));
        TestTrue(*(Context + TEXT(": identical input produces a stable suggestion order")),
            Offsets == FFBuildPlacementAssistance::MakeNearbyOffsets(RequestedSnap));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildPlacementAssistanceReasonsTest,
    "FestaFiesta.Building.BuildMode.PlacementAssistance.ActionableReasons",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildPlacementAssistanceReasonsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using EReason = EFFZSPlacementFailure;
    const TArray<EReason> Spatial = { EReason::OutsideZone, EReason::InvalidGround, EReason::ExcessSlope,
        EReason::HeightVarianceExceeded, EReason::PlacementCollision, EReason::InsufficientClearance,
        EReason::QueueFootprintBlocked, EReason::ProtectedRouteBlocked, EReason::AccessibilityRouteBlocked,
        EReason::RequiredRouteDisconnected };
    const FText Generic = FFBuildPlacementAssistance::ReasonTitle(EReason::InternalError);
    TestEqual(TEXT("The legacy clearance failure now describes reserved audience space"),
        FFBuildPlacementAssistance::ReasonTitle(EReason::InsufficientClearance).ToString(), FString(TEXT("관람 공간과 겹쳐요")));
    for (const auto Reason : Spatial)
    {
        const FString Context = StaticEnum<EReason>()->GetNameStringByValue(static_cast<int64>(Reason));
        const FText Title = FFBuildPlacementAssistance::ReasonTitle(Reason);
        TestTrue(*(Context + TEXT(": spatial diagnostics have a specific visible title")),
            !Title.IsEmpty() && !Title.EqualTo(Generic));
        TestTrue(*(Context + TEXT(": moving can resolve this spatial rejection")),
            FFBuildPlacementAssistance::CanResolveByMoving({Reason}));
    }
    TestTrue(TEXT("Multiple spatial problems can be resolved by the same authority-validated suggestion"),
        FFBuildPlacementAssistance::CanResolveByMoving(Spatial));
    TestFalse(TEXT("A valid preview does not offer an unnecessary nearby search"),
        FFBuildPlacementAssistance::CanResolveByMoving({}));
    for (const auto Reason : { EReason::InsufficientFunds, EReason::BuildLimitReached, EReason::WrongStage,
        EReason::FacilityOccupied, EReason::InvalidFacilityState, EReason::DataInvalid, EReason::StaleWorldRevision,
        EReason::NavigationUnavailable, EReason::ExternalPolicyRejected, EReason::ZoneNotFound, EReason::CategoryNotAllowed })
    {
        const FString Context = StaticEnum<EReason>()->GetNameStringByValue(static_cast<int64>(Reason));
        TestFalse(*(Context + TEXT(": moving cannot bypass a non-position requirement")),
            FFBuildPlacementAssistance::CanResolveByMoving({Reason}));
        TestFalse(*(Context + TEXT(": a spatial primary reason cannot hide a blocking secondary reason")),
            FFBuildPlacementAssistance::CanResolveByMoving({EReason::PlacementCollision, Reason}));
        TestFalse(*(Context + TEXT(": the search gate is independent of reason ordering")),
            FFBuildPlacementAssistance::CanResolveByMoving({Reason, EReason::OutsideZone}));
        TestFalse(*(Context + TEXT(": the player always receives a title")),
            FFBuildPlacementAssistance::ReasonTitle(Reason).IsEmpty());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildPlacementAssistanceIntegrationTest,
    "FestaFiesta.Building.BuildMode.PlacementAssistance.PreviewAuthority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildPlacementAssistanceIntegrationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    auto* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    // Empty test terrain has no physical floor. Occupancy, zone bounds, phase and economy stay real.
    TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
    TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
    TGuardValue<bool> Navigation(Settings->bEnableNavigationProjectionChecks, false);
    TGuardValue<bool> Domains(Settings->bRequireProductionPolicyDomains, false);
    TGuardValue<EFFZSChangePhase> Phase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    auto* Game = World->GetGameInstance();
    auto* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
    auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
    auto* Economy = Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Production build subsystem"), Build) || !TestNotNull(TEXT("Real zone authority"), Zones) ||
        !TestNotNull(TEXT("Real festival economy"), Economy)) return false;
    ON_SCOPE_EXIT
    {
        Build->ResetPlacementAssistance();
        Build->DestroyPlacementPreview();
        Build->State = EFF_BuildModeState::Closed;
    };
    TStrongObjectPtr<UDataTable> Costs(NewObject<UDataTable>(Game));
    Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
    FFF_FacilityCostRow Cost;
    Cost.InstallCost = 100;
    Costs->AddRow(TEXT("Cost.Road.Default"), Cost);
    TStrongObjectPtr<UFF_FestivalDefinition> Festival(NewObject<UFF_FestivalDefinition>(Game));
    Festival->FestivalId = TEXT("BuildPlacementAssistanceAutomation");
    Festival->FacilityCostTable = Costs.Get();
    Festival->InitialGold = 10000;
    Economy->StartFestival(Festival.Get());
    Zones->SetChangePhase(EFFZSChangePhase::InitialSetup);
    const FName ZoneId(FFProjectZoneTest::ZoneId());
    auto* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("Real construction zone"), Zone) ||
        !TestTrue(TEXT("Zone accepts the existing project preset"), Zone->ApplyAcceptedZonePreset(ZoneId))) return false;
    Zone->BoundarySpline->ClearSplinePoints(false);
    for (const FVector Corner : { FVector(0, 0, 0), FVector(2000, 0, 0), FVector(2000, 2000, 0), FVector(0, 2000, 0) })
    {
        const int32 Index = Zone->BoundarySpline->GetNumberOfSplinePoints();
        Zone->BoundarySpline->AddSplinePoint(Corner, ESplineCoordinateSpace::Local, false);
        Zone->BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    Zone->BoundarySpline->SetClosedLoop(true, true);
    if (!TestTrue(TEXT("Authority registers the fixture's explicit bounds"), Zones->RegisterZone(Zone))) return false;
    FFFZSFacilitySpec Spec;
    Spec.FacilityDefinitionId = TEXT("Road_Surface_AssistanceTest");
    Spec.DisplayName = FText::FromString(TEXT("배치 보조 테스트 바닥"));
    Spec.Category = EFFZSFacilityCategory::Road;
    Spec.FootprintSizeCells = FIntPoint(100, 100);
    Spec.ClearanceCells = 0;
    Spec.QueueRules.MaxWaitingAgents = 0;
    Spec.QueueRules.ServiceCapacity = 0;
    Spec.PlacementObstacleHeightCm = 8.f;
    Spec.InstallCostKey = TEXT("Cost.Road.Default");
    const auto InstallAt = [&](FIntPoint Origin)
    {
        FFFZSGridPlacement Placement;
        Placement.OriginCell = Origin;
        return UFFZSGridLibrary::MakeInstallCommand(Spec, ZoneId, Placement, Zones->GetWorldRevision());
    };
    const auto InitialInstall = InstallAt(FIntPoint(800, 800));
    const auto InitialResult = Zones->ExecuteCommand(InitialInstall);
    if (!TestTrue(*FString::Printf(TEXT("Fixture installs through real economy/authority: %s"), *InitialResult.Message.ToString()),
        InitialResult.bSucceeded)) return false;
    auto* Facility = Zones->FindFacility(InitialInstall.FacilityInstanceId);
    if (!TestNotNull(TEXT("The original installed facility"), Facility)) return false;
    Build->SpawnPlacementPreview();
    if (!TestNotNull(TEXT("The real placement preview"), Build->PlacementPreview.Get())) return false;
    Build->State = EFF_BuildModeState::Moving;
    Build->SelectedSpec = Spec;
    Build->SelectedFacilityDefinitionId = Spec.FacilityDefinitionId;
    Build->MovingFacilityInstanceId = InitialInstall.FacilityInstanceId;
    Build->MoveTransactionId = FGuid::NewGuid();
    Build->CurrentZoneId = ZoneId;
    Build->bHasPlacementTarget = true;
    Build->SetPreferredPositionSnapCm(100);
    const auto SelectOutside = [&]()
    {
        Build->ResetPlacementAssistance();
        Build->CurrentPlacement = FFFZSGridPlacement();
        Build->CurrentPlacement.OriginCell = FIntPoint(-100, 500);
        Build->RefreshCurrentPlacementValidation();
    };
    int32 ExpectedGold = Economy->GetGold();
    int64 ExpectedRevision = Zones->GetWorldRevision();
    int32 ExpectedCount = 1;
    const auto ExpectPreviewOnly = [&](const TCHAR* Context)
    {
        TestEqual(*FString::Printf(TEXT("%s: no payment"), Context), Economy->GetGold(), ExpectedGold);
        TestEqual(*FString::Printf(TEXT("%s: no authority command"), Context), Zones->GetWorldRevision(), ExpectedRevision);
        TestEqual(*FString::Printf(TEXT("%s: no installation"), Context), Zones->CountFacilitiesByDefinitionId(Spec.FacilityDefinitionId), ExpectedCount);
        TestEqual(*FString::Printf(TEXT("%s: original facility never moves before confirmation"), Context),
            Facility->GetGridPlacement().OriginCell, InitialInstall.Placement.OriginCell);
    };
    SelectOutside();
    if (!TestTrue(TEXT("The actual authority rejects the seeded position outside its zone"),
        Build->LastValidation.Reasons.Contains(EFFZSPlacementFailure::OutsideZone)) ||
        !TestTrue(TEXT("The actual spatial result offers nearby assistance"), Build->GetPlacementGuidance().bCanFindNearby)) return false;
    TestTrue(TEXT("A spatial placement failure teaches N where the player needs it"),
        Build->GetPlacementGuidance().Advice.ToString().Contains(TEXT("N으로 가까운 빈자리")));
    if (!Build->LastValidation.GetBoundaryFailureText().IsEmpty())
    {
        const FText AuthoritySummary = Build->LastValidation.Summary;
        const auto Guidance = Build->GetPlacementGuidance();
        TestEqual(TEXT("The actual preview names its failed region in the placement panel"),
            Guidance.Title.ToString(), Build->LastValidation.GetBoundaryFailureText().ToString());
        TestTrue(TEXT("Panel advice matches the clipped red hatching and internal-gap rule"),
            Guidance.Advice.ToString().Contains(TEXT("붉은 사선")) && Guidance.Advice.ToString().Contains(TEXT("빈틈")));
        TestTrue(TEXT("Reading panel guidance preserves the authority's first summary"),
            Build->LastValidation.Summary.EqualTo(AuthoritySummary));
    }
    {
        const FFFZSPlacementValidation Original = Build->LastValidation;
        Build->LastValidation = FFFZSPlacementValidation();
        Build->LastValidation.Reasons = {EFFZSPlacementFailure::PlacementCollision, EFFZSPlacementFailure::InsufficientClearance};
        FFFZSPlacementObstruction Terrain;
        Terrain.Kind = EFFZSPlacementObstructionKind::GroundSupport;
        Terrain.Reason = EFFZSPlacementFailure::InsufficientClearance;
        Terrain.DisplayName = FText::FromString(TEXT("관객석 바닥 높이 차가 너무 커요"));
        Terrain.Detail = FText::FromString(TEXT("바닥 높이 차 134 cm / 허용 80 cm"));
        Build->LastValidation.Obstructions.Add(Terrain);
        const auto TerrainGuidance = Build->GetPlacementGuidance(1);
        TestEqual(TEXT("Selected region terrain detail replaces the generic overlap title"),
            TerrainGuidance.Title.ToString(), Terrain.DisplayName.ToString());
        TestTrue(TEXT("Selected region terrain guidance retains numeric limits"),
            TerrainGuidance.Advice.ToString().Contains(TEXT("134 cm")) &&
            TerrainGuidance.Advice.ToString().Contains(TEXT("80 cm")));
        TestTrue(TEXT("Terrain limits are never listed as a colliding world obstacle"), TerrainGuidance.Obstruction.IsEmpty());
        TestFalse(TEXT("Selecting the independent footprint problem does not leak the terrain title"),
            Build->GetPlacementGuidance(0).Title.EqualTo(Terrain.DisplayName));
        Build->LastValidation = Original;
    }
    // The isolated world has no game viewport to take focus from an editor text
    // field. Preserve the real typing guard, but give this shortcut fixture an
    // explicit neutral focus state just like the drag/inspection input fixtures.
    const TSharedPtr<SWidget> PreviousKeyboardFocus = FSlateApplication::IsInitialized()
        ? FSlateApplication::Get().GetKeyboardFocusedWidget() : nullptr;
    ON_SCOPE_EXIT
    {
        if (FSlateApplication::IsInitialized())
        {
            if (PreviousKeyboardFocus.IsValid())
                FSlateApplication::Get().SetKeyboardFocus(PreviousKeyboardFocus);
            else
                FSlateApplication::Get().ClearKeyboardFocus();
            const TSharedPtr<SWidget> RestoredFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
            AddInfo(FString::Printf(TEXT("Nearby shortcut fixture restored external Slate focus: %s"),
                RestoredFocus.IsValid() ? *RestoredFocus->ToString() : TEXT("None")));
        }
    };
    if (FSlateApplication::IsInitialized())
    {
        AddInfo(FString::Printf(TEXT("Nearby shortcut fixture inherited Slate focus: type=%s tag=%s widget=%s"),
            PreviousKeyboardFocus.IsValid() ? *PreviousKeyboardFocus->GetTypeAsString() : TEXT("None"),
            PreviousKeyboardFocus.IsValid() ? *PreviousKeyboardFocus->GetTag().ToString() : TEXT("None"),
            PreviousKeyboardFocus.IsValid() ? *PreviousKeyboardFocus->ToString() : TEXT("None")));
        FSlateApplication::Get().ClearKeyboardFocus();
        if (!TestFalse(TEXT("Isolated shortcut fixture has no external editor keyboard focus"),
            FSlateApplication::Get().GetKeyboardFocusedWidget().IsValid())) return false;
    }
    for (const auto State : {EFF_BuildModeState::Closed, EFF_BuildModeState::BuildMode, EFF_BuildModeState::CatalogOpen})
    {
        TGuardValue<EFF_BuildModeState> InactivePreview(Build->State, State);
        TestFalse(TEXT("N cannot start nearby placement outside a facility preview"), Build->HandleNearbyPlacementShortcut(EKeys::N));
    }
    Build->ToggleDirectPlacementAdjustment();
    TestTrue(TEXT("Direct adjustment pins the preview"), Build->IsDirectPlacementAdjustment());
    Build->ToggleDirectPlacementAdjustment();
    TestFalse(TEXT("Toggling again releases the preview"), Build->IsDirectPlacementAdjustment());
    ExpectPreviewOnly(TEXT("Pin and release"));
    Build->NudgePlacement(FIntPoint(1, 0));
    TestTrue(TEXT("Manual adjustment revalidates the destination with the authority"),
        Build->IsCurrentPlacementValid() && Zones->ValidatePlacement(Build->PlacementPreview->PreviewCommand).bIsValid);
    ExpectPreviewOnly(TEXT("Manual adjustment"));
    SelectOutside();
    {
        TGuardValue<bool> Paused(Build->bInputSuspendedForPauseMenu, true);
        Build->ToggleDirectPlacementAdjustment();
        TestFalse(TEXT("Pause blocks direct adjustment changes"), Build->IsDirectPlacementAdjustment());
        TestFalse(TEXT("Pause blocks starting a suggestion search"), Build->FindNearbyPlacement());
        TestFalse(TEXT("Pause also blocks the N shortcut"), Build->HandleNearbyPlacementShortcut(EKeys::N));
    }
    TestFalse(TEXT("A different key cannot start nearby assistance"), Build->HandleNearbyPlacementShortcut(EKeys::M));
    TestFalse(TEXT("Modified N does not steal another shortcut"), Build->HandleNearbyPlacementShortcut(EKeys::N, true));
    TestFalse(TEXT("Holding N does not start repeated searches"), Build->HandleNearbyPlacementShortcut(EKeys::N, false, true));
    if (!TestTrue(TEXT("Shortcut search retains a real spatial rejection"), Build->GetPlacementGuidance().bCanFindNearby) ||
        !TestFalse(TEXT("Input guard checks do not start a search"), Build->IsFindingNearbyPlacement())) return false;
    if (!TestTrue(TEXT("N starts the real bounded nearby search"), Build->HandleNearbyPlacementShortcut(EKeys::N))) return false;
    TestTrue(TEXT("Searching pins the destination for later confirmation"), Build->IsDirectPlacementAdjustment());
    for (int32 Pass = 0; Pass < 128 && Build->IsFindingNearbyPlacement(); ++Pass) Build->TickPlacementAssistance();
    TestFalse(TEXT("A finite candidate search completes"), Build->IsFindingNearbyPlacement());
    TestTrue(TEXT("A found suggestion passes real validation and remains a preview"),
        Build->IsCurrentPlacementValid() && Zones->ValidatePlacement(Build->PlacementPreview->PreviewCommand).bIsValid);
    TestTrue(TEXT("A found suggestion explains the separate Enter confirmation step"),
        Build->GetPlacementAssistanceMessage().ToString().Contains(TEXT("Enter")));
    ExpectPreviewOnly(TEXT("Find nearby"));
    TestFalse(TEXT("N cannot search again at an already valid destination"), Build->HandleNearbyPlacementShortcut(EKeys::N));
    SelectOutside();
    Build->FindNearbyPlacement();
    Build->ToggleDirectPlacementAdjustment();
    TestFalse(TEXT("Releasing direct adjustment cancels an in-flight search"), Build->IsFindingNearbyPlacement());
    ExpectPreviewOnly(TEXT("Cancel search"));
    SelectOutside();
    if (!TestTrue(TEXT("A second search starts before an external world change"), Build->FindNearbyPlacement())) return false;
    const auto ExternalInstall = InstallAt(FIntPoint(1500, 1500));
    if (!TestTrue(TEXT("An independent authority command changes the world during search"), Zones->ExecuteCommand(ExternalInstall).bSucceeded)) return false;
    ExpectedGold = Economy->GetGold();
    ExpectedRevision = Zones->GetWorldRevision();
    ExpectedCount = 2;
    Build->TickPlacementAssistance();
    TestFalse(TEXT("A changed authority revision cancels old suggestions"), Build->IsFindingNearbyPlacement());
    TestEqual(TEXT("A stale search cannot move the preview to an unvalidated result"), Build->CurrentPlacement.OriginCell, FIntPoint(-100, 500));
    ExpectPreviewOnly(TEXT("Stale search"));
    // Target arithmetic must not wrap to the other side of the world before authority validation.
    for (const int32 Edge : { MIN_int32, MAX_int32 })
    {
        SelectOutside();
        if (!TestTrue(TEXT("Overflow guard fixture starts a real search"), Build->FindNearbyPlacement())) return false;
        Build->CurrentPlacement.OriginCell = FIntPoint(Edge, 500);
        Build->NearbySearchCommand.Placement.OriginCell = Build->CurrentPlacement.OriginCell;
        Build->NearbyOffsets = { FIntPoint(Edge < 0 ? -100 : 100, 0) };
        Build->TickPlacementAssistance();
        TestFalse(TEXT("Out-of-range candidate is skipped and the finite search finishes"), Build->IsFindingNearbyPlacement());
        TestEqual(TEXT("Suggestion arithmetic never wraps a grid coordinate"), Build->CurrentPlacement.OriginCell.X, Edge);
        ExpectPreviewOnly(TEXT("Overflow rejection"));
    }
    Zones->SetChangePhase(EFFZSChangePhase::LiveOperation);
    ExpectedRevision = Zones->GetWorldRevision();
    SelectOutside();
    TestTrue(TEXT("The real phase policy remains in the preview's diagnostic reasons"),
        Build->LastValidation.Reasons.Contains(EFFZSPlacementFailure::WrongStage));
    TestFalse(TEXT("An actual stage rejection blocks nearby assistance even outside the zone"), Build->FindNearbyPlacement());
    ExpectPreviewOnly(TEXT("Stage rejection"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildPlacementMemoryTest,
    "FestaFiesta.Building.BuildMode.PlacementAssistance.LastManipulatedPose",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildPlacementMemoryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    auto* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
    TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
    TGuardValue<bool> Navigation(Settings->bEnableNavigationProjectionChecks, false);
    TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
    TGuardValue<EFFZSChangePhase> Phase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
    UWorld* World = Fixture.GetTestWorld();
    UGameInstance* Game = World->GetGameInstance();
    auto* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
    auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
    auto* Catalog = Game ? Game->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
    auto* Economy = Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Production build subsystem"), Build) || !TestNotNull(TEXT("Real zone authority"), Zones) ||
        !TestNotNull(TEXT("Real facility catalog"), Catalog) || !TestNotNull(TEXT("Real festival economy"), Economy)) return false;
    ON_SCOPE_EXIT
    {
        Build->DestroyPlacementPreview();
        Build->ResetPlacementSelection();
        Build->State = EFF_BuildModeState::Closed;
    };

    // Keep asset discovery out of this isolated catalog. The actual StartPlacement,
    // preview validation and install command still use the production definitions.
    FFBuildCatalogModel::BeginNextRequest(Catalog->RequestGeneration);
    if (Catalog->ActiveLoadHandle.IsValid()) Catalog->ActiveLoadHandle->CancelHandle();
    Catalog->ActiveLoadHandle.Reset();
    Catalog->Entries.Reset();
    Catalog->EntryIndexByDefinitionId.Reset();
    TStrongObjectPtr<UFFZSFacilityDefinition> First(NewObject<UFFZSFacilityDefinition>(Game));
    TStrongObjectPtr<UFFZSFacilityDefinition> Second(NewObject<UFFZSFacilityDefinition>(Game));
    for (auto* Definition : { First.Get(), Second.Get() })
    {
        FFFZSFacilitySpec& Spec = Definition->Spec;
        Spec.FacilityDefinitionId = Definition == First.Get()
            ? FName(TEXT("Road_Surface_PoseFirst")) : FName(TEXT("Road_Surface_PoseSecond"));
        Spec.DisplayName = FText::FromString(TEXT("설치 위치 기억 테스트 바닥"));
        Spec.Category = EFFZSFacilityCategory::Road;
        Spec.FootprintSizeCells = FIntPoint(100, 200);
        Spec.ClearanceCells = 0;
        Spec.QueueRules.MaxWaitingAgents = 0;
        Spec.QueueRules.ServiceCapacity = 0;
        Spec.PlacementObstacleHeightCm = 8.f;
        Spec.InstallCostKey = TEXT("Cost.Road.Default");
        Definition->bEnableDragPlacement = true;
        const int32 Index = Catalog->Entries.AddDefaulted();
        Catalog->Entries[Index].Definition = Definition;
        Catalog->EntryIndexByDefinitionId.Add(Spec.FacilityDefinitionId, Index);
        TArray<FText> Errors;
        if (!TestTrue(TEXT("The fixture uses valid production facility definitions"), Definition->IsDefinitionValid(Errors)))
            return false;
    }

    TStrongObjectPtr<UDataTable> Costs(NewObject<UDataTable>(Game));
    Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
    FFF_FacilityCostRow Cost;
    Cost.InstallCost = 100;
    Costs->AddRow(TEXT("Cost.Road.Default"), Cost);
    TStrongObjectPtr<UFF_FestivalDefinition> Festival(NewObject<UFF_FestivalDefinition>(Game));
    Festival->FestivalId = TEXT("BuildPlacementMemoryAutomation");
    Festival->FacilityCostTable = Costs.Get();
    Festival->InitialGold = 10000;
    Economy->StartFestival(Festival.Get());
    Zones->SetChangePhase(EFFZSChangePhase::InitialSetup);

    const FName ZoneId(FFProjectZoneTest::ZoneId());
    auto* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("Construction region"), Zone) ||
        !TestTrue(TEXT("Accepted project region"), Zone->ApplyAcceptedZonePreset(ZoneId))) return false;
    Zone->BoundarySpline->ClearSplinePoints(false);
    for (const FVector Corner : { FVector(0, 0, 0), FVector(4000, 0, 0), FVector(4000, 4000, 0), FVector(0, 4000, 0) })
    {
        const int32 Index = Zone->BoundarySpline->GetNumberOfSplinePoints();
        Zone->BoundarySpline->AddSplinePoint(Corner, ESplineCoordinateSpace::Local, false);
        Zone->BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    Zone->BoundarySpline->SetClosedLoop(true, true);
    Zone->SetActorRotation(FRotator(0, 90, 0));
    if (!TestTrue(TEXT("Authority registers the rotated region"), Zones->RegisterZone(Zone))) return false;
    auto* BuildArea = World->SpawnActor<AFFZSBuildArea>();
    if (!TestNotNull(TEXT("Unified build area"), BuildArea) ||
        !TestTrue(TEXT("The production world-angle path is enabled"), Zones->RegisterBuildArea(BuildArea))) return false;

    const auto Reselect = [&](const UFFZSFacilityDefinition* Definition)
    {
        Build->DestroyPlacementPreview();
        Build->ResetPlacementSelection();
        Build->State = EFF_BuildModeState::CatalogOpen;
        return Build->StartPlacement(Definition->Spec.FacilityDefinitionId);
    };
    if (!TestTrue(TEXT("First selection starts through the real catalog entry point"), Reselect(First.Get()))) return false;
    TestFalse(TEXT("A never-used definition has no inherited position"), Build->HasPlacementTarget());
    TestFalse(TEXT("A never-used definition starts with cursor placement"), Build->IsDirectPlacementAdjustment());
    Build->CurrentZoneId = ZoneId;
    Build->CurrentPlacement.OriginCell = FIntPoint(800, 800);
    Build->bHasPlacementTarget = true;
    Build->SetPreferredPositionSnapCm(25);
    Build->RefreshCurrentPlacementValidation();
    if (!TestTrue(TEXT("The initial target passes real authority validation"), Build->IsCurrentPlacementValid())) return false;
    const int64 InitialRevision = Zones->GetWorldRevision();
    const int32 InitialGold = Economy->GetGold();
    Build->RotatePlacementDegrees(45.f);
    TestFalse(TEXT("Keyboard-style rotation keeps ordinary cursor placement available"), Build->IsDirectPlacementAdjustment());
    Build->SetPlacementYawDegrees(Build->GetPlacementYawDegrees());
    TestTrue(TEXT("An explicit angle-field edit pins the position even when the value is unchanged"), Build->IsDirectPlacementAdjustment());
    Build->ToggleDirectPlacementAdjustment();
    Build->NudgePlacement(FIntPoint(1, -1));
    TestTrue(TEXT("A manual position edit pins the preview until the user releases it"), Build->IsDirectPlacementAdjustment());
    TestEqual(TEXT("Position adjustment retains the selected 25cm step"), Build->CurrentPlacement.OriginCell, FIntPoint(825, 775));
    constexpr float WorldYaw = 37.25f;
    Build->SetPlacementYawDegrees(WorldYaw);
    Build->RefreshCurrentPlacementValidation();
    TestTrue(TEXT("An arbitrary world angle is kept on a rotated construction region"),
        FMath::Abs(FMath::FindDeltaAngleDegrees(Build->GetPlacementYawDegrees(), WorldYaw)) < 0.01f);
    const FFFZSGridPlacement ManipulatedPlacement = Build->CurrentPlacement;
    TestEqual(TEXT("Manipulating a preview does not execute an authority command"), Zones->GetWorldRevision(), InitialRevision);
    TestEqual(TEXT("Manipulating a preview does not charge the player"), Economy->GetGold(), InitialGold);

    // The source grid may move between uses. Reuse its integer position and the
    // user's world angle, then derive height and validation again from live data.
    Build->DestroyPlacementPreview();
    Build->ResetPlacementSelection();
    Zone->SetActorLocationAndRotation(FVector(0, 0, 180), FRotator(0, 270, 0));
    Zones->RefreshBuildArea();
    if (!TestTrue(TEXT("Reselecting a used definition restores its remembered pose"), Reselect(First.Get()))) return false;
    TestTrue(TEXT("The remembered pose is a pinned preview"), Build->HasPlacementTarget() && Build->IsDirectPlacementAdjustment());
    TestEqual(TEXT("Reselection preserves the last manually adjusted grid position"),
        Build->CurrentPlacement.OriginCell, ManipulatedPlacement.OriginCell);
    TestTrue(TEXT("A changed region rotation cannot rotate the user's saved world angle"),
        FMath::Abs(FMath::FindDeltaAngleDegrees(Build->GetPlacementYawDegrees(), WorldYaw)) < 0.01f);
    TestTrue(TEXT("Restoration derives the new local angle instead of copying the old local yaw"),
        FMath::Abs(FMath::FindDeltaAngleDegrees(UFFZSGridLibrary::YawCentidegreesToDegrees(
            UFFZSGridLibrary::GetEffectiveYawCentidegrees(Build->CurrentPlacement)), WorldYaw - 270.f)) < 0.01f);
    TestTrue(TEXT("The preview recomputes height from the live region"),
        FMath::IsNearlyEqual(Build->LastValidation.PlacementTransform.GetLocation().Z, 180.0, 0.01));
    TestEqual(TEXT("Restoration rebuilds the command with the current authority revision"),
        Build->PlacementPreview->PreviewCommand.ExpectedWorldRevision, Zones->GetWorldRevision());
    const FFFZSGridPlacement CommitPose = Build->CurrentPlacement;
    if (!TestTrue(TEXT("The pinned pose commits through the production install path"), Build->CommitPlacement())) return false;
    TestTrue(TEXT("Successful installation retains direct adjustment for the next preview"), Build->IsDirectPlacementAdjustment());
    TestTrue(TEXT("Successful installation retains the complete manipulated pose"), Build->CurrentPlacement == CommitPose);
    TestEqual(TEXT("One confirmation installs exactly one facility"), Zones->GetPlacedFacilityCount(), 1);
    TestEqual(TEXT("One confirmation charges exactly one install cost"), Economy->GetGold(), InitialGold - Cost.InstallCost);

    if (!TestTrue(TEXT("A different definition can be selected"), Reselect(Second.Get()))) return false;
    TestFalse(TEXT("Another definition cannot inherit the first definition's position"), Build->HasPlacementTarget());
    TestTrue(TEXT("Another definition cannot inherit the first definition's angle"), FMath::IsNearlyZero(Build->GetPlacementYawDegrees()));
    if (!TestTrue(TEXT("Returning to the first definition restores its own pose"), Reselect(First.Get()))) return false;
    TestTrue(TEXT("The installed location is restored without silently offsetting it"), Build->CurrentPlacement == CommitPose);
    TestFalse(TEXT("A reused occupied location is validated again instead of retaining an old green result"), Build->IsCurrentPlacementValid());
    TestTrue(TEXT("Authority occupancy remains visible after restoration"),
        Build->LastValidation.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    const auto& FeedbackAsset = GetDefault<UFF_BuildingSettings>()->PlacementFailureFeedbackClass;
    UClass* FeedbackClass = FeedbackAsset.LoadSynchronous();
    if (!TestNotNull(TEXT("The configured production failure-feedback WBP loads successfully"), FeedbackClass) ||
        !TestTrue(TEXT("The configured WBP derives from the production failure presenter"),
            FeedbackClass->IsChildOf(UFF_BuildPlacementFailureFeedback::StaticClass())) ||
        !TestNull(TEXT("This authority fixture intentionally has no local player controller"), Build->GetLocalPlayerController()))
        return false;
    // Production logs class-load failure and missing presentation owner through
    // the same branch. The class assertions above isolate this single expected
    // headless UI error without hiding a broken configured feedback asset.
    AddExpectedError(FString::Printf(TEXT("건축 실패 알림 WBP를 사용할 수 없습니다: %s"),
        *FeedbackAsset.ToSoftObjectPath().ToString()), EAutomationExpectedErrorFlags::Contains, 1);
    TestFalse(TEXT("Confirming the occupied remembered location cannot purchase a duplicate"), Build->CommitPlacement());
    TestTrue(TEXT("A rejected confirmation retains the user's chosen pose and adjustment mode"),
        Build->CurrentPlacement == CommitPose && Build->IsDirectPlacementAdjustment());
    TestEqual(TEXT("A rejected duplicate leaves the installation count unchanged"), Zones->GetPlacedFacilityCount(), 1);
    TestEqual(TEXT("A rejected duplicate leaves the player's money unchanged"), Economy->GetGold(), InitialGold - Cost.InstallCost);

    Build->ToggleDirectPlacementAdjustment();
    TestFalse(TEXT("T explicitly releases a remembered pose back to cursor placement"), Build->IsDirectPlacementAdjustment());
    TestTrue(TEXT("Cursor mode retains the selected definition's continuous placement support"), Build->IsDragPlacementEnabled());
    Build->ToggleDirectPlacementAdjustment();
    TestTrue(TEXT("The user can pin the preview again"), Build->IsDirectPlacementAdjustment());

    Build->State = EFF_BuildModeState::Moving;
    Build->CurrentPlacement.OriginCell = FIntPoint(2400, 2400);
    Build->CurrentPlacement.YawCentidegrees = 12000;
    Build->RememberPlacementPose();
    if (!TestTrue(TEXT("Returning from an existing-facility edit can reselect the definition"), Reselect(First.Get()))) return false;
    TestTrue(TEXT("Moving an existing instance cannot replace a new-installation pose"), Build->CurrentPlacement == CommitPose);

    Build->DestroyPlacementPreview();
    Build->ResetPlacementSelection();
    if (!TestTrue(TEXT("The fixture can unregister its coordinate region"), Zones->UnregisterZone(Zone))) return false;
    if (!TestTrue(TEXT("An unavailable saved region still permits a fresh placement session"), Reselect(First.Get()))) return false;
    TestFalse(TEXT("An unavailable saved region cannot produce a phantom placement target"), Build->HasPlacementTarget());
    TestFalse(TEXT("A missing coordinate region falls back to cursor placement"), Build->IsDirectPlacementAdjustment());
    TestTrue(TEXT("Losing a coordinate region still preserves the last user world angle"),
        FMath::Abs(FMath::FindDeltaAngleDegrees(Build->GetPlacementYawDegrees(), WorldYaw)) < 0.01f);
    return true;
}

#endif
