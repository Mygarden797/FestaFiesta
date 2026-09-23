// Fill out your copyright notice in the Description page of Project Settings.

#include "NPC/Insights/FF_SnsFeedGenerator.h"

#include "DataTables/FF_SnsPostTemplateRow.h"
#include "NPC/IndividualVariation/FF_DeterministicRandom.h"

namespace
{
	void CollectPeriodicTopicIds(EFF_SnsSentiment Polarity, TArray<int32>& OutIds)
	{
		// 9(피날레)는 뺀다 — 주기 선택 대상이 아니라 Report 단계 진입 시점에 ForcedTopicId로만 나간다
		static const int32 AllTopicIds[] = { 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14 };
		for (int32 Id : AllTopicIds)
		{
			if (FFF_SnsFeedGenerator::GetTopicPolarity(Id) == Polarity)
			{
				OutIds.Add(Id);
			}
		}
	}
}

EFF_SnsSentiment FFF_SnsFeedGenerator::GetTopicPolarity(int32 TopicId)
{
	switch (TopicId)
	{
	case 7: case 8: case 9: case 10: case 11: case 14:
		return EFF_SnsSentiment::Positive;
	default:
		return EFF_SnsSentiment::Negative;
	}
}

bool FFF_SnsFeedGenerator::IsTopicEligibleNow(int32 TopicId, const FFF_SnsFeedSignals& Signals)
{
	switch (TopicId)
	{
	case 8: // 공연 직전 기대감 — 공연 락 구간에서만
		return Signals.bMainProgramLocked;
	case 12: // 축제 초반 시설 부족 — 1일차에서만
		return Signals.CurrentDay == 1;
	default:
		return true;
	}
}

float FFF_SnsFeedGenerator::ComputeTopicScore(int32 TopicId, const FFF_SnsFeedSignals& Signals)
{
	switch (TopicId)
	{
	case 1: // 대기시간 김
		return 100.f - Signals.WaitScore0To100;
	case 2: // 시설 부족 — Need 충족도 낮음으로 근사(전용 신호 없음)
		return 100.f - Signals.NeedFulfillmentScore0To100;
	case 3: // 취향 불일치
		return 100.f - Signals.PreferenceScore0To100;
	case 4: // 붐빔
		return 100.f - Signals.DensityScore0To100;
	case 5: // 이동실패·입장거절
	case 6: // 인내심 초과 이탈 — ⑤와 신호원 공유. FailedDestinationPenalized가
			// MoveFailed/JoinRejected/QueuePatienceExpired를 이미 합쳐서 세기 때문에
			// (FF_CrowdDecisionSubsystem.cpp:653) 원인별로 분리할 데이터가 없다
		return static_cast<float>(Signals.FailedDestinationDeltaThisWindow) * 10.f;
	case 7: // 시설 이용 후 만족
		return static_cast<float>(Signals.NeedFulfillmentScore0To100);
	case 8: // 공연 직전 기대감
		return Signals.StimulationAverage0To1 * 100.f;
	case 10: // 인기 시설 — 제일 붐비는 시설의 대기 인원 수를 근사치로 쓴다
		return FMath::Clamp(static_cast<float>(Signals.MaxWaitingCount) * 5.f, 0.f, 100.f);
	case 11: // 한산해서 쾌적함
		return static_cast<float>(Signals.DensityScore0To100);
	case 12: // 축제 초반 시설 부족 — 시설 수가 적을수록 높은 점수
		return FMath::Clamp(100.f - static_cast<float>(Signals.FacilityCount) * 10.f, 0.f, 100.f);
	case 13: // 구역 쏠림 — 혼잡 셀 수를 거친 근사치. Zone별 밀도 분해가 없어 전역값을 대신 쓴다
		return FMath::Clamp(static_cast<float>(Signals.CongestedCellCount) * 10.f, 0.f, 100.f);
	case 14: // 재방문·단골 — utility_ai_todo.md §10 결정대로 가볍게, 고정 낮은 점수
		return 15.f;
	default:
		return 0.f;
	}
}

bool FFF_SnsFeedGenerator::RollPolarityIsPositive(
	int32 GlobalSimulationSeed, int32 PostSequenceIndex, int32 SimSatisfaction0To100)
{
	const uint32 Seed = FFF_DeterministicRandom::MakeTraitSeed(
		GlobalSimulationSeed, PostSequenceIndex, TEXT("sns_polarity"), 0, TEXT("SnsContent"));
	const float Fraction = FFF_DeterministicRandom::MakeUniformFraction(Seed);
	return Fraction < (static_cast<float>(SimSatisfaction0To100) / 100.f);
}

