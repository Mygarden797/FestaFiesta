// FFZAZoneAuthoringTests.cpp
// 역할: Zone 저작 계층 자동화 테스트.
// 실행: 에디터 Tools > Session Frontend > Automation 에서 "FF_ZoneAuthoring" 검색 후 실행.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FFZAAuthoredZone.h"
#include "FFZADistrict.h"
#include "FFZALayoutOps.h"
#include "FFZAPlot.h"
#include "FFZAPolygonMath.h"
#include "FFZASplineAuthoring.h"
#include "FFZAZoneAuthoringSubsystem.h"
#include "FFZAZoneCellMap.h"
#include "FFZAZoneDefinition.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

namespace FFZAZoneAuthoringTestsPrivate
{
    TArray<FVector2D> MakeRect(const double X0, const double Y0, const double X1, const double Y1)
    {
        return { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1) };
    }
}

// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZAPolygonMathTest,
    "FF_ZoneAuthoring.Unit.PolygonMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAPolygonMathTest::RunTest(const FString& Parameters)
{
    using namespace FFZAZoneAuthoringTestsPrivate;
    const TArray<FVector2D> Square = MakeRect(0, 0, 100, 100);
    TestTrue(TEXT("Square area"), FMath::IsNearlyEqual(FFZAPolygonMath::Area(Square), 10000.0, 1e-6));
    TestTrue(TEXT("Inside"), FFZAPolygonMath::IsPointInside(Square, FVector2D(50, 50)));
    TestTrue(TEXT("Boundary counts as inside"), FFZAPolygonMath::IsPointInside(Square, FVector2D(100, 50)));
    TestFalse(TEXT("Outside"), FFZAPolygonMath::IsPointInside(Square, FVector2D(150, 50)));
    TestFalse(TEXT("Bow tie is not simple"), FFZAPolygonMath::IsSimplePolygon(
        { FVector2D(0, 0), FVector2D(100, 100), FVector2D(100, 0), FVector2D(0, 100) }));

    TestTrue(TEXT("Quarter overlap"), FMath::IsNearlyEqual(
        FFZAPolygonMath::IntersectionAreaWithConvex(Square, MakeRect(50, 50, 150, 150)), 2500.0, 1e-6));
    TestTrue(TEXT("Edge contact is not overlap"),
        FFZAPolygonMath::IntersectionAreaWithConvex(Square, MakeRect(100, 0, 200, 100)) < 1e-6);

    const TArray<FVector2D> LShape = { FVector2D(0, 0), FVector2D(200, 0), FVector2D(200, 100),
        FVector2D(100, 100), FVector2D(100, 200), FVector2D(0, 200) };
    TestTrue(TEXT("Concave notch does not overlap"),
        FFZAPolygonMath::IntersectionAreaWithConvex(LShape, MakeRect(120, 120, 180, 180)) < 1e-6);

    TArray<int32> Indices;
    TestTrue(TEXT("Concave triangulation succeeds"), FFZAPolygonMath::Triangulate(LShape, Indices));
    double TriangleArea = 0.0;
    for (int32 I = 0; I < Indices.Num(); I += 3)
    {
        TriangleArea += FFZAPolygonMath::Area({ LShape[Indices[I]], LShape[Indices[I + 1]], LShape[Indices[I + 2]] });
    }
    TestTrue(TEXT("Triangulation preserves area"), FMath::IsNearlyEqual(TriangleArea, 30000.0, 1e-3));
    return true;
}

// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZAZoneCellMapTest,
    "FF_ZoneAuthoring.Unit.ZoneCellMap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAZoneCellMapTest::RunTest(const FString& Parameters)
{
    using namespace FFZAZoneAuthoringTestsPrivate;
    const TArray<TArray<FVector2D>> Polygons = {
        MakeRect(0, 0, 1000, 1000), MakeRect(1000, 0, 2000, 1000), MakeRect(1500, 500, 2500, 1500), MakeRect(5000, 5000, 6000, 6000)
    };
    FFFZAZoneCellMap Map;
    if (!TestTrue(TEXT("Build"), Map.Build(Polygons, 100.0)))
    {
        return false;
    }
    TestEqual(TEXT("Lookup zone 0"), Map.FindZoneIndexAt(FVector2D(500, 500)), 0);
    TestEqual(TEXT("Lookup outside"), Map.FindZoneIndexAt(FVector2D(3000, 3000)), static_cast<int32>(INDEX_NONE));
    TestTrue(TEXT("Overlap detected"), Map.IsOverlapAt(FVector2D(1700, 700)));
    TestTrue(TEXT("Shared edge is adjacency"), Map.GetAdjacentZonePairs().Contains(FIntPoint(0, 1)));
    TestFalse(TEXT("Far zones are not adjacent"), Map.GetAdjacentZonePairs().Contains(FIntPoint(0, 3)));
    TestEqual(TEXT("One overlap pair"), Map.GetOverlaps().Num(), 1);

    TArray<TArray<FVector2D>> Weld = { MakeRect(0, 0, 1000, 1000), MakeRect(1020, 300, 2000, 700) };
    TArray<bool> Changed;
    TestEqual(TEXT("Weld moves the two near vertices"), FFZALayoutOps::WeldPolygons(Weld, 50.0, Changed), 2);
    TestTrue(TEXT("Welded onto the reference edge"), Weld[1][0].Equals(FVector2D(1000, 300), 1e-6));
    TestEqual(TEXT("Weld is idempotent"), FFZALayoutOps::WeldPolygons(Weld, 50.0, Changed), 0);
    return true;
}

// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZAAuthoredZoneNormalizeTest,
    "FF_ZoneAuthoring.Integration.AuthoredZoneNormalizesAndAppliesDefinition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAuthoredZoneNormalizeTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> Presets(GetMutableDefault<UFFZSDeveloperSettings>()->bRestrictZoneIdsToProjectPresets, false);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("World"), World))
    {
        return false;
    }
    AFFZAAuthoredZone* Zone = World->SpawnActor<AFFZAAuthoredZone>();
    if (!TestNotNull(TEXT("Zone"), Zone) || !TestNotNull(TEXT("Spline"), Zone->BoundarySpline.Get()))
    {
        return false;
    }

    // 디자이너가 Scale/회전/소수점 위치를 막 바꾼 상황
    Zone->SetActorTransform(FTransform(FRotator(0, 37, 0), FVector(100.4, 200.6, 0), FVector(2, 3, 1)));
    TArray<FVector> Before;
    for (int32 I = 0; I < Zone->BoundarySpline->GetNumberOfSplinePoints(); ++I)
    {
        Before.Add(Zone->BoundarySpline->GetLocationAtSplinePoint(I, ESplineCoordinateSpace::World));
    }
    Zone->NormalizeTransformNow();

    TestTrue(TEXT("Scale reset to 1"), Zone->GetActorScale3D().Equals(FVector::OneVector, 1e-4));
    TestTrue(TEXT("Yaw snapped to 0/90/180/270"),
        FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(Zone->GetActorRotation().Yaw), 90.0), 1e-3));
    TestTrue(TEXT("Origin X whole cm"), FMath::IsNearlyEqual(Zone->GetActorLocation().X, FMath::RoundToDouble(Zone->GetActorLocation().X), 1e-3));
    for (int32 I = 0; I < Before.Num(); ++I)
    {
        TestTrue(TEXT("Visible boundary is unchanged"),
            Before[I].Equals(Zone->BoundarySpline->GetLocationAtSplinePoint(I, ESplineCoordinateSpace::World), 0.01));
    }

    UFFZAZoneDefinition* Definition = NewObject<UFFZAZoneDefinition>(GetTransientPackage());
    Definition->ZoneId = TEXT("GZ_AuthoringTest");
    Definition->bAllowAllFacilityCategories = false;
    Definition->AllowedFacilityCategories = { EFFZSFacilityCategory::Booth };
    Zone->ZoneDefinition = Definition;
    Zone->ApplyDefinition();
    TestTrue(TEXT("ZoneId comes from definition"), Zone->ZoneId == FName(TEXT("GZ_AuthoringTest")));
    TestFalse(TEXT("Category rule comes from definition"), Zone->bAllowAllFacilityCategories);
    TestFalse(TEXT("No longer out of sync"), Zone->IsOutOfSyncWithDefinition());

    TArray<FText> Issues;
    Zone->CollectAuthoringIssues(Issues);
    TestEqual(TEXT("Normalized zone with definition has no issues"), Issues.Num(), 0);
    return true;
}

