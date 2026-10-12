#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"

class UFF_FestivalDefinition;

// 하루 방문객 예보와 그 재료(입장료·화제성 배수)를 계산하는 순수 함수 모음.
struct FESTAFIESTA_API FFF_VisitorForecastRules
{
	// 배수 = 1 - Slope × (Fee - ReferenceFee), [MinMultiplier, MaxMultiplier] 로 자른다.
	static float CalcAdmissionFeeMultiplier(
		int32 FeeGold, int32 ReferenceFeeGold, float Slope, float MinMultiplier, float MaxMultiplier);

	// 배수 = 1 + (Buzz - 50) / 50 × Swing. Buzz 는 0~100 으로 자른 뒤 계산한다.
	static float CalcBuzzMultiplier(float Buzz, float Swing);

	// 어제 만족도(0~100)의 평판 배수. 50 에서 1.0, 0 에서 MinMultiplier, 100 에서 MaxMultiplier 로 직선 보간한다.
	// 어제가 없으면(첫날) 호출하지 말고 1.0 을 쓴다.
	static float CalcReputationMultiplier(float YesterdaySatisfaction, float MinMultiplier, float MaxMultiplier);

	// 홍보 등급의 방문객 배수. 미구매는 NoPromotionVisitorMultiplier.
	static float CalcPromotionMultiplier(EFF_PromotionGrade Grade, const UFF_FestivalDefinition& Definition);

	// 위 세 배수를 곱해 예보를 만든다. 각 단계에서 반올림하지 않고 마지막에 한 번만 내림한다 —
	// 중간에 자르면 같은 입력이라도 곱하는 순서에 따라 1명씩 어긋난다.
	static FFF_VisitorForecast BuildForecast(
		const UFF_FestivalDefinition& Definition,
		EFF_PromotionGrade Grade,
		int32 AdmissionFeeGold,
		float Buzz,
		float ReputationMultiplier = 1.f);
};

// 축제 화제성(Buzz) 규칙. 0~100, 50 이 중립이다.
struct FESTAFIESTA_API FFF_BuzzRules
{
	static constexpr float MinBuzz = 0.f;
	static constexpr float MaxBuzz = 100.f;
	static constexpr float NeutralBuzz = 50.f;

	static float Clamp(float Buzz) { return FMath::Clamp(Buzz, MinBuzz, MaxBuzz); }

	// 중립과의 거리를 DecayRatio 만큼 줄인다. 0 이면 그대로, 1 이면 중립으로 돌아간다.
	static float ApplyDailyDecay(float Buzz, float DecayRatio);

	// SNS 글 한 건의 영향. 긍정 +, 부정 -. 크기 = PerPost × (Salience / 100).
	static float CalcSnsPostDelta(bool bPositive, float Salience0To100, float PerPost);
};
