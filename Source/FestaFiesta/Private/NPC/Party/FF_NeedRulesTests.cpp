// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Party/FF_NeedRules.h"

/**
 * FFF_NeedRules 검증.
 *
 * 계수 로딩(LoadRiseRates/LoadReliefAmounts)은 DataTable 이 필요해 여기서 다루지 않는다 —
 * 테이블이 nullptr 이면 구조체 기본값을 그대로 쓴다는 것만 확인한다. 기본값과 CSV 값이
 * 어긋나는 사고는 이 테스트로 못 잡으므로, 값을 바꿀 때 두 곳을 같이 고쳐야 한다.
 */
namespace FFF_NeedRulesTestHelpers
{
	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	FFF_NeedRiseRates MakeNeedRulesTestRates(float HungerMinutes, float RestroomMinutes)
	{
		FFF_NeedRiseRates Rates;
		Rates.HungerMinutesToLimit = HungerMinutes;
		Rates.RestroomMinutesToLimit = RestroomMinutes;
		// P0 외 3종은 기본값 0(안 쌓임)을 유지한다
		return Rates;
	}

	FFF_NeedReliefFlags MakeNeedRulesTestFlags(bool bHunger, bool bRestroom)
	{
		FFF_NeedReliefFlags Flags;
		Flags.bRelievesHunger = bHunger;
		Flags.bRelievesRestroom = bRestroom;
		return Flags;
	}
}

using namespace FFF_NeedRulesTestHelpers;

// Need 는 "한계에 닿는 분"의 역수만큼 분당 쌓인다. 절반 시간이면 절반이 쌓여 있어야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_RisesProportionallyToElapsedTime, "FestaFiesta.NPC.NeedRules.RisesProportionallyToElapsedTime", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_RisesProportionallyToElapsedTime::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates Rates = MakeNeedRulesTestRates(/*Hunger=*/240.f, /*Restroom=*/150.f);
	const FFF_NeedState Fresh;

	const FFF_NeedState After120 = FFF_NeedRules::Advance(Fresh, 120.f, Rates);
	TestEqual(TEXT("240분이 한계면 120분에 0.5"), After120.Hunger, 0.5f, 0.001f);
	TestEqual(TEXT("150분이 한계면 120분에 0.8"), After120.Restroom, 0.8f, 0.001f);

	// 나눠서 불러도 결과가 같아야 한다 — L3 가 어떤 주기로 부를지 아직 안 정해졌기 때문이다
	FFF_NeedState Stepwise = Fresh;
	for (int32 Step = 0; Step < 4; ++Step)
	{
		Stepwise = FFF_NeedRules::Advance(Stepwise, 30.f, Rates);
	}
	TestEqual(TEXT("30분씩 4번 = 120분 한 번 (배고픔)"), Stepwise.Hunger, After120.Hunger, 0.001f);
	TestEqual(TEXT("30분씩 4번 = 120분 한 번 (화장실)"), Stepwise.Restroom, After120.Restroom, 0.001f);
	return true;
}

// 1.0 을 넘지 않는다. weight_need(1.8)가 weight_attraction+lambda_affinity(1.6)를 넘도록 잡은
// 계산이 "Need 최대치 1.0"을 전제하므로, 상한이 깨지면 그 부등식이 깨진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_ClampsAtOne, "FestaFiesta.NPC.NeedRules.ClampsAtOne", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_ClampsAtOne::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates Rates = MakeNeedRulesTestRates(/*Hunger=*/240.f, /*Restroom=*/150.f);

	// 한계 시간의 10배를 흘려도 1.0 을 넘지 않는다
	const FFF_NeedState Saturated = FFF_NeedRules::Advance(FFF_NeedState(), 2400.f, Rates);
	TestEqual(TEXT("배고픔은 1.0 을 넘지 않는다"), Saturated.Hunger, 1.0f, 0.001f);
	TestEqual(TEXT("화장실도 1.0 을 넘지 않는다"), Saturated.Restroom, 1.0f, 0.001f);

	// 이미 한계인 상태에서 더 흘려도 1.0 이다
	const FFF_NeedState StillOne = FFF_NeedRules::Advance(Saturated, 600.f, Rates);
	TestEqual(TEXT("포화 상태에서 더 흘려도 1.0"), StillOne.Hunger, 1.0f, 0.001f);
	return true;
}

