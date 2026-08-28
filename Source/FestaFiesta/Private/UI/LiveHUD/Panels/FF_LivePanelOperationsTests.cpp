// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/LiveHUD/Panels/FF_LivePanelOperations.h"

/**
 * 라이브 운영 패널 일일 목표 고정 테스트 (U6).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * "목표를 막 넘긴 순간" 은 라이브 중 한 프레임이고, "관객 수를 아직 못 읽은 상태" 는
 * 소유자 API 가 생기면 재현조차 안 된다. 셋을 못으로 박는다 —
 *   ① `모름` 과 `0%` 를 구분하는 것 (지금 관객 수 API 가 없어 모름이 기본 상태다)
 *   ② 경계에서 달성으로 넘어가는 것
 *   ③ 달성 뒤 `남은 수` 를 지우는 것 (`0명 남음` 은 아직 뭔가 남은 것처럼 읽힌다)
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveOperationsRules_GoalProgress, "FestaFiesta.UI.LiveOps.GoalProgress", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveOperationsRules_GoalProgress::RunTest(const FString& Parameters)
{
	{
		// ① 관객 수를 못 읽었다. **`0%` 가 아니라 `모름`이다** — 0 을 그리면
		//    "아무도 안 왔다"는 확정 사실이 된다.
		const FFF_LiveGoalProgress P = FFF_LiveOperationsRules::BuildGoalProgress(
			/*VisitorCount*/ INDEX_NONE, /*DailyGoal*/ 500);

		TestEqual(TEXT("관객 수를 모르면 모름"), P.State, EFF_LiveGoalState::Unknown);
		TestFalse(TEXT("모름은 알려진 상태가 아니다"), P.IsKnown());
		TestEqual(TEXT("모르면 남은 수도 모름"), P.Remaining, INDEX_NONE);
	}

	{
		// 목표를 못 읽은 경우도 같다. 0 으로 나누지 않는 것도 여기서 막힌다.
		const FFF_LiveGoalProgress P = FFF_LiveOperationsRules::BuildGoalProgress(320, /*DailyGoal*/ 0);
		TestEqual(TEXT("목표가 0 이면 모름"), P.State, EFF_LiveGoalState::Unknown);

		const FFF_LiveGoalProgress Q = FFF_LiveOperationsRules::BuildGoalProgress(320, INDEX_NONE);
		TestEqual(TEXT("목표를 모르면 모름"), Q.State, EFF_LiveGoalState::Unknown);
	}

	{
		// 진행 중. 비율과 남은 수가 둘 다 맞아야 한다.
		const FFF_LiveGoalProgress P = FFF_LiveOperationsRules::BuildGoalProgress(320, 500);

		TestEqual(TEXT("진행 중이다"), P.State, EFF_LiveGoalState::InProgress);
		// 🔴 `IsNearlyEqual` 기본 허용오차는 `UE_SMALL_NUMBER`(1e-8) 라 나눗셈 결과를
		//    소수 리터럴과 비교하기에 너무 빡빡하다. 화면이 실제로 쓰는 단위(%)로 본다.
		TestEqual(TEXT("비율은 64%"), FMath::RoundToInt(P.Ratio * 100.f), 64);
		TestEqual(TEXT("180명 남았다"), P.Remaining, 180);
	}

	{
		// ② 경계. 딱 맞으면 달성이다. `>` 로 쓰면 여기서 걸린다.
		const FFF_LiveGoalProgress P = FFF_LiveOperationsRules::BuildGoalProgress(500, 500);

		TestEqual(TEXT("목표와 같으면 달성"), P.State, EFF_LiveGoalState::Reached);
		TestEqual(TEXT("비율은 100%"), FMath::RoundToInt(P.Ratio * 100.f), 100);

		// ③ `0명 남음` 은 아직 뭔가 남은 것처럼 읽힌다. 값을 두지 않는다.
		TestEqual(TEXT("달성하면 남은 수를 지운다"), P.Remaining, INDEX_NONE);
	}

	{
		// 넘겨도 비율이 1 을 넘지 않아야 한다. 막대가 밖으로 나간다.
		const FFF_LiveGoalProgress P = FFF_LiveOperationsRules::BuildGoalProgress(900, 500);

		TestEqual(TEXT("넘겨도 달성"), P.State, EFF_LiveGoalState::Reached);
		TestEqual(TEXT("비율이 100% 를 넘지 않는다"), FMath::RoundToInt(P.Ratio * 100.f), 100);
	}

	{
		// 아직 아무도 안 온 것과 못 읽은 것은 다르다. 이쪽은 `진행 중 · 0명` 이다.
		const FFF_LiveGoalProgress P = FFF_LiveOperationsRules::BuildGoalProgress(0, 500);

		TestEqual(TEXT("0명은 모름이 아니라 진행 중이다"), P.State, EFF_LiveGoalState::InProgress);
		TestEqual(TEXT("500명 남았다"), P.Remaining, 500);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
