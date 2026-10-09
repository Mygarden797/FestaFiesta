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

// 마지막 날: "내일" 이야기를 하지 않고 축제 전체를 돌아본다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DailyReportComment_FinalDayLooksBack,
	"FestaFiesta.UI.DailyReportComment.FinalDayLooksBack",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DailyReportComment_FinalDayLooksBack::RunTest(const FString& Parameters)
{
	FFF_DailyReportCommentInputs Inputs;
	Inputs.bFinalDay = true;
	Inputs.DaysPlayed = 3;
	Inputs.TopFacilityName = FText::FromString(TEXT("오늘 1위"));
	Inputs.TopFacilityGold = 300;
	Inputs.FestivalTopFacilityName = FText::FromString(TEXT("피자 부스"));
	Inputs.FestivalTopFacilityGold = 1500;
	Inputs.FirstDaySatisfaction = 52;
	Inputs.FinalDaySatisfaction = 61;
	Inputs.GoalDaysMet = 2;
	Inputs.bHasSatisfaction = true;
	Inputs.WaitScore = 50;
	Inputs.DensityScore = 80;
	Inputs.PreferenceScore = 80;
	Inputs.NeedScore = 80;
	Inputs.AdmittedVisitors = 1200;
	Inputs.DailyVisitorGoal = 1100;

	const FString Text = FFF_DailyReportCommentRules::Build(Inputs).ToString();
	TestFalse(TEXT("마지막 날은 내일을 말하지 않는다"), Text.Contains(TEXT("내일")));
	TestTrue(TEXT("축제 전체 매출 1위를 말한다"), Text.Contains(TEXT("피자 부스"))
		&& (Text.Contains(TEXT("1,500")) || Text.Contains(TEXT("1500"))));
	TestFalse(TEXT("오늘 1위는 말하지 않는다"), Text.Contains(TEXT("오늘 1위")));
	TestTrue(TEXT("만족도 흐름"), Text.Contains(TEXT("52")) && Text.Contains(TEXT("61")) && Text.Contains(TEXT("올랐어요")));
	TestTrue(TEXT("목표 달성 일수"), Text.Contains(TEXT("3일 중 2일")));
	TestTrue(TEXT("가장 낮은 항목은 다음 축제 조언으로"), Text.Contains(TEXT("다음 축제")) && Text.Contains(TEXT("줄")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