// MinutesToLimit 이 0 인 Need 는 쌓이지 않는다. P0 에서 Hunger·Restroom 만 켜는 것이
// 코드 분기가 아니라 데이터 값이라는 뜻이다 — 켜는 것은 CSV 수정이면 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_ZeroRateNeedsStayFlat, "FestaFiesta.NPC.NeedRules.ZeroRateNeedsStayFlat", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_ZeroRateNeedsStayFlat::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates DefaultRates;
	TestEqual(TEXT("기본값에서 갈증은 꺼져 있다"), DefaultRates.ThirstMinutesToLimit, 0.f);
	TestEqual(TEXT("기본값에서 피로는 꺼져 있다"), DefaultRates.FatigueMinutesToLimit, 0.f);
	TestEqual(TEXT("기본값에서 자극은 꺼져 있다"), DefaultRates.StimulationMinutesToLimit, 0.f);
	TestEqual(TEXT("꺼진 Need 의 분당 증가량은 0 이다 — 0 으로 나누지 않는다"),
		DefaultRates.GetRisePerMinute(EFF_NeedType::Thirst), 0.f);

	const FFF_NeedState After = FFF_NeedRules::Advance(FFF_NeedState(), 600.f, DefaultRates);
	TestEqual(TEXT("갈증은 안 쌓인다(Phase 4)"), After.Thirst, 0.f);
	TestEqual(TEXT("피로는 안 쌓인다(Phase 4)"), After.Fatigue, 0.f);
	TestEqual(TEXT("자극은 안 쌓인다(Phase 2)"), After.Stimulation, 0.f);
	TestTrue(TEXT("반면 P0 두 개는 쌓인다"), After.Hunger > 0.f && After.Restroom > 0.f);

	// 음수 MinutesToLimit 도 안전하게 "꺼짐"으로 본다
	FFF_NeedRiseRates NegativeRates;
	NegativeRates.HungerMinutesToLimit = -100.f;
	const FFF_NeedState WithNegative = FFF_NeedRules::Advance(FFF_NeedState(), 60.f, NegativeRates);
	TestEqual(TEXT("음수 한계시간은 '꺼짐'이다 — Need 가 음수로 가지 않는다"), WithNegative.Hunger, 0.f);
	return true;
}

// 시간이 거꾸로 가면 아무것도 하지 않는다. 그대로 계산하면 Need 가 조용히 줄어들어
// "해소된 것"처럼 보이는데, 해소 경로는 Relieve 하나여야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_NegativeElapsedIsNoOp, "FestaFiesta.NPC.NeedRules.NegativeElapsedIsNoOp", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_NegativeElapsedIsNoOp::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates Rates = MakeNeedRulesTestRates(240.f, 150.f);
	const FFF_NeedState Start = FFF_NeedRules::Advance(FFF_NeedState(), 60.f, Rates);

	const FFF_NeedState Backwards = FFF_NeedRules::Advance(Start, -60.f, Rates);
	TestEqual(TEXT("음수 경과분은 배고픔을 바꾸지 않는다"), Backwards.Hunger, Start.Hunger);
	TestEqual(TEXT("음수 경과분은 화장실을 바꾸지 않는다"), Backwards.Restroom, Start.Restroom);

	const FFF_NeedState NoTime = FFF_NeedRules::Advance(Start, 0.f, Rates);
	TestEqual(TEXT("0분 경과도 그대로다"), NoTime.Hunger, Start.Hunger);
	return true;
}

// 해소는 플래그가 켜진 성분만 깎는다. 음식 부스가 화장실 욕구를 풀어주면 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_RelievesOnlyFlaggedComponents, "FestaFiesta.NPC.NeedRules.RelievesOnlyFlaggedComponents", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_RelievesOnlyFlaggedComponents::RunTest(const FString& Parameters)
{
	FFF_NeedState Hungry;
	Hungry.Hunger = 0.9f;
	Hungry.Restroom = 0.9f;

	const FFF_NeedReliefAmounts Amounts;  // Hunger 0.8 부분 해소 / Restroom 1.0 완전 해소

	// 음식만 해소하는 시설
	const FFF_NeedState AfterFood = FFF_NeedRules::Relieve(
		Hungry, MakeNeedRulesTestFlags(/*bHunger=*/true, /*bRestroom=*/false), Amounts);
	TestEqual(TEXT("배고픔은 0.9 - 0.8 = 0.1 로 부분 해소된다"), AfterFood.Hunger, 0.1f, 0.001f);
	TestEqual(TEXT("음식 부스는 화장실 욕구를 건드리지 않는다"), AfterFood.Restroom, 0.9f, 0.001f);

	// 화장실만 해소하는 시설
	const FFF_NeedState AfterToilet = FFF_NeedRules::Relieve(
		Hungry, MakeNeedRulesTestFlags(/*bHunger=*/false, /*bRestroom=*/true), Amounts);
	TestEqual(TEXT("화장실은 완전 해소된다"), AfterToilet.Restroom, 0.f, 0.001f);
	TestEqual(TEXT("화장실은 배고픔을 건드리지 않는다"), AfterToilet.Hunger, 0.9f, 0.001f);

	// 아무것도 해소하지 않는 시설(포토존 등)
	const FFF_NeedState AfterNothing = FFF_NeedRules::Relieve(
		Hungry, FFF_NeedReliefFlags(), Amounts);
	TestEqual(TEXT("해소 플래그가 없으면 그대로다"), AfterNothing.Hunger, 0.9f, 0.001f);
	return true;
}

