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
	// [기획서] §10. 여기 값이 바뀌면 아래 기대값도 같이 바꿔야 한다.
	constexpr int32 CostC = 2000;
	constexpr int32 CostA = 6000;
	constexpr int32 BonusA = 20;
	constexpr int32 CostS = 8000;

	// §2.5 3일 수지의 순이익. Finale 를 사는 예산이 이 값이다.
	//
	// 2026-08-24 에 7,680 → 7,930 으로 바뀌었다. 유료 프로그램이 하루짜리 카드가 되면서
	// 3일 순익 930 이 유입에 들어왔다(옛 값은 기간권 전제의 680 이었다).
	// **정원이 3일 내내 다 찰 때의 상한**이라 실제 플레이는 이보다 낮게 나온다.
	constexpr int32 ThreeDayNetProfit = 7930;

	// S 에 얼마나 모자란가. 이 간격이 좁을수록 "한 번 더 하면 닿는다"가 된다.
	constexpr int32 ShortfallForS = CostS - ThreeDayNetProfit;   // 70

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
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(2000)))
	{
		return false;
	}

	// 2,000 으로 S(8,000)를 살 수 없다. 클램프해서 2,000 만 빼는 일이 없어야 한다.
	TestEqual(TEXT("잔액이 부족하면 거부된다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::S),
		EFF_FinalePurchaseResult::InsufficientGold);
	TestEqual(TEXT("거부되면 잔액이 그대로다"), Harness.Progression->GetGold(), 2000);
	TestEqual(TEXT("거부되면 미구매 상태다"),
		Harness.Progression->GetPurchasedFinaleGrade(), EFF_FinaleGrade::None);
	TestEqual(TEXT("거부되면 원장에 아무것도 안 남는다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::FinaleFireworks), 0);

	// C(2,000)는 딱 살 수 있다.
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
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::FinaleFireworks), 2000);

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

	TestEqual(TEXT("보너스가 20 점이다"), Harness.Progression->GetFinaleSatisfactionBonus(),
		FFF_FinalePurchaseTestHelpers::BonusA);

	// 20 이 어디에도 Gold 로 새지 않았다.
	TestEqual(TEXT("누적 매출이 0 이다"), Harness.Progression->GetTotalRevenue(), 0);
	TestEqual(TEXT("오늘 매출이 0 이다"), Harness.Progression->GetTodayRevenue(), 0);
	TestEqual(TEXT("환급 집계도 0 이다"),
		Harness.Progression->GetDayLedger().GetTotalRefund(), 0);

	int32 EarnTotal = 0;
	for (int32 Index = 0; Index <= static_cast<int32>(EFF_EarnReason::MinigameReward); ++Index)
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
	// 가장 싼 등급(C = 2,000)보다 1 적게 들고 시작한다.
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

// 🔴 3일 루프가 닫혔다는 증거 — §2.5 순이익 7,930 으로 A 는 사지고 S 는 70 부족하다.
//
// 3일을 실제로 시뮬레이션하지 않고 순이익을 직접 넣는다. 유입·유출 순서가 섞이면
// 최종 잔액이 달라져서, 검산의 결론("A 는 되고 S 는 안 된다")이 아니라 진행 순서를 테스트하게 된다.
// 하루 경계 자체는 FF_DayBoundaryTests 와 FF_StageEconomyBridgeTests 가 이미 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinalePurchase_ThreeDayNetProfitBuysAButNotS, "FestaFiesta.Economy.FinalePurchase.ThreeDayNetProfitBuysAButNotS", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinalePurchase_ThreeDayNetProfitBuysAButNotS::RunTest(const FString& Parameters)
{
	// 순이익을 잔액으로 직접 넣는다. InitialGold 0 은 IsDefinitionValid 가 Error 로 거부하므로
	// (0 이면 Setup 에서 시설을 지을 수 없다) 매출로 채우는 대신 시작 잔액으로 준다.
	FFF_FinalePurchaseTestHelpers::FFinalePurchaseHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"),
		Harness.Initialize(FFF_FinalePurchaseTestHelpers::ThreeDayNetProfit)))
	{
		return false;
	}

	TestEqual(TEXT("Finale 예산이 3일 순이익과 같다"), Harness.Progression->GetGold(),
		FFF_FinalePurchaseTestHelpers::ThreeDayNetProfit);

	// 🔴 완벽하게 플레이해도 S 에 닿지 않는다. 이 성질이 §2.5 의 결론이다.
	//
	// 간격의 크기(70)가 아니라 **부호**를 고정한다. 카드 값을 밸런싱하면 70 은 움직이지만
	// "닿지 않는다"는 움직이면 안 된다 — 움직이는 순간 상위 피날레가 공짜가 된다.
	TestTrue(TEXT("3일 순이익으로 S 를 살 수 없다"),
		FFF_FinalePurchaseTestHelpers::ShortfallForS > 0);
	TestEqual(TEXT("기본 플레이로 S 는 못 산다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::S),
		EFF_FinalePurchaseResult::InsufficientGold);

	// A 는 사지고 남는다 — 반대쪽 부호도 같이 고정한다.
	// 둘 중 하나만 보면 "전부 못 산다"나 "전부 산다"로 무너져도 통과한다.
	TestTrue(TEXT("A 는 살 수 있는 예산이다"),
		FFF_FinalePurchaseTestHelpers::ThreeDayNetProfit > FFF_FinalePurchaseTestHelpers::CostA);
	TestEqual(TEXT("기본 플레이로 A 는 산다"),
		Harness.Progression->TryPurchaseFinale(EFF_FinaleGrade::A),
		EFF_FinalePurchaseResult::Success);
	TestEqual(TEXT("A 를 사고 남는 금액이 맞는다"), Harness.Progression->GetGold(),
		FFF_FinalePurchaseTestHelpers::ThreeDayNetProfit - FFF_FinalePurchaseTestHelpers::CostA);

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
