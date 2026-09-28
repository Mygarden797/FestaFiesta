// Copyright Epic Games, Inc. All Rights Reserved.

#include "Queue/FF_FacilityQueue.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSQueueGeometry.h"

DEFINE_LOG_CATEGORY(LogFestaFiestaQueue);

namespace
{
	bool TryResolvePolylineSlot(
		const TArray<FVector>& Points,
		const float DistanceAlongLaneCm,
		FVector& OutLocation,
		float& OutYawDegrees)
	{
		if (Points.Num() < 2 || !FMath::IsFinite(DistanceAlongLaneCm)
			|| Points.ContainsByPredicate([](const FVector& Point) { return Point.ContainsNaN(); }))
		{
			return false;
		}

		const double DistanceCm = FMath::Max(0.0, static_cast<double>(DistanceAlongLaneCm));
		FVector TailDirection = FVector::ZeroVector;
		if (!FFFZSQueueGeometry::TrySampleLinearPolylineAtDistanceWithTangent(
			Points, DistanceCm, OutLocation, TailDirection))
		{
			return false;
		}
		OutYawDegrees = (-TailDirection).Rotation().Yaw;
		return true;
	}

}

void FFF_FacilityQueue::Configure(int32 InFacilityId, const FFF_QueueConfig& InConfig, const FFF_QueueLaneGeometry& InLane)
{
	FacilityId = InFacilityId;
	Config = InConfig;
	Lane = InLane;

	Config.MaxCapacityParties = FMath::Clamp(
		Config.MaxCapacityParties, 0, UFFZSDeveloperSettings::MaxQueueWaitingAgents);
	Config.ConcurrentServiceSlots = FMath::Clamp(
		Config.ConcurrentServiceSlots, 1, UFFZSDeveloperSettings::MaxFacilityServiceCapacity);
	Config.ServiceSecondsPerParty = FMath::Max(Config.ServiceSecondsPerParty, KINDA_SMALL_NUMBER);
	Config.PartyFormationSpacing = FMath::Max(Config.PartyFormationSpacing, 0.f);

	Lane.SlotSpacingCm = FMath::Max(Lane.SlotSpacingCm, 1.f);
	// 방향이 0벡터면 슬롯이 전부 같은 자리에 겹친다. 조용히 틀리느니 기본축으로 되돌린다.
	if (!Lane.TailDirection.Normalize())
	{
		Lane.TailDirection = FVector(1.f, 0.f, 0.f);
		UE_LOG(LogFestaFiestaQueue, Warning,
			TEXT("[대기열] Facility=%d TailDirection이 0벡터라 +X로 대체했다. 시설 배치 데이터를 확인할 것."), FacilityId);
	}

	Parties.Reset();
	PendingEvents.Reset();
	PendingLogs.Reset();
	CompletedPartyCount = 0;
	TotalCompletedWaitSeconds = 0.0;
	bIsOpen = true;
	bSlotTargetsDirty = true;
}

