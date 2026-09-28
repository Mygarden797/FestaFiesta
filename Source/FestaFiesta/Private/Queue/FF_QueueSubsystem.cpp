// Copyright Epic Games, Inc. All Rights Reserved.

#include "Queue/FF_QueueSubsystem.h"
#include "FF_FFZSDeveloperSettings.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/StringBuilder.h"

void UFF_QueueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	SimTimeSeconds = 0.f;
	StepAccumulator = 0.f;
	bPaused = false;
}

void UFF_QueueSubsystem::Deinitialize()
{
	Queues.Reset();
	FacilityIdToIndex.Reset();
	PendingEvents.Reset();
	DirtySlotFacilities.Reset();
	// QueueLogs는 남긴다 — 레벨 종료 후 CSV를 뽑는 경우가 있다.
	Super::Deinitialize();
}

bool UFF_QueueSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

// ── 등록 ────────────────────────────────────────────────────────────────────

void UFF_QueueSubsystem::RebuildFacilityIndex()
{
	FacilityIdToIndex.Reset();
	for (int32 Index = 0; Index < Queues.Num(); ++Index)
	{
		FacilityIdToIndex.Add(Queues[Index]->GetFacilityId(), Index);
	}
}

bool UFF_QueueSubsystem::RegisterFacility(int32 FacilityId, const FFF_QueueConfig& Config, const FFF_QueueLaneGeometry& Lane)
{
	const bool bConfigValid =
		Config.MaxCapacityParties >= 0 &&
		Config.MaxCapacityParties <= UFFZSDeveloperSettings::MaxQueueWaitingAgents &&
		Config.ConcurrentServiceSlots > 0 &&
		Config.ConcurrentServiceSlots <= UFFZSDeveloperSettings::MaxFacilityServiceCapacity &&
		FMath::IsFinite(Config.ServiceSecondsPerParty) &&
		Config.ServiceSecondsPerParty > 0.0f &&
		!Config.ExitLocation.ContainsNaN();
	const bool bLaneValid =
		Lane.PolylinePoints.Num() <= UFFZSDeveloperSettings::MaxQueueSplinePoints &&
		Lane.ExplicitSlotTransforms.Num() <= UFFZSDeveloperSettings::MaxQueueWaitingAgents &&
		FMath::IsFinite(Lane.SlotSpacingCm) && Lane.SlotSpacingCm > 0.0f &&
		FMath::IsFinite(Lane.SlotYawDegrees) &&
		!Lane.HeadLocation.ContainsNaN() && !Lane.TailDirection.ContainsNaN() &&
		!Lane.PolylinePoints.ContainsByPredicate(
			[](const FVector& Point) { return Point.ContainsNaN(); }) &&
		!Lane.ExplicitSlotTransforms.ContainsByPredicate(
			[](const FTransform& Transform) { return Transform.ContainsNaN(); });
	if (FacilityId == INDEX_NONE || !bConfigValid || !bLaneValid)
	{
		// 거절 사유를 구분해서 남긴다. 조건이 셋인데 로그가 하나면
		// 설정이 잘못된 경우에도 "ID가 잘못됐다"로 읽혀서 엉뚱한 데를 뒤지게 된다.
		const TCHAR* Reason =
			(FacilityId == INDEX_NONE) ? TEXT("FacilityId 가 INDEX_NONE")
			: (!bConfigValid) ? TEXT("Config 값이 범위를 벗어남")
			: TEXT("Lane geometry 가 유효하지 않음");

		UE_LOG(LogFestaFiestaQueue, Warning,
			TEXT("[대기열] 시설 등록을 거절했다. Facility=%d, 사유=%s"), FacilityId, Reason);
		return false;
	}

	if (FFF_FacilityQueue* Existing = FindQueue(FacilityId))
	{
		// 재구성 단계에서 슬롯 배치가 바뀌는 경우. 대기 중이던 Party는 Configure에서 정리된다.
		UE_LOG(LogFestaFiestaQueue, Log, TEXT("[대기열] Facility=%d 재등록 — 기존 대기열을 초기화한다."), FacilityId);
		Existing->Configure(FacilityId, Config, Lane);
		DirtySlotFacilities.AddUnique(FacilityId);
		return true;
	}

	TUniquePtr<FFF_FacilityQueue> NewQueue = MakeUnique<FFF_FacilityQueue>();
	NewQueue->Configure(FacilityId, Config, Lane);
	Queues.Add(MoveTemp(NewQueue));

	// FacilityId 오름차순 유지 — 진행·스냅샷 순서를 등록 순서와 무관하게 만든다.
	Queues.Sort([](const TUniquePtr<FFF_FacilityQueue>& A, const TUniquePtr<FFF_FacilityQueue>& B)
	{
		return A->GetFacilityId() < B->GetFacilityId();
	});
	RebuildFacilityIndex();

	DirtySlotFacilities.AddUnique(FacilityId);
	return true;
}

