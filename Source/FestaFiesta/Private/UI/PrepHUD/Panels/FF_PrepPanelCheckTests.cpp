// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_RequiredFacilitiesSubsystem.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Tests/AutomationCommon.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelCheck.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "UObject/StrongObjectPtr.h"

/**
 * 점검 패널 견적 고정 테스트 (U5).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * 이 화면은 **개장 판단의 근거**다. 여기 숫자가 틀리면 플레이어가 그걸 믿고 개장하고,
 * 결과는 그날 하루가 끝난 뒤에야 드러난다. 게다가 수용 위험 구간은 그만큼의 방문객이
 * 실제로 오는 날까지 기다려야 눈으로 확인된다.
 * 네 가지를 못으로 박는다 —
 *   ① 모르는 값이 `여유` 나 `0` 으로 바뀌지 않는 것
 *   ② 경계값이 뒤집혀 설정돼도 더 심한 판정이 남는 것
 *   ③ 고용비를 모를 때 0 으로 바꾸지 않는 것
 *   ④ 목표 보너스의 `0`(못 받음)과 `모름`이 구분되는 것
 */
namespace FFF_PrepCheckRulesTestHelpers
{
	/** 첫 축제 기준값. 규칙 자체는 이 숫자를 모른다 — 전부 인자로 들어간다. */
	constexpr int32 HireCostPerStaff = 450;
	constexpr int32 AdmissionFee = 5;
	constexpr int32 DailyGoal = 500;
	constexpr int32 GoalBonus = 300;

	/** `assumption` 기본 경계. 산정 근거가 아직 없다. */
	constexpr float CautionRatio = 0.8f;
	constexpr float DangerRatio = 1.0f;
}

// 모르는 것을 안전하다고 알리면 플레이어가 그걸 믿고 개장한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepCheckRules_UnknownIsNotSafe, "FestaFiesta.UI.PrepCheck.UnknownIsNotSafe", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepCheckRules_UnknownIsNotSafe::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepCheckRulesTestHelpers;

	TestEqual(TEXT("예상 방문객을 모르면 판정 불가"),
		FFF_PrepCheckRules::ClassifyCapacityRisk(INDEX_NONE, 900, CautionRatio, DangerRatio),
		EFF_PrepCapacityRisk::Unknown);

	TestEqual(TEXT("정원을 모르면 판정 불가"),
		FFF_PrepCheckRules::ClassifyCapacityRisk(400, INDEX_NONE, CautionRatio, DangerRatio),
		EFF_PrepCapacityRisk::Unknown);

	// 시설이 하나도 없으면 정원 합이 0 이다. 그때 `여유` 라고 하면 정반대다.
	TestEqual(TEXT("정원이 0 이면 판정 불가"),
		FFF_PrepCheckRules::ClassifyCapacityRisk(400, 0, CautionRatio, DangerRatio),
		EFF_PrepCapacityRisk::Unknown);
	return true;
}

// 여유 / 주의 / 위험 세 구간이 경계에서 정확히 갈려야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepCheckRules_CapacityBands, "FestaFiesta.UI.PrepCheck.CapacityBands", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepCheckRules_CapacityBands::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepCheckRulesTestHelpers;

	const int32 Capacity = 1000;

	TestEqual(TEXT("79% 는 여유"),
		FFF_PrepCheckRules::ClassifyCapacityRisk(790, Capacity, CautionRatio, DangerRatio),
		EFF_PrepCapacityRisk::Comfortable);

	// 경계는 `이상` 이다. 800 이 여유로 떨어지면 구간 하나가 사라진다.
	TestEqual(TEXT("경계 80% 는 주의"),
		FFF_PrepCheckRules::ClassifyCapacityRisk(800, Capacity, CautionRatio, DangerRatio),
		EFF_PrepCapacityRisk::Caution);

	TestEqual(TEXT("99% 는 주의"),
		FFF_PrepCheckRules::ClassifyCapacityRisk(999, Capacity, CautionRatio, DangerRatio),
		EFF_PrepCapacityRisk::Caution);

	TestEqual(TEXT("경계 100% 는 위험"),
		FFF_PrepCheckRules::ClassifyCapacityRisk(1000, Capacity, CautionRatio, DangerRatio),
		EFF_PrepCapacityRisk::Danger);

	TestEqual(TEXT("정원을 넘으면 위험"),
		FFF_PrepCheckRules::ClassifyCapacityRisk(1500, Capacity, CautionRatio, DangerRatio),
		EFF_PrepCapacityRisk::Danger);

	// ★ 경계값은 `assumption` 이라 누가 잘못 설정할 수 있다. 뒤집혀도 더 심한 판정이 남아야
	//   플레이어를 위험한 상태로 안내하지 않는다.
	TestEqual(TEXT("경계가 뒤집혀도 위험이 이긴다"),
		FFF_PrepCheckRules::ClassifyCapacityRisk(900, Capacity, /*Caution*/ 1.0f, /*Danger*/ 0.8f),
		EFF_PrepCapacityRisk::Danger);
	return true;
}

