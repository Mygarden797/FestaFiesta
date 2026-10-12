#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Staff/FF_StaffDispatchSubsystem.h"
#include "Staff/FF_StaffFacilitySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffDispatchTrainingRuleTest,
	"FestaFiesta.Incident.StaffDispatch.TrainingWorkMinutes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_StaffDispatchTrainingRuleTest::RunTest(const FString& Parameters)
{
	float Minutes = 0.f, Ability = 0.f;
	FFF_StaffDispatchOrigin Member;
	Member.StaffId = 1;
	for (int32 Level = 1; Level <= 3; ++Level)
	{
		Member.TrainingLevel = Level;
		TestTrue(TEXT("원본 레벨을 실제 출동 시간 계산에 전달한다"),
			UFF_StaffDispatchSubsystem::CalculateWork({Member}, 12.f, 1, Minutes, Ability));
		TestEqual(FString::Printf(TEXT("Lv%d 현장 능력"), Level), Ability, static_cast<float>(Level));
		TestEqual(FString::Printf(TEXT("Lv%d 축제 분 환산"), Level), Minutes, 12.f / Level);
	}

	FFF_StaffDispatchOrigin Junior = Member;
	Junior.StaffId = 2;
	Junior.TrainingLevel = 1;
	TestTrue(TEXT("최소 인원을 넘는 팀의 시간을 계산한다"),
		UFF_StaffDispatchSubsystem::CalculateWork({Junior, Member}, 7.f, 1, Minutes, Ability));
	TestEqual(TEXT("선발 순서와 무관하게 Lv3 + 초과 Lv1 절반을 반영한다"), Ability, 3.5f);
	TestEqual(TEXT("7 사람-분은 2 축제 분이며 실제 초로 60배 늘리지 않는다"), Minutes, 2.f);
	TestTrue(TEXT("작업량이 0이면 기존 즉시 처리 선택지를 유지한다"),
		UFF_StaffDispatchSubsystem::CalculateWork({Member}, 0.f, 1, Minutes, Ability));
	TestEqual(TEXT("즉시 처리 작업은 0분이다"), Minutes, 0.f);
	TestFalse(TEXT("최소 인원을 채우지 못한 팀에는 예상 시간을 제시하지 않는다"),
		UFF_StaffDispatchSubsystem::CalculateWork({Member}, 12.f, 2, Minutes, Ability));
	TestEqual(TEXT("실패한 예상의 시간은 초기화된다"), Minutes, 0.f);
	TestEqual(TEXT("실패한 예상의 능력은 초기화된다"), Ability, 0.f);
	return true;
}

#endif
