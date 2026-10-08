// Fill out your copyright notice in the Description page of Project Settings.

#include "NPC/Insights/FF_SnsFeedGenerator.h"

#include "DataTables/FF_SnsPostTemplateRow.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "NPC/IndividualVariation/FF_DeterministicRandom.h"

namespace
{
	void CollectPeriodicTopicIds(EFF_SnsSentiment Polarity, TArray<int32>& OutIds)
	{
		// 9(피날레)는 뺀다 — 주기 선택 대상이 아니라 축제 종료 시점에 ForcedTopicId로만 나간다
		// 17~22(피날레 연출)도 뺀다 — 피날레 단계에서 따로 돈다
		static const int32 AllTopicIds[] = { 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16, 23, 24, 25, 26 };
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
	case 7: case 8: case 9: case 10: case 11: case 14: case 16:
	case 17: case 20: case 21: case 22:
	case 23: case 24: case 26:
		return EFF_SnsSentiment::Positive;
	default:
		return EFF_SnsSentiment::Negative;
	}
}

namespace
{
	// 첫 칸은 비워 둔다. (앞 0, 뒤 0) 이 원문이다
	const TCHAR* const PositivePrefixes[] = { TEXT(""), TEXT("와 "), TEXT("진짜 "), TEXT("헐 "), TEXT("대박 "), TEXT("와 진짜 ") };
	const TCHAR* const PositiveSuffixes[] = { TEXT(""), TEXT("!!"), TEXT(" ㅠㅠ"), TEXT("ㅋㅋㅋ"), TEXT(" 최고"), TEXT(" 미쳤다"), TEXT("~~") };
	const TCHAR* const NegativePrefixes[] = { TEXT(""), TEXT("음... "), TEXT("아 "), TEXT("흠 "), TEXT("솔직히 "), TEXT("에휴 ") };
	const TCHAR* const NegativeSuffixes[] = { TEXT(""), TEXT("..."), TEXT("ㅋㅋ"), TEXT(" ㅠ"), TEXT(" 아쉽다"), TEXT("..ㅎ") };
}

int32 FFF_SnsFeedGenerator::GetDecorationVariantCount(EFF_SnsSentiment Polarity)
{
	return Polarity == EFF_SnsSentiment::Positive
		? UE_ARRAY_COUNT(PositivePrefixes) * UE_ARRAY_COUNT(PositiveSuffixes)
		: UE_ARRAY_COUNT(NegativePrefixes) * UE_ARRAY_COUNT(NegativeSuffixes);
}

FString FFF_SnsFeedGenerator::DecorateBody(const FString& Body, EFF_SnsSentiment Polarity, int32 Variant)
{
	const bool bPositive = Polarity == EFF_SnsSentiment::Positive;
	const int32 PrefixCount = bPositive ? UE_ARRAY_COUNT(PositivePrefixes) : UE_ARRAY_COUNT(NegativePrefixes);
	const int32 SuffixCount = bPositive ? UE_ARRAY_COUNT(PositiveSuffixes) : UE_ARRAY_COUNT(NegativeSuffixes);
	const int32 Index = FMath::Abs(Variant) % (PrefixCount * SuffixCount);
	const TCHAR* Prefix = bPositive ? PositivePrefixes[Index / SuffixCount] : NegativePrefixes[Index / SuffixCount];
	const TCHAR* Suffix = bPositive ? PositiveSuffixes[Index % SuffixCount] : NegativeSuffixes[Index % SuffixCount];
	return FString(Prefix) + Body + Suffix;
}

FString FFF_SnsFeedGenerator::MakeUniqueDecoratedBody(
	const FString& Base, EFF_SnsSentiment Polarity, int32 Round, uint32 Salt, TSet<FString>& Used)
{
	const int32 VariantCount = GetDecorationVariantCount(Polarity);
	const int32 Decorated = VariantCount - 1;
	int32 Variant = Round <= 0 ? 0 : 1 + static_cast<int32>((static_cast<uint32>(Round) * 7u + Salt % 13u) % static_cast<uint32>(Decorated));
	FString Body = DecorateBody(Base, Polarity, Variant);
	for (int32 Attempt = 1; Used.Contains(Body) && Attempt < VariantCount; ++Attempt)
	{
		Variant = 1 + Variant % Decorated;
		Body = DecorateBody(Base, Polarity, Variant);
	}
	Used.Add(Body);
	return Body;
}

