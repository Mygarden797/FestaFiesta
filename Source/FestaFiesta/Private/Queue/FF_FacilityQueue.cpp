// Copyright Epic Games, Inc. All Rights Reserved.

#include "Queue/FF_FacilityQueue.h"

DEFINE_LOG_CATEGORY(LogFestaFiestaQueue);

namespace
{
	// 큰 DeltaSeconds 하나로 무한 루프에 빠지지 않게 하는 안전장치.
	// 고정 스텝(1/30초)에서는 절대 닿지 않는다 — 닿았다면 호출자가 스텝을 안 쪼갠 것이다.
	constexpr int32 MaxServiceCyclesPerAdvance = 64;
}

void FFF_FacilityQueue::Configure(int32 InFacilityId, const FFF_QueueConfig& InConfig, const FFF_QueueLaneGeometry& InLane)
{
	FacilityId = InFacilityId;
	Config = InConfig;
	Lane = InLane;

	Config.MaxCapacityMembers = FMath::Max(Config.MaxCapacityMembers, 0);
	Config.ConcurrentServiceSlots = FMath::Max(Config.ConcurrentServiceSlots, 1);
	Config.ServiceSecondsPerMember = FMath::Max(Config.ServiceSecondsPerMember, KINDA_SMALL_NUMBER);

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
	// 서비스를 마친 멤버도 Party 전원이 끝날 때까지 슬롯을 비우지 않는다 (설계문서 9.3).
	// 그래서 점유량은 MembersServed와 무관하게 MemberCount 총합이다.
	int32 Total = 0;
	for (const FFF_QueuedParty& Party : Parties)
	{
		Total += Party.MemberCount;
	}
	return Total;
}

int32 FFF_FacilityQueue::GetRemainingServiceMemberCount() const
{
	int32 Total = 0;
	for (const FFF_QueuedParty& Party : Parties)
	{
		Total += FMath::Max(0, Party.MemberCount - Party.MembersServed);
	}
	return Total;
}

float FFF_FacilityQueue::GetServiceRate() const
{
	return static_cast<float>(Config.ConcurrentServiceSlots) / Config.ServiceSecondsPerMember;
}

bool FFF_FacilityQueue::IsAcceptingNew() const
{
	return bIsOpen && (GetOccupiedMemberCount() < Config.MaxCapacityMembers);
}

float FFF_FacilityQueue::GetExpectedWaitSeconds(int32 IncomingMemberCount) const
{
	const float Rate = GetServiceRate();
	if (Rate <= 0.f)
	{
		return FFF_QueueTimings::UnreachableWaitSeconds;
	}
	const int32 Ahead = GetRemainingServiceMemberCount();
	return static_cast<float>(Ahead + FMath::Max(0, IncomingMemberCount)) / Rate;
}

FFF_QueueState FFF_FacilityQueue::GetState() const
{
	FFF_QueueState State;
	State.FacilityId = FacilityId;
	State.WaitingPartyCount = Parties.Num();
	State.OccupiedMemberCount = GetOccupiedMemberCount();
	State.RemainingServiceMemberCount = GetRemainingServiceMemberCount();
	State.MaxCapacityMembers = Config.MaxCapacityMembers;
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
	Assignment.TargetLocation = Lane.HeadLocation + Lane.TailDirection * (Lane.SlotSpacingCm * static_cast<float>(SlotIndex));
	Assignment.TargetYaw = Lane.SlotYawDegrees;
	return Assignment;
}

void FFF_FacilityQueue::BuildSlotAssignments(TArray<FFF_SlotAssignment>& OutAssignments) const
{
	OutAssignments.Reset();
	OutAssignments.Reserve(GetOccupiedMemberCount());

	int32 SlotCursor = 0;
	for (const FFF_QueuedParty& Party : Parties)
	{
		for (int32 MemberIndex = 0; MemberIndex < Party.MemberCount; ++MemberIndex)
		{
			// AgentId를 못 받았으면 INDEX_NONE으로 둔다 — Crowd 쪽에서 무시하거나 경고할 수 있게.
			const int32 AgentId = Party.MemberAgentIds.IsValidIndex(MemberIndex)
				? Party.MemberAgentIds[MemberIndex]
				: INDEX_NONE;
			OutAssignments.Add(ResolveSlot(AgentId, Party.PartyId, SlotCursor));
			++SlotCursor;
		}
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

	if (Request.MemberCount <= 0 || Request.PartyId == INDEX_NONE)
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
	if (GetOccupiedMemberCount() + Request.MemberCount > Config.MaxCapacityMembers)
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

	OutSlotIndex = GetOccupiedMemberCount();   // 이 Party의 선두 멤버가 설 슬롯
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
	if (Index != INDEX_NONE)
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

	FFF_QueueEvent Event;
	Event.Type = EFF_QueueEventType::PartyExited;
	Event.FacilityId = FacilityId;
	Event.PartyId = Party.PartyId;
	Event.WaitSeconds = Log.ActualWaitSeconds;
	Event.ExitType = ExitType;
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

void FFF_FacilityQueue::Advance(float DeltaSeconds, float SimTimeSeconds)
{
	if (!bIsOpen || Parties.Num() == 0)
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
			bSlotTargetsDirty = true;
		}
	}

	if (Parties.Num() == 0)
	{
		return;
	}

	// ── 2. 선두 Party 서비스 ──────────────────────────────────────
	FFF_QueuedParty& Head = Parties[0];

	if (!Head.bServiceStarted)
	{
		if (Config.bRequireArrivalConfirmation && !Head.bArrivedAtServicePoint)
		{
			// 아직 걸어오는 중. Crowd의 NotifyPartyArrived를 기다린다.
			return;
		}
		Head.bServiceStarted = true;
		Head.ServiceStartSimTime = SimTimeSeconds;

		FFF_QueueEvent Event;
		Event.Type = EFF_QueueEventType::ServiceStarted;
		Event.FacilityId = FacilityId;
		Event.PartyId = Head.PartyId;
		Event.WaitSeconds = SimTimeSeconds - Head.JoinSimTime;
		PendingEvents.Add(Event);
	}

	Head.ServiceElapsedSeconds += DeltaSeconds;

	// 한 사이클에 ConcurrentServiceSlots명씩 처리한다. P0은 1명이라 4인 가족이면 4사이클.
	int32 Guard = 0;
	while (Head.ServiceElapsedSeconds >= Config.ServiceSecondsPerMember
		&& Head.MembersServed < Head.MemberCount
		&& Guard++ < MaxServiceCyclesPerAdvance)
	{
		Head.ServiceElapsedSeconds -= Config.ServiceSecondsPerMember;
		Head.MembersServed = FMath::Min(Head.MemberCount, Head.MembersServed + Config.ConcurrentServiceSlots);
	}

	// ── 3. 전원 완료 시에만 슬롯 반납 (설계문서 9.3) ────────────────
	if (Head.MembersServed >= Head.MemberCount)
	{
		RecordExit(Head, EFF_QueueExit::Served, SimTimeSeconds);
		Parties.RemoveAt(0);
		bSlotTargetsDirty = true;
	}
}