bool UFF_QueueSubsystem::UnregisterFacility(int32 FacilityId)
{
	const int32* IndexPtr = FacilityIdToIndex.Find(FacilityId);
	if (!IndexPtr)
	{
		return false;
	}

	// 남아 있던 Party는 취소로 정리하고 로그를 남긴다 — 조용히 사라지면 통계에 구멍이 난다.
	Queues[*IndexPtr]->CloseFacility(EFF_QueueCloseReason::Reconfigure, SimTimeSeconds);
	Queues[*IndexPtr]->ConsumePendingEvents(PendingEvents);
	Queues[*IndexPtr]->ConsumePendingLogs(QueueLogs);

	Queues.RemoveAt(*IndexPtr);
	RebuildFacilityIndex();
	DirtySlotFacilities.Remove(FacilityId);
	return true;
}

bool UFF_QueueSubsystem::IsFacilityRegistered(int32 FacilityId) const
{
	return FacilityIdToIndex.Contains(FacilityId);
}

FFF_FacilityQueue* UFF_QueueSubsystem::FindQueue(int32 FacilityId)
{
	const int32* IndexPtr = FacilityIdToIndex.Find(FacilityId);
	return IndexPtr ? Queues[*IndexPtr].Get() : nullptr;
}

const FFF_FacilityQueue* UFF_QueueSubsystem::FindQueue(int32 FacilityId) const
{
	const int32* IndexPtr = FacilityIdToIndex.Find(FacilityId);
	return IndexPtr ? Queues[*IndexPtr].Get() : nullptr;
}

// ── 가입 · 이탈 ─────────────────────────────────────────────────────────────

FFF_QueueJoinResponse UFF_QueueSubsystem::TryJoin(const FFF_JoinRequest& Request)
{
	FFF_QueueJoinResponse Response;

	FFF_FacilityQueue* Queue = FindQueue(Request.FacilityId);
	if (!Queue)
	{
		Response.Result = EFF_QueueJoinResult::RejectedUnknownFacility;
		UE_LOG(LogFestaFiestaQueue, Warning, TEXT("[대기열] 미등록 Facility=%d 에 Party=%d 가입 시도."),
			Request.FacilityId, Request.PartyId);
		return Response;
	}

	Response.Result = Queue->TryJoin(Request, SimTimeSeconds, Response.SlotIndex, Response.EstimatedWaitSeconds);
	Response.bAccepted = (Response.Result == EFF_QueueJoinResult::Accepted);

	// 이벤트는 관찰자용이라 즉시 쏘지 않는다. 호출자는 반환값으로 이미 답을 받았고,
	// 여기서 브로드캐스트하면 Decision이 같은 콜스택에서 TryJoin을 다시 부를 수 있다.
	Queue->ConsumePendingEvents(PendingEvents);
	if (Queue->ConsumeSlotTargetsDirty())
	{
		DirtySlotFacilities.AddUnique(Request.FacilityId);
	}
	return Response;
}

bool UFF_QueueSubsystem::LeaveQueue(int32 FacilityId, int32 PartyId)
{
	FFF_FacilityQueue* Queue = FindQueue(FacilityId);
	if (!Queue)
	{
		return false;
	}
	const bool bRemoved = Queue->LeaveQueue(PartyId, SimTimeSeconds);
	Queue->ConsumePendingEvents(PendingEvents);
	Queue->ConsumePendingLogs(QueueLogs);
	if (Queue->ConsumeSlotTargetsDirty())
	{
		DirtySlotFacilities.AddUnique(FacilityId);
	}
	return bRemoved;
}