// 해소는 0 아래로 내려가지 않는다. 음수 Need 는 "미래의 배고픔을 미리 갚아둔" 상태가 되어
// 다음 재평가에서 그 NPC 가 비정상적으로 오래 안 먹는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_ReliefNeverGoesNegative, "FestaFiesta.NPC.NeedRules.ReliefNeverGoesNegative", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_ReliefNeverGoesNegative::RunTest(const FString& Parameters)
{
	FFF_NeedState BarelyHungry;
	BarelyHungry.Hunger = 0.1f;   // 해소량 0.8 보다 작다

	const FFF_NeedState After = FFF_NeedRules::Relieve(
		BarelyHungry, MakeNeedRulesTestFlags(true, false), FFF_NeedReliefAmounts());
	TestEqual(TEXT("해소량보다 Need 가 작으면 0 이다(음수 아님)"), After.Hunger, 0.f);

	// 해소량 0 인 성분(Phase 대기)은 플래그가 켜져 있어도 안 깎는다
	FFF_NeedState Thirsty;
	Thirsty.Thirst = 0.7f;
	FFF_NeedReliefFlags ThirstFlags;
	ThirstFlags.bRelievesThirst = true;
	const FFF_NeedState AfterThirst = FFF_NeedRules::Relieve(Thirsty, ThirstFlags, FFF_NeedReliefAmounts());
	TestEqual(TEXT("해소량이 0 인 성분은 플래그가 켜져도 안 깎인다"), AfterThirst.Thirst, 0.7f, 0.001f);
	return true;
}

// 해소 → 재증가 순환이 한 번의 체류 안에서 돈다는 것만 고정한다.
// (주의) 이 시나리오에서는 해소량 0.8 과 1.0 의 결과가 같다 — Need 0.5 에서 둘 다 clamp 되어 0 이다.
// 부분 해소가 실제로 갈리는 경우는 HungryArrivalCanEatTwice 쪽이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_HungerRecursWithinOneVisit, "FestaFiesta.NPC.NeedRules.HungerRecursWithinOneVisit", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_HungerRecursWithinOneVisit::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates Rates;          // 기본값: 배고픔 240분
	const FFF_NeedReliefAmounts Amounts;    // 기본값: 배고픔 0.8 부분 해소

	// 240분(4시간) 체류하며 중간에 한 번 먹는 시나리오
	FFF_NeedState Needs = FFF_NeedRules::Advance(FFF_NeedState(), 120.f, Rates);  // 0.5
	Needs = FFF_NeedRules::Relieve(Needs, MakeNeedRulesTestFlags(true, false), Amounts);
	TestEqual(TEXT("0.5 에서 0.8 을 깎으면 0 이다"), Needs.Hunger, 0.f, 0.001f);

	// 남은 120분 동안 다시 0.5 까지 쌓인다 → 두 번째 식사 동기가 생긴다
	Needs = FFF_NeedRules::Advance(Needs, 120.f, Rates);
	TestTrue(TEXT("퇴장 전에 배고픔이 다시 쌓여 두 번째 식사 동기가 생긴다"), Needs.Hunger > 0.4f);

	// 만약 완전 해소(1.0)였다면 같은 시나리오에서 결과는 같지만, 한계에서 먹은 경우가 갈린다:
	// 한계(1.0)에서 0.8 을 깎으면 0.2 가 남아 재방문이 더 빨라진다. 완전 해소면 0 이 된다.
	FFF_NeedState AtLimit;
	AtLimit.Hunger = 1.0f;
	const FFF_NeedState PartialRelief = FFF_NeedRules::Relieve(
		AtLimit, MakeNeedRulesTestFlags(true, false), Amounts);
	TestEqual(TEXT("한계에서 부분 해소하면 잔여 배고픔이 남는다"), PartialRelief.Hunger, 0.2f, 0.001f);
	return true;
}

// 그룹 집계(성분별 최대)와 규칙이 함께 돌 때 한 명의 급한 욕구가 희석되지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_GroupAggregationKeepsUrgentMember, "FestaFiesta.NPC.NeedRules.GroupAggregationKeepsUrgentMember", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_GroupAggregationKeepsUrgentMember::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates Rates;

	// 같은 시간을 보냈지만 한 명만 아까 화장실을 다녀왔다
	FFF_NeedState MemberA = FFF_NeedRules::Advance(FFF_NeedState(), 140.f, Rates);
	FFF_NeedState MemberB = MemberA;
	MemberB = FFF_NeedRules::Relieve(MemberB, MakeNeedRulesTestFlags(false, true), FFF_NeedReliefAmounts());

	const FFF_NeedState Group = FFF_NeedState::MakeAggregated(MemberA, MemberB);
	TestEqual(TEXT("그룹 화장실 욕구는 안 다녀온 멤버 값이다(평균이 아니다)"),
		Group.Restroom, MemberA.Restroom, 0.001f);
	TestTrue(TEXT("한 명만 급해도 그룹이 급하다"), Group.Restroom > 0.9f);
	return true;
}

