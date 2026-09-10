// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Integration/FF_ZoneSystemBridge.h"

/**
 * FFF_ZoneSystemBridge::BuildCandidates 검증.
 *
 * ### 여기서 검증하지 않는 것 둘
 *
 * **대기시간** — BuildCandidates 는 `EstimatedWaitMinutes` 를 채우지 않는다(2026-08-10 정리).
 * 소유자는 `FFF_QueueWaitAdapter` 이고 그쪽 테스트가 검증한다. 여기서는 0으로 남는 것만 확인한다.
 *
 * **"지금 이용 가능한가"** — `QueueState` 로 후보를 자르지 않는다. 정원이 찬 시설을 후보에서 뺄지는
 * 미확정 정책이고 판정 지점을 Queue 한 곳으로 모으기로 했다. 그래서 아래 테스트는 Full/Draining/
 * Blocked 시설도 **후보로 들어오는 것**을 기대값으로 고정한다 — 정책이 바뀌면 이 테스트가 깨지고,
 * 깨지는 것이 맞다(어느 쪽으로 정했는지가 여기 드러나야 한다).
 */
namespace FFF_ZoneSystemBridgeTestHelpers
{
	FFFZSZoneReadModel MakeBridgeTestZone(FName ZoneId, const TArray<FName>& NeighborIds,
		int32 ReportedPopulation = 0, int32 SoftCapacity = 100)
	{
		FFFZSZoneReadModel Zone;
		Zone.ZoneId = ZoneId;
		Zone.NeighborZoneIds = NeighborIds;
		Zone.ReportedPopulation = ReportedPopulation;
		Zone.SoftCapacity = SoftCapacity;
		return Zone;
	}

	// Index 로 FacilityInstanceId 를 만든다 — 테스트가 결정적이어야 하므로 NewGuid 를 쓰지 않는다.
	FGuid MakeBridgeTestFacilityId(int32 Index)
	{
		return FGuid(0, 0, 0, Index);
	}

	// 기본은 "정상 영업 중인 Booth 음식 부스". 각 테스트가 필요한 필드만 덮어쓴다.
	FFFZSFacilityReadModel MakeBridgeTestFacility(
		int32 Index,
		FName ZoneId,
		EFFZSFacilityCategory Category = EFFZSFacilityCategory::Booth,
		FName FacilityDefinitionId = TEXT("Booth_FoodStall_Test"))
	{
		FFFZSFacilityReadModel Facility;
		Facility.FacilityInstanceId = MakeBridgeTestFacilityId(Index);
		Facility.FacilityDefinitionId = FacilityDefinitionId;
		Facility.ZoneId = ZoneId;
		Facility.WorldLocation = FVector::ZeroVector;
		Facility.Category = Category;
		Facility.State = EFFZSFacilityState::Active;
		Facility.QueueState = EFFZSQueueState::Open;
		Facility.bNeedsRelocation = false;
		Facility.WaitingCount = 0;
		return Facility;
	}

	bool ContainsFacility(const TArray<FFF_DestinationCandidate>& Candidates, int32 Index)
	{
		const FName Expected = FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(
			MakeBridgeTestFacilityId(Index));
		return Candidates.ContainsByPredicate(
			[Expected](const FFF_DestinationCandidate& Candidate) { return Candidate.DestinationId == Expected; });
	}
}

using namespace FFF_ZoneSystemBridgeTestHelpers;

// 현재 Zone 과 인접 Zone 만 후보가 된다. 2홉 떨어진 Zone 은 들어오지 않는다 —
// 이 필터가 없으면 (동시 Party 수) × (전체 시설) 이 되어 프레임 예산이 무너진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_OnlyCurrentAndNeighborZones, "FestaFiesta.NPC.ZoneSystemBridge.OnlyCurrentAndNeighborZones", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_OnlyCurrentAndNeighborZones::RunTest(const FString& Parameters)
{
	FFFZSReadModel ReadModel;
	// A ↔ B ↔ C. A 에서 보면 C 는 2홉이다.
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneA"), { TEXT("ZoneB") }));
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneB"), { TEXT("ZoneA"), TEXT("ZoneC") }));
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneC"), { TEXT("ZoneB") }));

	ReadModel.Facilities.Add(MakeBridgeTestFacility(1, TEXT("ZoneA")));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(2, TEXT("ZoneB")));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(3, TEXT("ZoneC")));

	const TArray<FFF_DestinationCandidate> Candidates =
		FFF_ZoneSystemBridge::BuildCandidates(ReadModel, TEXT("ZoneA"), FVector::ZeroVector);

	TestEqual(TEXT("현재 Zone + 인접 Zone 의 시설 2개만 후보다"), Candidates.Num(), 2);
	TestTrue(TEXT("현재 Zone 시설은 후보다"), ContainsFacility(Candidates, 1));
	TestTrue(TEXT("인접 Zone 시설은 후보다"), ContainsFacility(Candidates, 2));
	TestFalse(TEXT("2홉 떨어진 Zone 시설은 후보가 아니다"), ContainsFacility(Candidates, 3));
	return true;
}

// 없는 ZoneId 를 넘기면 빈 배열이다. 조용히 넘어가면 "후보 없음"과 구분되지 않아 Warning 을 남긴다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_UnknownZoneReturnsEmpty, "FestaFiesta.NPC.ZoneSystemBridge.UnknownZoneReturnsEmpty", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_UnknownZoneReturnsEmpty::RunTest(const FString& Parameters)
{
	FFFZSReadModel ReadModel;
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneA"), {}));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(1, TEXT("ZoneA")));

	const TArray<FFF_DestinationCandidate> Candidates =
		FFF_ZoneSystemBridge::BuildCandidates(ReadModel, TEXT("ZoneDoesNotExist"), FVector::ZeroVector);

	TestEqual(TEXT("없는 Zone 이면 후보가 없다"), Candidates.Num(), 0);

	// 빈 ReadModel 로 불러도 크래시하지 않는다
	const FFFZSReadModel EmptyReadModel;
	TestEqual(TEXT("빈 ReadModel 도 안전하다"),
		FFF_ZoneSystemBridge::BuildCandidates(EmptyReadModel, TEXT("ZoneA"), FVector::ZeroVector).Num(), 0);
	return true;
}