void UFF_QueueSubsystem::NotifyPartyArrived(int32 FacilityId, int32 PartyId)
{
	if (FFF_FacilityQueue* Queue = FindQueue(FacilityId))
	{
		Queue->NotifyPartyArrived(PartyId);
	}
}

// ── 개폐 ────────────────────────────────────────────────────────────────────

void UFF_QueueSubsystem::CloseFacility(int32 FacilityId, EFF_QueueCloseReason Reason)
{
	FFF_FacilityQueue* Queue = FindQueue(FacilityId);
	if (!Queue)
	{
		return;
	}
	Queue->CloseFacility(Reason, SimTimeSeconds);
	Queue->ConsumePendingEvents(PendingEvents);
	Queue->ConsumePendingLogs(QueueLogs);
	if (Queue->ConsumeSlotTargetsDirty())
	{
		DirtySlotFacilities.AddUnique(FacilityId);
	}
}

void UFF_QueueSubsystem::ReopenFacility(int32 FacilityId)
{
	if (FFF_FacilityQueue* Queue = FindQueue(FacilityId))
	{
		Queue->ReopenFacility();
	}
}

// ── 조회 ────────────────────────────────────────────────────────────────────

FFF_QueueState UFF_QueueSubsystem::GetQueueState(int32 FacilityId) const
{
	if (const FFF_FacilityQueue* Queue = FindQueue(FacilityId))
	{
		return Queue->GetState();
	}
	FFF_QueueState Empty;
	Empty.FacilityId = FacilityId;
	Empty.bIsOpen = false;
	Empty.bAcceptingNew = false;
	return Empty;
}

TArray<FFF_QueueState> UFF_QueueSubsystem::GetAllQueueStates() const
{
	TArray<FFF_QueueState> States;
	States.Reserve(Queues.Num());
	for (const TUniquePtr<FFF_FacilityQueue>& Queue : Queues)
	{
		States.Add(Queue->GetState());
	}
	return States;
}

float UFF_QueueSubsystem::GetExpectedWaitSeconds(int32 FacilityId, int32 IncomingMemberCount) const
{
	const FFF_FacilityQueue* Queue = FindQueue(FacilityId);
	// 미등록 시설에 0을 돌려주면 "대기 없는 최고의 후보"로 보인다. 실패는 안전한 쪽으로 넘어져야 한다.
	return Queue ? Queue->GetExpectedWaitSeconds(IncomingMemberCount) : FFF_QueueTimings::UnreachableWaitSeconds;
}

float UFF_QueueSubsystem::GetExpectedWaitMinutes(int32 FacilityId, int32 IncomingMemberCount) const
{
	return GetExpectedWaitSeconds(FacilityId, IncomingMemberCount) / FFF_QueueUnits::SecondsPerDecisionMinute;
}

float UFF_QueueSubsystem::GetFacilityServiceRatePerMinute(int32 FacilityId) const
{
	const FFF_FacilityQueue* Queue = FindQueue(FacilityId);
	// 0은 "모름"이다. 호출부는 이걸로 나누면 안 된다.
	return Queue ? Queue->GetServiceRate() * FFF_QueueUnits::SecondsPerDecisionMinute : 0.f;
}

FVector UFF_QueueSubsystem::GetFacilityExitLocation(int32 FacilityId) const
{
	// 퇴장 경로가 대기열 입구와 겹치면 나가는 사람과 들어오는 사람이 서로를 막는다.
	// 겹침 검증 자체는 시설 배치(한광훈) 쪽 책임이고, Queue는 받은 값을 전달만 한다.
	const FFF_FacilityQueue* Queue = FindQueue(FacilityId);
	return Queue ? Queue->GetConfig().ExitLocation : FVector::ZeroVector;
}

// ── 진행 ────────────────────────────────────────────────────────────────────

