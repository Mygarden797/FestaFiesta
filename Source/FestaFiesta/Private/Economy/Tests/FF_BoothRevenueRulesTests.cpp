// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Economy/Rules/FF_BoothRevenueRules.h"
#include "Festival/FF_FestivalDefinition.h"
#include <limits>

/**
 * 부스 이용 1회의 매출·수수료 판정을 검증한다 (S10). [참고] economy_spec.md §2.4 · §2.5
 *
 * 여기는 순수 함수만 본다 — 신호 수신과 원장 기록은 브리지 테스트가 갖는다.
 *
 * 이 파일이 지키는 것은 두 가지다.
 *   ① 객단가 표가 §2.4 에서 조용히 흘러가지 않는 것
 *   ② 반올림을 **파티당 한 번만** 하는 것 — 1인씩 반올림하면 §2.5 의 수수료 9,000 이 무너진다
 */
namespace FFF_BoothRevenueTestHelpers
{
	// economy_spec.md §2.4. 여기 값이 바뀌면 스펙과 데이터 애셋을 같이 고쳐야 한다.
	constexpr int32 RevenueFoodStall = 20;
	constexpr int32 RevenueMarketGoods = 25;
	constexpr int32 RevenueCraftExperience = 30;
	constexpr int32 RevenueActivityGame = 30;
	constexpr int32 RevenueNightlife = 40;

	// §1.1 3번. [밸런스]
	constexpr float CommissionRate = 0.12f;

	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	TMap<FName, int32> MakeBoothRevenueTable()
	{
		return {
			{ TEXT("FoodStall"),       RevenueFoodStall },
			{ TEXT("MarketGoods"),     RevenueMarketGoods },
			{ TEXT("CraftExperience"), RevenueCraftExperience },
			{ TEXT("ActivityGame"),    RevenueActivityGame },
			{ TEXT("Nightlife"),       RevenueNightlife },
		};
	}
}

// --- 표 조회 ---------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BoothRevenue_FindsAuthoredTokens, "FestaFiesta.Economy.BoothRevenue.FindsAuthoredTokens", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_BoothRevenue_FindsAuthoredTokens::RunTest(const FString& Parameters)
{
	using namespace FFF_BoothRevenueTestHelpers;
	const TMap<FName, int32> Table = MakeBoothRevenueTable();

	TestEqual(TEXT("음식 부스"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("FoodStall")), RevenueFoodStall);
	TestEqual(TEXT("기념품"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("MarketGoods")), RevenueMarketGoods);
	TestEqual(TEXT("공예 체험"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("CraftExperience")), RevenueCraftExperience);
	TestEqual(TEXT("체험 게임"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("ActivityGame")), RevenueActivityGame);
	TestEqual(TEXT("야간"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("Nightlife")), RevenueNightlife);

	return true;
}

// 표에 없는 토큰은 0 이다. 무료 시설과 값이 같으므로 이 함수로는 둘을 구분할 수 없다 —
// 네이밍 컨벤션 위반(오타)으로 부스가 조용히 무료가 되는 것이 이 설계의 유일한 fail-silent 경로이고,
// 그 진단은 호출부(브리지)의 Warning-once 가 갖는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BoothRevenue_UnknownTokenIsFree, "FestaFiesta.Economy.BoothRevenue.UnknownTokenIsFree", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_BoothRevenue_UnknownTokenIsFree::RunTest(const FString& Parameters)
{
	using namespace FFF_BoothRevenueTestHelpers;
	const TMap<FName, int32> Table = MakeBoothRevenueTable();

	// 무료 시설 — 애초에 키를 넣지 않았다.
	TestEqual(TEXT("휴식"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("Seating")), 0);
	TestEqual(TEXT("공연"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("Busking")), 0);

	// FName 비교는 대소문자를 구분하지 않는다. `Booth_Foodstall_*` 같은 대소문자 실수는
	// 여기까지 오지도 않고(파서가 정규 토큰으로 돌려준다) 와도 정상 조회된다 — 안전한 쪽이다.
	TestEqual(TEXT("대소문자 실수는 흡수된다"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("Foodstall")), RevenueFoodStall);

	// 흡수되지 않는 것은 토큰 자체가 다른 경우다. 이쪽이 실제 fail-silent 경로다.
	TestEqual(TEXT("스네이크 표기는 Utility AI 취향 카테고리 쪽이다"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("food_stall")), 0);
	TestEqual(TEXT("복수형 오타"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("FoodStalls")), 0);

	TestEqual(TEXT("빈 토큰"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, NAME_None), 0);

	return true;
}

// 애셋에 음수가 저작돼도 매출이 음수가 되지 않는다. 음수 매출은 원장 정합성을 깨뜨린다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BoothRevenue_NegativeAuthoredValueIsClamped, "FestaFiesta.Economy.BoothRevenue.NegativeAuthoredValueIsClamped", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_BoothRevenue_NegativeAuthoredValueIsClamped::RunTest(const FString& Parameters)
{
	const TMap<FName, int32> Table = { { TEXT("FoodStall"), -20 } };

	TestEqual(TEXT("음수 저작은 0 으로 막힌다"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("FoodStall")), 0);

	return true;
}