// 오늘 실제로 지불한 고용비만 표시한다. 영구 고용 인원은 매일 다시 비용이 되지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepCheckRules_StaffHireCost, "FestaFiesta.UI.PrepCheck.StaffHireCost", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepCheckRules_StaffHireCost::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepCheckRulesTestHelpers;

	// 원장이 확정한 실제 지출을 표시한다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			/*TodayStaffHireCost*/ 1350,
			INDEX_NONE, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("고용비 1,350"), E.StaffHireCost, 1350);
	}

	// 오늘 구매하지 않았다면 이전 날 고용 인원이 남아 있어도 지출은 0 이다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			/*TodayStaffHireCost*/ 0,
			INDEX_NONE, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("오늘 지출이 없으면 고용비 0"), E.StaffHireCost, 0);
	}

	// 원장을 읽지 못한 상태를 "오늘 지출 없음"으로 바꾸지 않는다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			/*TodayStaffHireCost*/ INDEX_NONE,
			INDEX_NONE, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("고용비는 비운다"), E.StaffHireCost, static_cast<int32>(INDEX_NONE));
	}

	return true;
}

// 실제 고용 → 가격 변경 → 다음 날 이월까지 ViewModel과 점검 견적이 원장과 일치해야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepCheckRules_StaffHireLedgerAcrossDays,
	"FestaFiesta.UI.PrepCheck.StaffHireLedgerAcrossDays",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepCheckRules_StaffHireLedgerAcrossDays::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepCheckRulesTestHelpers;

	// 경제 서브시스템이 없는 초기 화면에서는 0이 아니라 모름이다.
	UGameInstance* UninitializedGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UFF_FestivalViewModelSubsystem* UninitializedViewModel =
		NewObject<UFF_FestivalViewModelSubsystem>(UninitializedGameInstance);
	TestEqual(TEXT("축제 시작 전 오늘 고용비는 모름"),
		UninitializedViewModel->GetTodayStaffHireCostGold(), static_cast<int32>(INDEX_NONE));

	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UGameInstance* GameInstance = TestWorld.GetTestWorld()->GetGameInstance();
	UFF_FestivalViewModelSubsystem* ViewModel = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
	UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("ViewModel"), ViewModel) || !TestNotNull(TEXT("경제 시스템"), Progression))
	{
		return false;
	}

	UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
	UDataTable* CostTable = NewObject<UDataTable>(GameInstance);
	CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
	CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
	Definition->FacilityCostTable = CostTable;
	Definition->FestivalId = TEXT("PrepCheckStaffHireLedgerAutomationFestival");
	Definition->InitialGold = 10000;
	Definition->InitialStaffCount = 6;
	Definition->StaffHireCostGold = HireCostPerStaff;
	Definition->MaxHiredStaffCount = 6;
	Progression->StartFestival(Definition);

	const auto ReadEstimate = [ViewModel]()
	{
		return FFF_PrepCheckRules::BuildDayEstimate(
			ViewModel->GetTodayStaffHireCostGold(), INDEX_NONE, AdmissionFee, DailyGoal, GoalBonus);
	};
	TestEqual(TEXT("축제 시작 후 아직 고용하지 않았으면 0"), ReadEstimate().StaffHireCost, 0);
	if (!TestTrue(TEXT("Day1 인원 2명 고용"), Progression->TryHireStaff(2)))
	{
		return false;
	}
	TestEqual(TEXT("Day1 실제 지출은 900"), ReadEstimate().StaffHireCost, 900);

	// 이후 단가가 달라져도 과거에 낸 900 Gold를 1,400으로 바꾸지 않는다.
	Definition->StaffHireCostGold = 700;
	TestEqual(TEXT("단가 변경은 오늘의 기존 지출을 바꾸지 않는다"), ReadEstimate().StaffHireCost, 900);

	const int32 GoldBeforeNextDay = Progression->GetGold();
	Progression->BeginNewDay(2);
	TestEqual(TEXT("Day2에도 고용 인원은 남는다"), Progression->GetHiredStaffCount(), 2);
	TestEqual(TEXT("날이 바뀌어도 고용비를 다시 내지 않는다"), Progression->GetGold(), GoldBeforeNextDay);
	TestEqual(TEXT("Day2 추가 고용 전 오늘 고용비는 0"), ReadEstimate().StaffHireCost, 0);
	if (!TestTrue(TEXT("Day2 변경된 단가로 인원 1명 추가 고용"), Progression->TryHireStaff(1)))
	{
		return false;
	}
	TestEqual(TEXT("Day2 고용 인원은 누적 3명"), Progression->GetHiredStaffCount(), 3);
	TestEqual(TEXT("Day2에는 그날의 실제 지출 700만 표시"), ReadEstimate().StaffHireCost, 700);
	TestEqual(TEXT("Day1 원장 지출은 계속 900"),
		Progression->GetEndedDayLedger().GetSpendByReason(EFF_SpendReason::StaffHire), 900);
	return true;
}

