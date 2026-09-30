// FF_FFZSUnifiedConstructionTests.cpp
// 역할: 여러 Zone을 하나의 건설 영역으로 쓸 때 설치·이동·저장 규칙이 유지되는지 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSBuildableGrid.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "UObject/UnrealType.h"

namespace
{
    struct FScopedUnifiedConstructionSettings
    {
        UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
        TGuardValue<bool> Ground{Settings->bEnableGroundChecks, false};
        TGuardValue<bool> Collision{Settings->bEnableWorldCollisionChecks, false};
        TGuardValue<bool> Navigation{Settings->bEnableNavigationProjectionChecks, false};
        TGuardValue<bool> Policies{Settings->bRequireProductionPolicyDomains, false};
        TGuardValue<bool> Presets{Settings->bRestrictZoneIdsToProjectPresets, false};
        TGuardValue<int32> Cardinality{Settings->MaxEnumeratedCellsPerFacility, 10000000};
    };

    constexpr double UnifiedSeamCm = 5050.0;
    const FName UnifiedRegionA(TEXT("GZ_Unified_A"));
    const FName UnifiedRegionB(TEXT("GZ_Unified_B"));
    const FName UnifiedRegionC(TEXT("GZ_Unified_C"));

    AFFZSGameplayZone* MakeUnifiedRegion(UWorld& World, const FName Id, const double OriginX,
        const double Width, const TArray<FName>& Neighbors = {})
    {
        AFFZSGameplayZone* Zone = World.SpawnActor<AFFZSGameplayZone>();
        if (!Zone) { return nullptr; }
        Zone->ZoneId = Id;
        Zone->SetActorLocation(FVector(OriginX, 0, 0));
        Zone->NeighborZoneIds = Neighbors;
        Zone->EntryExitAnchors = { FTransform(FVector(100, 100, 0)) };
        Zone->SoftCapacityOverride = 100;
        Zone->BoundarySpline->ClearSplinePoints(false);
        for (const FVector& Point : {FVector(0, 0, 0), FVector(Width, 0, 0),
            FVector(Width, 5000, 0), FVector(0, 5000, 0)})
        {
            Zone->BoundarySpline->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
        }
        for (int32 I = 0; I < 4; ++I)
        {
            Zone->BoundarySpline->SetSplinePointType(I, ESplinePointType::Linear, false);
        }
        Zone->BoundarySpline->SetClosedLoop(true, true);
        return Zone;
    }

    struct FUnifiedConstructionWorld
    {
        UWorld* World = nullptr;
        UFFZSWorldSubsystem* System = nullptr;
        AFFZSGameplayZone* A = nullptr;
        AFFZSGameplayZone* B = nullptr;

        bool Initialize(FAutomationTestBase& Test)
        {
            World = FAutomationEditorCommonUtils::CreateNewMap();
            System = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
            if (!Test.TestNotNull(TEXT("Unified construction world"), World) ||
                !Test.TestNotNull(TEXT("Unified construction subsystem"), System)) { return false; }
            A = MakeUnifiedRegion(*World, UnifiedRegionA, 0, UnifiedSeamCm, {UnifiedRegionB});
            B = MakeUnifiedRegion(*World, UnifiedRegionB, UnifiedSeamCm, 5000, {UnifiedRegionA});
            return Test.TestNotNull(TEXT("Region A"), A) && Test.TestNotNull(TEXT("Region B"), B) &&
                Test.TestTrue(TEXT("Register region A"), System->RegisterZone(A)) &&
                Test.TestTrue(TEXT("Register region B"), System->RegisterZone(B));
        }

        bool EnableBuildArea(FAutomationTestBase& Test)
        {
            AFFZSBuildArea* Area = World->SpawnActor<AFFZSBuildArea>();
            return Test.TestNotNull(TEXT("Unified build area"), Area) &&
                Test.TestTrue(TEXT("Register the union of existing regions"), System->RegisterBuildArea(Area)) &&
                Test.TestTrue(TEXT("The single build area is discoverable"), System->FindBuildArea() == Area);
        }
    };

