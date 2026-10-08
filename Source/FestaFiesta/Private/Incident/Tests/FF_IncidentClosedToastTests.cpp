// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Incident/Rules/FF_IncidentRules.h"
#include "UI/FF_UIDisplayTextLibrary.h"

/**
 * 사건이 닫혔을 때 토스트로 알릴지(FFF_IncidentRules::ShouldToastClosed)와 그 문구·색.
 * 순수 함수라 World 를 세우지 않는다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IncidentClosedToastTest,
	"FestaFiesta.Incident.ClosedToast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_IncidentClosedToastTest::RunTest(const FString& Parameters)
{
	auto MakeInfo = [](EFF_IncidentState State, bool bWasResponding)
	{
		FFF_IncidentClosedInfo Info;
		Info.FinalState = State;
		Info.bWasResponding = bWasResponding;
		return Info;
	};

	TestTrue(TEXT("A resolved incident is announced"), FFF_IncidentRules::ShouldToastClosed(MakeInfo(EFF_IncidentState::Resolved, true)));
	TestTrue(TEXT("An ignored incident is announced"), FFF_IncidentRules::ShouldToastClosed(MakeInfo(EFF_IncidentState::Ignored, false)));
	TestTrue(TEXT("A missed opportunity is announced"), FFF_IncidentRules::ShouldToastClosed(MakeInfo(EFF_IncidentState::Missed, false)));
	TestFalse(TEXT("An unattended expiry stays quiet"), FFF_IncidentRules::ShouldToastClosed(MakeInfo(EFF_IncidentState::Expired, false)));
	TestTrue(TEXT("An expiry while staff were responding is announced"), FFF_IncidentRules::ShouldToastClosed(MakeInfo(EFF_IncidentState::Expired, true)));
	TestFalse(TEXT("A still-open state is never a closing"), FFF_IncidentRules::ShouldToastClosed(MakeInfo(EFF_IncidentState::Active, true)));

	TestEqual(TEXT("Resolved label"), UFF_UIDisplayTextLibrary::GetIncidentClosedLabel(EFF_IncidentState::Resolved).ToString(), FString(TEXT("완료")));
	TestEqual(TEXT("Expired label"), UFF_UIDisplayTextLibrary::GetIncidentClosedLabel(EFF_IncidentState::Expired).ToString(), FString(TEXT("소멸")));
	TestEqual(TEXT("Ignored label"), UFF_UIDisplayTextLibrary::GetIncidentClosedLabel(EFF_IncidentState::Ignored).ToString(), FString(TEXT("방치")));
	TestEqual(TEXT("Missed label"), UFF_UIDisplayTextLibrary::GetIncidentClosedLabel(EFF_IncidentState::Missed).ToString(), FString(TEXT("놓침")));
	TestTrue(TEXT("A still-open state has no label"), UFF_UIDisplayTextLibrary::GetIncidentClosedLabel(EFF_IncidentState::Active).IsEmpty());

	const FText Title = FText::FromString(TEXT("혼잡"));
	TestEqual(TEXT("The text carries the incident title"),
		UFF_UIDisplayTextLibrary::GetIncidentClosedText(EFF_IncidentState::Resolved, Title).ToString(), FString(TEXT("혼잡 — 처리를 마쳤어요")));
	TestEqual(TEXT("A missing title still reads as a sentence"),
		UFF_UIDisplayTextLibrary::GetIncidentClosedText(EFF_IncidentState::Missed, FText::GetEmpty()).ToString(), FString(TEXT("기회를 놓쳤어요")));

	TestTrue(TEXT("Resolved is good"), UFF_UIDisplayTextLibrary::GetIncidentClosedKind(EFF_IncidentState::Resolved) == EFF_IncidentToastKind::Good);
	TestTrue(TEXT("Ignored is bad"), UFF_UIDisplayTextLibrary::GetIncidentClosedKind(EFF_IncidentState::Ignored) == EFF_IncidentToastKind::Danger);
	TestTrue(TEXT("Expired is neutral"), UFF_UIDisplayTextLibrary::GetIncidentClosedKind(EFF_IncidentState::Expired) == EFF_IncidentToastKind::Neutral);
	TestTrue(TEXT("Missed is neutral"), UFF_UIDisplayTextLibrary::GetIncidentClosedKind(EFF_IncidentState::Missed) == EFF_IncidentToastKind::Neutral);
	return true;
}

#endif