int32 FFF_FacilityQueue::FindPartyIndex(int32 PartyId) const
{
	for (int32 Index = 0; Index < Parties.Num(); ++Index)
	{
		if (Parties[Index].PartyId == PartyId)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

bool FFF_FacilityQueue::ContainsParty(int32 PartyId) const
{
	return FindPartyIndex(PartyId) != INDEX_NONE;
}

int32 FFF_FacilityQueue::GetOccupiedMemberCount() const
{
	// 실제 인원 통계는 팀 용량과 별개다. 여러 큰 팀의 인원을 팀 자리 상한으로 자르지 않는다.
	int64 Total = 0;
	for (const FFF_QueuedParty& Party : Parties)
	{
		Total += Party.MemberCount;
	}
	return static_cast<int32>(FMath::Clamp<int64>(
		Total, 0, MAX_int32));
}

int32 FFF_FacilityQueue::GetRemainingServiceMemberCount() const
{
	// 팀 전원이 같은 시점에 이용을 완료한다.
	return GetOccupiedMemberCount();
}

int32 FFF_FacilityQueue::GetActiveServicePartyCount() const
{
	int32 Count = 0;
	for (const FFF_QueuedParty& Party : Parties)
	{
		Count += Party.bServiceStarted ? 1 : 0;
	}
	return Count;
}

float FFF_FacilityQueue::GetServiceRate() const
{
	return static_cast<float>(FMath::Min(Config.ConcurrentServiceSlots, Config.MaxCapacityParties))
		/ Config.ServiceSecondsPerParty;
}

bool FFF_FacilityQueue::IsAcceptingNew() const
{
	return bIsOpen && (Parties.Num() < Config.MaxCapacityParties);
}

float FFF_FacilityQueue::GetExpectedWaitSeconds(int32 IncomingMemberCount) const
{
	const float Rate = GetServiceRate();
	if (Rate <= 0.f)
	{
		return FFF_QueueTimings::UnreachableWaitSeconds;
	}
	// 호환용 인원 인자는 팀 크기로 시간을 늘리지 않는다. 조회 대상은 항상 다음 한 팀이다.
	(void)IncomingMemberCount;
	TArray<float, TInlineAllocator<32>> SlotAvailableSeconds;
	SlotAvailableSeconds.Init(0.f, FMath::Min(Config.ConcurrentServiceSlots, Config.MaxCapacityParties));
	int32 ActiveSlot = 0;
	for (const FFF_QueuedParty& Party : Parties)
	{
		if (Party.bServiceStarted)
		{
			SlotAvailableSeconds[ActiveSlot++] = FMath::Max(0.f,
				Config.ServiceSecondsPerParty - Party.ServiceElapsedSeconds);
		}
	}
	for (const FFF_QueuedParty& Party : Parties)
	{
		if (Party.bServiceStarted)
		{
			continue;
		}
		int32 EarliestSlot = 0;
		for (int32 Index = 1; Index < SlotAvailableSeconds.Num(); ++Index)
		{
			if (SlotAvailableSeconds[Index] < SlotAvailableSeconds[EarliestSlot])
			{
				EarliestSlot = Index;
			}
		}
		SlotAvailableSeconds[EarliestSlot] += Config.ServiceSecondsPerParty;
	}
	float WaitSeconds = SlotAvailableSeconds[0];
	for (const float AvailableSeconds : SlotAvailableSeconds)
	{
		WaitSeconds = FMath::Min(WaitSeconds, AvailableSeconds);
	}
	return WaitSeconds;
}

FFF_QueueState FFF_FacilityQueue::GetState() const
{
	FFF_QueueState State;
	State.FacilityId = FacilityId;
	State.WaitingPartyCount = Parties.Num();
	State.OccupiedMemberCount = GetOccupiedMemberCount();
	State.RemainingServiceMemberCount = GetRemainingServiceMemberCount();
	State.RemainingServicePartyCount = Parties.Num();
	State.ActiveServicePartyCount = GetActiveServicePartyCount();
	State.CompletedPartyCount = CompletedPartyCount;
	State.AverageCompletedWaitSeconds = CompletedPartyCount > 0
		? static_cast<float>(TotalCompletedWaitSeconds / CompletedPartyCount) : 0.f;
	State.NextServiceStartWaitSeconds = GetExpectedWaitSeconds(1);
	State.MaxCapacityParties = Config.MaxCapacityParties;
	State.ServiceRate = GetServiceRate();
	State.bIsOpen = bIsOpen;
	State.bAcceptingNew = IsAcceptingNew();
	return State;
}

FFF_SlotAssignment FFF_FacilityQueue::ResolveSlot(int32 AgentId, int32 PartyId, int32 SlotIndex) const
{
	FFF_SlotAssignment Assignment;
	Assignment.AgentId = AgentId;
	Assignment.PartyId = PartyId;
	Assignment.SlotIndex = SlotIndex;

	if (Lane.ExplicitSlotTransforms.IsValidIndex(SlotIndex)
		&& !Lane.ExplicitSlotTransforms[SlotIndex].ContainsNaN())
	{
		const FTransform& ExplicitSlot = Lane.ExplicitSlotTransforms[SlotIndex];
		Assignment.TargetLocation = ExplicitSlot.GetLocation();
		Assignment.TargetYaw = ExplicitSlot.Rotator().Yaw;
		return Assignment;
	}

	if (TryResolvePolylineSlot(
		Lane.PolylinePoints,
		Lane.SlotSpacingCm * static_cast<float>(SlotIndex),
		Assignment.TargetLocation,
		Assignment.TargetYaw))
	{
		return Assignment;
	}

	// Backward-compatible fallback for existing callers and serialized straight lanes.
	Assignment.TargetLocation = Lane.HeadLocation + Lane.TailDirection * (Lane.SlotSpacingCm * static_cast<float>(SlotIndex));
	Assignment.TargetYaw = Lane.SlotYawDegrees;
	return Assignment;
}



void FFF_FacilityQueue::BuildSlotAssignments(TArray<FFF_SlotAssignment>& OutAssignments) const
{
	OutAssignments.Reset();
	OutAssignments.Reserve(GetOccupiedMemberCount());

	// 한 팀은 저작된 자리 하나를 쓴다. Crowd의 멤버별 queue column을 위해
	// 리더는 그 자리, 멤버는 그 자리 양옆에 둔다. 다음 팀 자리를 소비하지 않는다.
	int32 SlotCursor = 0;
	for (const FFF_QueuedParty& Party : Parties)
	{
		const FFF_SlotAssignment TeamAnchor = ResolveSlot(INDEX_NONE, Party.PartyId, SlotCursor);
		const FVector TeamRight = FRotator(0.f, TeamAnchor.TargetYaw, 0.f).RotateVector(FVector::RightVector);
		for (int32 i = 0; i < Party.MemberCount; ++i)
		{
			const int32 AgentId = Party.MemberAgentIds.IsValidIndex(i) ? Party.MemberAgentIds[i] : INDEX_NONE;
			FFF_SlotAssignment& A = OutAssignments.Add_GetRef(TeamAnchor);
			A.AgentId = AgentId;
			A.PartyMemberIndex = i;
			const int32 SideStep = (i + 1) / 2;
			const float Side = (i % 2 == 1) ? 1.f : -1.f;
			A.TargetLocation += TeamRight * (Side * SideStep * Config.PartyFormationSpacing);
		}
		++SlotCursor;
	}
}

bool FFF_FacilityQueue::ConsumeSlotTargetsDirty()
{
	const bool bWasDirty = bSlotTargetsDirty;
	bSlotTargetsDirty = false;
	return bWasDirty;
}

void FFF_FacilityQueue::ConsumePendingEvents(TArray<FFF_QueueEvent>& OutEvents)
{
	OutEvents.Append(MoveTemp(PendingEvents));
	PendingEvents.Reset();
}

void FFF_FacilityQueue::ConsumePendingLogs(TArray<FFF_QueueLog>& OutLogs)
{
	OutLogs.Append(MoveTemp(PendingLogs));
	PendingLogs.Reset();
}

EFF_QueueJoinResult FFF_FacilityQueue::TryJoin(const FFF_JoinRequest& Request, float SimTimeSeconds,
	int32& OutSlotIndex, float& OutEstimatedWaitSeconds)
{
	OutSlotIndex = INDEX_NONE;
	OutEstimatedWaitSeconds = 0.f;

	auto Reject = [this, &Request](EFF_QueueJoinResult Reason)
	{
		FFF_QueueEvent Event;
		Event.Type = EFF_QueueEventType::PartyRejected;
		Event.FacilityId = FacilityId;
		Event.PartyId = Request.PartyId;
		Event.RejectReason = Reason;
		PendingEvents.Add(Event);

		UE_LOG(LogFestaFiestaQueue, Verbose, TEXT("[대기열] Facility=%d Party=%d 가입 거절 (사유=%d, 인원=%d)"),
			FacilityId, Request.PartyId, static_cast<int32>(Reason), Request.MemberCount);
		return Reason;
	};

	if (Request.MemberCount <= 0 ||
		Request.MemberCount > UFFZSDeveloperSettings::MaxQueueWaitingAgents ||
		Request.PartyId == INDEX_NONE ||
		(Request.MemberAgentIds.Num() != 0 &&
		 Request.MemberAgentIds.Num() != Request.MemberCount))
	{
		return Reject(EFF_QueueJoinResult::RejectedInvalidParty);
	}
	if (!bIsOpen)
	{
		return Reject(EFF_QueueJoinResult::RejectedClosed);
	}
	if (ContainsParty(Request.PartyId))
	{
		return Reject(EFF_QueueJoinResult::RejectedAlreadyQueued);
	}

	// 전부 아니면 전무. 부분 수락은 되돌리기 어려운 상태를 만든다.
	if (Parties.Num() >= Config.MaxCapacityParties)
	{
		return Reject(EFF_QueueJoinResult::RejectedFull);
	}

	FFF_QueuedParty Party;
	Party.PartyId = Request.PartyId;
	Party.MemberCount = Request.MemberCount;
	Party.MemberAgentIds = Request.MemberAgentIds;
	Party.MaxWaitSeconds = Request.MaxWaitSeconds;
	Party.CauseId = Request.CauseId;
	Party.JoinSimTime = SimTimeSeconds;
	Party.EstimatedWaitAtJoinSeconds = GetExpectedWaitSeconds(Request.MemberCount);

	OutSlotIndex = Parties.Num();   // 인원수와 관계없는 팀 자리 번호
	OutEstimatedWaitSeconds = Party.EstimatedWaitAtJoinSeconds;

	FFF_QueueEvent Event;
	Event.Type = EFF_QueueEventType::PartyJoined;
	Event.FacilityId = FacilityId;
	Event.PartyId = Party.PartyId;
	Event.SlotIndex = OutSlotIndex;
	Event.WaitSeconds = OutEstimatedWaitSeconds;
	PendingEvents.Add(Event);

	Parties.Add(MoveTemp(Party));
	bSlotTargetsDirty = true;

	UE_LOG(LogFestaFiestaQueue, Verbose, TEXT("[대기열] Facility=%d Party=%d 가입 (인원=%d, 슬롯=%d, 예상대기=%.1fs)"),
		FacilityId, Request.PartyId, Request.MemberCount, OutSlotIndex, OutEstimatedWaitSeconds);

	return EFF_QueueJoinResult::Accepted;
}

void FFF_FacilityQueue::NotifyPartyArrived(int32 PartyId)
{
	const int32 Index = FindPartyIndex(PartyId);
	if (Index != INDEX_NONE && Index < Config.ConcurrentServiceSlots)
	{
		Parties[Index].bArrivedAtServicePoint = true;
	}
}

void FFF_FacilityQueue::RecordExit(const FFF_QueuedParty& Party, EFF_QueueExit ExitType, float SimTimeSeconds)
{
	// 서비스를 시작했으면 "기다린 시간"은 가입~서비스 시작이다. 못 받고 나갔으면 가입~퇴장 전부가 대기다.
	const float ActualWait = Party.bServiceStarted
		? (Party.ServiceStartSimTime - Party.JoinSimTime)
		: (SimTimeSeconds - Party.JoinSimTime);

	FFF_QueueLog Log;
	Log.FacilityId = FacilityId;
	Log.PartyId = Party.PartyId;
	Log.MemberCount = Party.MemberCount;
	Log.JoinSimTime = Party.JoinSimTime;
	Log.EstimatedWaitAtJoinSeconds = Party.EstimatedWaitAtJoinSeconds;
	Log.ActualWaitSeconds = FMath::Max(0.f, ActualWait);
	Log.ServiceSeconds = Party.bServiceStarted ? FMath::Max(0.f, SimTimeSeconds - Party.ServiceStartSimTime) : 0.f;
	Log.ExitSimTime = SimTimeSeconds;
	Log.ExitType = ExitType;
	Log.CauseId = Party.CauseId;
	PendingLogs.Add(Log);
	if (ExitType == EFF_QueueExit::Served)
	{
		++CompletedPartyCount;
		TotalCompletedWaitSeconds += Log.ActualWaitSeconds;
	}

	FFF_QueueEvent Event;
	Event.Type = EFF_QueueEventType::PartyExited;
	Event.FacilityId = FacilityId;
	Event.PartyId = Party.PartyId;
	Event.WaitSeconds = Log.ActualWaitSeconds;
	Event.ExitType = ExitType;
	Event.ServedMemberCount = ExitType == EFF_QueueExit::Served ? Party.MemberCount : 0;
	PendingEvents.Add(Event);

	UE_LOG(LogFestaFiestaQueue, Verbose,
		TEXT("[대기열] Facility=%d Party=%d 퇴장 (유형=%d, 예상=%.1fs 실제=%.1fs 서비스=%.1fs)"),
		FacilityId, Party.PartyId, static_cast<int32>(ExitType),
		Log.EstimatedWaitAtJoinSeconds, Log.ActualWaitSeconds, Log.ServiceSeconds);
}

bool FFF_FacilityQueue::LeaveQueue(int32 PartyId, float SimTimeSeconds)
{
	const int32 Index = FindPartyIndex(PartyId);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	RecordExit(Parties[Index], EFF_QueueExit::Cancelled, SimTimeSeconds);
	Parties.RemoveAt(Index);
	InvalidateShiftedArrivals(Index);
	bSlotTargetsDirty = true;
	return true;
}

void FFF_FacilityQueue::CloseFacility(EFF_QueueCloseReason Reason, float SimTimeSeconds)
{
	bIsOpen = false;
	if (Parties.Num() == 0)
	{
		return;
	}

	UE_LOG(LogFestaFiestaQueue, Log, TEXT("[대기열] Facility=%d 폐쇄 (사유=%d) — 대기 중 %d Party 전원 취소"),
		FacilityId, static_cast<int32>(Reason), Parties.Num());

	// 앞에서부터 기록해야 로그·이벤트 순서가 FIFO와 일치한다.
	for (const FFF_QueuedParty& Party : Parties)
	{
		RecordExit(Party, EFF_QueueExit::Cancelled, SimTimeSeconds);
	}
	Parties.Reset();
	bSlotTargetsDirty = true;
}

void FFF_FacilityQueue::ReopenFacility()
{
	bIsOpen = true;
	bSlotTargetsDirty = true;
}

void FFF_FacilityQueue::InvalidateShiftedArrivals(int32 FirstShiftedIndex)
{
	for (int32 Index = FirstShiftedIndex; Index < Parties.Num(); ++Index)
	{
		if (!Parties[Index].bServiceStarted)
		{
			Parties[Index].bArrivedAtServicePoint = false;
		}
	}
}

void FFF_FacilityQueue::Advance(float DeltaSeconds, float SimTimeSeconds)
{
	if (!bIsOpen || Parties.Num() == 0 || DeltaSeconds <= 0.f)
	{
		return;
	}

	// ── 1. 이탈 판정 ──────────────────────────────────────────────
	// 서비스가 이미 시작된 Party는 대상에서 뺀다. 창구에 선 사람이 갑자기 사라지면
	// 관측이 이상해지고, 서비스 타이머와 이탈 타이머가 경쟁하게 된다.
	{
		TArray<int32, TInlineAllocator<8>> AbandonIndices;
		for (int32 Index = 0; Index < Parties.Num(); ++Index)
		{
			const FFF_QueuedParty& Party = Parties[Index];
			if (Party.bServiceStarted || Party.MaxWaitSeconds < 0.f)
			{
				continue;
			}
			if ((SimTimeSeconds - Party.JoinSimTime) >= Party.MaxWaitSeconds)
			{
				AbandonIndices.Add(Index);
			}
		}

		// 기록은 앞에서부터(FIFO 순서), 제거는 뒤에서부터(인덱스 무효화 방지).
		for (int32 Index : AbandonIndices)
		{
			RecordExit(Parties[Index], EFF_QueueExit::Abandoned, SimTimeSeconds);
		}
		for (int32 Cursor = AbandonIndices.Num() - 1; Cursor >= 0; --Cursor)
		{
			Parties.RemoveAt(AbandonIndices[Cursor]);
		}
		if (AbandonIndices.Num() > 0)
		{
			InvalidateShiftedArrivals(AbandonIndices[0]);
			bSlotTargetsDirty = true;
		}
	}

	if (Parties.Num() == 0)
	{
		return;
	}

	// ── 2. 앞쪽 팀을 각각 독립된 이용 자리에서 동시에 처리 ────────────────
	// 완료 시각까지 스텝을 나누므로 남은 시간이 다음 팀에도 정확히 전달된다.
	float RemainingStep = DeltaSeconds;
	float StepTime = SimTimeSeconds - DeltaSeconds;
	const int32 MaxPasses = Parties.Num() + 1; // 매 반복에서 완료하거나 남은 스텝을 모두 소비한다.
	for (int32 Pass = 0; Pass < MaxPasses && RemainingStep > 0.f && Parties.Num() > 0; ++Pass)
	{
		for (int32 Index = 0; Index < FMath::Min(Config.ConcurrentServiceSlots, Parties.Num()); ++Index)
		{
			FFF_QueuedParty& Party = Parties[Index];
			if (Party.bServiceStarted)
			{
				continue;
			}
			if (Config.bRequireArrivalConfirmation && !Party.bArrivedAtServicePoint)
			{
				break; // 앞팀이 이용 자리에 도착하기 전에 뒷팀이 추월하지 않는다.
			}
			Party.bServiceStarted = true;
			Party.ServiceStartSimTime = FMath::Max(StepTime, Party.JoinSimTime);
			FFF_QueueEvent Event;
			Event.Type = EFF_QueueEventType::ServiceStarted;
			Event.FacilityId = FacilityId;
			Event.PartyId = Party.PartyId;
			Event.WaitSeconds = FMath::Max(0.f, Party.ServiceStartSimTime - Party.JoinSimTime);
			PendingEvents.Add(Event);
		}

		float SliceSeconds = RemainingStep;
		bool bHasActiveService = false;
		for (const FFF_QueuedParty& Party : Parties)
		{
			if (Party.bServiceStarted)
			{
				bHasActiveService = true;
				SliceSeconds = FMath::Min(SliceSeconds,
					FMath::Max(0.f, Party.ServiceStartSimTime - StepTime)
					+ FMath::Max(0.f, Config.ServiceSecondsPerParty - Party.ServiceElapsedSeconds));
			}
		}
		if (!bHasActiveService)
		{
			break;
		}
		const float SliceEnd = StepTime + SliceSeconds;
		for (FFF_QueuedParty& Party : Parties)
		{
			if (Party.bServiceStarted)
			{
				Party.ServiceElapsedSeconds += FMath::Max(0.f,
					SliceEnd - FMath::Max(StepTime, Party.ServiceStartSimTime));
			}
		}
		StepTime = SliceEnd;
		RemainingStep = FMath::Max(0.f, RemainingStep - SliceSeconds);

		TArray<int32, TInlineAllocator<32>> CompletedIndices;
		for (int32 Index = 0; Index < Parties.Num(); ++Index)
		{
			const FFF_QueuedParty& Party = Parties[Index];
			if (Party.bServiceStarted
				&& Party.ServiceElapsedSeconds + KINDA_SMALL_NUMBER >= Config.ServiceSecondsPerParty)
			{
				RecordExit(Party, EFF_QueueExit::Served, StepTime);
				CompletedIndices.Add(Index);
			}
		}
		for (int32 Cursor = CompletedIndices.Num() - 1; Cursor >= 0; --Cursor)
		{
			Parties.RemoveAt(CompletedIndices[Cursor]);
		}
		if (CompletedIndices.Num() > 0)
		{
			InvalidateShiftedArrivals(CompletedIndices[0]);
			bSlotTargetsDirty = true;
		}
	}
}
