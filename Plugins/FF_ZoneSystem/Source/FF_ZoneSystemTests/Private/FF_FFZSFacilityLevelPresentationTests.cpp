#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "FF_FFZSCollision.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSVisualGeometry.h"
#include "Tests/FF_FFZSTestFixtures.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSFacilityLevelPresentationTest,
    "FF_ZoneSystem.World.FacilityLevels.GeometryVisualAndRestore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityLevelPresentationTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UStaticMesh* BaseMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* LevelMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (!TestNotNull(TEXT("Test world exists"), World)
        || !TestNotNull(TEXT("Base engine mesh is available"), BaseMesh)
        || !TestNotNull(TEXT("Alternate engine mesh is available"), LevelMesh))
    {
        return false;
    }

    FFFZSFacilitySpec Base = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    Base.bUsesFacilityLevels = true;
    Base.bOverrideRevenuePerUseGold = true;
    Base.RevenuePerUseGold = 275;
    Base.VisualMesh = BaseMesh;
    Base.VisualSizingMode = EFFZSVisualSizingMode::SourceMeshSize;
    FFFZSFacilityUpgradeTier Level2;
    Level2.TargetLevel = 2;
    Level2.VisualMesh = LevelMesh;
    Level2.SizeMultiplier = 1.5f;
    Level2.ServiceDurationMultiplier = 0.8f;
    Level2.RevenueMultiplier = 1.5f;
    FFFZSFacilityUpgradeTier Level3;
    Level3.TargetLevel = 3;
    Level3.SizeMultiplier = 2.f;
    Level3.ServiceDurationMultiplier = 0.65f;
    Level3.RevenueMultiplier = 2.25f;
    Base.UpgradeTiers = { Level2, Level3 };
    FFFZSGridPlacement Placement;
    AFFZSFacilityActor* Actor = World->SpawnActor<AFFZSFacilityActor>();
    if (!TestNotNull(TEXT("Facility actor is created"), Actor)
        || !TestTrue(TEXT("Base specification initializes"), Actor->InitializeFacility(
            FGuid::NewGuid(), Base, TEXT("GZ_DadaPocha"), Placement, FTransform::Identity)))
    {
        return false;
    }
    TestTrue(TEXT("Facility declares level support"), Actor->HasFacilityLevels());
    TestEqual(TEXT("Level one has base revenue"), Actor->GetUpgradeRevenueMultiplier(), 1.f);
    TestTrue(TEXT("Level one displays the base mesh"), Actor->VisualMesh->GetStaticMesh() == BaseMesh);
    TestEqual(TEXT("Initialized mesh remains nonblocking before registration"),
        Actor->VisualMesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    Actor->SetPlayerCollisionEnabled(true);
    TestEqual(TEXT("Registration enables queries on the existing mesh"),
        Actor->VisualMesh->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
    Actor->VisualMesh->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));
    if (!TestTrue(TEXT("Level two applies"), Actor->ApplyUpgradeState(2, 0.8f, 1.5f, 1.5f))) return false;
    TestEqual(TEXT("Level two uses its authored revenue"), Actor->GetUpgradeRevenueMultiplier(), 1.5f);
    TestTrue(TEXT("Level two uses its own mesh"), Actor->VisualMesh->GetStaticMesh() == LevelMesh);
    TestEqual(TEXT("Upgrading keeps the registered mesh collision enabled"),
        Actor->VisualMesh->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
    TestEqual(TEXT("The replacement mesh keeps player-only object filtering"),
        Actor->VisualMesh->GetCollisionObjectType(), FFZSCollision::FacilityPlayerObjectChannel);
    TestEqual(TEXT("Old mesh material overrides are cleared"), Actor->VisualMesh->GetNumOverrideMaterials(), 0);
    TestTrue(TEXT("Source mesh size uses one absolute multiplier"),
        Actor->VisualMesh->GetRelativeScale3D().Equals(FVector(1.5)));
    TestTrue(TEXT("Actor scale stays one"), Actor->GetActorScale3D().Equals(FVector::OneVector));
    TestEqual(TEXT("Base footprint is immutable"), Actor->GetBaseFacilitySpec().FootprintSizeCells, Base.FootprintSizeCells);
    TestEqual(TEXT("Authoritative footprint grows with the level"), Actor->GetFacilitySpec().FootprintSizeCells,
        FIntPoint(FMath::CeilToInt(Base.FootprintSizeCells.X * 1.5f), FMath::CeilToInt(Base.FootprintSizeCells.Y * 1.5f)));
    TestTrue(TEXT("Footprint component matches effective geometry"), Actor->FootprintBounds->GetUnscaledBoxExtent().Equals(
        FVector(Actor->GetFacilitySpec().FootprintSizeCells.X * 0.5, Actor->GetFacilitySpec().FootprintSizeCells.Y * 0.5,
            Actor->GetFacilitySpec().PlacementObstacleHeightCm * 0.5)));
    TestTrue(TEXT("Navigation obstacle matches the changed footprint"),
        Actor->NavigationObstacleBounds->GetUnscaledBoxExtent().Equals(Actor->FootprintBounds->GetUnscaledBoxExtent()));
    TestTrue(TEXT("Selection proxy still encloses the current footprint"),
        Actor->SelectionProxy->GetUnscaledBoxExtent().X >= Actor->FootprintBounds->GetUnscaledBoxExtent().X
        && Actor->SelectionProxy->GetUnscaledBoxExtent().Y >= Actor->FootprintBounds->GetUnscaledBoxExtent().Y);
    FTransform Entry;
    const FFFZSAnchorDefinition* BaseEntry = Base.Anchors.FindByPredicate([](const FFFZSAnchorDefinition& Anchor)
    {
        return Anchor.Type == EFFZSAnchorType::Entry && Anchor.Index == 0;
    });
    TestTrue(TEXT("Entry anchor is rebuilt at the scaled location"), BaseEntry
        && Actor->GetAnchorWorldTransform(EFFZSAnchorType::Entry, 0, Entry)
        && Entry.GetLocation().Equals(BaseEntry->LocalTransform.GetLocation() * 1.5f));
    TestTrue(TEXT("Queue receives the effective service duration"), FMath::IsNearlyEqual(
        Actor->GetQueueComponent()->GetRules().ServiceDurationSeconds, Base.QueueRules.ServiceDurationSeconds * 0.8f));

    // Simulate a completed request that failed to resolve; no invalid asset is loaded by this test.
    Actor->ApplyLoadedVisualMesh(Actor->VisualLoadGeneration,
        FSoftObjectPath(TEXT("/Game/Automation/MissingLevelMesh.MissingLevelMesh")), true);
    TestTrue(TEXT("Failed tier mesh falls back to level one"), Actor->VisualMesh->GetStaticMesh() == BaseMesh);
    TestEqual(TEXT("Fallback mesh retains player collision"),
        Actor->VisualMesh->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
    TestTrue(TEXT("The specified second-level mesh can be selected again"), Actor->ApplyUpgradeState(2, 0.8f, 1.5f, 1.5f));
    const uint32 OldRequestGeneration = Actor->VisualLoadGeneration;
    if (!TestTrue(TEXT("Level three applies"), Actor->ApplyUpgradeState(3, 0.65f, 2.f, 2.25f))) return false;
    TestTrue(TEXT("Unspecified third-level mesh uses level one, not level two"), Actor->VisualMesh->GetStaticMesh() == BaseMesh);
    TestTrue(TEXT("Level three size is absolute, not compounded"), Actor->VisualMesh->GetRelativeScale3D().Equals(FVector(2.f)));
    Actor->ApplyLoadedVisualMesh(OldRequestGeneration, FSoftObjectPath(LevelMesh), true);
    TestTrue(TEXT("A stale asynchronous callback cannot restore the previous tier mesh"), Actor->VisualMesh->GetStaticMesh() == BaseMesh);
    TestEqual(TEXT("A stale callback preserves the current mesh collision"),
        Actor->VisualMesh->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
    TestFalse(TEXT("Invalid size is rejected before mutation"), Actor->ApplyUpgradeState(4, 0.5f, 0.f));
    TestEqual(TEXT("Rejected mutation preserves level three"), Actor->GetUpgradeLevel(), 3);
    TestTrue(TEXT("Rejected mutation preserves the mesh"), Actor->VisualMesh->GetStaticMesh() == BaseMesh);
    TestFalse(TEXT("Invalid revenue is rejected before mutation"), Actor->ApplyUpgradeState(4, 0.5f, 2.f, -1.f));
    TestEqual(TEXT("Rejected mutation preserves revenue"), Actor->GetUpgradeRevenueMultiplier(), 2.25f);

    const FFFZSFacilityInstanceSnapshot Snapshot = Actor->CaptureInstanceSnapshot();
    TestEqual(TEXT("Snapshot stores the original footprint"), Snapshot.FacilitySpec.FootprintSizeCells, Base.FootprintSizeCells);
    TestEqual(TEXT("Snapshot stores the absolute size"), Snapshot.UpgradeSizeMultiplier, 2.f);
    TestEqual(TEXT("Snapshot stores the absolute revenue"), Snapshot.UpgradeRevenueMultiplier, 2.25f);
    TestTrue(TEXT("Snapshot keeps the installed facility's revenue override"), Snapshot.FacilitySpec.bOverrideRevenuePerUseGold);
    TestEqual(TEXT("Upgrades preserve the installed base revenue amount"), Snapshot.FacilitySpec.RevenuePerUseGold, 275);
    TestTrue(TEXT("Read model reports levels"), Actor->GetReadModel().bUsesFacilityLevels);
    TestEqual(TEXT("Read model reports current size"), Actor->GetReadModel().UpgradeSizeMultiplier, 2.f);
    TestEqual(TEXT("Read model reports current revenue"), Actor->GetReadModel().UpgradeRevenueMultiplier, 2.25f);
    AFFZSFacilityActor* Restored = World->SpawnActor<AFFZSFacilityActor>();
    if (!TestTrue(TEXT("Snapshot initializes another actor"), Restored && Restored->InitializeFacility(
        Snapshot.FacilityInstanceId, Snapshot.FacilitySpec, Snapshot.ZoneId, Snapshot.Placement, Snapshot.WorldTransform,
        Snapshot.State, Snapshot.UpgradeLevel, Snapshot.UpgradeServiceDurationMultiplier, Snapshot.UpgradeSizeMultiplier,
        Snapshot.UpgradeRevenueMultiplier))) return false;
    TestEqual(TEXT("Restore does not double the footprint multiplier"), Restored->GetFacilitySpec().FootprintSizeCells, Actor->GetFacilitySpec().FootprintSizeCells);
    TestTrue(TEXT("Restore preserves visual size"), Restored->VisualMesh->GetRelativeScale3D().Equals(FVector(2.f)));
    TestEqual(TEXT("Restore preserves revenue"), Restored->GetUpgradeRevenueMultiplier(), 2.25f);
    TestTrue(TEXT("Restore preserves the facility's revenue policy"), Restored->GetFacilitySpec().bOverrideRevenuePerUseGold);
    TestEqual(TEXT("Restore keeps the original base amount separate from the level multiplier"),
        Restored->GetFacilitySpec().RevenuePerUseGold, 275);
    TestTrue(TEXT("Moving the restored actor succeeds"), Restored->ApplyPlacement(Snapshot.ZoneId, Snapshot.Placement, FTransform(FVector(500, 0, 0))));
    TestTrue(TEXT("Movement never compounds visual size"), Restored->VisualMesh->GetRelativeScale3D().Equals(FVector(2.f)));
    TestEqual(TEXT("Movement never compounds physical size"), Restored->GetFacilitySpec().FootprintSizeCells, Actor->GetFacilitySpec().FootprintSizeCells);
    TestEqual(TEXT("Movement never compounds revenue"), Restored->GetUpgradeRevenueMultiplier(), 2.25f);

    Base.VisualSizingMode = EFFZSVisualSizingMode::MatchPlacementBounds;
    AFFZSFacilityActor* Fitted = World->SpawnActor<AFFZSFacilityActor>();
    if (!TestTrue(TEXT("A fitted mesh initializes at level two"), Fitted && Fitted->InitializeFacility(
        FGuid::NewGuid(), Base, Snapshot.ZoneId, Placement, FTransform::Identity, EFFZSFacilityState::Validated, 2, 0.8f, 1.5f, 1.5f))) return false;
    const FBox FittedBounds = FFZSVisualGeometry::TransformBounds(LevelMesh->GetBoundingBox(), Fitted->VisualMesh->GetRelativeTransform());
    TestTrue(TEXT("Fitted mesh matches scaled physical bounds exactly once"), FittedBounds.GetSize().Equals(
        FVector(Fitted->GetFacilitySpec().FootprintSizeCells.X, Fitted->GetFacilitySpec().FootprintSizeCells.Y,
            Fitted->GetFacilitySpec().PlacementObstacleHeightCm), 0.01));

    Base.VisualMesh.Reset();
    AFFZSFacilityActor* NoBaseMesh = World->SpawnActor<AFFZSFacilityActor>();
    if (!TestTrue(TEXT("A facility without a base mesh initializes"), NoBaseMesh && NoBaseMesh->InitializeFacility(
        FGuid::NewGuid(), Base, Snapshot.ZoneId, Placement, FTransform::Identity, EFFZSFacilityState::Validated, 2, 0.8f, 1.5f, 1.5f))) return false;
    TestTrue(TEXT("A missing next-tier and base mesh can be selected"), NoBaseMesh->ApplyUpgradeState(3, 0.65f, 2.f, 2.25f));
    TestNull(TEXT("No base mesh preserves the intentional absence of a visual"), NoBaseMesh->VisualMesh->GetStaticMesh());
    NoBaseMesh->SetPlayerCollisionEnabled(true);
    TestEqual(TEXT("A missing mesh cannot create invisible player collision"),
        NoBaseMesh->VisualMesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

    Base.UpgradeTiers.SetNum(1);
    Base.UpgradeTiers[0].ServiceDurationMultiplier = 1.f;
    Base.UpgradeTiers[0].SizeMultiplier = 1.f;
    AFFZSFacilityActor* RevenueOnly = World->SpawnActor<AFFZSFacilityActor>();
    TestTrue(TEXT("A revenue-only level initializes without requiring faster service"), RevenueOnly && RevenueOnly->InitializeFacility(
        FGuid::NewGuid(), Base, Snapshot.ZoneId, Placement, FTransform::Identity, EFFZSFacilityState::Validated, 2, 1.f, 1.f, 1.5f));
    TestFalse(TEXT("Level one cannot contain paid revenue"), AFFZSFacilityActor::IsValidUpgradeState(1, 1.f, 1.f, 1.5f));
    TestTrue(TEXT("Sparse high levels and freely authored finite values are accepted"),
        AFFZSFacilityActor::IsValidUpgradeState(MAX_int32, 20.f, .75f, 1000.f));
    TestTrue(TEXT("A level may intentionally produce no revenue"), AFFZSFacilityActor::IsValidUpgradeState(25, 2.f, .75f, 0.f));
    FFFZSQueueRules HugeDurationRules = Base.QueueRules;
    HugeDurationRules.ServiceDurationSeconds = MAX_flt;
    const float SaturatedDuration = AFFZSFacilityActor::BuildEffectiveQueueRules(HugeDurationRules, MAX_flt).ServiceDurationSeconds;
    TestTrue(TEXT("Very large authored duration remains finite at runtime"), FMath::IsFinite(SaturatedDuration));
    TestEqual(TEXT("Only the final representable service duration saturates"), SaturatedDuration, MAX_flt);

    FFFZSFacilitySpec ShrinkingSpec = Base;
    ShrinkingSpec.VisualMesh = BaseMesh;
    ShrinkingSpec.VisualSizingMode = EFFZSVisualSizingMode::SourceMeshSize;
    ShrinkingSpec.QueueRules.MaxWaitingAgents = 1;
    // The stock head is exactly 50 cm from Service[0]. Give it 100 cm so a .75 scale
    // still preserves the unchanged 2 * 25 cm arrival-separation requirement.
    ShrinkingSpec.QueueSplinePointsCm[0].Y += 50.f;
    ShrinkingSpec.UpgradeTiers[0].TargetLevel = 25;
    ShrinkingSpec.UpgradeTiers[0].SizeMultiplier = .75f;
    ShrinkingSpec.UpgradeTiers[0].ServiceDurationMultiplier = 2.f;
    ShrinkingSpec.UpgradeTiers[0].RevenueMultiplier = 0.f;
    AFFZSFacilityActor* Shrinking = World->SpawnActor<AFFZSFacilityActor>();
    if (!TestTrue(TEXT("A sparse shrinking tier is a valid installed definition"), Shrinking && Shrinking->InitializeFacility(
        FGuid::NewGuid(), ShrinkingSpec, Snapshot.ZoneId, Placement, FTransform::Identity))) return false;
    const FFFZSFacilitySpec ShrinkingCandidate = FFZSFacilityUpgradeRules::BuildEffectiveSpec(ShrinkingSpec, 25, .75f);
    TArray<FText> ShrinkingErrors;
    if (!TestTrue(TEXT("The shrinking candidate preserves physical and queue constraints"), ShrinkingCandidate.IsValid(ShrinkingErrors)))
    {
        for (const FText& Error : ShrinkingErrors) AddError(Error.ToString());
        return false;
    }
    if (!TestTrue(TEXT("An authored level can reduce its real physical size"), Shrinking->ApplyUpgradeState(25, 2.f, .75f, 0.f))) return false;
    TestEqual(TEXT("Shrinking uses the base footprint once"), Shrinking->GetFacilitySpec().FootprintSizeCells,
        FIntPoint(FMath::CeilToInt(ShrinkingSpec.FootprintSizeCells.X * .75f), FMath::CeilToInt(ShrinkingSpec.FootprintSizeCells.Y * .75f)));
    TestTrue(TEXT("Shrinking applies the same scale to the source mesh"), Shrinking->VisualMesh->GetRelativeScale3D().Equals(FVector(.75f)));
    TestEqual(TEXT("Shrinking can intentionally remove facility revenue"), Shrinking->GetUpgradeRevenueMultiplier(), 0.f);
    return true;
}

#endif
