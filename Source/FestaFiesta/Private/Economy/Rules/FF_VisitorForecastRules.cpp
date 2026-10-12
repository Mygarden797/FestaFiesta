#include "Economy/Rules/FF_VisitorForecastRules.h"

#include "Economy/Rules/FF_PromotionRules.h"
#include "Festival/FF_FestivalDefinition.h"

float FFF_VisitorForecastRules::CalcAdmissionFeeMultiplier(
	const int32 FeeGold, const int32 ReferenceFeeGold, const float Slope,
	const float MinMultiplier, const float MaxMultiplier)
{
	const float Low = FMath::Max(0.f, FMath::Min(MinMultiplier, MaxMultiplier));
	const float High = FMath::Max(MinMultiplier, MaxMultiplier);
	const float Raw = 1.f - FMath::Max(Slope, 0.f) * static_cast<float>(FeeGold - ReferenceFeeGold);
	return FMath::Clamp(Raw, Low, High);
}

float FFF_VisitorForecastRules::CalcBuzzMultiplier(const float Buzz, const float Swing)
{
	const float Offset = (FFF_BuzzRules::Clamp(Buzz) - FFF_BuzzRules::NeutralBuzz) / FFF_BuzzRules::NeutralBuzz;
	return 1.f + Offset * FMath::Clamp(Swing, 0.f, 1.f);
}

float FFF_VisitorForecastRules::CalcReputationMultiplier(
	const float YesterdaySatisfaction, const float MinMultiplier, const float MaxMultiplier)
{
	const float Satisfaction = FMath::Clamp(YesterdaySatisfaction, 0.f, 100.f);
	const float Low = FMath::Min(MinMultiplier, 1.f);
	const float High = FMath::Max(MaxMultiplier, 1.f);
	return Satisfaction <= 50.f
		? FMath::Lerp(Low, 1.f, Satisfaction / 50.f)
		: FMath::Lerp(1.f, High, (Satisfaction - 50.f) / 50.f);
}

float FFF_VisitorForecastRules::CalcPromotionMultiplier(
	const EFF_PromotionGrade Grade, const UFF_FestivalDefinition& Definition)
{
	const FFF_PromotionTier* Tier = FFF_PromotionRules::FindTier(
		Grade, Definition.PromotionBasic, Definition.PromotionFocused, Definition.PromotionInfluencer);
	return Tier != nullptr ? Tier->VisitorMultiplier : Definition.NoPromotionVisitorMultiplier;
}

FFF_VisitorForecast FFF_VisitorForecastRules::BuildForecast(
	const UFF_FestivalDefinition& Definition,
	const EFF_PromotionGrade Grade,
	const int32 AdmissionFeeGold,
	const float Buzz,
	const float ReputationMultiplier)
{
	FFF_VisitorForecast Forecast;
	Forecast.AdmissionFeeGold = AdmissionFeeGold;
	Forecast.PromotionMultiplier = CalcPromotionMultiplier(Grade, Definition);
	Forecast.AdmissionFeeMultiplier = CalcAdmissionFeeMultiplier(
		AdmissionFeeGold, Definition.AdmissionFeeGold, Definition.AdmissionFeeDemandSlope,
		Definition.AdmissionFeeMinVisitorMultiplier, Definition.AdmissionFeeMaxVisitorMultiplier);
	Forecast.BuzzMultiplier = CalcBuzzMultiplier(Buzz, Definition.BuzzVisitorSwing);

	const float Base = static_cast<float>(FMath::Max(Definition.BaseDailyVisitors, 0));
	Forecast.ReputationMultiplier = FMath::Max(ReputationMultiplier, 0.f);
	const float WithoutFee = Base * Forecast.PromotionMultiplier * Forecast.BuzzMultiplier
		* Forecast.ReputationMultiplier;

	// 1e-4 를 더하고 내린다. 500 × 1.12 가 부동소수로 559.9999 가 되어 559 로 떨어지는 것을 막는다.
	Forecast.ExpectedVisitors = FMath::Max(0, FMath::FloorToInt32(WithoutFee * Forecast.AdmissionFeeMultiplier + 1e-4f));
	Forecast.VisitorsAtReferenceFee = FMath::Max(0, FMath::FloorToInt32(WithoutFee + 1e-4f));
	Forecast.VisitorDeltaFromFee = Forecast.ExpectedVisitors - Forecast.VisitorsAtReferenceFee;
	Forecast.ExpectedAdmissionRevenue = static_cast<int32>(FMath::Min<int64>(MAX_int32,
		static_cast<int64>(Forecast.ExpectedVisitors) * FMath::Max(AdmissionFeeGold, 0)));
	return Forecast;
}

float FFF_BuzzRules::ApplyDailyDecay(const float Buzz, const float DecayRatio)
{
	const float Clamped = Clamp(Buzz);
	return Clamped + (NeutralBuzz - Clamped) * FMath::Clamp(DecayRatio, 0.f, 1.f);
}

float FFF_BuzzRules::CalcSnsPostDelta(const bool bPositive, const float Salience0To100, const float PerPost)
{
	const float Magnitude = FMath::Max(PerPost, 0.f) * FMath::Clamp(Salience0To100, 0.f, 100.f) / 100.f;
	return bPositive ? Magnitude : -Magnitude;
}
