#if WITH_DEV_AUTOMATION_TESTS

#include "Secretary/FF_SecretaryAdvisor.h"

#include "Misc/AutomationTest.h"

namespace
{
	FString JoinBriefing(const FFF_SecretaryBriefingInputs& Inputs)
	{
		return FText::Join(FText::FromString(TEXT("\n")), FFSecretaryAdvisor::BuildMorningBriefing(Inputs)).ToString();
	}
}

// 아침 브리핑은 어제 결산이 고른 불만 1위 · 모자란 필요 · 매출 1등을 그대로 말한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SecretaryAdvisor_BriefingRepeatsReportVerdict,
	"FestaFiesta.Secretary.Advisor.BriefingRepeatsReportVerdict",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SecretaryAdvisor_BriefingRepeatsReportVerdict::RunTest(const FString& Parameters)
{
	FFF_SecretaryBriefingInputs Inputs;
	Inputs.Day = 2;
	Inputs.YesterdaySatisfaction = 64;
	Inputs.bJudgedYesterday = true;
	Inputs.Weakness = EFF_SecretaryWeakness::Need;
	Inputs.bHasWorstNeed = true;
	Inputs.WorstNeed = EFF_NeedType::Restroom;
	Inputs.TopRevenueFacility = FText::FromString(TEXT("버거 트럭"));
	Inputs.TopRevenueGold = 500;
	Inputs.ToiletCount = 0;

	const FString Text = JoinBriefing(Inputs);
	TestTrue(TEXT("어제 만족도"), Text.Contains(TEXT("64")));
	TestTrue(TEXT("결산이 고른 매출 1등 시설과 금액"), Text.Contains(TEXT("버거 트럭")) && Text.Contains(TEXT("500")));
	TestTrue(TEXT("결산이 짚은 필요를 말한다"), Text.Contains(TEXT("화장실")));
	TestFalse(TEXT("화장실 얘기를 했으면 화장실 권유를 되풀이하지 않는다"), Text.Contains(TEXT("꼭 설치")));

	Inputs.Weakness = EFF_SecretaryWeakness::None;
	TestTrue(TEXT("판정했는데 불만이 없으면 그렇게 말한다"), JoinBriefing(Inputs).Contains(TEXT("크게 아쉬운 점이 없었어요")));

	Inputs.bJudgedYesterday = false;
	Inputs.Weakness = EFF_SecretaryWeakness::Wait;
	const FString Unjudged = JoinBriefing(Inputs);
	TestFalse(TEXT("어제 결산을 못 봤으면 아쉬운 점 얘기를 하지 않는다"),
		Unjudged.Contains(TEXT("아쉬웠어요")) || Unjudged.Contains(TEXT("아쉬운 점")));
	return true;
}

// 못 채운 손님이 많아도 시설이 목표 개수만큼 있으면 "늘려 보세요"라고 하지 않는다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SecretaryAdvisor_UnmetNeedAsksMoreOnlyWhenShort,
	"FestaFiesta.Secretary.Advisor.UnmetNeedAsksMoreOnlyWhenShort",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SecretaryAdvisor_UnmetNeedAsksMoreOnlyWhenShort::RunTest(const FString& Parameters)
{
	const auto NeverResting = [](EFF_SecretaryTipKind) { return false; };
	const int32 Restroom = static_cast<int32>(EFF_NeedType::Restroom);
	const int32 Hunger = static_cast<int32>(EFF_NeedType::Hunger);

	FFF_SecretaryLiveSignals Signals;
	Signals.bNeedSampled = true;
	Signals.ToiletCount = 3;
	Signals.NeedUnmetPercent[Restroom] = 80.f;
	Signals.NeedCoverage[Restroom] = 0.5f;
	FFF_SecretaryTip Tip = FFSecretaryAdvisor::ChooseLiveTip(Signals, FFF_SecretaryThresholds(), NeverResting);
	TestTrue(TEXT("화장실이 목표보다 적으면 늘리라고 한다"), Tip.Text.ToString().Contains(TEXT("늘려")));

	Signals.NeedCoverage[Restroom] = 1.f;
	Tip = FFSecretaryAdvisor::ChooseLiveTip(Signals, FFF_SecretaryThresholds(), NeverResting);
	TestFalse(TEXT("목표만큼 있으면 늘리라고 하지 않는다"), Tip.Text.ToString().Contains(TEXT("늘려")));
	TestTrue(TEXT("대신 공연 뒤 몰림을 말한다"), Tip.Text.ToString().Contains(TEXT("몰려요")));

	Signals.NeedUnmetPercent[Hunger] = 50.f;
	Signals.NeedCoverage[Hunger] = 0.3f;
	Tip = FFSecretaryAdvisor::ChooseLiveTip(Signals, FFF_SecretaryThresholds(), NeverResting);
	TestTrue(TEXT("모자란 다른 필요가 있으면 그쪽을 늘리라고 한다"), Tip.Text.ToString().Contains(TEXT("먹을 곳")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