// 수명주기(State·bNeedsRelocation)로만 자른다. 큐 상태로는 자르지 않는다 — 파일 상단 주석 참고.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_FiltersByLifecycleOnly, "FestaFiesta.NPC.ZoneSystemBridge.FiltersByLifecycleOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_FiltersByLifecycleOnly::RunTest(const FString& Parameters)
{
	FFFZSReadModel ReadModel;
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneA"), {}));

	// 정상 영업 중
	ReadModel.Facilities.Add(MakeBridgeTestFacility(1, TEXT("ZoneA")));

	// 아직 안 지어졌다 — 제외
	FFFZSFacilityReadModel Planned = MakeBridgeTestFacility(2, TEXT("ZoneA"));
	Planned.State = EFFZSFacilityState::Planned;
	ReadModel.Facilities.Add(Planned);

	// 닫혔다 — 제외
	FFFZSFacilityReadModel ClosedState = MakeBridgeTestFacility(3, TEXT("ZoneA"));
	ClosedState.State = EFFZSFacilityState::Closed;
	ReadModel.Facilities.Add(ClosedState);

	// 재배치 대기 — 곧 사라질 자리다. 제외
	FFFZSFacilityReadModel NeedsRelocation = MakeBridgeTestFacility(4, TEXT("ZoneA"));
	NeedsRelocation.bNeedsRelocation = true;
	ReadModel.Facilities.Add(NeedsRelocation);

	// 아래 3개는 큐 상태만 다르다 — **후보로 들어와야 한다.**
	// "정원이 찼다"를 후보에서 빼면 balking 이 아니라 시설이 존재하지 않는 것처럼 처리되고,
	// 같은 판정을 FFF_QueueWaitAdapter 도 하고 있어서 두 곳으로 갈린다.
	FFFZSFacilityReadModel FullQueue = MakeBridgeTestFacility(5, TEXT("ZoneA"));
	FullQueue.QueueState = EFFZSQueueState::Full;
	ReadModel.Facilities.Add(FullQueue);

	FFFZSFacilityReadModel Draining = MakeBridgeTestFacility(6, TEXT("ZoneA"));
	Draining.QueueState = EFFZSQueueState::Draining;
	ReadModel.Facilities.Add(Draining);

	FFFZSFacilityReadModel Blocked = MakeBridgeTestFacility(7, TEXT("ZoneA"));
	Blocked.QueueState = EFFZSQueueState::Blocked;
	ReadModel.Facilities.Add(Blocked);

	const TArray<FFF_DestinationCandidate> Candidates =
		FFF_ZoneSystemBridge::BuildCandidates(ReadModel, TEXT("ZoneA"), FVector::ZeroVector);

	TestEqual(TEXT("Active + 재배치 아님인 시설 4개가 후보다"), Candidates.Num(), 4);
	TestTrue(TEXT("정상 영업 시설은 후보다"), ContainsFacility(Candidates, 1));
	TestFalse(TEXT("Planned 는 제외된다 — 아직 안 지어졌다"), ContainsFacility(Candidates, 2));
	TestFalse(TEXT("Closed 는 제외된다"), ContainsFacility(Candidates, 3));
	TestFalse(TEXT("재배치 대기는 제외된다"), ContainsFacility(Candidates, 4));
	TestTrue(TEXT("Full 은 후보로 들어온다 — 이용 가능 판정은 Queue 가 한다"), ContainsFacility(Candidates, 5));
	TestTrue(TEXT("Draining 도 후보로 들어온다"), ContainsFacility(Candidates, 6));
	TestTrue(TEXT("Blocked 도 후보로 들어온다"), ContainsFacility(Candidates, 7));

	for (const FFF_DestinationCandidate& Candidate : Candidates)
	{
		TestTrue(TEXT("후보의 bIsOpen 은 true 다 — Queue 어댑터가 나중에 AND 로 닫는다"), Candidate.bIsOpen);
		TestEqual(TEXT("대기시간은 채우지 않는다(0) — 소유자는 FFF_QueueWaitAdapter 다"),
			Candidate.EstimatedWaitMinutes, 0.f);
	}
	return true;
}

// Info·Safety·Lighting 은 목적지 후보 자체가 아니다 — 방문객 서비스가 없는 시설이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_ExcludesInfoAndSafety, "FestaFiesta.NPC.ZoneSystemBridge.ExcludesInfoAndSafety", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_ExcludesInfoAndSafety::RunTest(const FString& Parameters)
{
	FFFZSReadModel ReadModel;
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneA"), {}));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(1, TEXT("ZoneA"),
		EFFZSFacilityCategory::Info, TEXT("Info_Kiosk_Test")));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(2, TEXT("ZoneA"),
		EFFZSFacilityCategory::Safety, TEXT("Safety_FirstAid_Test")));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(3, TEXT("ZoneA"),
		EFFZSFacilityCategory::Toilet, TEXT("Toilet_Standard_Test")));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(4, TEXT("ZoneA"),
		EFFZSFacilityCategory::Lighting, TEXT("Lighting_Streetlamp_FestivalStreetlamp")));

	const TArray<FFF_DestinationCandidate> Candidates =
		FFF_ZoneSystemBridge::BuildCandidates(ReadModel, TEXT("ZoneA"), FVector::ZeroVector);

	TestEqual(TEXT("Toilet 만 후보로 남는다"), Candidates.Num(), 1);
	TestTrue(TEXT("Toilet 은 후보다 — Need 로만 해소되지만 목적지는 될 수 있다"), ContainsFacility(Candidates, 3));
	TestFalse(TEXT("조명은 방문객 서비스 목적지가 아니다"), ContainsFacility(Candidates, 4));
	if (Candidates.Num() == 1)
	{
		TestTrue(TEXT("Toilet 후보는 Restroom 을 해소한다"), Candidates[0].NeedRelief.bRelievesRestroom);
		TestTrue(TEXT("Toilet 은 취향 카테고리가 없다(NAME_None)"), Candidates[0].Category.IsNone());
	}
	return true;
}

// 밀집도는 Zone 단위 값을 물려받는다(v0 근사). Capacity 는 0 이 되어선 안 된다 —
// 선택기가 CurrentOccupancy/Capacity 로 나눈다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_DensityComesFromZoneAndNeverDividesByZero, "FestaFiesta.NPC.ZoneSystemBridge.DensityComesFromZoneAndNeverDividesByZero", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_DensityComesFromZoneAndNeverDividesByZero::RunTest(const FString& Parameters)
{
	FFFZSReadModel ReadModel;
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneA"), { TEXT("ZoneB") }, /*Population=*/250, /*SoftCapacity=*/500));
	// SoftCapacity 0 = "아직 설정 안 됨". 그대로 흘리면 0 나눗셈이다.
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneB"), { TEXT("ZoneA") }, /*Population=*/40, /*SoftCapacity=*/0));

	ReadModel.Facilities.Add(MakeBridgeTestFacility(1, TEXT("ZoneA")));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(2, TEXT("ZoneB")));

	const TArray<FFF_DestinationCandidate> Candidates =
		FFF_ZoneSystemBridge::BuildCandidates(ReadModel, TEXT("ZoneA"), FVector::ZeroVector);

	TestEqual(TEXT("후보 2개"), Candidates.Num(), 2);
	for (const FFF_DestinationCandidate& Candidate : Candidates)
	{
		TestTrue(TEXT("Capacity 는 항상 1 이상이다 — 선택기가 이 값으로 나눈다"), Candidate.Capacity >= 1.f);
		const float OccupancyRatio = Candidate.CurrentOccupancy / Candidate.Capacity;
		TestTrue(TEXT("밀집도 비율이 유한하다"), FMath::IsFinite(OccupancyRatio));
	}

	const FName ZoneAFacility = FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(MakeBridgeTestFacilityId(1));
	const FFF_DestinationCandidate* FromZoneA = Candidates.FindByPredicate(
		[ZoneAFacility](const FFF_DestinationCandidate& C) { return C.DestinationId == ZoneAFacility; });
	if (FromZoneA)
	{
		TestEqual(TEXT("점유는 Zone 의 ReportedPopulation 이다"), FromZoneA->CurrentOccupancy, 250.f);
		TestEqual(TEXT("수용은 Zone 의 SoftCapacity 다"), FromZoneA->Capacity, 500.f);
	}
	return true;
}

