// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"

/**
 * 피날레 구매의 결과를 검증한다 (S7). [참고] economy_spec.md §5.1 S7 · §2.5
 *
 * "살 수 있는가"는 FF_FinaleRulesTests.cpp 가 갖고, 여기는 산 뒤에 무엇이 바뀌는가를 본다 —
 * 잔액·원장·구매 상태·만족도 보너스, 그리고 §2.5 의 3일 수지가 실제로 A 에서 멈추는지.
 *
 * FinaleTiers 는 UFF_FestivalDefinition 생성자가 [기획서] §10 값으로 채우므로 저작 없이 돈다.
 */
namespace FFF_FinalePurchaseTestHelpers
{
	// UFF_FestivalDefinition 생성자의 FinaleTiers. 여기 값이 바뀌면 아래 기대값도 같이 바꿔야 한다.
	constexpr int32 CostC = 1000;
	constexpr int32 CostA = 2600;
	constexpr int32 BonusA = 10;
	constexpr int32 CostS = 3500;

	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	struct FFinalePurchaseHarness
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

			// 표가 비어 있으면 IsDefinitionValid 가 Error 를 찍고 자동화 프레임워크가 실패로 취급한다.
			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
			Definition->FacilityCostTable = CostTable;

			Definition->FestivalId = TEXT("FinalePurchaseAutomationFestival");
			Definition->InitialGold = InitialGold;

			Progression->StartFestival(Definition);
			return true;
		}

		// Σ지출. 정합성 불변식(초기Gold + Σ매출 − Σ지출 == Gold) 검산용.
		int32 CalcLedgerSpendTotal() const
		{
			const FFF_DayLedger& Ledger = Progression->GetDayLedger();
			int32 Total = 0;
			for (int32 Index = 0;
				Index <= static_cast<int32>(EFF_SpendReason::FacilityUpgrade);
				++Index)
			{
				Total += Ledger.GetSpendByReason(static_cast<EFF_SpendReason>(Index));
			}
			return Total;
		}

		UGameInstance* GameInstance = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
	};
}

