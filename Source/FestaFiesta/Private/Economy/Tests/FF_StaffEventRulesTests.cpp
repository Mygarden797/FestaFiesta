// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Economy/Rules/FF_StaffEventRules.h"

/**
 * 사건 판정 순수 함수 검증. 예시 수치는
 * data/metadata/SiriKim_Docs/staff_event_design.md §4 "대형 화재" 표를 그대로 쓴다
 * (최소 인원 3, 작업량 12, 제한 3분=180초).
 *
 * 순수 함수라 GameInstance 도 사건 시스템도 세우지 않는다 — 이정원 님의
 * UFF_FestivalEventSubsystem 이 실제로 사건을 만들기 전에도 이 계산만 따로 검증된다.
 */
namespace FFF_StaffEventRulesTestHelpers
{
	inline FFF_StaffMember MakeMember(int32 Level, int32 HiredOnDay = 0, int32 LastPromotedDay = INDEX_NONE)
	{
		FFF_StaffMember Member;
		Member.StaffId = 1;
		Member.TrainingLevel = Level;
		Member.HiredOnDay = HiredOnDay;
		Member.LastPromotedDay = LastPromotedDay;
		return Member;
	}

	inline FFF_FestivalEventCard MakeLargeFireCard()
	{
		FFF_FestivalEventCard Card;
		Card.MinStaff = 3;
		Card.MaxStaff = 5;
		Card.WorkAmount = 12.f;
		Card.TimeLimitSeconds = 180.f;
		Card.MaxOverduePenalty = 100;
		return Card;
	}
}

// 정원(하한 자리)까지는 레벨 그대로, 넘는 자리는 절반만 센다 — 문서 표의 다섯 조합 그대로.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffEventRules_AbilitySumWeightsExcessStaffAtHalf, "FestaFiesta.Economy.StaffEventRules.AbilitySumWeightsExcessStaffAtHalf", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StaffEventRules_AbilitySumWeightsExcessStaffAtHalf::RunTest(const FString& Parameters)
{
	using namespace FFF_StaffEventRulesTestHelpers;
	constexpr int32 MinStaff = 3;

	TestEqual(TEXT("신입 셋 = 3.0"),
		FFF_StaffEventRules::CalcAbilitySum({ MakeMember(1), MakeMember(1), MakeMember(1) }, MinStaff),
		3.0f);

	TestEqual(TEXT("신입 넷 = 3.5 (3명 온전 + 1명 절반)"),
		FFF_StaffEventRules::CalcAbilitySum(
			{ MakeMember(1), MakeMember(1), MakeMember(1), MakeMember(1) }, MinStaff),
		3.5f);

	TestEqual(TEXT("신입 다섯(정원 꽉 채움) = 4.0 (3명 온전 + 2명 절반)"),
		FFF_StaffEventRules::CalcAbilitySum(
			{ MakeMember(1), MakeMember(1), MakeMember(1), MakeMember(1), MakeMember(1) }, MinStaff),
		4.0f);

	TestEqual(TEXT("숙련자 셋(레벨2) = 6.0"),
		FFF_StaffEventRules::CalcAbilitySum({ MakeMember(2), MakeMember(2), MakeMember(2) }, MinStaff),
		6.0f);

	// 신입 셋 + 숙련자 하나(4명, 정원 초과 1명) — 낮은 레벨을 먼저 넣어도 내부에서 내림차순
	// 정렬해 레벨3이 하한 자리를 먼저 차지해야 한다. 안 그러면(입력 순서 그대로 자르면)
	// 레벨3 이 초과분으로 밀려나 4.5 가 나온다 — 정렬을 안 하면 이 값이 5.5 대신 4.5 로 나와서
	// 이 테스트가 잡아낸다.
	TestEqual(TEXT("호출 순서와 무관하게 레벨 높은 사람이 하한 자리를 먼저 차지한다"),
		FFF_StaffEventRules::CalcAbilitySum(
			{ MakeMember(1), MakeMember(1), MakeMember(1), MakeMember(3) }, MinStaff),
		5.5f);

	return true;
}

