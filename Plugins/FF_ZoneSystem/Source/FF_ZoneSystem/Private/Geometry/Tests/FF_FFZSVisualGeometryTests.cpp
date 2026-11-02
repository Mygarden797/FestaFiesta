// FF_FFZSVisualGeometryTests.cpp
// 역할: 메시의 중심과 크기 보정이 시설 표시 규격을 지키는지 자동 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "FF_FFZSVisualGeometry.h"
#include "FF_FFZSTypes.h"

namespace
{
    bool IsNearlyEqualVector(const FVector& Actual, const FVector& Expected, const double Tolerance = 0.01)
    {
        return Actual.Equals(Expected, Tolerance);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSVisualGeometrySizingTest,
    "FF_ZoneSystem.Unit.VisualGeometry.AuthoritativeSizingModes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSVisualGeometrySizingTest::RunTest(const FString& Parameters)
{
    const FBox SourceBounds(FVector(-100.0, -50.0, 0.0), FVector(100.0, 50.0, 100.0));

    FFFZSFacilitySpec Spec;
    Spec.FootprintSizeCells = FIntPoint(600, 300);
    Spec.PlacementObstacleHeightCm = 250.0f;

    Spec.VisualSizingMode = EFFZSVisualSizingMode::SourceMeshSize;
    const FTransform SourceTransform = FFZSVisualGeometry::ResolveVisualTransformFromBounds(Spec, SourceBounds);
    TestTrue(TEXT("SourceMeshSize는 임포트 크기와 identity transform을 보존한다"),
        SourceTransform.Equals(FTransform::Identity));

    Spec.VisualSizingMode = EFFZSVisualSizingMode::UniformFit;
    const FBox UniformBounds = FFZSVisualGeometry::TransformBounds(
        SourceBounds,
        FFZSVisualGeometry::ResolveVisualTransformFromBounds(Spec, SourceBounds));
    TestTrue(TEXT("UniformFit은 세 축 안에 들어오는 동일 스케일을 사용한다"),
        IsNearlyEqualVector(UniformBounds.GetSize(), FVector(500.0, 250.0, 250.0)));
    TestTrue(TEXT("UniformFit 결과는 footprint 중심과 지면에 정렬된다"),
        IsNearlyEqualVector(
            FVector(UniformBounds.GetCenter().X, UniformBounds.GetCenter().Y, UniformBounds.Min.Z),
            FVector::ZeroVector));

    Spec.VisualSizingMode = EFFZSVisualSizingMode::MatchPlacementBounds;
    const FBox ExactBounds = FFZSVisualGeometry::TransformBounds(
        SourceBounds,
        FFZSVisualGeometry::ResolveVisualTransformFromBounds(Spec, SourceBounds));
    TestTrue(TEXT("MatchPlacementBounds는 footprint와 장애물 높이에 정확히 맞춘다"),
        IsNearlyEqualVector(ExactBounds.GetSize(), FVector(600.0, 300.0, 250.0)));

    Spec.VisualRotation = FRotator(0.0, 90.0, 0.0);
    const FBox RotatedExactBounds = FFZSVisualGeometry::TransformBounds(
        SourceBounds,
        FFZSVisualGeometry::ResolveVisualTransformFromBounds(Spec, SourceBounds));
    TestTrue(TEXT("90도 메시 보정 회전도 최종 권위 X/Y 크기를 보존한다"),
        IsNearlyEqualVector(RotatedExactBounds.GetSize(), FVector(600.0, 300.0, 250.0)));

    Spec.VisualOffsetCm = FVector(10.0, -20.0, 30.0);
    const FBox OffsetBounds = FFZSVisualGeometry::TransformBounds(
        SourceBounds,
        FFZSVisualGeometry::ResolveVisualTransformFromBounds(Spec, SourceBounds));
    TestTrue(TEXT("사용자 오프셋은 자동 중심·지면 정렬 뒤 적용된다"),
        IsNearlyEqualVector(
            FVector(OffsetBounds.GetCenter().X, OffsetBounds.GetCenter().Y, OffsetBounds.Min.Z),
            Spec.VisualOffsetCm));

    TestTrue(TEXT("정확 맞춤은 90도 단위 회전을 허용한다"),
        FFZSVisualGeometry::IsCardinalYawRotation(FRotator(0.0, -90.0, 0.0)));
    TestFalse(TEXT("정확 맞춤은 임의 회전에서 조용히 왜곡된 결과를 만들지 않는다"),
        FFZSVisualGeometry::IsCardinalYawRotation(FRotator(0.0, 45.0, 0.0)));
    TestFalse(TEXT("정확 맞춤은 작은 Yaw 오차도 수용하지 않는다"),
        FFZSVisualGeometry::IsCardinalYawRotation(FRotator(0.0, 90.001, 0.0)));
    TestFalse(TEXT("정확 맞춤은 작은 Pitch 오차도 수용하지 않는다"),
        FFZSVisualGeometry::IsCardinalYawRotation(FRotator(0.001, 90.0, 0.0)));
    TestFalse(TEXT("정확 맞춤은 작은 Roll 오차도 수용하지 않는다"),
        FFZSVisualGeometry::IsCardinalYawRotation(FRotator(0.0, 90.0, 0.001)));
    TestFalse(TEXT("정확 맞춤은 float 해상도보다 작은 Yaw 오차도 수용하지 않는다"),
        FFZSVisualGeometry::IsCardinalYawRotation(FRotator(0.0, 90.0 + 1.0e-10, 0.0)));
    TestFalse(TEXT("정확 맞춤은 float에서 0으로 축소될 Pitch도 수용하지 않는다"),
        FFZSVisualGeometry::IsCardinalYawRotation(FRotator(1.0e-50, 90.0, 0.0)));

    return true;
}

#endif
