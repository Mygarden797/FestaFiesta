#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

// 역할: 이동 경로 템플릿과 실제 배치 인스턴스의 서로 다른 검증 조건을 검사한다.
// 템플릿에는 배치 ID가 없어도 되지만, 실제 경로에는 유효한 ID와 너비가 있어야 한다.

#include "Components/SplineComponent.h"
#include "CrowdSystem/Lane/FF_CrowdLane.h"
#include "Engine/World.h"
#include "Misc/DataValidation.h"
#include "Tests/AutomationCommon.h"

namespace
{
	bool HasLaneValidationIssue(const FDataValidationContext& Context, const TCHAR* PropertyName)
	{
		return Context.GetIssues().ContainsByPredicate([PropertyName](const FDataValidationContext::FIssue& Issue)
		{
			return Issue.Message.ToString().Contains(PropertyName);
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdLaneTemplateAndInstanceValidation,
	"FestaFiesta.Crowd.LaneValidation.TemplateAndPlacedInstanceContracts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_CrowdLaneTemplateAndInstanceValidation::RunTest(const FString& Parameters)
{
	(void)Parameters;
	AFF_CrowdLane* Template = GetMutableDefault<AFF_CrowdLane>();
	{
		TGuardValue<FName> RestoreId(Template->LaneId, NAME_None);
		FDataValidationContext TemplateContext;
		Template->IsDataValid(TemplateContext);
		TestFalse(TEXT("Class defaults do not require a placed lane identifier"),
			HasLaneValidationIssue(TemplateContext, TEXT("LaneId")));

		TGuardValue<float> RestoreWidth(Template->LaneHalfWidthCm, -1.f);
		FDataValidationContext InvalidTemplateContext;
		TestTrue(TEXT("Invalid template width is still rejected"),
			Template->IsDataValid(InvalidTemplateContext) == EDataValidationResult::Invalid);
		TestTrue(TEXT("Template validation retains the width diagnostic"),
			HasLaneValidationIssue(InvalidTemplateContext, TEXT("LaneHalfWidthCm")));
	}

	FTestWorldWrapper TestWorld;
	if (!TestTrue(TEXT("Editor test world is created"), TestWorld.CreateTestWorld(EWorldType::Editor)))
	{
		return false;
	}

	AFF_CrowdLane* Lane = TestWorld.GetTestWorld()->SpawnActor<AFF_CrowdLane>();
	if (!TestNotNull(TEXT("A lane instance is placed"), Lane))
	{
		return false;
	}
	Lane->LanePath->SetSplinePoints({FVector::ZeroVector, FVector(2000.f, 0.f, 0.f)}, ESplineCoordinateSpace::Local);
	Lane->LaneId = NAME_None;
	FDataValidationContext MissingIdContext;
	TestTrue(TEXT("Placed lane without an identifier remains invalid"),
		Lane->IsDataValid(MissingIdContext) == EDataValidationResult::Invalid);
	TestTrue(TEXT("Placed lane reports the missing identifier"),
		HasLaneValidationIssue(MissingIdContext, TEXT("LaneId")));

	Lane->LaneId = TEXT("Lane_ValidationTest");
	FDataValidationContext ValidInstanceContext;
	TestTrue(TEXT("A valid placed lane passes after assigning its identifier"),
		Lane->IsDataValid(ValidInstanceContext) == EDataValidationResult::Valid);
	TestEqual(TEXT("The valid instance has no validation errors"), ValidInstanceContext.GetNumErrors(), 0u);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
