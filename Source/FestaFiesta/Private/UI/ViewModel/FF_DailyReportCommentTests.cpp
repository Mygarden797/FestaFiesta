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


// 판정은 FindComplaint 하나가 한다. 아침 비서 브리핑도 이 판정을 그대로 말한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DailyReportComment_FindComplaintIsTheOnlyJudge,
	"FestaFiesta.UI.DailyReportComment.FindComplaintIsTheOnlyJudge",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DailyReportComment_FindComplaintIsTheOnlyJudge::RunTest(const FString& Parameters)
{
	FFF_DailyReportCommentInputs Inputs;
	TestFalse(TEXT("만족도를 모르면 판정하지 않는다"), FFF_DailyReportCommentRules::FindComplaint(Inputs).bJudged);

	Inputs.bHasSatisfaction = true;
	Inputs.NeedScore = 62;
	Inputs.WaitScore = 62;
	Inputs.DensityScore = 80;
	Inputs.PreferenceScore = 80;
	FFF_DailyComplaint Complaint = FFF_DailyReportCommentRules::FindComplaint(Inputs);
	TestTrue(TEXT("판정했다"), Complaint.bJudged);
	TestEqual(TEXT("점수가 같으면 필요가 먼저"), static_cast<int32>(Complaint.Axis), static_cast<int32>(EFF_DailyComplaintAxis::Need));
	TestFalse(TEXT("커버리지가 모두 충분하면 모자란 필요가 없다"), Complaint.bHasNeedShortage);

	Inputs.WaitScore = 61;
	TestEqual(TEXT("더 낮은 항목을 고른다"), static_cast<int32>(FFF_DailyReportCommentRules::FindComplaint(Inputs).Axis), static_cast<int32>(EFF_DailyComplaintAxis::Wait));

	Inputs.HungerCoverage = 0.5f;
	Inputs.RestroomCoverage = 0.5f;
	Inputs.StimulationCoverage = 0.4f;
	Complaint = FFF_DailyReportCommentRules::FindComplaint(Inputs);
	TestTrue(TEXT("모자란 필요가 있다"), Complaint.bHasNeedShortage);
	TestEqual(TEXT("커버리지가 가장 낮은 필요"), static_cast<int32>(Complaint.NeedShortage), static_cast<int32>(EFF_NeedType::Stimulation));
	Inputs.StimulationCoverage = 0.5f;
	TestEqual(TEXT("커버리지가 같으면 화장실이 먼저"), static_cast<int32>(FFF_DailyReportCommentRules::FindComplaint(Inputs).NeedShortage), static_cast<int32>(EFF_NeedType::Restroom));

	Inputs.NeedScore = Inputs.WaitScore = FFF_DailyReportCommentRules::ComplaintScoreBelow;
	Complaint = FFF_DailyReportCommentRules::FindComplaint(Inputs);
	TestTrue(TEXT("모두 기준 이상이어도 판정은 했다"), Complaint.bJudged);
	TestEqual(TEXT("모두 기준 이상이면 불만 없음"), static_cast<int32>(Complaint.Axis), static_cast<int32>(EFF_DailyComplaintAxis::None));
	return true;
}

