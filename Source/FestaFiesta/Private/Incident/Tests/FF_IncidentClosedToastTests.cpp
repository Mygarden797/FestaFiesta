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

	// 같은 Expired 상태여도 자연 해소와 시간 만료는 구분한다. 열람은 출동 상태와 독립적이다.
	FFF_IncidentClosedInfo Natural = MakeInfo(EFF_IncidentState::Expired, false);
	Natural.CloseReason = EFF_IncidentCloseReason::NaturalResolution;
	Natural.Title = Title;
	TestFalse(TEXT("열람하지 않은 자연 해소는 조용히 종료"), FFF_IncidentRules::ShouldToastClosed(Natural));
	TestTrue(TEXT("열람한 자연 해소는 출동 없이도 안내"), FFF_IncidentRules::ShouldToastClosed(Natural, true));
	TestEqual(TEXT("자연 해소 라벨"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedLabel(Natural).ToString(), FString(TEXT("자연 해소")));
	TestEqual(TEXT("자연 해소 이유와 제목"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedText(Natural).ToString(), FString(TEXT("혼잡 — 상황이 해소되었습니다")));
	Natural.bWasResponding = true;
	TestTrue(TEXT("출동 중 자연 해소는 반드시 안내"), FFF_IncidentRules::ShouldToastClosed(Natural));
	TestEqual(TEXT("출동 중 자연 해소는 복귀 안내"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedText(Natural).ToString(), FString(TEXT("혼잡 — 상황이 해소되어 스태프가 복귀합니다")));
	Natural.Title = FText::GetEmpty();
	TestEqual(TEXT("제목이 없는 자연 해소도 복귀 안내 유지"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedText(Natural).ToString(), FString(TEXT("상황이 해소되어 스태프가 복귀합니다")));

	FFF_IncidentClosedInfo Completed = MakeInfo(EFF_IncidentState::Resolved, false);
	Completed.CloseReason = EFF_IncidentCloseReason::ResponseCompleted;
	Completed.Title = Title;
	TestTrue(TEXT("완료 직전 복귀를 시작한 스태프도 완료 안내"), FFF_IncidentRules::ShouldToastClosed(Completed));
	TestEqual(TEXT("처리 완료 라벨"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedLabel(Completed).ToString(), FString(TEXT("처리 완료")));
	TestEqual(TEXT("처리 완료 이유와 제목"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedText(Completed).ToString(), FString(TEXT("혼잡 — 처리를 마쳤습니다")));

	FFF_IncidentClosedInfo TimedOut = MakeInfo(EFF_IncidentState::Expired, false);
	TimedOut.CloseReason = EFF_IncidentCloseReason::TimedOut;
	TimedOut.Title = Title;
	TestTrue(TEXT("열람하지 않은 시간 만료도 종료 안내"), FFF_IncidentRules::ShouldToastClosed(TimedOut));
	TestEqual(TEXT("시간 만료 라벨은 자연 해소와 다름"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedLabel(TimedOut).ToString(), FString(TEXT("시간 만료")));
	TestEqual(TEXT("시간 만료 안내"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedText(TimedOut).ToString(), FString(TEXT("혼잡 — 대응 시간이 지나 종료되었습니다")));
	TimedOut.FinalState = EFF_IncidentState::Missed;
	TestTrue(TEXT("기회 시간 만료는 놓침을 설명"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedText(TimedOut).ToString().Contains(TEXT("기회를 놓쳤습니다")));

	FFF_IncidentClosedInfo Reset = Natural;
	Reset.CloseReason = EFF_IncidentCloseReason::SessionReset;
	TestFalse(TEXT("열람·출동했던 사건도 세션 정리는 조용히"), FFF_IncidentRules::ShouldToastClosed(Reset, true));
	TestTrue(TEXT("세션 정리는 라벨 없음"), UFF_UIDisplayTextLibrary::GetIncidentFinishedLabel(Reset).IsEmpty());
	TestTrue(TEXT("세션 정리는 본문 없음"), UFF_UIDisplayTextLibrary::GetIncidentFinishedText(Reset).IsEmpty());

	FFF_IncidentClosedInfo Legacy = MakeInfo(EFF_IncidentState::Expired, false);
	Legacy.Title = Title;
	TestFalse(TEXT("원인이 없는 이전 계약의 무응답 소멸 정책 보존"), FFF_IncidentRules::ShouldToastClosed(Legacy, true));
	TestEqual(TEXT("원인이 없는 이전 계약의 라벨 보존"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedLabel(Legacy).ToString(),
		UFF_UIDisplayTextLibrary::GetIncidentClosedLabel(Legacy.FinalState).ToString());
	TestEqual(TEXT("원인이 없는 이전 계약의 본문 보존"),
		UFF_UIDisplayTextLibrary::GetIncidentFinishedText(Legacy).ToString(),
		UFF_UIDisplayTextLibrary::GetIncidentClosedText(Legacy.FinalState, Legacy.Title).ToString());
	Natural.FinalState = EFF_IncidentState::Active;
	TestFalse(TEXT("활성 상태는 원인이 붙어도 종료 알림 아님"), FFF_IncidentRules::ShouldToastClosed(Natural, true));
	return true;
}

#endif
