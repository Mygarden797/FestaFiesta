#include "Misc/AutomationTest.h"
#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace FFSnsTests
{
	FFF_LiveSocialPost Post(const TCHAR* Body, int32 Score, EFF_LiveSnsSentiment Direction = EFF_LiveSnsSentiment::Unknown)
	{
		FFF_LiveSocialPost Result;
		Result.Author = FText::FromString(TEXT("테스트 관객"));
		Result.Body = FText::FromString(Body);
		Result.SentimentScore = Score;
		Result.Sentiment = Direction;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsPolarityTest, "FestaFiesta.UI.SNS.Polarity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsPolarityTest::RunTest(const FString& Parameters)
{
	using namespace FFSnsTests;
	TestEqual(TEXT("예전 미확정 -1"), FFF_LiveSnsFeedState::ResolveSentiment(Post(TEXT("미확정"), -1)), EFF_LiveSnsSentiment::Unknown);
	TestEqual(TEXT("원본의 실제 -1 반응"), FFF_LiveSnsFeedState::ResolveSentiment(Post(TEXT("불만"), -1, EFF_LiveSnsSentiment::Negative)), EFF_LiveSnsSentiment::Negative);
	TestEqual(TEXT("강도 0인 긍정 유지"), FFF_LiveSnsFeedState::ResolveSentiment(Post(TEXT("좋음"), 0, EFF_LiveSnsSentiment::Positive)), EFF_LiveSnsSentiment::Positive);
	TestEqual(TEXT("강도 0인 아쉬움 유지"), FFF_LiveSnsFeedState::ResolveSentiment(Post(TEXT("아쉬움"), 0, EFF_LiveSnsSentiment::Negative)), EFF_LiveSnsSentiment::Negative);
	TestFalse(TEXT("미확정은 긍정 필터에서 제외"), FFF_LiveSnsFeedState::Matches(Post(TEXT("모름"), -1), EFF_LiveSnsFilter::Positive));
	TestFalse(TEXT("미확정은 아쉬움 필터에서 제외"), FFF_LiveSnsFeedState::Matches(Post(TEXT("모름"), -1), EFF_LiveSnsFilter::Negative));
	TestTrue(TEXT("미확정도 전체에서 확인"), FFF_LiveSnsFeedState::Matches(Post(TEXT("모름"), -1), EFF_LiveSnsFilter::All));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsReadingPositionTest, "FestaFiesta.UI.SNS.PreserveReading",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsReadingPositionTest::RunTest(const FString& Parameters)
{
	using namespace FFSnsTests;
	FFF_LiveSnsFeedState State;
	TArray<FFF_LiveSocialPost> Source = {Post(TEXT("읽는 글"), 40), Post(TEXT("이전 글"), -20)};
	State.Sync(Source, 30);
	State.RecordArrival(Post(TEXT("새 글"), 25));
	TestEqual(TEXT("새 글로 기존 행 수가 바뀌지 않음"), State.DisplayedPosts.Num(), 2);
	TestEqual(TEXT("첫 행 위치 보존"), State.DisplayedPosts[0].Body.ToString(), FString(TEXT("읽는 글")));
	TestEqual(TEXT("새 글 배지"), State.PendingCount, 1);
	Source.Insert(Post(TEXT("새 글"), 25), 0);
	State.Sync(Source, 30);
	TestEqual(TEXT("명시적 동기화 후 최신 글이 위"), State.DisplayedPosts[0].Body.ToString(), FString(TEXT("새 글")));
	TestEqual(TEXT("새 글 확인 후 배지 해제"), State.PendingCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFilterLimitTest, "FestaFiesta.UI.SNS.FilterAndLimit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFilterLimitTest::RunTest(const FString& Parameters)
{
	using namespace FFSnsTests;
	FFF_LiveSnsFeedState State;
	State.Filter = EFF_LiveSnsFilter::Negative;
	TArray<FFF_LiveSocialPost> Source;
	for (int32 Index = 0; Index < 80; ++Index)
	{
		Source.Add(Post(*FString::FromInt(Index), Index % 2 == 0 ? 20 : -20));
	}
	State.Sync(Source, 3);
	TestEqual(TEXT("필터 후 표시 수 제한"), State.DisplayedPosts.Num(), 3);
	TestEqual(TEXT("최신 부정 글 순서"), State.DisplayedPosts[0].Body.ToString(), FString(TEXT("1")));
	TestEqual(TEXT("필터 결과 세 번째"), State.DisplayedPosts[2].Body.ToString(), FString(TEXT("5")));
	State.RecordArrival(Post(TEXT("긍정 새 글"), 80));
	TestEqual(TEXT("현재 필터 밖 새 글은 배지에 미포함"), State.PendingCount, 0);
	State.RecordArrival(Post(TEXT("부정 새 글"), -20));
	TestEqual(TEXT("현재 필터의 새 글만 배지"), State.PendingCount, 1);
	State.Filter = EFF_LiveSnsFilter::All;
	State.Sync(Source, 1000);
	TestEqual(TEXT("표시 한도 80"), State.DisplayedPosts.Num(), 80);
	for (int32 Index = 0; Index < 100; ++Index) { State.RecordArrival(Source[0]); }
	TestEqual(TEXT("원본 보관량 이상 새 글 수를 약속하지 않음"), State.PendingCount, 80);
	State.Sync({}, 30);
	TestEqual(TEXT("빈 소스는 이전 글 제거"), State.DisplayedPosts.Num(), 0);
	TestEqual(TEXT("빈 소스 동기화도 배지 해제"), State.PendingCount, 0);
	return true;
}
#endif
