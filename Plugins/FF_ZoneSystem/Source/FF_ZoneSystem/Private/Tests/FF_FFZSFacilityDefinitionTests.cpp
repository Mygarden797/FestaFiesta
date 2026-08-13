// FF_FFZSFacilityDefinitionTests.cpp

// 1 cm 테스트 시설 사양, 압축 Queue Region, 예산과 Editor Data Validation 계약을 검증.
// 흐름: 테스트 fixture Spec은 압축 Rect 수와 면적만 읽고, 오류 경계는 작은 합성 입력이나 단일 필드 변형으로 검증.
// 불변식: SchemaVersion 2, 1 Cell=1 cm, Clearance 100=1 m이며 새 콘텐츠는 QueueReservedRects를 사용.
// 실패/성능: 600x300 같은 대형 fixture footprint를 Get*Cells로 펼치지 않고 Region API와 64비트 면적만 사용.


#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "Tests/FF_FFZSTestFixtures.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

//테스트 매트릭스
// TestFixtures: 7개 테스트 변형의 Schema 2, 1 cm 크기, Clearance 100, 압축 Queue Region 회귀
// Validation: 작은 합성 예산, Rect 유효성·정규화, Queue tail 포함, 오버플로 fail-closed
// EditorDataValidation: 같은 1 cm/Region 도메인 오류가 Data Asset 저작 UI까지 전달되는지 확인
// 표 기반 Arrange로 fixture 추가 누락을 드러내고 각 변형은 한 원인만 바꿔 오류 책임을 분리.
namespace
{
    struct FFFZSTestFacilityExpectation
    {
        EFFZSTestFacilityVariant Variant;
        FName DefinitionId;
        EFFZSFacilityCategory Category;
        FIntPoint Footprint;
        int32 QueueRectCount;
        int64 QueueAreaCells;
        int32 MaxWaitingAgents;
        int32 ServiceCapacity;
    };

    bool ContainsErrorText(const TArray<FText>& Errors, const TCHAR* ExpectedText)
    {
        for (const FText& Error : Errors)
        {
            if (Error.ToString().Contains(ExpectedText))
            {
                return true;
            }
        }
        return false;
    }

    // CanEnumerateFacilityCells의 보수적 예산만 검증하는 작은 합성 사양.
    // expanded footprint는 9x9=81, queue는 1이므로 필요한 작업 예산은 정확히 82다.
    
