// FF_FFZSGridTests.cpp

// 1 cm Grid와 압축 Cell Rect의 회전·합집합·예산·디버그 펼치기 계약을 검증한다.
// 흐름: 대형 테스트 fixture를 개별 Cell로 펼치지 않고, 작은 합성 Spec을 Region API에 넣어
// 반열림 사각형의 좌표·면적·분리·결정적 합집합을 확인한다.
// 불변식: 1 Unreal Unit=1 cm, 1 배치 Cell=1 cm이며 권위 계산은 FFFZSCellRect 배열을 사용한다.
// 실패/성능: 개별 Cell 호환 보기는 최대 10,000개에서만 허용되고 초과 시 빈 결과로 fail-closed한다.
 

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace
{
    // Region 연산의 정확한 좌표와 면적을 사람이 계산할 수 있도록 만든 작은 합성 사양이다.
     // footprint=[0,3)x[0,2), clearance=1, queue=[3,5)x[0,1)이다.
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
    TestEqual(TEXT("여유 영역에서 점유 영역과 대기열의 겹침이 제외된다"),
        UFFZSGridLibrary::GetCellRectArea(ClearanceRects), static_cast<int64>(13));
    TestEqual(TEXT("예약 Region은 중복 계산 없는 정확한 합집합이다"),
        UFFZSGridLibrary::GetCellRectArea(ReservedRects), static_cast<int64>(21));

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
    TestEqual(TEXT("작은 디버그 여유 영역의 모든 Cell이 구체화된다"),
        UFFZSGridLibrary::GetClearanceCells(Spec, Placement).Num(), 13);
    TestEqual(TEXT("작은 디버그 대기열의 모든 Cell이 구체화된다"),
        UFFZSGridLibrary::GetQueueCells(Spec, Placement).Num(), 2);
    TestEqual(TEXT("작은 디버그 예약 영역이 정확한 합집합으로 구체화된다"),
        UFFZSGridLibrary::GetReservedCells(Spec, Placement).Num(), 21);

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

    // 보수적 예산은 expanded footprint 20 + queue 2를 합산해 겹침과 무관하게 22로 판단한다.
    {
        TGuardValue<int32> BelowBudgetGuard(
            MutableSettings->MaxEnumeratedCellsPerFacility, 21);
        TestFalse(TEXT("보수적 Region 작업 예산은 필요한 22 Cell보다 1 부족하면 거부한다"),
            UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    }
    {
        TGuardValue<int32> ExactBudgetGuard(
            MutableSettings->MaxEnumeratedCellsPerFacility, 22);
        TestTrue(TEXT("보수적 Region 작업 예산은 정확한 경곗값을 허용한다"),
            UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    }
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

#endif