// 자산이 없는 테스트 환경에서도 BuildCandidates 가 inf/NaN 을 흘리지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_CandidateFieldsAreFinite, "FestaFiesta.NPC.ZoneSystemBridge.CandidateFieldsAreFinite", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_CandidateFieldsAreFinite::RunTest(const FString& Parameters)
{
	FFFZSReadModel ReadModel;
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneA"), {}, /*Population=*/10, /*SoftCapacity=*/0));
	FFFZSFacilityReadModel Facility = MakeBridgeTestFacility(1, TEXT("ZoneA"));
	Facility.WaitingCount = 120;
	ReadModel.Facilities.Add(Facility);

	const TArray<FFF_DestinationCandidate> Candidates =
		FFF_ZoneSystemBridge::BuildCandidates(ReadModel, TEXT("ZoneA"), FVector::ZeroVector);

	TestEqual(TEXT("후보 1개"), Candidates.Num(), 1);
	if (Candidates.Num() == 1)
	{
		const FFF_DestinationCandidate& Candidate = Candidates[0];
		TestTrue(TEXT("DistanceMeters 가 유한하다"), FMath::IsFinite(Candidate.DistanceMeters));
		TestTrue(TEXT("Capacity 가 유한하고 0 이 아니다"),
			FMath::IsFinite(Candidate.Capacity) && Candidate.Capacity > 0.f);
		// SoftCapacity 0 인 Zone 이라도 1로 clamp 되므로 나눗셈이 안전하다
		TestTrue(TEXT("밀집도 비율이 유한하다"),
			FMath::IsFinite(Candidate.CurrentOccupancy / Candidate.Capacity));
		// WaitingCount 가 120 이어도 대기시간은 채우지 않는다 — Queue 소유
		TestEqual(TEXT("대기시간은 0으로 남는다"), Candidate.EstimatedWaitMinutes, 0.f);
	}
	return true;
}

// Read Model의 시설 대표 위치와 Party 위치 차이를 cm → m 로 환산한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_DistanceConvertsCentimetersToMeters, "FestaFiesta.NPC.ZoneSystemBridge.DistanceConvertsCentimetersToMeters", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_DistanceConvertsCentimetersToMeters::RunTest(const FString& Parameters)
{
	FFFZSReadModel ReadModel;
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneA"), {}));
	FFFZSFacilityReadModel Facility = MakeBridgeTestFacility(1, TEXT("ZoneA"));
	Facility.WorldLocation = FVector(15000.f, 0.f, 0.f); // 150m 지점(cm 단위)
	ReadModel.Facilities.Add(Facility);

	const TArray<FFF_DestinationCandidate> Candidates = FFF_ZoneSystemBridge::BuildCandidates(
		ReadModel, TEXT("ZoneA"), FVector(5000.f, 0.f, 0.f));

	const FName Facility1 = FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(MakeBridgeTestFacilityId(1));
	const FFF_DestinationCandidate* Known = Candidates.FindByPredicate(
		[Facility1](const FFF_DestinationCandidate& C) { return C.DestinationId == Facility1; });
	if (Known)
	{
		// 10000cm 차이 = 100m
		TestEqual(TEXT("cm 차이를 m 로 환산한다"), Known->DistanceMeters, 100.f, 0.01f);
	}
	else
	{
		AddError(TEXT("위치를 준 시설이 후보에 없다"));
	}
	return true;
}

// 후보 상한이 실제로 걸리고, 현재 Zone → 가까운 순 → ID 순으로 결정적으로 잘린다.
// 마지막 ID 단계가 없으면 같은 스냅샷인데 호출마다 다른 후보가 잘려 재현성이 깨진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_TruncationPrefersCurrentZoneAndIsDeterministic, "FestaFiesta.NPC.ZoneSystemBridge.TruncationPrefersCurrentZoneAndIsDeterministic", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_TruncationPrefersCurrentZoneAndIsDeterministic::RunTest(const FString& Parameters)
{
	FFFZSReadModel ReadModel;
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneA"), { TEXT("ZoneB") }));
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneB"), { TEXT("ZoneA") }));

	// 현재 Zone 에 2개, 인접 Zone 에 3개 → 상한 3이면 현재 Zone 2개가 먼저 남아야 한다
	ReadModel.Facilities.Add(MakeBridgeTestFacility(1, TEXT("ZoneA")));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(2, TEXT("ZoneA")));
	FFFZSFacilityReadModel FarNeighbor = MakeBridgeTestFacility(3, TEXT("ZoneB"));
	FarNeighbor.WorldLocation = FVector(9000.f, 0.f, 0.f);
	ReadModel.Facilities.Add(FarNeighbor);
	FFFZSFacilityReadModel NearNeighbor = MakeBridgeTestFacility(4, TEXT("ZoneB"));
	NearNeighbor.WorldLocation = FVector(3000.f, 0.f, 0.f);
	ReadModel.Facilities.Add(NearNeighbor);
	FFFZSFacilityReadModel MiddleNeighbor = MakeBridgeTestFacility(5, TEXT("ZoneB"));
	MiddleNeighbor.WorldLocation = FVector(6000.f, 0.f, 0.f);
	ReadModel.Facilities.Add(MiddleNeighbor);

	const TArray<FFF_DestinationCandidate> Truncated = FFF_ZoneSystemBridge::BuildCandidates(
		ReadModel, TEXT("ZoneA"), FVector::ZeroVector, /*MaxCandidates=*/3);

	TestEqual(TEXT("상한 3이 걸린다"), Truncated.Num(), 3);
	TestTrue(TEXT("현재 Zone 시설 1은 살아남는다"), ContainsFacility(Truncated, 1));
	TestTrue(TEXT("현재 Zone 시설 2도 살아남는다"), ContainsFacility(Truncated, 2));
	TestTrue(TEXT("인접 Zone에서는 가장 가까운 시설이 살아남는다"), ContainsFacility(Truncated, 4));
	TestFalse(TEXT("더 먼 인접 Zone 시설은 상한으로 잘린다"), ContainsFacility(Truncated, 3));
	TestFalse(TEXT("중간 거리 인접 Zone 시설도 상한으로 잘린다"), ContainsFacility(Truncated, 5));

	// 같은 입력이면 같은 결과 — 잘린 뒤 남는 집합이 호출마다 달라지면 재현이 불가능해진다
	const TArray<FFF_DestinationCandidate> Again = FFF_ZoneSystemBridge::BuildCandidates(
		ReadModel, TEXT("ZoneA"), FVector::ZeroVector, /*MaxCandidates=*/3);
	TestEqual(TEXT("두 번 불러도 개수가 같다"), Again.Num(), Truncated.Num());
	for (int32 Index = 0; Index < FMath::Min(Again.Num(), Truncated.Num()); ++Index)
	{
		TestEqual(TEXT("두 번 불러도 순서·구성이 같다"),
			Again[Index].DestinationId, Truncated[Index].DestinationId);
	}

	// 상한 0 이하 = 상한 없음
	const TArray<FFF_DestinationCandidate> Unlimited = FFF_ZoneSystemBridge::BuildCandidates(
		ReadModel, TEXT("ZoneA"), FVector::ZeroVector, /*MaxCandidates=*/0);
	TestEqual(TEXT("MaxCandidates 0 은 상한 없음이다"), Unlimited.Num(), 5);
	return true;
}