// ---------------------------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZAPlotDistrictPolicyTest,
    "FF_ZoneAuthoring.Integration.PlotAndDistrictRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAPlotDistrictPolicyTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> Presets(GetMutableDefault<UFFZSDeveloperSettings>()->bRestrictZoneIdsToProjectPresets, false);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    UFFZAZoneAuthoringSubsystem* Authoring = World ? World->GetSubsystem<UFFZAZoneAuthoringSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("World"), World) || !TestNotNull(TEXT("Zone system"), ZoneSystem) ||
        !TestNotNull(TEXT("Authoring subsystem"), Authoring))
    {
        return false;
    }

    // 100 m × 100 m 기본 Zone(원점 0,0)
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    Zone->ZoneId = TEXT("GZ_PolicyTest");
    if (!TestTrue(TEXT("Zone registers"), ZoneSystem->RegisterZone(Zone)))
    {
        return false;
    }

    // 잠긴 구획: (2000,2000)~(4000,4000)
    AFFZAPlot* Plot = World->SpawnActor<AFFZAPlot>(FVector(2000, 2000, 0), FRotator::ZeroRotator);
    Plot->AreaId = TEXT("PLOT_Test");
    // 구역: (6000,6000)~(8000,8000), Toilet만 허용
    AFFZADistrict* District = World->SpawnActor<AFFZADistrict>(FVector(6000, 6000, 0), FRotator::ZeroRotator);
    District->AreaId = TEXT("DISTRICT_Test");
    District->AllowedFacilityCategories = { EFFZSFacilityCategory::Toilet };
    TestEqual(TEXT("Plot registered"), Authoring->GetPlots().Num(), 1);
    TestEqual(TEXT("District registered"), Authoring->GetDistricts().Num(), 1);

    const auto MakeContext = [](const FVector2D Center, const EFFZSFacilityCategory Category)
    {
        FFFZSPolicyContext Context;
        Context.Command.Type = EFFZSCommandType::Install;
        Context.Command.ZoneId = TEXT("GZ_PolicyTest");
        Context.Command.FacilitySpec.Category = Category;
        Context.Validation.ReservedOrientedRects.Add(FFFZSOrientedRect(Center, FVector2D(300, 300), 0.0f));
        return Context;
    };

    TestFalse(TEXT("Locked plot rejects"),
        Authoring->EvaluatePlacement(MakeContext(FVector2D(3000, 3000), EFFZSFacilityCategory::Booth)).bAllowed);
    TestTrue(TEXT("Outside plot allows"),
        Authoring->EvaluatePlacement(MakeContext(FVector2D(1000, 1000), EFFZSFacilityCategory::Booth)).bAllowed);
    TestTrue(TEXT("Unlock succeeds"), Authoring->UnlockPlot(TEXT("PLOT_Test")));
    TestTrue(TEXT("Unlocked plot allows"),
        Authoring->EvaluatePlacement(MakeContext(FVector2D(3000, 3000), EFFZSFacilityCategory::Booth)).bAllowed);

    const FFFZSPolicyDecision BoothInDistrict = Authoring->EvaluatePlacement(MakeContext(FVector2D(7000, 7000), EFFZSFacilityCategory::Booth));
    TestFalse(TEXT("District rejects disallowed category"), BoothInDistrict.bAllowed);
    TestTrue(TEXT("District uses CategoryNotAllowed"), BoothInDistrict.FailureReason == EFFZSPlacementFailure::CategoryNotAllowed);
    TestTrue(TEXT("District allows listed category"),
        Authoring->EvaluatePlacement(MakeContext(FVector2D(7000, 7000), EFFZSFacilityCategory::Toilet)).bAllowed);

    TestEqual(TEXT("Save list contains unlocked plot"), Authoring->GetUnlockedPlotIds().Num(), 1);
    Authoring->SetUnlockedPlotIds({});
    TestFalse(TEXT("Restore relocks"), Authoring->IsPlotUnlocked(TEXT("PLOT_Test")));

    TestTrue(TEXT("Cell map lookup"), Authoring->FindZoneIdAtWorldLocation(FVector(5000, 5000, 0)) == FName(TEXT("GZ_PolicyTest")));
    return true;
}

#endif
