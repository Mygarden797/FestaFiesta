// FF_FFZSGridTests.cpp

// 1 cm Grid와 압축 Cell Rect의 회전·합집합·예산·디버그 펼치기 계약을 검증한다.
// 흐름: 대형 테스트 fixture를 개별 Cell로 펼치지 않고, 작은 합성 Spec을 Region API에 넣어
// 반열림 사각형의 좌표·면적·분리·결정적 합집합을 확인한다.
// 불변식: 1 Unreal Unit=1 cm, 1 배치 Cell=1 cm이며 권위 계산은 FFFZSCellRect 배열을 사용한다.
// 실패/성능: 개별 Cell 호환 보기는 최대 10,000개에서만 허용되고 초과 시 빈 결과로 fail-closed한다.
 

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSOrientedGeometry.h"
#include "Sound/SoundBase.h"
#include "Tests/FF_FFZSTestFixtures.h"

#include <limits>

namespace
{
    // Region 연산의 정확한 좌표와 면적을 사람이 계산할 수 있도록 만든 작은 합성 사양이다.
     // footprint=[0,3)x[0,2), legacy clearance=1 (ignored), queue=[3,5)x[0,1).
     // 대형 1 cm fixture는 이 테스트에서 Get*Cells로 펼치지 않는다.
     
    FFFZSFacilitySpec MakeSmallRegionSpec()
    {
        FFFZSFacilitySpec Spec;
        Spec.SchemaVersion = 2;
        Spec.FootprintSizeCells = FIntPoint(3, 2);
        Spec.ClearanceCells = 1;
        Spec.QueueReservedRects = {
            FFFZSCellRect(FIntPoint(3, 0), FIntPoint(5, 1))
        };
        Spec.QueueReservedCells.Reset();
        return Spec;
    }

    bool AreOrientedRectArraysEquivalent(
        const TArray<FFFZSOrientedRect>& A,
        const TArray<FFFZSOrientedRect>& B)
    {
        if (A.Num() != B.Num())
        {
            return false;
        }

        for (int32 Index = 0; Index < A.Num(); ++Index)
        {
            if (!A[Index].CenterCell.Equals(B[Index].CenterCell, 1.0e-9) ||
                !A[Index].HalfExtentCells.Equals(B[Index].HalfExtentCells, 1.0e-9) ||
                !FMath::IsNearlyEqual(A[Index].YawDegrees, B[Index].YawDegrees, 1.0e-6f))
            {
                return false;
            }
        }
        return true;
    }
}

