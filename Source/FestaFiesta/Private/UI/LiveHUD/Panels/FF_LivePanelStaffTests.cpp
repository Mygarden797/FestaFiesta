// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/LiveHUD/Panels/FF_LivePanelStaff.h"

/**
 * 라이브 스태프 패널 인력 현황 고정 테스트 (U6).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * 손으로 재현하려면 축제를 켜고 알바를 뽑고 프로그램을 편성해야 하는데, 정작 확인해야 할
 * 세 경우는 그렇게 해서는 잘 안 나온다 —
 *   ① **못 읽은 값**. 축제 시작 전에는 인력 API 가 `INDEX_NONE` 이다. `0명` 으로 보이면
 *      "아무도 없다" 는 틀린 사실이 된다
 *   ② **경계**. 전원이 배치되면 `0명 남음` 이 아니라 `없음` 이어야 한다
 *   ③ **값 불일치**. 배치가 전체보다 많은 것은 버그인데, 0 으로 깎으면 화면에서 사라진다
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveStaffRules_Summary, "FestaFiesta.UI.LiveStaff.Summary", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveStaffRules_Summary::RunTest(const FString& Parameters)
{
	{
		// ① 전부 모른다. `0명 놀고 있음` 이 아니라 `모름` 이다.
		const FFF_LiveStaffSummary S = FFF_LiveStaffRules::BuildSummary(
			INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE);

		TestEqual(TEXT("모르면 모름"), S.IdleState, EFF_LiveStaffIdleState::Unknown);
		TestFalse(TEXT("모름은 알려진 상태가 아니다"), S.IsKnown());
		TestEqual(TEXT("남는 인원도 모름"), S.Idle, INDEX_NONE);
		TestEqual(TEXT("전체도 모름"), S.Total, INDEX_NONE);
	}

	{
		// 한쪽만 알아도 남는 인원은 못 낸다. 다만 **아는 값은 그대로 넘긴다** —
		// 하나가 없다고 나머지까지 숨기면 볼 수 있는 것까지 못 보게 된다.
		const FFF_LiveStaffSummary S = FFF_LiveStaffRules::BuildSummary(
			/*Total*/ 12, /*Base*/ 6, /*Temp*/ 6, /*Assigned*/ INDEX_NONE);

		TestEqual(TEXT("배치를 모르면 남는 인원도 모름"), S.IdleState, EFF_LiveStaffIdleState::Unknown);
		TestEqual(TEXT("그래도 전체는 보인다"), S.Total, 12);
		TestEqual(TEXT("내역도 보인다"), S.Base, 6);
	}

	{
		// 진행 중. 남는 인원이 있다.
		const FFF_LiveStaffSummary S = FFF_LiveStaffRules::BuildSummary(12, 6, 6, /*Assigned*/ 5);

		TestEqual(TEXT("남는 인원이 있다"), S.IdleState, EFF_LiveStaffIdleState::HasIdle);
		TestEqual(TEXT("7명이 논다"), S.Idle, 7);
	}

	{
		// ② 경계. 전원이 배치되면 `남는 인원 있음` 이 아니다.
		const FFF_LiveStaffSummary S = FFF_LiveStaffRules::BuildSummary(12, 6, 6, /*Assigned*/ 12);

		TestEqual(TEXT("전원 배치"), S.IdleState, EFF_LiveStaffIdleState::AllBusy);
		TestEqual(TEXT("남는 인원은 0"), S.Idle, 0);
	}

	{
		// 아무도 배치되지 않은 것은 정상이다 — 유료 프로그램을 안 넣은 날이 그렇다.
		const FFF_LiveStaffSummary S = FFF_LiveStaffRules::BuildSummary(12, 6, 6, /*Assigned*/ 0);

		TestEqual(TEXT("전원이 논다"), S.IdleState, EFF_LiveStaffIdleState::HasIdle);
		TestEqual(TEXT("12명이 논다"), S.Idle, 12);
	}

	{
		// ③ 값 불일치. **0 으로 깎지 않는다** — 깎으면 `전원 배치` 로 보여 버그가 숨는다.
		const FFF_LiveStaffSummary S = FFF_LiveStaffRules::BuildSummary(12, 6, 6, /*Assigned*/ 13);

		TestEqual(TEXT("값이 어긋났다"), S.IdleState, EFF_LiveStaffIdleState::Inconsistent);
		TestEqual(TEXT("남는 인원을 지어내지 않는다"), S.Idle, INDEX_NONE);

		// 원래 값은 그대로 남겨야 화면이 "13 / 12" 라고 말할 수 있다.
		TestEqual(TEXT("배치 값은 유지"), S.Assigned, 13);
		TestEqual(TEXT("전체 값도 유지"), S.Total, 12);
	}

	{
		// 인원이 0명인 축제도 성립한다. `모름` 과 구분되어야 한다.
		const FFF_LiveStaffSummary S = FFF_LiveStaffRules::BuildSummary(0, 0, 0, 0);

		TestTrue(TEXT("0명은 모름이 아니다"), S.IsKnown());
		TestEqual(TEXT("0명이면 전원 배치로 본다"), S.IdleState, EFF_LiveStaffIdleState::AllBusy);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