// 테이블이 없으면 구조체 기본값을 쓴다 — 테이블 없는 경로에서도 같은 행동이 나와야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_NullTableFallsBackToDefaults, "FestaFiesta.NPC.NeedRules.NullTableFallsBackToDefaults", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_NullTableFallsBackToDefaults::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates Rates = FFF_NeedRules::LoadRiseRates(nullptr);
	const FFF_NeedRiseRates DefaultRates;
	TestEqual(TEXT("테이블이 없으면 배고픔 기본값"), Rates.HungerMinutesToLimit, DefaultRates.HungerMinutesToLimit);
	TestEqual(TEXT("테이블이 없으면 화장실 기본값"), Rates.RestroomMinutesToLimit, DefaultRates.RestroomMinutesToLimit);

	const FFF_NeedReliefAmounts Amounts = FFF_NeedRules::LoadReliefAmounts(nullptr);
	const FFF_NeedReliefAmounts DefaultAmounts;
	TestEqual(TEXT("테이블이 없으면 배고픔 해소량 기본값"), Amounts.Hunger, DefaultAmounts.Hunger);
	TestEqual(TEXT("테이블이 없으면 화장실 해소량 기본값"), Amounts.Restroom, DefaultAmounts.Restroom);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 입장 시점 Need 초기값 흩뿌리기
// ─────────────────────────────────────────────────────────────────────────────

// 같은 개체는 항상 같은 초기값, 다른 개체는 다른 초기값. 개체 변주와 같은 결정성 요구다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_InitialNeedsAreDeterministicPerAgent, "FestaFiesta.NPC.NeedRules.InitialNeedsAreDeterministicPerAgent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_InitialNeedsAreDeterministicPerAgent::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates Rates;

	const FFF_NeedState First = FFF_NeedRules::MakeInitialNeeds(12345, /*AgentId=*/7, /*Gen=*/0, Rates);
	const FFF_NeedState Again = FFF_NeedRules::MakeInitialNeeds(12345, /*AgentId=*/7, /*Gen=*/0, Rates);
	TestEqual(TEXT("같은 개체는 배고픔 초기값이 같다"), Again.Hunger, First.Hunger);
	TestEqual(TEXT("같은 개체는 화장실 초기값이 같다"), Again.Restroom, First.Restroom);

	// 다른 개체는 최소한 하나는 달라야 한다 — 전원이 같으면 흩뿌리기가 안 되는 것이다
	bool bFoundDifferent = false;
	for (int32 AgentId = 1; AgentId <= 20 && !bFoundDifferent; ++AgentId)
	{
		const FFF_NeedState Other = FFF_NeedRules::MakeInitialNeeds(12345, AgentId, 0, Rates);
		bFoundDifferent = !FMath::IsNearlyEqual(Other.Hunger, First.Hunger, 0.0001f);
	}
	TestTrue(TEXT("개체마다 초기값이 갈린다"), bFoundDifferent);

	// 전역 시드가 다르면 같은 개체라도 달라진다(다른 회차 시뮬레이션)
	const FFF_NeedState OtherSeed = FFF_NeedRules::MakeInitialNeeds(999, 7, 0, Rates);
	TestFalse(TEXT("전역 시드가 다르면 초기값도 다르다"),
		FMath::IsNearlyEqual(OtherSeed.Hunger, First.Hunger, 0.0001f)
		&& FMath::IsNearlyEqual(OtherSeed.Restroom, First.Restroom, 0.0001f));
	return true;
}

// 초기값은 [0, MaxFraction] 안에 있고, 성분별로 독립이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_InitialNeedsStayWithinBounds, "FestaFiesta.NPC.NeedRules.InitialNeedsStayWithinBounds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_InitialNeedsStayWithinBounds::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates Rates;
	constexpr float MaxFraction = 0.50f;

	int32 IdenticalPairCount = 0;
	for (int32 AgentId = 1; AgentId <= 200; ++AgentId)
	{
		const FFF_NeedState Needs = FFF_NeedRules::MakeInitialNeeds(4242, AgentId, 0, Rates);

		if (Needs.Hunger < 0.f || Needs.Hunger > MaxFraction + 0.0001f)
		{
			AddError(FString::Printf(TEXT("배고픔 초기값이 범위를 벗어났다: %f (Agent %d)"), Needs.Hunger, AgentId));
			break;
		}
		if (Needs.Restroom < 0.f || Needs.Restroom > MaxFraction + 0.0001f)
		{
			AddError(FString::Printf(TEXT("화장실 초기값이 범위를 벗어났다: %f (Agent %d)"), Needs.Restroom, AgentId));
			break;
		}
		// 입장 즉시 한계인 개체가 나오면 입장 직후 화장실 직행이 생긴다 — 상한 0.5의 목적이다
		if (Needs.Restroom >= 1.f)
		{
			AddError(FString::Printf(TEXT("입장 즉시 한계인 개체가 생겼다 (Agent %d)"), AgentId));
			break;
		}
		if (FMath::IsNearlyEqual(Needs.Hunger, Needs.Restroom, 0.0001f))
		{
			++IdenticalPairCount;
		}
	}

	// 성분별 독립 스트림이므로 배고픔과 화장실이 같은 값인 개체는 거의 없어야 한다.
	// 상관을 넣을 근거가 없어서 독립으로 둔 것이고, 같은 시드를 재사용하면 이 테스트가 깨진다.
	TestTrue(TEXT("배고픔과 화장실은 독립된 난수 스트림이다"), IdenticalPairCount <= 2);
	return true;
}

