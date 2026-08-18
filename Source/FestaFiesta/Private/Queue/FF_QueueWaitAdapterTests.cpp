// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Queue/FF_QueueWaitAdapter.h"
#include "Queue/FF_FacilityQueue.h"
#include "Queue/FF_QueueSubsystem.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h"
#include "DataTables/FF_UtilityCoefficientRow.h"
#include "Engine/DataTable.h"

// 이 파일의 존재 이유는 단 하나다: 초/분 혼동이 다시 들어오면 빌드가 아니라 테스트가 잡게 한다.
// 60배 오차는 컴파일도 되고 크래시도 안 나고, 그냥 모든 시설이 조용히 no_choice로 밀린다.

namespace FFF_QueueWaitAdapterTestHelpers
{
	FFF_QueueState MakeState(int32 RemainingMembers, float ServiceRate, bool bAcceptingNew = true,
		bool bIsOpen = true)
	{
		FFF_QueueState State;
		State.FacilityId = 1;
		State.RemainingServiceMemberCount = RemainingMembers;
		State.OccupiedMemberCount = RemainingMembers;
		State.WaitingPartyCount = RemainingMembers;
		State.MaxCapacityMembers = 50;
		State.ServiceRate = ServiceRate;
		State.bIsOpen = bIsOpen;
		State.bAcceptingNew = bAcceptingNew;
		return State;
	}

	FFF_DestinationCandidate MakeQueueCandidate(FName Id, FName Category, bool bIsOpen = true)
	{
		FFF_DestinationCandidate Candidate;
		Candidate.DestinationId = Id;
		Candidate.Category = Category;
		Candidate.BaseAttraction = 0.9f;
		Candidate.DistanceMeters = 30.f;
		Candidate.EstimatedWaitMinutes = 0.f;
		Candidate.CurrentOccupancy = 10.f;
		Candidate.Capacity = 50.f;
		Candidate.bIsOpen = bIsOpen;
		return Candidate;
	}

	UDataTable* MakeAdapterTestCoefficientTable()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FFF_UtilityCoefficientRow::StaticStruct();

		auto AddRow = [Table](FName Id, float DefaultValue)
		{
			FFF_UtilityCoefficientRow Row;
			Row.CoefficientId = Id;
			Row.DefaultValue = DefaultValue;
			Table->AddRow(Id, Row);
		};

		AddRow(TEXT("weight_attraction"), 1.0f);
		AddRow(TEXT("lambda_affinity"), 0.6f);
		AddRow(TEXT("weight_distance"), 0.8f);
		AddRow(TEXT("weight_wait"), 0.7f);
		AddRow(TEXT("weight_density"), 0.5f);
		AddRow(TEXT("no_choice_utility"), 0.15f);
		AddRow(TEXT("utility_noise_scale"), 0.3f);
		AddRow(TEXT("distance_ref_meters"), 400.f);
		AddRow(TEXT("wait_ref_minutes"), 30.f);
		AddRow(TEXT("density_ref_ratio"), 1.f);
		AddRow(TEXT("preference_vector_scale_ref"), 17.f);
		return Table;
	}

	float FindAdapterProbability(const FFF_DestinationDecision& Decision, FName Id)
	{
		for (const FFF_DestinationUtilityBreakdown& Breakdown : Decision.AllCandidateScores)
		{
			if (Breakdown.DestinationId == Id)
			{
				return Breakdown.ChoiceProbability;
			}
		}
		return -1.f;
	}
}

using namespace FFF_QueueWaitAdapterTestHelpers;

// 1. 단위 변환 왕복
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_UnitConversionRoundTrips, "FestaFiesta.Queue.WaitAdapter.UnitConversionRoundTrips", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_UnitConversionRoundTrips::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("120초는 2분이다"), FFF_QueueWaitAdapter::ToDecisionMinutes(120.f), 2.f, 0.0001f);
	TestEqual(TEXT("2분은 120초다"), FFF_QueueWaitAdapter::ToQueueSeconds(2.f), 120.f, 0.0001f);
	TestEqual(TEXT("왕복 변환이 원값을 보존한다"),
		FFF_QueueWaitAdapter::ToQueueSeconds(FFF_QueueWaitAdapter::ToDecisionMinutes(987.f)), 987.f, 0.001f);
	return true;
}

