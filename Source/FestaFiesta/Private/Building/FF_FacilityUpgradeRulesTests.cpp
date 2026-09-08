#include "Building/FF_FacilityUpgradeTypes.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "FF_FFZSTypes.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFacilityUpgradePricingTest,
	"FestaFiesta.Building.FacilityUpgrade.PricingAndAuthoring",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFacilityUpgradePricingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	int32 Cost = -1;
	TestTrue(TEXT("400 Gold installation supports the 30% step"), FFFacilityUpgradeRules::ResolveCost(400, .3f, Cost));
	TestEqual(TEXT("Float authoring does not charge an extra Gold"), Cost, 120);
	TestTrue(TEXT("Second purchase costs 60%"), FFFacilityUpgradeRules::ResolveCost(400, .6f, Cost));
	TestEqual(TEXT("Second step costs 240, not 60% of first step"), Cost, 240);
	TestFalse(TEXT("Overflow cannot become free or negative"), FFFacilityUpgradeRules::ResolveCost(MAX_int32, 10.f, Cost));
	TestEqual(TEXT("Failed pricing clears output"), Cost, 0);
	TestFalse(TEXT("Negative price is rejected"), FFFacilityUpgradeRules::ResolveCost(400, -1.f, Cost));
	TestTrue(TEXT("A free installation has free multiplier-based upgrades"), FFFacilityUpgradeRules::ResolveCost(0, .3f, Cost));
	TestEqual(TEXT("A free installation quote is zero Gold"), Cost, 0);
	TestTrue(TEXT("Zero authored multiplier is a free upgrade"), FFFacilityUpgradeRules::ResolveCost(400, 0.f, Cost));
	TestEqual(TEXT("Zero multiplier does not charge minimum Gold"), Cost, 0);
	TestTrue(TEXT("A tiny price may round to zero"), FFFacilityUpgradeRules::ResolveCost(1, .01f, Cost));
	TestEqual(TEXT("Rounding respects zero Gold"), Cost, 0);
	TestTrue(TEXT("A multiplier beyond the former cap is supported"), FFFacilityUpgradeRules::ResolveCost(400, 25.f, Cost));
	TestEqual(TEXT("An unrestricted multiplier uses the authored price"), Cost, 10000);
	TestTrue(TEXT("The largest representable Gold amount remains valid"), FFFacilityUpgradeRules::ResolveCost(MAX_int32, 1.f, Cost));
	TestEqual(TEXT("Representable pricing does not truncate"), Cost, MAX_int32);
	TestFalse(TEXT("Non-finite pricing remains invalid"), FFFacilityUpgradeRules::ResolveCost(400, std::numeric_limits<float>::infinity(), Cost));

	FText Error;
	FFFZSFacilityUpgradeTier Level2;
	FFFZSFacilityUpgradeTier Level3;
	Level3.TargetLevel = 3;
	Level3.CostMultiplier = .6f;
	Level3.ServiceDurationMultiplier = .65f;
	const TArray<FFFZSFacilityUpgradeTier> AuthoredTiers = {Level2, Level3};
	TArray<FFFZSFacilityUpgradeTier> Tiers = AuthoredTiers;
	TestTrue(TEXT("Explicit per-facility tiers are valid"), FFZSFacilityUpgradeRules::ValidateTiers(Tiers, Error));
	TestNull(TEXT("An absent negative target has no exact tier"), FFZSFacilityUpgradeRules::FindTier(Tiers, MIN_int32));
	Tiers[1].TargetLevel = 4;
	TestTrue(TEXT("Skipped levels are allowed"), FFZSFacilityUpgradeRules::ValidateTiers(Tiers, Error));
	Tiers = AuthoredTiers;
	Tiers[1].ServiceDurationMultiplier = Tiers[0].ServiceDurationMultiplier;
	TestTrue(TEXT("Equal effects can be an authored upgrade"), FFZSFacilityUpgradeRules::ValidateTiers(Tiers, Error));
	Tiers = AuthoredTiers;
	Tiers[0].ServiceDurationMultiplier = 0.f;
	TestFalse(TEXT("Zero duration is rejected"), FFZSFacilityUpgradeRules::ValidateTiers(Tiers, Error));
	Tiers = AuthoredTiers;
	Tiers[0].SizeMultiplier = 0.1f;
	TestTrue(TEXT("Positive shrink multipliers are allowed structurally"), FFZSFacilityUpgradeRules::ValidateTiers(Tiers, Error));
	Tiers = AuthoredTiers;
	Tiers[0].ServiceDurationMultiplier = 1.f;
	Tiers[1].ServiceDurationMultiplier = 1.f;
	Tiers[1].RevenueMultiplier = 2.25f;
	TestTrue(TEXT("Revenue-only tiers can keep their service duration"), FFZSFacilityUpgradeRules::ValidateTiers(Tiers, Error));
	Tiers[1].RevenueMultiplier = 1.f;
	TestTrue(TEXT("An authored upgrade can reduce revenue"), FFZSFacilityUpgradeRules::ValidateTiers(Tiers, Error));
	Tiers = AuthoredTiers;
	Tiers[0].SizeMultiplier = 1.5f;
	TestTrue(TEXT("A later tier can shrink the facility"), FFZSFacilityUpgradeRules::ValidateTiers(Tiers, Error));
	Tiers[0].TargetLevel = 50;
	Tiers[1].TargetLevel = 12;
	Tiers[0].CostMultiplier = 0.f;
	Tiers[0].RevenueMultiplier = 0.f;
	Tiers[0].ServiceDurationMultiplier = 3.f;
	Tiers[0].SizeMultiplier = 5.f;
	TestTrue(TEXT("Unsorted, sparse levels above ten and unrestricted finite values are allowed"),
		FFZSFacilityUpgradeRules::ValidateTiers(Tiers, Error));
	const FFFZSFacilityUpgradeTier* NextTier = FFZSFacilityUpgradeRules::FindNextTier(Tiers, 1);
	if (!TestNotNull(TEXT("Unsorted tiers have a next level"), NextTier)) return false;
	TestEqual(TEXT("Next tier is the lowest authored level above current"), NextTier->TargetLevel, 12);
	TestEqual(TEXT("Max level is independent of array order"), FFZSFacilityUpgradeRules::GetMaxLevel(Tiers), 50);
	const FFFZSFacilityUpgradeTier* ExactTier = FFZSFacilityUpgradeRules::FindTier(Tiers, 50);
	if (!TestNotNull(TEXT("The high authored level has an exact match"), ExactTier)) return false;
	TestEqual(TEXT("Exact lookup does not assume array index"), ExactTier->TargetLevel, 50);
	TestNull(TEXT("Maximum level has no next tier"), FFZSFacilityUpgradeRules::FindNextTier(Tiers, 50));
	Tiers[1].TargetLevel = 50;
	TestFalse(TEXT("Duplicate target levels remain ambiguous and invalid"), FFZSFacilityUpgradeRules::ValidateTiers(Tiers, Error));
	Tiers.Reset();
	for (int32 Index = 0; Index < 20; ++Index)
	{
		FFFZSFacilityUpgradeTier Tier;
		Tier.TargetLevel = 100 - Index;
		Tiers.Add(Tier);
	}
	TestTrue(TEXT("There is no former nine-tier authoring cap"), FFZSFacilityUpgradeRules::ValidateTiers(Tiers, Error));
	return true;
}
#endif
