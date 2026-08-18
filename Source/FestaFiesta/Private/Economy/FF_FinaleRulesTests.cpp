// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Economy/FF_FinaleRules.h"

/**
 * 피날레 구매 판정 (S7). [참고] economy_spec.md §5.1 S7 · §3.1 · §3.4
 *
 * 순수 구조체라 GameInstance 도 Definition 도 세우지 않는다. 실제 구매(차감·원장·상태)는
 * FF_FinalePurchaseTests.cpp 가 본다 — 여기는 "살 수 있는가"만 고정한다.
 */
namespace FFF_FinaleRulesTestHelpers
{
	// [기획서] §10 값. 순서를 **일부러 섞어** 담는다 — 인덱스 의존을 잡기 위한 픽스처다.
	inline TArray<FFF_FinaleTier> MakeShuffledTiers()
	{
		return {
			{ EFF_FinaleGrade::S, 8000, 30 },
			{ EFF_FinaleGrade::C, 2000,  5 },
			{ EFF_FinaleGrade::A, 6000, 20 },
			{ EFF_FinaleGrade::B, 4000, 12 }
		};
	}
}

// 등급으로 찾는다. 배열 순서가 섞여도 맞는 티어가 나온다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinaleRules_FindsTierByGradeNotIndex, "FestaFiesta.Economy.FinaleRules.FindsTierByGradeNotIndex", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinaleRules_FindsTierByGradeNotIndex::RunTest(const FString& Parameters)
{
	const TArray<FFF_FinaleTier> Tiers = FFF_FinaleRulesTestHelpers::MakeShuffledTiers();

	// 픽스처가 의도대로 섞여 있는지 먼저 확인한다. 정렬돼 있으면 이 테스트가 아무것도 증명하지 않는다.
	TestNotEqual(TEXT("0번 원소가 C 가 아니다 — 인덱스와 등급이 다르다"),
		static_cast<int32>(Tiers[0].Grade), static_cast<int32>(EFF_FinaleGrade::C));

	const FFF_FinaleTier* CheapTier = FFF_FinaleRules::FindTier(Tiers, EFF_FinaleGrade::C);
	if (!TestNotNull(TEXT("C 등급을 찾는다"), CheapTier))
	{
		return false;
	}
	TestEqual(TEXT("C 비용이 2,000 이다"), CheapTier->CostGold, 2000);
	TestEqual(TEXT("C 보너스가 +5 다"), CheapTier->SatisfactionBonus, 5);

	const FFF_FinaleTier* TopTier = FFF_FinaleRules::FindTier(Tiers, EFF_FinaleGrade::S);
	if (!TestNotNull(TEXT("S 등급을 찾는다"), TopTier))
	{
		return false;
	}
	TestEqual(TEXT("S 비용이 8,000 이다"), TopTier->CostGold, 8000);

	// None 은 미구매 상태를 나타내는 값이라 목록에 없다.
	TestNull(TEXT("None 은 티어가 없다"),
		FFF_FinaleRules::FindTier(Tiers, EFF_FinaleGrade::None));

	return true;
}

// None 과 저작되지 않은 등급은 거부한다.
// 없는 등급을 0 으로 폴백하면 공짜 피날레가 조용히 생긴다 (§3.4 와 같은 fail-closed).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinaleRules_RejectsNoneAndUnauthoredGrade, "FestaFiesta.Economy.FinaleRules.RejectsNoneAndUnauthoredGrade", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinaleRules_RejectsNoneAndUnauthoredGrade::RunTest(const FString& Parameters)
{
	TArray<FFF_FinaleTier> Tiers = FFF_FinaleRulesTestHelpers::MakeShuffledTiers();
	FFF_FinaleTier OutTier;

	TestEqual(TEXT("None 으로는 살 수 없다"),
		FFF_FinaleRules::EvaluatePurchase(
			Tiers, EFF_FinaleGrade::None, EFF_FinaleGrade::None, 100000, OutTier),
		EFF_FinalePurchaseResult::InvalidGrade);

	// S 를 목록에서 빼면 저작되지 않은 등급이 된다.
	Tiers.RemoveAll([](const FFF_FinaleTier& Tier)
	{
		return Tier.Grade == EFF_FinaleGrade::S;
	});

	TestEqual(TEXT("저작되지 않은 등급은 돈이 넘쳐도 거부된다"),
		FFF_FinaleRules::EvaluatePurchase(
			Tiers, EFF_FinaleGrade::S, EFF_FinaleGrade::None, 100000, OutTier),
		EFF_FinalePurchaseResult::UnknownGrade);

	TestEqual(TEXT("목록이 비면 전부 거부된다"),
		FFF_FinaleRules::EvaluatePurchase(
			TArray<FFF_FinaleTier>(), EFF_FinaleGrade::C, EFF_FinaleGrade::None, 100000, OutTier),
		EFF_FinalePurchaseResult::UnknownGrade);

	// 실패했으면 OutTier 를 건드리지 않는다 — 호출자가 실패 후 값을 읽어도 0 이다.
	TestEqual(TEXT("실패 시 OutTier 가 채워지지 않는다"), OutTier.CostGold, 0);
	TestEqual(TEXT("실패 시 OutTier 등급이 None 이다"),
		OutTier.Grade, EFF_FinaleGrade::None);

	return true;
}

