// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Insights/FF_SnsFeedGenerator.h"
#include "DataTables/FF_SnsPostTemplateRow.h"

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

// {시설} 자리표시자가 있는 행은 실제 시설 데이터가 없는 지금 후보에서 빠져야 한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SnsFeedGenerator_ExcludesFacilityPlaceholderRows, "FestaFiesta.NPC.SnsFeedGenerator.ExcludesFacilityPlaceholderRows", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SnsFeedGenerator_ExcludesFacilityPlaceholderRows::RunTest(const FString& Parameters)
{
	FFF_SnsPostTemplateRow WithPlaceholder;
	WithPlaceholder.bUsesFacilityPlaceholder = true;
	TestFalse(TEXT("{시설} 있는 행은 제외된다"), FFF_SnsFeedGenerator::ShouldIncludeTemplateRow(WithPlaceholder));

	FFF_SnsPostTemplateRow WithoutPlaceholder;
	WithoutPlaceholder.bUsesFacilityPlaceholder = false;
	TestTrue(TEXT("일반 문장은 포함된다"), FFF_SnsFeedGenerator::ShouldIncludeTemplateRow(WithoutPlaceholder));

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

#endif // WITH_DEV_AUTOMATION_TESTS