int32 FFF_SnsFeedGenerator::ChooseTopicId(
	int32 GlobalSimulationSeed, int32 PostSequenceIndex, EFF_SnsSentiment Polarity,
	const FFF_SnsFeedSignals& Signals)
{
	TArray<int32> AllForPolarity;
	CollectPeriodicTopicIds(Polarity, AllForPolarity);

	TArray<int32> Candidates;
	for (int32 Id : AllForPolarity)
	{
		if (IsTopicEligibleNow(Id, Signals))
		{
			Candidates.Add(Id);
		}
	}

	if (Candidates.IsEmpty())
	{
		Candidates = AllForPolarity;
	}

	TArray<float> Scores;
	Scores.Reserve(Candidates.Num());
	float ScoreSum = 0.f;
	for (int32 Id : Candidates)
	{
		const float Score = FMath::Max(0.f, ComputeTopicScore(Id, Signals));
		Scores.Add(Score);
		ScoreSum += Score;
	}

	const uint32 Seed = FFF_DeterministicRandom::MakeTraitSeed(
		GlobalSimulationSeed, PostSequenceIndex, TEXT("sns_topic"), 0, TEXT("SnsContent"));
	const float Fraction = FFF_DeterministicRandom::MakeUniformFraction(Seed);

	if (ScoreSum <= KINDA_SMALL_NUMBER)
	{
		const int32 Index = FMath::Clamp(FMath::FloorToInt(Fraction * Candidates.Num()), 0, Candidates.Num() - 1);
		return Candidates[Index];
	}

	float Threshold = Fraction * ScoreSum;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		Threshold -= Scores[i];
		if (Threshold <= 0.f)
		{
			return Candidates[i];
		}
	}

	return Candidates.Last();
}

const FFF_SnsPostTemplateRow* FFF_SnsFeedGenerator::PickTemplateRow(
	int32 GlobalSimulationSeed, int32 PostSequenceIndex, int32 TopicId,
	const TMap<int32, TArray<const FFF_SnsPostTemplateRow*>>& TemplatesByTopic)
{
	const TArray<const FFF_SnsPostTemplateRow*>* Pool = TemplatesByTopic.Find(TopicId);
	if (!Pool || Pool->IsEmpty())
	{
		return nullptr;
	}

	const uint32 Seed = FFF_DeterministicRandom::MakeTraitSeed(
		GlobalSimulationSeed, PostSequenceIndex, TEXT("sns_post_line"), TopicId, TEXT("SnsContent"));
	const float Fraction = FFF_DeterministicRandom::MakeUniformFraction(Seed);
	const int32 Index = FMath::Clamp(FMath::FloorToInt(Fraction * Pool->Num()), 0, Pool->Num() - 1);
	return (*Pool)[Index];
}

FString FFF_SnsFeedGenerator::MakeDisplayHandle(int32 GlobalSimulationSeed, int32 PostSequenceIndex)
{
	const uint32 Seed = FFF_DeterministicRandom::MakeTraitSeed(
		GlobalSimulationSeed, PostSequenceIndex, TEXT("sns_handle"), 0, TEXT("SnsContent"));
	const float Fraction = FFF_DeterministicRandom::MakeUniformFraction(Seed);
	const int32 Number = 1000 + FMath::Clamp(FMath::FloorToInt(Fraction * 9000.f), 0, 8999);
	return FString::Printf(TEXT("방문객 #%d"), Number);
}

bool FFF_SnsFeedGenerator::ShouldIncludeTemplateRow(const FFF_SnsPostTemplateRow& Row)
{
	if (!Row.bUsesFacilityPlaceholder)
	{
		return true;
	}

	// 화제1(최장 대기)·10(최다 대기줄)·7(만족)·14(재방문)만 어떤 시설을 가리켜야 하는지가 분명하다.
	// 7·14 는 "줄이 가장 긴 곳 = 사람들이 좋아하는 곳" 으로 10 과 같은 신호를 쓴다.
	// 나머지 화제는 가리킬 시설을 정할 신호가 없어 계속 뺀다
	return Row.TopicId == 1 || Row.TopicId == 10 || Row.TopicId == 7 || Row.TopicId == 14;
}

FText FFF_SnsFeedGenerator::ResolveBodyText(const FFF_SnsPostTemplateRow& Row, const FFF_SnsFeedSignals& Signals)
{
	if (!Row.bUsesFacilityPlaceholder)
	{
		return Row.Text;
	}

	FString FacilityName;
	if (Row.TopicId == 1)
	{
		FacilityName = Signals.WorstWaitFacilityDisplayName;
	}
	else if (Row.TopicId == 10 || Row.TopicId == 7 || Row.TopicId == 14)
	{
		FacilityName = Signals.MostQueuedFacilityDisplayName;
	}

	if (FacilityName.IsEmpty())
	{
		FacilityName = TEXT("여기"); // 그 순간 가리킬 시설이 없을 때(예: 아직 시설이 하나도 없음)의 안전한 대체
	}

	return FText::FromString(Row.Text.ToString().Replace(TEXT("{시설}"), *FacilityName));
}