// 후보의 취향 카테고리·Need 해소가 Bridge 매핑 함수 결과와 일치한다(조립이 빠뜨리지 않았는지).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_CandidateCarriesPreferenceAndNeedRelief, "FestaFiesta.NPC.ZoneSystemBridge.CandidateCarriesPreferenceAndNeedRelief", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_CandidateCarriesPreferenceAndNeedRelief::RunTest(const FString& Parameters)
{
	FFFZSReadModel ReadModel;
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneA"), {}));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(1, TEXT("ZoneA"),
		EFFZSFacilityCategory::Booth, TEXT("Booth_FoodStall_DadaepoTruck")));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(2, TEXT("ZoneA"),
		EFFZSFacilityCategory::Rest, TEXT("Rest_Cooling_OasisZone")));

	const TArray<FFF_DestinationCandidate> Candidates =
		FFF_ZoneSystemBridge::BuildCandidates(ReadModel, TEXT("ZoneA"), FVector::ZeroVector);
	TestEqual(TEXT("후보 2개"), Candidates.Num(), 2);

	const FName FoodStallId = FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(MakeBridgeTestFacilityId(1));
	const FFF_DestinationCandidate* FoodStall = Candidates.FindByPredicate(
		[FoodStallId](const FFF_DestinationCandidate& C) { return C.DestinationId == FoodStallId; });
	if (FoodStall)
	{
		TestEqual(TEXT("Booth_FoodStall 은 food_stall 취향이다"), FoodStall->Category, FName(TEXT("food_stall")));
		TestTrue(TEXT("음식 부스는 Hunger 를 해소한다"), FoodStall->NeedRelief.bRelievesHunger);
		TestTrue(TEXT("음식 부스는 Thirst 도 해소한다"), FoodStall->NeedRelief.bRelievesThirst);
		TestEqual(TEXT("attraction_norm 은 아직 출처가 없어 0 이다"), FoodStall->BaseAttraction, 0.f);
	}
	else
	{
		AddError(TEXT("음식 부스 후보를 찾지 못했다"));
	}

	const FName CoolingId = FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(MakeBridgeTestFacilityId(2));
	const FFF_DestinationCandidate* Cooling = Candidates.FindByPredicate(
		[CoolingId](const FFF_DestinationCandidate& C) { return C.DestinationId == CoolingId; });
	if (Cooling)
	{
		TestEqual(TEXT("Rest 는 소분류와 무관하게 rest_area 취향이다"), Cooling->Category, FName(TEXT("rest_area")));
		TestTrue(TEXT("Rest 는 Fatigue 를 해소한다"), Cooling->NeedRelief.bRelievesFatigue);
		TestTrue(TEXT("Rest_Cooling 만 Thirst 를 해소한다(2026-08-06 결정)"), Cooling->NeedRelief.bRelievesThirst);
	}
	else
	{
		AddError(TEXT("무더위 쉼터 후보를 찾지 못했다"));
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 매핑 함수 회귀 테스트 (2026-08-10 추가)
//
// 이 4개 함수는 로직이 여러 번 바뀌었는데 테스트가 하나도 없었다. 특히 GetNeedReliefFlags 는
// "Rest 는 Thirst 를 해소한다" → "Rest_Cooling 만 해소한다"로 좁혀진 이력이 있어서, 되돌아가면
// 좌석만 있는 벤치가 갈증을 풀어주는 상태가 조용히 부활한다.
// ─────────────────────────────────────────────────────────────────────────────

// 8개 카테고리 전수. Booth 만 소분류가 취향을 갈라내고, 나머지는 대분류 1:1 또는 NAME_None 이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_MapsAllEightCategories, "FestaFiesta.NPC.ZoneSystemBridge.MapsAllEightCategories", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_MapsAllEightCategories::RunTest(const FString& Parameters)
{
	// Booth — 소분류 5종이 각각 다른 취향 카테고리로 간다
	TestEqual(TEXT("Booth_FoodStall → food_stall"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::Booth, TEXT("Booth_FoodStall_Dadaepo")),
		FName(TEXT("food_stall")));
	TestEqual(TEXT("Booth_MarketGoods → market_goods"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::Booth, TEXT("Booth_MarketGoods_Flea")),
		FName(TEXT("market_goods")));
	TestEqual(TEXT("Booth_CraftExperience → craft_experience"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::Booth, TEXT("Booth_CraftExperience_Pottery")),
		FName(TEXT("craft_experience")));
	TestEqual(TEXT("Booth_ActivityGame → activity_game"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::Booth, TEXT("Booth_ActivityGame_WaterFight")),
		FName(TEXT("activity_game")));
	TestEqual(TEXT("Booth_Nightlife → nightlife_entertainment"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::Booth, TEXT("Booth_Nightlife_SunsetClub")),
		FName(TEXT("nightlife_entertainment")));

	// Rest — 소분류 4종 전부 rest_area. 취향 단위에서는 구분하지 않는다
	for (const TCHAR* Minor : { TEXT("Seating"), TEXT("Shade"), TEXT("Picnic"), TEXT("Cooling") })
	{
		const FName Id = FName(*FString::Printf(TEXT("Rest_%s_Test"), Minor));
		TestEqual(*FString::Printf(TEXT("Rest_%s → rest_area"), Minor),
			FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::Rest, Id),
			FName(TEXT("rest_area")));
	}

	// Performance — 4종 전부 performance_stage
	// TODO(합의): ExperienceStage(참여형 공연)는 activity_game 이 더 맞을 수 있다(사용자 결정 대기).
	// 그 결정이 나면 이 루프가 깨지므로 여기가 변경 지점이 된다.
	for (const TCHAR* Minor : { TEXT("MainStage"), TEXT("SubStage"), TEXT("Busking"), TEXT("ExperienceStage") })
	{
		const FName Id = FName(*FString::Printf(TEXT("Performance_%s_Test"), Minor));
		TestEqual(*FString::Printf(TEXT("Performance_%s → performance_stage"), Minor),
			FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::Performance, Id),
			FName(TEXT("performance_stage")));
	}

	// PhotoSpot — 4종 전부 ambience_photo_zone
	for (const TCHAR* Minor : { TEXT("Landmark"), TEXT("Theme"), TEXT("Scenic"), TEXT("Interactive") })
	{
		const FName Id = FName(*FString::Printf(TEXT("PhotoSpot_%s_Test"), Minor));
		TestEqual(*FString::Printf(TEXT("PhotoSpot_%s → ambience_photo_zone"), Minor),
			FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::PhotoSpot, Id),
			FName(TEXT("ambience_photo_zone")));
	}

	// Toilet·Info·Safety — 취향 매칭 대상이 아니다. Toilet 은 Need 로만 해소된다
	TestTrue(TEXT("Toilet 은 취향 카테고리가 없다"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::Toilet, TEXT("Toilet_Standard_Test")).IsNone());
	TestTrue(TEXT("Info 는 취향 카테고리가 없다"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::Info, TEXT("Info_GeneralInfo_Test")).IsNone());
	TestTrue(TEXT("Safety 는 취향 카테고리가 없다"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::Safety, TEXT("Safety_FirstAid_Test")).IsNone());
	for (const TCHAR* Minor : { TEXT("Decorative"), TEXT("Pendant"), TEXT("Streetlamp") })
	{
		const FName Id(*FString::Printf(TEXT("Lighting_%s_Test"), Minor));
		TestTrue(TEXT("조명은 취향 서비스 카테고리가 없다"),
			FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::Lighting, Id).IsNone());
	}
	return true;
}

