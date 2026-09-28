// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Queue/FF_QueueTypes.h"
#include "Queue/FF_FacilityQueue.h"
#include "FF_QueueSubsystem.generated.h"

class UFF_ZoneSystemQueueBridgeSubsystem;

// ── Queue → Decision ────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FFF_OnQueuePartyJoined,
	int32, FacilityId, int32, PartyId, int32, SlotIndex, float, EstimatedWaitSeconds);

// 가입 실패. Decision은 이걸 받고 재평가한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFF_OnQueuePartyRejected,
	int32, FacilityId, int32, PartyId, EFF_QueueJoinResult, Reason);

// Queueing → Using 인계
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFF_OnQueueServiceStarted,
	int32, FacilityId, int32, PartyId, float, WaitedSeconds);

// Using → Recovering 인계 (Served) / 이탈·취소 시 Decision 재평가 (Abandoned·Cancelled)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FFF_OnQueuePartyExited,
	int32, FacilityId, int32, PartyId, EFF_QueueExit, ExitType, float, WaitedSeconds);

// 이용 완료 1건의 정산 자료. 가입 때 확정한 팀 인원을 전달해 Crowd의 이후 상태에 의존하지 않는다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FFF_OnQueuePartyServiceCompleted,
	int32, FacilityId, int32, PartyId, int32, ServedMemberCount, float, WaitedSeconds);

// ── Queue → Crowd ───────────────────────────────────────────────────────────
// 슬롯 목표 위치 배열. Crowd가 이 위치까지 "걸어서" 데려간다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFF_OnQueueSlotTargetsUpdated,
	int32, FacilityId, const TArray<FFF_SlotAssignment>&, Assignments);

/** TryJoin의 반환 묶음. 출력 파라미터를 여러 개 두는 것보다 BP에서 다루기 쉽다. */
USTRUCT(BlueprintType)
struct FFF_QueueJoinResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	EFF_QueueJoinResult Result = EFF_QueueJoinResult::RejectedUnknownFacility;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float EstimatedWaitSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	bool bAccepted = false;
};

/**
 * 모든 시설 대기열의 소유자.
 *
 * WorldSubsystem인 이유: 대기열은 레벨에 배치된 시설에 붙어 있고 레벨이 바뀌면 사라져야 한다.
 * (UFF_FestivalStageSubsystem은 축제 전체 진행이라 GameInstance가 맞다 — 수명이 다르다.)
 *
 * 경계:
 *   - 트랜스폼을 건드리지 않는다. OnSlotTargetsUpdated로 목표만 알린다.
 *   - Crowd·Decision을 폴링하지 않는다. 이벤트와 스냅샷만 쓴다.
 *   - 대기 중인 NPC를 Crowd에서 빼지 않는다. 밀도 집계에 그대로 잡혀야 한다 (8.4).
 *
 * 결정론: 실시간 Tick은 고정 스텝(SimulationStepSeconds)으로 쪼개서 코어를 돌린다.
 * 프레임 레이트가 서비스 완료 시점을 바꾸면 AT-001 Seed 재현성이 깨지기 때문이다.
 * 다만 프레임이 크게 밀리면 따라잡기를 포기하므로(스파이럴 방지), 엄밀한 재현 실행에서는
 * Tick 대신 SetPaused(true) + AdvanceFixedSteps()를 써야 한다.
 */
