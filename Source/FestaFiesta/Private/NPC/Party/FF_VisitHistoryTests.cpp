// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Party/FF_VisitHistory.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h"

/**
 * FFF_VisitHistory 검증.
 *
 * 페널티가 실제로 선택을 바꾸는지는 FF_UtilityDestinationSelectorTests 쪽에서 본다.
 * 여기서는 "언제 방문했는지를 정확히 기억하고, 용량을 넘기면 안전하게 잊는가"만 확인한다.
 */
namespace FFF_VisitHistoryTestHelpers
{
	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	FFF_DestinationCandidate MakeVisitTestCandidate(FName DestinationId)
	{
		FFF_DestinationCandidate Candidate;
		Candidate.DestinationId = DestinationId;
		return Candidate;
	}
}

using namespace FFF_VisitHistoryTestHelpers;

// 방문 기록과 조회. "안 가본 곳"과 "방금 다녀온 곳"이 구분되어야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_VisitHistory_DistinguishesNeverVisitedFromJustVisited, "FestaFiesta.NPC.VisitHistory.DistinguishesNeverVisitedFromJustVisited", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_VisitHistory_DistinguishesNeverVisitedFromJustVisited::RunTest(const FString& Parameters)
{
	FFF_VisitHistory History;

	// 아무것도 안 갔을 때
	TestTrue(TEXT("안 가본 곳은 음수다 — 0(방금 다녀옴)과 뜻이 다르다"),
		History.GetMinutesSinceVisit(TEXT("BoothA"), 100.f) < 0.f);

	History.RecordVisit(TEXT("BoothA"), 100.f);
	TestEqual(TEXT("방금 다녀온 곳은 0분이다"), History.GetMinutesSinceVisit(TEXT("BoothA"), 100.f), 0.f);
	TestEqual(TEXT("30분 뒤에는 30분이다"), History.GetMinutesSinceVisit(TEXT("BoothA"), 130.f), 30.f);
	TestTrue(TEXT("기록에 없는 곳은 여전히 음수다"),
		History.GetMinutesSinceVisit(TEXT("BoothB"), 130.f) < 0.f);

	// NAME_None 은 no_choice 다 — 기록하지 않는다. 배회가 이력 한 칸을 먹으면 안 된다
	History.RecordVisit(NAME_None, 140.f);
	TestEqual(TEXT("no_choice(NAME_None)는 기록하지 않는다"), History.DestinationIds.Num(), 1);
	TestTrue(TEXT("NAME_None 조회는 음수다"), History.GetMinutesSinceVisit(NAME_None, 140.f) < 0.f);

	// 시간이 거꾸로 가는 것은 호출부 버그다. 음수를 돌려주면 "안 가본 곳"으로 읽힌다
	TestEqual(TEXT("과거 시각으로 조회해도 음수가 아니라 0이다"),
		History.GetMinutesSinceVisit(TEXT("BoothA"), 50.f), 0.f);
	return true;
}

// 같은 곳을 다시 가면 새 칸을 쓰지 않고 시각만 갱신한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_VisitHistory_RevisitUpdatesTimeWithoutConsumingSlot, "FestaFiesta.NPC.VisitHistory.RevisitUpdatesTimeWithoutConsumingSlot", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_VisitHistory_RevisitUpdatesTimeWithoutConsumingSlot::RunTest(const FString& Parameters)
{
	FFF_VisitHistory History;
	History.RecordVisit(TEXT("BoothA"), 10.f);
	History.RecordVisit(TEXT("BoothA"), 40.f);
	History.RecordVisit(TEXT("BoothA"), 70.f);

	TestEqual(TEXT("같은 곳을 세 번 가도 칸은 1개다"), History.DestinationIds.Num(), 1);
	TestEqual(TEXT("시각은 마지막 방문으로 갱신된다"),
		History.GetMinutesSinceVisit(TEXT("BoothA"), 70.f), 0.f);
	return true;
}