bool FFF_SnsFeedGenerator::DoesSentimentMatchTopic(const FFF_SnsPostTemplateRow& Row)
{
	const bool bDeclaredPositive = Row.Sentiment == TEXT("positive");
	const bool bDeclaredNegative = Row.Sentiment == TEXT("negative");
	if (!bDeclaredPositive && !bDeclaredNegative)
	{
		return false; // 오타·빈 값 — "positive"/"negative" 둘 다 아니면 불일치로 취급
	}

	const bool bActualPositive = GetTopicPolarity(Row.TopicId) == EFF_SnsSentiment::Positive;
	return bDeclaredPositive == bActualPositive;
}

float FFF_SnsFeedGenerator::NextIntervalSeconds(
	int32 GlobalSimulationSeed, int32 PostSequenceIndex, float MinSeconds, float MaxSeconds)
{
	const uint32 Seed = FFF_DeterministicRandom::MakeTraitSeed(
		GlobalSimulationSeed, PostSequenceIndex, TEXT("sns_interval"), 0, TEXT("SnsContent"));
	const float Fraction = FFF_DeterministicRandom::MakeUniformFraction(Seed);
	return FMath::Lerp(MinSeconds, FMath::Max(MinSeconds, MaxSeconds), Fraction);
}

FFF_SnsPostRow FFF_SnsFeedGenerator::GeneratePost(
	int32 GlobalSimulationSeed,
	int32 PostSequenceIndex,
	const FFF_SnsFeedSignals& Signals,
	const TMap<int32, TArray<const FFF_SnsPostTemplateRow*>>& TemplatesByTopic,
	float GameTimestampSeconds,
	int32 ForcedTopicId)
{
	EFF_SnsSentiment Polarity = EFF_SnsSentiment::Positive;
	int32 TopicId = ForcedTopicId;
	float Salience = 100.f; // 강제 화제는 게이트를 통과했다는 사실 자체가 신호라 고정값

	if (TopicId == INDEX_NONE)
	{
		Polarity = RollPolarityIsPositive(GlobalSimulationSeed, PostSequenceIndex, Signals.SimSatisfaction0To100)
			? EFF_SnsSentiment::Positive : EFF_SnsSentiment::Negative;
		TopicId = ChooseTopicId(GlobalSimulationSeed, PostSequenceIndex, Polarity, Signals);
		Salience = FMath::Clamp(ComputeTopicScore(TopicId, Signals), 0.f, 100.f);
	}
	else
	{
		Polarity = GetTopicPolarity(TopicId);
	}

	FFF_SnsPostRow NewPost;
	NewPost.TopicSalience0To100 = Salience;

	const uint32 GuidSeedA = FFF_DeterministicRandom::MakeTraitSeed(
		GlobalSimulationSeed, PostSequenceIndex, TEXT("sns_post_id_a"), 0, TEXT("SnsContent"));
	const uint32 GuidSeedB = FFF_DeterministicRandom::MakeTraitSeed(
		GlobalSimulationSeed, PostSequenceIndex, TEXT("sns_post_id_b"), 0, TEXT("SnsContent"));
	NewPost.PostId = FGuid(GuidSeedA, GuidSeedB,
		static_cast<uint32>(GlobalSimulationSeed), static_cast<uint32>(PostSequenceIndex));

	NewPost.DisplayHandle = MakeDisplayHandle(GlobalSimulationSeed, PostSequenceIndex);
	// 화제1·10은 시설 실명을 Body에 직접 치환해 넣는다(ResolveBodyText) — 이 필드는 그것과
	// 별개로 구조화된 카테고리 노출 용도인데 아직 쓰는 곳이 없어 NAME_None으로 둔다
	NewPost.FacilityCategory = NAME_None;
	NewPost.Sentiment = Polarity;
	NewPost.TopicId = TopicId;

	if (const FFF_SnsPostTemplateRow* Row =
			PickTemplateRow(GlobalSimulationSeed, PostSequenceIndex, TopicId, TemplatesByTopic))
	{
		NewPost.MessageTemplateId = Row->TemplateId;
		NewPost.Body = ResolveBodyText(*Row, Signals);
	}

	NewPost.GameTimestampSeconds = GameTimestampSeconds;

	return NewPost;
}
