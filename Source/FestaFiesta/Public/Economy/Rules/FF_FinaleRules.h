#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"

/**
 * 피날레 구매 판정 용 순수 구조체
 *
 * 구매 시점(Day3 MorningCheck)은 여기서 보지 않는다. 단계 판정은
 * UFF_StageEconomyBridgeSubsystem::CanPurchaseFinaleNow() 가 갖는다
 */
struct FESTAFIESTA_API FFF_FinaleRules
{
	// 등급으로 티어를 찾는다. 없으면 nullptr.
	static const FFF_FinaleTier* FindTier(
		const TArray<FFF_FinaleTier>& Tiers,
		EFF_FinaleGrade Grade);
	
	// 살 수 있는지 판정한다. 상태를 바꾸지 않는다.
	// OutTier 는 Success 일 때만 채워진다.
	static EFF_FinalePurchaseResult EvaluatePurchase(
		const TArray<FFF_FinaleTier>& Tiers,
		EFF_FinaleGrade RequestedGrade,
		EFF_FinaleGrade PurchasedGrade,
		int32 CurrentGold,
		FFF_FinaleTier& OutTier);
};