    FFFZSFacilitySpec MakeSmallBudgetSpec()
    {
        FFFZSFacilitySpec Spec;
        Spec.SchemaVersion = 2;
        Spec.FootprintSizeCells = FIntPoint(7, 7);
        Spec.ClearanceCells = 1;
        Spec.QueueReservedRects = {
            FFFZSCellRect(FIntPoint(7, 0), FIntPoint(8, 1))
        };
        Spec.QueueReservedCells.Reset();
        return Spec;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSOneCentimeterTestFacilityDefinitionsTest,
    "FF_ZoneSystem.Unit.Definitions.OneCentimeterTestFacilityFixtures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOneCentimeterTestFacilityDefinitionsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    // 담당자가 프로젝트 예산을 낮춰도 fixture 회귀 테스트는 독립적으로 재현되어야 한다.
    TGuardValue<int32> StableRegionBudget(
        GetMutableDefault<UFFZSDeveloperSettings>()->MaxEnumeratedCellsPerFacility,
        1'000'000);

    // Arrange: 1 m 단위를 100배한 1 cm footprint와 압축 queue 면적을 표로 고정한다.
    const FFFZSTestFacilityExpectation Expectations[] = {
        { EFFZSTestFacilityVariant::FoodTruck,
          FName(TEXT("FD_Booth_FoodTruckBasic")),
          EFFZSFacilityCategory::Booth,
          FIntPoint(600, 300), 3, 160'000, 12, 1 },
        { EFFZSTestFacilityVariant::Toilet,
          FName(TEXT("FD_Toilet_Basic")),
          EFFZSFacilityCategory::Toilet,
          FIntPoint(400, 400), 1, 240'000, 8, 2 },
        { EFFZSTestFacilityVariant::Info,
          FName(TEXT("FD_Info_Basic")),
          EFFZSFacilityCategory::Info,
          FIntPoint(300, 200), 1, 70'000, 6, 1 },
        { EFFZSTestFacilityVariant::Rest,
          FName(TEXT("FD_Rest_Basic")),
          EFFZSFacilityCategory::Rest,
          FIntPoint(400, 300), 1, 40'000, 4, 2 },
        { EFFZSTestFacilityVariant::Performance,
          FName(TEXT("FD_Performance_Basic")),
          EFFZSFacilityCategory::Performance,
          FIntPoint(800, 500), 1, 180'000, 16, 2 },
        { EFFZSTestFacilityVariant::Safety,
          FName(TEXT("FD_Safety_Basic")),
          EFFZSFacilityCategory::Safety,
          FIntPoint(400, 300), 1, 70'000, 6, 2 },
        { EFFZSTestFacilityVariant::PhotoSpot,
          FName(TEXT("FD_PhotoSpot_Basic")),
          EFFZSFacilityCategory::PhotoSpot,
          FIntPoint(500, 300), 1, 90'000, 8, 1 }
    };

    // Act/Assert: 대형 fixture는 IsValid와 압축 Rect 면적만 사용하고 Get*Cells로 펼치지 않는다.
    TSet<FName> UniqueDefinitionIds;
    for (const FFFZSTestFacilityExpectation& Expected : Expectations)
    {
        const FString Label = Expected.DefinitionId.ToString();
        const FFFZSFacilitySpec Spec =
            FFZSTest::MakeValidFacilitySpec(Expected.Variant);
        TArray<FText> Errors;

        TestTrue(FString::Printf(TEXT("%s가 유효한 Schema 2 데이터다"), *Label),
            Spec.IsValid(Errors));
        TestEqual(*FString::Printf(TEXT("%s의 스키마가 일치한다"), *Label),
            Spec.SchemaVersion, 2);
        TestEqual(*FString::Printf(TEXT("%s의 정의 ID가 일치한다"), *Label),
            Spec.FacilityDefinitionId, Expected.DefinitionId);
        TestTrue(FString::Printf(TEXT("%s의 카테고리가 유효하다"), *Label),
            Spec.Category == Expected.Category);
        TestEqual(*FString::Printf(TEXT("%s의 1센티미터 점유 영역이 일치한다"), *Label),
            Spec.FootprintSizeCells, Expected.Footprint);
        TestEqual(*FString::Printf(TEXT("%s의 1미터 여유 영역이 일치한다"), *Label),
            Spec.ClearanceCells, 100);
        TestEqual(*FString::Printf(TEXT("%s의 압축 대기열 Rect 개수가 일치한다"), *Label),
            Spec.QueueReservedRects.Num(), Expected.QueueRectCount);
        TestEqual(*FString::Printf(TEXT("%s의 압축 대기열 면적이 일치한다"), *Label),
            UFFZSGridLibrary::GetCellRectArea(Spec.QueueReservedRects),
            Expected.QueueAreaCells);
        TestTrue(FString::Printf(TEXT("%s는 레거시 대기열 Cell을 작성하지 않는다"), *Label),
            Spec.QueueReservedCells.IsEmpty());
        TestTrue(FString::Printf(TEXT("%s는 보수적 Region 작업 예산 이내다"), *Label),
            UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
        TestEqual(*FString::Printf(TEXT("%s의 최대 대기 인원이 일치한다"), *Label),
            Spec.QueueRules.MaxWaitingAgents, Expected.MaxWaitingAgents);
        TestEqual(*FString::Printf(TEXT("%s의 서비스 수용량이 일치한다"), *Label),
            Spec.QueueRules.ServiceCapacity, Expected.ServiceCapacity);
        TestTrue(FString::Printf(TEXT("%s에 대기열 스플라인이 있다"), *Label),
            Spec.QueueSplinePointsCm.Num() >= 2);
        TestTrue(FString::Printf(TEXT("%s에 필수 앵커가 있다"), *Label),
            Spec.Anchors.Num() >= Expected.ServiceCapacity + 2);
        TestFalse(FString::Printf(TEXT("%s의 설치 비용 키가 설정되어 있다"), *Label),
            Spec.InstallCostKey.IsNone());
        TestFalse(FString::Printf(TEXT("%s의 수익 키가 설정되어 있다"), *Label),
            Spec.RevenueRuleKey.IsNone());
        TestFalse(FString::Printf(TEXT("%s의 경험치 키가 설정되어 있다"), *Label),
            Spec.ExperienceEffectKey.IsNone());
        TestFalse(FString::Printf(TEXT("%s의 ID가 고유하다"), *Label),
            UniqueDefinitionIds.Contains(Spec.FacilityDefinitionId));
        UniqueDefinitionIds.Add(Spec.FacilityDefinitionId);
    }

    // 두 기준 fixture는 압축 사각형의 물리 범위까지 고정해 1 m→1 cm 이관 회귀를 잡는다.
    const FFFZSFacilitySpec FoodTruck =
        FFZSTest::MakeValidFacilitySpec(
            EFFZSTestFacilityVariant::FoodTruck);
    TestEqual(TEXT("승인된 푸드 트럭의 여유 영역은 1cm Cell 100개다"),
        FoodTruck.ClearanceCells, 100);
    TestEqual(TEXT("승인된 푸드 트럭의 대기열은 압축 Rect 3개를 사용한다"),
        FoodTruck.QueueReservedRects.Num(), 3);
    TestEqual(TEXT("승인된 푸드 트럭의 대기열은 제곱미터 Cell 16개를 보존한다"),
        UFFZSGridLibrary::GetCellRectArea(FoodTruck.QueueReservedRects),
        static_cast<int64>(160'000));
    TestTrue(TEXT("승인된 푸드 트럭의 오른쪽 대기열 구간이 보존된다"),
        FoodTruck.QueueReservedRects.Contains(
            FFFZSCellRect(FIntPoint(500, 300), FIntPoint(600, 900))));
    TestTrue(TEXT("승인된 푸드 트럭의 위쪽 대기열 구간이 보존된다"),
        FoodTruck.QueueReservedRects.Contains(
            FFFZSCellRect(FIntPoint(0, 800), FIntPoint(500, 900))));
    TestTrue(TEXT("승인된 푸드 트럭의 왼쪽 대기열 구간이 보존된다"),
        FoodTruck.QueueReservedRects.Contains(
            FFFZSCellRect(FIntPoint(0, 300), FIntPoint(100, 800))));
    TestEqual(TEXT("승인된 푸드 트럭의 스플라인 포인트 수가 일치한다"),
        FoodTruck.QueueSplinePointsCm.Num(), 4);
    TestEqual(TEXT("승인된 푸드 트럭의 앵커 수가 일치한다"),
        FoodTruck.Anchors.Num(), 3);
    TestEqual(TEXT("승인된 푸드 트럭의 서비스 시간이 초 단위로 유지된다"),
        FoodTruck.QueueRules.ServiceDurationSeconds, 6.0f);
    TestEqual(TEXT("승인된 푸드 트럭의 슬롯 간격이 센티미터 단위로 유지된다"),
        FoodTruck.QueueRules.SlotSpacingCm, 115.0f);

    const FFFZSFacilitySpec Toilet =
        FFZSTest::MakeValidFacilitySpec(
            EFFZSTestFacilityVariant::Toilet);
    TestEqual(TEXT("승인된 화장실의 여유 영역은 1cm Cell 100개다"),
        Toilet.ClearanceCells, 100);
    TestEqual(TEXT("승인된 화장실의 대기열은 압축 Rect 1개를 사용한다"),
        Toilet.QueueReservedRects.Num(), 1);
    TestTrue(TEXT("승인된 화장실의 2m x 12m 대기열이 보존된다"),
        Toilet.QueueReservedRects.Num() == 1 &&
        Toilet.QueueReservedRects[0] ==
            FFFZSCellRect(FIntPoint(100, 400), FIntPoint(300, 1600)));
    TestEqual(TEXT("승인된 화장실의 압축 대기열 면적이 일치한다"),
        UFFZSGridLibrary::GetCellRectArea(Toilet.QueueReservedRects),
        static_cast<int64>(240'000));
    TestEqual(TEXT("승인된 화장실의 스플라인 포인트 수가 일치한다"),
        Toilet.QueueSplinePointsCm.Num(), 2);
    TestEqual(TEXT("승인된 화장실의 앵커 수가 일치한다"),
        Toilet.Anchors.Num(), 4);
    TestEqual(TEXT("승인된 화장실의 서비스 시간이 초 단위로 유지된다"),
        Toilet.QueueRules.ServiceDurationSeconds, 4.0f);
    TestEqual(TEXT("승인된 화장실의 슬롯 간격이 센티미터 단위로 유지된다"),
        Toilet.QueueRules.SlotSpacingCm, 115.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSOneCentimeterFacilityDefinitionValidationTest,
    "FF_ZoneSystem.Unit.Definitions.OneCentimeterCompressedRegionValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOneCentimeterFacilityDefinitionValidationTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    TArray<FText> Errors;
    UFFZSDeveloperSettings* MutableSettings =
        GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<int32> StableRegionBudget(
        MutableSettings->MaxEnumeratedCellsPerFacility, 1'000'000);

    // 작은 합성 Spec으로만 담당자 예산과 하드 clamp를 검증한다. fixture는 펼치지 않는다.
    {
        TGuardValue<int32> LimitGuard(
            MutableSettings->MaxEnumeratedCellsPerFacility, 64);
        TestEqual(TEXT("담당자 설정이 유효 Region 작업 제한을 결정한다"),
            UFFZSGridLibrary::GetMaxEnumeratedCellsPerFacility(), 64);
        TestFalse(TEXT("작업 단위 82인 합성 Region은 제한 64를 초과한다"),
            UFFZSGridLibrary::CanEnumerateFacilityCells(MakeSmallBudgetSpec()));
    }
    {
        TGuardValue<int32> LowerClampGuard(
            MutableSettings->MaxEnumeratedCellsPerFacility, 0);
        TestEqual(TEXT("Region 작업 제한은 최소 1로 제한된다"),
            UFFZSGridLibrary::GetMaxEnumeratedCellsPerFacility(), 1);
    }
    {
        TGuardValue<int32> UpperClampGuard(
            MutableSettings->MaxEnumeratedCellsPerFacility,
            TNumericLimits<int32>::Max());
        TestEqual(TEXT("Region 작업 제한은 최대 안전 한도로 제한된다"),
            UFFZSGridLibrary::GetMaxEnumeratedCellsPerFacility(), 10'000'000);
    }

    FFFZSFacilitySpec Spec =
        FFZSTest::MakeValidFacilitySpec(
            EFFZSTestFacilityVariant::FoodTruck);

    // 유효한 압축 Spec에서 한 경계만 바꿔 오류 원인을 독립적으로 확인한다.
    TestTrue(TEXT("빈 AllowedZoneIds는 이식 가능하며 유효하다"),
        Spec.IsValid(Errors));
    Spec.AllowedZoneIds = {
        FName(TEXT("PartnerVenue_Main")),
        FName(TEXT("Prototype.Zone.02"))
    };
    TestTrue(TEXT("Festa 외부의 고유한 Zone ID도 유효하다"),
        Spec.IsValid(Errors));

    Spec.AllowedZoneIds = {
        FName(TEXT("PartnerVenue_Main")),
        FName(TEXT("PartnerVenue_Main"))
    };
    TestFalse(TEXT("중복된 허용 Zone ID는 거부된다"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("중복 Zone 오류에 해결 가능한 정보가 포함된다"),
        ContainsErrorText(Errors, TEXT("고유")));

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    Spec.AllowedZoneIds = { NAME_None };
    TestFalse(TEXT("NAME_None 허용 Zone ID는 거부된다"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("빈 Zone 오류에 해결 가능한 정보가 포함된다"),
        ContainsErrorText(Errors, TEXT("NAME_None")));

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    Spec.SchemaVersion = 1;
    TestFalse(TEXT("레거시 1미터 FacilitySpec 스키마는 거부된다"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("스키마 오류가 1센티미터 Schema 2 계약을 명시한다"),
        ContainsErrorText(Errors, TEXT("SchemaVersion 2")));

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    Spec.FootprintSizeCells = FIntPoint(0, 1);
    TestFalse(TEXT("폭이 0인 1센티미터 점유 영역은 거부된다"),
        Spec.IsValid(Errors));

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    Spec.QueueReservedRects[0].Max = Spec.QueueReservedRects[0].Min;
    TestFalse(TEXT("면적이 0인 QueueReservedRect는 거부된다"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("잘못된 압축 Rect 오류가 반개방 경계를 설명한다"),
        ContainsErrorText(Errors, TEXT("Min.X < Max.X")));

    // 겹치거나 중복된 유효 Rect는 저작 오류가 아니라 결정적 합집합 입력이다.
    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    const int64 OriginalQueueArea =
        UFFZSGridLibrary::GetCellRectArea(Spec.QueueReservedRects);
    const FFFZSCellRect DuplicateQueueRect = Spec.QueueReservedRects[0];
    Spec.QueueReservedRects.Add(DuplicateQueueRect);
    TestTrue(TEXT("유효한 중복 Queue Rect는 유효성을 유지한 채 정규화된다"),
        Spec.IsValid(Errors));
    TestEqual(TEXT("유효한 중복 Queue Rect의 면적은 중복 계산되지 않는다"),
        UFFZSGridLibrary::GetCellRectArea(Spec.QueueReservedRects),
        OriginalQueueArea);

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::Info);
    Spec.QueueReservedRects.Reset();
    TestFalse(TEXT("새 Schema 2 데이터에는 압축 대기열 Region이 필요하다"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("Region 누락 오류가 QueueReservedRects를 명시한다"),
        ContainsErrorText(Errors, TEXT("QueueReservedRects")));

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::Info);
    TestTrue(TEXT("직선 프로토타입에 115cm 간격의 모든 슬롯이 포함된다"),
        Spec.IsValid(Errors));
    Spec.QueueReservedRects[0].Max.Y =
        Spec.QueueReservedRects[0].Min.Y + 100;
    TestFalse(TEXT("대기열 끝보다 짧은 압축 Region은 거부된다"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("대기열 끝 오류에 해결 가능한 정보가 포함된다"),
        ContainsErrorText(Errors, TEXT("Queue 슬롯")));

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    Spec.FootprintSizeCells =
        FIntPoint(TNumericLimits<int32>::Max(), TNumericLimits<int32>::Max());
    TestFalse(TEXT("오버플로 크기의 1센티미터 점유 영역은 Region 예산을 초과한다"),
        UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    TestFalse(TEXT("오버플로 크기의 1센티미터 점유 영역은 유효하지 않다"),
        Spec.IsValid(Errors));
    const FString EffectiveLimitText =
        FText::AsNumber(
            UFFZSGridLibrary::GetMaxEnumeratedCellsPerFacility()).ToString();
    TestTrue(TEXT("오버플로 오류가 담당자의 유효 설정을 명시한다"),
        ContainsErrorText(Errors, *EffectiveLimitText));
    TestTrue(TEXT("압축 Region API는 오버플로 데이터에 조기 실패한다"),
        UFFZSGridLibrary::GetReservedRects(
            Spec, FFFZSGridPlacement()).IsEmpty());

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    Spec.ClearanceCells = TNumericLimits<int32>::Max();
    TestFalse(TEXT("오버플로 크기의 1센티미터 여유 영역은 Region 예산을 초과한다"),
        UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    TestFalse(TEXT("오버플로 크기의 1센티미터 여유 영역은 유효하지 않다"),
        Spec.IsValid(Errors));

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    Spec.VisualSizingMode = EFFZSVisualSizingMode::MatchPlacementBounds;
    Spec.VisualOffsetCm = FVector(1.0, 0.0, 0.0);
    TestFalse(TEXT("정확한 배치 경계 모드에서 시각 오프셋은 거부된다"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("시각 오프셋 오류가 0 요구사항을 설명한다"),
        ContainsErrorText(Errors, TEXT("VisualOffsetCm")));

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    Spec.VisualSizingMode = EFFZSVisualSizingMode::MatchPlacementBounds;
    Spec.VisualOffsetCm = FVector(UE_DOUBLE_KINDA_SMALL_NUMBER * 0.5, 0.0, 0.0);
    TestFalse(TEXT("정확한 배치 경계 모드에서 아주 작은 비영 오프셋도 거부된다"),
        Spec.IsValid(Errors));

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    Spec.VisualSizingMode = EFFZSVisualSizingMode::MatchPlacementBounds;
    Spec.VisualRotation.Yaw = UE_KINDA_SMALL_NUMBER * 0.5f;
    TestFalse(TEXT("정확한 배치 경계 모드에서 아주 작은 비영 회전도 거부된다"),
        Spec.IsValid(Errors));

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    Spec.QueueRules.MaxWaitingAgents = MIN_int32;
    TestFalse(TEXT("손상된 최소 int32 대기 인원은 오버플로 없이 거부된다"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("손상된 대기 인원은 QueueRules 오류로 보고된다"),
        ContainsErrorText(Errors, TEXT("Queue")));

    Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    Spec.QueueRules.ServiceCapacity = MAX_int32;
    TestFalse(TEXT("손상된 최대 int32 서비스 용량은 대규모 반복 없이 거부된다"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("서비스 용량 안전 제한 오류가 보고된다"),
        ContainsErrorText(Errors, TEXT("ServiceCapacity")));

    const FFFZSFacilitySpec InvalidFixture =
        FFZSTest::MakeValidFacilitySpec(
            static_cast<EFFZSTestFacilityVariant>(
                TNumericLimits<uint8>::Max()));
    TestFalse(TEXT("알 수 없는 테스트 시설 변형은 안전 차단으로 실패한다"),
        InvalidFixture.IsValid(Errors));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSFacilitySpecCardinalitySafetyLimitsTest,
    "FF_ZoneSystem.Unit.Definitions.CardinalitySafetyLimitsFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilitySpecCardinalitySafetyLimitsTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    FFFZSFacilitySpec AtEveryLimit = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::FoodTruck);
    AtEveryLimit.QueueReservedRects.SetNum(
        UFFZSDeveloperSettings::MaxQueueReservedRects);
    AtEveryLimit.QueueReservedCells.SetNum(
        UFFZSDeveloperSettings::MaxLegacyQueueReservedCells);
    AtEveryLimit.QueueSplinePointsCm.SetNum(
        UFFZSDeveloperSettings::MaxQueueSplinePoints);
    AtEveryLimit.Anchors.SetNum(
        UFFZSDeveloperSettings::MaxFacilityAnchors);
    AtEveryLimit.AllowedZoneIds.SetNum(
        UFFZSDeveloperSettings::MaxAllowedZoneIds);
    AtEveryLimit.QueueRules.MaxWaitingAgents =
        UFFZSDeveloperSettings::MaxQueueWaitingAgents;
    AtEveryLimit.QueueRules.ServiceCapacity =
        UFFZSDeveloperSettings::MaxFacilityServiceCapacity;
    TestTrue(TEXT("Every cardinality guard is inclusive at its documented limit"),
        UFFZSGridLibrary::HasSafeFacilitySpecCardinality(AtEveryLimit));

    const auto ExpectUnsafeCardinality =
        [this](
            const TCHAR* Label,
            const TCHAR* ExpectedDiagnostic,
            TFunctionRef<void(FFFZSFacilitySpec&)> MakeUnsafe)
    {
        FFFZSFacilitySpec Unsafe = FFZSTest::MakeValidFacilitySpec(
            EFFZSTestFacilityVariant::FoodTruck);
        MakeUnsafe(Unsafe);

        TestFalse(*FString::Printf(TEXT("%s is rejected by the constant-time preflight"), Label),
            UFFZSGridLibrary::HasSafeFacilitySpecCardinality(Unsafe));
        TestFalse(*FString::Printf(TEXT("%s cannot enter region enumeration"), Label),
            UFFZSGridLibrary::CanEnumerateFacilityCells(Unsafe));

        TArray<FText> Errors;
        TestFalse(*FString::Printf(TEXT("%s makes the facility spec invalid"), Label),
            Unsafe.IsValid(Errors));
        TestTrue(*FString::Printf(TEXT("%s reports its actionable field name"), Label),
            ContainsErrorText(Errors, ExpectedDiagnostic));
        TestTrue(*FString::Printf(TEXT("%s stops before per-element diagnostics"), Label),
            Errors.Num() <= 2);

        FFFZSCommand UnsafeCommand;
        UnsafeCommand.FacilitySpec = MoveTemp(Unsafe);
        TestEqual(
            *FString::Printf(TEXT("%s uses the bounded invalid fingerprint sentinel"), Label),
            UFFZSGridLibrary::BuildCommandFingerprint(UnsafeCommand),
            FString(TEXT("ZFCommand.v4.InvalidUnsafeFacilitySpecCardinality")));
    };

    // Thousands of malformed rects ensure IsValid returns at the cardinality boundary instead
    // of canonicalizing or producing one diagnostic per element.
    ExpectUnsafeCardinality(
        TEXT("A huge QueueReservedRects array"),
        TEXT("QueueReservedRects"),
        [](FFFZSFacilitySpec& Spec)
        {
            Spec.QueueReservedRects.SetNum(4096);
        });
    ExpectUnsafeCardinality(
        TEXT("QueueReservedCells above the migration cap"),
        TEXT("QueueReservedCells"),
        [](FFFZSFacilitySpec& Spec)
        {
            Spec.QueueReservedCells.SetNum(
                UFFZSDeveloperSettings::MaxLegacyQueueReservedCells + 1);
        });
    ExpectUnsafeCardinality(
        TEXT("QueueSplinePointsCm above the path cap"),
        TEXT("QueueSplinePointsCm"),
        [](FFFZSFacilitySpec& Spec)
        {
            Spec.QueueSplinePointsCm.SetNum(
                UFFZSDeveloperSettings::MaxQueueSplinePoints + 1);
        });
    ExpectUnsafeCardinality(
        TEXT("Anchors above the facility cap"),
        TEXT("Anchors"),
        [](FFFZSFacilitySpec& Spec)
        {
            Spec.Anchors.SetNum(
                UFFZSDeveloperSettings::MaxFacilityAnchors + 1);
        });
    ExpectUnsafeCardinality(
        TEXT("AllowedZoneIds above the facility cap"),
        TEXT("AllowedZoneIds"),
        [](FFFZSFacilitySpec& Spec)
        {
            Spec.AllowedZoneIds.SetNum(
                UFFZSDeveloperSettings::MaxAllowedZoneIds + 1);
        });
    ExpectUnsafeCardinality(
        TEXT("MaxWaitingAgents above the runtime cap"),
        TEXT("MaxWaitingAgents"),
        [](FFFZSFacilitySpec& Spec)
        {
            Spec.QueueRules.MaxWaitingAgents =
                UFFZSDeveloperSettings::MaxQueueWaitingAgents + 1;
        });
    ExpectUnsafeCardinality(
        TEXT("ServiceCapacity above the runtime cap"),
        TEXT("ServiceCapacity"),
        [](FFFZSFacilitySpec& Spec)
        {
            Spec.QueueRules.ServiceCapacity =
                UFFZSDeveloperSettings::MaxFacilityServiceCapacity + 1;
        });
    return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSOneCentimeterFacilityDefinitionEditorValidationTest,
    "FF_ZoneSystem.Unit.Definitions.OneCentimeterEditorDataValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOneCentimeterFacilityDefinitionEditorValidationTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    TGuardValue<int32> StableRegionBudget(
        GetMutableDefault<UFFZSDeveloperSettings>()->MaxEnumeratedCellsPerFacility,
        1'000'000);

    // 실제 UPrimaryDataAsset 파생 객체에 유효한 Schema 2/압축 Region Spec을 넣는다.
    UFFZSFacilityDefinition* Definition =
        NewObject<UFFZSFacilityDefinition>();
    Definition->Spec = FFZSTest::MakeValidFacilitySpec(
        EFFZSTestFacilityVariant::Info);

    FDataValidationContext ValidContext;
    TestTrue(TEXT("유효한 1센티미터 Data Asset은 Valid를 보고한다"),
        Definition->IsDataValid(ValidContext) ==
            EDataValidationResult::Valid);
    TestEqual(TEXT("유효한 압축 Data Asset에는 검증 오류가 없다"),
        ValidContext.GetNumErrors(), 0U);

    // 필수 ID 하나를 제거하면 같은 도메인 오류가 Editor 저작 UI까지 전달되어야 한다.
    Definition->Spec.FacilityDefinitionId = NAME_None;
    FDataValidationContext InvalidContext;
    TestTrue(TEXT("유효하지 않은 1센티미터 Data Asset은 Invalid를 보고한다"),
        Definition->IsDataValid(InvalidContext) ==
            EDataValidationResult::Invalid);
    TestTrue(TEXT("유효하지 않은 Data Asset이 작성 오류를 제공한다"),
        InvalidContext.GetNumErrors() > 0U);
    return true;
}
#endif

#endif