bool UFF_QueueSubsystem::IsFestivalLive() const
{
	const UWorld* World = GetWorld();
	const UFF_FestivalStageSubsystem* Stage =
		UGameInstance::GetSubsystem<UFF_FestivalStageSubsystem>(World ? World->GetGameInstance() : nullptr);

	// 스테이지 서브시스템이 없는 맵(테스트 레벨 등)에서는 게이팅하지 않는다.
	return Stage ? (Stage->CurrentStage == EFF_FestivalStage::Live) : true;
}

void UFF_QueueSubsystem::AdvanceOneStep()
{
	SimTimeSeconds += SimulationStepSeconds;
	for (const TUniquePtr<FFF_FacilityQueue>& Queue : Queues)
	{
		Queue->Advance(SimulationStepSeconds, SimTimeSeconds);
	}
	CollectFromQueues();
}

void UFF_QueueSubsystem::CollectFromQueues()
{
	for (const TUniquePtr<FFF_FacilityQueue>& Queue : Queues)
	{
		Queue->ConsumePendingEvents(PendingEvents);
		Queue->ConsumePendingLogs(QueueLogs);
		if (Queue->ConsumeSlotTargetsDirty())
		{
			DirtySlotFacilities.AddUnique(Queue->GetFacilityId());
		}
	}
}

void UFF_QueueSubsystem::AdvanceFixedSteps(int32 NumSteps)
{
	for (int32 Step = 0; Step < NumSteps; ++Step)
	{
		AdvanceOneStep();
	}
	FlushPendingEvents();
}

void UFF_QueueSubsystem::Tick(float DeltaTime)
{
	// 일시정지는 전부 멈춘다. 알림도 풀릴 때 한꺼번에 나간다.
	if (bPaused)
	{
		return;
	}

	/* 시뮬레이션을 안 돌리는 동안에도 쌓인 알림은 내보낸다.

	   알림은 Tick 밖(합류·취소·시설 폐쇄)에서도 쌓인다. 예전에는 여기서 먼저 돌아가서,
	   마지막 시설이 철거돼 대기열이 0개가 되거나 Live 가 끝난 뒤에 취소된 손님은
	   그 알림을 다음 Live 까지 못 받았다. 그동안 줄이 없어진 자리에 서 있었다.
	*/
	if (Queues.Num() == 0 || (bAdvanceOnlyDuringLiveStage && !IsFestivalLive()))
	{
		FlushPendingEvents();
		return;
	}

	const float Step = FMath::Max(SimulationStepSeconds, KINDA_SMALL_NUMBER);
	StepAccumulator += DeltaTime;

	int32 Steps = 0;
	while (StepAccumulator >= Step && Steps < MaxCatchUpStepsPerFrame)
	{
		StepAccumulator -= Step;
		AdvanceOneStep();
		++Steps;
	}

	// 따라잡기 한도를 넘었으면 밀린 시간을 버린다. 안 버리면 히치 이후 몇 프레임 동안
	// 대기열이 순간이동하듯 소화된다. 대신 재현성은 깨지므로 AT-001 실행은 AdvanceFixedSteps를 쓸 것.
	if (Steps >= MaxCatchUpStepsPerFrame && StepAccumulator > Step)
	{
		UE_LOG(LogFestaFiestaQueue, Verbose, TEXT("[대기열] 따라잡기 한도 초과 — %.3fs 를 버렸다."), StepAccumulator);
		StepAccumulator = 0.f;
	}

	FlushPendingEvents();
}