// 꺼진 Need 는 0 에서 시작한다. 안 쌓이는데 초기값이 있으면 영원히 그 값에 머문다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_DisabledNeedsStartAtZero, "FestaFiesta.NPC.NeedRules.DisabledNeedsStartAtZero", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_DisabledNeedsStartAtZero::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates Rates;  // Thirst/Fatigue/Stimulation 은 0(꺼짐)

	for (int32 AgentId = 1; AgentId <= 30; ++AgentId)
	{
		const FFF_NeedState Needs = FFF_NeedRules::MakeInitialNeeds(777, AgentId, 0, Rates);
		if (Needs.Thirst != 0.f || Needs.Fatigue != 0.f || Needs.Stimulation != 0.f)
		{
			AddError(FString::Printf(TEXT("꺼진 Need 에 초기값이 들어갔다 (Agent %d)"), AgentId));
			return false;
		}
	}

	// 켜면 초기값도 생긴다
	FFF_NeedRiseRates WithThirst;
	WithThirst.ThirstMinutesToLimit = 180.f;
	bool bAnyThirst = false;
	for (int32 AgentId = 1; AgentId <= 30 && !bAnyThirst; ++AgentId)
	{
		bAnyThirst = FFF_NeedRules::MakeInitialNeeds(777, AgentId, 0, WithThirst).Thirst > 0.f;
	}
	TestTrue(TEXT("Need 를 켜면 초기값도 흩뿌려진다"), bAnyThirst);
	return true;
}

// 상한 0 은 흩뿌리기를 끈 것이다. Mode > Max 같은 잘못된 입력도 안전해야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_InitialScatterCanBeDisabled, "FestaFiesta.NPC.NeedRules.InitialScatterCanBeDisabled", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_InitialScatterCanBeDisabled::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates Rates;

	const FFF_NeedState Off = FFF_NeedRules::MakeInitialNeeds(
		1, 1, 0, Rates, /*Mode=*/0.15f, /*Max=*/0.f);
	TestEqual(TEXT("상한 0 이면 배고픔은 0 이다"), Off.Hunger, 0.f);
	TestEqual(TEXT("상한 0 이면 화장실도 0 이다"), Off.Restroom, 0.f);

	// Mode 가 Max 보다 큰 잘못된 입력 — clamp 해서 범위 안에 있어야 한다
	const FFF_NeedState Weird = FFF_NeedRules::MakeInitialNeeds(
		1, 1, 0, Rates, /*Mode=*/0.9f, /*Max=*/0.3f);
	TestTrue(TEXT("Mode > Max 여도 범위 안이다"), Weird.Hunger >= 0.f && Weird.Hunger <= 0.3f + 0.0001f);

	// 상한이 1.0 을 넘어도 Need 는 1.0 을 넘지 않는다
	const FFF_NeedState Overflow = FFF_NeedRules::MakeInitialNeeds(
		1, 1, 0, Rates, /*Mode=*/0.5f, /*Max=*/5.f);
	TestTrue(TEXT("상한이 1.0 을 넘어도 Need 는 1.0 이하다"), Overflow.Hunger <= 1.f);
	return true;
}