// 2. ExpectedWait 스냅샷 계산 (설계문서 9.1)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_ComputesExpectedWaitFromSnapshot, "FestaFiesta.Queue.WaitAdapter.ComputesExpectedWaitFromSnapshot", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_ComputesExpectedWaitFromSnapshot::RunTest(const FString& Parameters)
{
	// 20초/인 → 처리율 0.05. 앞에 9명, 내 일행 1명 → 10 / 0.05 = 200초
	const FFF_QueueState State = MakeState(/*RemainingMembers=*/9, /*ServiceRate=*/0.05f);
	TestEqual(TEXT("(앞 9명 + 나 1명) / 0.05 = 200초"),
		FFF_QueueWaitAdapter::ComputeExpectedWaitSeconds(State, 1), 200.f, 0.01f);

	// 처리율 0을 0초로 돌려주면 "대기 없음"으로 읽혀 모든 Party가 여기로 몰린다.
	const FFF_QueueState Stalled = MakeState(5, 0.f);
	TestEqual(TEXT("처리율 0인 줄은 도달 불가 센티널(24시간)을 돌려준다"),
		FFF_QueueWaitAdapter::ComputeExpectedWaitSeconds(Stalled, 1),
		FFF_QueueTimings::UnreachableWaitSeconds, 0.01f);

	// 음수 일행 수 같은 쓰레기 입력이 들어와도 음수 대기가 나오면 안 된다.
	TestTrue(TEXT("잘못된 입력에도 대기시간이 음수가 되지 않는다"),
		FFF_QueueWaitAdapter::ComputeExpectedWaitSeconds(State, -5) >= 0.f);
	return true;
}

// 3. ★ 60배 회귀 방지 ★ — 이 파일의 핵심
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_DoesNotLeakSecondsIntoMinutesField, "FestaFiesta.Queue.WaitAdapter.DoesNotLeakSecondsIntoMinutesField", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_DoesNotLeakSecondsIntoMinutesField::RunTest(const FString& Parameters)
{
	// 앞에 9명, 20초/인 → 200초 = 3분 20초. 현실적인 인기 부스 대기다.
	const FFF_QueueState State = MakeState(9, 0.05f);
	FFF_DestinationCandidate Candidate = MakeQueueCandidate(TEXT("FoodStallA"), TEXT("food_stall"));

	FFF_QueueWaitAdapter::ApplyQueueStateToCandidate(Candidate, State, /*IncomingMemberCount=*/1);

	TestEqual(TEXT("분 단위로 3.33이 들어간다"), Candidate.EstimatedWaitMinutes, 200.f / 60.f, 0.001f);
	// 초가 새면 200이 들어가고, wait_ref_minutes=30 기준으로 정규화가 1.0에 박혀 버린다.
	TestTrue(TEXT("초 값(200)이 분 필드로 새지 않는다"), Candidate.EstimatedWaitMinutes < 30.f);
	return true;
}

// 4. 실제 Selector에 통과시켜 확인 — 단위가 새면 후보가 no_choice로 밀린다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_CorrectUnitKeepsCandidateCompetitive, "FestaFiesta.Queue.WaitAdapter.CorrectUnitKeepsCandidateCompetitive", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_CorrectUnitKeepsCandidateCompetitive::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeAdapterTestCoefficientTable();
	TMap<FName, float> Preference;
	Preference.Add(TEXT("food_stall"), 10.f);

	const FFF_QueueState State = MakeState(9, 0.05f);   // 200초 = 3분 20초

	// (가) 어댑터를 거친 올바른 값
	TArray<FFF_DestinationCandidate> Correct;
	Correct.Add(MakeQueueCandidate(TEXT("Stall"), TEXT("food_stall")));
	FFF_QueueWaitAdapter::ApplyQueueStateToCandidate(Correct[0], State, 1);

	// (나) 초를 그대로 꽂은 버그 재현
	TArray<FFF_DestinationCandidate> Buggy;
	Buggy.Add(MakeQueueCandidate(TEXT("Stall"), TEXT("food_stall")));
	Buggy[0].EstimatedWaitMinutes = FFF_QueueWaitAdapter::ComputeExpectedWaitSeconds(State, 1);

	const float CorrectProb = FindAdapterProbability(
		FFF_UtilityDestinationSelector::Choose(Correct, Preference, Table, 1, 1, 0), TEXT("Stall"));
	const float BuggyProb = FindAdapterProbability(
		FFF_UtilityDestinationSelector::Choose(Buggy, Preference, Table, 1, 1, 0), TEXT("Stall"));
	const float CorrectNoChoice = FindAdapterProbability(
		FFF_UtilityDestinationSelector::Choose(Correct, Preference, Table, 1, 1, 0), NAME_None);

	TestTrue(TEXT("3분 대기 부스는 no_choice보다 매력적으로 남는다"), CorrectProb > CorrectNoChoice);
	TestTrue(TEXT("초를 그대로 넣으면 같은 부스의 선택 확률이 무너진다"), BuggyProb < CorrectProb);
	return true;
}

