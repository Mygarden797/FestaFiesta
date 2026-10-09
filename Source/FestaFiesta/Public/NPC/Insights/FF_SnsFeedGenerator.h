// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NPC/Insights/FF_SnsTypes.h"

struct FFF_SnsPostTemplateRow;
enum class EFF_FinaleGrade : uint8;

// SNS 피드 화제·문장 선택의 입력값. 전부 다른 서브시스템의 Get API에서 그대로 읽어온
// 스냅샷이다 — 이 구조체는 어떤 UObject도 참조하지 않는다.
struct FFF_SnsFeedSignals
{
	int32 SimSatisfaction0To100 = 50;
	int32 WaitScore0To100 = 50;
	int32 DensityScore0To100 = 50;
	int32 PreferenceScore0To100 = 50;
	int32 NeedFulfillmentScore0To100 = 50;

	// UFF_NpcNeedMetricsSubsystem::GetNeedAverage(Stimulation), 0~1 스케일
	float StimulationAverage0To1 = 0.f;

	// UFF_CrowdDecisionSubsystem::GetDecisionStats().FailedDestinationPenalized의
	// 지난 관측 이후 변화량(MoveFailed/JoinRejected/QueuePatienceExpired 합산 — §10 화제 5·6이 공유)
	int32 FailedDestinationDeltaThisWindow = 0;

	// UFF_CrowdSimulationSubsystem::GetCongestedCellCount()
	int32 CongestedCellCount = 0;

	bool bMainProgramLocked = false;
	int32 CurrentDay = 1;

	// 오늘 Live 가 얼마나 지났는지(0~1). 개장 기대감(26)과 1일차 재방문(14) 판정에 쓴다
	float LiveElapsedFraction = 0.f;

	// 오늘 시설을 실제로 쓴 손님이 한 명이라도 있었는지. 없으면 만족·한산·취향·재방문처럼
	// "이미 겪은" 글은 나가지 않는다
	bool bFacilityUsedToday = false;

	// 메인 공연이 지금 진행 중인지(23·25), 방금 끝났는지(24)
	bool bMainShowRunning = false;
	bool bMainShowJustEnded = false;

	// 이하 넷은 UFFZSWorldSubsystem::BuildReadModel().Facilities에서 뽑는다
	// 화제10·12 점수와 화제1·7·10·14의 {시설} 치환에 쓴다. 설치된 시설이 하나도 없으면 전부 기본값(0·빈 문자열)

	// 화제12(축제 초반 시설 부족) 점수 산출용 — 설치된 시설 총 수
	int32 FacilityCount = 0;

	// 화제1(대기시간 김)의 {시설} 치환용 — AverageCompletedWaitSeconds가 가장 큰 시설의 이름
	FString WorstWaitFacilityDisplayName;

	// 화제10(인기 시설) 점수 산출 + 화제7·10·14 {시설} 치환용 — WaitingCount가 가장 큰 시설과 그 값
	FString MostQueuedFacilityDisplayName;
	int32 MaxWaitingCount = 0;

	// 화제27(어제 글 보고 왔다)의 {시설} 치환용 — 어제 매출이 가장 많았던 시설 종류의 이름. 1일차·어제 매출 없음이면 빈 문자열
	FString YesterdayTopFacilityDisplayName;

	// 화제15(입장료 비쌈)·16(입장료 가성비) 점수용. 기준값보다 비쌀수록 15, 쌀수록 16 이 높다.
	// 셋 다 0 이면(축제 전) 두 화제 점수가 0 이다
	int32 AdmissionFeeGold = 0;
	int32 ReferenceAdmissionFeeGold = 0;
	int32 MaxAdmissionFeeGold = 0;
};

