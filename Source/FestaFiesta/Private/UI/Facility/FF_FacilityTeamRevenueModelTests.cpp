#include "UI/Facility/FF_FacilityInfoTypes.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Building/FF_FacilityUpgradeTypes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityTeamRevenueModelTest,
	"FestaFiesta.UI.FacilityInfo.TeamRevenueFormatting",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityTeamRevenueModelTest::RunTest(const FString& Parameters)
{
	FFF_FacilityUpgradePreview Preview;
	Preview.bRevenueAvailable = true;
	Preview.CurrentRevenuePerUseGold = 4;
	// 30 Gold per person, 12% commission: round only after multiplying the whole team.
	Preview.CurrentRevenueByTeamSizeGold = {4, 7, 11, 14, 18, 22};
	FFF_FacilityInfoViewData ViewData;
	for (const EFF_FacilityInfoContext Context : {EFF_FacilityInfoContext::Build, EFF_FacilityInfoContext::Live})
	{
		ViewData.Context = Context;
		FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
		const FString Text = ViewData.FacilityRevenueText.ToString();
		TestTrue(TEXT("Both inspection contexts identify team revenue for one ordinary visit"),
			Text.Contains(TEXT("팀당 예상 수익")) && Text.Contains(TEXT("일반 이용 1회")));
		TestTrue(TEXT("Explicit team sizes show authoritative rounded amounts"),
			Text.Contains(TEXT("1명 4 · 2명 7 · 3명 11 Gold")) &&
			Text.Contains(TEXT("4명 14 · 5명 18 · 6명 22 Gold")));
		TestFalse(TEXT("Team estimates remain available without upgrade levels"), ViewData.bHasFacilityLevels);
	}

	Preview.CurrentRevenuePerUseGold = 0;
	Preview.CurrentRevenueByTeamSizeGold = {0, 0, 0, 1, 1, 1};
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestTrue(TEXT("A rounded zero single-person quote never erases larger team revenue"),
		ViewData.FacilityRevenueText.ToString().Contains(TEXT("4명 1 · 5명 1 · 6명 1 Gold")));

	Preview.CurrentRevenueByTeamSizeGold = {0, 0, 0, 0, 0, 0};
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestTrue(TEXT("An explicitly free service clears the previous team estimate"), ViewData.FacilityRevenueText.IsEmpty());

	ViewData.Context = EFF_FacilityInfoContext::Build;
	Preview.bHasLevels = Preview.bSupported = Preview.bHasUpgradeQuote = true;
	Preview.TargetLevel = 2;
	Preview.CostGold = 50;
	Preview.GoldAfterUpgrade = 100;
	Preview.CurrentServiceDurationSeconds = 10.f;
	Preview.NextServiceDurationSeconds = 8.f;
	Preview.CurrentThroughputPerMinute = 6.f;
	Preview.NextThroughputPerMinute = 7.5f;
	Preview.CurrentFootprintCentimeters = FIntPoint(200, 200);
	Preview.NextFootprintCentimeters = FIntPoint(300, 300);
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestTrue(TEXT("Free facilities retain their valid upgrade quote"), ViewData.bShowUpgradeDetails);
	TestTrue(TEXT("Free upgrades hide both revenue columns and the revenue effect"),
		ViewData.UpgradeCurrentValues.Revenue.IsEmpty() && ViewData.UpgradeNextValues.Revenue.IsEmpty() &&
		ViewData.UpgradeRevenueEffectText.IsEmpty());
	TestFalse(TEXT("Free upgrade comparison omits the per-person revenue caption"),
		ViewData.UpgradeCurrentText.ToString().Contains(TEXT("1인 수익")) ||
		ViewData.UpgradeNextText.ToString().Contains(TEXT("1인 수익")));
	TestTrue(TEXT("Hiding income preserves time, throughput, size and price"),
		!ViewData.UpgradeCurrentValues.ServiceTime.IsEmpty() && !ViewData.UpgradeNextValues.Throughput.IsEmpty() &&
		!ViewData.UpgradeCurrentValues.Footprint.IsEmpty() && !ViewData.UpgradePriceText.IsEmpty());

	Preview.NextRevenuePerUseGold = 5;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestTrue(TEXT("Future income does not invent a current ordinary-use estimate"), ViewData.FacilityRevenueText.IsEmpty());
	TestTrue(TEXT("An upgrade that introduces income keeps both comparison cells and its effect"),
		ViewData.UpgradeCurrentValues.Revenue.ToString().Contains(TEXT("0 Gold")) &&
		ViewData.UpgradeNextValues.Revenue.ToString().Contains(TEXT("5 Gold")) &&
		!ViewData.UpgradeRevenueEffectText.IsEmpty());

	Preview.NextRevenuePerUseGold = 0;
	Preview.CurrentRevenueByTeamSizeGold = {0, 0, 0, 1, 1, 1};
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestTrue(TEXT("Larger-team income retains upgrade revenue when both one-person quotes round to zero"),
		!ViewData.UpgradeCurrentValues.Revenue.IsEmpty() && !ViewData.UpgradeNextValues.Revenue.IsEmpty() &&
		!ViewData.UpgradeRevenueEffectText.IsEmpty());
	Preview.bRevenueAvailable = false;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestTrue(TEXT("Unavailable pricing clears stale team and upgrade income"),
		ViewData.FacilityRevenueText.IsEmpty() && ViewData.UpgradeCurrentValues.Revenue.IsEmpty() &&
		ViewData.UpgradeNextValues.Revenue.IsEmpty() && ViewData.UpgradeRevenueEffectText.IsEmpty());
	TestFalse(TEXT("Unavailable income does not leave a revenue caption in the legacy comparison"),
		ViewData.UpgradeCurrentText.ToString().Contains(TEXT("1인 수익")));
	FFFacilityInfoModel::ApplyUpgradePreview(nullptr, ViewData);
	TestTrue(TEXT("Missing preview cannot retain previous income or upgrade comparisons"),
		ViewData.FacilityRevenueText.IsEmpty() && ViewData.UpgradeCurrentText.IsEmpty() &&
		ViewData.UpgradeNextValues.Revenue.IsEmpty());
	return true;
}
#endif