// 5. bIsOpen 은 AND 병합 — 대입이면 호출부가 닫아둔 시설이 열린다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_OpenFlagIsAndMerged, "FestaFiesta.Queue.WaitAdapter.OpenFlagIsAndMerged", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_OpenFlagIsAndMerged::RunTest(const FString& Parameters)
{
	const FFF_QueueState Accepting = MakeState(2, 0.05f, /*bAcceptingNew=*/true);
	const FFF_QueueState Full = MakeState(50, 0.05f, /*bAcceptingNew=*/false);

	// 운영시간 종료 등으로 호출부가 이미 닫아둔 후보 — 대기열이 열려 있어도 닫힌 채여야 한다.
	FFF_DestinationCandidate ClosedByCaller = MakeQueueCandidate(TEXT("A"), TEXT("food_stall"), /*bIsOpen=*/false);
	FFF_QueueWaitAdapter::ApplyQueueStateToCandidate(ClosedByCaller, Accepting, 1);
	TestFalse(TEXT("호출부가 닫은 시설을 대기열이 열어주지 않는다"), ClosedByCaller.bIsOpen);

	// 만석은 후보에서 빼지 않는다. 앞 Party 하나 빠지면 풀리는 순간 상태라, 이걸로 후보를 닫으면
	// 인기 부스가 결정할 때마다 사라졌다 나타났다 한다. 혼잡은 대기시간으로 표현된다.
	FFF_DestinationCandidate OpenButFull = MakeQueueCandidate(TEXT("B"), TEXT("food_stall"), /*bIsOpen=*/true);
	FFF_QueueWaitAdapter::ApplyQueueStateToCandidate(OpenButFull, Full, 1);
	TestTrue(TEXT("만석이어도 후보에 남는다 — 폐쇄가 아니다"), OpenButFull.bIsOpen);
	TestTrue(TEXT("대신 대기시간이 크게 잡힌다"), OpenButFull.EstimatedWaitMinutes > 5.f);

	// 폐쇄는 진짜로 뺀다.
	const FFF_QueueState Closed = MakeState(2, 0.05f, /*bAcceptingNew=*/false, /*bIsOpen=*/false);
	FFF_DestinationCandidate ClosedFacility = MakeQueueCandidate(TEXT("D"), TEXT("food_stall"), /*bIsOpen=*/true);
	FFF_QueueWaitAdapter::ApplyQueueStateToCandidate(ClosedFacility, Closed, 1);
	TestFalse(TEXT("폐쇄된 시설은 후보에서 닫힌다"), ClosedFacility.bIsOpen);

	FFF_DestinationCandidate BothOpen = MakeQueueCandidate(TEXT("C"), TEXT("food_stall"), /*bIsOpen=*/true);
	FFF_QueueWaitAdapter::ApplyQueueStateToCandidate(BothOpen, Accepting, 1);
	TestTrue(TEXT("양쪽 다 열려 있으면 열린 채로 남는다"), BothOpen.bIsOpen);
	return true;
}