// SNS 피드 화제 선택·문장 선택의 순수 계산기.
class FESTAFIESTA_API FFF_SnsFeedGenerator
{
public:
	/**
	 * 글 한 건을 만든다.
	 * 극성 = 톤(긍/부정), 화제 = 소재(무엇에 대해), 문장 = 실제 텍스트
	 * @param ForcedTopicId INDEX_NONE이면 2단 선택(극성→화제)을 돌린다. 값을 주면 그 화제로
	 *                      고정한다 — 피날레(화제9)처럼 주기 선택이 아니라 단계 전환 시점에
	 *                      한 번만 나가는 화제용이다.
	 * @param LineCycleIndex INDEX_NONE이면 문장을 매번 독립적으로 뽑는다. 값을 주면 그 화제의
	 *                       문장을 섞은 순서대로 돌려, 풀을 한 바퀴 다 쓰기 전에는 같은 문장이
	 *                       다시 나오지 않는다. 짧은 시간에 몰아 올리는 피날레 후기용이다.
	 */
	static FFF_SnsPostRow GeneratePost(
		int32 GlobalSimulationSeed,
		int32 PostSequenceIndex,
		const FFF_SnsFeedSignals& Signals,
		const TMap<int32, TArray<const FFF_SnsPostTemplateRow*>>& TemplatesByTopic,
		float GameTimestampSeconds,
		int32 ForcedTopicId = INDEX_NONE,
		int32 LineCycleIndex = INDEX_NONE);

	static float NextIntervalSeconds(
		int32 GlobalSimulationSeed, int32 PostSequenceIndex, float MinSeconds, float MaxSeconds);

	// 피날레 연출 중 후기. 처음 몇 초는 기대감(화제17)이고, 그 뒤로는 구매 등급의 평가 화제가 나간다.
	static constexpr int32 FinaleHypeTopicId = 17;
	static constexpr int32 FinaleTopicIdMin = 17;
	static constexpr int32 FinaleTopicIdMax = 22;

	// 한산(11) 글은 밀도 점수가 이 값 이상이고 붐비는 칸이 없을 때만 나간다
	static constexpr int32 CalmDensityScoreAtLeast = 85;

	// 미구매 18 · C 19 · B 20 · A 21 · S 22
	static int32 GetFinaleVerdictTopicId(EFF_FinaleGrade Grade);

	/**
	 * 문장 앞뒤에 추임새를 붙인 변형. Variant 0 은 원문이고, 1 부터는 (앞, 뒤) 추임새 조합을 하나씩 쓴다.
	 * 피날레는 문장 수보다 글이 많이 나가서 같은 문장이 두 번 보이지 않게 하는 데 쓴다.
	 * 긍정과 부정은 추임새가 다르다("최고"가 아쉬운 글에 붙으면 안 된다).
	 */
	static FString DecorateBody(const FString& Body, EFF_SnsSentiment Polarity, int32 Variant);
	static int32 GetDecorationVariantCount(EFF_SnsSentiment Polarity);

	/**
	 * Used 에 없는 문장을 만들어 Used 에 넣고 돌려준다. Round 0(풀 첫 바퀴)은 원문이고, 그 뒤 바퀴는 추임새를 붙인다.
	 * Salt 는 문장마다 출발 조합을 어긋나게 해 같은 바퀴의 글이 모두 같은 추임새를 쓰지 않게 한다.
	 */
	static FString MakeUniqueDecoratedBody(const FString& Base, EFF_SnsSentiment Polarity, int32 Round, uint32 Salt, TSet<FString>& Used);

	// 피날레 연출 중 후기(화제17~22)인지. 평가 직전이라 화제성에 반영하지 않는다.
	static bool IsFinaleShowTopic(int32 TopicId)
	{
		return TopicId >= FinaleTopicIdMin && TopicId <= FinaleTopicIdMax;
	}

	/**
	 * 이 문장을 지금 후보 풀에 넣어도 되는지. `{시설}` 자리표시자가 있는 행은 화제1·10만
	 * 통과시킨다 — 이 둘만 "어떤 시설을 가리켜야 하는지"가 신호로 분명하기 때문이다
	 * (화제1=제일 오래 기다리는 곳, 화제10=제일 붐비는 곳). 화제2·3·5·7·14는 가리킬 시설을
	 * 정할 신호가 아직 없어서 계속 뺀다(§13 참조).
	 */
	static bool ShouldIncludeTemplateRow(const FFF_SnsPostTemplateRow& Row);