// 구매가 잔액을 깎고 원장에 남고 보너스를 노출한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinalePurchase_SpendsGoldAndRecordsLedger, "FestaFiesta.Economy.FinalePurchase.SpendsGoldAndRecordsLedger", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinalePurchase_SpendsGoldAndRecordsLedger::RunTest(const FString& Parameters)
{
	FFF_FinalePurchaseTestHelpers::FFinalePurchaseHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	// 사기 전.
	TestEqual(TEXT("시작 시 미구매다"),
		Harness.Progression->GetPurchasedFinaleGrade(), EFF_FinaleGrade::None);
	TestEqual(TEXT("미구매면 보너스가 0 이다"),
		Harness.Progression->GetFinaleSatisfactionBonus(), 0);

	TestEqual(TEXT("A 를 산다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::A),
		EFF_FinalePurchaseResult::Success);

	TestEqual(TEXT("비용만큼 잔액이 줄었다"), Harness.Progression->GetGold(),
		10000 - FFF_FinalePurchaseTestHelpers::CostA);
	TestEqual(TEXT("구매 등급이 A 다"),
		Harness.Progression->GetPurchasedFinaleGrade(), EFF_FinaleGrade::A);
	TestEqual(TEXT("보너스가 노출된다"), Harness.Progression->GetFinaleSatisfactionBonus(),
		FFF_FinalePurchaseTestHelpers::BonusA);
	TestEqual(TEXT("피날레 폭죽 원인으로 원장에 남는다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::FinaleFireworks),
		FFF_FinalePurchaseTestHelpers::CostA);

	// 정합성 불변식: 초기Gold + Σ매출 − Σ지출 == Gold.
	TestEqual(TEXT("정합성 불변식이 유지된다"),
		10000 + Harness.Progression->GetTodayRevenue() - Harness.CalcLedgerSpendTotal(),
		Harness.Progression->GetGold());

	return true;
}

// 실패는 상태를 전혀 바꾸지 않는다 (§3.1) — 잔액 부족과 중복 구매 둘 다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinalePurchase_FailureDoesNotMutate, "FestaFiesta.Economy.FinalePurchase.FailureDoesNotMutate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinalePurchase_FailureDoesNotMutate::RunTest(const FString& Parameters)
{
	FFF_FinalePurchaseTestHelpers::FFinalePurchaseHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(FFF_FinalePurchaseTestHelpers::CostC)))
	{
		return false;
	}

	// C 값만 들고 S 를 살 수 없다. 클램프해서 가진 만큼만 빼는 일이 없어야 한다.
	TestEqual(TEXT("잔액이 부족하면 거부된다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::S),
		EFF_FinalePurchaseResult::InsufficientGold);
	TestEqual(TEXT("거부되면 잔액이 그대로다"), Harness.Progression->GetGold(),
		FFF_FinalePurchaseTestHelpers::CostC);
	TestEqual(TEXT("거부되면 미구매 상태다"),
		Harness.Progression->GetPurchasedFinaleGrade(), EFF_FinaleGrade::None);
	TestEqual(TEXT("거부되면 원장에 아무것도 안 남는다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::FinaleFireworks), 0);

	// C 는 딱 살 수 있다.
	TestEqual(TEXT("C 를 산다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::C),
		EFF_FinalePurchaseResult::Success);
	TestEqual(TEXT("잔액이 0 이 된다"), Harness.Progression->GetGold(), 0);

	// 돈이 다시 생겨도 등급을 바꿀 수 없다.
	Harness.Progression->AddRevenue(100000, EFF_EarnReason::FacilityCommission);
	const int32 GoldBeforeSecondTry = Harness.Progression->GetGold();

	TestEqual(TEXT("돈이 넘쳐도 두 번째 구매는 거부된다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::S),
		EFF_FinalePurchaseResult::AlreadyPurchased);
	TestEqual(TEXT("거부 후 잔액이 그대로다"),
		Harness.Progression->GetGold(), GoldBeforeSecondTry);
	TestEqual(TEXT("등급이 C 에 남아 있다"),
		Harness.Progression->GetPurchasedFinaleGrade(), EFF_FinaleGrade::C);
	TestEqual(TEXT("원장의 피날레 지출이 C 비용 그대로다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::FinaleFireworks),
		FFF_FinalePurchaseTestHelpers::CostC);

	return true;
}

// 만족도 보너스는 Gold 가 아니다 — 매출에 섞이지 않는다.
// 환급을 TotalRevenue 에서 분리한 것과 같은 종류의 사고 방지다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinalePurchase_BonusIsNotRevenue, "FestaFiesta.Economy.FinalePurchase.BonusIsNotRevenue", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinalePurchase_BonusIsNotRevenue::RunTest(const FString& Parameters)
{
	FFF_FinalePurchaseTestHelpers::FFinalePurchaseHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	TestEqual(TEXT("A 를 산다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::A),
		EFF_FinalePurchaseResult::Success);

	TestEqual(TEXT("보너스가 A 등급 값이다"), Harness.Progression->GetFinaleSatisfactionBonus(),
		FFF_FinalePurchaseTestHelpers::BonusA);

	// 보너스가 어디에도 Gold 로 새지 않았다.
	TestEqual(TEXT("누적 매출이 0 이다"), Harness.Progression->GetTotalRevenue(), 0);
	TestEqual(TEXT("오늘 매출이 0 이다"), Harness.Progression->GetTodayRevenue(), 0);
	TestEqual(TEXT("환급 집계도 0 이다"),
		Harness.Progression->GetDayLedger().GetTotalRefund(), 0);

	int32 EarnTotal = 0;
	for (int32 Index = 0; Index <= static_cast<int32>(EFF_EarnReason::IncidentReward); ++Index)
	{
		EarnTotal += Harness.Progression->GetDayLedger().GetEarnByReason(
			static_cast<EFF_EarnReason>(Index));
	}
	TestEqual(TEXT("원인별 매출 합계도 0 이다"), EarnTotal, 0);

	return true;
}

// 잔액이 모자라면 추가 규칙 없이 CanAfford 하나가 막는다. "무엇무엇이면 구매 금지" 같은
// 판정을 따로 만들지 않는다 — 같은 판정을 두 곳에 두지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinalePurchase_InsufficientGoldBlocksCheapestGrade, "FestaFiesta.Economy.FinalePurchase.InsufficientGoldBlocksCheapestGrade", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinalePurchase_InsufficientGoldBlocksCheapestGrade::RunTest(const FString& Parameters)
{
	// 가장 싼 등급(C)보다 1 적게 들고 시작한다.
	FFF_FinalePurchaseTestHelpers::FFinalePurchaseHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(FFF_FinalePurchaseTestHelpers::CostC - 1)))
	{
		return false;
	}

	TestEqual(TEXT("가장 싼 등급도 못 산다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::C),
		EFF_FinalePurchaseResult::InsufficientGold);
	TestEqual(TEXT("거부 후에도 미구매 상태다"),
		Harness.Progression->GetPurchasedFinaleGrade(), EFF_FinaleGrade::None);
	TestEqual(TEXT("거부는 잔액을 바꾸지 않는다"),
		Harness.Progression->GetGold(), FFF_FinalePurchaseTestHelpers::CostC - 1);

	return true;
}

// 이 파일의 가격 상수가 정의 생성자의 FinaleTiers 와 같다. 가격을 밸런싱하면 여기서 먼저 깨진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinalePurchase_DefaultTiersMatchConstants, "FestaFiesta.Economy.FinalePurchase.DefaultTiersMatchConstants", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinalePurchase_DefaultTiersMatchConstants::RunTest(const FString& Parameters)
{
	const UFF_FestivalDefinition* Defaults = GetDefault<UFF_FestivalDefinition>();
	auto FindCost = [Defaults](EFF_FinaleGrade Grade)
	{
		const FFF_FinaleTier* Tier = Defaults->FinaleTiers.FindByPredicate(
			[Grade](const FFF_FinaleTier& Candidate) { return Candidate.Grade == Grade; });
		return Tier ? Tier->CostGold : INDEX_NONE;
	};

	TestEqual(TEXT("C 가격"), FindCost(EFF_FinaleGrade::C), FFF_FinalePurchaseTestHelpers::CostC);
	TestEqual(TEXT("A 가격"), FindCost(EFF_FinaleGrade::A), FFF_FinalePurchaseTestHelpers::CostA);
	TestEqual(TEXT("S 가격"), FindCost(EFF_FinaleGrade::S), FFF_FinalePurchaseTestHelpers::CostS);
	return true;
}

// 새 축제를 시작하면 구매 상태가 초기화된다.
// 안 그러면 두 번째 플레이에서 첫 판의 등급이 남아 AlreadyPurchased 로 영구히 막힌다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinalePurchase_StartFestivalResetsPurchase, "FestaFiesta.Economy.FinalePurchase.StartFestivalResetsPurchase", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinalePurchase_StartFestivalResetsPurchase::RunTest(const FString& Parameters)
{
	FFF_FinalePurchaseTestHelpers::FFinalePurchaseHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	TestEqual(TEXT("A 를 산다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::A),
		EFF_FinalePurchaseResult::Success);

	// 같은 정의로 축제를 다시 시작한다. Gold·원장이 초기화되는 그 경로다.
	Harness.Progression->StartFestival(Harness.Definition);

	TestEqual(TEXT("구매 상태가 미구매로 돌아간다"),
		Harness.Progression->GetPurchasedFinaleGrade(), EFF_FinaleGrade::None);
	TestEqual(TEXT("보너스도 0 으로 돌아간다"),
		Harness.Progression->GetFinaleSatisfactionBonus(), 0);
	TestEqual(TEXT("다시 살 수 있다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::S),
		EFF_FinalePurchaseResult::Success);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