int32 FFF_SnsFeedGenerator::GetFinaleVerdictTopicId(const EFF_FinaleGrade Grade)
{
	switch (Grade)
	{
	case EFF_FinaleGrade::C: return 19;
	case EFF_FinaleGrade::B: return 20;
	case EFF_FinaleGrade::A: return 21;
	case EFF_FinaleGrade::S: return 22;
	default:                 return 18;
	}
}

bool FFF_SnsFeedGenerator::IsTopicEligibleNow(int32 TopicId, const FFF_SnsFeedSignals& Signals)
{
	switch (TopicId)
	{
	case 1: // 대기시간 김 — 대기를 마친 시설이 있어야 가리킬 곳이 생긴다
		return !Signals.WorstWaitFacilityDisplayName.IsEmpty();
	case 3: // 취향 불일치 — 무언가를 해 봐야 취향에 안 맞는다고 쓴다
	case 7: // 시설 이용 후 만족
	case 11: // 한산해서 쾌적함 — 문장이 "즐겼다"처럼 과거형이다
		return Signals.bFacilityUsedToday;
	case 8: // 공연 직전 기대감 — 공연 락 구간에서, 공연이 시작되기 전까지만
		return Signals.bMainProgramLocked && !Signals.bMainShowRunning;
	case 10: // 인기 시설 — 실제로 줄이 있어야 한다
		return Signals.MaxWaitingCount > 0;
	case 14: // 재방문 — 2일차부터. 1일차는 Live 후반에, 시설을 써 본 뒤에만
		return Signals.bFacilityUsedToday && (Signals.CurrentDay >= 2 || Signals.LiveElapsedFraction >= 0.5f);
	case 23: // 공연 중
	case 25: // 공연 관람 불편
		return Signals.bMainShowRunning;
	case 24: // 공연 여운
		return Signals.bMainShowJustEnded;
	case 26: // 개장 기대감 — 하루 시작 직후
		return Signals.LiveElapsedFraction < 0.15f;
	case 12: // 축제 초반 시설 부족 — 1일차에서만
		return Signals.CurrentDay == 1;
	case 15: // 입장료 비쌈 — 기준값보다 비쌀 때만
		return Signals.AdmissionFeeGold > Signals.ReferenceAdmissionFeeGold;
	case 16: // 입장료 가성비 — 기준값보다 쌀 때만
		return Signals.AdmissionFeeGold < Signals.ReferenceAdmissionFeeGold;
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
	case 15: // 입장료 비쌈 — 기준값에서 상한까지 0→100
		return CalcAdmissionFeeTension(Signals, /*bExpensive=*/true);
	case 16: // 입장료 가성비 — 기준값에서 0 Gold 까지 0→100
		return CalcAdmissionFeeTension(Signals, /*bExpensive=*/false);
	case 23: // 공연 중 — 공연 시간엔 공연 얘기가 대부분이어야 한다
		return 90.f;
	case 24: // 공연 여운
		return 70.f;
	case 25: // 공연 관람 불편 — 붐빌수록
		return 100.f - Signals.DensityScore0To100;
	case 26: // 개장 기대감 — 아직 겪은 게 없으니 이 화제가 대부분이다
		return 80.f;
	default:
		return 0.f;
	}
}

float FFF_SnsFeedGenerator::CalcAdmissionFeeTension(const FFF_SnsFeedSignals& Signals, const bool bExpensive)
{
	const int32 Fee = Signals.AdmissionFeeGold;
	const int32 Reference = Signals.ReferenceAdmissionFeeGold;
	if (bExpensive)
	{
		const int32 Span = FMath::Max(Signals.MaxAdmissionFeeGold - Reference, 1);
		return Fee > Reference ? FMath::Clamp(100.f * (Fee - Reference) / Span, 0.f, 100.f) : 0.f;
	}
	const int32 Span = FMath::Max(Reference, 1);
	return Fee < Reference ? FMath::Clamp(100.f * (Reference - Fee) / Span, 0.f, 100.f) : 0.f;
}