	/**
	 * 고른 문장의 `{시설}`을 실제 이름으로 채운다. 화제1·10이 아니거나 애초에 자리표시자가
	 * 없으면 원문 그대로. 그 순간 이름 신호가 비어 있으면(시설이 하나도 없는 등) "여기"로
	 * 대체해 깨진 텍스트를 막는다.
	 */
	static FText ResolveBodyText(const FFF_SnsPostTemplateRow& Row, const FFF_SnsFeedSignals& Signals);

	// CSV의 Sentiment 열이 GetTopicPolarity(Row.TopicId)와 일치하는지. 실제 극성 판정에는
	// 안 쓰이고(그건 GetTopicPolarity 전용), 콘텐츠 작성 실수를 로드 시점에 잡기 위한 것이다
	static bool DoesSentimentMatchTopic(const FFF_SnsPostTemplateRow& Row);

	// 화제 번호 → 극성 고정 매핑표
	// 화제 9(피날레)는 주기 선택 대상이 아니다 — §10대로 Report 단계 진입 시점에 한 번만 나간다
	static EFF_SnsSentiment GetTopicPolarity(int32 TopicId);
	static bool IsTopicEligibleNow(int32 TopicId, const FFF_SnsFeedSignals& Signals);
	
	// 화제별로 "지금 이게 얼마나 뽑힐 만한가"를 0~100 숫자로
	static float ComputeTopicScore(int32 TopicId, const FFF_SnsFeedSignals& Signals);

	// 입장료가 기준값에서 얼마나 벗어났나(0~100). bExpensive 면 비싼 쪽(상한에서 100), 아니면 싼 쪽(0 Gold 에서 100).
	static float CalcAdmissionFeeTension(const FFF_SnsFeedSignals& Signals, bool bExpensive);

	// 긍정/부정 추첨에 쓰는 만족도. 입장료가 상한이면 ExpensivePolarityPenalty 만큼 깎고, 0 이면 CheapPolarityBonus 만큼 더한다.
	static int32 CalcPolaritySatisfaction(const FFF_SnsFeedSignals& Signals);

	static constexpr float ExpensivePolarityPenalty = 25.f;
	static constexpr float CheapPolarityBonus = 10.f;
	
	// 극성이 정해진 다음 화제를 고르는 함수. 지금 조건에 맞는 화제가 없으면 INDEX_NONE
	static int32 ChooseTopicId(
		int32 GlobalSimulationSeed, int32 PostSequenceIndex, EFF_SnsSentiment Polarity,
		const FFF_SnsFeedSignals& Signals);
	
	// 극성 동전던지기
	static bool RollPolarityIsPositive(
		int32 GlobalSimulationSeed, int32 PostSequenceIndex, int32 SimSatisfaction0To100);

private:
	// 화제가 정해지면 그 화제의 문장 풀(TemplatesByTopic에서 찾음)에서 결정론적 인덱스 하나를 뽑아 실제 행(문장)을 리턴
	static const FFF_SnsPostTemplateRow* PickTemplateRow(
		int32 GlobalSimulationSeed, int32 PostSequenceIndex, int32 TopicId,
		const TMap<int32, TArray<const FFF_SnsPostTemplateRow*>>& TemplatesByTopic);
	// 화제의 문장 풀을 바퀴마다 다른 순서로 섞어 CycleIndex 번째 문장을 고른다
	static const FFF_SnsPostTemplateRow* PickCycledTemplateRow(
		int32 GlobalSimulationSeed, int32 TopicId, int32 CycleIndex,
		const TMap<int32, TArray<const FFF_SnsPostTemplateRow*>>& TemplatesByTopic);
	static FString MakeDisplayHandle(int32 GlobalSimulationSeed, int32 PostSequenceIndex);
};
