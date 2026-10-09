#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "UI/LiveHUD/OperationAlert/FF_OperationAlertPanel.h"
#include "UI/LiveHUD/OperationAlert/FF_OperationAlertSubsystem.h"
#include "UI/LiveHUD/OperationAlert/FF_OperationAlertTypes.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

// 판정: 커버리지가 1 미만이면 부족, 퇴장·이탈은 문턱 이상일 때만. 퇴장이 시설 부족보다 앞선다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OperationAlert_RulesPickKinds,
	"FestaFiesta.UI.OperationAlert.RulesPickKinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_OperationAlert_RulesPickKinds::RunTest(const FString& Parameters)
{
	const FFF_OperationAlertThresholds Thresholds;

	FFF_OperationAlertInputs Healthy;
	TestEqual(TEXT("모두 충분하면 알림이 없다"), FFF_OperationAlertRules::Evaluate(Healthy, Thresholds).Num(), 0);

	FFF_OperationAlertInputs Inputs;
	Inputs.RestroomCoverage = 0.3f;
	Inputs.QueueAbandonsDelta = Thresholds.QueueAbandonsAtLeast - 1;
	Inputs.NoChoiceLeavesDelta = Thresholds.NoChoiceLeavesAtLeast;
	const TArray<EFF_OperationAlertKind> Kinds = FFF_OperationAlertRules::Evaluate(Inputs, Thresholds);
	TestEqual(TEXT("갈 곳 없음 퇴장과 화장실 부족 두 가지"), Kinds.Num(), 2);
	TestTrue(TEXT("퇴장이 먼저"), Kinds.Num() == 2 && Kinds[0] == EFF_OperationAlertKind::NoChoiceLeave);
	TestTrue(TEXT("문턱 아래 줄 이탈은 알리지 않는다"), !Kinds.Contains(EFF_OperationAlertKind::QueueAbandon));

	for (int32 Index = 0; Index < static_cast<int32>(EFF_OperationAlertKind::Count); ++Index)
	{
		TestFalse(TEXT("종류마다 문구가 있다"),
			FFF_OperationAlertRules::GetMessage(static_cast<EFF_OperationAlertKind>(Index)).IsEmpty());
	}
	return true;
}

// 쿨다운: 같은 종류는 쿨다운 동안 다시 안 나오고, 시설 부족은 한 번에 하나, 날이 바뀌면 풀린다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OperationAlert_SubsystemCooldown,
	"FestaFiesta.UI.OperationAlert.SubsystemCooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_OperationAlert_SubsystemCooldown::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UFF_OperationAlertSubsystem> Alerts(NewObject<UFF_OperationAlertSubsystem>(GetTransientPackage()));
	Alerts->ShortageCooldownLiveSeconds = 120.f;

	FFF_OperationAlertInputs Shortages;
	Shortages.HungerCoverage = 0.5f;
	Shortages.RestroomCoverage = 0.3f;
	Shortages.StimulationCoverage = 0.3f;

	Alerts->EvaluateAt(15.f, 1, Shortages);
	TestEqual(TEXT("시설 부족은 한 번에 하나"), Alerts->GetHistory().Num(), 1);
	const EFF_OperationAlertKind First = Alerts->GetHistory()[0].Kind;

	Alerts->EvaluateAt(30.f, 1, Shortages);
	TestEqual(TEXT("다음 판정에 다른 부족이 나온다"), Alerts->GetHistory().Num(), 2);
	TestTrue(TEXT("같은 종류가 연달아 나오지 않는다"), Alerts->GetHistory()[1].Kind != First);

	Alerts->EvaluateAt(45.f, 1, Shortages);
	Alerts->EvaluateAt(60.f, 1, Shortages);
	TestEqual(TEXT("세 종류가 다 나온 뒤에는 쿨다운 동안 조용하다"), Alerts->GetHistory().Num(), 3);

	Alerts->EvaluateAt(15.f + 120.f, 1, Shortages);
	TestEqual(TEXT("쿨다운이 지나면 다시 나온다"), Alerts->GetHistory().Num(), 4);

	Alerts->EvaluateAt(15.f, 2, Shortages);
	TestEqual(TEXT("다음 날은 쿨다운이 풀린다"), Alerts->GetHistory().Num(), 5);
	TestEqual(TEXT("날짜가 기록된다"), Alerts->GetHistory().Last().Day, 2);
	return true;
}

// 위젯: 카드는 쌓였다가 사라지고, 지난 알림 목록은 사라진 카드도 보여 준다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OperationAlert_PanelToastsAndHistory,
	"FestaFiesta.UI.OperationAlert.PanelToastsAndHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_OperationAlert_PanelToastsAndHistory::RunTest(const FString& Parameters)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("에디터 월드"), World))
	{
		return false;
	}
	TStrongObjectPtr<UFF_OperationAlertPanel> Panel(
		CreateWidget<UFF_OperationAlertPanel>(World, UFF_OperationAlertPanel::StaticClass()));
	if (!TestNotNull(TEXT("위젯"), Panel.Get()))
	{
		return false;
	}
	Panel->TakeWidget();
	TestTrue(TEXT("Live 가 아니면 숨는다"), Panel->GetVisibility() == ESlateVisibility::Collapsed);
	Panel->SetForceVisible(true);
	TestTrue(TEXT("보일 때도 월드 클릭은 통과시킨다"), Panel->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);

	auto MakeAlert = [](EFF_OperationAlertKind Kind)
	{
		FFF_OperationAlert Alert;
		Alert.Kind = Kind;
		Alert.Message = FFF_OperationAlertRules::GetMessage(Kind);
		return Alert;
	};

	Panel->AddAlert(MakeAlert(EFF_OperationAlertKind::RestroomShortage));
	Panel->AddAlert(MakeAlert(EFF_OperationAlertKind::QueueAbandon));
	TestEqual(TEXT("카드가 쌓인다"), Panel->GetToastCount(), 2);
	TestEqual(TEXT("최근 것이 위"), Panel->GetToastMessageAt(0).ToString(),
		FFF_OperationAlertRules::GetMessage(EFF_OperationAlertKind::QueueAbandon).ToString());

	for (int32 Index = 0; Index < UFF_OperationAlertPanel::MaxToasts; ++Index)
	{
		Panel->AddAlert(MakeAlert(EFF_OperationAlertKind::FunShortage));
	}
	TestEqual(TEXT("카드는 MaxToasts 장까지만"), Panel->GetToastCount(), UFF_OperationAlertPanel::MaxToasts);

	Panel->AdvanceTime(Panel->DisplaySeconds + 0.1f);
	TestEqual(TEXT("시간이 지나면 카드가 사라진다"), Panel->GetToastCount(), 0);
	TestEqual(TEXT("지난 알림은 남는다"), Panel->GetHistoryCount(), 2 + UFF_OperationAlertPanel::MaxToasts);

	Panel->ToggleHistory();
	TestTrue(TEXT("지난 알림을 펼친다"), Panel->IsHistoryOpen());
	Panel->ToggleHistory();
	TestFalse(TEXT("다시 누르면 닫힌다"), Panel->IsHistoryOpen());

	Panel->ToggleHistory();
	Panel->SetForceVisible(false);
	TestFalse(TEXT("Live 를 벗어나면 펼친 목록을 닫는다"), Panel->IsHistoryOpen());
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