// 네이밍 컨벤션을 어기면 크래시가 아니라 "취향 없음"으로 동작한다.
// 이게 깨지면 부스 담당이 오타를 냈을 때 게임이 죽는다 — 조용히 넘어가는 것이 의도된 동작이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_NamingViolationDegradesToNoPreference, "FestaFiesta.NPC.ZoneSystemBridge.NamingViolationDegradesToNoPreference", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_NamingViolationDegradesToNoPreference::RunTest(const FString& Parameters)
{
	// 소분류가 규칙 표에 없다
	TestTrue(TEXT("모르는 Booth 소분류는 취향 없음이다"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(
			EFFZSFacilityCategory::Booth, TEXT("Booth_UnknownMinor_Violation")).IsNone());

	// 형태 자체가 다르다 (구분자 없음)
	TestTrue(TEXT("구분자가 없으면 취향 없음이다"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(
			EFFZSFacilityCategory::Booth, TEXT("BoothFoodStallViolation")).IsNone());

	// 대분류가 카테고리와 안 맞는다
	TestTrue(TEXT("대분류가 카테고리와 다르면 취향 없음이다"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(
			EFFZSFacilityCategory::Booth, TEXT("Rest_FoodStall_Violation")).IsNone());

	// NAME_None 은 위반이 아니라 "정의가 아직 안 붙었다"는 정상 상태다 — 경고도 안 낸다
	TestTrue(TEXT("NAME_None 은 취향 없음이다(위반 아님)"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(EFFZSFacilityCategory::Booth, NAME_None).IsNone());

	// Rest 위반은 Thirst 를 해소하지 않는다 — 소분류를 못 읽으면 안전한 쪽으로 뺀다는 규칙.
	// 이게 깨지면 좌석만 있는 벤치가 갈증을 풀어주게 된다.
	const FFF_NeedReliefFlags ViolatingRest = FFF_ZoneSystemBridge::GetNeedReliefFlags(
		EFFZSFacilityCategory::Rest, TEXT("Rest_Fountain_Violation"));
	TestTrue(TEXT("Rest 위반도 피로는 해소한다(대분류만으로 판정)"), ViolatingRest.bRelievesFatigue);
	TestFalse(TEXT("Rest 위반은 갈증을 해소하지 않는다 — 안전한 쪽으로 뺀다"), ViolatingRest.bRelievesThirst);

	// Booth 위반은 아무 Need 도 해소하지 않는다 (소분류를 못 읽으면 식음료인지 알 수 없다)
	const FFF_NeedReliefFlags ViolatingBooth = FFF_ZoneSystemBridge::GetNeedReliefFlags(
		EFFZSFacilityCategory::Booth, TEXT("Booth_UnknownMinor_Violation"));
	TestFalse(TEXT("Booth 위반은 아무것도 해소하지 않는다"), ViolatingBooth.RelievesAnything());
	return true;
}

// Need 해소 매핑 전수. FF_NeedState.h 상단 매핑 표를 코드로 옮긴 것이 맞는지 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_NeedReliefMatchesMappingTable, "FestaFiesta.NPC.ZoneSystemBridge.NeedReliefMatchesMappingTable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_NeedReliefMatchesMappingTable::RunTest(const FString& Parameters)
{
	// Toilet — 소분류 4종 전부 Restroom 만 해소한다
	for (const TCHAR* Minor : { TEXT("Standard"), TEXT("Accessible"), TEXT("Family"), TEXT("Temporary") })
	{
		const FName Id = FName(*FString::Printf(TEXT("Toilet_%s_Test"), Minor));
		const FFF_NeedReliefFlags Flags = FFF_ZoneSystemBridge::GetNeedReliefFlags(EFFZSFacilityCategory::Toilet, Id);
		TestTrue(*FString::Printf(TEXT("Toilet_%s 는 Restroom 을 해소한다"), Minor), Flags.bRelievesRestroom);
		TestFalse(*FString::Printf(TEXT("Toilet_%s 는 배고픔을 해소하지 않는다"), Minor), Flags.bRelievesHunger);
		TestFalse(*FString::Printf(TEXT("Toilet_%s 는 피로를 해소하지 않는다"), Minor), Flags.bRelievesFatigue);
	}

	// Rest — 4종 전부 Fatigue. Thirst 는 Cooling 만 (2026-08-06 결정)
	for (const TCHAR* Minor : { TEXT("Seating"), TEXT("Shade"), TEXT("Picnic") })
	{
		const FName Id = FName(*FString::Printf(TEXT("Rest_%s_Test"), Minor));
		const FFF_NeedReliefFlags Flags = FFF_ZoneSystemBridge::GetNeedReliefFlags(EFFZSFacilityCategory::Rest, Id);
		TestTrue(*FString::Printf(TEXT("Rest_%s 는 피로를 해소한다"), Minor), Flags.bRelievesFatigue);
		TestFalse(*FString::Printf(TEXT("Rest_%s 는 갈증을 해소하지 않는다 — Cooling 만 해소한다"), Minor),
			Flags.bRelievesThirst);
	}
	const FFF_NeedReliefFlags Cooling = FFF_ZoneSystemBridge::GetNeedReliefFlags(
		EFFZSFacilityCategory::Rest, TEXT("Rest_Cooling_OasisZone"));
	TestTrue(TEXT("Rest_Cooling 은 피로를 해소한다"), Cooling.bRelievesFatigue);
	TestTrue(TEXT("Rest_Cooling 만 갈증을 해소한다 — 무더위 쉼터의 음수대"), Cooling.bRelievesThirst);

	// Booth — 소분류에 따라 갈린다
	const FFF_NeedReliefFlags FoodStall = FFF_ZoneSystemBridge::GetNeedReliefFlags(
		EFFZSFacilityCategory::Booth, TEXT("Booth_FoodStall_Dadaepo"));
	TestTrue(TEXT("FoodStall 은 배고픔을 해소한다"), FoodStall.bRelievesHunger);
	TestTrue(TEXT("FoodStall 은 갈증도 해소한다"), FoodStall.bRelievesThirst);
	TestFalse(TEXT("FoodStall 은 자극을 해소하지 않는다"), FoodStall.bRelievesStimulation);

	const FFF_NeedReliefFlags Nightlife = FFF_ZoneSystemBridge::GetNeedReliefFlags(
		EFFZSFacilityCategory::Booth, TEXT("Booth_Nightlife_SunsetClub"));
	TestFalse(TEXT("Nightlife 는 배고픔을 해소하지 않는다"), Nightlife.bRelievesHunger);
	TestTrue(TEXT("Nightlife 는 갈증을 해소한다"), Nightlife.bRelievesThirst);
	TestTrue(TEXT("Nightlife 는 자극을 해소한다 — EDM 비치클럽·하우스DJ 라운지"), Nightlife.bRelievesStimulation);

	for (const TCHAR* Minor : { TEXT("MarketGoods"), TEXT("CraftExperience"), TEXT("ActivityGame") })
	{
		const FName Id = FName(*FString::Printf(TEXT("Booth_%s_Test"), Minor));
		const FFF_NeedReliefFlags Flags = FFF_ZoneSystemBridge::GetNeedReliefFlags(EFFZSFacilityCategory::Booth, Id);
		TestFalse(*FString::Printf(TEXT("Booth_%s 는 아무 Need 도 해소하지 않는다"), Minor), Flags.RelievesAnything());
	}

	// Performance·PhotoSpot — Stimulation 만
	const FFF_NeedReliefFlags Stage = FFF_ZoneSystemBridge::GetNeedReliefFlags(
		EFFZSFacilityCategory::Performance, TEXT("Performance_MainStage_Test"));
	TestTrue(TEXT("Performance 는 자극을 해소한다"), Stage.bRelievesStimulation);
	TestFalse(TEXT("Performance 는 피로를 해소하지 않는다"), Stage.bRelievesFatigue);

	const FFF_NeedReliefFlags Photo = FFF_ZoneSystemBridge::GetNeedReliefFlags(
		EFFZSFacilityCategory::PhotoSpot, TEXT("PhotoSpot_Landmark_Test"));
	TestTrue(TEXT("PhotoSpot 은 자극을 해소한다"), Photo.bRelievesStimulation);
	TestFalse(TEXT("PhotoSpot 은 배고픔을 해소하지 않는다"), Photo.bRelievesHunger);

	// Info·Safety — FF_NeedState.h 설계상 Need 대상이 아니다.
	// Info 는 "몰라서 헤맨다"는 욕구의 근거가 없어서, Safety 는 density_cost 항과 이중 반영이라 뺐다.
	TestFalse(TEXT("Info 는 아무 Need 도 해소하지 않는다"),
		FFF_ZoneSystemBridge::GetNeedReliefFlags(
			EFFZSFacilityCategory::Info, TEXT("Info_GeneralInfo_Test")).RelievesAnything());
	TestFalse(TEXT("Safety 는 아무 Need 도 해소하지 않는다"),
		FFF_ZoneSystemBridge::GetNeedReliefFlags(
			EFFZSFacilityCategory::Safety, TEXT("Safety_FirstAid_Test")).RelievesAnything());
	return true;
}

// GetMaxUrgency 는 합이 아니라 최대값이다. 합을 쓰면 3가지를 해소하는 시설이 각 성분이 아무리
// 안 급해도 1가지만 해소하는 시설을 항상 이긴다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_UrgencyIsMaxNotSum, "FestaFiesta.NPC.ZoneSystemBridge.UrgencyIsMaxNotSum", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_UrgencyIsMaxNotSum::RunTest(const FString& Parameters)
{
	FFF_NeedState Needs;
	Needs.Restroom = 0.9f;   // 급하다
	Needs.Hunger = 0.3f;
	Needs.Thirst = 0.3f;
	Needs.Stimulation = 0.3f;

	const FFF_NeedPriorityWeights Priority;  // Restroom 1.0 / Hunger 0.85 / Thirst 0.75 / Stimulation 0.5

	// 화장실 하나만 해소 → 0.9 * 1.0 = 0.9
	const FFF_NeedReliefFlags Toilet = FFF_ZoneSystemBridge::GetNeedReliefFlags(
		EFFZSFacilityCategory::Toilet, TEXT("Toilet_Standard_Test"));
	const float ToiletUrgency = Toilet.GetMaxUrgency(Needs, Priority);
	TestEqual(TEXT("화장실 급함은 0.9 * 1.0"), ToiletUrgency, 0.9f, 0.001f);

	// 3가지를 해소하지만 각 성분이 안 급한 시설 → 최대 0.3 * 0.75 = 0.225
	const FFF_NeedReliefFlags Nightlife = FFF_ZoneSystemBridge::GetNeedReliefFlags(
		EFFZSFacilityCategory::Booth, TEXT("Booth_Nightlife_SunsetClub"));
	const float NightlifeUrgency = Nightlife.GetMaxUrgency(Needs, Priority);
	TestEqual(TEXT("여러 개를 해소해도 가장 급한 성분값이다(합 아님)"), NightlifeUrgency, 0.225f, 0.001f);
	TestTrue(TEXT("1가지만 해소하는 화장실이 3가지 해소 시설을 이긴다"), ToiletUrgency > NightlifeUrgency);

	// 아무것도 해소하지 않는 시설은 0 — need 항이 죽는다
	const FFF_NeedReliefFlags Nothing;
	TestEqual(TEXT("해소하는 것이 없으면 급함은 0"), Nothing.GetMaxUrgency(Needs, Priority), 0.f);
	return true;
}

// FGuid ↔ FName 변환은 왕복해야 한다. 이 대응이 깨지면 목적지를 골라도 그 시설을 다시 찾지 못한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_DestinationIdRoundTrips, "FestaFiesta.NPC.ZoneSystemBridge.DestinationIdRoundTrips", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_DestinationIdRoundTrips::RunTest(const FString& Parameters)
{
	const FGuid Original(0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00);
	const FName DestinationId = FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(Original);

	FGuid Parsed;
	TestTrue(TEXT("변환한 DestinationId 는 다시 파싱된다"),
		FFF_ZoneSystemBridge::TryParseDestinationId(DestinationId, Parsed));
	TestTrue(TEXT("왕복 후 같은 FGuid 다"), Parsed == Original);

	// 서로 다른 시설은 서로 다른 DestinationId 를 갖는다
	const FGuid Other(0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF01);
	TestTrue(TEXT("다른 FGuid 는 다른 DestinationId 다"),
		FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(Other) != DestinationId);

	// 시설 인스턴스가 아닌 값을 넘기면 false — 호출부가 확인해야 한다
	FGuid Garbage;
	TestFalse(TEXT("GUID 형식이 아니면 false 다"),
		FFF_ZoneSystemBridge::TryParseDestinationId(TEXT("NotAGuid"), Garbage));
	TestFalse(TEXT("NAME_None 도 false 다"),
		FFF_ZoneSystemBridge::TryParseDestinationId(NAME_None, Garbage));
	return true;
}

// GetServiceRatePerMinute 는 Asset Manager 를 타므로 자산이 없는 테스트 환경에서 항상 0 이다.
// 그래서 "환산이 맞는가"는 여기서 검증할 수 없고, "0(모름)을 안전하게 돌려주는가"만 확인한다.
// 환산식(ServiceCapacity / 분 단위 서비스시간)을 검증하려면 UFFZSFacilityDefinition 을 로드하는
// 테스트가 필요하다 — 자산 의존 테스트라 별도 작업이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_ServiceRateReturnsZeroWhenUnknown, "FestaFiesta.NPC.ZoneSystemBridge.ServiceRateReturnsZeroWhenUnknown", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_ServiceRateReturnsZeroWhenUnknown::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("NAME_None 은 0(모름)이다"),
		FFF_ZoneSystemBridge::GetServiceRatePerMinute(NAME_None), 0.f);
	TestEqual(TEXT("로드되지 않은 자산도 0(모름)이다 — 강제 로드하지 않는다"),
		FFF_ZoneSystemBridge::GetServiceRatePerMinute(TEXT("Booth_FoodStall_DoesNotExist")), 0.f);

	// !!! 이 함수는 현재 호출부가 없다 !!! 대기시간은 FFF_QueueWaitAdapter 가 소유한다.
	// 그래도 테스트를 남기는 이유: 초 → 분 환산 계약이 이 파일에 있고, 나중에 대기시간을 다시
	// 다루게 될 때 "0 = 모름"이라는 반환 규약이 지켜지고 있는지 확인할 지점이 필요하다.
	return true;
}

// 단위 환산은 이 파일에서만 한다 — cm/초가 m/분 필드로 새면 wait_cost·distance_cost 가 60배 틀린다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_UnitConversions, "FestaFiesta.NPC.ZoneSystemBridge.UnitConversions", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_UnitConversions::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("100cm = 1m"), FFF_ZoneSystemBridge::CentimetersToMeters(100.f), 1.f, 0.0001f);
	TestEqual(TEXT("40000cm = 400m (distance_ref_meters 기본값)"),
		FFF_ZoneSystemBridge::CentimetersToMeters(40000.f), 400.f, 0.001f);
	TestEqual(TEXT("60초 = 1분"), FFF_ZoneSystemBridge::SecondsToMinutes(60.f), 1.f, 0.0001f);
	TestEqual(TEXT("1800초 = 30분 (wait_ref_minutes 기본값)"),
		FFF_ZoneSystemBridge::SecondsToMinutes(1800.f), 30.f, 0.001f);
	TestEqual(TEXT("0 은 0 이다"), FFF_ZoneSystemBridge::CentimetersToMeters(0.f), 0.f);
	return true;
}

// 공연 시간이 아니면 메인 무대만 후보에서 빠진다.
//
// 소규모 공연 세 종류는 일반 지역 시설이다. 메인 공연 시간 게이트로 닫지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_ProgramScheduleHidesOnlyMainStage, "FestaFiesta.NPC.ZoneSystemBridge.ProgramScheduleHidesOnlyMainStage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_ProgramScheduleHidesOnlyMainStage::RunTest(const FString& Parameters)
{
	FFFZSReadModel ReadModel;
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneA"), {}));

	ReadModel.Facilities.Add(MakeBridgeTestFacility(1, TEXT("ZoneA"),
		EFFZSFacilityCategory::Performance, TEXT("Performance_MainStage_SunsetMainStage")));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(2, TEXT("ZoneA"),
		EFFZSFacilityCategory::Performance, TEXT("Performance_Busking_StreetStage")));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(3, TEXT("ZoneA"),
		EFFZSFacilityCategory::Performance, TEXT("Performance_ExperienceStage_DanceBattle")));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(4, TEXT("ZoneA"),
		EFFZSFacilityCategory::Performance, TEXT("Performance_SubStage_LocalArtist")));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(5, TEXT("ZoneA")));

	const TArray<FFF_DestinationCandidate> Closed = FFF_ZoneSystemBridge::BuildCandidates(
		ReadModel, TEXT("ZoneA"), FVector::ZeroVector,
		FFF_ZoneSystemBridge::DefaultMaxCandidates, nullptr, /*bMainStageOpen=*/false);

	TestFalse(TEXT("공연 시간이 아니면 메인 무대가 빠진다"), ContainsFacility(Closed, 1));
	TestTrue(TEXT("버스킹은 상시라 남는다"), ContainsFacility(Closed, 2));
	TestTrue(TEXT("DanceBattle은 메인 공연 시간과 무관하게 남는다"), ContainsFacility(Closed, 3));
	TestTrue(TEXT("서브 무대는 게이팅 대상이 아니다"), ContainsFacility(Closed, 4));
	TestTrue(TEXT("무관한 카테고리는 영향 없다"), ContainsFacility(Closed, 5));

	const TArray<FFF_DestinationCandidate> Open = FFF_ZoneSystemBridge::BuildCandidates(
		ReadModel, TEXT("ZoneA"), FVector::ZeroVector,
		FFF_ZoneSystemBridge::DefaultMaxCandidates, nullptr, /*bMainStageOpen=*/true);

	TestTrue(TEXT("공연 시간이면 메인 무대가 돌아온다"), ContainsFacility(Open, 1));
	TestEqual(TEXT("열렸을 때는 아무것도 안 빠진다"), Open.Num(), 5);

	return true;
}

