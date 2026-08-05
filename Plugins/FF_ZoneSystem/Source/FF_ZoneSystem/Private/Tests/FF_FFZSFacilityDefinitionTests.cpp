/**
 * @file FF_FFZSFacilityDefinitionTests.cpp
 * @brief 1 cm 내장 시설 사양, 압축 Queue Region, 예산과 Editor Data Validation 계약을 검증한다.
 *
 * 흐름: 내장 Spec은 압축 Rect 수와 면적만 읽고, 오류 경계는 작은 합성 입력이나 단일 필드 변형으로 검증한다.
 * 불변식: SchemaVersion 2, 1 Cell=1 cm, Clearance 100=1 m이며 새 콘텐츠는 QueueReservedRects를 사용한다.
 * 실패/성능: 600x300 같은 대형 내장 footprint를 Get*Cells로 펼치지 않고 Region API와 64비트 면적만 사용한다.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

/*
 * 테스트 매트릭스
 * - BuiltInPresets: 7개 프리셋의 Schema 2, 1 cm 크기, Clearance 100, 압축 Queue Region 회귀
 * - Validation: 작은 합성 예산, Rect 유효성·정규화, Queue tail 포함, 오버플로 fail-closed
 * - EditorDataValidation: 같은 1 cm/Region 도메인 오류가 Data Asset 저작 UI까지 전달되는지 확인
 * 표 기반 Arrange로 프리셋 추가 누락을 드러내고 각 변형은 한 원인만 바꿔 오류 책임을 분리한다.
 */
namespace
{
    struct FFFZSPresetExpectation
    {
        EFFZSBuiltInFacilityPreset Preset;
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

    /**
     * CanEnumerateFacilityCells의 보수적 예산만 검증하는 작은 합성 사양이다.
     * expanded footprint는 9x9=81, queue는 1이므로 필요한 작업 예산은 정확히 82다.
     */
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
    FFFZSOneCentimeterBuiltInDefinitionsTest,
    "FF_ZoneSystem.Unit.Definitions.OneCentimeterCompressedBuiltInPresets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOneCentimeterBuiltInDefinitionsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    // 담당자가 프로젝트 예산을 낮춰도 내장 기준값 회귀 테스트는 독립적으로 재현되어야 한다.
    TGuardValue<int32> StableRegionBudget(
        GetMutableDefault<UFFZSDeveloperSettings>()->MaxEnumeratedCellsPerFacility,
        1'000'000);

