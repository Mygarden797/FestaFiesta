// Exercises the persistent presentation through public APIs in an isolated editor world.
// Ground/collision are disabled here: spatial mask/terrain tests own those rules, while these
// tests protect show/hide, re-entry, component reuse and registered-zone lifetime behavior.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSBuildableGridActor.h"
#include "FF_FFZSBuildableGridComponent.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace
{
    class FScopedGridPresentationSettings
    {
    public:
        FScopedGridPresentationSettings()
            : Settings(GetMutableDefault<UFFZSDeveloperSettings>())
            , bGround(Settings->bEnableGroundChecks)
            , bCollision(Settings->bEnableWorldCollisionChecks)
            , bRestrictZoneIds(Settings->bRestrictZoneIdsToProjectPresets)
            , GridRadius(Settings->PlacementGridMaxRadiusCm)
        {
            Settings->bEnableGroundChecks = false;
            Settings->bEnableWorldCollisionChecks = false;
            Settings->bRestrictZoneIdsToProjectPresets = false;
            Settings->PlacementGridMaxRadiusCm = 2000.0f;
        }

        ~FScopedGridPresentationSettings()
        {
            Settings->bEnableGroundChecks = bGround;
            Settings->bEnableWorldCollisionChecks = bCollision;
            Settings->bRestrictZoneIdsToProjectPresets = bRestrictZoneIds;
            Settings->PlacementGridMaxRadiusCm = GridRadius;
        }

    private:
        UFFZSDeveloperSettings* Settings;
        bool bGround;
        bool bCollision;
        bool bRestrictZoneIds;
        float GridRadius;
    };

    AFFZSGameplayZone* SpawnGridPresentationZone(UWorld& World, const FName ZoneId,
        const FVector Location = FVector::ZeroVector, const double SideCm = 1000.0)
    {
        AFFZSGameplayZone* Zone = World.SpawnActor<AFFZSGameplayZone>();
        if (!Zone || !Zone->ApplyAcceptedZonePreset(ZoneId))
        {
            return nullptr;
        }
        Zone->SetActorLocation(Location);
        const FVector Corners[] = {
            FVector(0.0, 0.0, 0.0), FVector(SideCm, 0.0, 0.0),
            FVector(SideCm, SideCm, 0.0), FVector(0.0, SideCm, 0.0)
        };
        Zone->BoundarySpline->ClearSplinePoints(false);
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Corners); ++Index)
        {
            Zone->BoundarySpline->AddSplinePoint(Corners[Index], ESplineCoordinateSpace::Local, false);
            Zone->BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
        }
        Zone->BoundarySpline->SetClosedLoop(true, true);
        return Zone;
    }

    // Observe submitted geometry rather than calling the mask again: this catches lost
    // placement context, stale component caches and accidental hide/show at the actor layer.
    bool HasOverviewLineAt(const AFFZSBuildableGridActor& Overview, const FVector2D& WorldPoint)
    {
        if (Overview.IsHidden()) return false;
        TArray<UFFZSBuildableGridComponent*> Grids;
        Overview.GetComponents(Grids);
        for (const UFFZSBuildableGridComponent* Grid : Grids)
        {
            if (!Grid || Grid->bHiddenInGame) continue;
            for (const FBatchedLine& Line : Grid->BatchedLines)
            {
                const FVector2D Start(Line.Start.X, Line.Start.Y);
                const FVector2D End(Line.End.X, Line.End.Y);
                const FVector2D Delta = End - Start;
                const double LengthSquared = Delta.SizeSquared();
                if (LengthSquared <= UE_SMALL_NUMBER) continue;
                const double T = FMath::Clamp(FVector2D::DotProduct(WorldPoint - Start, Delta) / LengthSquared, 0.0, 1.0);
                if (FVector2D::DistSquared(WorldPoint, Start + Delta * T) < 0.01) return true;
            }
        }
        return false;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSBuildableGridComponentLifecycleTest,
    "FF_ZoneSystem.Integration.BuildableGrid.ComponentLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildableGridComponentLifecycleTest::RunTest(const FString& Parameters)
{
    FScopedGridPresentationSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Editor world"), World))
    {
        return false;
    }
    UFFZSWorldSubsystem* System = World->GetSubsystem<UFFZSWorldSubsystem>();
    AFFZSGameplayZone* Zone = SpawnGridPresentationZone(*World, TEXT("GridPresentation"));
    if (!TestNotNull(TEXT("World subsystem"), System) || !TestNotNull(TEXT("Zone"), Zone) ||
        !TestTrue(TEXT("Zone registers"), System->RegisterZone(Zone)))
    {
        return false;
    }
    AActor* Owner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Presentation owner"), Owner))
    {
        return false;
    }
    UFFZSBuildableGridComponent* Grid = NewObject<UFFZSBuildableGridComponent>(Owner);
    Owner->SetRootComponent(Grid);
    Owner->AddInstanceComponent(Grid);
    Grid->RegisterComponent();
    FFFZSCommand Overview;
    Overview.ZoneId = Zone->ZoneId;
    const FVector Surface(500.0, 500.0, 0.0);
    const auto Refresh = [&]()
    {
        Grid->RefreshGrid(*System, Overview, FVector2D(500.0), FVector2D(400.0),
            100, Surface, FVector::UpVector);
    };

    TestFalse(TEXT("Persistent grid does not tick"), Grid->PrimaryComponentTick.bCanEverTick);
    TestEqual(TEXT("Presentation has no collision"), Grid->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    Refresh();
    if (!TestTrue(TEXT("Buildable land produces lines"), Grid->BatchedLines.Num() > 0))
    {
        return false;
    }
    TestFalse(TEXT("Buildable land is visible"), Grid->bHiddenInGame);
    const int32 OriginalLineCount = Grid->BatchedLines.Num();
    const FVector OriginalStart = Grid->BatchedLines[0].Start;
    const FVector OriginalEnd = Grid->BatchedLines[0].End;
    const uint64 OriginalPresentationRevision = Grid->GetPresentationRevision();
    Refresh();
    TestEqual(TEXT("Repeated cached refresh does not append duplicate lines"), Grid->BatchedLines.Num(), OriginalLineCount);
    TestTrue(TEXT("Stable context keeps the same first segment"),
        Grid->BatchedLines[0].Start.Equals(OriginalStart) && Grid->BatchedLines[0].End.Equals(OriginalEnd));
    FPlatformProcess::Sleep(0.55f);
    Refresh();
    TestEqual(TEXT("Background scenery checks do not republish an unchanged grid"),
        Grid->GetPresentationRevision(), OriginalPresentationRevision);
    Grid->InvalidateGrid();
    Refresh();
    TestEqual(TEXT("Even an explicit recheck retains the render proxy when its result is identical"),
        Grid->GetPresentationRevision(), OriginalPresentationRevision);

    Grid->HideGrid();
    TestTrue(TEXT("Hide clears persisted lines"), Grid->BatchedLines.IsEmpty());
    TestTrue(TEXT("Hide marks component hidden"), Grid->bHiddenInGame);
    Refresh();
    TestEqual(TEXT("Same context after hide rebuilds all lines"), Grid->BatchedLines.Num(), OriginalLineCount);
    TestFalse(TEXT("Same context after hide is visible again"), Grid->bHiddenInGame);

    const int64 RevisionBeforeProtection = System->GetWorldRevision();
    AFFZSProtectedArea* Protection = World->SpawnActor<AFFZSProtectedArea>();
    if (!TestNotNull(TEXT("Protected area"), Protection))
    {
        return false;
    }
    Protection->ProtectedAreaId = TEXT("GridPresentationNoBuild");
    Protection->LinkedZoneId = Zone->ZoneId;
    Protection->AreaType = EFFZSProtectedAreaType::NoBuild;
    Protection->AreaBounds->SetBoxExtent(FVector(600.0, 600.0, 300.0));
    Protection->SetActorLocation(FVector(500.0, 500.0, 100.0));
    if (!TestTrue(TEXT("Protection registers"), System->RegisterProtectedArea(Protection)))
    {
        return false;
    }
    Grid->InvalidateGrid();
    Refresh();
    TestTrue(TEXT("Invalidation removes all lines inside a newly protected region"), Grid->BatchedLines.IsEmpty());
    TestTrue(TEXT("An actual protected-area change still publishes updated geometry"),
        Grid->GetPresentationRevision() > OriginalPresentationRevision);
    TestTrue(TEXT("Fully excluded grid is hidden"), Grid->bHiddenInGame);
    TestEqual(TEXT("Presentation and protection registration do not commit a building"),
        System->GetWorldRevision(), RevisionBeforeProtection);

    TestTrue(TEXT("Protection unregisters"), System->UnregisterProtectedArea(Protection));
    Grid->InvalidateGrid();
    Refresh();
    TestEqual(TEXT("Removing protection restores the grid after invalidation"), Grid->BatchedLines.Num(), OriginalLineCount);

    Grid->RefreshGrid(*System, Overview, FVector2D(500.0), FVector2D(400.0),
        100, Surface, FVector::ForwardVector);
    TestTrue(TEXT("Invalid surface clears previously valid lines"), Grid->BatchedLines.IsEmpty());
    TestTrue(TEXT("Invalid surface is hidden"), Grid->bHiddenInGame);
    Refresh();
    TestTrue(TEXT("Returning to usable ground restores lines"), !Grid->BatchedLines.IsEmpty());
    TestTrue(TEXT("Zone unregisters"), System->UnregisterZone(Zone));
    Refresh();
    TestTrue(TEXT("Removed zone cannot leave a cached grid behind"), Grid->BatchedLines.IsEmpty());
    TestTrue(TEXT("Removed zone grid is hidden"), Grid->bHiddenInGame);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSBuildableGridOverviewLifecycleTest,
    "FF_ZoneSystem.Integration.BuildableGrid.OverviewLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildableGridOverviewLifecycleTest::RunTest(const FString& Parameters)
{
    FScopedGridPresentationSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Editor world"), World))
    {
        return false;
    }
    UFFZSWorldSubsystem* System = World->GetSubsystem<UFFZSWorldSubsystem>();
    AFFZSGameplayZone* First = SpawnGridPresentationZone(*World, TEXT("GridOverviewA"));
    AFFZSGameplayZone* Second = SpawnGridPresentationZone(*World, TEXT("GridOverviewB"), FVector(1500.0, 0.0, 0.0));
    AFFZSBuildableGridActor* Overview = World->SpawnActor<AFFZSBuildableGridActor>();
    if (!TestNotNull(TEXT("World subsystem"), System) || !TestNotNull(TEXT("First zone"), First) ||
        !TestNotNull(TEXT("Second zone"), Second) || !TestNotNull(TEXT("Overview actor"), Overview))
    {
        return false;
    }
    if (!TestTrue(TEXT("First zone registers"), System->RegisterZone(First)) ||
        !TestTrue(TEXT("Second zone registers"), System->RegisterZone(Second)))
    {
        return false;
    }
    const FVector Focus(1250.0, 500.0, 0.0);
    TestFalse(TEXT("Overview actor does not tick"), Overview->PrimaryActorTick.bCanEverTick);
    TestFalse(TEXT("Overview actor cannot block placement"), Overview->GetActorEnableCollision());
    Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100);
    TArray<UFFZSBuildableGridComponent*> OriginalComponents;
    Overview->GetComponents(OriginalComponents);
    if (!TestEqual(TEXT("Each nearby registered zone receives one component"), OriginalComponents.Num(), 2))
    {
        return false;
    }
    for (const UFFZSBuildableGridComponent* Grid : OriginalComponents)
    {
        TestTrue(TEXT("Each nearby zone has visible grid lines"), !Grid->BatchedLines.IsEmpty() && !Grid->bHiddenInGame);
    }
    Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100);
    TArray<UFFZSBuildableGridComponent*> ReusedComponents;
    Overview->GetComponents(ReusedComponents);
    TestEqual(TEXT("Refreshing overview does not allocate extra components"), ReusedComponents.Num(), 2);
    for (UFFZSBuildableGridComponent* Grid : OriginalComponents)
    {
        TestTrue(TEXT("Registered zones reuse the original component"), ReusedComponents.Contains(Grid));
    }

    Overview->HideGrid();
    TestTrue(TEXT("Closing overview hides its actor"), Overview->IsHidden());
    for (const UFFZSBuildableGridComponent* Grid : OriginalComponents)
    {
        TestTrue(TEXT("Closing overview clears all zone lines"), Grid->BatchedLines.IsEmpty() && Grid->bHiddenInGame);
    }
    Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100);
    TestFalse(TEXT("Reopening unhides overview actor"), Overview->IsHidden());
    for (const UFFZSBuildableGridComponent* Grid : OriginalComponents)
    {
        TestTrue(TEXT("Reopening rebuilds the original zone components"), !Grid->BatchedLines.IsEmpty());
    }

    Overview->UpdateGrid(*System, FVector(100000.0, 100000.0, 0.0), FVector::UpVector, 100);
    for (const UFFZSBuildableGridComponent* Grid : OriginalComponents)
    {
        TestTrue(TEXT("Leaving the zone window clears distant lines"), Grid->BatchedLines.IsEmpty() && Grid->bHiddenInGame);
    }
    Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100);
    TestTrue(TEXT("First zone unregisters"), System->UnregisterZone(First));
    Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100);
    TArray<UFFZSBuildableGridComponent*> RemainingComponents;
    Overview->GetComponents(RemainingComponents);
    TestEqual(TEXT("Removed zone releases its component"), RemainingComponents.Num(), 1);
    if (RemainingComponents.Num() == 1)
    {
        TestTrue(TEXT("Remaining registered zone keeps its existing component"), OriginalComponents.Contains(RemainingComponents[0]));
        TestTrue(TEXT("Remaining registered zone remains visible"), !RemainingComponents[0]->BatchedLines.IsEmpty());
    }
    TestTrue(TEXT("Second zone unregisters"), System->UnregisterZone(Second));
    Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100);
    Overview->GetComponents(RemainingComponents);
    TestTrue(TEXT("No registered zones leaves no grid components"), RemainingComponents.IsEmpty());
    TestEqual(TEXT("Overview never changes authority world revision"), System->GetWorldRevision(), int64(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSBuildableGridOverviewPlacementContextTest,
    "FF_ZoneSystem.Integration.BuildableGrid.OverviewPlacementContext",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildableGridOverviewPlacementContextTest::RunTest(const FString& Parameters)
{
    FScopedGridPresentationSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Editor world"), World)) return false;
    UFFZSWorldSubsystem* System = World->GetSubsystem<UFFZSWorldSubsystem>();
    AFFZSGameplayZone* BoothZone = SpawnGridPresentationZone(*World, TEXT("GridContextBooth"));
    AFFZSGameplayZone* ToiletZone = SpawnGridPresentationZone(*World, TEXT("GridContextToilet"), FVector(1500.0, 0.0, 0.0));
    AFFZSBuildableGridActor* Overview = World->SpawnActor<AFFZSBuildableGridActor>();
    if (!TestNotNull(TEXT("World system"), System) || !TestNotNull(TEXT("Booth zone"), BoothZone) ||
        !TestNotNull(TEXT("Toilet zone"), ToiletZone) || !TestNotNull(TEXT("Overview"), Overview)) return false;
    BoothZone->bAllowAllFacilityCategories = false;
    BoothZone->AllowedFacilityCategories = {EFFZSFacilityCategory::Booth};
    ToiletZone->bAllowAllFacilityCategories = false;
    ToiletZone->AllowedFacilityCategories = {EFFZSFacilityCategory::Toilet};
    if (!TestTrue(TEXT("Booth zone registers"), System->RegisterZone(BoothZone)) ||
        !TestTrue(TEXT("Toilet zone registers"), System->RegisterZone(ToiletZone))) return false;

    const FVector Focus(1250.0, 500.0, 0.0);
    const FVector2D BoothPoint(500.0, 500.0);
    const FVector2D ToiletPoint(2000.0, 500.0);
    Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100);
    TestTrue(TEXT("Unselected overview includes both usable categories"),
        HasOverviewLineAt(*Overview, BoothPoint) && HasOverviewLineAt(*Overview, ToiletPoint));
    TArray<UFFZSBuildableGridComponent*> OriginalComponents;
    Overview->GetComponents(OriginalComponents);

    FFFZSCommand Context;
    Context.Type = EFFZSCommandType::Install;
    Context.ZoneId = TEXT("CursorOnlyZone"); // Actor must override this for each surrounding zone.
    Context.FacilitySpec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    const auto Refresh = [&]() { Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100, &Context); };
    Refresh();
    TestTrue(TEXT("Booth placement preserves the surrounding booth grid"), HasOverviewLineAt(*Overview, BoothPoint));
    TestFalse(TEXT("Booth placement hides the forbidden surrounding toilet zone"), HasOverviewLineAt(*Overview, ToiletPoint));
    TestEqual(TEXT("Overview does not rewrite the caller's placement zone"), Context.ZoneId, FName(TEXT("CursorOnlyZone")));

    Context.FacilitySpec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::Toilet);
    Refresh();
    TestFalse(TEXT("Category changes invalidate the previous zone grid immediately"), HasOverviewLineAt(*Overview, BoothPoint));
    TestTrue(TEXT("Category changes reveal the newly allowed surrounding zone"), HasOverviewLineAt(*Overview, ToiletPoint));

    BoothZone->bAllowAllFacilityCategories = true;
    ToiletZone->bAllowAllFacilityCategories = true;
    Context.FacilitySpec.AllowedZoneIds = {BoothZone->ZoneId};
    Refresh();
    TestTrue(TEXT("AllowedZoneIds preserves its allowed lot"), HasOverviewLineAt(*Overview, BoothPoint));
    TestFalse(TEXT("AllowedZoneIds excludes other nearby lots"), HasOverviewLineAt(*Overview, ToiletPoint));
    Context.FacilitySpec.AllowedZoneIds = {ToiletZone->ZoneId};
    Refresh();
    TestFalse(TEXT("Changing only AllowedZoneIds removes the formerly allowed cached lot"), HasOverviewLineAt(*Overview, BoothPoint));
    TestTrue(TEXT("Changing only AllowedZoneIds reveals the new lot without a world revision"), HasOverviewLineAt(*Overview, ToiletPoint));

    Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100);
    TestTrue(TEXT("Canceling selection restores generic overview in both lots"),
        HasOverviewLineAt(*Overview, BoothPoint) && HasOverviewLineAt(*Overview, ToiletPoint));
    TArray<UFFZSBuildableGridComponent*> CurrentComponents;
    Overview->GetComponents(CurrentComponents);
    TestEqual(TEXT("Placement changes do not create additional grid components"), CurrentComponents.Num(), OriginalComponents.Num());
    for (UFFZSBuildableGridComponent* Grid : OriginalComponents)
    {
        TestTrue(TEXT("Selection transitions reuse each registered zone's component"), CurrentComponents.Contains(Grid));
    }
    TestEqual(TEXT("Selection-specific grid queries do not mutate authority"), System->GetWorldRevision(), int64(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSBuildableGridOverviewMoveContextTest,
    "FF_ZoneSystem.Integration.BuildableGrid.OverviewMoveContext",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildableGridOverviewMoveContextTest::RunTest(const FString& Parameters)
{
    FScopedGridPresentationSettings Settings;
    UFFZSDeveloperSettings* ValidationSettings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<bool> Navigation(ValidationSettings->bEnableNavigationProjectionChecks, false);
    TGuardValue<bool> Policies(ValidationSettings->bRequireProductionPolicyDomains, false);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Editor world"), World)) return false;
    UFFZSWorldSubsystem* System = World->GetSubsystem<UFFZSWorldSubsystem>();
    AFFZSGameplayZone* Zone = SpawnGridPresentationZone(*World, TEXT("GridMoveContext"), FVector::ZeroVector, 4000.0);
    AFFZSBuildableGridActor* Overview = World->SpawnActor<AFFZSBuildableGridActor>();
    if (!TestNotNull(TEXT("World system"), System) || !TestNotNull(TEXT("Zone"), Zone) ||
        !TestNotNull(TEXT("Overview"), Overview) || !TestTrue(TEXT("Zone registers"), System->RegisterZone(Zone))) return false;

    const FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec();
    FFFZSGridPlacement SourcePlacement;
    SourcePlacement.OriginCell = FIntPoint(500, 500);
    const FFFZSCommand InstallSource = UFFZSGridLibrary::MakeInstallCommand(Spec, Zone->ZoneId, SourcePlacement);
    FFFZSGridPlacement OtherPlacement;
    OtherPlacement.OriginCell = FIntPoint(2400, 500);
    const FFFZSCommand InstallOther = UFFZSGridLibrary::MakeInstallCommand(Spec, Zone->ZoneId, OtherPlacement);
    if (!TestTrue(TEXT("Source facility installs through authority"), System->ExecuteCommand(InstallSource).bSucceeded) ||
        !TestTrue(TEXT("Other facility installs through authority"), System->ExecuteCommand(InstallOther).bSucceeded)) return false;
    AFFZSFacilityActor* Source = System->FindFacility(InstallSource.FacilityInstanceId);
    if (!TestNotNull(TEXT("Installed source actor"), Source)) return false;
    const int64 Revision = System->GetWorldRevision();
    const FVector Focus(2000.0, 2000.0, 0.0);
    const FVector2D SourcePoint(700.0, 650.0);
    const FVector2D ClearancePoint(450.0, 600.0);
    const FVector2D QueuePoint(1050.0, 1000.0);
    const FVector2D OtherPoint(2600.0, 650.0);
    Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100);
    TestFalse(TEXT("Idle overview hides the original footprint"), HasOverviewLineAt(*Overview, SourcePoint));
    TestFalse(TEXT("Idle overview hides the original safety clearance"), HasOverviewLineAt(*Overview, ClearancePoint));
    TestFalse(TEXT("Idle overview hides the original queue reservation"), HasOverviewLineAt(*Overview, QueuePoint));
    TestFalse(TEXT("Idle overview hides the other facility"), HasOverviewLineAt(*Overview, OtherPoint));

    FFFZSCommand Moving;
    Moving.Type = EFFZSCommandType::Move;
    Moving.FacilityInstanceId = InstallSource.FacilityInstanceId;
    Moving.FacilitySpec = Spec;
    Moving.ZoneId = TEXT("DifferentCursorZone");
    Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100, &Moving);
    TestTrue(TEXT("Moving overview reveals the source footprint only for that move"), HasOverviewLineAt(*Overview, SourcePoint));
    TestTrue(TEXT("Moving overview releases the source's safety clearance visually"), HasOverviewLineAt(*Overview, ClearancePoint));
    TestTrue(TEXT("Moving overview releases the source's queue reservation visually"), HasOverviewLineAt(*Overview, QueuePoint));
    TestFalse(TEXT("Moving overview still hides unrelated occupancy"), HasOverviewLineAt(*Overview, OtherPoint));

    Moving.Type = EFFZSCommandType::Install;
    Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100, &Moving);
    TestFalse(TEXT("A non-move context cannot ignore a facility merely by carrying its ID"), HasOverviewLineAt(*Overview, SourcePoint));
    Overview->UpdateGrid(*System, Focus, FVector::UpVector, 100);
    TestFalse(TEXT("Canceling movement restores the source's hidden footprint immediately"), HasOverviewLineAt(*Overview, SourcePoint));
    TestEqual(TEXT("Overview transitions leave the source actor at its committed origin"), Source->GetGridPlacement().OriginCell, SourcePlacement.OriginCell);
    TestEqual(TEXT("Overview transitions never commit the move"), System->GetWorldRevision(), Revision);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSUnifiedBuildableGridReuseTest,
    "FF_ZoneSystem.Integration.BuildableGrid.UnifiedComponentReuse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSUnifiedBuildableGridReuseTest::RunTest(const FString& Parameters)
{
    FScopedGridPresentationSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Editor world"), World)) return false;
    UFFZSWorldSubsystem* System = World->GetSubsystem<UFFZSWorldSubsystem>();
    AFFZSGameplayZone* A = SpawnGridPresentationZone(*World, TEXT("UnifiedA"));
    AFFZSGameplayZone* B = SpawnGridPresentationZone(*World, TEXT("UnifiedB"), FVector(1000.0, 0.0, 0.0));
    if (!TestNotNull(TEXT("World subsystem"), System) || !TestNotNull(TEXT("Region A"), A) ||
        !TestNotNull(TEXT("Region B"), B) || !TestTrue(TEXT("Region A registers"), System->RegisterZone(A)) ||
        !TestTrue(TEXT("Region B registers"), System->RegisterZone(B))) return false;
    AFFZSBuildableGridActor* Overview = World->SpawnActor<AFFZSBuildableGridActor>();
    if (!TestNotNull(TEXT("Overview"), Overview)) return false;
    Overview->UpdateGrid(*System, FVector(500.0, 500.0, 0.0), FVector::UpVector, 100);
    AFFZSBuildArea* BuildArea = World->SpawnActor<AFFZSBuildArea>();
    if (!TestNotNull(TEXT("Unified building area"), BuildArea) ||
        !TestTrue(TEXT("Build area registers"), System->RegisterBuildArea(BuildArea))) return false;

    Overview->UpdateGrid(*System, FVector(500.0, 500.0, 0.0), FVector::UpVector, 100);
    TArray<UFFZSBuildableGridComponent*> Grids;
    Overview->GetComponents(Grids);
    if (!TestEqual(TEXT("Unified mode replaces legacy region renderers with one component"), Grids.Num(), 1)) return false;
    UFFZSBuildableGridComponent* SharedGrid = Grids[0];
    TestTrue(TEXT("A neighboring region remains visible in the unified mask"),
        HasOverviewLineAt(*Overview, FVector2D(1500.0, 500.0)));

    Overview->UpdateGrid(*System, FVector(1500.0, 500.0, 0.0), FVector::UpVector, 100);
    Grids.Reset();
    Overview->GetComponents(Grids);
    TestEqual(TEXT("Crossing a region boundary does not allocate another renderer"), Grids.Num(), 1);
    if (!Grids.IsEmpty()) TestTrue(TEXT("The same component survives a coordinate-region change"), Grids[0] == SharedGrid);
    TestTrue(TEXT("Crossing a region boundary keeps the first region visible"),
        HasOverviewLineAt(*Overview, FVector2D(500.0, 500.0)));

    Overview->UpdateGrid(*System, FVector(3500.0, 500.0, 0.0), FVector::UpVector, 100);
    TestTrue(TEXT("No coordinate region outside the original union hides the grid"), Overview->IsHidden());
    Overview->UpdateGrid(*System, FVector(500.0, 500.0, 0.0), FVector::UpVector, 100);
    TestFalse(TEXT("Re-entering the unified area restores the grid"), Overview->IsHidden());
    Grids.Reset();
    Overview->GetComponents(Grids);
    TestEqual(TEXT("Re-entry still owns exactly one component"), Grids.Num(), 1);
    if (!Grids.IsEmpty()) TestTrue(TEXT("Hiding does not replace the shared component"), Grids[0] == SharedGrid);
    TestEqual(TEXT("Rendering does not merge NPC regions"), System->GetRegisteredZoneIds().Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSUnifiedBuildableGridMajorLinesTest,
    "FF_ZoneSystem.Integration.BuildableGrid.UnifiedWorldMajorLines",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSUnifiedBuildableGridMajorLinesTest::RunTest(const FString& Parameters)
{
    FScopedGridPresentationSettings Settings;
    TGuardValue<int32> MajorInterval(GetMutableDefault<UFFZSDeveloperSettings>()->PlacementGridMajorIntervalCm, 500);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Editor world"), World)) return false;
    UFFZSWorldSubsystem* System = World->GetSubsystem<UFFZSWorldSubsystem>();
    AFFZSGameplayZone* Region = SpawnGridPresentationZone(*World, TEXT("UnifiedRotated"), FVector(1037.0, -2063.0, 0.0));
    if (!TestNotNull(TEXT("World subsystem"), System) || !TestNotNull(TEXT("Coordinate region"), Region)) return false;
    Region->SetActorRotation(FRotator(0.0, 90.0, 0.0));
    if (!TestTrue(TEXT("Coordinate region registers"), System->RegisterZone(Region))) return false;
    AFFZSBuildArea* BuildArea = World->SpawnActor<AFFZSBuildArea>();
    AFFZSBuildableGridActor* Overview = World->SpawnActor<AFFZSBuildableGridActor>();
    if (!TestNotNull(TEXT("Build area"), BuildArea) || !TestNotNull(TEXT("Overview"), Overview) ||
        !TestTrue(TEXT("Build area registers"), System->RegisterBuildArea(BuildArea))) return false;
    Overview->UpdateGrid(*System, FVector(537.0, -1563.0, 0.0), FVector::UpVector, 100);
    TArray<UFFZSBuildableGridComponent*> Grids;
    Overview->GetComponents(Grids);
    int32 MajorLines = 0;
    for (const UFFZSBuildableGridComponent* Grid : Grids)
    {
        for (const FBatchedLine& Line : Grid->BatchedLines)
        {
            // Foreground major strokes are 1.8 px; halos and exterior contours differ.
            if (!FMath::IsNearlyEqual(Line.Thickness, 1.8f)) continue;
            ++MajorLines;
            const bool bWorldVertical = FMath::IsNearlyEqual(Line.Start.X, Line.End.X, 0.01);
            const double Coordinate = bWorldVertical ? Line.Start.X : Line.Start.Y;
            TestTrue(TEXT("Major stroke styling follows the world grid across rotated offset regions"),
                FMath::IsNearlyEqual(Coordinate, FMath::GridSnap(Coordinate, 500.0), 0.01));
        }
    }
    TestTrue(TEXT("The visible interior contains major strokes"), MajorLines > 0);
    return true;
}

#endif
