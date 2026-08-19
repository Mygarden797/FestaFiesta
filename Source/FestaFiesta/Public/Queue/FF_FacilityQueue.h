// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Queue/FF_QueueTypes.h"

/** FIFO 배열에 들어 있는 Party 하나의 런타임 상태. 외부에 노출하지 않는다. */
struct FFF_QueuedParty
{
	int32 PartyId = INDEX_NONE;
	int32 MemberCount = 0;
	TArray<int32> MemberAgentIds;

	// 음수면 무제한 대기
	float MaxWaitSeconds = -1.f;
	FName CauseId;

	float JoinSimTime = 0.f;
	float EstimatedWaitAtJoinSeconds = 0.f;

	// 서비스 시작 전에는 -1
	float ServiceStartSimTime = -1.f;
	float ServiceElapsedSeconds = 0.f;
	int32 MembersServed = 0;

	bool bServiceStarted = false;
	// Crowd가 "슬롯 0에 도착했다"고 통지했는지. Config.bRequireArrivalConfirmation이 false면 무시된다.
	bool bArrivedAtServicePoint = false;
};

/**
 * 시설 한 곳의 대기열. 순수 C++ — UObject도 Actor도 아니고 월드를 모른다.
 *
 * 소유 범위: FIFO 순서 배열, 슬롯 배정, 서비스 타이머. 그게 전부다.
 *   - 트랜스폼을 직접 건드리지 않는다. 슬롯 목표 위치 배열만 만들어서 Crowd에 넘긴다.
 *   - 대기 중인 NPC를 Crowd 시뮬레이션에서 빼지 않는다. 빼면 Heat Map에서 부스 앞이
 *     텅 비게 나온다.
 *   - Crowd/Decision의 내부 상태를 폴링하지 않는다. 시간은 Advance 인자로 주입받는다.
 *
 * 결정론: 시각·포인터·프레임 번호를 읽지 않는다. 같은 (등록 순서, TryJoin 호출 순서,
 * 고정 스텝 시퀀스)면 항상 같은 결과가 나온다. AT-001 Seed 재현성이 여기에 걸려 있다.
 */
class FFF_FacilityQueue
{
public:
	void Configure(int32 InFacilityId, const FFF_QueueConfig& InConfig, const FFF_QueueLaneGeometry& InLane);

	/**
	 * Party 단위 전부-아니면-전무 가입.
	 * 4인 가족인데 슬롯이 2칸 남았다면 2명만 넣지 않고 전원 거절한다.
	 * 도착 판정(Crowd의 Arrived)과 가입 판정은 반드시 분리되어야 한다 — 걸어가는 동안 줄이 찰 수 있다.
	 */
	EFF_QueueJoinResult TryJoin(const FFF_JoinRequest& Request, float SimTimeSeconds,
		int32& OutSlotIndex, float& OutEstimatedWaitSeconds);

	/** Decision이 능동적으로 줄을 뺄 때 (목적지 재평가 등). ExitType = Cancelled. */
	bool LeaveQueue(int32 PartyId, float SimTimeSeconds);

	/** Crowd가 "이 Party의 리더가 슬롯 0에 도착했다"고 알려줄 때. */
	void NotifyPartyArrived(int32 PartyId);

	/**
	 * DeltaSeconds만큼 서비스 타이머를 돌리고 이탈을 판정한다.
	 * SimTimeSeconds는 이 스텝이 끝난 시점의 누적 시뮬레이션 시각이다.
	 * 호출자가 고정 스텝으로 불러야 결정론이 보장된다.
	 */
	void Advance(float DeltaSeconds, float SimTimeSeconds);

	/**
	 * 대기 중인 전원을 Cancelled로 내보내고 시설을 닫는다.
	 * 12명이 동시에 재결정을 요청하게 되지만 Decision 측 예산제가 자동으로 분산시킨다.
	 */
	void CloseFacility(EFF_QueueCloseReason Reason, float SimTimeSeconds);
	void ReopenFacility();

	FFF_QueueState GetState() const;

	/**
	 * : (대기인원 + 내 일행) / 처리율. 정확할 필요는 없고,
	 * "결정 시점에 알 수 있는 정보로만 계산"되는 게 중요하다.
	 */
	float GetExpectedWaitSeconds(int32 IncomingMemberCount) const;

	/**
	 * FFF_DestinationCandidate::EstimatedWaitMinutes 용 어댑터.
	 * 후보 배열 전체를 채우려면 FFF_QueueWaitAdapter 를 쓰는 편이 낫다.
	 */
	float GetExpectedWaitMinutes(int32 IncomingMemberCount) const
	{
		return GetExpectedWaitSeconds(IncomingMemberCount) / FFF_QueueUnits::SecondsPerDecisionMinute;
	}

	/** 현재 FIFO 순서대로 슬롯 목표 위치를 만든다. Crowd가 이걸 받아서 걸어간다. */
	void BuildSlotAssignments(TArray<FFF_SlotAssignment>& OutAssignments) const;

	/** 마지막 조회 이후 줄 구성이 바뀌었는지. true를 한 번 반환하면 플래그는 내려간다. */
	bool ConsumeSlotTargetsDirty();

	void ConsumePendingEvents(TArray<FFF_QueueEvent>& OutEvents);
	void ConsumePendingLogs(TArray<FFF_QueueLog>& OutLogs);

	int32 GetFacilityId() const { return FacilityId; }
	const FFF_QueueConfig& GetConfig() const { return Config; }
	const FFF_QueueLaneGeometry& GetLaneGeometry() const { return Lane; }
	bool IsOpen() const { return bIsOpen; }
	bool IsAcceptingNew() const;
	bool ContainsParty(int32 PartyId) const;
	int32 GetWaitingPartyCount() const { return Parties.Num(); }
	int32 GetOccupiedMemberCount() const;
	int32 GetRemainingServiceMemberCount() const;
	float GetServiceRate() const;

	/** 슬롯 인덱스 → 월드 목표. 스플라인 도입 시 교체할 유일한 지점이다. */
	FFF_SlotAssignment ResolveSlot(int32 AgentId, int32 PartyId, int32 SlotIndex) const;

private:
	int32 FindPartyIndex(int32 PartyId) const;

	/** 퇴장 이벤트 + QueueLog 레코드를 함께 쌓는다. 실제 배열 제거는 호출자가 한다. */
	void RecordExit(const FFF_QueuedParty& Party, EFF_QueueExit ExitType, float SimTimeSeconds);

	int32 FacilityId = INDEX_NONE;
	FFF_QueueConfig Config;
	FFF_QueueLaneGeometry Lane;

	// FIFO. 앞이 창구 쪽이다. 순서 자체가 이 클래스의 핵심 소유 데이터라 정렬하거나 재배치하지 않는다.
	TArray<FFF_QueuedParty> Parties;

	bool bIsOpen = true;
	bool bSlotTargetsDirty = false;

	TArray<FFF_QueueEvent> PendingEvents;
	TArray<FFF_QueueLog> PendingLogs;
};