void UFF_QueueSubsystem::FlushPendingEvents()
{
	if (PendingEvents.Num() > 0)
	{
		// 핸들러가 TryJoin을 부르면 PendingEvents에 다시 쌓인다. 지역 배열로 옮겨서
		// 순회 중 재할당을 피하고, 새로 쌓인 것은 다음 Flush로 넘긴다.
		TArray<FFF_QueueEvent> Dispatching = MoveTemp(PendingEvents);
		PendingEvents.Reset();

		for (const FFF_QueueEvent& Event : Dispatching)
		{
			switch (Event.Type)
			{
			case EFF_QueueEventType::PartyJoined:
				OnPartyJoined.Broadcast(Event.FacilityId, Event.PartyId, Event.SlotIndex, Event.WaitSeconds);
				break;
			case EFF_QueueEventType::PartyRejected:
				OnPartyRejected.Broadcast(Event.FacilityId, Event.PartyId, Event.RejectReason);
				break;
			case EFF_QueueEventType::ServiceStarted:
				OnServiceStarted.Broadcast(Event.FacilityId, Event.PartyId, Event.WaitSeconds);
				break;
			case EFF_QueueEventType::PartyExited:
				// 코어가 완료한 팀을 제거하면서 만든 스냅샷을 한 번만 전달한다.
				// Crowd의 퇴장 핸들러가 팀 상태를 바꾸기 전에 정산을 알린다.
				if (Event.ExitType == EFF_QueueExit::Served && Event.ServedMemberCount > 0)
				{
					OnPartyServiceCompleted.Broadcast(
						Event.FacilityId, Event.PartyId, Event.ServedMemberCount, Event.WaitSeconds);
				}
				OnPartyExited.Broadcast(Event.FacilityId, Event.PartyId, Event.ExitType, Event.WaitSeconds);
				break;
			default:
				break;
			}
		}
	}

	if (DirtySlotFacilities.Num() > 0)
	{
		TArray<int32> Dispatching = MoveTemp(DirtySlotFacilities);
		DirtySlotFacilities.Reset();
		Dispatching.Sort();   // 디스패치 순서를 FacilityId 오름차순으로 고정 (결정론)

		TArray<FFF_SlotAssignment> Assignments;
		for (int32 FacilityId : Dispatching)
		{
			const FFF_FacilityQueue* Queue = FindQueue(FacilityId);
			if (!Queue)
			{
				continue;
			}
			Queue->BuildSlotAssignments(Assignments);
			OnSlotTargetsUpdated.Broadcast(FacilityId, Assignments);
		}
	}
}

// ── 로그 ────────────────────────────────────────────────────────────────────

FString UFF_QueueSubsystem::BuildQueueLogCsv() const
{
	static const TCHAR* ExitTypeNames[] = { TEXT("served"), TEXT("abandoned"), TEXT("cancelled") };

	TStringBuilder<4096> Builder;
	Builder << TEXT("facility_id,party_id,member_count,join_sim_time_s,estimated_wait_at_join_s,")
			<< TEXT("actual_wait_s,service_s,exit_sim_time_s,exit_type,cause_id\n");

	for (const FFF_QueueLog& Log : QueueLogs)
	{
		const int32 ExitIndex = FMath::Clamp(static_cast<int32>(Log.ExitType), 0, UE_ARRAY_COUNT(ExitTypeNames) - 1);
		Builder.Appendf(TEXT("%d,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%s,%s\n"),
			Log.FacilityId, Log.PartyId, Log.MemberCount,
			Log.JoinSimTime, Log.EstimatedWaitAtJoinSeconds,
			Log.ActualWaitSeconds, Log.ServiceSeconds, Log.ExitSimTime,
			ExitTypeNames[ExitIndex],
			Log.CauseId.IsNone() ? TEXT("") : *Log.CauseId.ToString());
	}
	return Builder.ToString();
}

bool UFF_QueueSubsystem::SaveQueueLogCsv(const FString& FilePath) const
{
	const FString ResolvedPath = FPaths::IsRelative(FilePath)
		? FPaths::Combine(FPaths::ProjectSavedDir(), FilePath)
		: FilePath;

	const FString Csv = BuildQueueLogCsv();
	// UTF-8 BOM 없이 쓰면 엑셀에서 한글 CauseId가 깨진다. FFileHelper의 기본 UTF8WithBOM을 쓴다.
	const bool bSaved = FFileHelper::SaveStringToFile(Csv, *ResolvedPath,
		FFileHelper::EEncodingOptions::ForceUTF8);

	UE_LOG(LogFestaFiestaQueue, Log, TEXT("[대기열] QueueLog CSV %s: %s (%d행)"),
		bSaved ? TEXT("저장") : TEXT("저장 실패"), *ResolvedPath, QueueLogs.Num());
	return bSaved;
}