// 6. 밀도 항 입력은 건드리지 않는다 — 대기 인원은 이미 셀 밀도에 잡혀 있다 (설계문서 8.4)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_DoesNotTouchOccupancyFields, "FestaFiesta.Queue.WaitAdapter.DoesNotTouchOccupancyFields", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_DoesNotTouchOccupancyFields::RunTest(const FString& Parameters)
{
	FFF_DestinationCandidate Candidate = MakeQueueCandidate(TEXT("A"), TEXT("food_stall"));
	const float OriginalOccupancy = Candidate.CurrentOccupancy;
	const float OriginalCapacity = Candidate.Capacity;
	const float OriginalAttraction = Candidate.BaseAttraction;
	const float OriginalDistance = Candidate.DistanceMeters;

	FFF_QueueWaitAdapter::ApplyQueueStateToCandidate(Candidate, MakeState(40, 0.05f), 1);

	TestEqual(TEXT("CurrentOccupancy를 덮어쓰지 않는다 (Crowd 밀도와 이중 계산 방지)"), Candidate.CurrentOccupancy, OriginalOccupancy);
	TestEqual(TEXT("Capacity를 덮어쓰지 않는다"), Candidate.Capacity, OriginalCapacity);
	TestEqual(TEXT("BaseAttraction을 덮어쓰지 않는다"), Candidate.BaseAttraction, OriginalAttraction);
	TestEqual(TEXT("DistanceMeters를 덮어쓰지 않는다"), Candidate.DistanceMeters, OriginalDistance);
	return true;
}

// 7. 코어 식과 어댑터 식이 갈라지지 않았는지.
//    갈라지면 "가입 전 예측"과 "QueueLog에 기록된 예측"이 달라져 10.2절 튜닝 지표가 통째로 무의미해진다.
//    두 곳에 같은 식이 복사되어 있는 한, 이 테스트가 유일한 연결고리다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_MatchesQueueCoreFormula, "FestaFiesta.Queue.WaitAdapter.MatchesQueueCoreFormula", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_MatchesQueueCoreFormula::RunTest(const FString& Parameters)
{
	FFF_QueueConfig Config;
	Config.MaxCapacityMembers = 50;
	Config.ServiceSecondsPerMember = 20.f;
	Config.ConcurrentServiceSlots = 1;

	FFF_QueueLaneGeometry Lane;
	Lane.HeadLocation = FVector::ZeroVector;
	Lane.TailDirection = FVector(1.f, 0.f, 0.f);

	FFF_FacilityQueue Queue;
	Queue.Configure(1, Config, Lane);

	FFF_JoinRequest Request;
	Request.PartyId = 1;
	Request.FacilityId = 1;
	Request.MemberCount = 7;
	Request.MaxWaitSeconds = -1.f;
	int32 SlotIndex = INDEX_NONE;
	float EstimatedWait = 0.f;
	Queue.TryJoin(Request, 0.f, SlotIndex, EstimatedWait);

	const FFF_QueueState State = Queue.GetState();
	for (int32 PartySize = 1; PartySize <= 4; ++PartySize)
	{
		TestEqual(*FString::Printf(TEXT("%d인 Party: 어댑터 초 == 코어 초"), PartySize),
			FFF_QueueWaitAdapter::ComputeExpectedWaitSeconds(State, PartySize),
			Queue.GetExpectedWaitSeconds(PartySize), 0.001f);

		TestEqual(*FString::Printf(TEXT("%d인 Party: 어댑터 분 == 코어 분"), PartySize),
			FFF_QueueWaitAdapter::ToDecisionMinutes(FFF_QueueWaitAdapter::ComputeExpectedWaitSeconds(State, PartySize)),
			Queue.GetExpectedWaitMinutes(PartySize), 0.001f);
	}

	// (앞 7명 + 나 1명) / (1/20) = 160초. 손으로 계산한 값과도 맞아야 한다.
	TestEqual(TEXT("실제 값이 손계산과 일치한다"),
		FFF_QueueWaitAdapter::ComputeExpectedWaitSeconds(State, 1), 160.f, 0.001f);
	return true;
}

