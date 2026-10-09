#if WITH_DEV_AUTOMATION_TESTS

#include "UI/ViewModel/FF_DailyReportComment.h"

#include "Misc/AutomationTest.h"

// 비서 한마디: 매출 1위, 가장 낮은 항목의 조언(시설 부족이면 그 시설), 방문객 목표를 한 줄씩
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DailyReportComment_PicksWorstAxisAndShortFacility,
	"FestaFiesta.UI.DailyReportComment.PicksWorstAxisAndShortFacility",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DailyReportComment_PicksWorstAxisAndShortFacility::RunTest(const FString& Parameters)
{
	FFF_DailyReportCommentInputs Inputs;
	Inputs.TopFacilityName = FText::FromString(TEXT("피자 부스"));
	Inputs.TopFacilityGold = 420;
	Inputs.bHasSatisfaction = true;
	Inputs.WaitScore = 80;
	Inputs.DensityScore = 75;
	Inputs.PreferenceScore = 60;
	Inputs.NeedScore = 40;
	Inputs.RestroomCoverage = 0.3f;
	Inputs.HungerCoverage = 0.67f;
	Inputs.AdmittedVisitors = 850;
	Inputs.DailyVisitorGoal = 900;

	const FString Text = FFF_DailyReportCommentRules::Build(Inputs).ToString();
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	TestEqual(TEXT("세 줄"), Lines.Num(), 3);
	TestTrue(TEXT("매출 1위 시설 이름과 금액"), Text.Contains(TEXT("피자 부스")) && Text.Contains(TEXT("420")));
	TestTrue(TEXT("가장 낮은 Need 항목에서 가장 모자란 화장실을 짚는다"), Text.Contains(TEXT("화장실")));
	TestTrue(TEXT("모자란 인원을 말한다"), Text.Contains(TEXT("50")));

	Inputs.NeedScore = 90;
	Inputs.PreferenceScore = 90;
	Inputs.AdmittedVisitors = 950;
	const FString Happy = FFF_DailyReportCommentRules::Build(Inputs).ToString();
	TestTrue(TEXT("모든 항목이 기준 이상이면 칭찬한다"), Happy.Contains(TEXT("만족")));
	TestTrue(TEXT("목표를 넘기면 그렇게 말한다"), Happy.Contains(TEXT("넘겼어요")));

	FFF_DailyReportCommentInputs Empty;
	TestTrue(TEXT("아무 값도 없으면 빈 문구"), FFF_DailyReportCommentRules::Build(Empty).IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