// 목표 보너스의 0(못 받음)과 모름은 다른 정보다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepCheckRules_RevenueAndBonus, "FestaFiesta.UI.PrepCheck.RevenueAndBonus", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepCheckRules_RevenueAndBonus::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepCheckRulesTestHelpers;

	// 목표 달성 — 600명 × 5 = 3,000 수입 + 보너스 300.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			0, /*Expected*/ 600, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("입장료 수입 3,000"), E.ExpectedAdmissionRevenue, 3000);
		TestEqual(TEXT("목표를 넘으면 보너스"), E.ExpectedGoalBonus, 300);
	}

	// 경계 — 목표와 같으면 달성이다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			0, /*Expected*/ 500, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("목표와 같으면 달성"), E.ExpectedGoalBonus, 300);
	}

	// 미달 — 0 은 `모름` 이 아니라 "못 받는다"는 확정 정보다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			0, /*Expected*/ 400, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("미달이면 보너스 0"), E.ExpectedGoalBonus, 0);
		TestEqual(TEXT("수입은 그대로 계산된다"), E.ExpectedAdmissionRevenue, 2000);
	}

	// 예상 방문객을 모르면 수입과 보너스를 둘 다 비운다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			0, /*Expected*/ INDEX_NONE, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("수입을 비운다"), E.ExpectedAdmissionRevenue, static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("보너스도 비운다 — 0 으로 두면 못 받는다는 뜻이 된다"),
			E.ExpectedGoalBonus, static_cast<int32>(INDEX_NONE));
	}

	// 목표를 모르면 보너스만 비우고 수입은 계산한다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			0, 600, AdmissionFee, /*Goal*/ INDEX_NONE, GoalBonus);

		TestEqual(TEXT("수입은 계산된다"), E.ExpectedAdmissionRevenue, 3000);
		TestEqual(TEXT("보너스는 비운다"), E.ExpectedGoalBonus, static_cast<int32>(INDEX_NONE));
	}
	return true;
}

// 필수 목록을 재판정하지 않고 소유자의 설치 개수/누락/저작 오류를 그대로 알린다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepCheckRules_RequiredSummary,
	"FestaFiesta.UI.PrepCheck.RequiredSummary",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepCheckRules_RequiredSummary::RunTest(const FString& Parameters)
{
	UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>();
	UFFZSFacilityDefinition* Facility = NewObject<UFFZSFacilityDefinition>(Definition);
	Facility->Spec.FacilityDefinitionId = TEXT("Stage.MainStage");
	Facility->Spec.DisplayName = FText::FromString(TEXT("시험 공연장"));
	Facility->Spec.MaxInstancesPerWorld = 3;
	FFF_RequiredFacility& Requirement = Definition->RequiredSetupFacilities.AddDefaulted_GetRef();
	Requirement.FacilityDefinition = Facility;
	Requirement.MinimumCount = 2;
	FFFZSReadModel Model;
	FFFZSFacilityReadModel& Installed = Model.Facilities.AddDefaulted_GetRef();
	Installed.FacilityDefinitionId = Facility->Spec.FacilityDefinitionId;
	Installed.State = EFFZSFacilityState::Active;
	const FText Format = FText::FromString(TEXT("{Name} · {Installed}/{Required} · {State}"));
	const FText Empty = FText::FromString(TEXT("필수 시설 조건 없음"));
	const auto Display = [&Format, &Empty](const FFF_RequiredFacilitiesStatus& Status)
	{
		return FFF_PrepCheckRules::FormatRequiredFacilities(Status, Format, Empty).ToString();
	};
	FFF_RequiredFacilitiesStatus Status = UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model);
	TestTrue(TEXT("실제 설치 1/2와 미충족 표시"), Display(Status).Contains(TEXT("시험 공연장 · 1/2 · ! 추가 설치 필요")));
	const FFFZSFacilityReadModel SecondInstalled = Installed;
	Model.Facilities.Add(SecondInstalled);
	Status = UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model);
	TestTrue(TEXT("실제 설치 2/2는 체크와 완료 문구"), Display(Status).Contains(TEXT("2/2 · ✓ 설치 완료")));
	Model.Facilities.Last().bNeedsRelocation = true;
	Status = UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model);
	TestTrue(TEXT("이전 완료 상태를 남기지 않고 재배치 필요 시설은 제외"), Display(Status).Contains(TEXT("1/2 · ! 추가 설치 필요")));
	Requirement.MinimumCount = 4;
	Status = UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model);
	TestTrue(TEXT("저작 오류는 설치 완료로 표시하지 않음"), Display(Status).Contains(TEXT("설정 확인 필요")));
	Definition->RequiredSetupFacilities.Reset();
	Status = UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model);
	TestEqual(TEXT("빈 필수 목록에 새 선택조건을 추가하지 않음"), Display(Status), Empty.ToString());
	Status = UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(nullptr, Model);
	TestEqual(TEXT("원본 미확인 사유를 그대로 표시"), Display(Status), Status.BlockingReason.ToString());
	return true;
}

