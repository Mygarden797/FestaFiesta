// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"

/**
 * 하루 경계와 알바 고용 검증 (S6). economy_spec.md §5.1 S6 · §3.7 · §2.5.1.
 *
 * **월드를 만들지 않는다.** UFF_FestivalProgressionSubsystem 은 GameInstanceSubsystem 이지만
 * 정산 경로에서 GetGameInstance() 를 쓰지 않으므로 GameInstance 를 Outer 로 NewObject 하면 된다.
 * Initialize() 는 부르지 않는다 — 프로젝트 설정의 실제 축제를 로드해 StartFestival 을 부르기
 * 때문에 테스트가 저작 상태에 묶인다.
 *
 * 이 파일이 Stage 서브시스템을 세우지 않는 것이 브리지를 분리한 이유다. "언제 부르는가"는
 * UFF_StageEconomyBridgeSubsystem 이 갖고, "무엇이 일어나는가"는 여기서 고정한다.
 */
namespace FFF_DayBoundaryTestHelpers
{
	// 2026-08-14 결정값 (E-22 · E-23). 하루 고정비 = 임대료 2,400 (임금은 고정비가 아니다).
	constexpr int32 DailyRent = 2400;
	constexpr int32 WagePerStaff = 150;
	constexpr int32 BaseStaff = 6;
	constexpr int32 MaxHires = 6;

	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	struct FDayBoundaryHarness
	{
		bool Initialize(int32 InitialGold)
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			if (GameInstance == nullptr)
			{
				return false;
			}

			Progression = NewObject<UFF_FestivalProgressionSubsystem>(GameInstance);
			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			CostTable = NewObject<UDataTable>(GameInstance);
			if (Progression == nullptr || Definition == nullptr || CostTable == nullptr)
			{
				return false;
			}

			// 이 테스트는 시설을 짓지 않지만, 표가 비어 있으면 IsDefinitionValid 가 Error 를 찍고
			// **UE 자동화 프레임워크는 Error 로그 자체를 테스트 실패로 취급한다.** 행 하나로 채운다.
			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
			Definition->FacilityCostTable = CostTable;

			Definition->FestivalId = TEXT("DayBoundaryAutomationFestival");
			Definition->InitialGold = InitialGold;
			Definition->DailyVenueRentGold = DailyRent;
			Definition->DailyStaffWageGold = WagePerStaff;
			Definition->InitialStaffCount = BaseStaff;
			Definition->MaxHiredStaffPerDay = MaxHires;

			// StartFestival 이 Day1 을 정산한다. Day1 은 Setup 으로 시작해 MorningCheck 을
			// 통과하지 않으므로 브리지가 아니라 여기가 Day1 의 트리거다.
			Progression->StartFestival(Definition);
			return true;
		}

		// Σ지출. 정합성 불변식(초기Gold + Σ매출 − Σ지출 == Gold) 검산용.
		// 미납은 아직 안 나간 돈이라 여기 들어가지 않는다.
		int32 CalcLedgerSpendTotal() const
		{
			const FFF_DayLedger& Ledger = Progression->GetDayLedger();
			int32 Total = 0;
			for (int32 Index = 0;
				Index <= static_cast<int32>(EFF_SpendReason::FinaleFireworks);
				++Index)
			{
				Total += Ledger.GetSpendByReason(static_cast<EFF_SpendReason>(Index));
			}
			return Total;
		}

		// Σ당일매출. TodayRevenue 와 같아야 한다 (중복 값의 불변식).
		int32 CalcLedgerEarnTotal() const
		{
			const FFF_DayLedger& Ledger = Progression->GetDayLedger();
			int32 Total = 0;
			for (int32 Index = 0;
				Index <= static_cast<int32>(EFF_EarnReason::MinigameReward);
				++Index)
			{
				Total += Ledger.GetEarnByReason(static_cast<EFF_EarnReason>(Index));
			}
			return Total;
		}

		UGameInstance* GameInstance = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
	};
}