// 인자를 생략하면 게이팅이 없다. Stage 가 없는 월드(크라우드 단독 테스트 맵)와
// 이 함수를 부르는 기존 호출부가 전부 이 기본값에 기대고 있다 —
// 기본값을 false 로 뒤집으면 무대가 아무 데서도 안 보인다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneSystemBridge_MainStageGatingDefaultsToOpen, "FestaFiesta.NPC.ZoneSystemBridge.MainStageGatingDefaultsToOpen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ZoneSystemBridge_MainStageGatingDefaultsToOpen::RunTest(const FString& Parameters)
{
	FFFZSReadModel ReadModel;
	ReadModel.Zones.Add(MakeBridgeTestZone(TEXT("ZoneA"), {}));
	ReadModel.Facilities.Add(MakeBridgeTestFacility(1, TEXT("ZoneA"),
		EFFZSFacilityCategory::Performance, TEXT("Performance_MainStage_SunsetMainStage")));

	const TArray<FFF_DestinationCandidate> Candidates =
		FFF_ZoneSystemBridge::BuildCandidates(ReadModel, TEXT("ZoneA"), FVector::ZeroVector);

	TestTrue(TEXT("기본값은 게이팅 없음이다"), ContainsFacility(Candidates, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_GlobalMainStageCandidates,
	"FestaFiesta.NPC.ZoneSystemBridge.GlobalMainStagesSurviveLocalCap",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_GlobalMainStageCandidates::RunTest(const FString& Parameters)
{
	FFFZSReadModel Model;
	Model.Zones.Add(MakeBridgeTestZone(TEXT("A"), {TEXT("B")}));
	Model.Zones.Add(MakeBridgeTestZone(TEXT("B"), {TEXT("A"), TEXT("C")}));
	Model.Zones.Add(MakeBridgeTestZone(TEXT("C"), {TEXT("B")}, 80, 100));
	for (int32 I = 1; I <= 14; ++I)
	{
		Model.Facilities.Add(MakeBridgeTestFacility(I, TEXT("A")));
	}
	Model.Facilities.Add(MakeBridgeTestFacility(20, TEXT("C"), EFFZSFacilityCategory::Performance, TEXT("Performance_MainStage_SunsetMainStage")));
	Model.Facilities.Last().WorldLocation = FVector(50000, 0, 0);
	Model.Facilities.Add(MakeBridgeTestFacility(21, TEXT("A"), EFFZSFacilityCategory::Performance, TEXT("Performance_MainStage_SunsetMainStage")));
	Model.Facilities.Add(MakeBridgeTestFacility(22, TEXT("C"), EFFZSFacilityCategory::Performance, TEXT("Performance_Busking_Far")));
	Model.Facilities.Add(MakeBridgeTestFacility(23, TEXT("C")));
	const auto Candidates = FFF_ZoneSystemBridge::BuildCandidates(Model, TEXT("A"), FVector::ZeroVector, 12, nullptr, true, true);
	TestEqual(TEXT("Twelve local choices plus both main stages"), Candidates.Num(), 14);
	TestTrue(TEXT("Distant main stage survives cap"), ContainsFacility(Candidates, 20));
	TestTrue(TEXT("Local main stage gets one protected entry"), ContainsFacility(Candidates, 21));
	TestFalse(TEXT("Distant busking stays local"), ContainsFacility(Candidates, 22));
	TestFalse(TEXT("Distant ordinary booth stays local"), ContainsFacility(Candidates, 23));
	TSet<FName> Seen;
	for (const auto& Candidate : Candidates)
	{
		TestFalse(TEXT("No duplicate destination"), Seen.Contains(Candidate.DestinationId));
		Seen.Add(Candidate.DestinationId);
		if (Candidate.DestinationId == FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(MakeBridgeTestFacilityId(20)))
		{
			TestEqual(TEXT("Global stage retains real distance cost input"), Candidate.DistanceMeters, 500.f);
			TestEqual(TEXT("Global stage retains destination zone density"), Candidate.CurrentOccupancy, 80.f);
			TestEqual(TEXT("Global stage retains destination zone capacity"), Candidate.Capacity, 100.f);
		}
	}
	const auto Unlimited = FFF_ZoneSystemBridge::BuildCandidates(Model, TEXT("A"), FVector::ZeroVector, 0, nullptr, true, true);
	TestEqual(TEXT("Unlimited locals also contain each stage once"), Unlimited.Num(), 16);
	const auto Ended = FFF_ZoneSystemBridge::BuildCandidates(Model, TEXT("A"), FVector::ZeroVector, 12, nullptr, false, true);
	TestEqual(TEXT("After event only local ordinary candidates remain"), Ended.Num(), 12);
	TestFalse(TEXT("Ended global stage removed"), ContainsFacility(Ended, 20));
	TestFalse(TEXT("Ended local main stage removed"), ContainsFacility(Ended, 21));
	const auto NoBroadcast = FFF_ZoneSystemBridge::BuildCandidates(Model, TEXT("A"), FVector::ZeroVector, 0, nullptr, true, false);
	TestFalse(TEXT("Open gate alone does not broadcast"), ContainsFacility(NoBroadcast, 20));
	Model.Facilities[14].State = EFFZSFacilityState::Closed;
	Model.Facilities[15].bNeedsRelocation = true;
	const auto Closed = FFF_ZoneSystemBridge::BuildCandidates(Model, TEXT("A"), FVector::ZeroVector, 12, nullptr, true, true);
	TestFalse(TEXT("Global discovery respects closed facility"), ContainsFacility(Closed, 20));
	TestFalse(TEXT("Global discovery respects relocation"), ContainsFacility(Closed, 21));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SunsetOnlyDiscovery,
	"FestaFiesta.NPC.ZoneSystemBridge.OnlySunsetHasTimeGateAndGlobalDiscovery",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SunsetOnlyDiscovery::RunTest(const FString& Parameters)
{
	FFFZSReadModel Model;
	const FName ZoneIds[] = {TEXT("Here"), TEXT("Neighbor"), TEXT("Far")};
	Model.Zones.Add(MakeBridgeTestZone(ZoneIds[0], {ZoneIds[1]}));
	Model.Zones.Add(MakeBridgeTestZone(ZoneIds[1], {ZoneIds[0], ZoneIds[2]}));
	Model.Zones.Add(MakeBridgeTestZone(ZoneIds[2], {ZoneIds[1]}));
	const FName Definitions[] = {
		TEXT("Performance_MainStage_SunsetMainStage"),
		TEXT("Performance_Busking_StreetStage"),
		TEXT("Performance_ExperienceStage_DanceBattle"),
		TEXT("Performance_SubStage_LocalArtist"),
		TEXT("Performance_MainStage_OtherStage")
	};
	for (int32 ZoneIndex = 0; ZoneIndex < 3; ++ZoneIndex)
	{
		for (int32 TypeIndex = 0; TypeIndex < 5; ++TypeIndex)
		{
			Model.Facilities.Add(MakeBridgeTestFacility(100 + ZoneIndex * 10 + TypeIndex,
				ZoneIds[ZoneIndex], EFFZSFacilityCategory::Performance, Definitions[TypeIndex]));
		}
	}
	// Closed covers both pre-show and post-show. Open with/without global covers show vs standalone world.
	for (bool bOpen : {false, true})
	{
		for (bool bGlobal : {false, true})
		{
			const auto Candidates = FFF_ZoneSystemBridge::BuildCandidates(Model, ZoneIds[0],
				FVector::ZeroVector, 0, nullptr, bOpen, bGlobal);
			for (int32 ZoneIndex = 0; ZoneIndex < 3; ++ZoneIndex)
			{
				for (int32 TypeIndex = 0; TypeIndex < 5; ++TypeIndex)
				{
					const bool bExpected = TypeIndex == 0
						? bOpen && (ZoneIndex < 2 || bGlobal) : ZoneIndex < 2;
					TestEqual(*FString::Printf(TEXT("%s zone=%s open=%d global=%d"),
						*Definitions[TypeIndex].ToString(), *ZoneIds[ZoneIndex].ToString(), bOpen, bGlobal),
						ContainsFacility(Candidates, 100 + ZoneIndex * 10 + TypeIndex), bExpected);
				}
			}
		}
	}
	TestFalse(TEXT("Category alone never marks a main stage"),
		FFF_ZoneSystemBridge::IsMainStageFacility(EFFZSFacilityCategory::Performance, NAME_None));
	TestFalse(TEXT("Other category cannot opt in with Sunset's ID"),
		FFF_ZoneSystemBridge::IsMainStageFacility(EFFZSFacilityCategory::Booth, Definitions[0]));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