// 같은 차단 enum에서 설치 수가 바뀌어도 열린 패널의 사유가 갱신되어야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepCheckRules_CtaReasonRefresh,
	"FestaFiesta.UI.PrepCheck.CtaReasonRefresh",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepCheckRules_CtaReasonRefresh::RunTest(const FString& Parameters)
{
	UClass* BarClass = LoadClass<UFF_PrepHudBar>(nullptr,
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudBar.WBP_FF_PrepHudBar_C"));
	UClass* PanelClass = LoadClass<UFF_PrepPanelCheck>(nullptr,
		TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels/WBP_FF_PrepPanelCheck.WBP_FF_PrepPanelCheck_C"));
	if (!TestNotNull(TEXT("저장 HUD 클래스"), BarClass) || !TestNotNull(TEXT("저장 점검 클래스"), PanelClass)) { return false; }
	TStrongObjectPtr<UFF_PrepHudBar> Bar(NewObject<UFF_PrepHudBar>(GetTransientPackage(), BarClass));
	TStrongObjectPtr<UFF_PrepPanelCheck> Panel(NewObject<UFF_PrepPanelCheck>(GetTransientPackage(), PanelClass));
	Panel->Text_OpenGateReason = NewObject<UTextBlock>(Panel.Get());
	Panel->Text_OpenGateLabel = NewObject<UTextBlock>(Panel.Get());
	Panel->Button_OpenGate = NewObject<UButton>(Panel.Get());
	Panel->InitializePanel(Bar.Get(), EFF_PrepHudTab::Check, FText::FromString(TEXT("점검")));
	Bar->OnCtaStateChanged.AddUniqueDynamic(Panel.Get(), &UFF_PrepPanelCheck::HandleCtaStateChanged);

	const FText Before = FText::FromString(TEXT("시험 공연장: 설치 0/2"));
	const FText After = FText::FromString(TEXT("시험 공연장: 설치 1/2"));
	Bar->SetCtaState(EFF_PrepCtaState::BlockedStageLocked, Before);
	TestEqual(TEXT("첫 차단 사유"), Panel->Text_OpenGateReason->GetText().ToString(), Before.ToString());
	Bar->SetCtaState(EFF_PrepCtaState::BlockedStageLocked, After);
	TestEqual(TEXT("동일 상태의 사유 변경도 구독자에 전달"), Panel->Text_OpenGateReason->GetText().ToString(), After.ToString());
	TestEqual(TEXT("공개 사유가 화면과 일치"), Bar->GetCtaReason().ToString(), After.ToString());
	TestFalse(TEXT("필수 시설 미충족에서는 시작 비활성"), Panel->Button_OpenGate->GetIsEnabled());
	Bar->SetCtaState(EFF_PrepCtaState::Ready, FText::GetEmpty());
	TestTrue(TEXT("소유 판정이 Ready면 선택사항을 추가로 요구하지 않음"), Panel->Button_OpenGate->GetIsEnabled());
	TestEqual(TEXT("Ready 안내는 편집값 사용"), Panel->Text_OpenGateReason->GetText().ToString(), Panel->OpenGateReadyText.ToString());
	TestTrue(TEXT("Setup 시작 문구 유지"), Panel->Text_OpenGateLabel->GetText().ToString().Contains(TEXT("시작")));
	Bar->SetCtaState(EFF_PrepCtaState::Hidden, FText::GetEmpty());
	TestEqual(TEXT("진행 액션이 없는 단계는 숨김"), Panel->Button_OpenGate->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("숨긴 단계는 이전 사유 제거"), Panel->Text_OpenGateReason->GetText().IsEmpty());
	Bar->OnCtaStateChanged.RemoveDynamic(Panel.Get(), &UFF_PrepPanelCheck::HandleCtaStateChanged);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