// 소요시간 = 작업량 ÷ 능력치 합. 문서 표의 네 결과와 대조한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffEventRules_ResolveSecondsMatchesWorkedExample, "FestaFiesta.Economy.StaffEventRules.ResolveSecondsMatchesWorkedExample", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StaffEventRules_ResolveSecondsMatchesWorkedExample::RunTest(const FString& Parameters)
{
	using namespace FFF_StaffEventRulesTestHelpers;
	const FFF_FestivalEventCard Card = MakeLargeFireCard();

	TestEqual(TEXT("신입 셋(능력 3.0) = 4분(240초), 제한 초과"),
		FFF_StaffEventRules::CalcResolveSeconds(Card, 3.0f), 240.0f, 0.01f);
	TestTrue(TEXT("240초는 제한(180초) 초과다"),
		FFF_StaffEventRules::CalcResolveSeconds(Card, 3.0f) > Card.TimeLimitSeconds);

	TestEqual(TEXT("신입 다섯(능력 4.0) = 3분(180초), 딱 맞음"),
		FFF_StaffEventRules::CalcResolveSeconds(Card, 4.0f), 180.0f, 0.01f);

	TestEqual(TEXT("숙련자 셋(능력 6.0) = 2분(120초), 여유"),
		FFF_StaffEventRules::CalcResolveSeconds(Card, 6.0f), 120.0f, 0.01f);

	TestTrue(TEXT("능력치 합이 0이면 끝낼 수 없다(최댓값)"),
		FFF_StaffEventRules::CalcResolveSeconds(Card, 0.f) > 1'000'000.f);

	return true;
}

// 승급은 고용 당일 불가, 하루 한 번만, 레벨 3이 상한이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffEventRules_CanPromoteGatesOnHireDayAndOncePerDay, "FestaFiesta.Economy.StaffEventRules.CanPromoteGatesOnHireDayAndOncePerDay", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StaffEventRules_CanPromoteGatesOnHireDayAndOncePerDay::RunTest(const FString& Parameters)
{
	using namespace FFF_StaffEventRulesTestHelpers;

	const FFF_StaffMember HiredToday = MakeMember(1, /*HiredOnDay*/ 2);
	TestFalse(TEXT("고용한 바로 그날은 승급 못 한다"),
		FFF_StaffEventRules::CanPromote(HiredToday, /*CurrentDay*/ 2));
	TestTrue(TEXT("다음 날 아침부터는 승급 가능하다"),
		FFF_StaffEventRules::CanPromote(HiredToday, /*CurrentDay*/ 3));

	const FFF_StaffMember PromotedToday = MakeMember(2, /*HiredOnDay*/ 1, /*LastPromotedDay*/ 3);
	TestFalse(TEXT("같은 날 두 번 승급 못 한다"),
		FFF_StaffEventRules::CanPromote(PromotedToday, /*CurrentDay*/ 3));
	TestTrue(TEXT("다음 날엔 다시 승급 가능하다"),
		FFF_StaffEventRules::CanPromote(PromotedToday, /*CurrentDay*/ 4));

	const FFF_StaffMember MaxLevel = MakeMember(3, /*HiredOnDay*/ 0);
	TestFalse(TEXT("레벨 3은 더 못 올린다"),
		FFF_StaffEventRules::CanPromote(MaxLevel, /*CurrentDay*/ 5));

	const FFF_StaffMember BaseTeamDay1 = MakeMember(1, /*HiredOnDay*/ 0);
	TestTrue(TEXT("기본 팀은 Day1 부터 승급 가능하다(HiredOnDay 0)"),
		FFF_StaffEventRules::CanPromote(BaseTeamDay1, /*CurrentDay*/ 1));

	return true;
}

// 페널티 = 최대치 x (1 - 진행률), 진행률은 0~1로 잘린다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffEventRules_OverduePenaltyScalesWithProgress, "FestaFiesta.Economy.StaffEventRules.OverduePenaltyScalesWithProgress", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StaffEventRules_OverduePenaltyScalesWithProgress::RunTest(const FString& Parameters)
{
	using namespace FFF_StaffEventRulesTestHelpers;
	const FFF_FestivalEventCard Card = MakeLargeFireCard(); // MaxOverduePenalty = 100

	TestEqual(TEXT("착수도 못 했으면(진행률 0) 페널티는 최대치 그대로"),
		FFF_StaffEventRules::CalcOverduePenalty(Card, 0.f), 100);
	TestEqual(TEXT("절반 끌고 갔으면 절반만 문다"),
		FFF_StaffEventRules::CalcOverduePenalty(Card, 0.5f), 50);
	TestEqual(TEXT("90%까지 갔으면 10%만 문다"),
		FFF_StaffEventRules::CalcOverduePenalty(Card, 0.9f), 10);
	TestEqual(TEXT("진행률이 음수로 들어와도 0으로 잘려 최대치를 문다"),
		FFF_StaffEventRules::CalcOverduePenalty(Card, -0.5f), 100);
	TestEqual(TEXT("진행률이 1을 넘어와도 1로 잘려 0을 문다"),
		FFF_StaffEventRules::CalcOverduePenalty(Card, 1.5f), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
