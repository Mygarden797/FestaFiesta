// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Insights/FF_SnsFeedGenerator.h"
#include "DataTables/FF_SnsPostTemplateRow.h"
#include "Economy/FF_FestivalEconomyTypes.h"

// UFF_CrowdInsightsSubsystem 자체는 GameInstance를 Outer로 요구하므로 테스트에서 만들 수
// 없다. 그래서 화제·문장 선택 로직을 FFF_SnsFeedGenerator(순수 함수)로 분리했고
// 여기서는 그것만 검증한다 — FF_SatisfactionMetricsSubsystemTests.cpp와 같은 이유·같은 패턴이다.

namespace FFF_SnsFeedGeneratorTestHelpers
{
	// 화제 하나에 문장 Count개를 채워 RowStorage에 누적한다. 실제 TMap은 전부 다 채운 뒤
	// BuildPoolsFromStorage로 한 번에 만든다 — TArray 재할당으로 포인터가 죽는 것을 막기 위해서다.
	void AddTopicPool(TArray<FFF_SnsPostTemplateRow>& RowStorage, int32 TopicId, int32 Count)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			FFF_SnsPostTemplateRow Row;
			Row.TemplateId = FName(*FString::Printf(TEXT("topic%d_line%d"), TopicId, i));
			Row.TopicId = TopicId;
			Row.Text = FText::FromString(FString::Printf(TEXT("문장 %d-%d"), TopicId, i));
			RowStorage.Add(Row);
		}
	}

	void BuildPoolsFromStorage(TMap<int32, TArray<const FFF_SnsPostTemplateRow*>>& TemplatesByTopic,
		const TArray<FFF_SnsPostTemplateRow>& RowStorage)
	{
		TemplatesByTopic.Empty();
		for (const FFF_SnsPostTemplateRow& Row : RowStorage)
		{
			TemplatesByTopic.FindOrAdd(Row.TopicId).Add(&Row);
		}
	}
}

using namespace FFF_SnsFeedGeneratorTestHelpers;

// 같은 (Seed, SequenceIndex, 신호)면 몇 번을 다시 뽑아도 완전히 같은 글이 나온다 — 리플레이 재현성
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFeedGenerator_SameInputsProduceIdenticalPost, "FestaFiesta.NPC.SnsFeedGenerator.SameInputsProduceIdenticalPost", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFeedGenerator_SameInputsProduceIdenticalPost::RunTest(const FString& Parameters)
{
	TArray<FFF_SnsPostTemplateRow> RowStorage;
	for (int32 TopicId : {1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14})
	{
		AddTopicPool(RowStorage, TopicId, 5);
	}

	TMap<int32, TArray<const FFF_SnsPostTemplateRow*>> TemplatesByTopic;
	BuildPoolsFromStorage(TemplatesByTopic, RowStorage);

	FFF_SnsFeedSignals Signals;
	Signals.SimSatisfaction0To100 = 70;
	Signals.WaitScore0To100 = 30;
	Signals.DensityScore0To100 = 40;

	const FFF_SnsPostRow First = FFF_SnsFeedGenerator::GeneratePost(20260812, 5, Signals, TemplatesByTopic, 123.f);
	const FFF_SnsPostRow Second = FFF_SnsFeedGenerator::GeneratePost(20260812, 5, Signals, TemplatesByTopic, 123.f);

	TestEqual(TEXT("같은 입력이면 같은 화제"), First.TopicId, Second.TopicId);
	TestEqual(TEXT("같은 입력이면 같은 극성"), (int32)First.Sentiment, (int32)Second.Sentiment);
	TestEqual(TEXT("같은 입력이면 같은 문장"), First.MessageTemplateId, Second.MessageTemplateId);
	TestEqual(TEXT("같은 입력이면 같은 표시 핸들"), First.DisplayHandle, Second.DisplayHandle);
	TestEqual(TEXT("같은 입력이면 같은 PostId"), First.PostId, Second.PostId);

	const FFF_SnsPostRow DifferentIndex = FFF_SnsFeedGenerator::GeneratePost(20260812, 6, Signals, TemplatesByTopic, 123.f);
	TestTrue(TEXT("SequenceIndex가 다르면 최소 하나는 달라진다(문장이나 핸들)"),
		DifferentIndex.MessageTemplateId != First.MessageTemplateId || DifferentIndex.DisplayHandle != First.DisplayHandle);

	return true;
}

