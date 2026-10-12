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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsAutoInsertTest, "FestaFiesta.UI.SNS.AutoInsertNewMarks",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsAutoInsertTest::RunTest(const FString& Parameters)
{
	using namespace FFSnsTests;
	FFF_LiveSnsFeedState State;
	TArray<FFF_LiveSocialPost> Source = {Post(TEXT("기존1"), 10), Post(TEXT("기존2"), -10)};
	State.Sync(Source, 30);
	TestFalse(TEXT("구독 전부터 있던 글은 새 글 아님"), State.IsNew(0));

	// 열려 있을 때 도착: 버튼 없이 맨 위에 들어가고 점이 붙는다.
	Source.Insert(Post(TEXT("새1"), 20), 0);
	State.NoteArrival();
	TestTrue(TEXT("열린 목록에 바로 추가"), State.InsertArrival(Source[0], 30));
	TestEqual(TEXT("새 글이 맨 위"), State.DisplayedPosts[0].Body.ToString(), FString(TEXT("새1")));
	TestTrue(TEXT("새 글 점"), State.IsNew(0));
	TestFalse(TEXT("기존 글은 점 없음"), State.IsNew(1));
	TestEqual(TEXT("열려 있을 때는 배지 없음"), State.PendingCount, 0);

	// 패널을 닫으면 읽음.
	State.MarkAllSeen();
	TestFalse(TEXT("닫은 뒤 점 해제"), State.IsNew(0));

	// 닫혀 있는 동안 두 글 도착 → 배지, 열 때 다시 읽으면 두 글에만 점.
	Source.Insert(Post(TEXT("새2"), -5, EFF_LiveSnsSentiment::Negative), 0);
	State.NoteArrival(); State.RecordArrival(Source[0]);
	Source.Insert(Post(TEXT("새3"), 5, EFF_LiveSnsSentiment::Positive), 0);
	State.NoteArrival(); State.RecordArrival(Source[0]);
	TestEqual(TEXT("닫힌 동안 배지 2"), State.PendingCount, 2);
	State.Sync(Source, 30);
	TestTrue(TEXT("새3 점"), State.IsNew(0));
	TestTrue(TEXT("새2 점"), State.IsNew(1));
	TestFalse(TEXT("이미 본 새1 점 없음"), State.IsNew(2));
	TestEqual(TEXT("열면 배지 해제"), State.PendingCount, 0);

	// 필터를 바꿔 다시 읽어도 같은 글에만 점.
	State.Filter = EFF_LiveSnsFilter::Positive;
	State.Sync(Source, 30);
	TestEqual(TEXT("긍정 필터 첫 글"), State.DisplayedPosts[0].Body.ToString(), FString(TEXT("새3")));
	TestTrue(TEXT("필터 바꿔도 새 글 점 유지"), State.IsNew(0));
	TestFalse(TEXT("필터 바꿔도 읽은 글은 점 없음"), State.IsNew(1));
	TestFalse(TEXT("필터 밖 글은 열린 목록에 안 들어감"), State.InsertArrival(Post(TEXT("아쉬움"), -3, EFF_LiveSnsSentiment::Negative), 30));

	// 시간으로 지우기: 이 번호까지 읽음.
	State.MarkSeenUpTo(State.DisplayedSerials[0]);
	TestFalse(TEXT("시간이 지난 글 점 해제"), State.IsNew(0));

	// 한도: 맨 위에 넣고 아래를 뺀다.
	State.Filter = EFF_LiveSnsFilter::All;
	State.Sync(Source, 2);
	State.NoteArrival();
	State.InsertArrival(Post(TEXT("새4"), 1), 2);
	TestEqual(TEXT("한도 유지"), State.DisplayedPosts.Num(), 2);
	TestEqual(TEXT("번호도 같은 수"), State.DisplayedSerials.Num(), 2);
	TestEqual(TEXT("한도에서도 새 글이 위"), State.DisplayedPosts[0].Body.ToString(), FString(TEXT("새4")));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UnifiedNewsSourceTest, "FestaFiesta.UI.SNS.UnifiedNewsSources",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UnifiedNewsSourceTest::RunTest(const FString& Parameters)
{
	using namespace FFSnsTests;
	FFF_LiveSocialPost Operation = Post(TEXT("음식 부스 부족"), INDEX_NONE);
	Operation.bOperationAlert = true;
	Operation.OperationDay = 2;
	const TArray<FFF_LiveSocialPost> Source = { Post(TEXT("관객 최신 글"), 20), Operation, Post(TEXT("관객 이전 글"), -20) };
	FFF_LiveSnsFeedState State;
	State.TotalArrivals = 3;
	State.Sync(Source, 80);
	TestEqual(TEXT("전체는 도착 순서대로 두 원본 포함"), State.DisplayedPosts.Num(), 3);
	State.Filter = EFF_LiveSnsFilter::Social;
	State.Sync(Source, 80);
	TestEqual(TEXT("SNS는 긍정과 아쉬움을 모두 포함"), State.DisplayedPosts.Num(), 2);
	TestEqual(TEXT("종류 필터 후에도 원래 도착 번호 유지"), State.DisplayedSerials[1], int64(1));
	State.Filter = EFF_LiveSnsFilter::Operation;
	State.Sync(Source, 80);
	TestEqual(TEXT("운영 알림만 따로 조회"), State.DisplayedPosts.Num(), 1);
	TestTrue(TEXT("운영 종류 보존"), State.DisplayedPosts[0].bOperationAlert);
	TestEqual(TEXT("운영 일차 보존"), State.DisplayedPosts[0].OperationDay, 2);
	TestEqual(TEXT("운영 도착 번호 보존"), State.DisplayedSerials[0], int64(2));
	State.NoteArrival();
	TestFalse(TEXT("운영 필터에서 SNS 실시간 삽입 제외"), State.InsertArrival(Source[0], 80));
	State.NoteArrival();
	TestTrue(TEXT("운영 필터에서 운영 실시간 삽입"), State.InsertArrival(Operation, 80));
	return true;
}
#endif