// 임대료가 **날마다 정확히 1회** 징수된다. 같은 날을 두 번 열어도 두 번 빠지지 않는다.
// RequestTransition 에 같은 단계 가드가 없어서 MorningCheck 재진입이 실제로 가능하고,
// 이중 징수는 크래시도 로그도 없이 3일 수지(§2.5)만 조용히 깨뜨린다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_RentSettlesExactlyOncePerDay, "FestaFiesta.Economy.DayBoundary.RentSettlesExactlyOncePerDay", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_RentSettlesExactlyOncePerDay::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	// StartFestival 이 Day1 을 정산했다. 임금은 고정비가 아니므로 임대료만 나간다.
	TestEqual(TEXT("Day1 이 축제 시작과 함께 정산된다"),
		Harness.Progression->GetGold(), 10000 - FFF_DayBoundaryTestHelpers::DailyRent);
	TestEqual(TEXT("마지막 정산일이 1 이다"), Harness.Progression->GetLastProcessedDay(), 1);

	// 같은 날을 다시 열어도 아무 일도 없어야 한다. 이 줄이 "정확히 1회"를 고정한다.
	Harness.Progression->BeginNewDay(1);
	TestEqual(TEXT("Day1 을 다시 열어도 임대료가 두 번 빠지지 않는다"),
		Harness.Progression->GetGold(), 10000 - FFF_DayBoundaryTestHelpers::DailyRent);

	// 지난 날로 되돌리는 호출도 무시된다.
	Harness.Progression->BeginNewDay(0);
	TestEqual(TEXT("지난 날 호출은 무시된다"),
		Harness.Progression->GetGold(), 10000 - FFF_DayBoundaryTestHelpers::DailyRent);
	TestEqual(TEXT("마지막 정산일이 되돌아가지 않는다"),
		Harness.Progression->GetLastProcessedDay(), 1);

	Harness.Progression->BeginNewDay(2);
	TestEqual(TEXT("Day2 가 정산된다"), Harness.Progression->GetGold(),
		10000 - FFF_DayBoundaryTestHelpers::DailyRent * 2);
	Harness.Progression->BeginNewDay(2);
	TestEqual(TEXT("Day2 재진입도 무시된다"), Harness.Progression->GetGold(),
		10000 - FFF_DayBoundaryTestHelpers::DailyRent * 2);

	// 3일 임대료 합계가 7,200 이어야 한다 — §2.6 입장료 7,500 이 이를 덮는다.
	Harness.Progression->BeginNewDay(3);
	TestEqual(TEXT("3일 임대료 합계가 7,200 이다"),
		10000 - Harness.Progression->GetGold(), 7200);
	TestEqual(TEXT("미납 없이 전액 정산됐다"), Harness.Progression->GetTotalArrears(), 0);

	return true;
}

// 하루 경계는 TodayRevenue·DayLedger·알바 인원만 리셋한다.
// 누적 매출·잔액·미납·기본 Staff 는 날을 넘어간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_ResetsOnlyDailyScopedValues, "FestaFiesta.Economy.DayBoundary.ResetsOnlyDailyScopedValues", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_ResetsOnlyDailyScopedValues::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	Harness.Progression->AddRevenue(1200, EFF_EarnReason::AdmissionFee);
	TestTrue(TEXT("알바를 4명 고용한다"), Harness.Progression->TryHireTemporaryStaff(4));

	TestEqual(TEXT("오늘 매출이 쌓인다"), Harness.Progression->GetTodayRevenue(), 1200);
	TestEqual(TEXT("알바가 고용됐다"), Harness.Progression->GetTemporaryStaffCount(), 4);

	const int32 GoldBeforeNewDay = Harness.Progression->GetGold();

	Harness.Progression->BeginNewDay(2);

	// --- 리셋되는 것 ---
	TestEqual(TEXT("오늘 매출은 리셋된다"), Harness.Progression->GetTodayRevenue(), 0);
	TestEqual(TEXT("당일 원장의 어제 매출은 지워진다"),
		Harness.Progression->GetDayLedger().GetEarnByReason(EFF_EarnReason::AdmissionFee), 0);
	// 알바는 일당 계약이라 매일 0 으로 돌아간다.
	TestEqual(TEXT("알바 인원은 0 으로 리셋된다"),
		Harness.Progression->GetTemporaryStaffCount(), 0);

	// --- 유지되는 것 ---
	TestEqual(TEXT("누적 매출은 유지된다"), Harness.Progression->GetTotalRevenue(), 1200);
	// 기본 팀이 리셋되면 잔액 0 에서 인원이 0 이 되어 회복이 불가능해진다 (§2.5.1).
	TestEqual(TEXT("기본 Staff 는 유지된다"), Harness.Progression->GetBaseStaffCount(),
		FFF_DayBoundaryTestHelpers::BaseStaff);
	TestEqual(TEXT("전체 인원은 기본 팀만 남는다"),
		Harness.Progression->GetTotalStaffCount(), FFF_DayBoundaryTestHelpers::BaseStaff);
	// 잔액은 리셋되지 않는다 — 어제 번 돈으로 오늘을 산다.
	TestEqual(TEXT("잔액은 어제 값에서 오늘 임대료만 빠진다"),
		Harness.Progression->GetGold(), GoldBeforeNewDay - FFF_DayBoundaryTestHelpers::DailyRent);

	return true;
}