// 만족도가 100이면 거의 항상 긍정, 0이면 거의 항상 부정 — 극성이 SimSatisfaction 확률을 따른다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFeedGenerator_PolarityFollowsSatisfactionProbability, "FestaFiesta.NPC.SnsFeedGenerator.PolarityFollowsSatisfactionProbability", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFeedGenerator_PolarityFollowsSatisfactionProbability::RunTest(const FString& Parameters)
{
	int32 PositiveCountAt100 = 0;
	int32 PositiveCountAt0 = 0;
	constexpr int32 SampleCount = 200;

	for (int32 i = 0; i < SampleCount; ++i)
	{
		if (FFF_SnsFeedGenerator::RollPolarityIsPositive(20260812, i, 100)) ++PositiveCountAt100;
		if (FFF_SnsFeedGenerator::RollPolarityIsPositive(20260812, i, 0)) ++PositiveCountAt0;
	}

	TestEqual(TEXT("만족도 100이면 항상 긍정"), PositiveCountAt100, SampleCount);
	TestEqual(TEXT("만족도 0이면 항상 부정"), PositiveCountAt0, 0);
	return true;
}

// ChooseTopicId는 요청한 극성에 속한 화제만 돌려준다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFeedGenerator_ChooseTopicIdRespectsPolarity, "FestaFiesta.NPC.SnsFeedGenerator.ChooseTopicIdRespectsPolarity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFeedGenerator_ChooseTopicIdRespectsPolarity::RunTest(const FString& Parameters)
{
	FFF_SnsFeedSignals Signals;

	for (int32 i = 0; i < 50; ++i)
	{
		const int32 PositiveTopic = FFF_SnsFeedGenerator::ChooseTopicId(20260812, i, EFF_SnsSentiment::Positive, Signals);
		TestTrue(TEXT("긍정 화제는 항상 Positive 화제여야 한다"),
			FFF_SnsFeedGenerator::GetTopicPolarity(PositiveTopic) == EFF_SnsSentiment::Positive);
		TestNotEqual(TEXT("화제 9(피날레)는 주기 선택에서 절대 안 나온다"), PositiveTopic, 9);

		const int32 NegativeTopic = FFF_SnsFeedGenerator::ChooseTopicId(20260812, i, EFF_SnsSentiment::Negative, Signals);
		TestTrue(TEXT("부정 화제는 항상 Negative 화제여야 한다"),
			FFF_SnsFeedGenerator::GetTopicPolarity(NegativeTopic) == EFF_SnsSentiment::Negative);
	}

	return true;
}

