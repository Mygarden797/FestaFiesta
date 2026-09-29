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
	TestTrue(TEXT("Explicit free service displays zero rather than unavailable"),
		ViewData.FacilityRevenueText.ToString().Contains(TEXT("4명 0 · 5명 0 · 6명 0 Gold")));
	Preview.bRevenueAvailable = false;
	FFFacilityInfoModel::ApplyUpgradePreview(&Preview, ViewData);
	TestEqual(TEXT("Unavailable pricing replaces stale team quotes"), ViewData.FacilityRevenueText.ToString(),
		FString(TEXT("팀당 예상 수익  정보 없음")));
	FFFacilityInfoModel::ApplyUpgradePreview(nullptr, ViewData);
	TestEqual(TEXT("Missing preview cannot retain a previous facility price"), ViewData.FacilityRevenueText.ToString(),
		FString(TEXT("팀당 예상 수익  정보 없음")));
	return true;
}
#endif
