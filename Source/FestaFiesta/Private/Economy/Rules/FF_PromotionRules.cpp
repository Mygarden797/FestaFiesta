#include "Economy/Rules/FF_PromotionRules.h"

const FFF_PromotionTier* FFF_PromotionRules::FindTier(
	const EFF_PromotionGrade Grade,
	const FFF_PromotionTier& Basic,
	const FFF_PromotionTier& Focused,
	const FFF_PromotionTier& Influencer)
{
	switch (Grade)
	{
	case EFF_PromotionGrade::Basic:      return &Basic;
	case EFF_PromotionGrade::Focused:    return &Focused;
	case EFF_PromotionGrade::Influencer: return &Influencer;
	default:                          return nullptr;
	}
}

EFF_PromotionPurchaseResult FFF_PromotionRules::EvaluatePurchase(
	const EFF_PromotionGrade RequestedGrade,
	const EFF_PromotionGrade PurchasedGrade,
	const int32 CurrentGold,
	const FFF_PromotionTier& Basic,
	const FFF_PromotionTier& Focused,
	const FFF_PromotionTier& Influencer,
	FFF_PromotionTier& OutTier)
{
	// 이미 산 뒤에는 등급이 무엇이든 거절한다. 같은 등급 재구매도 마찬가지다 —
	// 통과시키면 배수는 그대로인데 Gold 만 한 번 더 빠진다.
	if (PurchasedGrade != EFF_PromotionGrade::None)
	{
		return EFF_PromotionPurchaseResult::AlreadyPurchased;
	}

	// None 을 사는 것은 "안 함"이라 구매 행위가 아니다. 0 Gold 로 통과시키면
	// 그 뒤로 진짜 홍보를 못 사게 된다(위 가드에 걸린다).
	const FFF_PromotionTier* Tier = FindTier(RequestedGrade, Basic, Focused, Influencer);
	if (Tier == nullptr)
	{
		return EFF_PromotionPurchaseResult::InvalidGrade;
	}

	if (CurrentGold < Tier->CostGold)
	{
		return EFF_PromotionPurchaseResult::InsufficientGold;
	}

	OutTier = *Tier;
	return EFF_PromotionPurchaseResult::Success;
}

int32 FFF_PromotionRules::CalcExpectedVisitors(
	const int32 BaseDailyVisitors, const float VisitorMultiplier)
{
	if (BaseDailyVisitors <= 0)
	{
		return 0;
	}

	const float SafeMultiplier = FMath::Max(VisitorMultiplier, 1.f);
	return FMath::FloorToInt32(BaseDailyVisitors * SafeMultiplier);
}