    FFFZSCommand UnifiedInstall(const FName Region, const FIntPoint Origin, const bool bLevels = false)
    {
        FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec();
        if (bLevels)
        {
            Spec.bUsesFacilityLevels = true;
            FFFZSFacilityUpgradeTier Tier;
            Tier.ServiceDurationMultiplier = 0.8f;
            Tier.SizeMultiplier = 2.0f;
            Tier.RevenueMultiplier = 1.0f;
            Spec.UpgradeTiers = {Tier};
        }
        return UFFZSGridLibrary::MakeInstallCommand(Spec, Region,
            FFFZSGridPlacement{Origin, 0}, -1);
    }

    FFFZSCommand UnifiedFacilityCommand(const EFFZSCommandType Type, const FGuid Id)
    {
        FFFZSCommand Command;
        Command.CommandId = FGuid::NewGuid();
        Command.TransactionId = FGuid::NewGuid();
        Command.Type = Type;
        Command.FacilityInstanceId = Id;
        return Command;
    }

    bool UnifiedGridCell(const FFFZSBuildableGridMask& Mask, const FVector2D& Point)
    {
        return Mask.IsBuildable(FMath::FloorToInt((Point.X - Mask.GetMinLocalCm().X) / Mask.GetStepCm()),
            FMath::FloorToInt((Point.Y - Mask.GetMinLocalCm().Y) / Mask.GetStepCm()));
    }

