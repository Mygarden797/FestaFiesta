#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"

// 홍보 구매 판정과 방문객 계산용 순수 구조체.
struct FESTAFIESTA_API FFF_PromotionRules
{
	// 등급에 해당하는 티어. None 이거나 없는 등급이면 nullptr.
	static const FFF_PromotionTier* FindTier(
		EFF_PromotionGrade Grade,
		const FFF_PromotionTier& Basic,
		const FFF_PromotionTier& Focused,
		const FFF_PromotionTier& Influencer);

	// 살 수 있는지 판정한다. 상태를 바꾸지 않는다.
	// OutTier 는 Success 일 때만 채워진다.
	static EFF_PromotionPurchaseResult EvaluatePurchase(
		EFF_PromotionGrade RequestedGrade,
		EFF_PromotionGrade PurchasedGrade,
		int32 CurrentGold,
		const FFF_PromotionTier& Basic,
		const FFF_PromotionTier& Focused,
		const FFF_PromotionTier& Influencer,
		FFF_PromotionTier& OutTier);

	// 홍보를 반영한 하루 방문객.
	// 내림한다 — 견적서가 실제보다 많게 나오면 개장 판단이 낙관적으로 기운다.
	// 배수가 1.0 미만이면 저작 오류이므로 1.0 으로 본다(홍보 티어가 손님을 줄이지는 않는다).
	// [참고] 실제 예상 방문객은 입장료·화제성까지 곱하는 FFF_VisitorForecastRules::BuildForecast 가 낸다.
	static int32 CalcExpectedVisitors(int32 BaseDailyVisitors, float VisitorMultiplier);
};