// 테스트 매트릭스
 // Units: 1 uu=1 cm, 1 Cell=1 cm, 디버그 펼치기 10,000 Cell 상한
 // Rotation: FIntPoint와 [Min,Max) Cell Rect의 90도 회전
 // CompressedRegions: 작은 합성 Spec의 footprint/clearance/queue/reserved 압축 영역
 // RegionUnion/Budget: 겹침·맞닿음 합집합, 보수적 작업 예산, 10k 초과 fail-closed
 // Commands: QueueReservedRects를 포함한 전체 의미 필드의 문화권 독립 Payload 지문
 // 각 케이스는 Arrange -> Act -> Assert 순서를 유지하며 대형 fixture를 개별 셀로 열거하지 않는다.
 
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSOneCentimeterUnitContractTest,
    "FF_ZoneSystem.Unit.Units.OnePlacementCellEqualsOneCentimeter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOneCentimeterUnitContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    // Arrange/Act/Assert: 월드 없이 코드 상수만 읽어 물리 단위와 안전 상한을 고정한다.
    TestEqual(TEXT("배치 Cell 하나는 정확히 1센티미터이자 Unreal Unit 1이다"),
        UFFZSDeveloperSettings::DefaultPlacementCellSizeCm, 1.0);
    TestEqual(TEXT("디버그 호환 배열은 최대 Cell 10,000개까지만 구체화한다"),
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, 10'000);
    TestEqual(TEXT("지면 샘플링은 1센티미터 점유보다 더 거친 간격을 유지한다"),
        UFFZSDeveloperSettings::DefaultGroundSampleSpacingCm, 100.0f);
    TestEqual(TEXT("대기열 간격은 Cell 저장 방식과 무관하게 센티미터 값을 유지한다"),
        FFZSTest::MakeValidFacilitySpec(
            EFFZSTestFacilityVariant::FoodTruck).QueueRules.SlotSpacingCm,
        115.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSCellAndRectRotationTest,
    "FF_ZoneSystem.Unit.Grid.OneCentimeterCellAndRectQuarterTurns",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSCellAndRectRotationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    // Arrange: 비대칭 Cell과 반열림 Rect를 사용해 90/180/270도 결과를 서로 구분한다.
    const FIntPoint SourceCell(2, 1);
    const FFFZSCellRect SourceRect(FIntPoint(1, 2), FIntPoint(4, 6));
    const FFFZSCellRect UnitRect(FIntPoint(0, 0), FIntPoint(1, 1));
    // 단일 Cell Rect는 RotateCell(0,0)의 규칙과 정확히 같아야 한다. 연속 좌표식 -Max를 쓰면
    // [-1,0)으로 한 칸 밀리는 회귀를 이 단언이 잡는다.
    TestTrue(TEXT("단일 Cell Rect의 q1이 RotateCell 인덱스 의미와 일치한다"),
        UFFZSGridLibrary::RotateRect(UnitRect, 1) == UnitRect);
    FFFZSCellRect FourTurnRect = SourceRect;
    for (int32 Turn = 0; Turn < 4; ++Turn)
    {
        FourTurnRect = UFFZSGridLibrary::RotateRect(FourTurnRect, 1);
    }
    TestTrue(TEXT("Rect를 연속 네 번 90도 회전하면 정확히 원본으로 돌아온다"),
        FourTurnRect == SourceRect);

    // Act/Assert: Cell 회전은 정수 좌표, Rect 회전은 전체 [Min,Max) 영역을 보존해야 한다.
    TestEqual(TEXT("90도 회전 0회의 Cell 결과가 일치한다"),
        UFFZSGridLibrary::RotateCell(SourceCell, 0), FIntPoint(2, 1));
    TestEqual(TEXT("시계 방향으로 90도 1회 회전한 Cell 결과가 일치한다"),
        UFFZSGridLibrary::RotateCell(SourceCell, 1), FIntPoint(-1, 2));
    TestEqual(TEXT("시계 방향으로 90도 2회 회전한 Cell 결과가 일치한다"),
        UFFZSGridLibrary::RotateCell(SourceCell, 2), FIntPoint(-2, -1));
    TestEqual(TEXT("시계 방향으로 90도 3회 회전한 Cell 결과가 일치한다"),
        UFFZSGridLibrary::RotateCell(SourceCell, 3), FIntPoint(1, -2));
    TestEqual(TEXT("음수 Cell 회전 횟수는 3으로 정규화된다"),
        UFFZSGridLibrary::RotateCell(SourceCell, -1), FIntPoint(1, -2));

    // Negating MIN_int32 is not representable by int32. The checked API must reject the
    // operation without mutating the caller's output, while identity remains representable.
    FIntPoint CheckedRotation(77, 88);
    TestTrue(TEXT("MIN_int32 coordinates remain valid when no negation is required"),
        UFFZSGridLibrary::TryRotateCell(
            FIntPoint(MIN_int32, 42), 0, CheckedRotation));
    TestEqual(TEXT("The checked identity rotation preserves MIN_int32 exactly"),
        CheckedRotation, FIntPoint(MIN_int32, 42));

    CheckedRotation = FIntPoint(77, 88);
    TestFalse(TEXT("A quarter turn rejects negating a MIN_int32 Y coordinate"),
        UFFZSGridLibrary::TryRotateCell(
            FIntPoint(42, MIN_int32), 1, CheckedRotation));
    TestEqual(TEXT("A failed checked rotation leaves the output untouched"),
        CheckedRotation, FIntPoint(77, 88));

    CheckedRotation = FIntPoint(77, 88);
    TestFalse(TEXT("A half turn rejects negating a MIN_int32 X coordinate"),
        UFFZSGridLibrary::TryRotateCell(
            FIntPoint(MIN_int32, 42), 2, CheckedRotation));
    TestEqual(TEXT("The legacy value-returning API fails closed at the integer boundary"),
        UFFZSGridLibrary::RotateCell(FIntPoint(MIN_int32, 42), 2),
        FIntPoint::ZeroValue);

    const FFFZSCellRect RotatedOnce = UFFZSGridLibrary::RotateRect(SourceRect, 1);
    const FFFZSCellRect RotatedTwice = UFFZSGridLibrary::RotateRect(SourceRect, 2);
    const FFFZSCellRect RotatedThrice = UFFZSGridLibrary::RotateRect(SourceRect, 3);
    TestTrue(TEXT("Rect 1회 회전 경계가 포함된 모든 Cell을 보존한다"),
        RotatedOnce == FFFZSCellRect(FIntPoint(-5, 1), FIntPoint(-1, 4)));
    TestTrue(TEXT("Rect 2회 회전 경계가 포함된 모든 Cell을 보존한다"),
        RotatedTwice == FFFZSCellRect(FIntPoint(-3, -5), FIntPoint(0, -1)));
    TestTrue(TEXT("Rect 3회 회전 경계가 포함된 모든 Cell을 보존한다"),
        RotatedThrice == FFFZSCellRect(FIntPoint(2, -3), FIntPoint(6, 0)));
    TestEqual(TEXT("Rect 면적은 회전해도 변하지 않는다"),
        RotatedOnce.GetArea(), SourceRect.GetArea());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSCompressedRegionContractTest,
    "FF_ZoneSystem.Unit.Grid.OneCentimeterCompressedRegions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSCompressedRegionContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    // 다른 프로젝트의 담당자 설정이 낮아도 이 단위 테스트의 기준 입력은 동일해야 한다.
    UFFZSDeveloperSettings* MutableSettings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<int32> StableRegionBudget(
        MutableSettings->MaxEnumeratedCellsPerFacility, 1'000'000);

    // Arrange: 작은 3x2 Cell 합성 사양만 사용해 호환 Cell 배열도 안전하게 확인한다.
    const FFFZSFacilitySpec Spec = MakeSmallRegionSpec();
    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(10, 20);
    Placement.QuarterTurns = 1;

    // Act: 권위 Region API를 먼저 계산한다.
    const TArray<FFFZSCellRect> FootprintRects =
        UFFZSGridLibrary::GetFootprintRects(Spec, Placement);
    const TArray<FFFZSCellRect> ClearanceRects =
        UFFZSGridLibrary::GetClearanceRects(Spec, Placement);
    const TArray<FFFZSCellRect> QueueRects =
        UFFZSGridLibrary::GetQueueRects(Spec, Placement);
    const TArray<FFFZSCellRect> ReservedRects =
        UFFZSGridLibrary::GetReservedRects(Spec, Placement);

    // Assert: 시계 방향 90도 회전 후 Origin을 더한 반열림 좌표를 정확히 고정한다.
    TestEqual(TEXT("작은 합성 점유 영역은 압축 Rect 하나로 유지된다"),
        FootprintRects.Num(), 1);
    TestTrue(TEXT("회전된 점유 영역의 경계가 정확하다"),
        FootprintRects.Num() == 1 &&
        FootprintRects[0] == FFFZSCellRect(FIntPoint(9, 20), FIntPoint(11, 23)));
    TestEqual(TEXT("작은 합성 대기열은 압축 Rect 하나로 유지된다"),
        QueueRects.Num(), 1);
    TestTrue(TEXT("회전된 대기열 경계가 정확하다"),
        QueueRects.Num() == 1 &&
        QueueRects[0] == FFFZSCellRect(FIntPoint(10, 23), FIntPoint(11, 25)));

    TestEqual(TEXT("점유 Region이 1센티미터 Cell 6개를 포함한다"),
        UFFZSGridLibrary::GetCellRectArea(FootprintRects), static_cast<int64>(6));
    TestEqual(TEXT("대기열 Region이 1센티미터 Cell 2개를 포함한다"),
        UFFZSGridLibrary::GetCellRectArea(QueueRects), static_cast<int64>(2));
    TestEqual(TEXT("Legacy clearance does not reserve additional space"),
        UFFZSGridLibrary::GetCellRectArea(ClearanceRects), static_cast<int64>(0));
    TestEqual(TEXT("예약 Region은 중복 계산 없는 정확한 합집합이다"),
        UFFZSGridLibrary::GetCellRectArea(ReservedRects), static_cast<int64>(8));

    TestFalse(TEXT("점유 영역과 여유 Region이 서로 겹치지 않는다"),
        UFFZSGridLibrary::DoCellRectSetsOverlap(FootprintRects, ClearanceRects));
    TestFalse(TEXT("점유 영역과 대기열 Region은 경계에서만 맞닿는다"),
        UFFZSGridLibrary::DoCellRectSetsOverlap(FootprintRects, QueueRects));
    TestFalse(TEXT("여유 영역에서 대기열 Region이 제외된다"),
        UFFZSGridLibrary::DoCellRectSetsOverlap(ClearanceRects, QueueRects));
    TestTrue(TEXT("예약 Region에 회전된 점유 Cell이 포함된다"),
        UFFZSGridLibrary::IsCellInRects(FIntPoint(9, 20), ReservedRects));
    TestTrue(TEXT("예약 Region에 대기열 끝 Cell이 포함된다"),
        UFFZSGridLibrary::IsCellInRects(FIntPoint(10, 24), ReservedRects));
    TestFalse(TEXT("예약 Region에서 관련 없는 Cell이 제외된다"),
        UFFZSGridLibrary::IsCellInRects(FIntPoint(11, 25), ReservedRects));

    // 작은 합성 입력에서만 구 호환 Get*Cells를 펼쳐 Region 면적과 같은지 확인한다.
    TestEqual(TEXT("작은 디버그 점유 영역의 모든 Cell이 구체화된다"),
        UFFZSGridLibrary::GetFootprintCells(Spec, Placement).Num(), 6);
    TestEqual(TEXT("Legacy clearance has no debug cells"),
        UFFZSGridLibrary::GetClearanceCells(Spec, Placement).Num(), 0);
    TestEqual(TEXT("작은 디버그 대기열의 모든 Cell이 구체화된다"),
        UFFZSGridLibrary::GetQueueCells(Spec, Placement).Num(), 2);
    TestEqual(TEXT("작은 디버그 예약 영역이 정확한 합집합으로 구체화된다"),
        UFFZSGridLibrary::GetReservedCells(Spec, Placement).Num(), 8);

    // Arrange/Act: 겹치고 맞닿는 Rect 입력을 결정적 비중첩 합집합으로 정규화한다.
    const TArray<FFFZSCellRect> Canonical = UFFZSGridLibrary::CanonicalizeCellRects({
        FFFZSCellRect(FIntPoint(0, 0), FIntPoint(2, 2)),
        FFFZSCellRect(FIntPoint(2, 0), FIntPoint(4, 2)),
        FFFZSCellRect(FIntPoint(1, 1), FIntPoint(3, 3))
    });
    TestEqual(TEXT("맞닿거나 겹치는 Rect들이 결정적인 두 밴드로 합쳐진다"),
        Canonical.Num(), 2);
    TestEqual(TEXT("정규 합집합 면적에서 겹침이 정확히 제거된다"),
        UFFZSGridLibrary::GetCellRectArea(Canonical), static_cast<int64>(10));
    TestFalse(TEXT("정규 출력 Rect들은 서로 겹치지 않는다"),
        Canonical.Num() == 2 && Canonical[0].Intersects(Canonical[1]));

    TArray<FIntPoint> Materialized;
    TestTrue(TEXT("Cell 10개의 정규 합집합은 정확한 제한에서 구체화된다"),
        UFFZSGridLibrary::TryMaterializeCellRects(Canonical, 10, Materialized));
    TestEqual(TEXT("정규 합집합이 고유 Cell 10개로 구체화된다"),
        Materialized.Num(), 10);

    // 10,000 Cell 경계는 성공하고 단 한 Cell이라도 초과하면 출력까지 비우는 fail-closed 계약이다.
    const TArray<FFFZSCellRect> AtDebugLimit = {
        FFFZSCellRect(FIntPoint(0, 0), FIntPoint(100, 100))
    };
    TestTrue(TEXT("디버그 Cell은 정확히 10,000개까지 허용된다"),
        UFFZSGridLibrary::TryMaterializeCellRects(
            AtDebugLimit,
            UFFZSDeveloperSettings::MaxMaterializedDebugCells,
            Materialized));
    TestEqual(TEXT("정확한 디버그 제한에서 Cell 10,000개가 생성된다"),
        Materialized.Num(), 10'000);

    const TArray<FFFZSCellRect> OverDebugLimit = {
        FFFZSCellRect(FIntPoint(0, 0), FIntPoint(10'001, 1))
    };
    TestFalse(TEXT("디버그 Cell 10,001개는 안전 차단으로 실패한다"),
        UFFZSGridLibrary::TryMaterializeCellRects(
            OverDebugLimit,
            UFFZSDeveloperSettings::MaxMaterializedDebugCells,
            Materialized));
    TestTrue(TEXT("구체화 실패 시 기존 출력 배열이 비워진다"),
        Materialized.IsEmpty());

    const TArray<FFFZSCellRect> MaximumCoordinateRect = {
        FFFZSCellRect(FIntPoint(MAX_int32 - 1, 0), FIntPoint(MAX_int32, 1))
    };
    TestTrue(TEXT("Materialization handles a valid rect adjacent to MAX_int32"),
        UFFZSGridLibrary::TryMaterializeCellRects(
            MaximumCoordinateRect, 1, Materialized));
    TestTrue(TEXT("The materialized boundary cell is exact and does not wrap"),
        Materialized.Num() == 1 &&
        Materialized[0] == FIntPoint(MAX_int32 - 1, 0));

    Materialized.Add(FIntPoint(7, 7));
    const TArray<FFFZSCellRect> InvalidRects = {
        FFFZSCellRect(FIntPoint(2, 2), FIntPoint(2, 3))
    };
    TestFalse(TEXT("잘못된 반개방 Rect는 빈 성공으로 처리되지 않고 구체화에 실패한다"),
        UFFZSGridLibrary::TryMaterializeCellRects(InvalidRects, 10, Materialized));
    TestTrue(TEXT("잘못된 Rect 구체화도 기존 출력을 비운다"),
        Materialized.IsEmpty());

    FFFZSFacilitySpec LargeDebugSpec = MakeSmallRegionSpec();
    LargeDebugSpec.FootprintSizeCells = FIntPoint(101, 100);
    LargeDebugSpec.ClearanceCells = 0;
    LargeDebugSpec.QueueReservedRects.Reset();
    TestTrue(TEXT("압축 권위 데이터는 Cell 10,100개의 합성 점유 영역을 허용한다"),
        UFFZSGridLibrary::CanEnumerateFacilityCells(LargeDebugSpec));
    TestEqual(TEXT("압축 권위 데이터가 Cell 10,100개의 면적을 보존한다"),
        UFFZSGridLibrary::GetCellRectArea(
            UFFZSGridLibrary::GetFootprintRects(
                LargeDebugSpec, FFFZSGridPlacement())),
        static_cast<int64>(10'100));
    TestTrue(TEXT("레거시 점유 Cell 뷰는 10,000개를 초과하면 안전 차단으로 실패한다"),
        UFFZSGridLibrary::GetFootprintCells(
            LargeDebugSpec, FFFZSGridPlacement()).IsEmpty());
    TestTrue(TEXT("레거시 예약 Cell 뷰도 10,000개를 초과하면 안전 차단으로 실패한다"),
        UFFZSGridLibrary::GetReservedCells(
            LargeDebugSpec, FFFZSGridPlacement()).IsEmpty());

    // 본체와 expanded는 int32 범위 안이지만 Queue Max만 넘는 표본이다. 어떤 하위 영역도
    // 부분 권위 결과로 반환하지 않고 clearance/reserved 전체가 fail-closed해야 한다.
    FFFZSGridPlacement QueueOverflowPlacement;
    QueueOverflowPlacement.OriginCell = FIntPoint(MAX_int32 - 4, 0);
    TestFalse(TEXT("대기열 전용 오버플로 픽스처에서도 점유 영역은 표현 가능하다"),
        UFFZSGridLibrary::GetFootprintRects(Spec, QueueOverflowPlacement).IsEmpty());
    TestTrue(TEXT("대기열 전용 좌표 오버플로로 Queue Region 생성이 실패한다"),
        UFFZSGridLibrary::GetQueueRects(Spec, QueueOverflowPlacement).IsEmpty());
    TestTrue(TEXT("대기열 전용 오버플로는 불완전한 여유 Region을 노출하지 않는다"),
        UFFZSGridLibrary::GetClearanceRects(Spec, QueueOverflowPlacement).IsEmpty());
    TestTrue(TEXT("대기열 전용 오버플로는 불완전한 예약 합집합을 노출하지 않는다"),
        UFFZSGridLibrary::GetReservedRects(Spec, QueueOverflowPlacement).IsEmpty());

    // The budget covers footprint 6 + queue 2, without legacy clearance.
    {
        TGuardValue<int32> BelowBudgetGuard(
            MutableSettings->MaxEnumeratedCellsPerFacility, 7);
        TestFalse(TEXT("The region budget rejects one cell below the required eight"),
            UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    }
    {
        TGuardValue<int32> ExactBudgetGuard(
            MutableSettings->MaxEnumeratedCellsPerFacility, 8);
        TestTrue(TEXT("보수적 Region 작업 예산은 정확한 경곗값을 허용한다"),
            UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSPassiveFootprintRegionsTest,
    "FF_ZoneSystem.Unit.Grid.PassiveInfrastructureReservesOnlyFootprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSPassiveFootprintRegionsTest::RunTest(const FString& Parameters)
{
    TGuardValue<int32> Budget(GetMutableDefault<UFFZSDeveloperSettings>()->MaxEnumeratedCellsPerFacility, 6);
    for (const EFFZSFacilityCategory Category : {
        EFFZSFacilityCategory::Road, EFFZSFacilityCategory::Tile, EFFZSFacilityCategory::Lighting })
    {
        FFFZSFacilitySpec Legacy = MakeSmallRegionSpec();
        Legacy.Category = Category;
        Legacy.ClearanceCells = 100;
        Legacy.QueueReservedCells.Add(FIntPoint(100, 100));
        TestTrue(TEXT("Only the 3x2 footprint consumes the region budget"),
            UFFZSGridLibrary::CanEnumerateFacilityCells(Legacy));
        const FFFZSGridPlacement Identity;
        TestTrue(TEXT("Direct queue query ignores obsolete reservations"),
            UFFZSGridLibrary::GetQueueRects(Legacy, Identity).IsEmpty());
        TestEqual(TEXT("Reserved area equals the exact footprint"),
            UFFZSGridLibrary::GetCellRectArea(UFFZSGridLibrary::GetReservedRects(Legacy, Identity)), int64(6));
        for (const int32 Yaw : { 0, 3700, 9000, 22500 })
        {
            FFFZSGridPlacement Placement;
            Placement.OriginCell = FIntPoint(123, 456);
            Placement.YawCentidegrees = Yaw;
            TArray<FFFZSOrientedRect> Footprint, Clearance, Queue, Reserved;
            UFFZSGridLibrary::BuildPlacementOrientedRegions(Legacy, Placement, Footprint, Clearance, Queue, Reserved);
            TestEqual(TEXT("A rotated passive footprint remains one rectangle"), Footprint.Num(), 1);
            TestTrue(TEXT("Rotation never adds clearance or a queue"), Clearance.IsEmpty() && Queue.IsEmpty());
            TestTrue(TEXT("Reserved geometry exactly matches the rotated footprint"),
                AreOrientedRectArraysEquivalent(Footprint, Reserved));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSNativeStageAreaBudgetTest,
    "FF_ZoneSystem.Unit.Grid.NativeStageFitsCompressedAreaBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSNativeStageAreaBudgetTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UFFZSDeveloperSettings* MutableSettings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<int32> BudgetGuard(MutableSettings->MaxEnumeratedCellsPerFacility,
        UFFZSDeveloperSettings::DefaultMaxEnumeratedCellsPerFacility);

    // Actual main-stage mesh bounds with an ignored legacy 1m clearance.
    // The area remains compressed even though it exceeds the previous 1,000m2 allowance.
    FFFZSFacilitySpec Spec;
    Spec.FootprintSizeCells = FIntPoint(5071, 3149);
    Spec.ClearanceCells = 100;
    const FFFZSGridPlacement Placement;
    TestTrue(TEXT("Native main-stage dimensions fit the production area budget"),
        UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    const TArray<FFFZSCellRect> Footprint = UFFZSGridLibrary::GetFootprintRects(Spec, Placement);
    TestEqual(TEXT("A native-size stage stays one authoritative footprint rectangle"), Footprint.Num(), 1);
    TestEqual(TEXT("The stage rectangle preserves its full native footprint area"),
        UFFZSGridLibrary::GetCellRectArea(Footprint), static_cast<int64>(15'968'579));
    TestEqual(TEXT("The stage reservation does not include legacy clearance"),
        UFFZSGridLibrary::GetCellRectArea(UFFZSGridLibrary::GetReservedRects(Spec, Placement)),
        static_cast<int64>(15'968'579));
    TestTrue(TEXT("Native stage size does not enable massive debug cell materialization"),
        UFFZSGridLibrary::GetReservedCells(Spec, Placement).IsEmpty());

    // A separate audience allowance must remain independent of the stage body/queue limit.
    Spec.Audience.bEnabled = true;
    Spec.Audience.Columns = 22;
    Spec.Audience.Rows = 22;
    Spec.Audience.FirstSlotLocalCm = FVector(0, 2500, 0);
    if (!TestTrue(TEXT("The audience layout can arrange its seats and portals"),
            Spec.Audience.TryCenterAndArrangePortals()))
    {
        return false;
    }
    TestFalse(TEXT("The large stage and audience exceed the shared area budget"),
        UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    Spec.Audience.AreaLimitM2 = 909.f;
    TestTrue(TEXT("A 909-square-meter audience allowance permits the native-size stage audience"),
        UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    TestTrue(TEXT("The separate audience allowance still leaves large debug cell arrays empty"),
        UFFZSGridLibrary::GetReservedCells(Spec, Placement).IsEmpty());

    Spec.Audience.bEnabled = false;
    Spec.ClearanceCells = 0;
    Spec.FootprintSizeCells = FIntPoint(5000, 4000);
    TestTrue(TEXT("Exactly 20,000,000 square centimeters are permitted"),
        UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    Spec.FootprintSizeCells.X += 1;
    TestFalse(TEXT("A footprint beyond 20,000,000 square centimeters is rejected"),
        UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    TestTrue(TEXT("An oversized footprint cannot expose partial authority rectangles"),
        UFFZSGridLibrary::GetFootprintRects(Spec, Placement).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSArbitraryYawFootprintGeometryTest,
    "FF_ZoneSystem.Unit.Grid.OrientedPlacement.FortyFiveDegreeFootprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSArbitraryYawFootprintGeometryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UFFZSDeveloperSettings* MutableSettings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<int32> StableRegionBudget(
        MutableSettings->MaxEnumeratedCellsPerFacility, 1'000'000);

    FFFZSFacilitySpec Spec = MakeSmallRegionSpec();
    Spec.FootprintSizeCells = FIntPoint(600, 300);
    Spec.ClearanceCells = 0;
    Spec.QueueReservedRects.Reset();

    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(100, -50);
    Placement.YawCentidegrees = 4500;

    TArray<FFFZSOrientedRect> Footprint;
    TArray<FFFZSOrientedRect> Clearance;
    TArray<FFFZSOrientedRect> Queue;
    TArray<FFFZSOrientedRect> Reserved;
    UFFZSGridLibrary::BuildPlacementOrientedRegions(
        Spec, Placement, Footprint, Clearance, Queue, Reserved);

    TestEqual(TEXT("600x300 footprint는 단일 oriented rect로 유지된다"), Footprint.Num(), 1);
    if (Footprint.Num() != 1)
    {
        return false;
    }

    const double InvSqrtTwo = FMath::Sqrt(0.5);
    const FVector2D ExpectedCenter(
        100.5 + 150.0 * InvSqrtTwo,
        -49.5 + 449.0 * InvSqrtTwo);
    TestTrue(TEXT("45도 footprint 중심은 cell(0,0) 중심 회전식을 따른다"),
        Footprint[0].CenterCell.Equals(ExpectedCenter, 1.0e-6));
    TestTrue(TEXT("회전해도 footprint 반크기는 300x150 Cell로 보존된다"),
        Footprint[0].HalfExtentCells.Equals(FVector2D(300.0, 150.0), 1.0e-9));
    TestTrue(TEXT("centidegree 4500은 oriented yaw 45도로 보존된다"),
        FMath::IsNearlyEqual(Footprint[0].YawDegrees, 45.0f, 1.0e-6f));

    const FFFZSCellRect ConservativeAabb =
        FFZSOrientedGeometry::ToConservativeCellRect(Footprint[0]);
    TestTrue(TEXT("600x300 45도 footprint의 보수적 Cell AABB가 정확하다"),
        ConservativeAabb ==
            FFFZSCellRect(FIntPoint(-112, -51), FIntPoint(525, 587)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSOrientedBroadPhaseFalsePositiveTest,
    "FF_ZoneSystem.Unit.Grid.OrientedPlacement.SatRejectsConservativeAabbFalsePositive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOrientedBroadPhaseFalsePositiveTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UFFZSDeveloperSettings* MutableSettings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<int32> StableRegionBudget(
        MutableSettings->MaxEnumeratedCellsPerFacility, 1'000'000);

    FFFZSFacilitySpec Spec = MakeSmallRegionSpec();
    Spec.FootprintSizeCells = FIntPoint(600, 300);
    Spec.ClearanceCells = 0;
    Spec.QueueReservedRects.Reset();

    FFFZSGridPlacement PlacementA;
    PlacementA.YawCentidegrees = 4500;
    FFFZSGridPlacement PlacementB;
    PlacementB.OriginCell = FIntPoint(-213, 213);
    PlacementB.YawCentidegrees = 4500;

    TArray<FFFZSOrientedRect> FootprintA;
    TArray<FFFZSOrientedRect> ClearanceA;
    TArray<FFFZSOrientedRect> QueueA;
    TArray<FFFZSOrientedRect> ReservedA;
    UFFZSGridLibrary::BuildPlacementOrientedRegions(
        Spec, PlacementA, FootprintA, ClearanceA, QueueA, ReservedA);

    TArray<FFFZSOrientedRect> FootprintB;
    TArray<FFFZSOrientedRect> ClearanceB;
    TArray<FFFZSOrientedRect> QueueB;
    TArray<FFFZSOrientedRect> ReservedB;
    UFFZSGridLibrary::BuildPlacementOrientedRegions(
        Spec, PlacementB, FootprintB, ClearanceB, QueueB, ReservedB);

    TestTrue(TEXT("두 45도 시설의 oriented footprint가 모두 생성된다"),
        FootprintA.Num() == 1 && FootprintB.Num() == 1);
    if (FootprintA.Num() != 1 || FootprintB.Num() != 1)
    {
        return false;
    }

    const TArray<FFFZSCellRect> BoundsA = {
        FFZSOrientedGeometry::ToConservativeCellRect(FootprintA[0])
    };
    const TArray<FFFZSCellRect> BoundsB = {
        FFZSOrientedGeometry::ToConservativeCellRect(FootprintB[0])
    };
    TestTrue(TEXT("보수적 AABB만 사용하면 두 45도 시설을 겹침으로 오인한다"),
        UFFZSGridLibrary::DoCellRectSetsOverlap(BoundsA, BoundsB));
    TestFalse(TEXT("SAT OBB 좁은 판정은 AABB 오탐을 제거한다"),
        UFFZSGridLibrary::DoOrientedRectSetsOverlap(FootprintA, FootprintB));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSOrientedRegionOverflowTest,
    "FF_ZoneSystem.Unit.Grid.OrientedPlacement.QueueOverflowFailsClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOrientedRegionOverflowTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FFFZSFacilitySpec Spec = MakeSmallRegionSpec();
    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(MAX_int32 - 4, 0);

    const FFFZSOrientedRect Sentinel(
        FVector2D::ZeroVector, FVector2D(1.0, 1.0), 0.0f);
    TArray<FFFZSOrientedRect> Footprint = { Sentinel };
    TArray<FFFZSOrientedRect> Clearance = { Sentinel };
    TArray<FFFZSOrientedRect> Queue = { Sentinel };
    TArray<FFFZSOrientedRect> Reserved = { Sentinel };
    UFFZSGridLibrary::BuildPlacementOrientedRegions(
        Spec, Placement, Footprint, Clearance, Queue, Reserved);

    TestTrue(TEXT("Queue Max가 int32를 넘으면 footprint 출력도 비운다"), Footprint.IsEmpty());
    TestTrue(TEXT("Queue Max가 int32를 넘으면 clearance 출력도 비운다"), Clearance.IsEmpty());
    TestTrue(TEXT("Queue Max가 int32를 넘으면 queue 출력도 비운다"), Queue.IsEmpty());
    TestTrue(TEXT("Queue Max가 int32를 넘으면 reserved 출력도 비운다"), Reserved.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSCardinalYawCompatibilityTest,
    "FF_ZoneSystem.Unit.Grid.OrientedPlacement.LegacyQuarterTurnMatchesModernYaw",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSCardinalYawCompatibilityTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FFFZSFacilitySpec Spec = MakeSmallRegionSpec();
    FFFZSGridPlacement LegacyPlacement;
    LegacyPlacement.OriginCell = FIntPoint(10, 20);
    LegacyPlacement.QuarterTurns = 1;

    FFFZSGridPlacement ModernPlacement;
    ModernPlacement.OriginCell = LegacyPlacement.OriginCell;
    ModernPlacement.YawCentidegrees = 9000;

    TArray<FFFZSOrientedRect> LegacyFootprint;
    TArray<FFFZSOrientedRect> LegacyClearance;
    TArray<FFFZSOrientedRect> LegacyQueue;
    TArray<FFFZSOrientedRect> LegacyReserved;
    UFFZSGridLibrary::BuildPlacementOrientedRegions(
        Spec,
        LegacyPlacement,
        LegacyFootprint,
        LegacyClearance,
        LegacyQueue,
        LegacyReserved);

    TArray<FFFZSOrientedRect> ModernFootprint;
    TArray<FFFZSOrientedRect> ModernClearance;
    TArray<FFFZSOrientedRect> ModernQueue;
    TArray<FFFZSOrientedRect> ModernReserved;
    UFFZSGridLibrary::BuildPlacementOrientedRegions(
        Spec,
        ModernPlacement,
        ModernFootprint,
        ModernClearance,
        ModernQueue,
        ModernReserved);

    TestEqual(TEXT("legacy q1은 modern yaw9000과 같은 유효 yaw를 갖는다"),
        UFFZSGridLibrary::GetEffectiveYawCentidegrees(LegacyPlacement),
        UFFZSGridLibrary::GetEffectiveYawCentidegrees(ModernPlacement));
    TestTrue(TEXT("legacy q1과 yaw9000의 footprint oriented 결과가 같다"),
        AreOrientedRectArraysEquivalent(LegacyFootprint, ModernFootprint));
    TestTrue(TEXT("legacy q1과 yaw9000의 clearance oriented 결과가 같다"),
        AreOrientedRectArraysEquivalent(LegacyClearance, ModernClearance));
    TestTrue(TEXT("legacy q1과 yaw9000의 queue oriented 결과가 같다"),
        AreOrientedRectArraysEquivalent(LegacyQueue, ModernQueue));
    TestTrue(TEXT("legacy q1과 yaw9000의 reserved oriented 결과가 같다"),
        AreOrientedRectArraysEquivalent(LegacyReserved, ModernReserved));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSCompressedRegionCommandHashTest,
    "FF_ZoneSystem.Unit.Commands.OneCentimeterCompressedRegionPayloadIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSCompressedRegionCommandHashTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FFFZSGridPlacement Placement;
    // Arrange: 테스트 fixture는 값 복사와 Fingerprint에만 사용하고 개별 Cell로 펼치지 않는다.
    const FFFZSCommand BaselineCommand = UFFZSGridLibrary::MakeInstallCommand(
        FFZSTest::MakeValidFacilitySpec(
            EFFZSTestFacilityVariant::Toilet),
        TEXT("GZ_DadaPocha"),
        Placement,
        0);
    const FString BaselineFingerprint =
        UFFZSGridLibrary::BuildCommandFingerprint(BaselineCommand);
    TestEqual(TEXT("동일한 압축 페이로드는 같은 지문을 생성한다"),
        BaselineFingerprint,
        UFFZSGridLibrary::BuildCommandFingerprint(BaselineCommand));

    // Act/Assert: 의미 필드 하나만 바꾼 복사본은 기준 지문과 달라야 한다.
    const auto ExpectMutationDetected =
        [this, &BaselineCommand, &BaselineFingerprint](
            const TCHAR* Label,
            TFunctionRef<void(FFFZSCommand&)> Mutate)
    {
        FFFZSCommand Mutated = BaselineCommand;
        Mutate(Mutated);
        TestNotEqual(
            Label,
            BaselineFingerprint,
            UFFZSGridLibrary::BuildCommandFingerprint(Mutated));
    };

    ExpectMutationDetected(TEXT("예상 리비전 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) { ++C.ExpectedWorldRevision; });
    ExpectMutationDetected(TEXT("트랜잭션 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) { C.TransactionId = FGuid::NewGuid(); });
    ExpectMutationDetected(TEXT("배치 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) { ++C.Placement.OriginCell.X; });
    ExpectMutationDetected(TEXT("스키마 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) { ++C.FacilitySpec.SchemaVersion; });
    ExpectMutationDetected(TEXT("1센티미터 점유 영역 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) { ++C.FacilitySpec.FootprintSizeCells.X; });
    ExpectMutationDetected(TEXT("1센티미터 여유 영역 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) { ++C.FacilitySpec.ClearanceCells; });
    ExpectMutationDetected(TEXT("카테고리 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.Category = EFFZSFacilityCategory::Info;
        });
    ExpectMutationDetected(TEXT("최대 대기 인원 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) {
            ++C.FacilitySpec.QueueRules.MaxWaitingAgents;
        });
    ExpectMutationDetected(TEXT("도착 허용 오차 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.QueueRules.SlotArrivalToleranceCm += 1.0f;
        });
    ExpectMutationDetected(TEXT("대기열 스플라인 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.QueueSplinePointsCm[0].X += 1.0;
        });
    ExpectMutationDetected(TEXT("압축 대기열 Rect 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) {
            ++C.FacilitySpec.QueueReservedRects[0].Min.X;
        });
    ExpectMutationDetected(TEXT("앵커 트랜스폼 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.Anchors[0].LocalTransform.AddToTranslation(
                FVector(1.0, 0.0, 0.0));
        });
    ExpectMutationDetected(TEXT("허용 Zone 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.AllowedZoneIds.Add(TEXT("GZ_Operations"));
        });
    ExpectMutationDetected(TEXT("외부 키 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.InstallCostKey = TEXT("Cost.Changed");
        });
    ExpectMutationDetected(TEXT("매우 작은 비영 시각 오프셋도 지문에 반영된다"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.VisualOffsetCm.X = UE_DOUBLE_KINDA_SMALL_NUMBER * 0.5;
        });
    ExpectMutationDetected(TEXT("매우 작은 비영 시각 회전도 지문에 반영된다"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.VisualRotation.Yaw = UE_KINDA_SMALL_NUMBER * 0.5f;
        });
    ExpectMutationDetected(TEXT("시설 사운드 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.AudioSettings.AmbientSound =
                TSoftObjectPtr<USoundBase>(FSoftObjectPath(
                    TEXT("/Game/Audio/Facility/Ambient_Test.Ambient_Test")));
        });
    ExpectMutationDetected(TEXT("시설 최대 가청 범위 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.AudioSettings.MaxAudibleDistanceCm += 100.0f;
        });
    ExpectMutationDetected(TEXT("장애물 높이 변경이 지문에 반영된다"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.PlacementObstacleHeightCm += 1.0f;
        });

    FFFZSCommand Same = BaselineCommand;
    TestEqual(TEXT("CRC 헬퍼의 결과가 안정적으로 유지된다"),
        UFFZSGridLibrary::HashCommandPayload(BaselineCommand),
        UFFZSGridLibrary::HashCommandPayload(Same));

    // 표시 문화권은 Payload 의미가 아니므로 같은 FText 값의 지문이 문화권에 의존하면 안 된다.
    const FString OriginalCulture =
        FInternationalization::Get().GetCurrentCulture()->GetName();
    FFFZSCommand CultureCommand = BaselineCommand;
    if (FInternationalization::Get().SetCurrentCulture(TEXT("en-US")))
    {
        CultureCommand.FacilitySpec.DisplayName = FText::AsNumber(1234567.5);
        const FString EnglishFingerprint =
            UFFZSGridLibrary::BuildCommandFingerprint(CultureCommand);
        if (FInternationalization::Get().SetCurrentCulture(TEXT("fr-FR")))
        {
            TestEqual(TEXT("FText 페이로드 식별값은 문화권에 영향을 받지 않는다"),
                EnglishFingerprint,
                UFFZSGridLibrary::BuildCommandFingerprint(CultureCommand));
        }
    }
    FInternationalization::Get().SetCurrentCulture(OriginalCulture);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSAuthoredUpgradeSpecTest,
    "FF_ZoneSystem.Unit.Upgrade.AuthoredSpecGeometryAndFingerprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSAuthoredUpgradeSpecTest::RunTest(const FString& Parameters)
{
    FFFZSFacilitySpec Base = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    Base.VisualMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Test/SM_Level1.SM_Level1")));
    const FFFZSFacilitySpec Legacy = Base;
    FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(Base, TEXT("GZ_DadaPocha"), FFFZSGridPlacement(), -1);
    const FString LegacyFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Command);
    TestFalse(TEXT("Default specs do not add an upgrade fingerprint block"), LegacyFingerprint.Contains(TEXT("spec.uses_facility_levels")));
    Command.FacilitySpec.bUsesFacilityLevels = true;
    TestNotEqual(TEXT("Level-one-only is distinct from no levels"),
        UFFZSGridLibrary::BuildCommandFingerprint(Command), LegacyFingerprint);

    Base.bUsesFacilityLevels = true;
    FFFZSFacilityUpgradeTier Level2;
    Level2.SizeMultiplier = 2.0f;
    Level2.VisualMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Test/SM_Level2.SM_Level2")));
    FFFZSFacilityUpgradeTier Level3;
    Level3.TargetLevel = 3;
    Level3.CostMultiplier = 0.7f;
    Level3.ServiceDurationMultiplier = 0.6f;
    Level3.SizeMultiplier = 2.5f;
    Base.UpgradeTiers = { Level2, Level3 };
    FText Error;
    TestTrue(TEXT("Independent authored tiers validate"), FFZSFacilityUpgradeRules::ValidateTiers(Base.UpgradeTiers, Error));
    TestTrue(TEXT("Lookup finds exact authored level"), FFZSFacilityUpgradeRules::FindTier(Base.UpgradeTiers, 3) == &Base.UpgradeTiers[1]);
    TestNull(TEXT("Unknown level has no tier"), FFZSFacilityUpgradeRules::FindTier(Base.UpgradeTiers, 4));
    TArray<FText> Errors;
    TestTrue(TEXT("Authored spec is valid"), Base.IsValid(Errors));
    UFFZSFacilityDefinition* Definition = NewObject<UFFZSFacilityDefinition>();
    Definition->Spec = Base;
    TestTrue(TEXT("Definition validates every authored physical level"), Definition->IsDefinitionValid(Errors));
    Errors = { FText::FromString(TEXT("Existing caller error")) };
    TestTrue(TEXT("Definition validity is independent of prior caller errors"), Definition->IsDefinitionValid(Errors));
    TestEqual(TEXT("Definition preserves prior caller errors"), Errors.Num(), 1);
    {
        TGuardValue<int32> BudgetGuard(GetMutableDefault<UFFZSDeveloperSettings>()->MaxEnumeratedCellsPerFacility, 1'000'000);
        TestTrue(TEXT("Original geometry remains within its budget"), Definition->Spec.IsValid(Errors));
        TestFalse(TEXT("Definition authoring catches an oversized derived building"), Definition->IsDefinitionValid(Errors));
        TestTrue(TEXT("Derived definition errors identify the authored level"), !Errors.IsEmpty() && Errors[0].ToString().StartsWith(TEXT("Lv.2:")));
    }
    const FFFZSFacilitySpec Identity = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Legacy, 1, 1.0f);
    FFFZSFacilitySpec NormalizedLegacy = Legacy;
    NormalizedLegacy.ClearanceCells = 0;
    TestTrue(TEXT("Identity scale preserves authored geometry and removes legacy clearance"),
        FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(&NormalizedLegacy, &Identity, 0));
    const FFFZSFacilitySpec DecimalGrowth = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Base, 2, 1.15f);
    TestTrue(TEXT("Decimal growth keeps a touching queue outside the building"), DecimalGrowth.IsValid(Errors));
    TestEqual(TEXT("Decimal float noise does not steal a row at the shared edge"),
        DecimalGrowth.QueueReservedRects[0].Min.Y, DecimalGrowth.FootprintSizeCells.Y);
    TestEqual(TEXT("Positive float noise does not add a footprint cell"),
        FFZSFacilityUpgradeRules::BuildEffectiveSpec(Base, 2, 1.1f).FootprintSizeCells, FIntPoint(660, 330));

    FFFZSFacilitySpec OddBase = Base;
    OddBase.FootprintSizeCells = FIntPoint(601, 301);
    for (FFFZSCellRect& Rect : OddBase.QueueReservedRects)
    {
        ++Rect.Min.Y;
        ++Rect.Max.Y;
    }
    // Preserve the original route's placement in its reservation after changing the centre.
    const FVector OddOriginOffset(-0.5, 0.5, 0.0);
    for (FVector& Point : OddBase.QueueSplinePointsCm)
    {
        Point += OddOriginOffset;
    }
    for (FFFZSAnchorDefinition& Anchor : OddBase.Anchors)
    {
        Anchor.LocalTransform.AddToTranslation(OddOriginOffset);
    }
    OddBase.QueueReservedRects.Add(FFFZSCellRect(FIntPoint(601, 0), FIntPoint(701, 301)));
    TestTrue(TEXT("Odd-size fixture is valid before upgrading"), OddBase.IsValid(Errors));
    const FFFZSFacilitySpec OddGrowth = FFZSFacilityUpgradeRules::BuildEffectiveSpec(OddBase, 2, 1.15f);
    TestTrue(TEXT("Odd-size growth keeps both touching external sides valid"), OddGrowth.IsValid(Errors));
    TestEqual(TEXT("Genuinely fractional footprint dimensions still round up"), OddGrowth.FootprintSizeCells, FIntPoint(692, 347));
    TestEqual(TEXT("Top reservation shares the rounded footprint edge"), OddGrowth.QueueReservedRects[0].Min.Y, 347);
    TestEqual(TEXT("Right reservation shares the rounded footprint edge"), OddGrowth.QueueReservedRects.Last().Min.X, 692);
    FFFZSFacilitySpec InvalidOverlap = Base;
    --InvalidOverlap.QueueReservedRects[0].Min.Y;
    TestFalse(TEXT("Actual authored overlap is still rejected after scaling"),
        FFZSFacilityUpgradeRules::BuildEffectiveSpec(InvalidOverlap, 2, 1.15f).IsValid(Errors));
    const FFFZSFacilitySpec Grown = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Base, 2, 2.0f);
    TestTrue(TEXT("Expanded authored geometry validates"), Grown.IsValid(Errors));
    TestEqual(TEXT("Actual footprint doubles"), Grown.FootprintSizeCells, FIntPoint(1200, 600));
    TestEqual(TEXT("Actual obstacle height doubles"), Grown.PlacementObstacleHeightCm, Base.PlacementObstacleHeightCm * 2.0f);
    TestEqual(TEXT("Upgrades discard legacy safety clearance"), Grown.ClearanceCells, 0);
    TestEqual(TEXT("Queue slot spacing stays unchanged"), Grown.QueueRules.SlotSpacingCm, Base.QueueRules.SlotSpacingCm);
    TestEqual(TEXT("Queue capacity stays unchanged"), Grown.QueueRules.MaxWaitingAgents, Base.QueueRules.MaxWaitingAgents);
    TestEqual(TEXT("Original service duration is not compounded in effective spec"), Grown.QueueRules.ServiceDurationSeconds, Base.QueueRules.ServiceDurationSeconds);
    TestTrue(TEXT("Queue path scales with the actual building"), Grown.QueueSplinePointsCm[0].Equals(Base.QueueSplinePointsCm[0] * 2.0));
    TestTrue(TEXT("Service anchors scale with the actual building"),
        Grown.Anchors[1].LocalTransform.GetTranslation().Equals(Base.Anchors[1].LocalTransform.GetTranslation() * 2.0));
    TestEqual(TEXT("An authored level visual is selected"), Grown.VisualMesh.ToSoftObjectPath(), Level2.VisualMesh.ToSoftObjectPath());
    const FFFZSFacilitySpec Third = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Base, 3, 2.5f);
    TestEqual(TEXT("Missing level visual always returns to Lv1 instead of inheriting Lv2"), Third.VisualMesh.ToSoftObjectPath(), Base.VisualMesh.ToSoftObjectPath());
    TestEqual(TEXT("Absolute size does not compound from Lv2"), Third.FootprintSizeCells, FIntPoint(1500, 750));

    FFFZSFacilitySpec Disabled = Base;
    Disabled.bUsesFacilityLevels = false;
    TestTrue(TEXT("Disabled levels retain valid authored tier data"), Disabled.IsValid(Errors));
    TestEqual(TEXT("Disabled tier visual falls back to base"),
        FFZSFacilityUpgradeRules::BuildEffectiveSpec(Disabled, 2, 1.f).VisualMesh.ToSoftObjectPath(), Base.VisualMesh.ToSoftObjectPath());
    FFFZSFacilitySpec Invalid = Base;
    Invalid.UpgradeTiers[1].ServiceDurationMultiplier = 0.9f;
    TestTrue(TEXT("Spec allows a later tier with slower service"), Invalid.IsValid(Errors));
    Invalid = Base;
    Invalid.UpgradeTiers[0].SizeMultiplier = 4.1f;
    TestTrue(TEXT("Tier authoring has no arbitrary physical scale cap"), Invalid.IsValid(Errors));
    Invalid = Base;
    Invalid.UpgradeTiers[1].SizeMultiplier = 1.5f;
    TestTrue(TEXT("Later levels may shrink the building"), Invalid.IsValid(Errors));
    FFFZSFacilitySpec RevenueOnly = Base;
    RevenueOnly.UpgradeTiers[0].ServiceDurationMultiplier = 1.0f;
    RevenueOnly.UpgradeTiers[1].ServiceDurationMultiplier = 1.0f;
    RevenueOnly.UpgradeTiers[1].RevenueMultiplier = 2.0f;
    TestTrue(TEXT("Increasing revenue alone is a valid upgrade"), RevenueOnly.IsValid(Errors));
    RevenueOnly.UpgradeTiers[1].RevenueMultiplier = 1.5f;
    TestTrue(TEXT("Unchanged duration and revenue can represent an authored upgrade"), RevenueOnly.IsValid(Errors));
    RevenueOnly.UpgradeTiers[1].RevenueMultiplier = 1.25f;
    TestTrue(TEXT("Later levels may reduce revenue"), RevenueOnly.IsValid(Errors));
    RevenueOnly.UpgradeTiers[1].RevenueMultiplier = 100.1f;
    TestTrue(TEXT("Revenue multiplier has no arbitrary upper cap"), RevenueOnly.IsValid(Errors));
    FFFZSFacilitySpec Flexible = Base;
    Flexible.UpgradeTiers.Reset();
    for (int32 Index = 25; Index >= 1; --Index)
    {
        FFFZSFacilityUpgradeTier Tier;
        Tier.TargetLevel = Index * 10;
        Tier.CostMultiplier = Index == 1 ? 0.f : 100.f;
        Tier.ServiceDurationMultiplier = Index % 2 ? 3.f : 0.001f;
        Tier.RevenueMultiplier = Index % 2 ? 0.f : 1000.f;
        Tier.SizeMultiplier = Index % 2 ? 0.5f : 5.f;
        Flexible.UpgradeTiers.Add(Tier);
    }
    FFFZSFacilityUpgradeTier Last;
    Last.TargetLevel = MAX_int32;
    Flexible.UpgradeTiers.Add(Last);
    TestTrue(TEXT("Unsorted sparse tiers beyond ten have no artificial caps"), FFZSFacilityUpgradeRules::ValidateTiers(Flexible.UpgradeTiers, Error));
    TestTrue(TEXT("Fingerprint preflight accepts more than nine tiers"), UFFZSGridLibrary::HasSafeFacilitySpecCardinality(Flexible));
    TestEqual(TEXT("Maximum level is independent of array ordering"), FFZSFacilityUpgradeRules::GetMaxLevel(Flexible.UpgradeTiers), MAX_int32);
    const FFFZSFacilityUpgradeTier* Next = FFZSFacilityUpgradeRules::FindNextTier(Flexible.UpgradeTiers, 1);
    TestTrue(TEXT("Next level is the smallest larger authored number"), Next && Next->TargetLevel == 10);
    Next = FFZSFacilityUpgradeRules::FindNextTier(Flexible.UpgradeTiers, 10);
    TestTrue(TEXT("A missing level number is skipped"), Next && Next->TargetLevel == 20);
    TestNull(TEXT("Highest representable level does not overflow next-level lookup"), FFZSFacilityUpgradeRules::FindNextTier(Flexible.UpgradeTiers, MAX_int32));
    TestTrue(TEXT("Exact lookup finds high levels in an unordered array"), FFZSFacilityUpgradeRules::FindTier(Flexible.UpgradeTiers, 250) == &Flexible.UpgradeTiers[0]);
    Flexible.UpgradeTiers.Last().TargetLevel = 250;
    TestFalse(TEXT("Duplicate authored levels are rejected"), FFZSFacilityUpgradeRules::ValidateTiers(Flexible.UpgradeTiers, Error));
    TestTrue(TEXT("Duplicate error names the invalid field"), Error.ToString().Contains(TEXT("Target Level")));
    Flexible.UpgradeTiers.Last().TargetLevel = MAX_int32;
    Flexible.UpgradeTiers[0].CostMultiplier = -1.f;
    TestFalse(TEXT("Negative cost remains invalid"), FFZSFacilityUpgradeRules::ValidateTiers(Flexible.UpgradeTiers, Error));
    TestTrue(TEXT("Cost error names the invalid field"), Error.ToString().Contains(TEXT("Cost Multiplier")));
    Flexible.UpgradeTiers[0].CostMultiplier = 0.f;
    Flexible.UpgradeTiers[0].ServiceDurationMultiplier = 0.f;
    TestFalse(TEXT("Zero duration remains invalid"), FFZSFacilityUpgradeRules::ValidateTiers(Flexible.UpgradeTiers, Error));
    TestTrue(TEXT("Duration error names the invalid field"), Error.ToString().Contains(TEXT("Service Duration Multiplier")));
    Flexible.UpgradeTiers[0].ServiceDurationMultiplier = 1.f;
    Flexible.UpgradeTiers[0].RevenueMultiplier = -1.f;
    TestFalse(TEXT("Negative revenue remains invalid"), FFZSFacilityUpgradeRules::ValidateTiers(Flexible.UpgradeTiers, Error));
    TestTrue(TEXT("Revenue error names the invalid field"), Error.ToString().Contains(TEXT("Revenue Multiplier")));
    Flexible.UpgradeTiers[0].RevenueMultiplier = 0.f;
    Flexible.UpgradeTiers[0].SizeMultiplier = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Nonfinite physical size remains invalid"), FFZSFacilityUpgradeRules::ValidateTiers(Flexible.UpgradeTiers, Error));
    TestTrue(TEXT("Size error names the invalid field"), Error.ToString().Contains(TEXT("Size Multiplier")));
    Flexible.UpgradeTiers[0].SizeMultiplier = 0.f;
    TestFalse(TEXT("Zero physical size remains invalid"), FFZSFacilityUpgradeRules::ValidateTiers(Flexible.UpgradeTiers, Error));
    Flexible.UpgradeTiers[0].SizeMultiplier = 1.f;
    Flexible.UpgradeTiers[0].TargetLevel = 1;
    TestFalse(TEXT("The base level cannot also be an upgrade tier"), FFZSFacilityUpgradeRules::ValidateTiers(Flexible.UpgradeTiers, Error));

    Command.FacilitySpec = Base;
    const FString Fingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Command);
    const auto CheckMutation = [this, &Command, &Fingerprint](const TCHAR* Label, TFunctionRef<void(FFFZSFacilitySpec&)> Mutate)
    {
        FFFZSCommand Changed = Command;
        Mutate(Changed.FacilitySpec);
        TestNotEqual(Label, UFFZSGridLibrary::BuildCommandFingerprint(Changed), Fingerprint);
    };
    CheckMutation(TEXT("Use-levels flag participates in receipts"), [](FFFZSFacilitySpec& S) { S.bUsesFacilityLevels = false; });
    CheckMutation(TEXT("Authored level participates in receipts"), [](FFFZSFacilitySpec& S) { ++S.UpgradeTiers[0].TargetLevel; });
    CheckMutation(TEXT("Authored price participates in receipts"), [](FFFZSFacilitySpec& S) { S.UpgradeTiers[0].CostMultiplier = 0.4f; });
    CheckMutation(TEXT("Authored duration participates in receipts"), [](FFFZSFacilitySpec& S) { S.UpgradeTiers[0].ServiceDurationMultiplier = 0.75f; });
    CheckMutation(TEXT("Authored size participates in receipts"), [](FFFZSFacilitySpec& S) { S.UpgradeTiers[0].SizeMultiplier = 1.5f; });
    CheckMutation(TEXT("Authored revenue participates in receipts"), [](FFFZSFacilitySpec& S) { S.UpgradeTiers[0].RevenueMultiplier = 2.0f; });
    CheckMutation(TEXT("Authored mesh participates in receipts"), [](FFFZSFacilitySpec& S) { S.UpgradeTiers[0].VisualMesh.Reset(); });
    FFFZSCommand Upgrade;
    Upgrade.Type = EFFZSCommandType::Upgrade;
    const FString OldUpgradeFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Upgrade);
    TestFalse(TEXT("Default size preserves old upgrade receipts"), OldUpgradeFingerprint.Contains(TEXT("upgrade.size_multiplier")));
    TestFalse(TEXT("Default revenue preserves old upgrade receipts"), OldUpgradeFingerprint.Contains(TEXT("upgrade.revenue_multiplier")));
    Upgrade.UpgradeRevenueMultiplier = 1.5f;
    TestNotEqual(TEXT("Changed revenue invalidates duplicate command payload"),
        UFFZSGridLibrary::BuildCommandFingerprint(Upgrade), OldUpgradeFingerprint);
    Upgrade.UpgradeRevenueMultiplier = 1.0f;
    Upgrade.UpgradeSizeMultiplier = 1.5f;
    TestNotEqual(TEXT("Changed physical size invalidates duplicate command payload"),
        UFFZSGridLibrary::BuildCommandFingerprint(Upgrade), OldUpgradeFingerprint);

    FFFZSFacilitySpec LegacyCells = MakeSmallRegionSpec();
    LegacyCells.QueueReservedCells = { FIntPoint(-2, 0), FIntPoint(-1, 0) };
    const FFFZSFacilitySpec ScaledCells = FFZSFacilityUpgradeRules::BuildEffectiveSpec(LegacyCells, 2, 1.5f);
    TestTrue(TEXT("Legacy queue cells convert to exact compressed regions"), ScaledCells.QueueReservedCells.IsEmpty());
    TestTrue(TEXT("Scaled negative queue boundaries round outwards"),
        UFFZSGridLibrary::IsCellInRects(FIntPoint(-3, 0), ScaledCells.QueueReservedRects));
    LegacyCells.FootprintSizeCells.X = MAX_int32;
    const FFFZSFacilitySpec Overflow = FFZSFacilityUpgradeRules::BuildEffectiveSpec(LegacyCells, 2, 4.f);
    TestEqual(TEXT("Unsafe scaling fails closed instead of overflowing"), Overflow.FootprintSizeCells, FIntPoint::ZeroValue);
    return true;
}

#endif