    // Arrange: 1 m 단위를 100배한 1 cm footprint와 압축 queue 면적을 표로 고정한다.
    const FFFZSPresetExpectation Expectations[] = {
        { EFFZSBuiltInFacilityPreset::FoodTruckBasic,
          FName(TEXT("FD_Booth_FoodTruckBasic")),
          EFFZSFacilityCategory::Booth,
          FIntPoint(600, 300), 3, 160'000, 12, 1 },
        { EFFZSBuiltInFacilityPreset::ToiletBasic,
          FName(TEXT("FD_Toilet_Basic")),
          EFFZSFacilityCategory::Toilet,
          FIntPoint(400, 400), 1, 240'000, 8, 2 },
        { EFFZSBuiltInFacilityPreset::InfoBasic,
          FName(TEXT("FD_Info_Basic")),
          EFFZSFacilityCategory::Info,
          FIntPoint(300, 200), 1, 70'000, 6, 1 },
        { EFFZSBuiltInFacilityPreset::RestBasic,
          FName(TEXT("FD_Rest_Basic")),
          EFFZSFacilityCategory::Rest,
          FIntPoint(400, 300), 1, 40'000, 4, 2 },
        { EFFZSBuiltInFacilityPreset::PerformanceBasic,
          FName(TEXT("FD_Performance_Basic")),
          EFFZSFacilityCategory::Performance,
          FIntPoint(800, 500), 1, 180'000, 16, 2 },
        { EFFZSBuiltInFacilityPreset::SafetyBasic,
          FName(TEXT("FD_Safety_Basic")),
          EFFZSFacilityCategory::Safety,
          FIntPoint(400, 300), 1, 70'000, 6, 2 },
        { EFFZSBuiltInFacilityPreset::PhotoSpotBasic,
          FName(TEXT("FD_PhotoSpot_Basic")),
          EFFZSFacilityCategory::PhotoSpot,
          FIntPoint(500, 300), 1, 90'000, 8, 1 }
    };

    // Act/Assert: 대형 프리셋은 IsValid와 압축 Rect 면적만 사용하고 Get*Cells로 펼치지 않는다.
    TSet<FName> UniqueDefinitionIds;
    for (const FFFZSPresetExpectation& Expected : Expectations)
    {
        const FString Label = Expected.DefinitionId.ToString();
        const FFFZSFacilitySpec Spec =
            UFFZSGridLibrary::MakeBuiltInFacilitySpec(Expected.Preset);
        TArray<FText> Errors;

        TestTrue(FString::Printf(TEXT("%s is valid Schema 2 data"), *Label),
            Spec.IsValid(Errors));
        TestEqual(*FString::Printf(TEXT("%s schema"), *Label),
            Spec.SchemaVersion, 2);
        TestEqual(*FString::Printf(TEXT("%s definition ID"), *Label),
            Spec.FacilityDefinitionId, Expected.DefinitionId);
        TestTrue(FString::Printf(TEXT("%s category"), *Label),
            Spec.Category == Expected.Category);
        TestEqual(*FString::Printf(TEXT("%s one-centimeter footprint"), *Label),
            Spec.FootprintSizeCells, Expected.Footprint);
        TestEqual(*FString::Printf(TEXT("%s one-meter clearance"), *Label),
            Spec.ClearanceCells, 100);
        TestEqual(*FString::Printf(TEXT("%s compressed queue Rect count"), *Label),
            Spec.QueueReservedRects.Num(), Expected.QueueRectCount);
        TestEqual(*FString::Printf(TEXT("%s compressed queue area"), *Label),
            UFFZSGridLibrary::GetCellRectArea(Spec.QueueReservedRects),
            Expected.QueueAreaCells);
        TestTrue(FString::Printf(TEXT("%s does not author legacy queue Cells"), *Label),
            Spec.QueueReservedCells.IsEmpty());
        TestTrue(FString::Printf(TEXT("%s stays inside the conservative Region budget"), *Label),
            UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
        TestEqual(*FString::Printf(TEXT("%s maximum queue"), *Label),
            Spec.QueueRules.MaxWaitingAgents, Expected.MaxWaitingAgents);
        TestEqual(*FString::Printf(TEXT("%s service capacity"), *Label),
            Spec.QueueRules.ServiceCapacity, Expected.ServiceCapacity);
        TestTrue(FString::Printf(TEXT("%s has a queue spline"), *Label),
            Spec.QueueSplinePointsCm.Num() >= 2);
        TestTrue(FString::Printf(TEXT("%s has required anchors"), *Label),
            Spec.Anchors.Num() >= Expected.ServiceCapacity + 2);
        TestFalse(FString::Printf(TEXT("%s install key"), *Label),
            Spec.InstallCostKey.IsNone());
        TestFalse(FString::Printf(TEXT("%s revenue key"), *Label),
            Spec.RevenueRuleKey.IsNone());
        TestFalse(FString::Printf(TEXT("%s experience key"), *Label),
            Spec.ExperienceEffectKey.IsNone());
        TestFalse(FString::Printf(TEXT("%s ID is unique"), *Label),
            UniqueDefinitionIds.Contains(Spec.FacilityDefinitionId));
        UniqueDefinitionIds.Add(Spec.FacilityDefinitionId);
    }

    // 승인된 두 기준 프리셋은 압축 사각형의 물리 범위까지 고정해 1 m→1 cm 이관 회귀를 잡는다.
    const FFFZSFacilitySpec FoodTruck =
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(
            EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    TestEqual(TEXT("Accepted Food Truck clearance is one hundred 1 cm Cells"),
        FoodTruck.ClearanceCells, 100);
    TestEqual(TEXT("Accepted Food Truck queue uses three compressed Rects"),
        FoodTruck.QueueReservedRects.Num(), 3);
    TestEqual(TEXT("Accepted Food Truck queue preserves sixteen square-meter Cells"),
        UFFZSGridLibrary::GetCellRectArea(FoodTruck.QueueReservedRects),
        static_cast<int64>(160'000));
    TestTrue(TEXT("Accepted Food Truck right queue arm is preserved"),
        FoodTruck.QueueReservedRects.Contains(
            FFFZSCellRect(FIntPoint(500, 300), FIntPoint(600, 900))));
    TestTrue(TEXT("Accepted Food Truck top queue arm is preserved"),
        FoodTruck.QueueReservedRects.Contains(
            FFFZSCellRect(FIntPoint(0, 800), FIntPoint(500, 900))));
    TestTrue(TEXT("Accepted Food Truck left queue arm is preserved"),
        FoodTruck.QueueReservedRects.Contains(
            FFFZSCellRect(FIntPoint(0, 300), FIntPoint(100, 800))));
    TestEqual(TEXT("Accepted Food Truck spline points"),
        FoodTruck.QueueSplinePointsCm.Num(), 4);
    TestEqual(TEXT("Accepted Food Truck anchors"),
        FoodTruck.Anchors.Num(), 3);
    TestEqual(TEXT("Accepted Food Truck service duration remains seconds"),
        FoodTruck.QueueRules.ServiceDurationSeconds, 6.0f);
    TestEqual(TEXT("Accepted Food Truck slot spacing remains centimeters"),
        FoodTruck.QueueRules.SlotSpacingCm, 115.0f);

    const FFFZSFacilitySpec Toilet =
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(
            EFFZSBuiltInFacilityPreset::ToiletBasic);
    TestEqual(TEXT("Accepted Restroom clearance is one hundred 1 cm Cells"),
        Toilet.ClearanceCells, 100);
    TestEqual(TEXT("Accepted Restroom queue uses one compressed Rect"),
        Toilet.QueueReservedRects.Num(), 1);
    TestTrue(TEXT("Accepted Restroom two-meter by twelve-meter queue is preserved"),
        Toilet.QueueReservedRects.Num() == 1 &&
        Toilet.QueueReservedRects[0] ==
            FFFZSCellRect(FIntPoint(100, 400), FIntPoint(300, 1600)));
    TestEqual(TEXT("Accepted Restroom compressed queue area"),
        UFFZSGridLibrary::GetCellRectArea(Toilet.QueueReservedRects),
        static_cast<int64>(240'000));
    TestEqual(TEXT("Accepted Restroom spline points"),
        Toilet.QueueSplinePointsCm.Num(), 2);
    TestEqual(TEXT("Accepted Restroom anchors"),
        Toilet.Anchors.Num(), 4);
    TestEqual(TEXT("Accepted Restroom service duration remains seconds"),
        Toilet.QueueRules.ServiceDurationSeconds, 4.0f);
    TestEqual(TEXT("Accepted Restroom slot spacing remains centimeters"),
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

    // 작은 합성 Spec으로만 담당자 예산과 하드 clamp를 검증한다. 내장 프리셋은 펼치지 않는다.
    {
        TGuardValue<int32> LimitGuard(
            MutableSettings->MaxEnumeratedCellsPerFacility, 64);
        TestEqual(TEXT("Owner setting drives the effective Region work limit"),
            UFFZSGridLibrary::GetMaxEnumeratedCellsPerFacility(), 64);
        TestFalse(TEXT("An 82-work-unit synthetic Region exceeds a limit of 64"),
            UFFZSGridLibrary::CanEnumerateFacilityCells(MakeSmallBudgetSpec()));
    }
    {
        TGuardValue<int32> LowerClampGuard(
            MutableSettings->MaxEnumeratedCellsPerFacility, 0);
        TestEqual(TEXT("Region work limit clamps to at least one"),
            UFFZSGridLibrary::GetMaxEnumeratedCellsPerFacility(), 1);
    }
    {
        TGuardValue<int32> UpperClampGuard(
            MutableSettings->MaxEnumeratedCellsPerFacility,
            TNumericLimits<int32>::Max());
        TestEqual(TEXT("Region work limit clamps to the hard safety ceiling"),
            UFFZSGridLibrary::GetMaxEnumeratedCellsPerFacility(), 1'000'000);
    }

    FFFZSFacilitySpec Spec =
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(
            EFFZSBuiltInFacilityPreset::FoodTruckBasic);

    // 유효한 압축 Spec에서 한 경계만 바꿔 오류 원인을 독립적으로 확인한다.
    TestTrue(TEXT("Empty AllowedZoneIds remains portable and valid"),
        Spec.IsValid(Errors));
    Spec.AllowedZoneIds = {
        FName(TEXT("PartnerVenue_Main")),
        FName(TEXT("Prototype.Zone.02"))
    };
    TestTrue(TEXT("Unique non-Festa Zone IDs are valid"),
        Spec.IsValid(Errors));

    Spec.AllowedZoneIds = {
        FName(TEXT("PartnerVenue_Main")),
        FName(TEXT("PartnerVenue_Main"))
    };
    TestFalse(TEXT("Duplicate allowed Zone IDs are rejected"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("Duplicate Zone error is actionable"),
        ContainsErrorText(Errors, TEXT("unique")));

    Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(
        EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    Spec.AllowedZoneIds = { NAME_None };
    TestFalse(TEXT("NAME_None allowed Zone ID is rejected"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("Empty Zone error is actionable"),
        ContainsErrorText(Errors, TEXT("NAME_None")));

    Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(
        EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    Spec.SchemaVersion = 1;
    TestFalse(TEXT("Legacy one-meter FacilitySpec schema is rejected"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("Schema error names the one-centimeter Schema 2 contract"),
        ContainsErrorText(Errors, TEXT("SchemaVersion 2")));

    Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(
        EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    Spec.FootprintSizeCells = FIntPoint(0, 1);
    TestFalse(TEXT("Zero-width one-centimeter footprint is rejected"),
        Spec.IsValid(Errors));

    Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(
        EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    Spec.QueueReservedRects[0].Max = Spec.QueueReservedRects[0].Min;
    TestFalse(TEXT("Zero-area QueueReservedRect is rejected"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("Invalid compressed Rect error explains half-open bounds"),
        ContainsErrorText(Errors, TEXT("Min.X < Max.X")));

    // 겹치거나 중복된 유효 Rect는 저작 오류가 아니라 결정적 합집합 입력이다.
    Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(
        EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    const int64 OriginalQueueArea =
        UFFZSGridLibrary::GetCellRectArea(Spec.QueueReservedRects);
    const FFFZSCellRect DuplicateQueueRect = Spec.QueueReservedRects[0];
    Spec.QueueReservedRects.Add(DuplicateQueueRect);
    TestTrue(TEXT("Duplicate valid Queue Rects canonicalize without changing validity"),
        Spec.IsValid(Errors));
    TestEqual(TEXT("Duplicate valid Queue Rects do not double-count area"),
        UFFZSGridLibrary::GetCellRectArea(Spec.QueueReservedRects),
        OriginalQueueArea);

    Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(
        EFFZSBuiltInFacilityPreset::InfoBasic);
    Spec.QueueReservedRects.Reset();
    TestFalse(TEXT("New Schema 2 data requires a compressed queue Region"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("Missing Region error names QueueReservedRects"),
        ContainsErrorText(Errors, TEXT("QueueReservedRects")));

    Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(
        EFFZSBuiltInFacilityPreset::InfoBasic);
    TestTrue(TEXT("Straight prototype contains every 115 cm-spaced slot"),
        Spec.IsValid(Errors));
    Spec.QueueReservedRects[0].Max.Y =
        Spec.QueueReservedRects[0].Min.Y + 100;
    TestFalse(TEXT("A compressed Region shorter than the queue tail is rejected"),
        Spec.IsValid(Errors));
    TestTrue(TEXT("Queue tail error is actionable"),
        ContainsErrorText(Errors, TEXT("Queue slot")));

    Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(
        EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    Spec.FootprintSizeCells =
        FIntPoint(TNumericLimits<int32>::Max(), TNumericLimits<int32>::Max());
    TestFalse(TEXT("Overflow-sized one-centimeter footprint exceeds the Region budget"),
        UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    TestFalse(TEXT("Overflow-sized one-centimeter footprint is invalid"),
        Spec.IsValid(Errors));
    const FString EffectiveLimitText =
        FText::AsNumber(
            UFFZSGridLibrary::GetMaxEnumeratedCellsPerFacility()).ToString();
    TestTrue(TEXT("Overflow error names the effective owner setting"),
        ContainsErrorText(Errors, *EffectiveLimitText));
    TestTrue(TEXT("Compressed Region API fails fast for overflow data"),
        UFFZSGridLibrary::GetReservedRects(
            Spec, FFFZSGridPlacement()).IsEmpty());

    Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(
        EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    Spec.ClearanceCells = TNumericLimits<int32>::Max();
    TestFalse(TEXT("Overflow-sized one-centimeter clearance exceeds the Region budget"),
        UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    TestFalse(TEXT("Overflow-sized one-centimeter clearance is invalid"),
        Spec.IsValid(Errors));

    const FFFZSFacilitySpec InvalidPreset =
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(
            static_cast<EFFZSBuiltInFacilityPreset>(
                TNumericLimits<uint8>::Max()));
    TestFalse(TEXT("Unknown one-centimeter built-in preset fails closed"),
        InvalidPreset.IsValid(Errors));
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
    Definition->Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(
        EFFZSBuiltInFacilityPreset::InfoBasic);

    FDataValidationContext ValidContext;
    TestTrue(TEXT("Valid one-centimeter Data Asset reports Valid"),
        Definition->IsDataValid(ValidContext) ==
            EDataValidationResult::Valid);
    TestEqual(TEXT("Valid compressed Data Asset has no validation errors"),
        ValidContext.GetNumErrors(), 0U);

    // 필수 ID 하나를 제거하면 같은 도메인 오류가 Editor 저작 UI까지 전달되어야 한다.
    Definition->Spec.FacilityDefinitionId = NAME_None;
    FDataValidationContext InvalidContext;
    TestTrue(TEXT("Invalid one-centimeter Data Asset reports Invalid"),
        Definition->IsDataValid(InvalidContext) ==
            EDataValidationResult::Invalid);
    TestTrue(TEXT("Invalid Data Asset exposes authoring errors"),
        InvalidContext.GetNumErrors() > 0U);
    return true;
}
#endif

#endif
