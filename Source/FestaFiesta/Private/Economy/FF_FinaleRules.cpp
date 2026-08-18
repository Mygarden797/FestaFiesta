#include "../../Public/Economy/FF_FinaleRules.h"

const FFF_FinaleTier* FFF_FinaleRules::FindTier(const TArray<FFF_FinaleTier>& Tiers, EFF_FinaleGrade Grade)
{
	// 찾은 원소를 가리키는 포인터 반환
	return Tiers.FindByPredicate([Grade](const FFF_FinaleTier& Tier)
	{
		return Tier.Grade == Grade;
	});
}

EFF_FinalePurchaseResult FFF_FinaleRules::EvaluatePurchase(const TArray<FFF_FinaleTier>& Tiers,
	EFF_FinaleGrade RequestedGrade, EFF_FinaleGrade PurchasedGrade, int32 CurrentGold, FFF_FinaleTier& OutTier)
{
	// None 은 "안 샀다"는 상태를 나타내는 값이라 구매 대상이 아니다.
	if (RequestedGrade == EFF_FinaleGrade::None)
	{
		return EFF_FinalePurchaseResult::InvalidGrade;
	}

	// 이미 산 뒤에는 등급을 바꾸지 않는다. 되돌릴 수 있으면 Finale 가
	// "3일 벌어서 사는 목표"가 아니라 임시 저축이 된다.
	if (PurchasedGrade != EFF_FinaleGrade::None)
	{
		return EFF_FinalePurchaseResult::AlreadyPurchased;
	}

	// 저작에 없는 등급은 거부한다. 0 으로 폴백하면 공짜 피날레가 조용히 생긴다 (§3.4).
	const FFF_FinaleTier* Tier = FindTier(Tiers, RequestedGrade);
	if (Tier == nullptr)
	{
		return EFF_FinalePurchaseResult::UnknownGrade;
	}

	if (CurrentGold < Tier->CostGold)
	{
		return EFF_FinalePurchaseResult::InsufficientGold;
	}

	OutTier = *Tier;
	return EFF_FinalePurchaseResult::Success;
}
