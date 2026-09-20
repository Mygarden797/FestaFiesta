// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/PrepHUD/Panels/FF_PrepPanelCheck.h"

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
 *   ③ 임금을 모를 때 0 으로 바꾸지 않는 것
 *   ④ 목표 보너스의 `0`(못 받음)과 `모름`이 구분되는 것
 */
namespace FFF_PrepCheckRulesTestHelpers
{
	/** 첫 축제 기준값. 규칙 자체는 이 숫자를 모른다 — 전부 인자로 들어간다. */
	constexpr int32 WagePerStaff = 150;
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

// 오늘 나가는 돈은 알바 임금 하나뿐이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepCheckRules_StaffWageCost, "FestaFiesta.UI.PrepCheck.StaffWageCost", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepCheckRules_StaffWageCost::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepCheckRulesTestHelpers;

	// 알바 3명 — 450.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			/*TempStaff*/ 3, WagePerStaff,
			INDEX_NONE, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("알바 임금 450"), E.StaffWageCost, 450);
	}

	// 알바 0명 — 임금 0 은 확정 정보다. `모름` 으로 바뀌면 안 된다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			/*TempStaff*/ 0, WagePerStaff,
			INDEX_NONE, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("알바 0명이면 임금 0"), E.StaffWageCost, 0);
	}

	// 임금을 모르면 비운다. 0 으로 바꾸면 "오늘 나갈 돈이 없다"는 확정 사실이 된다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			/*TempStaff*/ 3, /*Wage*/ INDEX_NONE,
			INDEX_NONE, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("임금은 비운다"), E.StaffWageCost, static_cast<int32>(INDEX_NONE));
	}

	// 인원을 모르면(축제 시작 전) 임금도 비운다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			/*TempStaff*/ INDEX_NONE, WagePerStaff,
			INDEX_NONE, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("인원을 모르면 임금도 모른다"), E.StaffWageCost, static_cast<int32>(INDEX_NONE));
	}
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
			0, WagePerStaff, /*Expected*/ 600, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("입장료 수입 3,000"), E.ExpectedAdmissionRevenue, 3000);
		TestEqual(TEXT("목표를 넘으면 보너스"), E.ExpectedGoalBonus, 300);
	}

	// 경계 — 목표와 같으면 달성이다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			0, WagePerStaff, /*Expected*/ 500, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("목표와 같으면 달성"), E.ExpectedGoalBonus, 300);
	}

	// 미달 — 0 은 `모름` 이 아니라 "못 받는다"는 확정 정보다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			0, WagePerStaff, /*Expected*/ 400, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("미달이면 보너스 0"), E.ExpectedGoalBonus, 0);
		TestEqual(TEXT("수입은 그대로 계산된다"), E.ExpectedAdmissionRevenue, 2000);
	}

	// 예상 방문객을 모르면 수입과 보너스를 둘 다 비운다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			0, WagePerStaff, /*Expected*/ INDEX_NONE, AdmissionFee, DailyGoal, GoalBonus);

		TestEqual(TEXT("수입을 비운다"), E.ExpectedAdmissionRevenue, static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("보너스도 비운다 — 0 으로 두면 못 받는다는 뜻이 된다"),
			E.ExpectedGoalBonus, static_cast<int32>(INDEX_NONE));
	}

	// 목표를 모르면 보너스만 비우고 수입은 계산한다.
	{
		const FFF_PrepDayEstimate E = FFF_PrepCheckRules::BuildDayEstimate(
			0, WagePerStaff, 600, AdmissionFee, /*Goal*/ INDEX_NONE, GoalBonus);

		TestEqual(TEXT("수입은 계산된다"), E.ExpectedAdmissionRevenue, 3000);
		TestEqual(TEXT("보너스는 비운다"), E.ExpectedGoalBonus, static_cast<int32>(INDEX_NONE));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
