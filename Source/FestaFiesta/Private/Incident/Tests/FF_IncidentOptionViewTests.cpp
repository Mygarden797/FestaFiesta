// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Incident/FF_IncidentDefinition.h"
#include "Incident/FF_IncidentOption.h"
#include "Incident/Effects/FF_IncidentEffect_Reward.h"
#include "Incident/Rules/FF_IncidentRules.h"
#include "UObject/Package.h"

/**
 * 선택지 View 변환. [참고] 작업사항 저장/2026-09-29_사건_비용보상_표시_구조가이드.md §4
 *
 * 정의 에셋을 Transient 로 만들어 World 없이 확인한다.
 * null 선택지가 자리를 지키는지, 인원 역전이 정리되는지, 보상 문구가 효과에서 오는지를 본다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentOptionView_BuildKeepsIndexAndClamps, "FestaFiesta.Incident.OptionView.BuildKeepsIndexAndClamps", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_IncidentOptionView_BuildKeepsIndexAndClamps::RunTest(const FString& Parameters)
{
	UFF_IncidentDefinition* Definition = NewObject<UFF_IncidentDefinition>(GetTransientPackage());

	UFF_IncidentOption* Swapped = NewObject<UFF_IncidentOption>(Definition);
	Swapped->Label = FText::FromString(TEXT("Swapped"));
	Swapped->CostGold = 30;
	Swapped->MinStaff = 3;
	Swapped->WorkAmount = 2.f;
	UFF_IncidentEffect_Reward* Reward = NewObject<UFF_IncidentEffect_Reward>(Swapped);
	Reward->RewardGold = 50;
	Swapped->OnResolvedEffect.Add(nullptr);
	Swapped->OnResolvedEffect.Add(Reward);

	UFF_IncidentOption* NoEffect = NewObject<UFF_IncidentOption>(Definition);
	NoEffect->MinStaff = 0;

	Definition->Options.Add(Swapped);
	Definition->Options.Add(nullptr);
	Definition->Options.Add(NoEffect);

	TArray<FFF_IncidentOptionView> Views;
	Views.AddDefaulted(5);
	FFF_IncidentRules::BuildOptionViews(*Definition, Views);

	if (!TestEqual(TEXT("선택지 수는 정의와 같다"), Views.Num(), 3))
	{
		return false;
	}

	TestEqual(TEXT("0번 인덱스"), Views[0].OptionIndex, 0);
	TestTrue(TEXT("0번 유효"), Views[0].bValid);
	TestEqual(TEXT("0번 비용"), Views[0].CostGold, 30);
	TestEqual(TEXT("0번 MinStaff"), Views[0].MinStaff, 3);
	TestEqual(TEXT("0번 작업량"), Views[0].WorkAmount, 2.f);
	TestEqual(TEXT("보상 문구는 효과에서 오고 빈 효과 칸은 건너뛴다"), Views[0].RewardText.ToString(), FString(TEXT("50 G")));

	TestEqual(TEXT("null 선택지도 자리를 지킨다"), Views[1].OptionIndex, 1);
	TestFalse(TEXT("null 선택지는 무효"), Views[1].bValid);

	TestEqual(TEXT("2번 인덱스는 밀리지 않는다"), Views[2].OptionIndex, 2);
	TestTrue(TEXT("2번 유효"), Views[2].bValid);
	TestEqual(TEXT("MinStaff 는 1 미만으로 내려가지 않는다"), Views[2].MinStaff, 1);
	TestTrue(TEXT("효과가 없으면 보상 문구가 비었다"), Views[2].RewardText.IsEmpty());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