// 8. 단위를 이름에 박은 접근자 — 초당/분당을 섞으면 60배가 조용히 들어간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_ServiceRateAccessorsCarryUnits, "FestaFiesta.Queue.WaitAdapter.ServiceRateAccessorsCarryUnits", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_ServiceRateAccessorsCarryUnits::RunTest(const FString& Parameters)
{
	const FFF_QueueState State = MakeState(0, 0.05f);   // 20초에 1명
	TestEqual(TEXT("초당 접근자는 원본 그대로다"), State.GetServiceRatePerSecond(), 0.05f, 1e-6f);
	TestEqual(TEXT("분당 접근자는 60배 — 20초에 1명이면 분당 3명"), State.GetServiceRatePerMinute(), 3.f, 1e-4f);
	TestTrue(TEXT("두 접근자가 같은 값을 돌려주지 않는다"),
		!FMath::IsNearlyEqual(State.GetServiceRatePerSecond(), State.GetServiceRatePerMinute()));
	return true;
}

// 9. QueueLog 분 단위 뷰 — 결정 측 스냅샷(분)과 조인할 때 쓴다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_QueueLogExposesMinutes, "FestaFiesta.Queue.WaitAdapter.QueueLogExposesMinutes", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_QueueLogExposesMinutes::RunTest(const FString& Parameters)
{
	FFF_QueueLog Log;
	Log.JoinSimTime = 120.f;
	Log.ExitSimTime = 300.f;
	Log.EstimatedWaitAtJoinSeconds = 90.f;
	Log.ActualWaitSeconds = 150.f;
	Log.ServiceSeconds = 30.f;

	TestEqual(TEXT("가입 시각 2분"), Log.GetJoinSimTimeMinutes(), 2.f, 1e-4f);
	TestEqual(TEXT("퇴장 시각 5분"), Log.GetExitSimTimeMinutes(), 5.f, 1e-4f);
	TestEqual(TEXT("예상 대기 1.5분"), Log.GetEstimatedWaitAtJoinMinutes(), 1.5f, 1e-4f);
	TestEqual(TEXT("실제 대기 2.5분"), Log.GetActualWaitMinutes(), 2.5f, 1e-4f);
	TestEqual(TEXT("서비스 0.5분"), Log.GetServiceMinutes(), 0.5f, 1e-4f);
	// 저장은 초 하나만 한다. 분은 파생값이라 어긋날 수 없다.
	TestEqual(TEXT("초 필드는 그대로 남는다"), Log.ActualWaitSeconds, 150.f, 1e-4f);
	return true;
}

// 10. 미등록 시설은 fail-closed.
//     "지금 들어갈 수 있는가"를 판정하는 곳이 이 어댑터 한 곳뿐이라, 여기서 열어두면
//     존재하지 않는 시설이 "열려 있고 대기 0"인 최고의 후보가 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_UnregisteredFacilityFailsClosed, "FestaFiesta.Queue.WaitAdapter.UnregisteredFacilityFailsClosed", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_UnregisteredFacilityFailsClosed::RunTest(const FString& Parameters)
{
	// 시설을 하나도 등록하지 않은 빈 서브시스템이면 충분하다.
	UFF_QueueSubsystem* EmptyQueue = NewObject<UFF_QueueSubsystem>();
	TestFalse(TEXT("전제: 이 시설은 등록돼 있지 않다"), EmptyQueue->IsFacilityRegistered(42));

	FFF_DestinationCandidate Ghost = MakeQueueCandidate(TEXT("ghost_booth"), TEXT("food_stall"), /*bIsOpen=*/true);
	Ghost.EstimatedWaitMinutes = 0.f;   // 후보 생성부가 0으로 두고 소유권을 어댑터에 넘긴 상태

	const bool bApplied = FFF_QueueWaitAdapter::ApplyToCandidate(EmptyQueue, Ghost, 42, 1);

	TestFalse(TEXT("반영했다고 보고하지 않는다"), bApplied);
	TestFalse(TEXT("후보에서 닫힌다 — 열어두면 유령 시설이 1등이 된다"), Ghost.bIsOpen);
	TestTrue(TEXT("대기시간도 0으로 남지 않는다"), Ghost.EstimatedWaitMinutes > 1000.f);
	return true;
}