// 흩뿌리기 + 부분 해소가 맞물려 "배고픈 채 입장한 개체"의 2회 식사를 만든다.
// need_hunger_relief_amount 를 1.0 으로 올리면 이 테스트가 깨진다 — 그게 0.8 을 두는 유일한 이유다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_HungryArrivalCanEatTwice, "FestaFiesta.NPC.NeedRules.HungryArrivalCanEatTwice", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_HungryArrivalCanEatTwice::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates Rates;        // 배고픔 240분
	const FFF_NeedReliefAmounts Amounts;  // 배고픔 0.8 부분 해소
	const FFF_NeedReliefFlags FoodStall = MakeNeedRulesTestFlags(/*bHunger=*/true, /*bRestroom=*/false);

	// 배고픔 0.5 로 입장한 개체 (흩뿌리기 상한이 0.5라 실제로 나올 수 있는 값이다)
	FFF_NeedState Hungry;
	Hungry.Hunger = 0.5f;

	// 120분 뒤 한계에 닿는다 → 먹는다
	Hungry = FFF_NeedRules::Advance(Hungry, 120.f, Rates);
	TestEqual(TEXT("배고픔 0.5 로 입장하면 120분에 한계다"), Hungry.Hunger, 1.0f, 0.001f);
	Hungry = FFF_NeedRules::Relieve(Hungry, FoodStall, Amounts);
	TestEqual(TEXT("한계에서 부분 해소하면 0.2 가 남는다"), Hungry.Hunger, 0.2f, 0.001f);

	// 남은 120분 — 잔여 0.2 에서 다시 오르므로 0.7 에 닿는다
	Hungry = FFF_NeedRules::Advance(Hungry, 120.f, Rates);
	TestTrue(TEXT("잔여가 남아 퇴장 전 두 번째 식사 동기가 생긴다"), Hungry.Hunger > 0.6f);

	// 대조군: 0 에서 입장한 개체는 같은 240분 동안 한 번만 먹는다
	FFF_NeedState Fresh;
	Fresh = FFF_NeedRules::Advance(Fresh, 120.f, Rates);
	Fresh = FFF_NeedRules::Relieve(Fresh, FoodStall, Amounts);
	TestEqual(TEXT("0 에서 입장한 개체는 해소량이 0.8이든 1.0이든 0 이 된다(clamp)"), Fresh.Hunger, 0.f, 0.001f);
	Fresh = FFF_NeedRules::Advance(Fresh, 120.f, Rates);
	TestTrue(TEXT("그 개체는 퇴장 시점에도 두 번째 식사 동기가 약하다"), Fresh.Hunger < Hungry.Hunger);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// ConsumeStay — Need 로 체류시간을 깎는 규칙 (사용자 확정, 2026-08-11)
// ─────────────────────────────────────────────────────────────────────────────

// 기본값 1.0 은 "기능 꺼짐"이다. 근거 있는 계수가 없어서 중립값을 기본으로 뒀으므로,
// 이 테스트가 깨지면 누군가 그럴듯한 상수를 기본값으로 넣었다는 뜻이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_StayDrainIsOffByDefault, "FestaFiesta.NPC.NeedRules.StayDrainIsOffByDefault", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_StayDrainIsOffByDefault::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("테이블 없으면 배수는 중립값 1.0 (기능 꺼짐)"),
		FFF_NeedRules::LoadStayDrainMultiplierAtMaxNeed(nullptr), 1.0f, 0.001f);

	// 한계까지 쌓인 Need 라도 배수가 1.0 이면 경과분 그대로만 소모한다
	FFF_NeedState AtLimit;
	AtLimit.Restroom = 1.0f;
	const FFF_StayConsumption Consumed = FFF_NeedRules::ConsumeStay(AtLimit, 30.f, 1.0f);
	TestEqual(TEXT("배수 1.0 이면 경과분만 소모한다"), Consumed.ConsumedStayBudgetMinutes, 30.f, 0.001f);
	TestEqual(TEXT("배수 1.0 이면 Need 몫은 0 이다"), Consumed.ExtraStayBudgetFromNeed, 0.f, 0.001f);
	return true;
}

// 한계에서 배수만큼, 중간이면 선형으로 비례한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_StayDrainScalesLinearlyWithUrgency, "FestaFiesta.NPC.NeedRules.StayDrainScalesLinearlyWithUrgency", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_StayDrainScalesLinearlyWithUrgency::RunTest(const FString& Parameters)
{
	// Restroom 은 need_priority 가 1.0 고정이라 Need 값이 곧 급함이다 — 배수 검산에 쓰기 좋다
	FFF_NeedState Needs;

	Needs.Restroom = 0.f;
	TestEqual(TEXT("Need 0 이면 배수 1.0"),
		FFF_NeedRules::ConsumeStay(Needs, 10.f, 2.0f).ConsumedStayBudgetMinutes, 10.f, 0.001f);

	Needs.Restroom = 0.5f;
	const FFF_StayConsumption Half = FFF_NeedRules::ConsumeStay(Needs, 10.f, 2.0f);
	TestEqual(TEXT("Need 0.5 면 배수 1.5"), Half.ConsumedStayBudgetMinutes, 15.f, 0.001f);
	TestEqual(TEXT("Need 몫은 소모분 - 경과분이다"), Half.ExtraStayBudgetFromNeed, 5.f, 0.001f);

	Needs.Restroom = 1.0f;
	TestEqual(TEXT("Need 한계면 배수가 계수값 그대로"),
		FFF_NeedRules::ConsumeStay(Needs, 10.f, 2.0f).ConsumedStayBudgetMinutes, 20.f, 0.001f);
	return true;
}

