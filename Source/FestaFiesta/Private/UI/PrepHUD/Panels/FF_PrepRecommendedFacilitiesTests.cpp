#if WITH_DEV_AUTOMATION_TESTS
#include "UI/PrepHUD/Panels/FF_PrepRecommendedFacilities.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepRecommendedFacilitiesTest, "FestaFiesta.UI.PrepCheck.RecommendedFacilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_PrepRecommendedFacilitiesTest::RunTest(const FString& Parameters)
{
	FFFZSReadModel Model;
	FFF_PrepRecommendedFacilityRules Rules;
	using EKind = EFF_PrepRecommendedFacilityKind;
	TestFalse(TEXT("원본 미연결은 8종 미설치로 표시하지 않음"), FFPrepRecommendedFacilities::Evaluate(Model, Rules, false).bAvailable);
	TestEqual(TEXT("연결된 빈 축제는 8종 안내"), FFPrepRecommendedFacilities::Evaluate(Model, Rules, true).MissingKinds.Num(), 8);
	auto& Cafe = Model.Facilities.AddDefaulted_GetRef();
	Cafe.Category = EFFZSFacilityCategory::Booth;
	Cafe.FacilityDefinitionId = TEXT("Booth_FoodStall_CafeTruck");
	Cafe.State = EFFZSFacilityState::Closed;
	auto Result = FFPrepRecommendedFacilities::Evaluate(Model, Rules, true);
	TestFalse(TEXT("개장 전 닫힌 카페도 음식 제공 시설"), Result.MissingKinds.Contains(EKind::Food));
	TestFalse(TEXT("카페 한 곳으로 음료 안내도 해소"), Result.MissingKinds.Contains(EKind::Drink));
	Cafe.bNeedsRelocation = true;
	TestEqual(TEXT("재배치 필요 시설은 유효 설치 아님"), FFPrepRecommendedFacilities::Evaluate(Model, Rules, true).MissingKinds.Num(), 8);
	Cafe.bNeedsRelocation = false; Cafe.State = EFFZSFacilityState::Removed;
	TestEqual(TEXT("철거 후 안내가 다시 나타남"), FFPrepRecommendedFacilities::Evaluate(Model, Rules, true).MissingKinds.Num(), 8);
	TestFalse(TEXT("메인무대는 추가 즐길거리에서 제외"), Rules.Matches(EKind::Entertainment, EFFZSFacilityCategory::Performance, TEXT("Performance_MainStage_SunsetMainStage")));
	TestTrue(TEXT("게임장은 즐길거리"), Rules.Matches(EKind::Entertainment, EFFZSFacilityCategory::Booth, TEXT("Booth_ActivityGame_Game")));
	TestFalse(TEXT("햄버거 가게를 음료 판매로 오판하지 않음"), Rules.Matches(EKind::Drink, EFFZSFacilityCategory::Booth, TEXT("Booth_FoodStall_Burger")));
	TestTrue(TEXT("음료 분류를 데이터로 확장"), [&]() { Rules.DrinkFacilityIds.Add(TEXT("Booth_FoodStall_Burger")); return Rules.Matches(EKind::Drink, EFFZSFacilityCategory::Booth, TEXT("Booth_FoodStall_Burger")); }());
	TestTrue(TEXT("응급 구분"), Rules.Matches(EKind::FirstAid, EFFZSFacilityCategory::Safety, TEXT("Safety_FirstAid_FirstAid")));
	TestFalse(TEXT("소방을 응급으로 중복 집계하지 않음"), Rules.Matches(EKind::FirstAid, EFFZSFacilityCategory::Safety, TEXT("Safety_FireSafety_FireSafety")));
	TestTrue(TEXT("소방 구분"), Rules.Matches(EKind::FireSafety, EFFZSFacilityCategory::Safety, TEXT("Safety_FireSafety_FireSafety")));
	TestTrue(TEXT("휴게 구분"), Rules.Matches(EKind::Rest, EFFZSFacilityCategory::Rest, TEXT("Rest_Seating_Seating")));
	TestTrue(TEXT("화장실 구분"), Rules.Matches(EKind::Toilet, EFFZSFacilityCategory::Toilet, TEXT("Toilet_Standard_Standard")));
	return true;
}
#endif
