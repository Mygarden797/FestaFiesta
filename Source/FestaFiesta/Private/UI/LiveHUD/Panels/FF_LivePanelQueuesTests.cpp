// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/LiveHUD/Panels/FF_LivePanelQueues.h"

/**
 * 라이브 대기열 패널 요약 고정 테스트 (U6).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * 여기서 잘못되면 **화면이 안심되는 거짓말을 한다.** 그것도 조용히 —
 *   ① 아직 아무도 줄을 끝내지 못한 시설을 `0초` 로 세면 평균이 실제보다 낮아진다.
 *      줄이 길게 늘어서 있는데 `평균 20초` 가 뜬다
 *   ② 대기가 0 명인데 `가장 긴 줄 · 화장실A 0명` 이 뜨면 거기가 붐비는 것처럼 읽힌다
 *   ③ 상태를 못 읽은 시설을 `닫힘` 에 섞으면, 못 읽은 것이 정상적으로 닫힌 것처럼 보인다
 * 셋 다 손으로 보면 그럴듯해서 안 걸린다.
 */

namespace FFLiveQueueTestLocal
{
	FFF_LiveFacilityView MakeView(
		const TCHAR* Name,
		int32 Waiting,
		int32 InService = 0,
		int32 AvgWait = INDEX_NONE,
		EFF_LiveFacilityQueueState QueueState = EFF_LiveFacilityQueueState::Open)
	{
		FFF_LiveFacilityView View;
		View.DisplayName = FText::FromString(Name);
		View.WaitingCount = Waiting;
		View.ActiveServiceCount = InService;
		View.AverageWaitSeconds = AvgWait;
		View.QueueState = QueueState;
		return View;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveQueueRules_Summary, "FestaFiesta.UI.LiveQueue.Summary", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveQueueRules_Summary::RunTest(const FString& Parameters)
{
	using namespace FFLiveQueueTestLocal;

	{
		// 시설이 없다. 0 과 모름이 뒤섞이지 않아야 한다.
		const FFF_LiveQueueSummary S = FFF_LiveQueueRules::BuildSummary({});

		TestEqual(TEXT("대기 0"), S.TotalWaiting, 0);
		TestEqual(TEXT("평균은 모름"), S.AverageWaitSeconds, INDEX_NONE);
		TestEqual(TEXT("평균 표본 0"), S.AverageSampleCount, 0);
		TestFalse(TEXT("줄이 없다"), S.HasAnyQueue());
	}

	{
		// 합계.
		const FFF_LiveQueueSummary S = FFF_LiveQueueRules::BuildSummary({
			MakeView(TEXT("A"), 10, 2),
			MakeView(TEXT("B"), 6, 3) });

		TestEqual(TEXT("대기 합"), S.TotalWaiting, 16);
		TestEqual(TEXT("이용 합"), S.TotalInService, 5);
	}

	{
		// ① 🔴 아직 아무도 못 끝낸 시설은 평균에서 뺀다.
		//    0 으로 세면 (60+0)/2 = 30 이 되어 실제보다 낮아진다.
		const FFF_LiveQueueSummary S = FFF_LiveQueueRules::BuildSummary({
			MakeView(TEXT("A"), 10, 0, /*AvgWait*/ 60),
			MakeView(TEXT("B"), 10, 0, /*AvgWait*/ INDEX_NONE) });

		TestEqual(TEXT("평균은 센 것만"), S.AverageWaitSeconds, 60);
		TestEqual(TEXT("한 곳만 셌다"), S.AverageSampleCount, 1);
	}

	{
		// 아무도 못 끝냈으면 평균은 0 이 아니라 모름이다.
		const FFF_LiveQueueSummary S = FFF_LiveQueueRules::BuildSummary({
			MakeView(TEXT("A"), 10),
			MakeView(TEXT("B"), 5) });

		TestEqual(TEXT("평균 모름"), S.AverageWaitSeconds, INDEX_NONE);
		TestEqual(TEXT("표본 없음"), S.AverageSampleCount, 0);
	}

	{
		// 평균 계산 자체.
		const FFF_LiveQueueSummary S = FFF_LiveQueueRules::BuildSummary({
			MakeView(TEXT("A"), 1, 0, 30),
			MakeView(TEXT("B"), 1, 0, 60),
			MakeView(TEXT("C"), 1, 0, 90) });

		TestEqual(TEXT("평균 60초"), S.AverageWaitSeconds, 60);
		TestEqual(TEXT("세 곳"), S.AverageSampleCount, 3);
	}

	{
		// 가장 긴 줄.
		const FFF_LiveQueueSummary S = FFF_LiveQueueRules::BuildSummary({
			MakeView(TEXT("짧음"), 3),
			MakeView(TEXT("김"), 21),
			MakeView(TEXT("중간"), 9) });

		TestTrue(TEXT("줄이 있다"), S.HasAnyQueue());
		TestEqual(TEXT("가장 긴 줄 이름"), S.LongestQueueName.ToString(), FString(TEXT("김")));
		TestEqual(TEXT("가장 긴 줄 인원"), S.LongestQueueWaiting, 21);
	}

	{
		// 같은 길이면 이름으로 고정한다. 안 그러면 갱신할 때마다 표시가 바뀐다.
		const FFF_LiveQueueSummary S = FFF_LiveQueueRules::BuildSummary({
			MakeView(TEXT("나나"), 7),
			MakeView(TEXT("가가"), 7) });

		TestEqual(TEXT("이름이 앞선 쪽"), S.LongestQueueName.ToString(), FString(TEXT("가가")));
	}

	{
		// ② 🔴 대기가 아예 없으면 `가장 긴 줄` 을 두지 않는다.
		//    `화장실A 0명` 이 뜨면 거기가 붐비는 것처럼 읽힌다.
		const FFF_LiveQueueSummary S = FFF_LiveQueueRules::BuildSummary({
			MakeView(TEXT("A"), 0),
			MakeView(TEXT("B"), 0) });

		TestFalse(TEXT("줄이 없으면 없는 것으로"), S.HasAnyQueue());
		TestEqual(TEXT("이름도 비운다"), S.LongestQueueName.ToString(), FString());
	}

	{
		// ③ 상태별 개수. 모름을 닫힘에 섞지 않는다.
		const FFF_LiveQueueSummary S = FFF_LiveQueueRules::BuildSummary({
			MakeView(TEXT("A"), 1, 0, INDEX_NONE, EFF_LiveFacilityQueueState::Open),
			MakeView(TEXT("B"), 1, 0, INDEX_NONE, EFF_LiveFacilityQueueState::Full),
			MakeView(TEXT("C"), 1, 0, INDEX_NONE, EFF_LiveFacilityQueueState::Closed),
			MakeView(TEXT("D"), 1, 0, INDEX_NONE, EFF_LiveFacilityQueueState::Draining),
			MakeView(TEXT("E"), 1, 0, INDEX_NONE, EFF_LiveFacilityQueueState::Blocked),
			MakeView(TEXT("F"), 1, 0, INDEX_NONE, EFF_LiveFacilityQueueState::Unknown) });

		TestEqual(TEXT("받는 중 1"), S.OpenCount, 1);
		TestEqual(TEXT("정원 참 1"), S.FullCount, 1);

		// 닫힘과 비우는 중은 사람이 할 일이 같아 한 칸으로 묶는다.
		TestEqual(TEXT("안 받음 2 (닫힘 + 비우는 중)"), S.NotAcceptingCount, 2);
		TestEqual(TEXT("막힘 1"), S.BlockedCount, 1);

		// ★ 여기가 핵심. 모름이 `안 받음` 으로 새면 못 읽은 것이 정상처럼 보인다.
		TestEqual(TEXT("모름은 따로 센다"), S.UnknownCount, 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveQueueRules_Sort, "FestaFiesta.UI.LiveQueue.Sort", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveQueueRules_Sort::RunTest(const FString& Parameters)
{
	using namespace FFLiveQueueTestLocal;

	TArray<FFF_LiveFacilityView> Views = {
		MakeView(TEXT("짧음"), 2),
		MakeView(TEXT("김"), 30),
		MakeView(TEXT("중간"), 11) };

	FFF_LiveQueueRules::SortByWaiting(Views);

	// ★ 시설 패널은 **급한 순**, 여기는 **줄이 긴 순**이다. 답하는 질문이 달라서다.
	TestEqual(TEXT("긴 줄이 맨 위"), Views[0].DisplayName.ToString(), FString(TEXT("김")));
	TestEqual(TEXT("그다음"), Views[1].DisplayName.ToString(), FString(TEXT("중간")));
	TestEqual(TEXT("짧은 줄이 아래"), Views[2].DisplayName.ToString(), FString(TEXT("짧음")));

	{
		// 값이 같으면 이름으로 고정한다 — 갱신할 때마다 순서가 흔들리면 눈이 못 따라간다.
		TArray<FFF_LiveFacilityView> Same = { MakeView(TEXT("나나"), 5), MakeView(TEXT("가가"), 5) };
		FFF_LiveQueueRules::SortByWaiting(Same);
		TestEqual(TEXT("같으면 이름순"), Same[0].DisplayName.ToString(), FString(TEXT("가가")));
	}

	return true;
}

// =============================================================================
// 지금 서면 가장 오래 걸리는 곳
// =============================================================================
//
// 🔴 `가장 긴 줄` 과 다른 값이다. 사람이 많아도 처리가 빠르면 금방 빠지고, 줄이 짧아도
// 처리가 느리면 오래 걸린다. 둘이 같은 시설을 가리킬 것이라고 가정하면 안 된다.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveQueueRules_WorstExpectedWait, "FestaFiesta.UI.LiveQueue.WorstExpectedWait", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveQueueRules_WorstExpectedWait::RunTest(const FString& Parameters)
{
	auto Make = [](const TCHAR* Name, int32 Waiting, int32 ExpectedWait)
	{
		FFF_LiveFacilityView View;
		View.DisplayName = FText::FromString(Name);
		View.WaitingCount = Waiting;
		View.ExpectedWaitSeconds = ExpectedWait;
		return View;
	};

	{
		// 줄은 짧은데 느린 곳이 가장 오래 걸린다 — `가장 긴 줄` 과 갈린다.
		const TArray<FFF_LiveFacilityView> Facilities = {
			Make(TEXT("붐비는 곳"), /*Waiting=*/20, /*ExpectedWait=*/30),
			Make(TEXT("느린 곳"),   /*Waiting=*/3,  /*ExpectedWait=*/300),
		};

		const FFF_LiveQueueSummary Summary = FFF_LiveQueueRules::BuildSummary(Facilities);

		TestEqual(TEXT("가장 긴 줄은 인원이 많은 곳"),
			Summary.LongestQueueName.ToString(), FString(TEXT("붐비는 곳")));

		TestEqual(TEXT("가장 오래 걸리는 곳은 느린 곳"),
			Summary.WorstExpectedWaitName.ToString(), FString(TEXT("느린 곳")));

		TestEqual(TEXT("그 대기 시간"), Summary.WorstExpectedWaitSeconds, 300);
		TestEqual(TEXT("둘 다 읽었다"), Summary.ExpectedWaitSampleCount, 2);
	}

	{
		// 🔴 **못 읽은 시설을 0 으로 세지 않는다.** 0 으로 세면 그 곳이 `가장 빠른 곳` 이
		//    되어 실제로 붐비는 곳을 가린다 — 평균에서 뺀 것과 같은 이유다.
		//
		//    `INDEX_NONE` 은 큐에 등록되지 않았거나 다리를 못 건넌 것이고,
		//    Queue 쪽 `UnreachableWaitSeconds`(86400) 도 ViewModel 이 여기로 바꿔 준다.
		const TArray<FFF_LiveFacilityView> Facilities = {
			Make(TEXT("모르는 곳"), /*Waiting=*/50, /*ExpectedWait=*/INDEX_NONE),
			Make(TEXT("아는 곳"),   /*Waiting=*/1,  /*ExpectedWait=*/45),
		};

		const FFF_LiveQueueSummary Summary = FFF_LiveQueueRules::BuildSummary(Facilities);

		TestEqual(TEXT("모르는 곳은 안 센다"), Summary.ExpectedWaitSampleCount, 1);
		TestEqual(TEXT("아는 곳이 최악으로 남는다"),
			Summary.WorstExpectedWaitName.ToString(), FString(TEXT("아는 곳")));
		TestEqual(TEXT("그 값"), Summary.WorstExpectedWaitSeconds, 45);
	}

	{
		// 하나도 못 읽으면 `모름` 이다. 0 이 아니다 — 0 은 `어디든 바로 이용 가능` 이라는 뜻이 된다.
		const TArray<FFF_LiveFacilityView> Facilities = {
			Make(TEXT("가"), 10, INDEX_NONE),
			Make(TEXT("나"), 20, INDEX_NONE),
		};

		const FFF_LiveQueueSummary Summary = FFF_LiveQueueRules::BuildSummary(Facilities);

		TestEqual(TEXT("표본 0"), Summary.ExpectedWaitSampleCount, 0);
		TestEqual(TEXT("값은 모름"), Summary.WorstExpectedWaitSeconds, INDEX_NONE);
		TestTrue(TEXT("이름도 비어 있다"), Summary.WorstExpectedWaitName.IsEmpty());
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