// 성분별 급함(need_priority_*)이 반영된다. Stimulation 은 0.5 라 한계여도 절반만 작용한다 —
// 생리적 욕구가 아닌 성분이 이탈을 밀어붙이지 않게 하려고 의도적으로 낮게 잡은 값이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_StayDrainRespectsPriorityWeights, "FestaFiesta.NPC.NeedRules.StayDrainRespectsPriorityWeights", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_StayDrainRespectsPriorityWeights::RunTest(const FString& Parameters)
{
	FFF_NeedState Bored;
	Bored.Stimulation = 1.0f;

	// 배수 3.0, 급함 0.5 → 1 + (3-1)*0.5 = 2.0
	TestEqual(TEXT("Stimulation 한계는 급함 0.5 로 환산된다"),
		FFF_NeedRules::ConsumeStay(Bored, 10.f, 3.0f).ConsumedStayBudgetMinutes, 20.f, 0.001f);

	FFF_NeedState Bursting;
	Bursting.Restroom = 1.0f;
	TestTrue(TEXT("같은 한계값이면 화장실이 지루함보다 이탈을 더 밀어붙인다"),
		FFF_NeedRules::ConsumeStay(Bursting, 10.f, 3.0f).ConsumedStayBudgetMinutes >
		FFF_NeedRules::ConsumeStay(Bored, 10.f, 3.0f).ConsumedStayBudgetMinutes);
	return true;
}

// 합이 아니라 최대값이다. 안 급한 욕구 여러 개가 한계에 닿은 욕구 하나를 이기면 안 된다 —
// GetMaxUrgency 와 같은 이유이고, 합으로 바꾸면 성분을 추가할 때마다 이탈률이 조용히 올라간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_StayDrainUsesMaxNotSum, "FestaFiesta.NPC.NeedRules.StayDrainUsesMaxNotSum", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_StayDrainUsesMaxNotSum::RunTest(const FString& Parameters)
{
	FFF_NeedState Spread;
	Spread.Hunger = 0.3f;
	Spread.Thirst = 0.3f;
	Spread.Restroom = 0.3f;
	Spread.Fatigue = 0.3f;
	Spread.Stimulation = 0.3f;

	// 합이면 급함이 1.0 을 넘어 clamp 되어 최대 배수가 나온다. 최대값이면 0.3(Restroom) 이 이긴다.
	const float Consumed = FFF_NeedRules::ConsumeStay(Spread, 10.f, 2.0f).ConsumedStayBudgetMinutes;
	TestEqual(TEXT("0.3 이 다섯 개면 급함은 0.3 이다 (합이 아니다)"), Consumed, 13.f, 0.001f);
	TestTrue(TEXT("한계 하나짜리보다 반드시 덜 깎인다"), Consumed < 20.f);
	return true;
}

// 1.0 아래 계수는 "Need 가 쌓이면 더 오래 머문다"는 정반대 정책이다. clamp 로 막는다 —
// CSV 오타로 0.5 가 들어갔을 때 체류시간이 늘어나는 사고를 조용히 통과시키지 않기 위한 것이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_StayDrainNeverExtendsStay, "FestaFiesta.NPC.NeedRules.StayDrainNeverExtendsStay", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_StayDrainNeverExtendsStay::RunTest(const FString& Parameters)
{
	FFF_NeedState AtLimit;
	AtLimit.Restroom = 1.0f;

	TestEqual(TEXT("배수 0.5 는 1.0 으로 clamp 된다"),
		FFF_NeedRules::ConsumeStay(AtLimit, 10.f, 0.5f).ConsumedStayBudgetMinutes, 10.f, 0.001f);
	TestEqual(TEXT("배수 0 도 1.0 으로 clamp 된다"),
		FFF_NeedRules::ConsumeStay(AtLimit, 10.f, 0.f).ConsumedStayBudgetMinutes, 10.f, 0.001f);
	TestEqual(TEXT("음수 배수도 1.0 으로 clamp 된다"),
		FFF_NeedRules::ConsumeStay(AtLimit, 10.f, -2.f).ConsumedStayBudgetMinutes, 10.f, 0.001f);
	return true;
}

// 음수 경과는 아무것도 소모하지 않는다. Advance 와 같은 규칙 — 그대로 계산하면 체류시간이
// 늘어나 "퇴장이 미뤄지는" 조용한 사고가 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_StayDrainNegativeElapsedIsNoOp, "FestaFiesta.NPC.NeedRules.StayDrainNegativeElapsedIsNoOp", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_StayDrainNegativeElapsedIsNoOp::RunTest(const FString& Parameters)
{
	FFF_NeedState AtLimit;
	AtLimit.Restroom = 1.0f;

	const FFF_StayConsumption Negative = FFF_NeedRules::ConsumeStay(AtLimit, -30.f, 2.0f);
	TestEqual(TEXT("음수 경과는 소모 0"), Negative.ConsumedStayBudgetMinutes, 0.f, 0.001f);
	TestEqual(TEXT("음수 경과는 Need 몫도 0"), Negative.ExtraStayBudgetFromNeed, 0.f, 0.001f);

	const FFF_StayConsumption Zero = FFF_NeedRules::ConsumeStay(AtLimit, 0.f, 2.0f);
	TestEqual(TEXT("0 경과는 소모 0"), Zero.ConsumedStayBudgetMinutes, 0.f, 0.001f);
	return true;
}