UCLASS()
class FESTAFIESTA_API UFF_QueueSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// ── 등록 ────────────────────────────────────────────────────────────────
	// Config·Lane은 Queue가 소유하지 않는다. 시설 데이터(한광훈) 쪽에서 넘어온다.
	UFUNCTION(BlueprintPure, Category = "Queue")
	bool IsFacilityRegistered(int32 FacilityId) const;

	// ── 가입 · 이탈 ─────────────────────────────────────────────────────────
	/**
	 * Party 단위 전부-아니면-전무 가입.
	 * 결과는 반환값으로 즉시 온다. 이벤트는 관찰자용이라 다음 Tick에 브로드캐스트된다.
	 */
	/** Crowd가 슬롯 0 도착을 알릴 때. Config.bRequireArrivalConfirmation이 true일 때만 의미가 있다. */
	// ── 개폐 ────────────────────────────────────────────────────────────────
	// ── 조회 (Decision 스냅샷용) ────────────────────────────────────────────
	UFUNCTION(BlueprintPure, Category = "Queue")
	FFF_QueueState GetQueueState(int32 FacilityId) const;

	/** FacilityId 오름차순으로 정렬된 전체 스냅샷. FFF_CrowdReadModel.QueueStates 채울 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "Queue")
	TArray<FFF_QueueState> GetAllQueueStates() const;

	UFUNCTION(BlueprintPure, Category = "Queue")
	float GetExpectedWaitSeconds(int32 FacilityId, int32 IncomingMemberCount) const;

	/** FFF_DestinationCandidate::EstimatedWaitMinutes 용. 초를 그대로 넣으면 60배 과대평가된다. */
	UFUNCTION(BlueprintPure, Category = "Queue")
	float GetExpectedWaitMinutes(int32 FacilityId, int32 IncomingMemberCount) const;

	/**
	 * 이 시설의 설정 처리율(분당 팀).
	 * 시설 정적 규칙에서 처리율을 뽑는 경로가 프로젝트에 따로 있는데 그쪽은 분당이고
	 * FFF_QueueState::ServiceRate 는 초당이다. 비교할 일이 생기면 같은 단위로 놓고 보라고 둔다.
	 * 미등록 시설은 0("모름")이다 — 이걸로 나누지 말 것.
	 */
	UFUNCTION(BlueprintPure, Category = "Queue")
	float GetFacilityServiceRatePerMinute(int32 FacilityId) const;

	UFUNCTION(BlueprintPure, Category = "Queue")
	FVector GetFacilityExitLocation(int32 FacilityId) const;

	UFUNCTION(BlueprintPure, Category = "Queue")
	float GetSimTimeSeconds() const { return SimTimeSeconds; }

	// ── 진행 제어 ───────────────────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Queue")
	void SetPaused(bool bInPaused) { bPaused = bInPaused; }

	UFUNCTION(BlueprintPure, Category = "Queue")
	bool IsPaused() const { return bPaused; }

	/** 재현 실행·테스트용. 고정 스텝을 정확히 NumSteps번 돌리고 이벤트까지 흘려보낸다. */
	UFUNCTION(BlueprintCallable, Category = "Queue")
	void AdvanceFixedSteps(int32 NumSteps);

	/** 쌓인 이벤트를 즉시 브로드캐스트한다. 이벤트 핸들러 안에서 부르면 재진입한다 — 주의. */
	UFUNCTION(BlueprintCallable, Category = "Queue")
	void FlushPendingEvents();

	// ── 로그 ────────────────────────────────────────────────────────────────
	// UHT가 UFUNCTION의 TArray const& 반환을 허용하지 않아 값으로 돌려준다.
	// 핫패스에서 부르지 말 것 — 로그 전량 복사다.
	UFUNCTION(BlueprintPure, Category = "Queue|Log")
	TArray<FFF_QueueLog> GetQueueLogs() const { return QueueLogs; }

	/** C++ 전용 무복사 접근. */
	const TArray<FFF_QueueLog>& GetQueueLogsRef() const { return QueueLogs; }

	UFUNCTION(BlueprintPure, Category = "Queue|Log")
	FString BuildQueueLogCsv() const;

	/** 상대 경로면 ProjectSavedDir() 기준. 성공하면 true. */
	UFUNCTION(BlueprintCallable, Category = "Queue|Log")
	bool SaveQueueLogCsv(const FString& FilePath) const;

	UFUNCTION(BlueprintCallable, Category = "Queue|Log")
	void ClearQueueLogs() { QueueLogs.Reset(); }

	// ── 이벤트 ──────────────────────────────────────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Queue|Events")
	FFF_OnQueuePartyJoined OnPartyJoined;

	UPROPERTY(BlueprintAssignable, Category = "Queue|Events")
	FFF_OnQueuePartyRejected OnPartyRejected;

	UPROPERTY(BlueprintAssignable, Category = "Queue|Events")
	FFF_OnQueueServiceStarted OnServiceStarted;

	UPROPERTY(BlueprintAssignable, Category = "Queue|Events")
	FFF_OnQueuePartyExited OnPartyExited;

	UPROPERTY(BlueprintAssignable, Category = "Queue|Events")
	FFF_OnQueuePartyServiceCompleted OnPartyServiceCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Queue|Events")
	FFF_OnQueueSlotTargetsUpdated OnSlotTargetsUpdated;

	// ── 설정 ────────────────────────────────────────────────────────────────
	// 30Hz. 스냅샷 권장 주기(30틱)와 맞춰뒀다.
	UPROPERTY(EditDefaultsOnly, Category = "Queue|Config")
	float SimulationStepSeconds = 1.f / 30.f;

	// 한 프레임에 따라잡을 최대 스텝 수. 넘으면 밀린 시간을 버린다(스파이럴 방지).
	UPROPERTY(EditDefaultsOnly, Category = "Queue|Config")
	int32 MaxCatchUpStepsPerFrame = 8;

	// Live 단계가 아니면 서비스·이탈 타이머를 멈춘다. UFF_FestivalStageSubsystem을 읽기 전용으로 참조한다.
	// 이 결합이 부담스러우면 false로 두고 외부에서 SetPaused를 직접 호출하면 된다.
	UPROPERTY(EditDefaultsOnly, Category = "Queue|Config")
	bool bAdvanceOnlyDuringLiveStage = true;

	// ── UTickableWorldSubsystem ─────────────────────────────────────────────
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_QueueSubsystem, STATGROUP_Tickables);
	}

