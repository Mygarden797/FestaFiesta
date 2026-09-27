// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"

/**
 * 하루 경계와 인원 고용 검증 (S6). economy_spec.md §5.1 S6 · §3.7 · §2.5.1.
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
	constexpr int32 HireCost = 450;
	constexpr int32 BaseStaff = 6;
	constexpr int32 MaxHires = 6;

	// 라이브 원장과 스냅샷 원장 둘 다에 써야 해서 원장을 인자로 받는다.
	// (스냅샷 테스트가 생기기 전에는 Progression 의 라이브 원장 전용이었다)
	inline int32 SumSpend(const FFF_DayLedger& Ledger)
	{
		int32 Total = 0;
		for (int32 Index = 0;
			Index <= static_cast<int32>(EFF_SpendReason::FacilityUpgrade);
			++Index)
		{
			Total += Ledger.GetSpendByReason(static_cast<EFF_SpendReason>(Index));
		}
		return Total;
	}

	inline int32 SumEarn(const FFF_DayLedger& Ledger)
	{
		int32 Total = 0;
		for (int32 Index = 0;
			Index <= static_cast<int32>(EFF_EarnReason::MinigameReward);
			++Index)
		{
			Total += Ledger.GetEarnByReason(static_cast<EFF_EarnReason>(Index));
		}
		return Total;
	}

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
			Definition->StaffHireCostGold = HireCost;
			Definition->InitialStaffCount = BaseStaff;
			Definition->MaxHiredStaffCount = MaxHires;

			// StartFestival 이 Day1 을 정산한다. Day1 은 Setup 으로 시작해 MorningCheck 을
			// 통과하지 않으므로 브리지가 아니라 여기가 Day1 의 트리거다.
			Progression->StartFestival(Definition);
			return true;
		}

		// Σ지출. 정합성 불변식(초기Gold + Σ매출 − Σ지출 == Gold) 검산용.
		int32 CalcLedgerSpendTotal() const
		{
			return SumSpend(Progression->GetDayLedger());
		}

		// Σ당일매출. TodayRevenue 와 같아야 한다 (중복 값의 불변식).
		int32 CalcLedgerEarnTotal() const
		{
			return SumEarn(Progression->GetDayLedger());
		}

		UGameInstance* GameInstance = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
	};
}

// 하루를 여는 것은 **날마다 정확히 1회**다. 같은 날을 두 번 열어도 두 번 열리지 않는다.
// RequestTransition 에 같은 단계 가드가 없어서 MorningCheck 재진입이 실제로 가능하고,
// 두 번 열리면 장중에 그날 원장이 지워진다 — 크래시도 로그도 없이 결산만 조용히 빈다.
//
// 경계 자체는 아무것도 걷지 않는다. 임대료를 없앤 뒤로 잔액을 줄이는 경로는 구매뿐이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_OpensExactlyOncePerDay, "FestaFiesta.Economy.DayBoundary.OpensExactlyOncePerDay", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_OpensExactlyOncePerDay::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	// StartFestival 이 Day1 을 열었다. 여는 것만으로는 한 푼도 안 나간다.
	TestEqual(TEXT("Day1 을 열어도 잔액이 초기 자금 그대로다"),
		Harness.Progression->GetGold(), 10000);
	TestEqual(TEXT("마지막 정산일이 1 이다"), Harness.Progression->GetLastProcessedDay(), 1);

	// 오늘 매출을 만들어 두면 "하루가 다시 열렸는가"를 잔액이 아니라 이 값으로 볼 수 있다.
	Harness.Progression->AddRevenue(900, EFF_EarnReason::AdmissionFee);

	// 같은 날을 다시 열어도 아무 일도 없어야 한다. 이 줄이 "정확히 1회"를 고정한다.
	Harness.Progression->BeginNewDay(1);
	TestEqual(TEXT("Day1 을 다시 열어도 그날 매출이 안 지워진다"),
		Harness.Progression->GetTodayRevenue(), 900);

	// 지난 날로 되돌리는 호출도 무시된다.
	Harness.Progression->BeginNewDay(0);
	TestEqual(TEXT("지난 날 호출은 무시된다"),
		Harness.Progression->GetTodayRevenue(), 900);
	TestEqual(TEXT("마지막 정산일이 되돌아가지 않는다"),
		Harness.Progression->GetLastProcessedDay(), 1);

	// 다음 날은 실제로 열린다 — 그날 매출이 0 으로 돌아간다.
	Harness.Progression->BeginNewDay(2);
	TestEqual(TEXT("Day2 가 열리면 그날 매출이 0 이 된다"),
		Harness.Progression->GetTodayRevenue(), 0);
	TestEqual(TEXT("마지막 정산일이 2 가 된다"), Harness.Progression->GetLastProcessedDay(), 2);

	// 사흘을 다 열어도 잔액은 초기 자금 그대로다. 누적 매출은 경계를 넘어 살아남는다.
	Harness.Progression->BeginNewDay(3);
	TestEqual(TEXT("사흘을 다 열어도 잔액이 안 줄어든다"),
		Harness.Progression->GetGold(), 10000 + 900);
	TestEqual(TEXT("경계가 만든 지출 줄이 없다"),
		FFF_DayBoundaryTestHelpers::SumSpend(Harness.Progression->GetDayLedger()), 0);

	return true;
}

// 하루 경계는 TodayRevenue·DayLedger만 리셋한다.
// 누적 매출·잔액·기본 Staff·고용 인원은 날을 넘어간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_ResetsOnlyDailyScopedValues, "FestaFiesta.Economy.DayBoundary.ResetsOnlyDailyScopedValues", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_ResetsOnlyDailyScopedValues::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	Harness.Progression->AddRevenue(1200, EFF_EarnReason::AdmissionFee);
	TestTrue(TEXT("인원을 4명 고용한다"), Harness.Progression->TryHireStaff(4));

	TestEqual(TEXT("오늘 매출이 쌓인다"), Harness.Progression->GetTodayRevenue(), 1200);
	TestEqual(TEXT("인원이 고용됐다"), Harness.Progression->GetHiredStaffCount(), 4);

	const int32 GoldBeforeNewDay = Harness.Progression->GetGold();

	Harness.Progression->BeginNewDay(2);

	// --- 리셋되는 것 ---
	TestEqual(TEXT("오늘 매출은 리셋된다"), Harness.Progression->GetTodayRevenue(), 0);
	TestEqual(TEXT("당일 원장의 어제 매출은 지워진다"),
		Harness.Progression->GetDayLedger().GetEarnByReason(EFF_EarnReason::AdmissionFee), 0);

	// --- 유지되는 것 ---
	TestEqual(TEXT("누적 매출은 유지된다"), Harness.Progression->GetTotalRevenue(), 1200);
	// ★ 고용은 1회 구매라 하루 경계를 넘어 남는다. 예전 일당 계약과 정반대다.
	TestEqual(TEXT("고용 인원은 다음 날에도 남는다"),
		Harness.Progression->GetHiredStaffCount(), 4);
	// 기본 팀이 리셋되면 잔액 0 에서 인원이 0 이 되어 회복이 불가능해진다 (§2.5.1).
	TestEqual(TEXT("기본 Staff 는 유지된다"), Harness.Progression->GetBaseStaffCount(),
		FFF_DayBoundaryTestHelpers::BaseStaff);
	TestEqual(TEXT("전체 인원은 기본 + 고용 그대로다"),
		Harness.Progression->GetTotalStaffCount(),
		FFF_DayBoundaryTestHelpers::BaseStaff + 4);
	// 잔액은 리셋되지 않는다 — 어제 번 돈으로 오늘을 산다.
	TestEqual(TEXT("잔액은 어제 값 그대로다 — 경계가 걷는 돈이 없다"),
		Harness.Progression->GetGold(), GoldBeforeNewDay);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_DayBoundary_PermanentHireCapPersistsAcrossDays,
	"FestaFiesta.Economy.DayBoundary.PermanentHireCapPersistsAcrossDays",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_DayBoundary_PermanentHireCapPersistsAcrossDays::RunTest(const FString& Parameters)
{
	using namespace FFF_DayBoundaryTestHelpers;
	(void)Parameters;
	FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("영구 고용 상한 픽스처가 구성된다"), Harness.Initialize(10000))) return false;

	if (!TestTrue(TEXT("첫날 4명을 고용한다"), Harness.Progression->TryHireStaff(4))) return false;
	const int32 GoldAfterFirstHire = 10000 - 4 * HireCost;
	TestEqual(TEXT("첫날 고용비는 한 번만 지불한다"), Harness.Progression->GetGold(), GoldAfterFirstHire);
	Harness.Progression->BeginNewDay(2);
	TestEqual(TEXT("둘째 날에도 첫날 고용 인원이 남는다"), Harness.Progression->GetHiredStaffCount(), 4);
	TestEqual(TEXT("둘째 날 고용 상한은 2명만 남는다"), Harness.Progression->GetRemainingHireCap(), MaxHires - 4);
	TestEqual(TEXT("돈이 충분해도 남은 총원 상한만 고용할 수 있다"),
		Harness.Progression->GetAffordableHireCount(), MaxHires - 4);
	TestEqual(TEXT("둘째 날 기존 인원 비용을 다시 내지 않는다"),
		Harness.Progression->GetGold(), GoldAfterFirstHire);
	TestEqual(TEXT("둘째 날 자동 지출이 없다"), Harness.CalcLedgerSpendTotal(), 0);
	TestFalse(TEXT("날짜가 바뀌어도 총원 상한을 넘기는 3명 고용은 거절된다"),
		Harness.Progression->TryHireStaff(3));
	TestEqual(TEXT("거절 후에도 Gold가 보존된다"), Harness.Progression->GetGold(), GoldAfterFirstHire);
	TestEqual(TEXT("거절 후에도 인원이 보존된다"), Harness.Progression->GetHiredStaffCount(), 4);
	TestEqual(TEXT("거절한 고용은 지출에 기록되지 않는다"), Harness.CalcLedgerSpendTotal(), 0);

	if (!TestTrue(TEXT("둘째 날 남은 2명은 고용할 수 있다"),
		Harness.Progression->TryHireStaff(MaxHires - 4))) return false;
	TestEqual(TEXT("둘째 날 원장에는 새 고용 비용만 기록된다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::StaffHire),
		(MaxHires - 4) * HireCost);
	const int32 GoldAfterAllHires = 10000 - MaxHires * HireCost;
	Harness.Progression->BeginNewDay(3);
	TestEqual(TEXT("셋째 날에도 축제 전체 고용 인원이 남는다"),
		Harness.Progression->GetHiredStaffCount(), MaxHires);
	TestEqual(TEXT("셋째 날 총원은 기본 인원과 누적 고용 인원의 합이다"),
		Harness.Progression->GetTotalStaffCount(), BaseStaff + MaxHires);
	TestEqual(TEXT("셋째 날에도 고용 상한이 다시 열리지 않는다"),
		Harness.Progression->GetRemainingHireCap(), 0);
	TestEqual(TEXT("상한을 다 쓰면 잔액이 있어도 추가 고용할 수 없다"),
		Harness.Progression->GetAffordableHireCount(), 0);
	TestFalse(TEXT("셋째 날 1명 추가 고용도 거절된다"), Harness.Progression->TryHireStaff(1));
	TestEqual(TEXT("세 날 전체에서 각 인원의 비용은 한 번씩만 지불한다"),
		Harness.Progression->GetGold(), GoldAfterAllHires);
	TestEqual(TEXT("셋째 날에도 자동 지출이 없다"), Harness.CalcLedgerSpendTotal(), 0);

	return true;
}

// 자동 고용이 없다. BeginNewDay 는 **한 명도 뽑지 않는다.**
// 여기서 최대로 고용하면 "사람 쓸까 Finale 살까"(§2.5.1)를 게임이 대신 결정한 것이 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_DoesNotAutoHire, "FestaFiesta.Economy.DayBoundary.DoesNotAutoHire", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_DoesNotAutoHire::RunTest(const FString& Parameters)
{
	// 돈이 넉넉해도 자동으로 고용하지 않아야 한다.
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(100'000)))
	{
		return false;
	}

	TestEqual(TEXT("축제 시작 직후 고용이 0명이다"),
		Harness.Progression->GetHiredStaffCount(), 0);
	TestEqual(TEXT("고용비가 한 푼도 안 나갔다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::StaffHire), 0);
	TestEqual(TEXT("잔액이 초기 자금 그대로다"),
		Harness.Progression->GetGold(), 100'000);

	Harness.Progression->BeginNewDay(2);
	TestEqual(TEXT("새 날에도 자동 고용하지 않는다"),
		Harness.Progression->GetHiredStaffCount(), 0);
	TestEqual(TEXT("새 날에도 고용비가 안 나간다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::StaffHire), 0);

	// 고용은 플레이어가 부를 때만 일어난다.
	TestEqual(TEXT("살 수 있는 인원은 상한까지다"),
		Harness.Progression->GetAffordableHireCount(), FFF_DayBoundaryTestHelpers::MaxHires);

	return true;
}

// 잔액이 0 이어도 기본 Staff 는 일한다. 자금난의 바닥이 "아무것도 못 사는 상태"이지
// "인원이 사라지는 상태"가 아니어야 회복이 가능하다 (§2.5.1).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_ZeroGoldKeepsBaseStaff, "FestaFiesta.Economy.DayBoundary.ZeroGoldKeepsBaseStaff", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_ZeroGoldKeepsBaseStaff::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(1000)))
	{
		return false;
	}

	// 가진 돈을 전부 쓴다. 거절할 수 없는 지출이 없으니 잔액이 0 이 되는 것은 이 경로뿐이다.
	TestTrue(TEXT("잔액 전액을 지출한다"),
		Harness.Progression->TrySpend(1000, EFF_SpendReason::FacilityInstall));
	TestEqual(TEXT("잔액이 0 이다"), Harness.Progression->GetGold(), 0);

	// 자금난의 바닥 — 잔액 0 이어도 기본 팀은 일한다.
	TestEqual(TEXT("잔액 0 이어도 기본 Staff 는 일한다"),
		Harness.Progression->GetTotalStaffCount(), FFF_DayBoundaryTestHelpers::BaseStaff);
	TestEqual(TEXT("추가 고용은 되지 않는다"),
		Harness.Progression->GetHiredStaffCount(), 0);
	TestEqual(TEXT("살 수 있는 인원이 0명이다"),
		Harness.Progression->GetAffordableHireCount(), 0);

	// 잔액 0 에서는 더 살 수 없을 뿐이고 잔액이 음수로 내려가지 않는다.
	TestFalse(TEXT("잔액 0 에서는 더 못 산다"),
		Harness.Progression->TrySpend(1, EFF_SpendReason::FacilityInstall));
	TestEqual(TEXT("거부 후에도 잔액이 0 이다"), Harness.Progression->GetGold(), 0);

	// 하루가 넘어가도 빚이 생기지 않는다 — 걷는 것이 없으니 잔액은 0 에 머문다.
	Harness.Progression->BeginNewDay(2);
	TestEqual(TEXT("하루를 넘겨도 잔액이 0 에 머문다"), Harness.Progression->GetGold(), 0);
	TestEqual(TEXT("경계가 만든 지출도 없다"), Harness.CalcLedgerSpendTotal(), 0);

	return true;
}

// 매출과 환급은 그대로 잔액이 된다. 중간에서 가져가는 것이 없다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_RevenueAndRefundLandInGoldDirectly, "FestaFiesta.Economy.DayBoundary.RevenueAndRefundLandInGoldDirectly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_RevenueAndRefundLandInGoldDirectly::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(1000)))
	{
		return false;
	}

	Harness.Progression->AddRevenue(300, EFF_EarnReason::AdmissionFee);
	TestEqual(TEXT("매출이 전액 잔액이 된다"), Harness.Progression->GetGold(), 1300);
	TestEqual(TEXT("누적 매출도 전액 잡힌다"), Harness.Progression->GetTotalRevenue(), 300);
	TestEqual(TEXT("오늘 매출도 전액 잡힌다"), Harness.Progression->GetTodayRevenue(), 300);

	// 환급은 매출이 아니다 — 잔액만 늘고 누적 매출은 그대로다.
	Harness.Progression->Refund(100);
	TestEqual(TEXT("환급도 전액 잔액이 된다"), Harness.Progression->GetGold(), 1400);
	TestEqual(TEXT("환급은 매출에 안 섞인다"), Harness.Progression->GetTotalRevenue(), 300);

	Harness.Progression->AddRevenue(1300, EFF_EarnReason::FacilityCommission, TEXT("Booth_FoodStall"));
	TestEqual(TEXT("매출이 계속 그대로 쌓인다"), Harness.Progression->GetGold(), 2700);

	// 정합성 불변식: 초기Gold + Σ매출 + 환급 − Σ지출 == Gold.
	const int32 Refunded = Harness.Progression->GetDayLedger().GetTotalRefund();
	TestEqual(TEXT("초기Gold + Σ매출 + 환급 − Σ지출 == Gold"),
		1000 + Harness.Progression->GetTotalRevenue() + Refunded - Harness.CalcLedgerSpendTotal(),
		Harness.Progression->GetGold());

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

// 인원 고용은 **부분 고용하지 않는다.** 상한 초과·잔액 부족은 전체를 거부하고 상태를 안 바꾼다.
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
		Harness.Progression->TryHireStaff(FFF_DayBoundaryTestHelpers::MaxHires + 1));
	TestEqual(TEXT("거부되면 아무도 고용되지 않는다"),
		Harness.Progression->GetHiredStaffCount(), 0);
	TestEqual(TEXT("거부되면 잔액이 그대로다"), Harness.Progression->GetGold(), GoldAfterRent);

	// 정상 고용.
	TestTrue(TEXT("상한과 같은 인원은 고용된다"),
		Harness.Progression->TryHireStaff(FFF_DayBoundaryTestHelpers::MaxHires));
	TestEqual(TEXT("고용 인원이 반영된다"),
		Harness.Progression->GetHiredStaffCount(), FFF_DayBoundaryTestHelpers::MaxHires);
	TestEqual(TEXT("고용비가 인원분만큼 빠진다"),
		Harness.Progression->GetGold(), GoldAfterRent
			- FFF_DayBoundaryTestHelpers::MaxHires * FFF_DayBoundaryTestHelpers::HireCost);
	TestEqual(TEXT("고용비가 원장에 기록된다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::StaffHire),
		FFF_DayBoundaryTestHelpers::MaxHires * FFF_DayBoundaryTestHelpers::HireCost);
	TestEqual(TEXT("전체 인원은 기본 + 고용이다"),
		Harness.Progression->GetTotalStaffCount(),
		FFF_DayBoundaryTestHelpers::BaseStaff + FFF_DayBoundaryTestHelpers::MaxHires);

	// 상한을 다 썼으면 1명도 더 안 된다. 누적으로 상한을 본다.
	TestEqual(TEXT("남은 고용 여력이 0 이다"),
		Harness.Progression->GetAffordableHireCount(), 0);
	TestFalse(TEXT("상한을 다 쓰면 추가 고용이 거부된다"),
		Harness.Progression->TryHireStaff(1));
	TestEqual(TEXT("거부 후에도 인원이 그대로다"),
		Harness.Progression->GetHiredStaffCount(), FFF_DayBoundaryTestHelpers::MaxHires);

	// 0 이하 요청은 거부된다 (고용 명령으로 돈을 돌려받는 경로를 만들지 않는다).
	TestFalse(TEXT("0명 요청은 거부된다"), Harness.Progression->TryHireStaff(0));
	TestFalse(TEXT("음수 요청은 거부된다"), Harness.Progression->TryHireStaff(-3));

	return true;
}

// 잔액이 모자라면 고용이 거부되고 돈이 한 푼도 안 빠진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_HireRejectedWhenGoldIsShort, "FestaFiesta.Economy.DayBoundary.HireRejectedWhenGoldIsShort", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_HireRejectedWhenGoldIsShort::RunTest(const FString& Parameters)
{
	// 1,000 만 남기면 2명분(900)만 살 수 있다.
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(1000)))
	{
		return false;
	}

	TestEqual(TEXT("잔액이 1,000 이다"), Harness.Progression->GetGold(), 1000);
	TestEqual(TEXT("살 수 있는 인원은 2명이다"),
		Harness.Progression->GetAffordableHireCount(), 2);

	// 3명은 1,350 이라 못 산다. **부분 고용하지 않고 전체를 거부한다.**
	TestFalse(TEXT("잔액을 넘는 요청은 거부된다"),
		Harness.Progression->TryHireStaff(3));
	TestEqual(TEXT("거부되면 아무도 고용되지 않는다"),
		Harness.Progression->GetHiredStaffCount(), 0);
	TestEqual(TEXT("거부되면 잔액이 그대로다"), Harness.Progression->GetGold(), 1000);
	TestEqual(TEXT("거부되면 고용비 기록도 없다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::StaffHire), 0);

	// 살 수 있는 만큼은 성공한다.
	TestTrue(TEXT("2명은 고용된다"), Harness.Progression->TryHireStaff(2));
	TestEqual(TEXT("2명분 고용비만 빠진다"), Harness.Progression->GetGold(), 100);
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

	TestFalse(TEXT("고용도 거부된다"), Progression->TryHireStaff(1));
	TestEqual(TEXT("살 수 있는 인원도 0 이다"),
		Progression->GetAffordableHireCount(), 0);

	return true;
}


// ============================================================================
// 직전 하루 원장 스냅샷 (GetEndedDayLedger / GetEndedDay)
//
// 결산 화면이 "방금 끝난 하루"를 그리는데 BeginNewDay 가 DayLedger 를 리셋한다.
// 스냅샷을 Reset 과 같은 함수(TryProcessNewDay)에 둬서 소유자를 하나로 유지한다.
//
// 🔴 두 Getter 는 서로를 대체하지 않는다. 유효 구간이 정확히 갈린다:
//    · Report 단계 안(경계 넘기 전) → GetDayLedger()  (라이브)
//    · 경계를 넘은 뒤              → GetEndedDayLedger()
//    Day1 Report 에는 스냅샷이 아예 없고, Day3 뒤에는 경계가 없어 스냅샷이 Day2 에 멈춘다.
//    아래 테스트들이 그 계약을 고정한다 — UI 가 한쪽으로 통일하면 Day2 만 맞는다.
//
// 🔴 GetEndedDay() 와 GetLastProcessedDay() 는 하루 어긋난다. BeginNewDay(2) 직후
//    GetLastProcessedDay() == 2 (열린 날) 이고 GetEndedDay() == 1 (닫힌 날) 이다.
//    잘못 골라도 컴파일되고 그럴듯한 숫자가 나오므로 아래 테스트들이 값으로 갈라둔다.
// ============================================================================

// 첫 경계를 넘기 전에는 "끝난 하루"가 없다.
// 🔴 Day1 Report 함정: 여기서 결산 화면이 GetEndedDayLedger() 를 읽으면 빈 표가 뜬다.
//    EndedDay == 0 이 그 상태를 UI 가 구분할 수 있는 유일한 신호다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_EndedDayLedgerIsEmptyBeforeFirstBoundary, "FestaFiesta.Economy.DayBoundary.EndedDayLedgerIsEmptyBeforeFirstBoundary", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_EndedDayLedgerIsEmptyBeforeFirstBoundary::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	Harness.Progression->AddRevenue(1200, EFF_EarnReason::AdmissionFee);

	// StartFestival 이 Day1 을 정산했지만 그건 "Day1 시작"이지 "Day0 종료"가 아니다.
	TestEqual(TEXT("끝난 하루가 아직 없다"), Harness.Progression->GetEndedDay(), 0);

	const FFF_DayLedger& Snapshot = Harness.Progression->GetEndedDayLedger();
	TestEqual(TEXT("스냅샷 지출이 비어 있다"), FFF_DayBoundaryTestHelpers::SumSpend(Snapshot), 0);
	TestEqual(TEXT("스냅샷 매출이 비어 있다"), FFF_DayBoundaryTestHelpers::SumEarn(Snapshot), 0);
	TestEqual(TEXT("스냅샷 환급도 비어 있다"), Snapshot.GetTotalRefund(), 0);
	TestEqual(TEXT("스냅샷 부스별 표도 비어 있다"), Snapshot.GetRevenueByFacility().Num(), 0);

	// 반면 라이브 원장에는 Day1 이 살아 있다 — Report 단계에서 봐야 할 쪽은 이것이다.
	TestEqual(TEXT("라이브 원장에는 Day1 매출이 있다"), Harness.CalcLedgerEarnTotal(), 1200);
	TestEqual(TEXT("라이브 원장에 Day1 지출은 아직 없다"),
		Harness.CalcLedgerSpendTotal(), 0);

	return true;
}

// 경계를 넘으면 스냅샷이 끝난 하루를 통째로 들고, 라이브는 새 하루로 갈린다.
// 원인별·부스별·환급이 전부 보존되어야 한다 — 결산 표가 그 세 가지를 그린다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_EndedDayLedgerHoldsTheClosedDay, "FestaFiesta.Economy.DayBoundary.EndedDayLedgerHoldsTheClosedDay", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_EndedDayLedgerHoldsTheClosedDay::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	// Day1 에 세 종류를 전부 만든다: 매출(부스별 포함) · 고용비 지출 · 철거 환급.
	Harness.Progression->AddRevenue(1200, EFF_EarnReason::AdmissionFee);
	Harness.Progression->AddRevenue(300, EFF_EarnReason::FacilityCommission, TEXT("Booth_FoodStall"));
	TestTrue(TEXT("Day1 에 인원 2명을 고용한다"), Harness.Progression->TryHireStaff(2));
	Harness.Progression->Refund(500);

	Harness.Progression->BeginNewDay(2);

	TestEqual(TEXT("스냅샷이 Day1 의 것이다"), Harness.Progression->GetEndedDay(), 1);

	const FFF_DayLedger& Snapshot = Harness.Progression->GetEndedDayLedger();

	// --- 원인별 매출 ---
	TestEqual(TEXT("스냅샷에 입장료가 남는다"),
		Snapshot.GetEarnByReason(EFF_EarnReason::AdmissionFee), 1200);
	TestEqual(TEXT("스냅샷에 시설 수수료가 남는다"),
		Snapshot.GetEarnByReason(EFF_EarnReason::FacilityCommission), 300);
	TestEqual(TEXT("스냅샷 매출 합이 1,500 이다"), FFF_DayBoundaryTestHelpers::SumEarn(Snapshot), 1500);

	// --- 원인별 지출 ---
	TestEqual(TEXT("스냅샷에 Day1 고용비가 남는다"),
		Snapshot.GetSpendByReason(EFF_SpendReason::StaffHire),
		2 * FFF_DayBoundaryTestHelpers::HireCost);

	// --- 환급은 매출과 분리된 채로 보존된다 (2026-08-13 규칙) ---
	TestEqual(TEXT("스냅샷에 환급이 남는다"), Snapshot.GetTotalRefund(), 500);
	TestEqual(TEXT("환급이 스냅샷 매출에 섞이지 않았다"),
		FFF_DayBoundaryTestHelpers::SumEarn(Snapshot), 1500);

	// --- 부스별 표 ---
	const int32* FoodStallRevenue = Snapshot.GetRevenueByFacility().Find(TEXT("Booth_FoodStall"));
	if (TestNotNull(TEXT("스냅샷에 부스별 매출이 남는다"), FoodStallRevenue))
	{
		TestEqual(TEXT("부스별 금액이 보존된다"), *FoodStallRevenue, 300);
	}

	// --- 라이브는 새 하루다. 두 원장이 실제로 갈렸는지 지출 합으로 구분한다 ---
	// 스냅샷 = 고용비 2명분 / 라이브 = Day2 에 아직 아무 지출도 없다.
	TestEqual(TEXT("스냅샷 지출 합이 고용비 2명분이다"),
		FFF_DayBoundaryTestHelpers::SumSpend(Snapshot),
		2 * FFF_DayBoundaryTestHelpers::HireCost);
	TestEqual(TEXT("라이브 지출 합은 0 이다"), Harness.CalcLedgerSpendTotal(), 0);
	TestEqual(TEXT("라이브에 어제 고용비가 넘어오지 않는다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::StaffHire), 0);
	TestEqual(TEXT("라이브 매출은 0 이다"), Harness.CalcLedgerEarnTotal(), 0);
	TestEqual(TEXT("라이브 환급도 0 이다"),
		Harness.Progression->GetDayLedger().GetTotalRefund(), 0);
	TestEqual(TEXT("라이브 부스별 표도 비었다"),
		Harness.Progression->GetDayLedger().GetRevenueByFacility().Num(), 0);

	return true;
}

// 🔴 중복 정산 가드가 스냅샷도 지켜야 한다.
// 스냅샷 갱신을 LastProcessedDay 가드 **밖**에 두면, MorningCheck 재진입 한 번에
// 어제 결산이 "오늘 막 열린 빈 원장"으로 덮인다. 크래시도 로그도 없이 표만 틀린다.
// 그리고 스냅샷이 참조가 아니라 **값 복사**인지도 여기서 같이 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_DuplicateBoundaryDoesNotOverwriteSnapshot, "FestaFiesta.Economy.DayBoundary.DuplicateBoundaryDoesNotOverwriteSnapshot", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_DuplicateBoundaryDoesNotOverwriteSnapshot::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	Harness.Progression->AddRevenue(1200, EFF_EarnReason::AdmissionFee);
	Harness.Progression->BeginNewDay(2);

	TestEqual(TEXT("스냅샷이 Day1 을 담았다"),
		FFF_DayBoundaryTestHelpers::SumEarn(Harness.Progression->GetEndedDayLedger()), 1200);

	// Day2 에 매출이 쌓인다. 스냅샷은 값 복사라 여기 영향을 받으면 안 된다.
	Harness.Progression->AddRevenue(999, EFF_EarnReason::MinigameReward);
	TestEqual(TEXT("스냅샷은 값 복사라 라이브 변경에 흔들리지 않는다"),
		FFF_DayBoundaryTestHelpers::SumEarn(Harness.Progression->GetEndedDayLedger()), 1200);
	TestEqual(TEXT("날짜도 그대로 1 이다"), Harness.Progression->GetEndedDay(), 1);

	// MorningCheck 재진입 — 같은 날을 다시 연다. 가드가 스냅샷까지 막아야 한다.
	Harness.Progression->BeginNewDay(2);
	TestEqual(TEXT("같은 날 재진입이 스냅샷을 덮지 않는다"),
		FFF_DayBoundaryTestHelpers::SumEarn(Harness.Progression->GetEndedDayLedger()), 1200);
	TestEqual(TEXT("재진입 후에도 날짜가 1 이다"), Harness.Progression->GetEndedDay(), 1);
	TestEqual(TEXT("재진입해도 Day2 라이브 매출이 살아 있다"),
		Harness.CalcLedgerEarnTotal(), 999);

	// 지난 날로 되돌리는 호출도 마찬가지다.
	Harness.Progression->BeginNewDay(1);
	TestEqual(TEXT("지난 날 호출도 스냅샷을 덮지 않는다"),
		FFF_DayBoundaryTestHelpers::SumEarn(Harness.Progression->GetEndedDayLedger()), 1200);
	TestEqual(TEXT("지난 날 호출 후에도 날짜가 1 이다"), Harness.Progression->GetEndedDay(), 1);

	return true;
}

// 스냅샷은 **가장 최근 경계 하나**만 들고 있다. 날이 넘어가면 갱신된다.
// 🔴 그리고 Day3 뒤에는 경계가 없다 — AdvanceToNextDay 가 CurrentDay>=3 에서 early return 한다.
//    그래서 최종 결산 시점에 GetEndedDay() 는 2 에 멈춰 있고 Day3 은 라이브에만 있다.
//    UI 가 결산을 GetEndedDayLedger() 로 통일하면 최종 결산에 Day2 표가 뜬다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_EndedDayLedgerTracksLatestBoundaryOnly, "FestaFiesta.Economy.DayBoundary.EndedDayLedgerTracksLatestBoundaryOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_EndedDayLedgerTracksLatestBoundaryOnly::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	// Day1 → Day2
	Harness.Progression->AddRevenue(100, EFF_EarnReason::AdmissionFee);
	Harness.Progression->BeginNewDay(2);
	TestEqual(TEXT("경계 1 회 후 날짜가 1"), Harness.Progression->GetEndedDay(), 1);
	TestEqual(TEXT("스냅샷이 Day1 매출"),
		FFF_DayBoundaryTestHelpers::SumEarn(Harness.Progression->GetEndedDayLedger()), 100);

	// Day2 → Day3. 스냅샷이 Day2 로 갈아끼워지고 Day1 은 사라진다(누적하지 않는다).
	Harness.Progression->AddRevenue(200, EFF_EarnReason::AdmissionFee);
	Harness.Progression->BeginNewDay(3);
	TestEqual(TEXT("경계 2 회 후 날짜가 2"), Harness.Progression->GetEndedDay(), 2);
	TestEqual(TEXT("스냅샷이 Day2 로 갱신된다"),
		FFF_DayBoundaryTestHelpers::SumEarn(Harness.Progression->GetEndedDayLedger()), 200);

	// Day3 은 여기서 끝난다 — 뒤에 BeginNewDay(4) 가 없다.
	Harness.Progression->AddRevenue(300, EFF_EarnReason::AdmissionFee);

	TestEqual(TEXT("Day3 매출은 라이브에만 있다"), Harness.CalcLedgerEarnTotal(), 300);
	// 🔴 최종 결산의 계약: 스냅샷은 Day2 에 멈춰 있다. 여기를 읽으면 틀린 표가 나온다.
	TestEqual(TEXT("Day3 에도 스냅샷은 Day2 에 멈춘다"),
		FFF_DayBoundaryTestHelpers::SumEarn(Harness.Progression->GetEndedDayLedger()), 200);
	TestEqual(TEXT("Day3 에도 EndedDay 는 2 다"), Harness.Progression->GetEndedDay(), 2);

	// 그래서 UI 판정은 이 형태여야 한다: 그리려는 날짜가 EndedDay 와 같을 때만 스냅샷.
	const int32 DayToDraw = 3;
	TestFalse(TEXT("Day3 을 그릴 때는 스냅샷이 대상이 아니다"),
		DayToDraw == Harness.Progression->GetEndedDay());

	return true;
}

// 새 축제를 시작하면 이전 판의 스냅샷이 남지 않는다.
// StartFestival 이 다른 상태를 전부 되돌리므로 여기만 빠지면 이전 판 결산이 새 판에 보인다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_StartFestivalClearsSnapshot, "FestaFiesta.Economy.DayBoundary.StartFestivalClearsSnapshot", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_StartFestivalClearsSnapshot::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	Harness.Progression->AddRevenue(1200, EFF_EarnReason::AdmissionFee);
	Harness.Progression->BeginNewDay(2);
	TestEqual(TEXT("이전 판에 스냅샷이 쌓였다"),
		FFF_DayBoundaryTestHelpers::SumEarn(Harness.Progression->GetEndedDayLedger()), 1200);
	TestEqual(TEXT("이전 판 날짜가 1"), Harness.Progression->GetEndedDay(), 1);

	// 같은 정의로 축제를 다시 시작한다.
	Harness.Progression->StartFestival(Harness.Definition);

	TestEqual(TEXT("새 축제에는 끝난 하루가 없다"), Harness.Progression->GetEndedDay(), 0);
	TestEqual(TEXT("이전 판 스냅샷 매출이 남지 않는다"),
		FFF_DayBoundaryTestHelpers::SumEarn(Harness.Progression->GetEndedDayLedger()), 0);
	TestEqual(TEXT("이전 판 스냅샷 지출도 남지 않는다"),
		FFF_DayBoundaryTestHelpers::SumSpend(Harness.Progression->GetEndedDayLedger()), 0);

	return true;
}

// 영구 고용 인원·피날레·홍보 구매 상태가 저장·복원을 견딘다.
// 이 셋은 2026-09-22 까지 저장 형식에서 빠져 있었다 — 임대료 삭제(9/21)로 Gold 만 있으면
// 되던 시절의 저장 형식이 그대로 남아 있었다. Staff 가 영구 고용으로 바뀌고 홍보·피날레가
// 이미 있는 지금은 이어하기가 이 셋을 잃으면 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_SaveRestoresStaffFinalePromotion, "FestaFiesta.Economy.DayBoundary.SaveRestoresStaffFinalePromotion", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_SaveRestoresStaffFinalePromotion::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(20000)))
	{
		return false;
	}

	if (!TestTrue(TEXT("인원 3명을 고용한다"), Harness.Progression->TryHireStaff(3))) return false;
	if (!TestEqual(TEXT("피날레 C 구매가 성공한다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::C),
		EFF_FinalePurchaseResult::Success)) return false;
	if (!TestEqual(TEXT("홍보 기본 구매가 성공한다"),
		Harness.Progression->TryPurchasePromotion(EFF_PromotionGrade::Basic),
		EFF_PromotionPurchaseResult::Success)) return false;

	const int32 StaffBeforeSave = Harness.Progression->GetTotalStaffCount();

	TArray<uint8> Saved;
	Harness.Progression->WriteSaveData(Saved);

	// 새 축제로 리셋해서 저장 없이는 값이 안 살아남는다는 것부터 확인한다.
	Harness.Progression->StartFestival(Harness.Definition);
	TestNotEqual(TEXT("리셋 직후 인원 수는 저장 시점과 다르다"),
		Harness.Progression->GetTotalStaffCount(), StaffBeforeSave);
	TestEqual(TEXT("리셋 직후 피날레는 미구매로 돌아간다"),
		Harness.Progression->GetPurchasedFinaleGrade(), EFF_FinaleGrade::None);
	TestEqual(TEXT("리셋 직후 홍보도 미구매로 돌아간다"),
		Harness.Progression->GetPurchasedPromotionGrade(), EFF_PromotionGrade::None);

	Harness.Progression->ReadSaveData(Saved);

	TestEqual(TEXT("복원 후 고용 인원이 저장 시점과 같다"),
		Harness.Progression->GetTotalStaffCount(), StaffBeforeSave);
	TestEqual(TEXT("복원 후 피날레 등급이 되돌아온다"),
		Harness.Progression->GetPurchasedFinaleGrade(), EFF_FinaleGrade::C);
	TestEqual(TEXT("복원 후 홍보 등급이 되돌아온다"),
		Harness.Progression->GetPurchasedPromotionGrade(), EFF_PromotionGrade::Basic);

	return true;
}

// 승급은 고용 당일엔 막히고, 다음 날 아침부터 가능하며, 한 번 올린 레벨은 저장·복원을 견딘다.
// staff_event_design.md §4 "고용 시점이 최종 도달 등급을 정한다" 규칙의 회귀 가드.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayBoundary_PromotionRespectsHireDayAndPersists, "FestaFiesta.Economy.DayBoundary.PromotionRespectsHireDayAndPersists", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayBoundary_PromotionRespectsHireDayAndPersists::RunTest(const FString& Parameters)
{
	FFF_DayBoundaryTestHelpers::FDayBoundaryHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(20000)))
	{
		return false;
	}

	if (!TestTrue(TEXT("Day1 에 1명 고용한다"), Harness.Progression->TryHireStaff(1)))
	{
		return false;
	}

	TArray<FFF_StaffMember> Roster = Harness.Progression->GetStaffRoster();
	const FFF_StaffMember* Hired = Roster.FindByPredicate(
		[](const FFF_StaffMember& Member) { return !Member.bIsBaseStaff; });
	if (!TestNotNull(TEXT("방금 고용한 사람이 명단에 있다"), Hired))
	{
		return false;
	}
	const int32 HiredId = Hired->StaffId;
	TestEqual(TEXT("Day1 고용은 HiredOnDay 1로 남는다"), Hired->HiredOnDay, 1);
	TestEqual(TEXT("고용 직후 레벨은 1이다"), Hired->TrainingLevel, 1);

	TestFalse(TEXT("고용한 당일은 승급 못 한다"), Harness.Progression->TryPromoteStaff(HiredId));

	Harness.Progression->BeginNewDay(2);
	TestTrue(TEXT("다음 날 아침부터는 승급 가능하다"), Harness.Progression->TryPromoteStaff(HiredId));
	TestFalse(TEXT("같은 날 두 번 승급은 안 된다"), Harness.Progression->TryPromoteStaff(HiredId));
	TestFalse(TEXT("존재하지 않는 StaffId는 조용히 실패한다"), Harness.Progression->TryPromoteStaff(99999));

	Roster = Harness.Progression->GetStaffRoster();
	const FFF_StaffMember* Promoted = Roster.FindByPredicate(
		[HiredId](const FFF_StaffMember& Member) { return Member.StaffId == HiredId; });
	if (!TestNotNull(TEXT("승급한 사람이 명단에 그대로 있다"), Promoted))
	{
		return false;
	}
	TestEqual(TEXT("승급 후 레벨은 2다"), Promoted->TrainingLevel, 2);

	// 저장·복원을 거쳐도 개인별 레벨·고용일이 살아남아야 한다.
	TArray<uint8> Saved;
	Harness.Progression->WriteSaveData(Saved);
	Harness.Progression->StartFestival(Harness.Definition);
	Harness.Progression->ReadSaveData(Saved);

	Roster = Harness.Progression->GetStaffRoster();
	const FFF_StaffMember* Restored = Roster.FindByPredicate(
		[HiredId](const FFF_StaffMember& Member) { return Member.StaffId == HiredId; });
	if (!TestNotNull(TEXT("복원 후에도 같은 StaffId가 남아 있다"), Restored))
	{
		return false;
	}
	TestEqual(TEXT("복원 후에도 레벨 2가 유지된다"), Restored->TrainingLevel, 2);
	TestEqual(TEXT("복원 후에도 HiredOnDay가 유지된다"), Restored->HiredOnDay, 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