// 11. Resolver 오버로드 — 줄 없는 목적지(INDEX_NONE)는 손대지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_ResolverSkipsQueuelessDestinations, "FestaFiesta.Queue.WaitAdapter.ResolverSkipsQueuelessDestinations", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_ResolverSkipsQueuelessDestinations::RunTest(const FString& Parameters)
{
	UFF_QueueSubsystem* EmptyQueue = NewObject<UFF_QueueSubsystem>();

	TArray<FFF_DestinationCandidate> Candidates;
	Candidates.Add(MakeQueueCandidate(TEXT("photo_zone"), TEXT("ambience_photo_zone"), /*bIsOpen=*/true));
	const float Before = Candidates[0].EstimatedWaitMinutes;

	const int32 Applied = FFF_QueueWaitAdapter::ApplyToCandidates(
		EmptyQueue, Candidates, [](FName) -> int32 { return INDEX_NONE; }, 1);

	TestEqual(TEXT("줄 없는 목적지는 반영 대상이 아니다"), Applied, 0);
	TestEqual(TEXT("대기시간을 건드리지 않는다"), Candidates[0].EstimatedWaitMinutes, Before, 1e-6f);
	TestTrue(TEXT("열림 상태도 유지된다 — 미등록과 달리 정상 경로다"), Candidates[0].bIsOpen);
	return true;
}

// 12. 목적지 ID 는 시설 인스턴스 GUID 문자열이다.
//     이 계약이 깨지면 대기시간이 통째로 반영되지 않는데, 크래시도 경고도 없이 조용히 그렇게 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_DestinationIdCarriesFacilityGuid, "FestaFiesta.Queue.WaitAdapter.DestinationIdCarriesFacilityGuid", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_DestinationIdCarriesFacilityGuid::RunTest(const FString& Parameters)
{
	// 결정 계층이 후보에 담는 것과 같은 형태로 왕복시킨다.
	const FGuid Original = FGuid::NewGuid();
	const FName DestinationId(*Original.ToString());

	FGuid Parsed;
	TestTrue(TEXT("목적지 ID 에서 GUID 를 되찾을 수 있다"),
		FFF_ZoneSystemBridge::TryParseDestinationId(DestinationId, Parsed));
	TestEqual(TEXT("왕복해도 같은 GUID 다"), Parsed, Original);

	// GUID 가 아닌 이름은 실패해야 한다. 여기서 성공하면 엉뚱한 시설로 매핑된다.
	FGuid Garbage;
	TestFalse(TEXT("GUID 가 아닌 이름은 파싱되지 않는다"),
		FFF_ZoneSystemBridge::TryParseDestinationId(TEXT("ramen_booth"), Garbage));
	TestFalse(TEXT("빈 이름도 파싱되지 않는다"),
		FFF_ZoneSystemBridge::TryParseDestinationId(NAME_None, Garbage));
	return true;
}

// 13. 서브시스템이 없어도 결정은 돌아야 한다.
//     대기열이 없는 테스트 맵에서 후보가 닫히면 NPC 가 아무 데도 못 간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueWaitAdapter_ZoneBridgePathDegradesQuietly, "FestaFiesta.Queue.WaitAdapter.ZoneBridgePathDegradesQuietly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueWaitAdapter_ZoneBridgePathDegradesQuietly::RunTest(const FString& Parameters)
{
	TArray<FFF_DestinationCandidate> Candidates;
	Candidates.Add(MakeQueueCandidate(TEXT("A"), TEXT("food_stall"), /*bIsOpen=*/true));
	const float BeforeWait = Candidates[0].EstimatedWaitMinutes;

	// 월드가 없는 경우.
	TestEqual(TEXT("월드가 없으면 0 건"),
		FFF_QueueWaitAdapter::ApplyFromZoneBridge(nullptr, Candidates, 1), 0);

	// 브리지가 없는 경우.
	UFF_QueueSubsystem* EmptyQueue = NewObject<UFF_QueueSubsystem>();
	TestEqual(TEXT("브리지가 없으면 0 건"),
		FFF_QueueWaitAdapter::ApplyFromZoneBridge(EmptyQueue, nullptr, Candidates, 1), 0);

	// 어느 경우에도 후보를 건드리지 않아야 한다.
	TestEqual(TEXT("대기시간을 건드리지 않는다"), Candidates[0].EstimatedWaitMinutes, BeforeWait, 1e-6f);
	TestTrue(TEXT("후보를 닫지 않는다"), Candidates[0].bIsOpen);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
