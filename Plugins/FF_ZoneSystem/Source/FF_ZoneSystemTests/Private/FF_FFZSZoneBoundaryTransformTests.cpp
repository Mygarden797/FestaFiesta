// The boundary shown by a transformed spline component must match placement authority.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSZoneBoundaryComponentTransformTest,
    "FF_ZoneSystem.Integration.Zone.VisibleSplineTransformMatchesBuildBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSZoneBoundaryComponentTransformTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TGuardValue<bool> Presets(GetMutableDefault<UFFZSDeveloperSettings>()->bRestrictZoneIdsToProjectPresets, false);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Test world"), World) || !TestNotNull(TEXT("Zone subsystem"), System)) return false;
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    AFFZSBuildArea* Area = World->SpawnActor<AFFZSBuildArea>();
    if (!TestNotNull(TEXT("Zone"), Zone) || !TestNotNull(TEXT("Build area"), Area)) return false;
    Zone->ZoneId = TEXT("GZ_VisibleBoundaryTransform");
    Zone->SetActorTransform(FTransform(FRotator(0, 90, 0), FVector(2000, 3000, 0)));
    USplineComponent* Spline = Zone->BoundarySpline;
    Spline->ClearSplinePoints(false);
    for (const FVector& Point : {FVector(0, 0, 0), FVector(100, 0, 0), FVector(100, 100, 0), FVector(0, 100, 0)})
    {
        Spline->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
        Spline->SetSplinePointType(Spline->GetNumberOfSplinePoints() - 1, ESplinePointType::Linear, false);
    }
    Spline->SetClosedLoop(true, true);
    Spline->SetRelativeTransform(FTransform(FRotator(0, 30, 0), FVector(500, 800, 0), FVector(2, 3, 1)));
    const TArray<FVector2D> Polygon = Zone->GetBoundaryPolygonLocalCm();
    if (!TestEqual(TEXT("Every visible spline vertex reaches the authority polygon"), Polygon.Num(), 4)) return false;
    for (int32 Index = 0; Index < Polygon.Num(); ++Index)
    {
        const FVector PolygonWorld = Zone->GetActorTransform().TransformPosition(FVector(Polygon[Index], 0));
        TestTrue(TEXT("Authority vertex exactly matches the component's visible world position"),
            PolygonWorld.Equals(Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World), 0.001));
    }
    TestTrue(TEXT("Component scaling is reflected in authoritative area"),
        FMath::IsNearlyEqual(Zone->GetBoundaryAreaSquareMeters(), 6.0f, 0.001f));
    const FVector VisibleCenter = Spline->GetComponentTransform().TransformPosition(FVector(50, 50, 0));
    const FVector OldUntransformedCenter = Zone->GetActorTransform().TransformPosition(FVector(50, 50, 0));
    TestTrue(TEXT("The visible interior belongs to the zone"), Zone->ContainsWorldPoint(VisibleCenter));
    TestFalse(TEXT("The obsolete untransformed boundary grants no buildable area"), Zone->ContainsWorldPoint(OldUntransformedCenter));
    if (!TestTrue(TEXT("Transformed zone registers"), System->RegisterZone(Zone)) ||
        !TestTrue(TEXT("Build area registers"), System->RegisterBuildArea(Area))) return false;
    TestTrue(TEXT("The union accepts the same visible interior"), Area->ContainsWorldPoint(VisibleCenter));
    TestFalse(TEXT("The union rejects the same obsolete interior"), Area->ContainsWorldPoint(OldUntransformedCenter));
    TArray<FFFZSBuildAreaLineSegment> Lines;
    Area->GetBoundarySegments(0, FBox2D(FVector2D(VisibleCenter) - FVector2D(500),
        FVector2D(VisibleCenter) + FVector2D(500)), Lines);
    TestEqual(TEXT("The presentation exports the transformed polygon's four actual edges"), Lines.Num(), 4);
    for (const FFFZSBuildAreaLineSegment& Line : Lines)
    {
        TestTrue(TEXT("The drawn edge remains an allowed boundary contact"),
            Area->ContainsWorldPoint(FVector((Line.Start + Line.End) * 0.5, 0)));
    }
    return true;
}

#endif
