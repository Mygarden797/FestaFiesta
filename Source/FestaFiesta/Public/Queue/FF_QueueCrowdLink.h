// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Queue/FF_QueueTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_QueueCrowdLink.generated.h"

class UFF_CrowdSimulationSubsystem;
class UFF_QueueSubsystem;
class UFF_ZoneSystemQueueBridgeSubsystem;

// Queue 가 발행한 슬롯 목표를 실제 NPC 이동으로 옮기고, 거절·취소를 재평가 요청으로 잇는다.
//
// ─── 왜 별도 서브시스템인가 ─────────────────────────────────────────────────
// Queue 는 순서를 소유하고 Crowd 는 이동을 소유한다. 둘 다 상대를 직접 부르면 안 되는데,
// 이벤트를 받아 상대 API 를 호출하는 코드는 어딘가에 있어야 한다. 그 자리를 여기로 둔다.
//
// 양쪽 파일을 수정하지 않는다. Queue 이벤트를 구독하고 Crowd 의 공개 함수만 부른다.
//
// ─── AgentIndex 를 저장하지 않는 이유 ───────────────────────────────────────
// Crowd 의 AgentIndex 는 배열 슬롯이라 디스폰 후 재사용된다. 프레임을 넘겨 들고 있으면
// 그 인덱스가 다른 사람을 가리킨다. Party 가 줄에 서 있는 시간은 분 단위라 그 사이
// 재사용이 충분히 일어난다.
//
// 그래서 인덱스를 캐시하지 않고 발행할 때마다 GetPartyMembers 로 다시 받는다.
// 순서는 AgentIndex 오름차순으로 결정적이고, 그 전제는 테스트로 고정해두었다.
// ───────────────────────────────────────────────────────────────────────────
UCLASS()
class FESTAFIESTA_API UFF_QueueCrowdLink : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	// 결선을 끊거나 다시 잇는다. 끊으면 대기열이 논리적으로만 돌고 NPC 는 안 움직인다.
	// 대조군이 있어야 "줄이 안 선다" 가 발행 문제인지 수신 문제인지 가릴 수 있다.
	UFUNCTION(BlueprintCallable, Category = "Queue|Crowd")
	void SetLinkEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Queue|Crowd")
	bool IsLinkEnabled() const { return bLinkEnabled; }

	// 몇 명에게 슬롯 목표를 적용했는지 누적. 0 에서 안 올라가면 결선이 안 된 것이다.
	UFUNCTION(BlueprintPure, Category = "Queue|Crowd")
	int32 GetAppliedSlotTargetCount() const { return AppliedSlotTargets; }

	// Party 인원과 실제 Crowd 멤버 수가 어긋난 횟수. 계속 늘어나면 가입 시점 인원이 틀린 것이다.
	UFUNCTION(BlueprintPure, Category = "Queue|Crowd")
	int32 GetMemberCountMismatches() const { return MemberCountMismatches; }

	// 재평가를 요청한 횟수.
	UFUNCTION(BlueprintPure, Category = "Queue|Crowd")
	int32 GetDecisionRequests() const { return DecisionRequests; }

	UFUNCTION(BlueprintCallable, Category = "Queue|Crowd")
	void ResetStats();

private:
	UFUNCTION()
	void HandleSlotTargetsUpdated(int32 FacilityId, const TArray<FFF_SlotAssignment>& Assignments);

	UFUNCTION()
	void HandlePartyRejected(int32 FacilityId, int32 PartyId, EFF_QueueJoinResult Reason);

	UFUNCTION()
	void HandlePartyExited(int32 FacilityId, int32 PartyId, EFF_QueueExit ExitType, float WaitedSeconds);

	void BindToQueue();
	void UnbindFromQueue();


	UFF_QueueSubsystem* GetQueue() const;
	UFF_CrowdSimulationSubsystem* GetCrowd() const;
	UFF_ZoneSystemQueueBridgeSubsystem* GetBridge() const;

	bool bLinkEnabled = true;
	bool bBound = false;

	int32 AppliedSlotTargets = 0;
	int32 MemberCountMismatches = 0;
	int32 DecisionRequests = 0;
};