// 통합 성격 — 해소 못 한 개체가 실제로 더 일찍 나가는가.
// 화장실을 못 간 개체와 제때 간 개체를 같은 체류시간으로 출발시켜 소진 시각을 비교한다.
// 퇴장 게이트를 늘리지 않고도 "해소 실패 → 이탈"이 표현되는지가 이 결정의 핵심이었다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NeedRules_UnrelievedAgentLeavesEarlier, "FestaFiesta.NPC.NeedRules.UnrelievedAgentLeavesEarlier", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NeedRules_UnrelievedAgentLeavesEarlier::RunTest(const FString& Parameters)
{
	const FFF_NeedRiseRates Rates = MakeNeedRulesTestRates(/*Hunger=*/240.f, /*Restroom=*/150.f);
	const FFF_NeedReliefFlags Toilet = MakeNeedRulesTestFlags(/*bHunger=*/false, /*bRestroom=*/true);
	const FFF_NeedReliefAmounts Amounts;
	const float DrainMultiplier = 2.0f;
	const float StepMinutes = 10.f;

	struct FSimResult
	{
		// 실제로 흐른 분. 체류 예산과 다른 값이다 — 예산은 이보다 빨리 빠진다
		float MinutesUntilExit = 0.f;
		float ExtraStayBudgetFromNeed = 0.f;
	};

	// 30분마다 화장실을 해결하는 개체 vs 한 번도 못 가는 개체
	const auto SimulateRemainingStay = [&](bool bRelievePeriodically) -> FSimResult
	{
		FFF_NeedState Needs;
		float RemainingStay = 240.f;
		float ExtraFromNeed = 0.f;
		float ElapsedTotal = 0.f;

		while (RemainingStay > 0.f)
		{
			Needs = FFF_NeedRules::Advance(Needs, StepMinutes, Rates);
			ElapsedTotal += StepMinutes;

			if (bRelievePeriodically && FMath::IsNearlyZero(FMath::Fmod(ElapsedTotal, 30.f)))
			{
				Needs = FFF_NeedRules::Relieve(Needs, Toilet, Amounts);
			}

			const FFF_StayConsumption Consumption =
				FFF_NeedRules::ConsumeStay(Needs, StepMinutes, DrainMultiplier);
			RemainingStay -= Consumption.ConsumedStayBudgetMinutes;
			ExtraFromNeed += Consumption.ExtraStayBudgetFromNeed;
		}

		FSimResult Result;
		Result.MinutesUntilExit = ElapsedTotal;
		Result.ExtraStayBudgetFromNeed = ExtraFromNeed;
		return Result;
	};

	const FSimResult Relieved = SimulateRemainingStay(/*bRelievePeriodically=*/true);
	const FSimResult Unrelieved = SimulateRemainingStay(/*bRelievePeriodically=*/false);

	TestTrue(TEXT("해소 못 한 개체가 더 일찍 퇴장한다"),
		Unrelieved.MinutesUntilExit < Relieved.MinutesUntilExit);
	TestTrue(TEXT("해소 못 한 개체의 Need 몫 단축분이 더 크다"),
		Unrelieved.ExtraStayBudgetFromNeed > Relieved.ExtraStayBudgetFromNeed);
	TestTrue(TEXT("해소한 개체도 배고픔은 쌓이므로 240분보다는 일찍 나간다"),
		Relieved.MinutesUntilExit < 240.f);

	// 체류시간은 시계가 아니라 예산이다 — 예산 240 을 배수 2.0 으로 쓰면 실제로는 그 절반 만큼만
	// 머문다. 이 관계를 못 지키면 "ConsumedStayBudgetMinutes 가 경과분보다 크다"가 모순이 된다.
	TestTrue(TEXT("예산 240 을 배수 2.0 으로 쓰면 실제 체류는 120분에 가까워진다(하한)"),
		Unrelieved.MinutesUntilExit >= 120.f);
	TestTrue(TEXT("한계 Need 가 유지되면 실제 체류가 예산의 절반 근처까지 줄어든다"),
		Unrelieved.MinutesUntilExit <= 180.f);

	// 배수가 꺼져 있으면 둘 다 정확히 체류시간만큼 머문다 — 기능을 켜야만 차이가 생긴다
	const FFF_StayConsumption Off = FFF_NeedRules::ConsumeStay(FFF_NeedState(), StepMinutes, 1.0f);
	TestEqual(TEXT("기능이 꺼져 있으면 Need 와 무관하게 경과분만 소모"), Off.ConsumedStayBudgetMinutes, StepMinutes, 0.001f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