// 용량을 넘기면 가장 오래된 칸을 덮는다. 8만 개체 규모에서 무한 이력은 감당이 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_VisitHistory_ForgetsOldestWhenFull, "FestaFiesta.NPC.VisitHistory.ForgetsOldestWhenFull", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_VisitHistory_ForgetsOldestWhenFull::RunTest(const FString& Parameters)
{
	FFF_VisitHistory History;

	// 용량보다 2개 더 방문한다
	const int32 TotalVisits = FFF_VisitHistory::Capacity + 2;
	for (int32 Index = 0; Index < TotalVisits; ++Index)
	{
		History.RecordVisit(FName(*FString::Printf(TEXT("Booth%d"), Index)), static_cast<float>(Index * 10));
	}

	TestEqual(TEXT("칸 수는 용량을 넘지 않는다"), History.DestinationIds.Num(), FFF_VisitHistory::Capacity);
	TestEqual(TEXT("시각 배열도 같은 길이다"),
		History.VisitedAtLiveMinutes.Num(), History.DestinationIds.Num());

	// 가장 최근 방문은 반드시 남아 있어야 한다 — 그게 페널티가 가장 큰 곳이다
	const FName Newest = FName(*FString::Printf(TEXT("Booth%d"), TotalVisits - 1));
	TestTrue(TEXT("가장 최근 방문은 남아 있다"),
		History.GetMinutesSinceVisit(Newest, static_cast<float>((TotalVisits - 1) * 10)) >= 0.f);

	// 가장 오래된 것은 잊혔다 — 감쇠 때문에 어차피 페널티가 0 에 가깝다
	TestTrue(TEXT("가장 오래된 방문은 잊혔다"),
		History.GetMinutesSinceVisit(TEXT("Booth0"), static_cast<float>(TotalVisits * 10)) < 0.f);

	History.Reset();
	TestEqual(TEXT("Reset 하면 비어 있다"), History.DestinationIds.Num(), 0);
	return true;
}

// 후보 채우기. 안 가본 곳은 건드리지 않는다(기본값이 이미 "안 가봤다").
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_VisitHistory_AppliesOnlyToVisitedCandidates, "FestaFiesta.NPC.VisitHistory.AppliesOnlyToVisitedCandidates", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_VisitHistory_AppliesOnlyToVisitedCandidates::RunTest(const FString& Parameters)
{
	FFF_VisitHistory History;
	History.RecordVisit(TEXT("BoothA"), 100.f);
	History.RecordVisit(TEXT("BoothB"), 130.f);

	TArray<FFF_DestinationCandidate> Candidates = {
		MakeVisitTestCandidate(TEXT("BoothA")),
		MakeVisitTestCandidate(TEXT("BoothB")),
		MakeVisitTestCandidate(TEXT("BoothC")),   // 안 가본 곳
	};

	const int32 AppliedCount = History.ApplyToCandidates(Candidates, 150.f);
	TestEqual(TEXT("방문 기록이 있는 후보 2개만 채워진다"), AppliedCount, 2);
	TestEqual(TEXT("BoothA 는 50분 전"), Candidates[0].MinutesSinceLastVisit, 50.f);
	TestEqual(TEXT("BoothB 는 20분 전"), Candidates[1].MinutesSinceLastVisit, 20.f);
	TestTrue(TEXT("안 가본 곳은 기본값(음수) 그대로다"), Candidates[2].MinutesSinceLastVisit < 0.f);

	// 이력이 비면 아무것도 안 건드린다 — 신규성 선호가 꺼진 상태와 같다
	const FFF_VisitHistory Empty;
	TArray<FFF_DestinationCandidate> Untouched = { MakeVisitTestCandidate(TEXT("BoothA")) };
	TestEqual(TEXT("빈 이력은 0개를 채운다"), Empty.ApplyToCandidates(Untouched, 150.f), 0);
	TestTrue(TEXT("빈 이력이면 후보가 그대로다"), Untouched[0].MinutesSinceLastVisit < 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