// --- 수수료 계산 -----------------------------------------------------------

/**
 * 🔴 이 파일에서 가장 중요한 테스트다.
 *
 * 인원을 먼저 곱하고 **마지막에 한 번만** 반올림한다. 1인씩 반올림해서 더하면
 * 4인 파티에서 이미 8 vs 10 으로 갈리고, 하루 결제 건수가 1,250 건 규모(500명 × 2.5회)라
 * §2.5 의 수수료 9,000 이 7,000 대로 내려앉는다. 부스 수수료는 §2.6 에서 Finale 를 사는
 * 성장 자금이라, 깎이면 S 등급(정확히 320 부족하게 맞춰 둔 것)이 도달 불가가 된다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BoothRevenue_RoundsOncePerParty, "FestaFiesta.Economy.BoothRevenue.RoundsOncePerParty", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_BoothRevenue_RoundsOncePerParty::RunTest(const FString& Parameters)
{
	using namespace FFF_BoothRevenueTestHelpers;

	// 20 × 4 × 0.12 = 9.6 → 10
	const int32 PartyAtOnce = FFF_BoothRevenueRules::ResolveCommission(RevenueFoodStall, 4, CommissionRate);
	TestEqual(TEXT("파티 단위로 한 번 계산한다"), PartyAtOnce, 10);

	// 1인씩이면 20 × 1 × 0.12 = 2.4 → 2 이고, 네 번 더해도 8 이다.
	const int32 PerMember = FFF_BoothRevenueRules::ResolveCommission(RevenueFoodStall, 1, CommissionRate);
	TestEqual(TEXT("1인 계산은 2 다"), PerMember, 2);
	TestNotEqual(TEXT("1인씩 더한 값과 달라야 한다 — 같아지면 계산 순서가 바뀐 것이다"), PartyAtOnce, PerMember * 4);

	// 다른 카테고리도 같은 규칙이다.
	TestEqual(TEXT("기념품 2인"), FFF_BoothRevenueRules::ResolveCommission(RevenueMarketGoods, 2, CommissionRate), 6);
	TestEqual(TEXT("야간 3인"), FFF_BoothRevenueRules::ResolveCommission(RevenueNightlife, 3, CommissionRate), 14);

	return true;
}

// 정확히 0.5 인 값은 올린다. 0.12 는 이진수로 정확히 떨어지지 않아 경계를 만들 수 없으므로
// 비율을 0.5 로 두고 규칙 자체만 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BoothRevenue_RoundsHalfUp, "FestaFiesta.Economy.BoothRevenue.RoundsHalfUp", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_BoothRevenue_RoundsHalfUp::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("12.5 는 13 이 된다"), FFF_BoothRevenueRules::ResolveCommission(25, 1, 0.5f), 13);
	TestEqual(TEXT("11.5 는 12 가 된다"), FFF_BoothRevenueRules::ResolveCommission(23, 1, 0.5f), 12);

	return true;
}

// 무료 시설은 이용해도 매출이 없다. 호출부가 AddRevenue 를 부르지 않게 하는 근거다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BoothRevenue_FreeFacilityEarnsNothing, "FestaFiesta.Economy.BoothRevenue.FreeFacilityEarnsNothing", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_BoothRevenue_FreeFacilityEarnsNothing::RunTest(const FString& Parameters)
{
	using namespace FFF_BoothRevenueTestHelpers;

	TestEqual(TEXT("객단가 0"), FFF_BoothRevenueRules::ResolveCommission(0, 6, CommissionRate), 0);
	TestEqual(TEXT("객단가 음수"), FFF_BoothRevenueRules::ResolveCommission(-20, 6, CommissionRate), 0);
	TestEqual(TEXT("수수료율 0"), FFF_BoothRevenueRules::ResolveCommission(RevenueFoodStall, 6, 0.f), 0);
	TestEqual(TEXT("수수료율 음수"), FFF_BoothRevenueRules::ResolveCommission(RevenueFoodStall, 6, -0.12f), 0);

	return true;
}

// 수수료율 1.0 이면 부스 총매출이 전부 들어온다. 상한이 막혀 있지 않은지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BoothRevenue_FullRateTakesGrossRevenue, "FestaFiesta.Economy.BoothRevenue.FullRateTakesGrossRevenue", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_BoothRevenue_FullRateTakesGrossRevenue::RunTest(const FString& Parameters)
{
	using namespace FFF_BoothRevenueTestHelpers;

	TestEqual(TEXT("20 × 4 전액"), FFF_BoothRevenueRules::ResolveCommission(RevenueFoodStall, 4, 1.f), 80);

	return true;
}

// 빈 파티는 ensure 로 막고 0 을 돌려준다. 파티 퇴장 신호가 인원 0 을 보내면 그쪽이 버그다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BoothRevenue_RejectsEmptyParty, "FestaFiesta.Economy.BoothRevenue.RejectsEmptyParty", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_BoothRevenue_RejectsEmptyParty::RunTest(const FString& Parameters)
{
	using namespace FFF_BoothRevenueTestHelpers;

	// ensureMsgf 는 배너·조건식·메시지·콜스택을 여러 줄로 나눠 찍고 실행마다 어느 줄이
	// Error 로 잡히는지가 달라진다. 그래서 개수를 세지 않고(0) 빈 패턴으로 전부 흡수한다.
	AddExpectedError(TEXT(""), EAutomationExpectedErrorFlags::Contains, 0);

	TestEqual(TEXT("인원 0"), FFF_BoothRevenueRules::ResolveCommission(RevenueFoodStall, 0, CommissionRate), 0);
	TestEqual(TEXT("인원 음수"), FFF_BoothRevenueRules::ResolveCommission(RevenueFoodStall, -3, CommissionRate), 0);

	return true;
}

// --- 데이터 애셋 드리프트 ---------------------------------------------------

// 생성자 기본값이 §2.4 표와 어긋나면 여기서 죽는다. DA_Festival_Busan 을 저작하지 않아도
// 게임이 정상 매출을 내는 근거이기도 하다 — 표가 비면 모든 부스가 조용히 무료가 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BoothRevenue_DefinitionDefaultsMatchSpec, "FestaFiesta.Economy.BoothRevenue.DefinitionDefaultsMatchSpec", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_BoothRevenue_DefinitionDefaultsMatchSpec::RunTest(const FString& Parameters)
{
	using namespace FFF_BoothRevenueTestHelpers;

	const UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>(GetTransientPackage());
	if (!TestNotNull(TEXT("정의를 만든다"), Definition))
	{
		return false;
	}

	const TMap<FName, int32>& Table = Definition->RevenuePerUseByMinorToken;

	TestEqual(TEXT("저작된 소분류는 5종이다"), Table.Num(), 5);
	TestEqual(TEXT("음식 부스"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("FoodStall")), RevenueFoodStall);
	TestEqual(TEXT("기념품"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("MarketGoods")), RevenueMarketGoods);
	TestEqual(TEXT("공예 체험"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("CraftExperience")), RevenueCraftExperience);
	TestEqual(TEXT("체험 게임"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("ActivityGame")), RevenueActivityGame);
	TestEqual(TEXT("야간"), FFF_BoothRevenueRules::FindRevenuePerUse(Table, TEXT("Nightlife")), RevenueNightlife);

	// 수수료율도 같은 표에서 온다 (§1.1 3번).
	TestEqual(TEXT("수수료율 12%"), Definition->BoothCommissionRate, CommissionRate);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BoothRevenue_UpgradeUsesFinalRounding, "FestaFiesta.Economy.BoothRevenue.UpgradeUsesFinalRounding", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_BoothRevenue_UpgradeUsesFinalRounding::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("1.5x multiplies the unrounded party commission"),
		FFF_BoothRevenueRules::ResolveCommission(20, 4, 0.12f, 1.5f), 14);
	TestEqual(TEXT("2.25x is absolute to base revenue"),
		FFF_BoothRevenueRules::ResolveCommission(20, 4, 0.12f, 2.25f), 22);
	TestEqual(TEXT("An exact 50 percent increase applies before rounding"),
		FFF_BoothRevenueRules::ResolveCommission(20, 1, 0.5f, 1.5f), 15);
	TestEqual(TEXT("A free facility stays free at any upgrade level"),
		FFF_BoothRevenueRules::ResolveCommission(0, 4, 0.12f, 2.25f), 0);
	TestEqual(TEXT("An authored revenue reduction applies without a design minimum"),
		FFF_BoothRevenueRules::ResolveCommission(20, 1, .5f, .5f), 5);
	TestEqual(TEXT("An authored zero multiplier intentionally earns nothing"),
		FFF_BoothRevenueRules::ResolveCommission(20, 1, .5f, 0.f), 0);
	TestEqual(TEXT("Large finite multipliers have no design maximum"),
		FFF_BoothRevenueRules::ResolveCommission(20, 1, .5f, 1000.f), 10000);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BoothRevenue_UpgradeRejectsNonFiniteAndSaturates, "FestaFiesta.Economy.BoothRevenue.UpgradeRejectsNonFiniteAndSaturates", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_BoothRevenue_UpgradeRejectsNonFiniteAndSaturates::RunTest(const FString& Parameters)
{
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Infinity = std::numeric_limits<float>::infinity();
	for (const float Invalid : { NaN, Infinity, -Infinity, -1.f })
	{
		TestEqual(TEXT("Invalid upgrade revenue produces no payout"),
			FFF_BoothRevenueRules::ResolveCommission(20, 4, 0.12f, Invalid), 0);
		TestEqual(TEXT("Invalid commission rate produces no payout"),
			FFF_BoothRevenueRules::ResolveCommission(20, 4, Invalid, 1.5f), 0);
	}
	TestEqual(TEXT("Large finite amounts saturate instead of overflowing"),
		FFF_BoothRevenueRules::ResolveCommission(MAX_int32, MAX_int32, 1.f, 100.f), MAX_int32);
	TestEqual(TEXT("A valid amount at the integer limit remains payable"),
		FFF_BoothRevenueRules::ResolveCommission(MAX_int32, 1, 1.f, 1.f), MAX_int32);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