// 자동 고용이 없다. BeginNewDay 는 알바를 **한 명도 뽑지 않는다.**
// 여기서 최대로 고용하면 "알바 쓸까 Finale 살까"(§2.5.1)를 게임이 대신 결정한 것이 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_DoesNotAutoHire, "FestaFiesta.Economy.DayBoundary.DoesNotAutoHire", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_DoesNotAutoHire::RunTest(const FString& Parameters)
{
	// 돈이 넉넉해도 자동으로 고용하지 않아야 한다.
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(100'000)))
	{
		return false;
	}

	TestEqual(TEXT("축제 시작 직후 알바가 0명이다"),
		Harness.Progression->GetTemporaryStaffCount(), 0);
	TestEqual(TEXT("임금이 한 푼도 안 나갔다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::StaffWages), 0);
	TestEqual(TEXT("잔액에서 임대료만 빠졌다"),
		Harness.Progression->GetGold(), 100'000 - FFF_DayBoundaryTestHelpers::DailyRent);

	Harness.Progression->BeginNewDay(2);
	TestEqual(TEXT("새 날에도 자동 고용하지 않는다"),
		Harness.Progression->GetTemporaryStaffCount(), 0);
	TestEqual(TEXT("새 날에도 임금이 안 나간다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::StaffWages), 0);

	// 고용은 플레이어가 부를 때만 일어난다.
	TestEqual(TEXT("살 수 있는 인원은 상한까지다"),
		Harness.Progression->GetAffordableTemporaryStaffCount(), FFF_DayBoundaryTestHelpers::MaxHires);

	return true;
}

// 잔액이 모자라면 임대료는 미납이 되고, 그래도 기본 Staff 는 일한다.
// 임금은 절대 미납이 되지 않는다 (§3.7 의 비대칭).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_RentBecomesArrearsButBaseStaffSurvives, "FestaFiesta.Economy.DayBoundary.RentBecomesArrearsButBaseStaffSurvives", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_RentBecomesArrearsButBaseStaffSurvives::RunTest(const FString& Parameters)
{
	// 임대료 2,400 인데 1,000 뿐이다.
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(1000)))
	{
		return false;
	}

	TestEqual(TEXT("낼 수 있는 만큼만 임대료로 나간다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::VenueRent), 1000);
	TestEqual(TEXT("부족분 1,400 이 미납이 된다"), Harness.Progression->GetTotalArrears(), 1400);
	TestEqual(TEXT("잔액은 음수가 되지 않는다"), Harness.Progression->GetGold(), 0);

	// 자금난의 바닥 — 잔액 0 이어도 기본 팀은 일한다.
	TestEqual(TEXT("잔액 0 이어도 기본 Staff 는 일한다"),
		Harness.Progression->GetTotalStaffCount(), FFF_DayBoundaryTestHelpers::BaseStaff);
	TestEqual(TEXT("알바는 고용되지 않는다"),
		Harness.Progression->GetTemporaryStaffCount(), 0);
	TestEqual(TEXT("살 수 있는 알바가 0명이다"),
		Harness.Progression->GetAffordableTemporaryStaffCount(), 0);

	// 임금은 미납에 섞이지 않는다 — 미납 총액은 임대료 부족분 그대로다.
	TestEqual(TEXT("임금 지출은 0 이다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::StaffWages), 0);
	TestEqual(TEXT("임금이 미납에 섞이지 않는다"), Harness.Progression->GetTotalArrears(), 1400);

	return true;
}

// 미납이 있으면 들어온 돈이 잔액에 남지 않고 **먼저 임대료 미납을 갚는다.**
// 상환은 임대료 지출로 기록되어 정합성 불변식이 유지된다 (§5.1).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_RevenueRepaysArrearsFirst, "FestaFiesta.Economy.DayBoundary.RevenueRepaysArrearsFirst", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_RevenueRepaysArrearsFirst::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(1000)))
	{
		return false;
	}

	TestEqual(TEXT("미납 1,400 으로 시작한다"), Harness.Progression->GetTotalArrears(), 1400);
	TestEqual(TEXT("잔액 0 으로 시작한다"), Harness.Progression->GetGold(), 0);

	// 매출 300 -> 전액이 상환에 쓰인다. 잔액은 0 에 붙어 있다.
	Harness.Progression->AddRevenue(300, EFF_EarnReason::AdmissionFee);
	TestEqual(TEXT("매출이 미납을 갚아 잔액이 오르지 않는다"), Harness.Progression->GetGold(), 0);
	TestEqual(TEXT("미납이 300 줄어든다"), Harness.Progression->GetTotalArrears(), 1100);
	// 매출은 상환과 무관하게 전액 잡힌다 — 매출은 "번 돈"이다.
	TestEqual(TEXT("누적 매출은 전액 잡힌다"), Harness.Progression->GetTotalRevenue(), 300);
	TestEqual(TEXT("오늘 매출도 전액 잡힌다"), Harness.Progression->GetTodayRevenue(), 300);

	// 환급도 같은 규칙을 받는다 — 밀린 채로 시설을 팔아 현금을 쥐는 경로를 막는다.
	Harness.Progression->Refund(100);
	TestEqual(TEXT("환급도 미납 상환에 쓰인다"), Harness.Progression->GetGold(), 0);
	TestEqual(TEXT("환급만큼 미납이 줄어든다"), Harness.Progression->GetTotalArrears(), 1000);

	// 미납을 다 갚고 남은 돈은 잔액이 된다.
	Harness.Progression->AddRevenue(1300, EFF_EarnReason::FacilityCommission, TEXT("Booth_FoodStall"));
	TestEqual(TEXT("미납을 다 갚으면 나머지가 잔액이 된다"), Harness.Progression->GetGold(), 300);
	TestEqual(TEXT("미납이 사라진다"), Harness.Progression->GetTotalArrears(), 0);

	// 상환분까지 합쳐 임대료 지출이 하루치 2,400 이 된다 — 늦게 나갔을 뿐 같은 임대료다.
	TestEqual(TEXT("임대료 지출이 하루치로 채워진다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::VenueRent),
		FFF_DayBoundaryTestHelpers::DailyRent);

	// 정합성 불변식: 초기Gold + Σ매출 − Σ지출 == Gold. 환급은 매출이 아니라 별도다.
	const int32 Refunded = Harness.Progression->GetDayLedger().GetTotalRefund();
	TestEqual(TEXT("초기Gold + Σ매출 + 환급 − Σ지출 == Gold"),
		1000 + Harness.Progression->GetTotalRevenue() + Refunded - Harness.CalcLedgerSpendTotal(),
		Harness.Progression->GetGold());

	// 미납이 없으면 매출이 그대로 잔액에 쌓인다.
	Harness.Progression->AddRevenue(100, EFF_EarnReason::AdmissionFee);
	TestEqual(TEXT("미납이 없으면 매출이 잔액을 늘린다"), Harness.Progression->GetGold(), 400);

	return true;
}

// TodayRevenue 와 원장의 원인별 매출 합이 항상 같아야 한다.
// 두 값이 중복이라 한쪽만 갱신하는 코드가 들어가면 상단바와 Report 가 다른 숫자를 보여준다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_TodayRevenueMatchesLedgerSum, "FestaFiesta.Economy.DayBoundary.TodayRevenueMatchesLedgerSum", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_TodayRevenueMatchesLedgerSum::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	Harness.Progression->AddRevenue(500, EFF_EarnReason::AdmissionFee);
	Harness.Progression->AddRevenue(300, EFF_EarnReason::FacilityCommission, TEXT("Booth_FoodStall"));
	Harness.Progression->AddRevenue(150, EFF_EarnReason::MinigameReward);

	TestEqual(TEXT("TodayRevenue == Σ원인별 매출"),
		Harness.Progression->GetTodayRevenue(), Harness.CalcLedgerEarnTotal());
	TestEqual(TEXT("합이 실제로 950 이다"), Harness.Progression->GetTodayRevenue(), 950);

	// 하루 경계 뒤에도 둘이 함께 0 이 되어야 한다 — 리셋이 짝을 맞추는지 확인한다.
	Harness.Progression->BeginNewDay(2);
	TestEqual(TEXT("리셋 후 TodayRevenue 가 0"), Harness.Progression->GetTodayRevenue(), 0);
	TestEqual(TEXT("리셋 후 원장 합도 0"), Harness.CalcLedgerEarnTotal(), 0);
	TestEqual(TEXT("리셋 후에도 둘이 같다"),
		Harness.Progression->GetTodayRevenue(), Harness.CalcLedgerEarnTotal());

	return true;
}

// 알바 고용은 **부분 고용하지 않는다.** 상한 초과·잔액 부족은 전체를 거부하고 상태를 안 바꾼다.
// 고용은 플레이어가 사는 것이라 §3.7 의 "거절 가능한 지출"이고 시설 구매와 같은 규칙이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_HireRejectsInsteadOfPartialHiring, "FestaFiesta.Economy.DayBoundary.HireRejectsInsteadOfPartialHiring", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_HireRejectsInsteadOfPartialHiring::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	const int32 GoldAfterRent = Harness.Progression->GetGold();

	// 상한 초과는 거부되고 상태가 그대로다.
	TestFalse(TEXT("상한을 넘는 요청은 거부된다"),
		Harness.Progression->TryHireTemporaryStaff(FFF_DayBoundaryTestHelpers::MaxHires + 1));
	TestEqual(TEXT("거부되면 아무도 고용되지 않는다"),
		Harness.Progression->GetTemporaryStaffCount(), 0);
	TestEqual(TEXT("거부되면 잔액이 그대로다"), Harness.Progression->GetGold(), GoldAfterRent);

	// 정상 고용.
	TestTrue(TEXT("상한과 같은 인원은 고용된다"),
		Harness.Progression->TryHireTemporaryStaff(FFF_DayBoundaryTestHelpers::MaxHires));
	TestEqual(TEXT("고용 인원이 반영된다"),
		Harness.Progression->GetTemporaryStaffCount(), FFF_DayBoundaryTestHelpers::MaxHires);
	TestEqual(TEXT("임금이 인원분만큼 빠진다"),
		Harness.Progression->GetGold(), GoldAfterRent
			- FFF_DayBoundaryTestHelpers::MaxHires * FFF_DayBoundaryTestHelpers::WagePerStaff);
	TestEqual(TEXT("임금이 원장에 기록된다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::StaffWages),
		FFF_DayBoundaryTestHelpers::MaxHires * FFF_DayBoundaryTestHelpers::WagePerStaff);
	TestEqual(TEXT("전체 인원은 기본 + 알바다"),
		Harness.Progression->GetTotalStaffCount(),
		FFF_DayBoundaryTestHelpers::BaseStaff + FFF_DayBoundaryTestHelpers::MaxHires);

	// 상한을 다 썼으면 1명도 더 안 된다. 누적으로 상한을 본다.
	TestEqual(TEXT("남은 고용 여력이 0 이다"),
		Harness.Progression->GetAffordableTemporaryStaffCount(), 0);
	TestFalse(TEXT("상한을 다 쓰면 추가 고용이 거부된다"),
		Harness.Progression->TryHireTemporaryStaff(1));
	TestEqual(TEXT("거부 후에도 인원이 그대로다"),
		Harness.Progression->GetTemporaryStaffCount(), FFF_DayBoundaryTestHelpers::MaxHires);

	// 0 이하 요청은 거부된다 (고용 명령으로 돈을 돌려받는 경로를 만들지 않는다).
	TestFalse(TEXT("0명 요청은 거부된다"), Harness.Progression->TryHireTemporaryStaff(0));
	TestFalse(TEXT("음수 요청은 거부된다"), Harness.Progression->TryHireTemporaryStaff(-3));

	return true;
}

// 잔액이 모자라면 고용이 거부되고 돈이 한 푼도 안 빠진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_HireRejectedWhenGoldIsShort, "FestaFiesta.Economy.DayBoundary.HireRejectedWhenGoldIsShort", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_HireRejectedWhenGoldIsShort::RunTest(const FString& Parameters)
{
	// 임대료 2,400 을 내고 400 이 남는다 -> 2명분(300)만 살 수 있다.
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(2800)))
	{
		return false;
	}

	TestEqual(TEXT("임대료 뒤 400 이 남는다"), Harness.Progression->GetGold(), 400);
	TestEqual(TEXT("살 수 있는 인원은 2명이다"),
		Harness.Progression->GetAffordableTemporaryStaffCount(), 2);

	// 3명은 450 이라 못 산다. **부분 고용하지 않고 전체를 거부한다.**
	TestFalse(TEXT("잔액을 넘는 요청은 거부된다"),
		Harness.Progression->TryHireTemporaryStaff(3));
	TestEqual(TEXT("거부되면 아무도 고용되지 않는다"),
		Harness.Progression->GetTemporaryStaffCount(), 0);
	TestEqual(TEXT("거부되면 잔액이 그대로다"), Harness.Progression->GetGold(), 400);
	TestEqual(TEXT("거부되면 임금 기록도 없다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::StaffWages), 0);

	// 살 수 있는 만큼은 성공한다.
	TestTrue(TEXT("2명은 고용된다"), Harness.Progression->TryHireTemporaryStaff(2));
	TestEqual(TEXT("2명분 임금만 빠진다"), Harness.Progression->GetGold(), 100);
	TestEqual(TEXT("전체 인원은 기본 + 2명"),
		Harness.Progression->GetTotalStaffCount(), FFF_DayBoundaryTestHelpers::BaseStaff + 2);

	return true;
}

// 축제가 시작되지 않았으면 하루 경계와 고용이 아무 일도 하지 않는다.
// "설정이 안 됐다"와 "돈이 없다"는 다른 상태다 (StartFestival 주석과 같은 원칙).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_IgnoredBeforeFestivalStarts, "FestaFiesta.Economy.DayBoundary.IgnoredBeforeFestivalStarts", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_IgnoredBeforeFestivalStarts::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UFF_FestivalProgressionSubsystem* Progression =
		NewObject<UFF_FestivalProgressionSubsystem>(GameInstance);

	if (!TestNotNull(TEXT("Progression 이 만들어진다"), Progression))
	{
		return false;
	}

	Progression->BeginNewDay(1);

	TestEqual(TEXT("잔액이 0 으로 남는다"), Progression->GetGold(), 0);
	TestEqual(TEXT("정산되지 않았다"), Progression->GetLastProcessedDay(), 0);
	TestEqual(TEXT("미납도 생기지 않는다"), Progression->GetTotalArrears(), 0);

	TestFalse(TEXT("고용도 거부된다"), Progression->TryHireTemporaryStaff(1));
	TestEqual(TEXT("살 수 있는 인원도 0 이다"),
		Progression->GetAffordableTemporaryStaffCount(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