// 게이트가 있는 화제(⑧공연 임박, ⑫초반)는 조건이 안 맞으면 후보에서 빠진다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFeedGenerator_GatedTopicsRespectEligibility, "FestaFiesta.NPC.SnsFeedGenerator.GatedTopicsRespectEligibility", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFeedGenerator_GatedTopicsRespectEligibility::RunTest(const FString& Parameters)
{
	FFF_SnsFeedSignals Locked;
	Locked.bMainProgramLocked = false;
	TestFalse(TEXT("공연 락 구간이 아니면 화제8은 자격이 없다"),
		FFF_SnsFeedGenerator::IsTopicEligibleNow(8, Locked));

	FFF_SnsFeedSignals Unlocked;
	Unlocked.bMainProgramLocked = true;
	TestTrue(TEXT("공연 락 구간이면 화제8은 자격이 있다"),
		FFF_SnsFeedGenerator::IsTopicEligibleNow(8, Unlocked));

	FFF_SnsFeedSignals Day1;
	Day1.CurrentDay = 1;
	TestTrue(TEXT("1일차면 화제12는 자격이 있다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(12, Day1));

	FFF_SnsFeedSignals Day2;
	Day2.CurrentDay = 2;
	TestFalse(TEXT("2일차면 화제12는 자격이 없다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(12, Day2));

	// 화제27(어제 글 보고 왔다): 2일차 오전 + 어제 매출 1위가 있을 때만
	FFF_SnsFeedSignals Inflow;
	Inflow.CurrentDay = 2;
	Inflow.LiveElapsedFraction = 0.1f;
	TestFalse(TEXT("어제 매출 1위가 없으면 화제27은 자격이 없다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(27, Inflow));
	Inflow.YesterdayTopFacilityDisplayName = TEXT("피자 부스");
	TestTrue(TEXT("2일차 오전이면 화제27은 자격이 있다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(27, Inflow));
	Inflow.LiveElapsedFraction = 0.5f;
	TestFalse(TEXT("오후에는 화제27이 나오지 않는다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(27, Inflow));
	// 한산(11)은 붐비는 칸이 없고 밀도 점수가 높을 때만, 붐빔(4)은 붐비는 칸이 있을 때만
	FFF_SnsFeedSignals Calm;
	Calm.bFacilityUsedToday = true;
	Calm.DensityScore0To100 = FFF_SnsFeedGenerator::CalmDensityScoreAtLeast;
	TestTrue(TEXT("한산하면 화제11"), FFF_SnsFeedGenerator::IsTopicEligibleNow(11, Calm));
	TestFalse(TEXT("붐비는 칸이 없으면 화제4는 없다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(4, Calm));
	Calm.CongestedCellCount = 1;
	TestFalse(TEXT("붐비는 칸이 있으면 한산 글은 없다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(11, Calm));
	TestTrue(TEXT("붐비는 칸이 있으면 화제4"), FFF_SnsFeedGenerator::IsTopicEligibleNow(4, Calm));

	Inflow.LiveElapsedFraction = 0.1f;
	Inflow.CurrentDay = 1;
	TestFalse(TEXT("1일차에는 화제27이 나오지 않는다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(27, Inflow));

	FFF_SnsPostTemplateRow InflowRow;
	InflowRow.TopicId = 27;
	InflowRow.bUsesFacilityPlaceholder = true;
	InflowRow.Text = FText::FromString(TEXT("어제 SNS 보니까 {시설} 있어서 왔어요"));
	Inflow.CurrentDay = 2;
	TestTrue(TEXT("화제27의 {시설} 행은 쓴다"), FFF_SnsFeedGenerator::ShouldIncludeTemplateRow(InflowRow));
	TestEqual(TEXT("{시설}은 어제 매출 1위로 바뀐다"),
		FFF_SnsFeedGenerator::ResolveBodyText(InflowRow, Inflow).ToString(), FString(TEXT("어제 SNS 보니까 피자 부스 있어서 왔어요")));

	return true;
}

// ForcedTopicId를 주면 2단 선택을 건너뛰고 그 화제로 고정된다 — 피날레 전용 경로
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFeedGenerator_ForcedTopicIdSkipsSelection, "FestaFiesta.NPC.SnsFeedGenerator.ForcedTopicIdSkipsSelection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFeedGenerator_ForcedTopicIdSkipsSelection::RunTest(const FString& Parameters)
{
	TArray<FFF_SnsPostTemplateRow> RowStorage;
	AddTopicPool(RowStorage, 9, 3);
	TMap<int32, TArray<const FFF_SnsPostTemplateRow*>> TemplatesByTopic;
	BuildPoolsFromStorage(TemplatesByTopic, RowStorage);

	FFF_SnsFeedSignals Signals; // 만족도 등은 무시되어야 한다
	Signals.SimSatisfaction0To100 = 0; // 부정 극성으로 유도하려는 시도 — 무시돼야 함

	const FFF_SnsPostRow Post = FFF_SnsFeedGenerator::GeneratePost(
		20260812, 1, Signals, TemplatesByTopic, 999.f, /*ForcedTopicId=*/9);

	TestEqual(TEXT("고정한 화제 그대로"), Post.TopicId, 9);
	TestTrue(TEXT("화제9는 항상 긍정"), Post.Sentiment == EFF_SnsSentiment::Positive);
	TestTrue(TEXT("문장이 채워진다"), !Post.MessageTemplateId.IsNone());

	return true;
}

// 화제 풀이 비어있으면 문장 없이도 크래시하지 않고, 나머지 필드는 채워진다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFeedGenerator_EmptyPoolDoesNotCrash, "FestaFiesta.NPC.SnsFeedGenerator.EmptyPoolDoesNotCrash", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFeedGenerator_EmptyPoolDoesNotCrash::RunTest(const FString& Parameters)
{
	TMap<int32, TArray<const FFF_SnsPostTemplateRow*>> EmptyTemplates;
	FFF_SnsFeedSignals Signals;

	const FFF_SnsPostRow Post = FFF_SnsFeedGenerator::GeneratePost(20260812, 1, Signals, EmptyTemplates, 0.f);

	TestTrue(TEXT("풀이 비어도 MessageTemplateId는 비어있을 뿐 크래시하지 않는다"), Post.MessageTemplateId.IsNone());
	TestTrue(TEXT("PostId는 여전히 채워진다(NIL GUID가 아니다)"), Post.PostId.IsValid());
	return true;
}

// TopicSalience0To100은 실제로 그 화제를 고른 점수를 그대로 담는다(임의 상수가 아니다) —
// UI가 감정 세기(-100~100)로 변환할 때 이 값을 쓸 근거가 된다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFeedGenerator_SalienceReflectsChosenTopicScore, "FestaFiesta.NPC.SnsFeedGenerator.SalienceReflectsChosenTopicScore", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFeedGenerator_SalienceReflectsChosenTopicScore::RunTest(const FString& Parameters)
{
	TMap<int32, TArray<const FFF_SnsPostTemplateRow*>> EmptyTemplates;

	FFF_SnsFeedSignals Signals;
	Signals.WaitScore0To100 = 20; // 화제1(대기시간 김) 점수 = 100 - 20 = 80
	Signals.WorstWaitFacilityDisplayName = TEXT("어묵 포장마차"); // 대기를 마친 시설이 있어야 화제1이 나간다

	// 극성·화제 선택이 어떤 인덱스에서 화제1(대기)을 고르는지 몰라도, 고른 화제의 점수와
	// 실제로 채워진 값이 일치하기만 하면 된다
	bool bFoundTopic1Sample = false;
	for (int32 i = 0; i < 100 && !bFoundTopic1Sample; ++i)
	{
		const FFF_SnsPostRow Post = FFF_SnsFeedGenerator::GeneratePost(20260812, i, Signals, EmptyTemplates, 0.f);
		if (Post.TopicId == 1)
		{
			bFoundTopic1Sample = true;
			TestEqual(TEXT("화제1이 뽑히면 세기는 WaitScore 기반 80이어야 한다"), Post.TopicSalience0To100, 80.f);
		}
	}
	TestTrue(TEXT("100번 시도하면 화제1이 최소 한 번은 뽑혀야 한다"), bFoundTopic1Sample);

	// 강제 화제는 고정 100
	const FFF_SnsPostRow Forced = FFF_SnsFeedGenerator::GeneratePost(20260812, 1, Signals, EmptyTemplates, 0.f, 9);
	TestEqual(TEXT("강제 화제(피날레)는 세기 100 고정"), Forced.TopicSalience0To100, 100.f);

	return true;
}

// {시설} 자리표시자는 가리킬 시설이 신호로 분명한 화제(1·4·7·10·14·27)만 통과한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFeedGenerator_FacilityPlaceholderOnlyForSignaledTopics, "FestaFiesta.NPC.SnsFeedGenerator.FacilityPlaceholderOnlyForSignaledTopics", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFeedGenerator_FacilityPlaceholderOnlyForSignaledTopics::RunTest(const FString& Parameters)
{
	for (const int32 TopicId : {4, 7, 14})
	{
		FFF_SnsPostTemplateRow Row;
		Row.TopicId = TopicId;
		Row.bUsesFacilityPlaceholder = true;
		TestTrue(FString::Printf(TEXT("화제%d의 {시설} 문장은 포함된다"), TopicId),
			FFF_SnsFeedGenerator::ShouldIncludeTemplateRow(Row));

		Row.Text = FText::FromString(TEXT("{시설} 또 올래"));
		FFF_SnsFeedSignals Signals;
		Signals.MostQueuedFacilityDisplayName = TEXT("타코 트럭");
		TestEqual(FString::Printf(TEXT("화제%d는 줄이 가장 긴 시설로 치환된다"), TopicId),
			FFF_SnsFeedGenerator::ResolveBodyText(Row, Signals).ToString(), FString(TEXT("타코 트럭 또 올래")));
	}

	FFF_SnsPostTemplateRow Topic1;
	Topic1.TopicId = 1;
	Topic1.bUsesFacilityPlaceholder = true;
	TestTrue(TEXT("화제1의 {시설} 문장은 포함된다"), FFF_SnsFeedGenerator::ShouldIncludeTemplateRow(Topic1));

	FFF_SnsPostTemplateRow Topic10;
	Topic10.TopicId = 10;
	Topic10.bUsesFacilityPlaceholder = true;
	TestTrue(TEXT("화제10의 {시설} 문장은 포함된다"), FFF_SnsFeedGenerator::ShouldIncludeTemplateRow(Topic10));

	FFF_SnsPostTemplateRow Topic2;
	Topic2.TopicId = 2;
	Topic2.bUsesFacilityPlaceholder = true;
	TestFalse(TEXT("화제2의 {시설} 문장은 아직 제외된다(가리킬 시설 신호가 없음)"),
		FFF_SnsFeedGenerator::ShouldIncludeTemplateRow(Topic2));

	FFF_SnsPostTemplateRow NoPlaceholder;
	NoPlaceholder.TopicId = 2;
	NoPlaceholder.bUsesFacilityPlaceholder = false;
	TestTrue(TEXT("자리표시자 없는 문장은 화제와 무관하게 항상 포함된다"),
		FFF_SnsFeedGenerator::ShouldIncludeTemplateRow(NoPlaceholder));

	return true;
}

// ResolveBodyText가 {시설}을 실제 이름으로 치환한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFeedGenerator_ResolveBodyTextSubstitutesFacilityName, "FestaFiesta.NPC.SnsFeedGenerator.ResolveBodyTextSubstitutesFacilityName", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFeedGenerator_ResolveBodyTextSubstitutesFacilityName::RunTest(const FString& Parameters)
{
	FFF_SnsPostTemplateRow Row;
	Row.TopicId = 1;
	Row.bUsesFacilityPlaceholder = true;
	Row.Text = FText::FromString(TEXT("{시설} 줄 실화냐"));

	FFF_SnsFeedSignals WithName;
	WithName.WorstWaitFacilityDisplayName = TEXT("타코 트럭");
	TestEqual(TEXT("화제1은 WorstWaitFacilityDisplayName으로 치환된다"),
		FFF_SnsFeedGenerator::ResolveBodyText(Row, WithName).ToString(), FString(TEXT("타코 트럭 줄 실화냐")));

	FFF_SnsFeedSignals WithoutName; // 이름 신호가 비어있는 상황(시설이 아직 없는 등)
	TestEqual(TEXT("이름이 없으면 \"여기\"로 안전하게 대체된다"),
		FFF_SnsFeedGenerator::ResolveBodyText(Row, WithoutName).ToString(), FString(TEXT("여기 줄 실화냐")));

	FFF_SnsPostTemplateRow PlainRow;
	PlainRow.bUsesFacilityPlaceholder = false;
	PlainRow.Text = FText::FromString(TEXT("그대로 나가는 문장"));
	TestEqual(TEXT("자리표시자가 없으면 원문 그대로"),
		FFF_SnsFeedGenerator::ResolveBodyText(PlainRow, WithName).ToString(), FString(TEXT("그대로 나가는 문장")));

	return true;
}

// 화제10(인기 시설)·12(초반 시설부족) 점수가 실제 시설 신호를 반영한다(2026-09-17, 한광훈의
// DisplayName 스레딩 이후 배선)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFeedGenerator_FacilityTopicScoresReflectSignals, "FestaFiesta.NPC.SnsFeedGenerator.FacilityTopicScoresReflectSignals", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFeedGenerator_FacilityTopicScoresReflectSignals::RunTest(const FString& Parameters)
{
	FFF_SnsFeedSignals Busy;
	Busy.MaxWaitingCount = 10;
	TestEqual(TEXT("화제10 점수는 MaxWaitingCount에 비례한다"),
		FFF_SnsFeedGenerator::ComputeTopicScore(10, Busy), 50.f);

	FFF_SnsFeedSignals Empty;
	TestEqual(TEXT("대기 인원이 없으면 화제10 점수는 0"),
		FFF_SnsFeedGenerator::ComputeTopicScore(10, Empty), 0.f);

	FFF_SnsFeedSignals FewFacilities;
	FewFacilities.FacilityCount = 2;
	TestEqual(TEXT("시설이 적으면 화제12 점수가 높다"),
		FFF_SnsFeedGenerator::ComputeTopicScore(12, FewFacilities), 80.f);

	FFF_SnsFeedSignals ManyFacilities;
	ManyFacilities.FacilityCount = 15;
	TestEqual(TEXT("시설이 충분히 많으면 화제12 점수는 0으로 바닥난다"),
		FFF_SnsFeedGenerator::ComputeTopicScore(12, ManyFacilities), 0.f);

	return true;
}

// Sentiment 열이 화제의 실제 극성(GetTopicPolarity)과 일치하는지 검증
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFeedGenerator_DetectsSentimentMismatch, "FestaFiesta.NPC.SnsFeedGenerator.DetectsSentimentMismatch", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFeedGenerator_DetectsSentimentMismatch::RunTest(const FString& Parameters)
{
	FFF_SnsPostTemplateRow Matching;
	Matching.TopicId = 1; // 부정 화제
	Matching.Sentiment = TEXT("negative");
	TestTrue(TEXT("화제1(부정)에 negative 라벨이면 일치"), FFF_SnsFeedGenerator::DoesSentimentMatchTopic(Matching));

	FFF_SnsPostTemplateRow Mismatched;
	Mismatched.TopicId = 1; // 부정 화제인데
	Mismatched.Sentiment = TEXT("positive"); // 라벨은 긍정 — 오타 상황
	TestFalse(TEXT("화제1(부정)에 positive 라벨이면 불일치"), FFF_SnsFeedGenerator::DoesSentimentMatchTopic(Mismatched));

	FFF_SnsPostTemplateRow Empty;
	Empty.TopicId = 7; // 긍정 화제
	Empty.Sentiment = NAME_None; // 값이 아예 비어있는 상황
	TestFalse(TEXT("Sentiment가 비어있으면 불일치로 취급"), FFF_SnsFeedGenerator::DoesSentimentMatchTopic(Empty));

	return true;
}

// SNS 입장료 화제 — 비쌀 때만 15, 쌀 때만 16. 비싸면 부정 글이 는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Sns_AdmissionFeeTopics, "FestaFiesta.NPC.SnsFeedGenerator.AdmissionFeeTopics", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Sns_AdmissionFeeTopics::RunTest(const FString& Parameters)
{
	FFF_SnsFeedSignals Signals;
	Signals.SimSatisfaction0To100 = 60;
	Signals.ReferenceAdmissionFeeGold = 5;
	Signals.MaxAdmissionFeeGold = 15;

	Signals.AdmissionFeeGold = 5;
	TestFalse(TEXT("기준값이면 비쌈 화제가 안 나온다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(15, Signals));
	TestFalse(TEXT("기준값이면 가성비 화제가 안 나온다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(16, Signals));
	TestEqual(TEXT("기준값이면 극성 추첨 만족도가 그대로"), FFF_SnsFeedGenerator::CalcPolaritySatisfaction(Signals), 60);

	Signals.AdmissionFeeGold = 15;
	TestTrue(TEXT("상한이면 비쌈 화제"), FFF_SnsFeedGenerator::IsTopicEligibleNow(15, Signals));
	TestEqual(TEXT("상한이면 점수 100"), FFF_SnsFeedGenerator::ComputeTopicScore(15, Signals), 100.f, 1e-3f);
	TestEqual(TEXT("상한이면 부정 쪽으로 25 기운다"), FFF_SnsFeedGenerator::CalcPolaritySatisfaction(Signals), 35);
	TestEqual(TEXT("비쌈은 부정 화제"), FFF_SnsFeedGenerator::GetTopicPolarity(15), EFF_SnsSentiment::Negative);

	Signals.AdmissionFeeGold = 0;
	TestTrue(TEXT("0 Gold 면 가성비 화제"), FFF_SnsFeedGenerator::IsTopicEligibleNow(16, Signals));
	TestEqual(TEXT("0 Gold 면 점수 100"), FFF_SnsFeedGenerator::ComputeTopicScore(16, Signals), 100.f, 1e-3f);
	TestEqual(TEXT("가성비는 긍정 화제"), FFF_SnsFeedGenerator::GetTopicPolarity(16), EFF_SnsSentiment::Positive);
	TestEqual(TEXT("0 Gold 면 긍정 쪽으로 10 기운다"), FFF_SnsFeedGenerator::CalcPolaritySatisfaction(Signals), 70);
	return true;
}

// 피날레는 문장 수보다 글이 많이 나간다. 풀을 여러 바퀴 돌아도 같은 문장이 두 번 보이지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Sns_FinaleBodiesNeverRepeat, "FestaFiesta.NPC.SnsFeedGenerator.FinaleBodiesNeverRepeat", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Sns_FinaleBodiesNeverRepeat::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Variant 0 은 원문"), FFF_SnsFeedGenerator::DecorateBody(TEXT("불꽃"), EFF_SnsSentiment::Positive, 0), FString(TEXT("불꽃")));
	TestNotEqual(TEXT("Variant 1 부터는 추임새가 붙는다"), FFF_SnsFeedGenerator::DecorateBody(TEXT("불꽃"), EFF_SnsSentiment::Positive, 1), FString(TEXT("불꽃")));

	// S 등급 한 판(약 90초 · 0.35초 간격 ≈ 260건)보다 넉넉하게 400건을 62문장 풀로 만든다
	constexpr int32 PoolSize = 62;
	constexpr int32 Posts = 400;
	TSet<FString> Used;
	for (int32 Cycle = 0; Cycle < Posts; ++Cycle)
	{
		const int32 Slot = Cycle % PoolSize;
		const FString Base = FString::Printf(TEXT("문장 %d"), Slot);
		const int32 Before = Used.Num();
		FFF_SnsFeedGenerator::MakeUniqueDecoratedBody(Base, EFF_SnsSentiment::Positive, Cycle / PoolSize, GetTypeHash(Base), Used);
		if (Used.Num() != Before + 1)
		{
			AddError(FString::Printf(TEXT("%d번째 글에서 같은 문장이 다시 나왔다"), Cycle));
			break;
		}
	}

	// 부정 쪽 추임새도 같은 성질이다
	TSet<FString> UsedNegative;
	for (int32 Cycle = 0; Cycle < 120; ++Cycle)
	{
		const FString Base = FString::Printf(TEXT("아쉬움 %d"), Cycle % 25);
		FFF_SnsFeedGenerator::MakeUniqueDecoratedBody(Base, EFF_SnsSentiment::Negative, Cycle / 25, GetTypeHash(Base), UsedNegative);
	}
	TestEqual(TEXT("부정 120건도 전부 다른 문장"), UsedNegative.Num(), 120);
	return true;
}

// 첫날 개장 직후에는 "또 왔다"·"좋았다"처럼 이미 겪은 글이 나가지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Sns_OpeningHasNoExperiencedPosts, "FestaFiesta.NPC.SnsFeedGenerator.OpeningHasNoExperiencedPosts", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Sns_OpeningHasNoExperiencedPosts::RunTest(const FString& Parameters)
{
	FFF_SnsFeedSignals Opening;
	Opening.CurrentDay = 1;
	Opening.LiveElapsedFraction = 0.02f;
	Opening.SimSatisfaction0To100 = 90;

	for (int32 Sequence = 0; Sequence < 300; ++Sequence)
	{
		for (const EFF_SnsSentiment Polarity : { EFF_SnsSentiment::Positive, EFF_SnsSentiment::Negative })
		{
			const int32 Topic = FFF_SnsFeedGenerator::ChooseTopicId(7, Sequence, Polarity, Opening);
			if (Topic == 1 || Topic == 3 || Topic == 7 || Topic == 10 || Topic == 11 || Topic == 14)
			{
				AddError(FString::Printf(TEXT("개장 직후에 화제 %d 가 나왔다"), Topic));
				return true;
			}
		}
	}
	TestTrue(TEXT("개장 직후에는 개장 기대감이 나온다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(26, Opening));

	FFF_SnsFeedSignals Day1Late = Opening;
	Day1Late.LiveElapsedFraction = 0.6f;
	Day1Late.bFacilityUsedToday = true;
	TestTrue(TEXT("1일차 후반에 시설을 써 봤으면 재방문 글이 나올 수 있다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(14, Day1Late));
	TestFalse(TEXT("후반에는 개장 기대감이 안 나온다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(26, Day1Late));

	FFF_SnsFeedSignals Day2Early = Opening;
	Day2Early.CurrentDay = 2;
	Day2Early.bFacilityUsedToday = true;
	TestTrue(TEXT("2일차에는 이른 시간에도 재방문 글이 나올 수 있다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(14, Day2Early));
	return true;
}

// 공연 시간에는 공연 얘기가 나온다. 공연 직전 기대감은 공연이 시작되면 그친다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Sns_MainShowTopics, "FestaFiesta.NPC.SnsFeedGenerator.MainShowTopics", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Sns_MainShowTopics::RunTest(const FString& Parameters)
{
	FFF_SnsFeedSignals Show;
	Show.LiveElapsedFraction = 0.8f;
	Show.bMainProgramLocked = true;
	Show.bMainShowRunning = true;
	TestTrue(TEXT("공연 중 글"), FFF_SnsFeedGenerator::IsTopicEligibleNow(23, Show));
	TestTrue(TEXT("공연 관람 불편 글"), FFF_SnsFeedGenerator::IsTopicEligibleNow(25, Show));
	TestFalse(TEXT("공연이 시작되면 직전 기대감은 그친다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(8, Show));
	TestFalse(TEXT("공연 중에는 여운 글이 아니다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(24, Show));
	TestEqual(TEXT("공연 중 글은 긍정"), FFF_SnsFeedGenerator::GetTopicPolarity(23), EFF_SnsSentiment::Positive);
	TestEqual(TEXT("관람 불편은 부정"), FFF_SnsFeedGenerator::GetTopicPolarity(25), EFF_SnsSentiment::Negative);

	FFF_SnsFeedSignals After;
	After.LiveElapsedFraction = 0.9f;
	After.bMainShowJustEnded = true;
	TestTrue(TEXT("공연이 막 끝나면 여운 글"), FFF_SnsFeedGenerator::IsTopicEligibleNow(24, After));
	TestFalse(TEXT("끝나면 공연 중 글은 그친다"), FFF_SnsFeedGenerator::IsTopicEligibleNow(23, After));
	return true;
}

// 피날레 후기 — 등급마다 다른 화제로 가고, 미구매·C 는 부정이며, 주기 선택에는 끼지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Sns_FinaleTopicsFollowGrade, "FestaFiesta.NPC.SnsFeedGenerator.FinaleTopicsFollowGrade", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Sns_FinaleTopicsFollowGrade::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("미구매"), FFF_SnsFeedGenerator::GetFinaleVerdictTopicId(EFF_FinaleGrade::None), 18);
	TestEqual(TEXT("C"), FFF_SnsFeedGenerator::GetFinaleVerdictTopicId(EFF_FinaleGrade::C), 19);
	TestEqual(TEXT("B"), FFF_SnsFeedGenerator::GetFinaleVerdictTopicId(EFF_FinaleGrade::B), 20);
	TestEqual(TEXT("A"), FFF_SnsFeedGenerator::GetFinaleVerdictTopicId(EFF_FinaleGrade::A), 21);
	TestEqual(TEXT("S"), FFF_SnsFeedGenerator::GetFinaleVerdictTopicId(EFF_FinaleGrade::S), 22);

	TestEqual(TEXT("기대감은 긍정"), FFF_SnsFeedGenerator::GetTopicPolarity(17), EFF_SnsSentiment::Positive);
	TestEqual(TEXT("미구매는 부정"), FFF_SnsFeedGenerator::GetTopicPolarity(18), EFF_SnsSentiment::Negative);
	TestEqual(TEXT("C 는 아쉬움(부정)"), FFF_SnsFeedGenerator::GetTopicPolarity(19), EFF_SnsSentiment::Negative);
	TestEqual(TEXT("B 는 긍정"), FFF_SnsFeedGenerator::GetTopicPolarity(20), EFF_SnsSentiment::Positive);
	TestEqual(TEXT("S 는 긍정"), FFF_SnsFeedGenerator::GetTopicPolarity(22), EFF_SnsSentiment::Positive);

	TestTrue(TEXT("17~22 는 피날레 연출 후기"), FFF_SnsFeedGenerator::IsFinaleShowTopic(17) && FFF_SnsFeedGenerator::IsFinaleShowTopic(22));
	TestFalse(TEXT("9(축제 총평)는 연출 후기가 아니다"), FFF_SnsFeedGenerator::IsFinaleShowTopic(9));

	// 주기 선택은 피날레 화제를 고르지 않는다 — 기대감이 낮 피드에 섞이면 안 된다
	FFF_SnsFeedSignals Signals;
	for (int32 Sequence = 0; Sequence < 200; ++Sequence)
	{
		const int32 Positive = FFF_SnsFeedGenerator::ChooseTopicId(42, Sequence, EFF_SnsSentiment::Positive, Signals);
		const int32 Negative = FFF_SnsFeedGenerator::ChooseTopicId(42, Sequence, EFF_SnsSentiment::Negative, Signals);
		if (FFF_SnsFeedGenerator::IsFinaleShowTopic(Positive) || FFF_SnsFeedGenerator::IsFinaleShowTopic(Negative))
		{
			AddError(FString::Printf(TEXT("주기 선택이 피날레 화제를 골랐다: %d / %d"), Positive, Negative));
			break;
		}
	}
	return true;
}

// 피날레처럼 몰아 올릴 때는 문장 풀을 한 바퀴 다 쓰기 전에 같은 문장이 다시 나오지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Sns_CycledLinesDoNotRepeatWithinRound, "FestaFiesta.NPC.SnsFeedGenerator.CycledLinesDoNotRepeatWithinRound", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Sns_CycledLinesDoNotRepeatWithinRound::RunTest(const FString& Parameters)
{
	constexpr int32 PoolSize = 7;
	TArray<FFF_SnsPostTemplateRow> RowStorage;
	AddTopicPool(RowStorage, 22, PoolSize);
	TMap<int32, TArray<const FFF_SnsPostTemplateRow*>> TemplatesByTopic;
	BuildPoolsFromStorage(TemplatesByTopic, RowStorage);

	const FFF_SnsFeedSignals Signals;
	for (int32 Round = 0; Round < 3; ++Round)
	{
		TSet<FName> Seen;
		for (int32 Slot = 0; Slot < PoolSize; ++Slot)
		{
			const int32 Cycle = Round * PoolSize + Slot;
			const FFF_SnsPostRow Post = FFF_SnsFeedGenerator::GeneratePost(
				42, 1000 + Cycle, Signals, TemplatesByTopic, 0.f, 22, Cycle);
			Seen.Add(Post.MessageTemplateId);
		}
		TestEqual(FString::Printf(TEXT("%d바퀴째에 문장 %d개를 한 번씩 다 썼다"), Round + 1, PoolSize), Seen.Num(), PoolSize);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