// 잔액 부족은 거부다. 비용과 잔액이 정확히 같으면 성공한다 (경계값).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinaleRules_RejectsWhenGoldIsShort, "FestaFiesta.Economy.FinaleRules.RejectsWhenGoldIsShort", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinaleRules_RejectsWhenGoldIsShort::RunTest(const FString& Parameters)
{
	const TArray<FFF_FinaleTier> Tiers = FFF_FinaleRulesTestHelpers::MakeShuffledTiers();
	FFF_FinaleTier OutTier;

	TestEqual(TEXT("1 Gold 부족하면 거부된다"),
		FFF_FinaleRules::EvaluatePurchase(
			Tiers, EFF_FinaleGrade::S, EFF_FinaleGrade::None, 7999, OutTier),
		EFF_FinalePurchaseResult::InsufficientGold);

	TestEqual(TEXT("비용과 잔액이 같으면 살 수 있다"),
		FFF_FinaleRules::EvaluatePurchase(
			Tiers, EFF_FinaleGrade::S, EFF_FinaleGrade::None, 8000, OutTier),
		EFF_FinalePurchaseResult::Success);

	// 성공했을 때만 OutTier 가 채워진다. 호출자가 이 값으로 차감한다.
	TestEqual(TEXT("성공 시 OutTier 에 비용이 담긴다"), OutTier.CostGold, 8000);
	TestEqual(TEXT("성공 시 OutTier 에 보너스가 담긴다"), OutTier.SatisfactionBonus, 30);

	return true;
}

// 구매는 축제당 1회다. 되돌릴 수 있으면 Finale 가 목표가 아니라 임시 저축이 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FinaleRules_RejectsSecondPurchase, "FestaFiesta.Economy.FinaleRules.RejectsSecondPurchase", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FinaleRules_RejectsSecondPurchase::RunTest(const FString& Parameters)
{
	const TArray<FFF_FinaleTier> Tiers = FFF_FinaleRulesTestHelpers::MakeShuffledTiers();
	FFF_FinaleTier OutTier;

	// A 를 이미 산 상태. 돈은 S 를 두 번 살 만큼 있다.
	TestEqual(TEXT("더 높은 등급으로 올릴 수 없다"),
		FFF_FinaleRules::EvaluatePurchase(
			Tiers, EFF_FinaleGrade::S, EFF_FinaleGrade::A, 100000, OutTier),
		EFF_FinalePurchaseResult::AlreadyPurchased);

	TestEqual(TEXT("더 낮은 등급으로 내릴 수도 없다"),
		FFF_FinaleRules::EvaluatePurchase(
			Tiers, EFF_FinaleGrade::C, EFF_FinaleGrade::A, 100000, OutTier),
		EFF_FinalePurchaseResult::AlreadyPurchased);

	TestEqual(TEXT("같은 등급을 다시 사는 것도 거부된다"),
		FFF_FinaleRules::EvaluatePurchase(
			Tiers, EFF_FinaleGrade::A, EFF_FinaleGrade::A, 100000, OutTier),
		EFF_FinalePurchaseResult::AlreadyPurchased);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