private:
	friend class UFF_ZoneSystemQueueBridgeSubsystem;

	// Internal projection sink. Production Zone facilities are registered only by
	// the bridge from an installed actor's frozen UFFZSFacilityDefinition Spec.
	bool RegisterFacility(int32 FacilityId, const FFF_QueueConfig& Config, const FFF_QueueLaneGeometry& Lane);
	bool UnregisterFacility(int32 FacilityId);
	FFF_QueueJoinResponse TryJoin(const FFF_JoinRequest& Request);
	bool LeaveQueue(int32 FacilityId, int32 PartyId);
	void NotifyPartyArrived(int32 FacilityId, int32 PartyId);
	void CloseFacility(int32 FacilityId, EFF_QueueCloseReason Reason);
	void ReopenFacility(int32 FacilityId);

	FFF_FacilityQueue* FindQueue(int32 FacilityId);
	const FFF_FacilityQueue* FindQueue(int32 FacilityId) const;

	void AdvanceOneStep();
	void CollectFromQueues();
	bool IsFestivalLive() const;
	void RebuildFacilityIndex();

	// TUniquePtr 배열 + 인덱스 맵. 배열 순서(FacilityId 오름차순)가 곧 진행·스냅샷 순서라
	// TMap 순회 순서에 의존하지 않는다 — 결정론에 필요하다.
	TArray<TUniquePtr<FFF_FacilityQueue>> Queues;
	TMap<int32, int32> FacilityIdToIndex;

	TArray<FFF_QueueEvent> PendingEvents;
	TArray<FFF_QueueLog> QueueLogs;

	// 다음 Flush에서 슬롯 목표를 다시 쏴야 하는 시설들 (FacilityId 오름차순 유지)
	TArray<int32> DirtySlotFacilities;

	float SimTimeSeconds = 0.f;
	float StepAccumulator = 0.f;
	bool bPaused = false;
};