    void ReportUnifiedStructDifferences(FAutomationTestBase& Test, const UStruct* Type,
        const void* Expected, const void* Actual, const FString& Path)
    {
        for (TFieldIterator<FProperty> It(Type); It; ++It)
        {
            const FProperty* Property = *It;
            if (Property->Identical_InContainer(Expected, Actual)) { continue; }
            const FString PropertyPath = Path + TEXT(".") + Property->GetName();
            const void* ExpectedValue = Property->ContainerPtrToValuePtr<void>(Expected);
            const void* ActualValue = Property->ContainerPtrToValuePtr<void>(Actual);
            if (const FStructProperty* Struct = CastField<FStructProperty>(Property);
                Struct && Struct->Struct->PropertyLink)
            {
                ReportUnifiedStructDifferences(Test, Struct->Struct, ExpectedValue, ActualValue, PropertyPath);
                continue;
            }
            if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
            {
                FScriptArrayHelper ExpectedArray(Array, ExpectedValue);
                FScriptArrayHelper ActualArray(Array, ActualValue);
                if (const FStructProperty* Inner = CastField<FStructProperty>(Array->Inner);
                    Inner && ExpectedArray.Num() == ActualArray.Num())
                {
                    for (int32 I = 0; I < ExpectedArray.Num(); ++I)
                    {
                        ReportUnifiedStructDifferences(Test, Inner->Struct, ExpectedArray.GetRawPtr(I),
                            ActualArray.GetRawPtr(I), FString::Printf(TEXT("%s[%d]"), *PropertyPath, I));
                    }
                    continue;
                }
            }
            FString ExpectedText;
            FString ActualText;
            Property->ExportTextItem_Direct(ExpectedText, ExpectedValue, nullptr, nullptr, PPF_None);
            Property->ExportTextItem_Direct(ActualText, ActualValue, nullptr, nullptr, PPF_None);
            Test.AddInfo(FString::Printf(TEXT("Snapshot difference %s: expected=%s actual=%s"),
                *PropertyPath, *ExpectedText, *ActualText));
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSUnifiedConstructionLifecycleTest,
    "FF_ZoneSystem.Integration.UnifiedConstruction.SeamLifecycleAndCrossRegionOccupancy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSUnifiedConstructionLifecycleTest::RunTest(const FString& Parameters)
{
    FScopedUnifiedConstructionSettings Settings;
    FUnifiedConstructionWorld Fixture;
    if (!Fixture.Initialize(*this) || !Fixture.EnableBuildArea(*this)) { return false; }
    UFFZSWorldSubsystem& System = *Fixture.System;
    const FFFZSCommand Install = UnifiedInstall(UnifiedRegionA, FIntPoint(4700, 1000), true);
    const FFFZSCommandResult Installed = System.ExecuteCommand(Install);
    if (!TestTrue(TEXT("A facility and its reservations can cross the internal seam"), Installed.bSucceeded))
    {
        AddError(Installed.Message.ToString());
        return false;
    }
    AFFZSFacilityActor* Facility = Installed.FacilityActor.Get();
    if (!TestNotNull(TEXT("Installed seam facility"), Facility)) { return false; }
    TestTrue(TEXT("Footprint selection works on region A's side"),
        System.FindFacilityAtWorldPoint(FVector(4800, 1150, 0)) == Facility);
    TestTrue(TEXT("Footprint selection works on region B's side"),
        System.FindFacilityAtWorldPoint(FVector(5200, 1150, 0)) == Facility);
    const FFFZSCommand Overlap = UnifiedInstall(UnifiedRegionB, FIntPoint(-250, 1000));
    TestFalse(TEXT("A different region ID cannot bypass world occupancy"), System.ExecuteCommand(Overlap).bSucceeded);

    FFFZSCommand Move = UnifiedFacilityCommand(EFFZSCommandType::Move, Install.FacilityInstanceId);
    Move.ZoneId = UnifiedRegionB;
    Move.Placement.OriginCell = FIntPoint(-250, 2500);
    const FFFZSCommandResult Moved = System.ExecuteCommand(Move);
    if (!TestTrue(TEXT("Move changes the storage region while crossing its boundary"), Moved.bSucceeded))
    {
        AddError(Moved.Message.ToString());
        return false;
    }
    TestEqual(TEXT("The facility retains the requested regional identity"), Facility->GetZoneId(), UnifiedRegionB);
    const FFFZSCommandResult Reused = System.ExecuteCommand(UnifiedInstall(UnifiedRegionB, FIntPoint(-250, 1000)));
    TestTrue(TEXT("Move releases occupancy in the old buckets"), Reused.bSucceeded);
    TestFalse(TEXT("Move populates the new buckets across region IDs"),
        System.ExecuteCommand(UnifiedInstall(UnifiedRegionA, FIntPoint(4700, 2500))).bSucceeded);

    const FFFZSCommandResult Neighbor = System.ExecuteCommand(UnifiedInstall(UnifiedRegionA, FIntPoint(4700, 3600)));
    if (!TestTrue(TEXT("A nearby facility fits before enlargement"), Neighbor.bSucceeded)) { return false; }
    FFFZSCommand Upgrade = UnifiedFacilityCommand(EFFZSCommandType::Upgrade, Install.FacilityInstanceId);
    Upgrade.TargetUpgradeLevel = 2;
    Upgrade.UpgradeServiceDurationMultiplier = 0.8f;
    Upgrade.UpgradeSizeMultiplier = 2.0f;
    const int64 BeforeFailure = System.GetWorldRevision();
    const FFFZSCommandResult CollidingUpgrade = System.ExecuteCommand(Upgrade);
    TestFalse(TEXT("Enlargement checks reservations from the neighboring region"), CollidingUpgrade.bSucceeded);
    TestTrue(TEXT("Cross-region enlargement reaches spatial validation"),
        CollidingUpgrade.FailureReason == EFFZSPlacementFailure::PlacementCollision ||
        CollidingUpgrade.FailureReason == EFFZSPlacementFailure::InsufficientClearance ||
        CollidingUpgrade.FailureReason == EFFZSPlacementFailure::QueueFootprintBlocked);
    TestEqual(TEXT("Rejected enlargement leaves authority unchanged"), System.GetWorldRevision(), BeforeFailure);
    if (!TestTrue(TEXT("Remove the neighboring blocker"), System.ExecuteCommand(UnifiedFacilityCommand(
        EFFZSCommandType::Remove, Neighbor.FacilityActor->GetFacilityInstanceId())).bSucceeded)) { return false; }
    Upgrade.CommandId = FGuid::NewGuid();
    Upgrade.TransactionId = FGuid::NewGuid();
    const FFFZSCommandResult Upgraded = System.ExecuteCommand(Upgrade);
    if (!TestTrue(TEXT("Enlargement can cross the seam after the blocker is removed"), Upgraded.bSucceeded))
    {
        AddError(Upgraded.Message.ToString());
        return false;
    }
    TestEqual(TEXT("The upgraded footprint is authoritative"), Facility->GetFacilitySpec().FootprintSizeCells,
        FIntPoint(1200, 600));
    TestFalse(TEXT("Expanded occupancy blocks a fresh neighbor after upgrade"),
        System.ExecuteCommand(UnifiedInstall(UnifiedRegionA, FIntPoint(4700, 3600))).bSucceeded);
    TestTrue(TEXT("Remove clears the expanded index record"), System.ExecuteCommand(UnifiedFacilityCommand(
        EFFZSCommandType::Remove, Install.FacilityInstanceId)).bSucceeded);
    TestTrue(TEXT("The released area accepts a new facility"),
        System.ExecuteCommand(UnifiedInstall(UnifiedRegionA, FIntPoint(4700, 2500))).bSucceeded);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSUnifiedConstructionBoundaryTest,
    "FF_ZoneSystem.Integration.UnifiedConstruction.LegacyBoundaryGridGapsAndProtection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSUnifiedConstructionBoundaryTest::RunTest(const FString& Parameters)
{
    FScopedUnifiedConstructionSettings Settings;
    FUnifiedConstructionWorld Fixture;
    if (!Fixture.Initialize(*this)) { return false; }
    UFFZSWorldSubsystem& System = *Fixture.System;
    const FFFZSCommand Seam = UnifiedInstall(UnifiedRegionA, FIntPoint(4700, 1000));
    TestTrue(TEXT("Without a build area the legacy exactly-one-zone rule remains"),
        System.ValidatePlacement(Seam).Reasons.Contains(EFFZSPlacementFailure::OutsideZone));
    AFFZSGameplayZone* C = MakeUnifiedRegion(*Fixture.World, UnifiedRegionC, 12000, 2000);
    if (!TestNotNull(TEXT("Disjoint region"), C) || !TestTrue(TEXT("Register disjoint region"), System.RegisterZone(C)) ||
        !Fixture.EnableBuildArea(*this)) { return false; }
    TestTrue(TEXT("The same seam placement becomes valid in the region union"), System.ValidatePlacement(Seam).bIsValid);
    FFFZSBuildableGridMask Mask;
    FFFZSCommand Overview;
    Overview.ZoneId = UnifiedRegionA;
    if (!TestTrue(TEXT("Build a local visual mask spanning multiple regions"), System.BuildPlacementGridMask(
        Overview, FVector2D(4500, 1000), FVector2D(12500, 1500), 100, 0, Mask))) { return false; }
    TestTrue(TEXT("The visual cell straddling the 5050cm seam is allowed"),
        UnifiedGridCell(Mask, FVector2D(5050, 1150)));
    TestTrue(TEXT("The neighboring region is visible through the same build context"),
        UnifiedGridCell(Mask, FVector2D(7050, 1150)));
    TestFalse(TEXT("The real gap between disconnected regions stays unbuildable"),
        UnifiedGridCell(Mask, FVector2D(11050, 1150)));
    TestTrue(TEXT("A disconnected region is included without filling its surrounding gap"),
        UnifiedGridCell(Mask, FVector2D(12250, 1150)));
    TestFalse(TEXT("A facility cannot be installed in the union's empty gap"),
        System.ValidatePlacement(UnifiedInstall(UnifiedRegionA, FIntPoint(10700, 1000))).bIsValid);

    AFFZSProtectedArea* Protected = Fixture.World->SpawnActor<AFFZSProtectedArea>();
    if (!TestNotNull(TEXT("Protection in neighboring region"), Protected)) { return false; }
    Protected->ProtectedAreaId = TEXT("PA_Unified_B");
    Protected->LinkedZoneId = UnifiedRegionB;
    Protected->AreaType = EFFZSProtectedAreaType::NoBuild;
    Protected->AreaBounds->SetBoxExtent(FVector(40, 40, 100));
    Protected->SetActorLocation(FVector(5200, 1150, 100));
    if (!TestTrue(TEXT("Register the regional protected area"), System.RegisterProtectedArea(Protected))) { return false; }
    TestFalse(TEXT("An A-owned facility obeys a B-linked protected area it overlaps"),
        System.ValidatePlacement(Seam).bIsValid);
    System.BuildPlacementGridMask(Overview, FVector2D(4500, 1000), FVector2D(5500, 1500), 100, 0, Mask);
    TestFalse(TEXT("The unified visual mask also removes neighboring protection"),
        UnifiedGridCell(Mask, FVector2D(5250, 1150)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSUnifiedConstructionSnapshotTest,
    "FF_ZoneSystem.Integration.UnifiedConstruction.RegionalReadModelAndAtomicSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSUnifiedConstructionSnapshotTest::RunTest(const FString& Parameters)
{
    FScopedUnifiedConstructionSettings Settings;
    FUnifiedConstructionWorld Fixture;
    if (!Fixture.Initialize(*this)) { return false; }
    UFFZSWorldSubsystem& System = *Fixture.System;
    System.ReportZonePopulation(UnifiedRegionA, 17, 1.0f);
    System.ReportZonePopulation(UnifiedRegionB, 43, 1.0f);
    const FFFZSReadModel BeforeArea = System.BuildReadModel();
    if (!Fixture.EnableBuildArea(*this)) { return false; }
    const FFFZSCommand First = UnifiedInstall(UnifiedRegionA, FIntPoint(4700, 1000));
    const FFFZSCommand Second = UnifiedInstall(UnifiedRegionB, FIntPoint(1750, 1000));
    if (!TestTrue(TEXT("Install a seam facility and a second regional facility"),
        System.ExecuteCommand(First).bSucceeded && System.ExecuteCommand(Second).bSucceeded)) { return false; }
    const FFFZSReadModel WithArea = System.BuildReadModel();
    TestEqual(TEXT("Construction unification keeps both population regions"), WithArea.Zones.Num(), BeforeArea.Zones.Num());
    for (int32 I = 0; I < FMath::Min(WithArea.Zones.Num(), BeforeArea.Zones.Num()); ++I)
    {
        TestTrue(TEXT("Regional graph, anchors, area, population and density are preserved"),
            FFFZSZoneReadModel::StaticStruct()->CompareScriptStruct(&WithArea.Zones[I], &BeforeArea.Zones[I], 0));
    }
    // A newly installed queue is Closed with bDrainRequested=false; the existing queue
    // restore contract canonicalizes Closed queues to bDrainRequested=true. Close through
    // normal authority commands before capture so this construction test can require exact
    // roundtrip equality for every snapshot field, including the queue and command receipts.
    if (!TestTrue(TEXT("Close both facilities into a canonical, drained save state"),
        System.ExecuteCommand(UnifiedFacilityCommand(EFFZSCommandType::Close, First.FacilityInstanceId)).bSucceeded &&
        System.ExecuteCommand(UnifiedFacilityCommand(EFFZSCommandType::Close, Second.FacilityInstanceId)).bSucceeded))
    {
        return false;
    }
    const FFFZSWorldSnapshot Saved = System.CaptureWorldSnapshot();
    for (const FFFZSFacilityInstanceSnapshot& Item : Saved.Facilities)
    {
        TestTrue(TEXT("The saved fixture already records completed draining"),
            Item.Queue.State == EFFZSQueueState::Closed && Item.Queue.bDrainRequested);
    }
    TArray<FText> Errors;
    if (!TestTrue(TEXT("A valid cross-seam snapshot restores"), System.RestoreWorldSnapshot(Saved, Errors)))
    {
        for (const FText& Error : Errors) { AddError(Error.ToString()); }
        return false;
    }
    const FFFZSWorldSnapshot Restored = System.CaptureWorldSnapshot();
    if (!TestTrue(TEXT("Snapshot roundtrip retains regional IDs, grid placement, world transforms and receipts"),
        FFFZSWorldSnapshot::StaticStruct()->CompareScriptStruct(&Saved, &Restored, 0)))
    {
        ReportUnifiedStructDifferences(*this, FFFZSWorldSnapshot::StaticStruct(), &Saved, &Restored, TEXT("World"));
    }
    TestFalse(TEXT("Restoration rebuilds world occupancy across region identities"),
        System.ValidatePlacement(UnifiedInstall(UnifiedRegionB, FIntPoint(-250, 1000))).bIsValid);

    FFFZSWorldSnapshot Corrupt = Restored;
    const FFFZSFacilityInstanceSnapshot* FirstSaved = Corrupt.Facilities.FindByPredicate(
        [&](const FFFZSFacilityInstanceSnapshot& Item) { return Item.FacilityInstanceId == First.FacilityInstanceId; });
    FFFZSFacilityInstanceSnapshot* SecondSaved = Corrupt.Facilities.FindByPredicate(
        [&](const FFFZSFacilityInstanceSnapshot& Item) { return Item.FacilityInstanceId == Second.FacilityInstanceId; });
    if (!TestNotNull(TEXT("First saved instance"), FirstSaved) || !TestNotNull(TEXT("Second saved instance"), SecondSaved))
    {
        return false;
    }
    SecondSaved->Placement = FirstSaved->Placement;
    SecondSaved->Placement.OriginCell.X -= static_cast<int32>(UnifiedSeamCm);
    SecondSaved->WorldTransform = FirstSaved->WorldTransform;
    TestEqual(TEXT("The corrupt overlap keeps a distinct storage region"), SecondSaved->ZoneId, UnifiedRegionB);
    const FFFZSWorldSnapshot BeforeRejectedRestore = System.CaptureWorldSnapshot();
    Errors.Reset();
    TestFalse(TEXT("A cross-region world overlap is rejected before replacing live state"),
        System.RestoreWorldSnapshot(Corrupt, Errors));
    TestTrue(TEXT("Rejected overlap provides a validation reason"), !Errors.IsEmpty());
    const FFFZSWorldSnapshot AfterRejectedRestore = System.CaptureWorldSnapshot();
    if (!TestTrue(TEXT("The entire authority snapshot is unchanged after the rejected restore"),
        FFFZSWorldSnapshot::StaticStruct()->CompareScriptStruct(&BeforeRejectedRestore, &AfterRejectedRestore, 0)))
    {
        ReportUnifiedStructDifferences(*this, FFFZSWorldSnapshot::StaticStruct(),
            &BeforeRejectedRestore, &AfterRejectedRestore, TEXT("World"));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSUnifiedConstructionSpatialLookupTest,
    "FF_ZoneSystem.Integration.UnifiedConstruction.SpatialMembershipSelectionToleranceAndRegionRemoval",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSUnifiedConstructionSpatialLookupTest::RunTest(const FString& Parameters)
{
    FScopedUnifiedConstructionSettings Settings;
    TGuardValue<float> Hysteresis(Settings.Settings->ZoneMembershipHysteresisCm, 50.0f);
    FUnifiedConstructionWorld Fixture;
    if (!Fixture.Initialize(*this) || !Fixture.EnableBuildArea(*this)) { return false; }
    UFFZSWorldSubsystem& System = *Fixture.System;
    {
        // UWorld stores WorldType as TEnumAsByte. Only this scope enables the production
        // region-index path; the editor test world and its actors keep their normal lifetime.
        TGuardValue<TEnumAsByte<EWorldType::Type>> GameWorld(Fixture.World->WorldType, EWorldType::Game);
        TestEqual(TEXT("A shared boundary retains the lexical region tie-break"),
            System.ResolveZoneAtWorldLocation(FVector(UnifiedSeamCm, 250, 0)), UnifiedRegionA);
        TestEqual(TEXT("A previous B membership is preserved on the shared boundary"),
            System.ResolveZoneAtWorldLocation(FVector(UnifiedSeamCm, 250, 0), UnifiedRegionB), UnifiedRegionB);
        TestEqual(TEXT("Previous-region hysteresis runs before spatial candidate filtering"),
            System.ResolveZoneAtWorldLocation(FVector(UnifiedSeamCm - 25, 250, 0), UnifiedRegionB), UnifiedRegionB);
        TestEqual(TEXT("Without history the same point belongs to A"),
            System.ResolveZoneAtWorldLocation(FVector(UnifiedSeamCm - 25, 250, 0)), UnifiedRegionA);
        TestTrue(TEXT("XY candidates cannot bypass the regional height range"),
            System.ResolveZoneAtWorldLocation(FVector(UnifiedSeamCm, 250, 1001), UnifiedRegionB).IsNone());
        TestTrue(TEXT("Empty space remains outside every indexed region"),
            System.ResolveZoneAtWorldLocation(FVector(11000, 250, 0)).IsNone());
    }

    FFFZSCommand Install = UnifiedInstall(UnifiedRegionA, FIntPoint(4700, 1000));
    // Remove clearance so the point is outside the broad-phase reservation AABB while
    // still inside the existing 2cm footprint selection tolerance.
    Install.FacilitySpec.ClearanceCells = 0;
    const FFFZSCommandResult Installed = System.ExecuteCommand(Install);
    if (!TestTrue(TEXT("Install a seam facility without selection-masking clearance"), Installed.bSucceeded))
    {
        AddError(Installed.Message.ToString());
        return false;
    }
    AFFZSFacilityActor* Facility = Installed.FacilityActor.Get();
    if (!TestNotNull(TEXT("The seam facility exists"), Facility)) { return false; }
    TestTrue(TEXT("The spatial lookup retains the existing 2cm footprint tolerance"),
        System.FindFacilityAtWorldPoint(FVector(4698.5, 1150, 0)) == Facility);
    TestNull(TEXT("The broad phase does not expand the precise 2cm selection tolerance"),
        System.FindFacilityAtWorldPoint(FVector(4697.5, 1150, 0)));

    if (!TestTrue(TEXT("The source region may unregister while its physical facility remains"),
        System.UnregisterZone(Fixture.A))) { return false; }
    TestTrue(TEXT("Unregistering a region does not discard the existing facility"),
        System.FindFacility(Install.FacilityInstanceId) == Facility);
    TestTrue(TEXT("The facility remains selectable in the surviving B region"),
        System.FindFacilityAtWorldPoint(FVector(5200, 1150, 0)) == Facility);
    const FFFZSPlacementValidation Overlap = System.ValidatePlacement(
        UnifiedInstall(UnifiedRegionB, FIntPoint(100, 1000)));
    TestFalse(TEXT("The candidate fits within the surviving build area"),
        Overlap.Reasons.Contains(EFFZSPlacementFailure::OutsideZone));
    TestTrue(TEXT("The retained world occupancy blocks overlap after its source region unregisters"),
        Overlap.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    {
        TGuardValue<TEnumAsByte<EWorldType::Type>> GameWorld(Fixture.World->WorldType, EWorldType::Game);
        TestTrue(TEXT("The refreshed region index no longer resolves the removed A region"),
            System.ResolveZoneAtWorldLocation(FVector(2500, 250, 0)).IsNone());
        TestEqual(TEXT("The refreshed region index still resolves B"),
            System.ResolveZoneAtWorldLocation(FVector(6000, 250, 0)), UnifiedRegionB);
    }
    return true;
}

#endif
