/**
 * @file FF_FFZSGridTests.cpp
 * @brief 1 cm Grid와 압축 Cell Rect의 회전·합집합·예산·디버그 펼치기 계약을 검증한다.
 *
 * 흐름: 대형 내장 프리셋을 개별 Cell로 펼치지 않고, 작은 합성 Spec을 Region API에 넣어
 *       반열림 사각형의 좌표·면적·분리·결정적 합집합을 확인한다.
 * 불변식: 1 Unreal Unit=1 cm, 1 배치 Cell=1 cm이며 권위 계산은 FFFZSCellRect 배열을 사용한다.
 * 실패/성능: 개별 Cell 호환 보기는 최대 10,000개에서만 허용되고 초과 시 빈 결과로 fail-closed한다.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"

namespace
{
    /**
     * Region 연산의 정확한 좌표와 면적을 사람이 계산할 수 있도록 만든 작은 합성 사양이다.
     *
     * footprint=[0,3)x[0,2), clearance=1, queue=[3,5)x[0,1)이다.
     * 대형 1 cm 내장 프리셋은 이 테스트에서 Get*Cells로 펼치지 않는다.
     */
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

/*
 * 테스트 매트릭스
 * - Units: 1 uu=1 cm, 1 Cell=1 cm, 디버그 펼치기 10,000 Cell 상한
 * - Rotation: FIntPoint와 [Min,Max) Cell Rect의 90도 회전
 * - CompressedRegions: 작은 합성 Spec의 footprint/clearance/queue/reserved 압축 영역
 * - RegionUnion/Budget: 겹침·맞닿음 합집합, 보수적 작업 예산, 10k 초과 fail-closed
 * - Commands: QueueReservedRects를 포함한 전체 의미 필드의 문화권 독립 Payload 지문
 * 각 케이스는 Arrange -> Act -> Assert 순서를 유지하며 대형 내장 프리셋을 개별 셀로 열거하지 않는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSOneCentimeterUnitContractTest,
    "FF_ZoneSystem.Unit.Units.OnePlacementCellEqualsOneCentimeter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOneCentimeterUnitContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    // Arrange/Act/Assert: 월드 없이 코드 상수만 읽어 물리 단위와 안전 상한을 고정한다.
    TestEqual(TEXT("One placement Cell is exactly one centimeter/Unreal Unit"),
        UFFZSDeveloperSettings::DefaultPlacementCellSizeCm, 1.0);
    TestEqual(TEXT("Debug compatibility arrays materialize at most ten thousand Cells"),
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, 10'000);
    TestEqual(TEXT("Ground sampling remains coarser than one-centimeter occupancy"),
        UFFZSDeveloperSettings::DefaultGroundSampleSpacingCm, 100.0f);
    TestEqual(TEXT("Queue spacing remains a centimeter value independent of Cell storage"),
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(
            EFFZSBuiltInFacilityPreset::FoodTruckBasic).QueueRules.SlotSpacingCm,
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
    TestTrue(TEXT("Single-Cell Rect q1 matches RotateCell index semantics"),
        UFFZSGridLibrary::RotateRect(UnitRect, 1) == UnitRect);
    FFFZSCellRect FourTurnRect = SourceRect;
    for (int32 Turn = 0; Turn < 4; ++Turn)
    {
        FourTurnRect = UFFZSGridLibrary::RotateRect(FourTurnRect, 1);
    }
    TestTrue(TEXT("Four successive Rect quarter-turns return exactly to the source"),
        FourTurnRect == SourceRect);

    // Act/Assert: Cell 회전은 정수 좌표, Rect 회전은 전체 [Min,Max) 영역을 보존해야 한다.
    TestEqual(TEXT("Cell at zero quarter turns"),
        UFFZSGridLibrary::RotateCell(SourceCell, 0), FIntPoint(2, 1));
    TestEqual(TEXT("Cell at one clockwise quarter turn"),
        UFFZSGridLibrary::RotateCell(SourceCell, 1), FIntPoint(-1, 2));
    TestEqual(TEXT("Cell at two clockwise quarter turns"),
        UFFZSGridLibrary::RotateCell(SourceCell, 2), FIntPoint(-2, -1));
    TestEqual(TEXT("Cell at three clockwise quarter turns"),
        UFFZSGridLibrary::RotateCell(SourceCell, 3), FIntPoint(1, -2));
    TestEqual(TEXT("Negative Cell turns normalize to three"),
        UFFZSGridLibrary::RotateCell(SourceCell, -1), FIntPoint(1, -2));

    const FFFZSCellRect RotatedOnce = UFFZSGridLibrary::RotateRect(SourceRect, 1);
    const FFFZSCellRect RotatedTwice = UFFZSGridLibrary::RotateRect(SourceRect, 2);
    const FFFZSCellRect RotatedThrice = UFFZSGridLibrary::RotateRect(SourceRect, 3);
    TestTrue(TEXT("Rect one-turn bounds preserve every covered Cell"),
        RotatedOnce == FFFZSCellRect(FIntPoint(-5, 1), FIntPoint(-1, 4)));
    TestTrue(TEXT("Rect two-turn bounds preserve every covered Cell"),
        RotatedTwice == FFFZSCellRect(FIntPoint(-3, -5), FIntPoint(0, -1)));
    TestTrue(TEXT("Rect three-turn bounds preserve every covered Cell"),
        RotatedThrice == FFFZSCellRect(FIntPoint(2, -3), FIntPoint(6, 0)));
    TestEqual(TEXT("Rect area is invariant under rotation"),
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
    TestEqual(TEXT("Small synthetic footprint remains one compressed Rect"),
        FootprintRects.Num(), 1);
    TestTrue(TEXT("Rotated footprint bounds are exact"),
        FootprintRects.Num() == 1 &&
        FootprintRects[0] == FFFZSCellRect(FIntPoint(9, 20), FIntPoint(11, 23)));
    TestEqual(TEXT("Small synthetic queue remains one compressed Rect"),
        QueueRects.Num(), 1);
    TestTrue(TEXT("Rotated queue bounds are exact"),
        QueueRects.Num() == 1 &&
        QueueRects[0] == FFFZSCellRect(FIntPoint(10, 23), FIntPoint(11, 25)));

    TestEqual(TEXT("Footprint Region covers six one-centimeter Cells"),
        UFFZSGridLibrary::GetCellRectArea(FootprintRects), static_cast<int64>(6));
    TestEqual(TEXT("Queue Region covers two one-centimeter Cells"),
        UFFZSGridLibrary::GetCellRectArea(QueueRects), static_cast<int64>(2));
    TestEqual(TEXT("Clearance excludes footprint and the queue overlap"),
        UFFZSGridLibrary::GetCellRectArea(ClearanceRects), static_cast<int64>(13));
    TestEqual(TEXT("Reserved Region is the exact union without double counting"),
        UFFZSGridLibrary::GetCellRectArea(ReservedRects), static_cast<int64>(21));

    TestFalse(TEXT("Footprint and clearance Regions are disjoint"),
        UFFZSGridLibrary::DoCellRectSetsOverlap(FootprintRects, ClearanceRects));
    TestFalse(TEXT("Footprint and queue Regions only touch at a boundary"),
        UFFZSGridLibrary::DoCellRectSetsOverlap(FootprintRects, QueueRects));
    TestFalse(TEXT("Clearance subtracts the queue Region"),
        UFFZSGridLibrary::DoCellRectSetsOverlap(ClearanceRects, QueueRects));
    TestTrue(TEXT("Reserved Region contains a rotated footprint Cell"),
        UFFZSGridLibrary::IsCellInRects(FIntPoint(9, 20), ReservedRects));
    TestTrue(TEXT("Reserved Region contains the queue tail Cell"),
        UFFZSGridLibrary::IsCellInRects(FIntPoint(10, 24), ReservedRects));
    TestFalse(TEXT("Reserved Region excludes an unrelated Cell"),
        UFFZSGridLibrary::IsCellInRects(FIntPoint(11, 25), ReservedRects));

    // 작은 합성 입력에서만 구 호환 Get*Cells를 펼쳐 Region 면적과 같은지 확인한다.
    TestEqual(TEXT("Small debug footprint materializes all Cells"),
        UFFZSGridLibrary::GetFootprintCells(Spec, Placement).Num(), 6);
    TestEqual(TEXT("Small debug clearance materializes all Cells"),
        UFFZSGridLibrary::GetClearanceCells(Spec, Placement).Num(), 13);
    TestEqual(TEXT("Small debug queue materializes all Cells"),
        UFFZSGridLibrary::GetQueueCells(Spec, Placement).Num(), 2);
    TestEqual(TEXT("Small debug reservation materializes the exact union"),
        UFFZSGridLibrary::GetReservedCells(Spec, Placement).Num(), 21);

    // Arrange/Act: 겹치고 맞닿는 Rect 입력을 결정적 비중첩 합집합으로 정규화한다.
    const TArray<FFFZSCellRect> Canonical = UFFZSGridLibrary::CanonicalizeCellRects({
        FFFZSCellRect(FIntPoint(0, 0), FIntPoint(2, 2)),
        FFFZSCellRect(FIntPoint(2, 0), FIntPoint(4, 2)),
        FFFZSCellRect(FIntPoint(1, 1), FIntPoint(3, 3))
    });
    TestEqual(TEXT("Touching and overlapping Rects collapse to two deterministic bands"),
        Canonical.Num(), 2);
    TestEqual(TEXT("Canonical union area removes overlap exactly"),
        UFFZSGridLibrary::GetCellRectArea(Canonical), static_cast<int64>(10));
    TestFalse(TEXT("Canonical output Rects do not overlap"),
        Canonical.Num() == 2 && Canonical[0].Intersects(Canonical[1]));

    TArray<FIntPoint> Materialized;
    TestTrue(TEXT("A ten-Cell canonical union materializes at its exact limit"),
        UFFZSGridLibrary::TryMaterializeCellRects(Canonical, 10, Materialized));
    TestEqual(TEXT("Canonical union materializes ten unique Cells"),
        Materialized.Num(), 10);

    // 10,000 Cell 경계는 성공하고 단 한 Cell이라도 초과하면 출력까지 비우는 fail-closed 계약이다.
    const TArray<FFFZSCellRect> AtDebugLimit = {
        FFFZSCellRect(FIntPoint(0, 0), FIntPoint(100, 100))
    };
    TestTrue(TEXT("Exactly ten thousand debug Cells are allowed"),
        UFFZSGridLibrary::TryMaterializeCellRects(
            AtDebugLimit,
            UFFZSDeveloperSettings::MaxMaterializedDebugCells,
            Materialized));
    TestEqual(TEXT("Exact debug limit produces ten thousand Cells"),
        Materialized.Num(), 10'000);

    const TArray<FFFZSCellRect> OverDebugLimit = {
        FFFZSCellRect(FIntPoint(0, 0), FIntPoint(10'001, 1))
    };
    TestFalse(TEXT("Ten thousand and one debug Cells fail closed"),
        UFFZSGridLibrary::TryMaterializeCellRects(
            OverDebugLimit,
            UFFZSDeveloperSettings::MaxMaterializedDebugCells,
            Materialized));
    TestTrue(TEXT("Failed materialization clears a previous output array"),
        Materialized.IsEmpty());

    Materialized.Add(FIntPoint(7, 7));
    const TArray<FFFZSCellRect> InvalidRects = {
        FFFZSCellRect(FIntPoint(2, 2), FIntPoint(2, 3))
    };
    TestFalse(TEXT("An invalid half-open Rect fails materialization instead of becoming an empty success"),
        UFFZSGridLibrary::TryMaterializeCellRects(InvalidRects, 10, Materialized));
    TestTrue(TEXT("Invalid Rect materialization also clears previous output"),
        Materialized.IsEmpty());

    FFFZSFacilitySpec LargeDebugSpec = MakeSmallRegionSpec();
    LargeDebugSpec.FootprintSizeCells = FIntPoint(101, 100);
    LargeDebugSpec.ClearanceCells = 0;
    LargeDebugSpec.QueueReservedRects.Reset();
    TestTrue(TEXT("Compressed authority accepts a 10,100-Cell synthetic footprint"),
        UFFZSGridLibrary::CanEnumerateFacilityCells(LargeDebugSpec));
    TestEqual(TEXT("Compressed authority preserves the 10,100-Cell area"),
        UFFZSGridLibrary::GetCellRectArea(
            UFFZSGridLibrary::GetFootprintRects(
                LargeDebugSpec, FFFZSGridPlacement())),
        static_cast<int64>(10'100));
    TestTrue(TEXT("Legacy footprint Cell view fails closed above 10,000"),
        UFFZSGridLibrary::GetFootprintCells(
            LargeDebugSpec, FFFZSGridPlacement()).IsEmpty());
    TestTrue(TEXT("Legacy reserved Cell view also fails closed above 10,000"),
        UFFZSGridLibrary::GetReservedCells(
            LargeDebugSpec, FFFZSGridPlacement()).IsEmpty());

    // 본체와 expanded는 int32 범위 안이지만 Queue Max만 넘는 표본이다. 어떤 하위 영역도
    // 부분 권위 결과로 반환하지 않고 clearance/reserved 전체가 fail-closed해야 한다.
    FFFZSGridPlacement QueueOverflowPlacement;
    QueueOverflowPlacement.OriginCell = FIntPoint(MAX_int32 - 4, 0);
    TestFalse(TEXT("Queue-only overflow fixture keeps a representable footprint"),
        UFFZSGridLibrary::GetFootprintRects(Spec, QueueOverflowPlacement).IsEmpty());
    TestTrue(TEXT("Queue-only coordinate overflow fails the Queue Region"),
        UFFZSGridLibrary::GetQueueRects(Spec, QueueOverflowPlacement).IsEmpty());
    TestTrue(TEXT("Queue-only overflow cannot leak a partial clearance Region"),
        UFFZSGridLibrary::GetClearanceRects(Spec, QueueOverflowPlacement).IsEmpty());
    TestTrue(TEXT("Queue-only overflow cannot leak a partial reserved union"),
        UFFZSGridLibrary::GetReservedRects(Spec, QueueOverflowPlacement).IsEmpty());

    // 보수적 예산은 expanded footprint 20 + queue 2를 합산해 겹침과 무관하게 22로 판단한다.
    {
        TGuardValue<int32> BelowBudgetGuard(
            MutableSettings->MaxEnumeratedCellsPerFacility, 21);
        TestFalse(TEXT("Conservative Region work budget rejects one Cell below 22"),
            UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    }
    {
        TGuardValue<int32> ExactBudgetGuard(
            MutableSettings->MaxEnumeratedCellsPerFacility, 22);
        TestTrue(TEXT("Conservative Region work budget accepts its exact bound"),
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
    // Arrange: 내장 프리셋은 값 복사와 Fingerprint에만 사용하고 개별 Cell로 펼치지 않는다.
    const FFFZSCommand BaselineCommand = UFFZSGridLibrary::MakeInstallCommand(
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(
            EFFZSBuiltInFacilityPreset::ToiletBasic),
        TEXT("GZ_DadaPocha"),
        Placement,
        0);
    const FString BaselineFingerprint =
        UFFZSGridLibrary::BuildCommandFingerprint(BaselineCommand);
    TestEqual(TEXT("Same compressed payload fingerprints identically"),
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

    ExpectMutationDetected(TEXT("Expected revision participates"),
        [](FFFZSCommand& C) { ++C.ExpectedWorldRevision; });
    ExpectMutationDetected(TEXT("Transaction participates"),
        [](FFFZSCommand& C) { C.TransactionId = FGuid::NewGuid(); });
    ExpectMutationDetected(TEXT("Placement participates"),
        [](FFFZSCommand& C) { ++C.Placement.OriginCell.X; });
    ExpectMutationDetected(TEXT("Schema participates"),
        [](FFFZSCommand& C) { ++C.FacilitySpec.SchemaVersion; });
    ExpectMutationDetected(TEXT("One-centimeter footprint participates"),
        [](FFFZSCommand& C) { ++C.FacilitySpec.FootprintSizeCells.X; });
    ExpectMutationDetected(TEXT("One-centimeter clearance participates"),
        [](FFFZSCommand& C) { ++C.FacilitySpec.ClearanceCells; });
    ExpectMutationDetected(TEXT("Category participates"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.Category = EFFZSFacilityCategory::Info;
        });
    ExpectMutationDetected(TEXT("Max waiting participates"),
        [](FFFZSCommand& C) {
            ++C.FacilitySpec.QueueRules.MaxWaitingAgents;
        });
    ExpectMutationDetected(TEXT("Arrival tolerance participates"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.QueueRules.SlotArrivalToleranceCm += 1.0f;
        });
    ExpectMutationDetected(TEXT("Queue spline participates"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.QueueSplinePointsCm[0].X += 1.0;
        });
    ExpectMutationDetected(TEXT("Compressed queue Rect participates"),
        [](FFFZSCommand& C) {
            ++C.FacilitySpec.QueueReservedRects[0].Min.X;
        });
    ExpectMutationDetected(TEXT("Anchor transform participates"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.Anchors[0].LocalTransform.AddToTranslation(
                FVector(1.0, 0.0, 0.0));
        });
    ExpectMutationDetected(TEXT("Allowed Zones participate"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.AllowedZoneIds.Add(TEXT("GZ_Operations"));
        });
    ExpectMutationDetected(TEXT("External keys participate"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.InstallCostKey = TEXT("Cost.Changed");
        });
    ExpectMutationDetected(TEXT("Obstacle height participates"),
        [](FFFZSCommand& C) {
            C.FacilitySpec.PlacementObstacleHeightCm += 1.0f;
        });

    FFFZSCommand Same = BaselineCommand;
    TestEqual(TEXT("CRC helper remains stable"),
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
            TestEqual(TEXT("FText payload identity is culture invariant"),
                EnglishFingerprint,
                UFFZSGridLibrary::BuildCommandFingerprint(CultureCommand));
        }
    }
    FInternationalization::Get().SetCurrentCulture(OriginalCulture);
    return true;
}

#endif