// 카드가 조언을 앞세워도 원래 매출·목표 정보를 버리거나 중복하지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DailyReportComment_AdvicePresentationPreservesSource,
	"FestaFiesta.UI.DailyReportComment.AdvicePresentationPreservesSource",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DailyReportComment_AdvicePresentationPreservesSource::RunTest(const FString& Parameters)
{
	FFF_DailyReportCommentInputs Inputs;
	Inputs.TopFacilityName = FText::FromString(TEXT("테스트 음악 부스"));
	Inputs.TopFacilityGold = 731;
	Inputs.bHasSatisfaction = true;
	Inputs.NeedScore = 84; Inputs.WaitScore = 41; Inputs.DensityScore = 89; Inputs.PreferenceScore = 77;
	Inputs.AdmittedVisitors = 812; Inputs.DailyVisitorGoal = 900;
	const auto Presentation = FFF_DailyReportCommentRules::BuildPresentation(Inputs);
	TArray<FString> LegacyLines;
	Presentation.FullComment.ToString().ParseIntoArrayLines(LegacyLines);
	if (!TestEqual(TEXT("기존 세 문장 유지"), LegacyLines.Num(), 3)) return false;
	TestEqual(TEXT("불만 원본 문장이 핵심 조언"), Presentation.Advice.ToString(), LegacyLines[1]);
	TestEqual(TEXT("매출·목표는 순서대로 보조 정보"), Presentation.Details.ToString(), LegacyLines[0] + TEXT("\n") + LegacyLines[2]);
	TestTrue(TEXT("실제 매출 원본 유지"), Presentation.Details.ToString().Contains(TEXT("731")));
	TestTrue(TEXT("목표 차이 원본 유지"), Presentation.Details.ToString().Contains(TEXT("88")));
	TestFalse(TEXT("조언을 보조 정보에 반복하지 않음"), Presentation.Details.ToString().Contains(Presentation.Advice.ToString()));
	TestEqual(TEXT("기존 호출자는 원래 전체 문장"), FFF_DailyReportCommentRules::Build(Inputs).ToString(), Presentation.FullComment.ToString());

	Inputs.bHasSatisfaction = false;
	const auto Unjudged = FFF_DailyReportCommentRules::BuildPresentation(Inputs);
	TestEqual(TEXT("판정 없는 날은 실제 매출 요약을 앞세움"), Unjudged.Advice.ToString(), LegacyLines[0]);
	TestEqual(TEXT("판정 없는 날도 목표 정보 유지"), Unjudged.Details.ToString(), LegacyLines[2]);
	TestFalse(TEXT("판정 없는 날에 대기 불만을 만들지 않음"), Unjudged.FullComment.ToString().Contains(TEXT("줄이 길다는")));
	const auto Empty = FFF_DailyReportCommentRules::BuildPresentation(FFF_DailyReportCommentInputs());
	TestTrue(TEXT("원본이 없으면 빈 카드"), Empty.Advice.IsEmpty() && Empty.Details.IsEmpty() && Empty.FullComment.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DailyReportComment_FinalAdvicePresentation,
	"FestaFiesta.UI.DailyReportComment.FinalAdvicePresentation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DailyReportComment_FinalAdvicePresentation::RunTest(const FString& Parameters)
{
	FFF_DailyReportCommentInputs Inputs;
	Inputs.bFinalDay = true; Inputs.DaysPlayed = 3; Inputs.GoalDaysMet = 2;
	Inputs.FestivalTopFacilityName = FText::FromString(TEXT("축제 전체 시설")); Inputs.FestivalTopFacilityGold = 1731;
	Inputs.FirstDaySatisfaction = 47; Inputs.FinalDaySatisfaction = 68;
	Inputs.bHasSatisfaction = true; Inputs.NeedScore = 83; Inputs.WaitScore = 91;
	Inputs.PreferenceScore = 40; Inputs.DensityScore = 90;
	const auto Presentation = FFF_DailyReportCommentRules::BuildPresentation(Inputs);
	TestTrue(TEXT("마지막 날의 실제 취향 조언"), Presentation.Advice.ToString().Contains(TEXT("여러 종류")));
	TestFalse(TEXT("마지막 날에는 내일 조언 없음"), Presentation.Advice.ToString().Contains(TEXT("내일")));
	TArray<FString> FullLines, DetailLines;
	Presentation.FullComment.ToString().ParseIntoArrayLines(FullLines);
	Presentation.Details.ToString().ParseIntoArrayLines(DetailLines);
	TestEqual(TEXT("전체 4문장을 조언1 + 보조3으로 보존"), FullLines.Num(), 4);
	TestEqual(TEXT("매출·만족도흐름·목표일수 보존"), DetailLines.Num(), 3);
	TestEqual(TEXT("조언이 원래 마지막 문장과 동일"), Presentation.Advice.ToString(), FullLines.Last());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
