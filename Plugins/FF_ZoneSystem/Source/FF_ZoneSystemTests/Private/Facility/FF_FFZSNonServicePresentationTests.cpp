// Roads, tiles and lights discard legacy service geometry in previews and installed actors.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityAnchorComponent.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_FFZSTestFixtures.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSNonServicePresentationTest,
    "FF_ZoneSystem.Integration.Placement.NonServicePresentationHasOnlyFootprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSNonServicePresentationTest::RunTest(const FString& Parameters)
{
    UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
    TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
    TGuardValue<bool> Navigation(Settings->bEnableNavigationProjectionChecks, false);
    TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
    TGuardValue<bool> ZoneRestriction(Settings->bRestrictZoneIdsToProjectPresets, false);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* Subsystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Presentation test world"), World) ||
        !TestNotNull(TEXT("Placement subsystem"), Subsystem)) return false;

    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("Presentation test zone"), Zone)) return false;
    Zone->ZoneId = TEXT("GZ_NonServicePresentation");
    Zone->BoundarySpline->ClearSplinePoints(false);
    for (const FVector& Point : { FVector(0, 0, 0), FVector(20000, 0, 0),
        FVector(20000, 20000, 0), FVector(0, 20000, 0) })
    {
        const int32 Index = Zone->BoundarySpline->GetNumberOfSplinePoints();
        Zone->BoundarySpline->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
        Zone->BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    Zone->BoundarySpline->SetClosedLoop(true, true);
    if (!TestTrue(TEXT("Presentation zone registers"), Subsystem->RegisterZone(Zone))) return false;

    const EFFZSFacilityCategory Categories[] = {
        EFFZSFacilityCategory::Road, EFFZSFacilityCategory::Tile, EFFZSFacilityCategory::Lighting
    };
    for (int32 CaseIndex = 0; CaseIndex < UE_ARRAY_COUNT(Categories); ++CaseIndex)
    {
        // Deliberately retain the legacy booth's clearance, queue rectangles, spline and anchors.
        FFFZSFacilitySpec LegacySpec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
        LegacySpec.Category = Categories[CaseIndex];
        FFFZSGridPlacement Placement;
        Placement.OriginCell = FIntPoint(2000 + CaseIndex * 4000, 2000);
        Placement.YawCentidegrees = 3700;
        const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(LegacySpec, Zone->ZoneId, Placement);

        AFFZSPlacementPreviewActor* Preview = World->SpawnActor<AFFZSPlacementPreviewActor>();
        if (!TestNotNull(TEXT("Placement preview"), Preview)) return false;
        Preview->PreviewCommand = Install;
        const FFFZSPlacementValidation Validation = Preview->RefreshPreview();
        if (!TestTrue(TEXT("Legacy non-service spec previews successfully"), Validation.bIsValid)) return false;
        TestTrue(TEXT("Preview has no clearance regions"), Validation.ClearanceOrientedRects.IsEmpty());
        TestTrue(TEXT("Preview has no queue regions"), Validation.QueueOrientedRects.IsEmpty());
        TestTrue(TEXT("Preview draws the footprint"), !Preview->RangeLines->BatchedLines.IsEmpty());
        const FVector2D HalfExtent = FVector2D(LegacySpec.FootprintSizeCells) * 0.5;
        const auto IsWithinFootprint = [&Validation, HalfExtent](const FVector& Point)
        {
            const FVector Local = Validation.PlacementTransform.InverseTransformPosition(Point);
            return FMath::Abs(Local.X) <= HalfExtent.X + 0.01 &&
                FMath::Abs(Local.Y) <= HalfExtent.Y + 0.01;
        };
        for (const FBatchedLine& Line : Preview->RangeLines->BatchedLines)
            TestTrue(TEXT("Preview has no lines outside the rotated footprint"),
                IsWithinFootprint(Line.Start) && IsWithinFootprint(Line.End));
        Preview->Destroy();

        if (!TestTrue(TEXT("Legacy non-service facility installs"), Subsystem->ExecuteCommand(Install).bSucceeded)) return false;
        AFFZSFacilityActor* Facility = Subsystem->FindFacility(Install.FacilityInstanceId);
        if (!TestNotNull(TEXT("Installed non-service facility"), Facility)) return false;
        TestEqual(TEXT("Saved base spec removes legacy clearance"), Facility->GetBaseFacilitySpec().ClearanceCells, 0);
        TestTrue(TEXT("Saved base spec removes legacy queue"), Facility->GetBaseFacilitySpec().QueueReservedRects.IsEmpty());
        TestEqual(TEXT("Actor does not create queue spline points"), Facility->QueueSpline->GetNumberOfSplinePoints(), 0);
        TestTrue(TEXT("Actor queue bounds are empty"), Facility->QueueAreaBounds->GetUnscaledBoxExtent().IsZero());
        TInlineComponentArray<UFFZSFacilityAnchorComponent*> Anchors(Facility);
        TestTrue(TEXT("Actor does not create service anchors"), Anchors.IsEmpty());
        TestFalse(TEXT("Non-service read model has no NPC approach target"), Facility->GetReadModel().bHasApproachWorldTransform);
        for (UBoxComponent* Box : TInlineComponentArray<UBoxComponent*>(Facility))
        {
            if (Box->GetFName() != TEXT("FacilityQueueLaneNavCostBounds")) continue;
            TestTrue(TEXT("Queue navigation bounds are empty"), Box->GetUnscaledBoxExtent().IsZero());
            TestFalse(TEXT("Queue navigation cost is disabled"), Box->CanEverAffectNavigation());
        }
        Facility->SetBuildModeRangeVisible(true);
        bool bFoundRangeLines = false;
        for (ULineBatchComponent* Lines : TInlineComponentArray<ULineBatchComponent*>(Facility))
        {
            if (Lines->GetFName() != TEXT("BuildModeRangeLines")) continue;
            bFoundRangeLines = !Lines->BatchedLines.IsEmpty();
            for (const FBatchedLine& Line : Lines->BatchedLines)
                TestTrue(TEXT("Installed range contains only the footprint"),
                    IsWithinFootprint(Line.Start) && IsWithinFootprint(Line.End));
        }
        TestTrue(TEXT("Installed footprint range remains visible"), bFoundRangeLines);

        FFFZSCommand Open;
        Open.CommandId = FGuid::NewGuid();
        Open.TransactionId = FGuid::NewGuid();
        Open.Type = EFFZSCommandType::Open;
        Open.FacilityInstanceId = Install.FacilityInstanceId;
        TestTrue(TEXT("Non-service facility can become active"), Subsystem->ExecuteCommand(Open).bSucceeded);
        TestEqual(TEXT("Active facility has no open queue"), Facility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Closed);
        TestEqual(TEXT("Queue has no waiting capacity"), Facility->GetQueueComponent()->GetRules().MaxWaitingAgents, 0);
        TestEqual(TEXT("Queue has no service capacity"), Facility->GetQueueComponent()->GetRules().ServiceCapacity, 0);
        TestFalse(TEXT("Non-service queue never ticks"), Facility->GetQueueComponent()->IsComponentTickEnabled());
        FGuid Ticket;
        TestEqual(TEXT("Visitor cannot join a non-service facility"),
            Subsystem->JoinFacilityQueue(Install.FacilityInstanceId, TEXT("NoServiceVisitor"), nullptr, Ticket),
            EFFZSQueueJoinResult::NotOpen);
        TestFalse(TEXT("Non-service queue assigns no ticket"), Ticket.IsValid());
    }
    return true;
}

#endif
