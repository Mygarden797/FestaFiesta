// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/LiveHUD/Panels/FF_LivePanelGuests.h"

/**
 * 라이브 관객 패널 판단 규칙 고정 테스트 (U6).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * 🔴 여기서 틀리면 **화면이 가장 안심되는 거짓말을 한다.**
 * `UFF_NpcNeedMetricsSubsystem` 은 표본이 0 일 때도 미해소율을 `0` 으로 돌려준다
 * (2026-08-28 김시리 주의). 그것을 그대로 그리면 **아직 아무것도 못 읽은 상태**가
 * `배고픔 0% · 화장실 0%` 로 보이고, 사람은 "잘 굴러가는구나" 하고 넘어간다.
 *
 * 셋을 못으로 박는다 —
 *   ① 표본이 없으면 `모름` 이고 `괜찮음` 이 아닌 것
 *   ② 임계 경계가 포함인 것
 *   ③ 정렬이 급한 순이고, `모름` 이 `괜찮음` 보다 위인 것
 */

namespace FFLiveGuestTestLocal
{
	FFF_LiveNeedRow MakeRow(EFF_NeedType Type, float Percent, int32 SampleCount = 100)
	{
		FFF_LiveNeedRow Row;
		Row.View.NeedType = Type;
		Row.View.UnmetRatioPercent = Percent;
		Row.Severity = FFF_LiveGuestRules::ResolveSeverity(Percent, SampleCount, 25.f, 50.f);
		return Row;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveGuestRules_Severity, "FestaFiesta.UI.LiveGuests.Severity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveGuestRules_Severity::RunTest(const FString& Parameters)
{
	// ① 🔴 여기가 핵심. 표본이 없으면 `괜찮음` 이 아니라 `모름` 이다.
	TestEqual(TEXT("표본이 없으면 모름"),
		FFF_LiveGuestRules::ResolveSeverity(/*Percent*/ 0.f, /*Sample*/ 0, 25.f, 50.f),
		EFF_LiveNeedSeverity::Unknown);

	// 표본이 없는데 값이 높게 와도 마찬가지다 — 믿을 수 없는 값이다.
	TestEqual(TEXT("표본이 없으면 값이 높아도 모름"),
		FFF_LiveGuestRules::ResolveSeverity(90.f, 0, 25.f, 50.f),
		EFF_LiveNeedSeverity::Unknown);

	// 표본이 있고 0% 인 것은 **진짜로 괜찮은 것**이다. 위와 구분되어야 한다.
	TestEqual(TEXT("표본이 있고 0% 면 괜찮음"),
		FFF_LiveGuestRules::ResolveSeverity(0.f, /*Sample*/ 100, 25.f, 50.f),
		EFF_LiveNeedSeverity::Ok);

	// ② 경계는 포함이다. `>` 로 쓰면 여기서 걸린다.
	TestEqual(TEXT("주의 경계 바로 아래"),
		FFF_LiveGuestRules::ResolveSeverity(24.9f, 100, 25.f, 50.f), EFF_LiveNeedSeverity::Ok);
	TestEqual(TEXT("주의 경계와 같으면 주의"),
		FFF_LiveGuestRules::ResolveSeverity(25.f, 100, 25.f, 50.f), EFF_LiveNeedSeverity::Watch);
	TestEqual(TEXT("나쁨 경계와 같으면 나쁨"),
		FFF_LiveGuestRules::ResolveSeverity(50.f, 100, 25.f, 50.f), EFF_LiveNeedSeverity::Bad);
	TestEqual(TEXT("100% 는 나쁨"),
		FFF_LiveGuestRules::ResolveSeverity(100.f, 100, 25.f, 50.f), EFF_LiveNeedSeverity::Bad);

	// ★ 임계를 거꾸로 넣어도 화면이 조용히 망가지지 않는다. 그대로 두면 `나쁨` 이 영영
	//   안 뜨거나 전부 `나쁨` 이 되는데, 둘 다 화면만 보고는 설정 실수인 줄 모른다.
	TestEqual(TEXT("임계가 뒤집혀도 바로잡는다"),
		FFF_LiveGuestRules::ResolveSeverity(60.f, 100, /*Watch*/ 50.f, /*Bad*/ 25.f),
		EFF_LiveNeedSeverity::Bad);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveGuestRules_Sort, "FestaFiesta.UI.LiveGuests.Sort", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveGuestRules_Sort::RunTest(const FString& Parameters)
{
	using namespace FFLiveGuestTestLocal;

	TArray<FFF_LiveNeedRow> Rows = {
		MakeRow(EFF_NeedType::Hunger, 5.f),					// 괜찮음
		MakeRow(EFF_NeedType::Thirst, 70.f),				// 나쁨
		MakeRow(EFF_NeedType::Restroom, 0.f, /*Sample*/ 0),	// 모름
		MakeRow(EFF_NeedType::Fatigue, 30.f)				// 주의
	};

	FFF_LiveGuestRules::SortBySeverity(Rows);

	// ③ 급한 순이다.
	TestEqual(TEXT("나쁨이 맨 위"), Rows[0].View.NeedType, EFF_NeedType::Thirst);
	TestEqual(TEXT("주의가 그다음"), Rows[1].View.NeedType, EFF_NeedType::Fatigue);

	// ★ 모름이 괜찮음보다 위다. 읽지 못한 것은 확인이 필요한 것이지 괜찮다고
	//   확인된 것이 아니다.
	TestEqual(TEXT("모름이 괜찮음보다 위"), Rows[2].View.NeedType, EFF_NeedType::Restroom);
	TestEqual(TEXT("괜찮음이 맨 아래"), Rows[3].View.NeedType, EFF_NeedType::Hunger);

	{
		// 같은 급이면 값이 큰 쪽이 위다.
		TArray<FFF_LiveNeedRow> Tie = {
			MakeRow(EFF_NeedType::Hunger, 60.f),
			MakeRow(EFF_NeedType::Thirst, 80.f) };

		FFF_LiveGuestRules::SortBySeverity(Tie);
		TestEqual(TEXT("값이 큰 쪽이 위"), Tie[0].View.NeedType, EFF_NeedType::Thirst);
	}

	{
		// ★ 값까지 같으면 enum 순서로 고정한다 — 갱신할 때마다 줄이 흔들리면
		//   읽는 중에 눈이 못 따라간다.
		TArray<FFF_LiveNeedRow> Same = {
			MakeRow(EFF_NeedType::Stimulation, 60.f),
			MakeRow(EFF_NeedType::Hunger, 60.f) };

		FFF_LiveGuestRules::SortBySeverity(Same);
		TestEqual(TEXT("같으면 enum 순서"), Same[0].View.NeedType, EFF_NeedType::Hunger);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
