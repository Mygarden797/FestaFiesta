#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "FFZASplineAuthoring.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldSplineHeightsTest,
    "FF_ZoneAuthoring.Unit.WeldSpline.HeightsAndTransform",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldSplineHeightsTest::RunTest(const FString& Parameters)
{
    USplineComponent* Spline = NewObject<USplineComponent>(GetTransientPackage());
    Spline->SetWorldTransform(FTransform(FRotator(0, 37, 0), FVector(300, -700, 50), FVector(2, 3, 1)));
    const TArray<FVector> Original = {
        FVector(0, 0, 10), FVector(1000, 0, 30), FVector(1000, 1000, 70), FVector(0, 1000, 90)
    };
    Spline->SetSplinePoints(Original, ESplineCoordinateSpace::World, false);
    FFZASplineAuthoring::EnsureClosedLinear(*Spline);

    const TArray<FVector2D> Welded = {
        FVector2D(20, 0), FVector2D(500, 0), FVector2D(1000, 0),
        FVector2D(1000, 1000), FVector2D(0, 1000), FVector2D(0, 500)
    };
    const TArray<double> Keys = { 0.0, 0.5, 1.0, 2.0, 3.0, 3.5 };
    TestTrue(TEXT("Apply noded boundary without a level"),
        FFZASplineAuthoring::SetWorldPolygonPreservingHeights(*Spline, Welded, Keys));
    const TArray<double> ExpectedHeights = { 10.0, 20.0, 30.0, 70.0, 90.0, 50.0 };
    TestEqual(TEXT("Both inserted points retained"), Spline->GetNumberOfSplinePoints(), Welded.Num());
    for (int32 Index = 0; Index < Welded.Num(); ++Index)
    {
        const FVector World = Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World);
        TestTrue(TEXT("Welded world XY survives nonuniform component transform"),
            FVector2D(World.X, World.Y).Equals(Welded[Index], 1e-6));
        TestEqual(TEXT("Original heights preserved and inserted heights interpolated"), World.Z, ExpectedHeights[Index], 1e-6);
        TestEqual(TEXT("Boundary remains linear"), Spline->GetSplinePointType(Index), ESplinePointType::Linear);
    }
    TestTrue(TEXT("Boundary remains closed"), Spline->IsClosedLoop());
    TArray<FVector2D> Moved = Welded;
    Moved[0].X += 5.0;
    TestTrue(TEXT("Apply a vertex move without changing point count"),
        FFZASplineAuthoring::SetWorldPolygonPreservingHeights(*Spline, Moved, { 0.0, 1.0, 2.0, 3.0, 4.0, 5.0 }));
    for (int32 Index = 0; Index < ExpectedHeights.Num(); ++Index)
    {
        TestEqual(TEXT("Same-count movement retains each original Z"),
            Spline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World).Z, ExpectedHeights[Index], 1e-6);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldSplineInvalidKeysTest,
    "FF_ZoneAuthoring.Unit.WeldSpline.RejectInvalidWriteback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldSplineInvalidKeysTest::RunTest(const FString& Parameters)
{
    USplineComponent* Spline = NewObject<USplineComponent>(GetTransientPackage());
    FFZASplineAuthoring::SetLocalRectangle(*Spline, FVector2D(1000, 1000));
    TArray<FVector2D> Before;
    FFZASplineAuthoring::GetWorldPolygon(*Spline, Before);
    const TArray<TArray<double>> InvalidKeys = {
        { 0.0, 1.0, 2.0 }, { 0.0, 1.0, 2.0, 4.0 }, { -0.5, 1.0, 2.0, 3.0 },
        { 0.0, 1.0, 2.0, std::numeric_limits<double>::quiet_NaN() },
        { 0.0, 1.0, 2.0, std::numeric_limits<double>::infinity() }
    };
    for (const TArray<double>& Keys : InvalidKeys)
    {
        TestFalse(TEXT("Reject mismatched count or out-of-range source key"),
            FFZASplineAuthoring::SetWorldPolygonPreservingHeights(*Spline, Before, Keys));
        TArray<FVector2D> After;
        FFZASplineAuthoring::GetWorldPolygon(*Spline, After);
        TestTrue(TEXT("Rejection leaves all spline points intact"), Before == After);
    }
    Spline->SetClosedLoop(false);
    TestFalse(TEXT("Open boundary cannot be silently repaired by weld"),
        FFZASplineAuthoring::SetWorldPolygonPreservingHeights(*Spline, Before, { 0.0, 1.0, 2.0, 3.0 }));
    Spline->SetClosedLoop(true);
    Spline->SetSplinePointType(0, ESplinePointType::Curve);
    TestFalse(TEXT("Curved boundary cannot be silently flattened by weld"),
        FFZASplineAuthoring::SetWorldPolygonPreservingHeights(*Spline, Before, { 0.0, 1.0, 2.0, 3.0 }));
    return true;
}

#endif