int32 FFF_SnsFeedGenerator::CalcPolaritySatisfaction(const FFF_SnsFeedSignals& Signals)
{
	// 입장료가 비싸면 부정 글이 늘고 싸면 긍정 글이 는다. 만족도 자체는 바꾸지 않는다 —
	// 만족도는 시설 경험의 값이고, 이건 "글을 쓰는 기분" 에만 얹는다.
	const float Expensive = CalcAdmissionFeeTension(Signals, true) / 100.f;
	const float Cheap = CalcAdmissionFeeTension(Signals, false) / 100.f;
	return FMath::Clamp(FMath::RoundToInt32(
		Signals.SimSatisfaction0To100 - ExpensivePolarityPenalty * Expensive + CheapPolarityBonus * Cheap), 0, 100);
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

	// 조건을 무시하고 아무 화제나 고르면 첫날 아침에 "또 왔다" 같은 글이 나간다. 없으면 없다고 돌려준다.
	if (Candidates.IsEmpty())
	{
		return INDEX_NONE;
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

const FFF_SnsPostTemplateRow* FFF_SnsFeedGenerator::PickCycledTemplateRow(
	int32 GlobalSimulationSeed, int32 TopicId, int32 CycleIndex,
	const TMap<int32, TArray<const FFF_SnsPostTemplateRow*>>& TemplatesByTopic)
{
	const TArray<const FFF_SnsPostTemplateRow*>* Pool = TemplatesByTopic.Find(TopicId);
	if (!Pool || Pool->IsEmpty())
	{
		return nullptr;
	}

	const int32 Count = Pool->Num();
	const int32 Round = FMath::Max(0, CycleIndex) / Count;
	const int32 Slot = FMath::Max(0, CycleIndex) % Count;

	// Fisher-Yates. 바퀴마다 시드가 달라 같은 순서가 반복되지 않는다.
	TArray<int32> Order;
	Order.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		Order.Add(i);
	}
	for (int32 i = Count - 1; i > 0; --i)
	{
		const uint32 Seed = FFF_DeterministicRandom::MakeTraitSeed(
			GlobalSimulationSeed, Round * 1000 + i, TEXT("sns_line_cycle"), TopicId, TEXT("SnsContent"));
		const int32 j = FMath::Clamp(FMath::FloorToInt(FFF_DeterministicRandom::MakeUniformFraction(Seed) * (i + 1)), 0, i);
		Order.Swap(i, j);
	}
	return (*Pool)[Order[Slot]];
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
	int32 ForcedTopicId,
	int32 LineCycleIndex)
{
	EFF_SnsSentiment Polarity = EFF_SnsSentiment::Positive;
	int32 TopicId = ForcedTopicId;
	float Salience = 100.f; // 강제 화제는 게이트를 통과했다는 사실 자체가 신호라 고정값

	if (TopicId == INDEX_NONE)
	{
		Polarity = RollPolarityIsPositive(GlobalSimulationSeed, PostSequenceIndex, CalcPolaritySatisfaction(Signals))
			? EFF_SnsSentiment::Positive : EFF_SnsSentiment::Negative;
		TopicId = ChooseTopicId(GlobalSimulationSeed, PostSequenceIndex, Polarity, Signals);
		if (TopicId == INDEX_NONE)
		{
			// 이 극성에 지금 쓸 화제가 없으면 반대 극성에서 찾는다. 그래도 없으면 글 없이 돌려준다
			Polarity = Polarity == EFF_SnsSentiment::Positive ? EFF_SnsSentiment::Negative : EFF_SnsSentiment::Positive;
			TopicId = ChooseTopicId(GlobalSimulationSeed, PostSequenceIndex, Polarity, Signals);
		}
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

	const FFF_SnsPostTemplateRow* Row = LineCycleIndex == INDEX_NONE
		? PickTemplateRow(GlobalSimulationSeed, PostSequenceIndex, TopicId, TemplatesByTopic)
		: PickCycledTemplateRow(GlobalSimulationSeed, TopicId, LineCycleIndex, TemplatesByTopic);
	if (Row)
	{
		NewPost.MessageTemplateId = Row->TemplateId;
		NewPost.Body = ResolveBodyText(*Row, Signals);
	}

	NewPost.GameTimestampSeconds = GameTimestampSeconds;

	return NewPost;
}
