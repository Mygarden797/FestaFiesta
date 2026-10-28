#include "UI/PrepHUD/FF_PrepHudTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_RequiredFacilitiesSubsystem.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Misc/AutomationTest.h"
#include "UI/PrepHUD/FF_PrepHudTabButton.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepRequiredAttentionLifecycle,
	"FestaFiesta.UI.PrepHud.RequiredAttentionLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PrepRequiredAttentionLifecycle::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UFF_FestivalDefinition> Definition(NewObject<UFF_FestivalDefinition>());
	UFFZSFacilityDefinition* Info = NewObject<UFFZSFacilityDefinition>(Definition.Get());
	Info->Spec.FacilityDefinitionId = TEXT("Info.AttentionTest");
	Info->Spec.DisplayName = FText::FromString(TEXT("시험 안내소"));
	Info->Spec.Category = EFFZSFacilityCategory::Info;
	Info->Spec.StaffCapacity = 2;
	Info->Spec.MaxInstancesPerWorld = 3;
	Definition->RequiredSetupFacilities.Reset();
	auto& Requirement = Definition->RequiredSetupFacilities.AddDefaulted_GetRef();
	Requirement.FacilityDefinition = Info;
	Requirement.MinimumAssignedStaff = 1;
	FFFZSReadModel Model;
	FFF_RequiredFacilityStaffing Staffing;
	Staffing.bKnown = true;
	Staffing.UnassignedStaffCount = 1;
	const auto Read = [&]() { return UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition.Get(), Model, Staffing); };
	const auto Badge = [](EFF_PrepHudTab Tab, const FFF_RequiredFacilitiesStatus& Status)
	{
		return FFF_PrepHudRules::NeedsRequiredAttention(Tab, EFF_FestivalStage::Setup, Status);
	};
	TestTrue(TEXT("미설치 안내소는 점검에 필수 배지"), Badge(EFF_PrepHudTab::Check, Read()));
	TestFalse(TEXT("아직 배정할 시설이 없으면 스태프 배지는 숨김"), Badge(EFF_PrepHudTab::Staff, Read()));
	TestEqual(TEXT("설치 후 배정까지 같은 안내소 준비 한 건"), FFF_PrepHudRules::CountPendingRequiredTasks(Read()), 1);
	auto& Facility = Model.Facilities.AddDefaulted_GetRef();
	Facility.FacilityInstanceId = FGuid::NewGuid();
	Facility.FacilityDefinitionId = Info->Spec.FacilityDefinitionId;
	Facility.Category = EFFZSFacilityCategory::Info;
	Facility.State = EFFZSFacilityState::Active;
	TestTrue(TEXT("설치 뒤 필수 배정이 남으면 스태프 배지"), Badge(EFF_PrepHudTab::Staff, Read()));

	UClass* TabClass = LoadClass<UFF_PrepHudTabButton>(nullptr,
		TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepHudTabButton.WBP_FF_PrepHudTabButton_C"));
	if (!TestNotNull(TEXT("저장된 준비 탭 클래스"), TabClass)) { return false; }
	TStrongObjectPtr<UFF_PrepHudTabButton> Tab(NewObject<UFF_PrepHudTabButton>(GetTransientPackage(), TabClass));
	Tab->SetRequiredAttention(Badge(EFF_PrepHudTab::Staff, Read()));
	Tab->SetSelected(true);
	TestTrue(TEXT("탭을 열어도 필수 배지는 유지"), Tab->HasRequiredAttention());
	Tab->SetSelected(false);
	TestTrue(TEXT("탭을 닫아도 미충족 상태 유지"), Tab->HasRequiredAttention());

	Staffing.AssignedStaffByFacility.Add(Facility.FacilityInstanceId, 1);
	Staffing.UnassignedStaffCount = 0;
	TestTrue(TEXT("원본 필수 조건 완료"), Read().bCanProceed);
	Tab->SetRequiredAttention(Badge(EFF_PrepHudTab::Staff, Read()));
	TestFalse(TEXT("실제 배정 완료 후 배지 해제"), Tab->HasRequiredAttention());
	TestFalse(TEXT("선택 시설이 더 없어도 점검 배지 해제"), Badge(EFF_PrepHudTab::Check, Read()));
	Staffing.AssignedStaffByFacility.Reset();
	TestTrue(TEXT("배정을 해제하면 다시 표시"), Badge(EFF_PrepHudTab::Staff, Read()));
	Facility.bNeedsRelocation = true;
	TestTrue(TEXT("재배치가 필요하면 점검 배지"), Badge(EFF_PrepHudTab::Check, Read()));
	TestFalse(TEXT("재배치 전 스태프 행동을 앞서 요구하지 않는다"), Badge(EFF_PrepHudTab::Staff, Read()));
	TestFalse(TEXT("Live에는 준비 배지 없음"), FFF_PrepHudRules::NeedsRequiredAttention(EFF_PrepHudTab::Check, EFF_FestivalStage::Live, Read()));
	const auto Unknown = UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(nullptr, Model);
	TestTrue(TEXT("원본 미확인은 점검에서 확인"), Badge(EFF_PrepHudTab::Check, Unknown));
	TestFalse(TEXT("원본 미확인을 인력 부족으로 추정하지 않는다"), Badge(EFF_PrepHudTab::Staff, Unknown));
	return true;
}

#endif
