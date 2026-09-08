#include "Building/FF_FacilityUpgradeTypes.h"

#define LOCTEXT_NAMESPACE "FFFacilityUpgradeRules"

bool FFFacilityUpgradeRules::ResolveCost(const int32 InstallCost, const float CostMultiplier, int32& OutCost)
{
	OutCost = 0;
	if (InstallCost < 0 || !FMath::IsFinite(CostMultiplier) || CostMultiplier < 0.f)
	{
		return false;
	}
	// Float authoring such as 0.3 must give 120 Gold for a 400 Gold installation, not ceil to 121.
	const double RawCost = static_cast<double>(InstallCost) * CostMultiplier;
	const double RoundedCost = FMath::FloorToDouble(RawCost + 0.5);
	if (!FMath::IsFinite(RoundedCost) || RoundedCost > static_cast<double>(MAX_int32))
	{
		return false;
	}
	OutCost = static_cast<int32>(RoundedCost);
	return true;
}

#undef LOCTEXT_NAMESPACE
