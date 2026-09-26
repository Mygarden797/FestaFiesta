// FF_FFZSQueueComponent.cpp

// 시설별 결정론적 FIFO 대기열, 서비스, Drain, Cancel, Snapshot 복원을 구현한다.
// 흐름: 명시적 simulation step에서 도착·서비스·이탈을 Stable ID 순서로 갱신한다.
// 불변식: 중복 Agent와 불법 상태를 거절하며 컴포넌트 RestoreSnapshot 자체는 완전 원자적이지 않다.
// 실패/성능: Tick 간격과 Agent 수에 따라 비용이 늘며 월드 복원 staging이 전체 원자성을 제공한다.

#include "FF_FFZSQueueComponent.h"

#include "Components/SplineComponent.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSQueueAgent.h"
#include "FF_FFZSQueueGeometry.h"

#define LOCTEXT_NAMESPACE "FFZoneSystemQueue"

//큐 상태와 물리 이동의 핵심 계약
 // Closed: 신규 참가와 시뮬레이션이 모두 멈춘다.
 // Open/Full: 참가 가능 여부만 다르고, 이미 입장한 Agent의 FIFO 순서는 같다.
 // Blocked: 기존 배치를 보존한 채 시간 기반 재경로 요청만 진행.
 //  Draining: 신규 참가를 막고 Waiting/Service/Exit의 기존 소유권이 모두 해제될 때까지 진행.
 // Agent는 Waiting -> ActiveServices -> Exiting 순서로 이동하며, 실제 Exit 도착 또는 즉시 해제
 // 경로가 끝나기 전까지 큐 소유권을 유지한다. 따라서 배열 갱신과 이벤트 발행 순서가 곧 게임 규칙.
 
UFFZSFacilityQueueComponent::UFFZSFacilityQueueComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.bTickEvenWhenPaused = false;
    PrimaryComponentTick.TickInterval = FMath::Max(
        0.0f,
        GetDefault<UFFZSDeveloperSettings>()->QueueSimulationTickIntervalSeconds);
}

void UFFZSFacilityQueueComponent::Configure(const FFFZSQueueRules& NewRules)
{
    // 조명 시설의 0/0 정원은 서비스 없는 시설을 나타낸다. 서비스 시설은 기존처럼
    // 양수 정원으로 정규화하며, 조명의 빈 대기열을 1인 서비스로 바꾸지 않는다.
    Rules = NewRules;
    const int32 MinimumCapacity = NewRules.MaxWaitingAgents == 0 && NewRules.ServiceCapacity == 0 ? 0 : 1;
    Rules.MaxWaitingAgents = FMath::Max(MinimumCapacity, Rules.MaxWaitingAgents);
    Rules.ServiceCapacity = FMath::Max(MinimumCapacity, Rules.ServiceCapacity);
    Rules.SlotSpacingCm = FMath::Max(1.0f, Rules.SlotSpacingCm);
    Rules.ServiceDurationSeconds = FMath::Max(0.01f, Rules.ServiceDurationSeconds);
    Rules.SlotArrivalToleranceCm = FMath::Max(0.0f, Rules.SlotArrivalToleranceCm);
    Rules.NavigationProjectionExtentCm = FMath::Max(0.0f, Rules.NavigationProjectionExtentCm);
    Rules.BlockedTimeoutSeconds = FMath::Max(0.0f, Rules.BlockedTimeoutSeconds);
    Rules.MaxRepathAttempts = FMath::Clamp(
        Rules.MaxRepathAttempts,
        0,
        UFFZSDeveloperSettings::MaxQueueRepathAttempts);
    if (QueueState == EFFZSQueueState::Open || QueueState == EFFZSQueueState::Full)
    {
        RefreshOpenOrFullState();
    }
}

void UFFZSFacilityQueueComponent::OpenQueue()
{
    if (Rules.MaxWaitingAgents == 0 && Rules.ServiceCapacity == 0)
    {
        // 시설 자체의 점등/운영 상태와 무관하게 조명은 방문객 대기열을 열지 않는다.
        SetQueueState(EFFZSQueueState::Closed);
        return;
    }
    bDrainRequested = false;
    BlockedElapsedSeconds = 0.0f;
    RepathAttemptCount = 0;
    StateBeforeBlocked = EFFZSQueueState::Open;
    SetQueueState(EFFZSQueueState::Open);
    RefreshOpenOrFullState();
    FillServiceSlots();
}

void UFFZSFacilityQueueComponent::BeginDrain()
{
    StateBeforeBlocked = EFFZSQueueState::Draining;
    bDrainRequested = true;
    SetQueueState(IsOccupied() ? EFFZSQueueState::Draining : EFFZSQueueState::Closed);
}

void UFFZSFacilityQueueComponent::EmergencyCancel()
{
    // 먼저 세 런타임 컨테이너에서 모든 항목을 떼어 낸 뒤 Exit/즉시 해제로 다시 분류.
    // 이 순서를 지키면 아래 콜백이 재진입하더라도 취소 전 Waiting/Service 상태가 노출되지 않는다.
    TArray<FRuntimeEntry> ExitRequests;
    ExitRequests.Reserve(Waiting.Num() + ActiveServices.Num() + Exiting.Num());
    ExitRequests.Append(Waiting);
    ExitRequests.Append(ActiveServices);
    ExitRequests.Append(Exiting);
    Waiting.Reset();
    ActiveServices.Reset();
    Exiting.Reset();

    TArray<FRuntimeEntry> PhysicalNotifications;
    TArray<FRuntimeEntry> ImmediateReleases;
    for (FRuntimeEntry& Entry : ExitRequests)
    {
        Entry.bExiting = true;
        Entry.AssignedQueueIndex = INDEX_NONE;
        Entry.ServiceIndex = INDEX_NONE;
        Entry.ExitIndex = 0;
        Entry.PendingReleaseReason = EFFZSQueueReleaseReason::Cancelled;
        Entry.MovementRepathCount = 0;
        AActor* AgentActor = Entry.AgentActor.Get();
        if (AgentActor && AgentActor->GetClass()->ImplementsInterface(UFFZSQueueAgent::StaticClass()))
        {
            Exiting.Add(Entry);
            PhysicalNotifications.Add(Entry);
        }
        else
        {
            ImmediateReleases.Add(Entry);
        }
    }

    bDrainRequested = true;
    BlockedElapsedSeconds = 0.0f;
    RepathAttemptCount = 0;
    StateBeforeBlocked = Exiting.IsEmpty() ? EFFZSQueueState::Closed : EFFZSQueueState::Draining;
    SetQueueState(StateBeforeBlocked);
    // 상태와 소유권 배열을 먼저 확정하고, 물리 이동 요청과 최종 해제 이벤트는 그 다음 보낸다.
    // 리스너는 콜백 시점에 이미 일관된 취소 결과를 조회할 수 있다.
    for (const FRuntimeEntry& Entry : PhysicalNotifications)
    {
        NotifyCurrentMovementTarget(Entry);
    }
    for (const FRuntimeEntry& Entry : ImmediateReleases)
    {
        FinalizeRelease(Entry);
    }
}

void UFFZSFacilityQueueComponent::SetBlocked(const bool bBlocked)
{
    if (bBlocked)
    {
        if (QueueState == EFFZSQueueState::Closed)
        {
            return;
        }
        if (QueueState != EFFZSQueueState::Blocked)
        {
            StateBeforeBlocked = QueueState;
            BlockedElapsedSeconds = 0.0f;
            RepathAttemptCount = 0;
            SetQueueState(EFFZSQueueState::Blocked);
        }
        return;
    }

    if (QueueState != EFFZSQueueState::Blocked)
    {
        return;
    }
    BlockedElapsedSeconds = 0.0f;
    if (bDrainRequested || StateBeforeBlocked == EFFZSQueueState::Draining)
    {
        SetQueueState(IsOccupied() ? EFFZSQueueState::Draining : EFFZSQueueState::Closed);
    }
    else
    {
        SetQueueState(EFFZSQueueState::Open);
        RefreshOpenOrFullState();
        FillServiceSlots();
    }
}

EFFZSQueueJoinResult UFFZSFacilityQueueComponent::JoinQueue(const FName AgentId, AActor* AgentActor, FGuid& OutTicketId)
{
    // 거절 우선순위도 API 계약이다: 잘못된 ID/중복/정원 초과/비영업 상태 순으로 가장 구체적인
    // 원인을 반환한다. 성공 시 JoinSequence가 절대 FIFO 기준이며 배열 인덱스는 현재 표시 위치다.
    OutTicketId.Invalidate();
    if (AgentId.IsNone())
    {
        return EFFZSQueueJoinResult::InvalidAgent;
    }
    if (ContainsAgent(AgentId))
    {
        return EFFZSQueueJoinResult::DuplicateAgent;
    }
    if (Rules.MaxWaitingAgents == 0 && Rules.ServiceCapacity == 0)
    {
        return EFFZSQueueJoinResult::NotOpen;
    }
    if (Waiting.Num() + ActiveServices.Num() + Exiting.Num() >=
        UFFZSDeveloperSettings::MaxQueueRuntimeEntries)
    {
        return EFFZSQueueJoinResult::Full;
    }
    if (QueueState == EFFZSQueueState::Full || Waiting.Num() >= Rules.MaxWaitingAgents)
    {
        return EFFZSQueueJoinResult::Full;
    }
    if (QueueState != EFFZSQueueState::Open)
    {
        return EFFZSQueueJoinResult::NotOpen;
    }
    if (NextJoinSequence <= 0 || NextJoinSequence == MAX_int64)
    {
        return EFFZSQueueJoinResult::NotOpen;
    }

    FRuntimeEntry& Entry = Waiting.AddDefaulted_GetRef();
    Entry.TicketId = FGuid::NewGuid();
    Entry.AgentId = AgentId;
    Entry.AgentActor = AgentActor;
    Entry.JoinSequence = NextJoinSequence++;
    Entry.AssignedQueueIndex = Waiting.Num() - 1;
    const FGuid TicketId = Entry.TicketId;
    const int32 QueueIndex = Entry.AssignedQueueIndex;
    OutTicketId = TicketId;
    RefreshOpenOrFullState();

    // 참가 항목과 Full/Open 상태를 먼저 게시한 뒤 이벤트를 보낸다. 이벤트 수신자가 즉시 큐를
    // 조회해도 Ticket과 QueueIndex를 찾을 수 있어야 한다.
    OnAgentJoined.Broadcast(AgentId, TicketId, QueueIndex);
    if (AgentActor && AgentActor->GetClass()->ImplementsInterface(UFFZSQueueAgent::StaticClass()))
    {
        IFFZSQueueAgent::Execute_OnFFZoneSystemQueueJoined(AgentActor, GetOwnerFacilityId(), TicketId, QueueIndex);
    }
    return EFFZSQueueJoinResult::Joined;
}

bool UFFZSFacilityQueueComponent::LeaveQueue(const FName AgentId, const EFFZSQueueReleaseReason Reason)
{
    const int32 WaitingIndex = Waiting.IndexOfByPredicate(
        [AgentId](const FRuntimeEntry& Entry) { return Entry.AgentId == AgentId; });
    if (WaitingIndex != INDEX_NONE)
    {
        FRuntimeEntry Entry = Waiting[WaitingIndex];
        Waiting.RemoveAt(WaitingIndex);
        RefreshOpenOrFullState();
        NotifyQueueIndices();
        BeginExit(MoveTemp(Entry), Reason);
        return true;
    }
    const int32 ServiceIndex = ActiveServices.IndexOfByPredicate(
        [AgentId](const FRuntimeEntry& Entry) { return Entry.AgentId == AgentId; });
    if (ServiceIndex != INDEX_NONE)
    {
        FRuntimeEntry Entry = ActiveServices[ServiceIndex];
        ActiveServices.RemoveAt(ServiceIndex);
        FillServiceSlots();
        RefreshOpenOrFullState();
        BeginExit(MoveTemp(Entry), Reason);
        return true;
    }
    const int32 ExitIndex = Exiting.IndexOfByPredicate(
        [AgentId](const FRuntimeEntry& Entry) { return Entry.AgentId == AgentId; });
    if (ExitIndex != INDEX_NONE)
    {
        Exiting[ExitIndex].PendingReleaseReason = Reason;
        const FRuntimeEntry Entry = Exiting[ExitIndex];
        AActor* AgentActor = Entry.AgentActor.Get();
        if (AgentActor && AgentActor->GetClass()->ImplementsInterface(UFFZSQueueAgent::StaticClass()))
        {
            NotifyCurrentMovementTarget(Entry);
        }
        else
        {
            Exiting.RemoveAt(ExitIndex);
            if (bDrainRequested && !IsOccupied())
            {
                StateBeforeBlocked = EFFZSQueueState::Closed;
                SetQueueState(EFFZSQueueState::Closed);
            }
            FinalizeRelease(Entry);
        }
        return true;
    }
    return false;
}

void UFFZSFacilityQueueComponent::AdvanceSimulation(const float DeltaSeconds)
{
    // 이벤트 스텝 방식: 남은 Delta 전체를 한 번에 더하지 않고, 서비스 완료/대기 포기/Blocked
    // 재경로 중 가장 가까운 경계까지만 전진한다. 큰 프레임과 여러 작은 프레임의 결과를 같게 만든다.
    float RemainingSeconds = FMath::Max(0.0f, DeltaSeconds);
    if (RemainingSeconds <= 0.0f || QueueState == EFFZSQueueState::Closed)
    {
        return;
    }

    constexpr int32 MaxEventsPerAdvance = 4096;
    int32 ProcessedEvents = 0;
    while (RemainingSeconds > UE_KINDA_SMALL_NUMBER && QueueState != EFFZSQueueState::Closed && ProcessedEvents < MaxEventsPerAdvance)
    {
        if (QueueState != EFFZSQueueState::Blocked)
        {
            FillServiceSlots();
        }

        // 다음 사건까지의 최소 시간을 계산한다. 0초 사건도 아래에서 한 번 처리한 뒤
        // 진행 여부를 검사하므로 부동소수점 경계에서 사건을 건너뛰지 않는다.
        float StepSeconds = RemainingSeconds;
        if (Rules.AbandonAfterSeconds > 0.0f)
        {
            for (const FRuntimeEntry& Entry : Waiting)
            {
                StepSeconds = FMath::Min(StepSeconds, FMath::Max(0.0f, Rules.AbandonAfterSeconds - Entry.WaitSeconds));
            }
        }
        for (const FRuntimeEntry& Entry : ActiveServices)
        {
            if (Entry.bArrivedAtServicePoint)
            {
                StepSeconds = FMath::Min(StepSeconds, FMath::Max(0.0f, Rules.ServiceDurationSeconds - Entry.ServiceElapsedSeconds));
            }
        }
        if (QueueState == EFFZSQueueState::Blocked && Rules.BlockedTimeoutSeconds > 0.0f &&
            RepathAttemptCount < Rules.MaxRepathAttempts)
        {
            StepSeconds = FMath::Min(StepSeconds, FMath::Max(0.0f, Rules.BlockedTimeoutSeconds - BlockedElapsedSeconds));
        }

        for (FRuntimeEntry& Entry : Waiting)
        {
            Entry.WaitSeconds += StepSeconds;
        }
        for (FRuntimeEntry& Entry : ActiveServices)
        {
            if (Entry.bArrivedAtServicePoint)
            {
                Entry.ServiceElapsedSeconds += StepSeconds;
            }
        }
        if (QueueState == EFFZSQueueState::Blocked)
        {
            BlockedElapsedSeconds += StepSeconds;
        }
        RemainingSeconds = FMath::Max(0.0f, RemainingSeconds - StepSeconds);
        bool bProcessedEvent = false;

        // 같은 시각에 서비스 완료와 대기 포기가 겹치면 서비스 완료를 먼저 확정한다.
        // 완료로 빈 서비스 슬롯이 생기면 FIFO 선두를 승격한 후 남은 대기자의 포기를 판정한다.
        TArray<FRuntimeEntry> CompletedEntries;
        for (int32 Index = ActiveServices.Num() - 1; Index >= 0; --Index)
        {
            if (ActiveServices[Index].bArrivedAtServicePoint &&
                ActiveServices[Index].ServiceElapsedSeconds + UE_KINDA_SMALL_NUMBER >= Rules.ServiceDurationSeconds)
            {
                FRuntimeEntry Entry = ActiveServices[Index];
                ActiveServices.RemoveAt(Index);
                CompletedCount = static_cast<int32>(FMath::Min<int64>(
                    static_cast<int64>(CompletedCount) + 1, MAX_int32));
                TotalCompletedWaitSeconds += Entry.WaitSeconds;
                CompletedEntries.Add(MoveTemp(Entry));
                bProcessedEvent = true;
            }
        }
        for (FRuntimeEntry& Entry : CompletedEntries)
        {
            BeginExit(MoveTemp(Entry), EFFZSQueueReleaseReason::ServiceCompleted);
        }
        if (!CompletedEntries.IsEmpty() && QueueState != EFFZSQueueState::Blocked)
        {
            FillServiceSlots();
        }

        TArray<FRuntimeEntry> AbandonedEntries;
        if (Rules.AbandonAfterSeconds > 0.0f)
        {
            for (int32 Index = Waiting.Num() - 1; Index >= 0; --Index)
            {
                if (Waiting[Index].WaitSeconds + UE_KINDA_SMALL_NUMBER >= Rules.AbandonAfterSeconds)
                {
                    FRuntimeEntry Entry = Waiting[Index];
                    Waiting.RemoveAt(Index);
                    AbandonedCount = static_cast<int32>(FMath::Min<int64>(
                        static_cast<int64>(AbandonedCount) + 1, MAX_int32));
                    AbandonedEntries.Add(MoveTemp(Entry));
                    bProcessedEvent = true;
                }
            }
        }
        if (!AbandonedEntries.IsEmpty())
        {
            NotifyQueueIndices();
            for (FRuntimeEntry& Entry : AbandonedEntries)
            {
                BeginExit(MoveTemp(Entry), EFFZSQueueReleaseReason::Abandoned);
            }
        }

        if (QueueState == EFFZSQueueState::Blocked && Rules.BlockedTimeoutSeconds > 0.0f &&
            RepathAttemptCount < Rules.MaxRepathAttempts &&
            BlockedElapsedSeconds + UE_KINDA_SMALL_NUMBER >= Rules.BlockedTimeoutSeconds)
        {
            BlockedElapsedSeconds = FMath::Max(0.0f, BlockedElapsedSeconds - Rules.BlockedTimeoutSeconds);
            ++RepathAttemptCount;
            OnRepathRequested.Broadcast(RepathAttemptCount, Rules.MaxRepathAttempts);
            bProcessedEvent = true;
        }

        if (bDrainRequested && !IsOccupied())
        {
            StateBeforeBlocked = EFFZSQueueState::Closed;
            SetQueueState(EFFZSQueueState::Closed);
        }
        else if (!bDrainRequested && QueueState != EFFZSQueueState::Blocked)
        {
            RefreshOpenOrFullState();
        }

        if (StepSeconds <= UE_KINDA_SMALL_NUMBER && !bProcessedEvent)
        {
            break;
        }
        if (bProcessedEvent)
        {
            ++ProcessedEvents;
        }
    }
}

bool UFFZSFacilityQueueComponent::ContainsAgent(const FName AgentId) const
{
    return Waiting.ContainsByPredicate([AgentId](const FRuntimeEntry& Entry) { return Entry.AgentId == AgentId; }) ||
        ActiveServices.ContainsByPredicate([AgentId](const FRuntimeEntry& Entry) { return Entry.AgentId == AgentId; }) ||
        Exiting.ContainsByPredicate([AgentId](const FRuntimeEntry& Entry) { return Entry.AgentId == AgentId; });
}

// 물리 도착 확인은 XY 평면 거리로 판정한다. 높이 보정이나 지형 투영은 이동 계층 책임이며,
// 현재 FIFO 인덱스와 실제 할당 인덱스가 모두 맞아야 이전 목표에 대한 늦은 응답을 거부한다.
bool UFFZSFacilityQueueComponent::ReportAgentArrivedAtQueueSlot(const FName AgentId, const FVector ActualWorldLocation)
{
    if (ActualWorldLocation.ContainsNaN())
    {
        return false;
    }
    for (int32 Index = 0; Index < Waiting.Num(); ++Index)
    {
        FRuntimeEntry& Entry = Waiting[Index];
        if (Entry.AgentId != AgentId || Entry.AssignedQueueIndex != Index)
        {
            continue;
        }
        const float ToleranceSquared = FMath::Square(Rules.SlotArrivalToleranceCm);
        if (FVector::DistSquared2D(ActualWorldLocation, GetQueueSlotWorldLocation(Index)) > ToleranceSquared)
        {
            return false;
        }
        Entry.bArrivedAtAssignedSlot = true;
        Entry.MovementRepathCount = 0;
        FillServiceSlots();
        RefreshOpenOrFullState();
        return true;
    }
    return false;
}

// Service 슬롯 배정만으로 서비스 시간은 흐르지 않는다. 실제 Anchor 허용 반경에 들어온 최초
// 보고에서만 시작 이벤트를 기록하며, 같은 도착 보고의 재전송은 멱등 성공이다.
bool UFFZSFacilityQueueComponent::ReportAgentArrivedAtServicePoint(const FName AgentId, const FVector ActualWorldLocation)
{
    if (ActualWorldLocation.ContainsNaN())
    {
        return false;
    }
    const int32 Index = ActiveServices.IndexOfByPredicate(
        [AgentId](const FRuntimeEntry& Entry) { return Entry.AgentId == AgentId; });
    if (Index == INDEX_NONE || ActiveServices[Index].ServiceIndex == INDEX_NONE)
    {
        return false;
    }

    FRuntimeEntry& Entry = ActiveServices[Index];
    const float ToleranceSquared = FMath::Square(Rules.SlotArrivalToleranceCm);
    if (FVector::DistSquared2D(ActualWorldLocation, GetServiceWorldLocation(Entry.ServiceIndex)) > ToleranceSquared)
    {
        return false;
    }
    if (Entry.bArrivedAtServicePoint)
    {
        return true;
    }

    Entry.bArrivedAtServicePoint = true;
    Entry.MovementRepathCount = 0;
    const FRuntimeEntry StartedEntry = Entry;
    OnServiceStarted.Broadcast(StartedEntry.AgentId, StartedEntry.TicketId, StartedEntry.ServiceIndex);
    if (AActor* AgentActor = StartedEntry.AgentActor.Get();
        AgentActor && AgentActor->GetClass()->ImplementsInterface(UFFZSQueueAgent::StaticClass()))
    {
        IFFZSQueueAgent::Execute_OnFFZoneSystemServiceStarted(
            AgentActor, GetOwnerFacilityId(), StartedEntry.ServiceIndex);
    }
    return true;
}

// Exit 도착이 물리 Agent의 최종 소유권 해제 지점이다. 배열에서 제거하고 Draining 종료를
// 확정한 다음 Release 이벤트를 보내므로 콜백에서 조회해도 이미 최종 상태가 보인다.
bool UFFZSFacilityQueueComponent::ReportAgentArrivedAtExit(const FName AgentId, const FVector ActualWorldLocation)
{
    if (ActualWorldLocation.ContainsNaN())
    {
        return false;
    }
    const int32 Index = Exiting.IndexOfByPredicate(
        [AgentId](const FRuntimeEntry& Entry) { return Entry.AgentId == AgentId; });
    if (Index == INDEX_NONE || Exiting[Index].ExitIndex == INDEX_NONE)
    {
        return false;
    }
    const float ToleranceSquared = FMath::Square(Rules.SlotArrivalToleranceCm);
    if (FVector::DistSquared2D(ActualWorldLocation, GetExitWorldLocation(Exiting[Index].ExitIndex)) > ToleranceSquared)
    {
        return false;
    }

    const FRuntimeEntry Entry = Exiting[Index];
    Exiting.RemoveAt(Index);
    if (bDrainRequested && !IsOccupied())
    {
        StateBeforeBlocked = EFFZSQueueState::Closed;
        SetQueueState(EFFZSQueueState::Closed);
    }
    else
    {
        RefreshOpenOrFullState();
    }
    FinalizeRelease(Entry);
    return true;
}

// MovementRepathCount는 Agent별 이동 실패 예산이다. BlockedElapsedSeconds가 만드는 큐 전체
// 재경로 횟수와 별개이며, 허용 횟수를 초과한 Agent만 RemovedBySystem Exit 경로로 보낸다.
bool UFFZSFacilityQueueComponent::ReportAgentMovementFailed(const FName AgentId)
{
    const int32 WaitingIndex = Waiting.IndexOfByPredicate(
        [AgentId](const FRuntimeEntry& Entry) { return Entry.AgentId == AgentId; });
    if (WaitingIndex != INDEX_NONE)
    {
        FRuntimeEntry& Entry = Waiting[WaitingIndex];
        Entry.bArrivedAtAssignedSlot = false;
        if (Entry.MovementRepathCount < Rules.MaxRepathAttempts)
        {
            ++Entry.MovementRepathCount;
            const FRuntimeEntry Notification = Entry;
            NotifyCurrentMovementTarget(Notification);
            return true;
        }
        FRuntimeEntry Removed = Entry;
        Waiting.RemoveAt(WaitingIndex);
        NotifyQueueIndices();
        RefreshOpenOrFullState();
        BeginExit(MoveTemp(Removed), EFFZSQueueReleaseReason::RemovedBySystem);
        return true;
    }

    const int32 ServiceIndex = ActiveServices.IndexOfByPredicate(
        [AgentId](const FRuntimeEntry& Entry) { return Entry.AgentId == AgentId; });
    if (ServiceIndex != INDEX_NONE)
    {
        FRuntimeEntry& Entry = ActiveServices[ServiceIndex];
        Entry.bArrivedAtServicePoint = false;
        if (Entry.MovementRepathCount < Rules.MaxRepathAttempts)
        {
            ++Entry.MovementRepathCount;
            const FRuntimeEntry Notification = Entry;
            NotifyCurrentMovementTarget(Notification);
            return true;
        }
        FRuntimeEntry Removed = Entry;
        ActiveServices.RemoveAt(ServiceIndex);
        FillServiceSlots();
        RefreshOpenOrFullState();
        BeginExit(MoveTemp(Removed), EFFZSQueueReleaseReason::RemovedBySystem);
        return true;
    }

    const int32 ExitIndex = Exiting.IndexOfByPredicate(
        [AgentId](const FRuntimeEntry& Entry) { return Entry.AgentId == AgentId; });
    if (ExitIndex != INDEX_NONE)
    {
        FRuntimeEntry& Entry = Exiting[ExitIndex];
        if (Entry.MovementRepathCount < Rules.MaxRepathAttempts)
        {
            ++Entry.MovementRepathCount;
            const FRuntimeEntry Notification = Entry;
            NotifyCurrentMovementTarget(Notification);
            return true;
        }

        const FRuntimeEntry Removed = Entry;
        Exiting.RemoveAt(ExitIndex);
        if (bDrainRequested && !IsOccupied())
        {
            StateBeforeBlocked = EFFZSQueueState::Closed;
            SetQueueState(EFFZSQueueState::Closed);
        }
        FinalizeRelease(Removed);
        return true;
    }
    return false;
}

bool UFFZSFacilityQueueComponent::RebindAgentActor(const FName AgentId, AActor* AgentActor)
{
    if (!IsValid(AgentActor) || !AgentActor->GetClass()->ImplementsInterface(UFFZSQueueAgent::StaticClass()) ||
        IFFZSQueueAgent::Execute_GetFFZoneSystemAgentId(AgentActor) != AgentId)
    {
        return false;
    }

    auto RebindIn = [this, AgentId, AgentActor](TArray<FRuntimeEntry>& Entries)
    {
        const int32 Index = Entries.IndexOfByPredicate(
            [AgentId](const FRuntimeEntry& Entry) { return Entry.AgentId == AgentId; });
        if (Index == INDEX_NONE)
        {
            return false;
        }
        Entries[Index].AgentActor = AgentActor;
        const FRuntimeEntry Notification = Entries[Index];
        NotifyCurrentMovementTarget(Notification);
        return true;
    };
    return RebindIn(Waiting) || RebindIn(ActiveServices) || RebindIn(Exiting);
}

FVector UFFZSFacilityQueueComponent::GetQueueSlotWorldLocation(const int32 QueueIndex) const
{
    const AFFZSFacilityActor* Facility = Cast<AFFZSFacilityActor>(GetOwner());
    const USplineComponent* QueueSpline = Facility ? Facility->GetQueueSpline() : nullptr;
    if (!QueueSpline)
    {
        return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
    }
    FVector LocalSlotPoint = FVector::ZeroVector;
    const double DistanceCm = static_cast<double>(QueueIndex) * Rules.SlotSpacingCm;
    if (!FFFZSQueueGeometry::TrySampleLinearPolylineAtDistance(
            Facility->GetFacilitySpec().QueueSplinePointsCm,
            DistanceCm,
            LocalSlotPoint))
    {
        return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
    }
    return QueueSpline->GetComponentTransform().TransformPosition(LocalSlotPoint);
}
FVector UFFZSFacilityQueueComponent::GetServiceWorldLocation(const int32 ServiceIndex) const
{
    FTransform AnchorTransform;
    if (const AFFZSFacilityActor* Facility = Cast<AFFZSFacilityActor>(GetOwner());
        Facility && Facility->GetAnchorWorldTransform(EFFZSAnchorType::Service, ServiceIndex, AnchorTransform))
    {
        return AnchorTransform.GetLocation();
    }
    return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

FVector UFFZSFacilityQueueComponent::GetExitWorldLocation(const int32 ExitIndex) const
{
    FTransform AnchorTransform;
    if (const AFFZSFacilityActor* Facility = Cast<AFFZSFacilityActor>(GetOwner());
        Facility && Facility->GetAnchorWorldTransform(EFFZSAnchorType::Exit, ExitIndex, AnchorTransform))
    {
        return AnchorTransform.GetLocation();
    }
    return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

// 저장 배열은 Waiting(FIFO 순서) -> Service -> Exit 순으로 기록한다. 복원은 JoinSequence를
// 기준으로 다시 정렬하므로 컨테이너 종류와 무관하게 결정적인 검사 순서를 얻는다.
FFFZSQueueSnapshot UFFZSFacilityQueueComponent::CaptureSnapshot() const
{
    FFFZSQueueSnapshot Snapshot;
    Snapshot.State = QueueState;
    Snapshot.StateBeforeBlocked = StateBeforeBlocked;
    Snapshot.NextJoinSequence = NextJoinSequence;
    Snapshot.CompletedCount = CompletedCount;
    Snapshot.AbandonedCount = AbandonedCount;
    Snapshot.TotalCompletedWaitSeconds = TotalCompletedWaitSeconds;
    Snapshot.BlockedElapsedSeconds = BlockedElapsedSeconds;
    Snapshot.RepathAttemptCount = RepathAttemptCount;
    Snapshot.bDrainRequested = bDrainRequested;
    Snapshot.Entries.Reserve(Waiting.Num() + ActiveServices.Num() + Exiting.Num());

    const auto SaveEntry = [&Snapshot](const FRuntimeEntry& Entry, const bool bInService, const bool bExiting)
    {
        FFFZSQueueEntrySnapshot& Saved = Snapshot.Entries.AddDefaulted_GetRef();
        Saved.TicketId = Entry.TicketId;
        Saved.AgentId = Entry.AgentId;
        Saved.WaitSeconds = Entry.WaitSeconds;
        Saved.ServiceElapsedSeconds = Entry.ServiceElapsedSeconds;
        Saved.JoinSequence = Entry.JoinSequence;
        Saved.bInService = bInService;
        Saved.bArrivedAtAssignedSlot = Entry.bArrivedAtAssignedSlot;
        Saved.bArrivedAtServicePoint = Entry.bArrivedAtServicePoint;
        Saved.bExiting = bExiting;
        Saved.MovementRepathCount = Entry.MovementRepathCount;
        Saved.AssignedQueueIndex = Entry.AssignedQueueIndex;
        Saved.ServiceIndex = Entry.ServiceIndex;
        Saved.ExitIndex = Entry.ExitIndex;
        Saved.PendingReleaseReason = Entry.PendingReleaseReason;
    };
    for (const FRuntimeEntry& Entry : Waiting)
    {
        SaveEntry(Entry, false, false);
    }
    for (const FRuntimeEntry& Entry : ActiveServices)
    {
        SaveEntry(Entry, true, false);
    }
    for (const FRuntimeEntry& Entry : Exiting)
    {
        SaveEntry(Entry, false, true);
    }
    return Snapshot;
}

// 입력 검증 중에는 현재 런타임 배열을 건드리지 않는다. 중복 ID/순서/슬롯을 임시 컨테이너에
// 구성해 구조적으로 잘못된 저장 데이터가 라이브 큐를 지우지 못하게 한다.
bool UFFZSFacilityQueueComponent::RestoreSnapshot(const FFFZSQueueSnapshot& Snapshot, TArray<FText>& OutErrors)
{
    OutErrors.Reset();
    if (Rules.MaxWaitingAgents == 0 && Rules.ServiceCapacity == 0 &&
        (Snapshot.State != EFFZSQueueState::Closed || !Snapshot.Entries.IsEmpty()))
    {
        OutErrors.Add(LOCTEXT("PassiveFacilityQueueSnapshot", "서비스 없는 시설은 비어 있는 Closed 대기열만 복원할 수 있습니다."));
        return false;
    }
    if (Snapshot.Entries.Num() > UFFZSDeveloperSettings::MaxQueueRuntimeEntries)
    {
        OutErrors.Add(LOCTEXT(
            "SnapshotEntrySafetyLimit",
            "Queue snapshot entry count exceeds the runtime safety limit."));
        return false;
    }
    TArray<FRuntimeEntry> NewWaiting;
    TArray<FRuntimeEntry> NewActiveServices;
    TArray<FRuntimeEntry> NewExiting;
    TSet<FName> SeenAgents;
    TSet<FGuid> SeenTickets;
    TSet<int64> SeenJoinSequences;
    TSet<int32> SeenServiceIndices;
    int64 MaximumJoinSequence = 0;

    if (!StaticEnum<EFFZSQueueState>()->IsValidEnumValue(static_cast<int64>(Snapshot.State)) ||
        !StaticEnum<EFFZSQueueState>()->IsValidEnumValue(static_cast<int64>(Snapshot.StateBeforeBlocked)) ||
        Snapshot.CompletedCount < 0 || Snapshot.AbandonedCount < 0 ||
        Snapshot.NextJoinSequence <= 0 ||
        !FMath::IsFinite(Snapshot.TotalCompletedWaitSeconds) || Snapshot.TotalCompletedWaitSeconds < 0.0f ||
        !FMath::IsFinite(Snapshot.BlockedElapsedSeconds) || Snapshot.BlockedElapsedSeconds < 0.0f)
    {
        OutErrors.Add(LOCTEXT("InvalidSnapshotHeader", "Queue 스냅샷에 잘못된 상태 또는 집계 카운터가 있습니다."));
        return false;
    }

    // TicketId는 비정상 데이터에서 JoinSequence가 같을 때도 검사 순서를 결정적으로 만드는
    // 보조 키다. 정상 스냅샷에서는 JoinSequence 중복 자체가 아래에서 거부된다.
    TArray<FFFZSQueueEntrySnapshot> Sorted = Snapshot.Entries;
    Sorted.Sort([](const FFFZSQueueEntrySnapshot& A, const FFFZSQueueEntrySnapshot& B)
        {
            return A.JoinSequence == B.JoinSequence ? A.TicketId < B.TicketId : A.JoinSequence < B.JoinSequence;
        });
    for (const FFFZSQueueEntrySnapshot& Saved : Sorted)
    {
        if (Saved.AgentId.IsNone() || !Saved.TicketId.IsValid() || SeenAgents.Contains(Saved.AgentId) ||
            SeenTickets.Contains(Saved.TicketId) || SeenJoinSequences.Contains(Saved.JoinSequence) ||
            Saved.JoinSequence <= 0 || Saved.JoinSequence == MAX_int64 ||
            !FMath::IsFinite(Saved.WaitSeconds) || Saved.WaitSeconds < 0.0f ||
            !FMath::IsFinite(Saved.ServiceElapsedSeconds) || Saved.ServiceElapsedSeconds < 0.0f ||
            Saved.MovementRepathCount < 0 || Saved.MovementRepathCount > Rules.MaxRepathAttempts)
        {
            OutErrors.Add(LOCTEXT("InvalidSnapshotEntry", "Queue 스냅샷에 잘못되었거나 중복된 Agent/Ticket/JoinSequence 항목이 있습니다."));
            return false;
        }
        SeenAgents.Add(Saved.AgentId);
        SeenTickets.Add(Saved.TicketId);
        SeenJoinSequences.Add(Saved.JoinSequence);
        MaximumJoinSequence = FMath::Max(MaximumJoinSequence, Saved.JoinSequence);

        FRuntimeEntry Entry;
        Entry.TicketId = Saved.TicketId;
        Entry.AgentId = Saved.AgentId;
        Entry.WaitSeconds = Saved.WaitSeconds;
        Entry.ServiceElapsedSeconds = Saved.ServiceElapsedSeconds;
        Entry.JoinSequence = Saved.JoinSequence;
        Entry.bArrivedAtAssignedSlot = Saved.bArrivedAtAssignedSlot;
        Entry.bArrivedAtServicePoint = Saved.bArrivedAtServicePoint;
        Entry.bExiting = Saved.bExiting;
        Entry.MovementRepathCount = Saved.MovementRepathCount;
        Entry.AssignedQueueIndex = Saved.AssignedQueueIndex;
        Entry.ServiceIndex = Saved.ServiceIndex;
        Entry.ExitIndex = Saved.ExitIndex;
        Entry.PendingReleaseReason = Saved.PendingReleaseReason;

        if (Saved.bExiting)
        {
            if (Saved.bInService || Entry.AssignedQueueIndex != INDEX_NONE || Entry.ServiceIndex != INDEX_NONE ||
                Entry.ExitIndex < 0 ||
                !StaticEnum<EFFZSQueueReleaseReason>()->IsValidEnumValue(static_cast<int64>(Entry.PendingReleaseReason)))
            {
                OutErrors.Add(LOCTEXT("InvalidExitState", "Queue 스냅샷에 잘못된 Exit 할당이 있습니다."));
                return false;
            }
            NewExiting.Add(Entry);
        }
        else if (Saved.bInService)
        {
            if (Entry.ServiceIndex < 0 || Entry.ServiceIndex >= Rules.ServiceCapacity ||
                SeenServiceIndices.Contains(Entry.ServiceIndex) || Entry.AssignedQueueIndex != INDEX_NONE ||
                Entry.ExitIndex != INDEX_NONE ||
                (!Entry.bArrivedAtServicePoint && Entry.ServiceElapsedSeconds > UE_KINDA_SMALL_NUMBER))
            {
                OutErrors.Add(LOCTEXT("InvalidServiceSlot", "Queue 스냅샷에 잘못되었거나 중복된 Service 슬롯이 있습니다."));
                return false;
            }
            SeenServiceIndices.Add(Entry.ServiceIndex);
            NewActiveServices.Add(Entry);
        }
        else
        {
            if (Entry.AssignedQueueIndex != NewWaiting.Num() || Entry.ServiceIndex != INDEX_NONE ||
                Entry.ExitIndex != INDEX_NONE || Entry.bArrivedAtServicePoint ||
                Entry.ServiceElapsedSeconds > UE_KINDA_SMALL_NUMBER)
            {
                OutErrors.Add(LOCTEXT("InvalidWaitingSlot", "Queue 스냅샷의 대기 인덱스가 결정론적 FIFO 순서와 일치하지 않습니다."));
                return false;
            }
            NewWaiting.Add(Entry);
        }
    }

    if (NewWaiting.Num() > Rules.MaxWaitingAgents || NewActiveServices.Num() > Rules.ServiceCapacity ||
        (Snapshot.State == EFFZSQueueState::Closed && (!NewWaiting.IsEmpty() || !NewActiveServices.IsEmpty() || !NewExiting.IsEmpty())) ||
        (Snapshot.State == EFFZSQueueState::Full && NewWaiting.Num() < Rules.MaxWaitingAgents))
    {
        OutErrors.Add(LOCTEXT("InvalidSnapshotCapacity", "Queue 스냅샷의 상태 또는 점유 수가 설정된 수용량을 초과했습니다."));
        return false;
    }
    if (Snapshot.State == EFFZSQueueState::Blocked &&
        (Snapshot.StateBeforeBlocked == EFFZSQueueState::Blocked || Snapshot.StateBeforeBlocked == EFFZSQueueState::Closed))
    {
        OutErrors.Add(LOCTEXT("InvalidBlockedSnapshot", "Blocked Queue 스냅샷에는 이전 Open, Full 또는 Draining 상태가 기록되어야 합니다."));
        return false;
    }

    // 게시 경계: 여기서부터 이 컴포넌트의 런타임 상태가 새 값으로 바뀐다. 아래 최종
    // ValidateInvariants 실패 시 false를 반환하지만 컴포넌트 자체를 이전 상태로 롤백하지 않는다.
    // 월드 복원은 임시 Actor에 붙인 이 컴포넌트를 검사하고 실패 시 Actor 전체를 폐기함으로써
    // 원자성을 완성한다. 따라서 이 함수만 "완전 원자적 복원"이라고 해석하면 안 된다.
    Waiting = MoveTemp(NewWaiting);
    ActiveServices = MoveTemp(NewActiveServices);
    Exiting = MoveTemp(NewExiting);
    StateBeforeBlocked = Snapshot.StateBeforeBlocked;
    bDrainRequested = Snapshot.bDrainRequested || Snapshot.State == EFFZSQueueState::Draining || Snapshot.State == EFFZSQueueState::Closed;
    const int64 MinimumNextSequence = MaximumJoinSequence == MAX_int64
        ? MAX_int64
        : MaximumJoinSequence + 1;
    NextJoinSequence = FMath::Max<int64>(Snapshot.NextJoinSequence, MinimumNextSequence);
    CompletedCount = Snapshot.CompletedCount;
    AbandonedCount = Snapshot.AbandonedCount;
    TotalCompletedWaitSeconds = Snapshot.TotalCompletedWaitSeconds;
    BlockedElapsedSeconds = FMath::Max(0.0f, Snapshot.BlockedElapsedSeconds);
    RepathAttemptCount = FMath::Clamp(Snapshot.RepathAttemptCount, 0, Rules.MaxRepathAttempts);
    SetQueueState(Snapshot.State);
    return ValidateInvariants(OutErrors);
}

// 단일 배열의 모양뿐 아니라 Waiting/Service/Exit 전체에서 Agent, Ticket, JoinSequence가
// 유일한지 검사한다. 저장 직후와 월드 게시 직전에 같은 불변식을 재사용하는 최종 방어선이다.
bool UFFZSFacilityQueueComponent::ValidateInvariants(TArray<FText>& OutErrors) const
{
    OutErrors.Reset();
    if (Waiting.Num() + ActiveServices.Num() + Exiting.Num() >
        UFFZSDeveloperSettings::MaxQueueRuntimeEntries)
    {
        OutErrors.Add(LOCTEXT(
            "RuntimeEntrySafetyLimit",
            "Queue runtime entry count exceeds the safety limit."));
        return false;
    }
    if (ActiveServices.Num() > Rules.ServiceCapacity)
    {
        OutErrors.Add(LOCTEXT("CapacityExceeded", "활성 Service 수가 ServiceCapacity를 초과했습니다."));
    }
    if (Waiting.Num() > Rules.MaxWaitingAgents)
    {
        OutErrors.Add(LOCTEXT("QueueExceeded", "대기 인원이 MaxWaitingAgents를 초과했습니다."));
    }
    if (QueueState == EFFZSQueueState::Closed && IsOccupied())
    {
        OutErrors.Add(LOCTEXT("ClosedOccupied", "Closed Queue에는 대기 중이거나 Service 중인 Agent가 있을 수 없습니다."));
    }
    if (QueueState == EFFZSQueueState::Full && Waiting.Num() < Rules.MaxWaitingAgents)
    {
        OutErrors.Add(LOCTEXT("StaleFullState", "Full Queue 상태가 현재 대기 점유 수와 일치하지 않습니다."));
    }

    TSet<FName> AgentIds;
    TSet<FGuid> TicketIds;
    TSet<int64> JoinSequences;
    TSet<int32> ServiceIndices;
    int64 PreviousSequence = MIN_int64;
    for (int32 Index = 0; Index < Waiting.Num(); ++Index)
    {
        const FRuntimeEntry& Entry = Waiting[Index];
        if (Entry.AgentId.IsNone() || !Entry.TicketId.IsValid() || AgentIds.Contains(Entry.AgentId) ||
            TicketIds.Contains(Entry.TicketId) || JoinSequences.Contains(Entry.JoinSequence))
        {
            OutErrors.Add(LOCTEXT("DuplicateWaiting", "대기 Queue에 잘못되었거나 중복된 Agent/Ticket/Sequence가 있습니다."));
        }
        if (Entry.JoinSequence < PreviousSequence)
        {
            OutErrors.Add(LOCTEXT("NonFifo", "대기 Queue가 JoinSequence 순서대로 정렬되어 있지 않습니다."));
        }
        if (Entry.AssignedQueueIndex != Index || Entry.ServiceIndex != INDEX_NONE)
        {
            OutErrors.Add(LOCTEXT("WaitingAssignment", "대기 Agent 할당이 FIFO 인덱스와 일치하지 않습니다."));
        }
        PreviousSequence = Entry.JoinSequence;
        AgentIds.Add(Entry.AgentId);
        TicketIds.Add(Entry.TicketId);
        JoinSequences.Add(Entry.JoinSequence);
    }
    for (const FRuntimeEntry& Entry : ActiveServices)
    {
        if (Entry.AgentId.IsNone() || !Entry.TicketId.IsValid() || AgentIds.Contains(Entry.AgentId) ||
            TicketIds.Contains(Entry.TicketId) || JoinSequences.Contains(Entry.JoinSequence))
        {
            OutErrors.Add(LOCTEXT("DuplicateService", "Queue와 Service 전체에 동일한 Agent/Ticket/Sequence가 두 번 이상 있습니다."));
        }
        if (Entry.AssignedQueueIndex != INDEX_NONE || Entry.ServiceIndex < 0 ||
            Entry.ServiceIndex >= Rules.ServiceCapacity || ServiceIndices.Contains(Entry.ServiceIndex))
        {
            OutErrors.Add(LOCTEXT("ServiceAssignment", "활성 Agent에 잘못되었거나 중복된 Service 슬롯이 할당되었습니다."));
        }
        AgentIds.Add(Entry.AgentId);
        TicketIds.Add(Entry.TicketId);
        JoinSequences.Add(Entry.JoinSequence);
        ServiceIndices.Add(Entry.ServiceIndex);
    }
    for (const FRuntimeEntry& Entry : Exiting)
    {
        if (Entry.AgentId.IsNone() || !Entry.TicketId.IsValid() || Entry.JoinSequence <= 0 ||
            AgentIds.Contains(Entry.AgentId) || TicketIds.Contains(Entry.TicketId) ||
            JoinSequences.Contains(Entry.JoinSequence))
        {
            OutErrors.Add(LOCTEXT("DuplicateExit", "Queue, Service 및 Exit 전체에 동일한 Agent/Ticket/Sequence가 두 번 이상 있습니다."));
        }
        if (!Entry.bExiting || Entry.AssignedQueueIndex != INDEX_NONE || Entry.ServiceIndex != INDEX_NONE ||
            Entry.ExitIndex < 0 ||
            !StaticEnum<EFFZSQueueReleaseReason>()->IsValidEnumValue(static_cast<int64>(Entry.PendingReleaseReason)))
        {
            OutErrors.Add(LOCTEXT("ExitAssignment", "퇴장 중인 Agent의 Exit 할당이 잘못되었습니다."));
        }
        if (Entry.MovementRepathCount < 0 || Entry.MovementRepathCount > Rules.MaxRepathAttempts)
        {
            OutErrors.Add(LOCTEXT("ExitRepath", "퇴장 중인 Agent의 이동 재시도 횟수가 잘못되었습니다."));
        }
        AgentIds.Add(Entry.AgentId);
        TicketIds.Add(Entry.TicketId);
        JoinSequences.Add(Entry.JoinSequence);
    }
    return OutErrors.IsEmpty();
}

float UFFZSFacilityQueueComponent::GetAverageCompletedWaitSeconds() const
{
    return CompletedCount > 0 ? TotalCompletedWaitSeconds / CompletedCount : 0.0f;
}

void UFFZSFacilityQueueComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceSimulation(DeltaTime);
}

void UFFZSFacilityQueueComponent::SetQueueState(const EFFZSQueueState NewState)
{
    const bool bStateChanged = QueueState != NewState;
    const EFFZSQueueState Previous = QueueState;
    QueueState = NewState;

    // Tick 활성 여부는 상태의 파생 불변식이다. 복원처럼 같은 상태를 다시 적용하는 호출에서도
    // 반드시 수리해야 하므로 상태 변경 이벤트 발생 여부와 분리한다. Closed만 Tick을 끈다.
    SetComponentTickEnabled(QueueState != EFFZSQueueState::Closed);

    if (bStateChanged)
    {
        if (AFFZSFacilityActor* Facility = Cast<AFFZSFacilityActor>(GetOwner()))
        {
            Facility->HandleQueueStateChanged(Previous, QueueState);
        }
        OnQueueStateChanged.Broadcast(Previous, QueueState);
    }
}

void UFFZSFacilityQueueComponent::RefreshOpenOrFullState()
{
    if (bDrainRequested || QueueState == EFFZSQueueState::Blocked || QueueState == EFFZSQueueState::Closed)
    {
        return;
    }
    SetQueueState(Waiting.Num() >= Rules.MaxWaitingAgents ? EFFZSQueueState::Full : EFFZSQueueState::Open);
}

// FIFO 선두가 실제 Queue 슬롯에 도착한 경우에만 가장 낮은 빈 Service 인덱스로 승격한다.
// 뒤 Agent가 먼저 도착해도 선두를 추월하지 않으며 Blocked/Closed에서는 배정 자체를 멈춘다.
void UFFZSFacilityQueueComponent::FillServiceSlots()
{
    if (QueueState == EFFZSQueueState::Closed || QueueState == EFFZSQueueState::Blocked ||
        Waiting.IsEmpty() || ActiveServices.Num() >= Rules.ServiceCapacity || !Waiting[0].bArrivedAtAssignedSlot)
    {
        return;
    }

    TArray<FRuntimeEntry> AssignedNotifications;
    const int32 MaximumAssignments = FMath::Min(Waiting.Num(), Rules.ServiceCapacity - ActiveServices.Num());
    AssignedNotifications.Reserve(MaximumAssignments);
    int32 AssignedCount = 0;
    int32 NextServiceIndex = 0;
    while (AssignedCount < MaximumAssignments && Waiting[AssignedCount].bArrivedAtAssignedSlot)
    {
        // 낮은 번호부터 빈 자리를 찾는다. 이미 확인한 번호는 다음 사람을 배정할 때 다시 검사하지 않는다.
        while (NextServiceIndex < Rules.ServiceCapacity &&
            ActiveServices.ContainsByPredicate([NextServiceIndex](const FRuntimeEntry& Entry)
                { return Entry.ServiceIndex == NextServiceIndex; }))
        {
            ++NextServiceIndex;
        }
        if (NextServiceIndex >= Rules.ServiceCapacity)
        {
            break;
        }

        FRuntimeEntry Entry = Waiting[AssignedCount++];
        Entry.AssignedQueueIndex = INDEX_NONE;
        Entry.ServiceIndex = NextServiceIndex++;
        Entry.ExitIndex = INDEX_NONE;
        Entry.ServiceElapsedSeconds = 0.0f;
        Entry.bArrivedAtServicePoint = false;
        Entry.bExiting = false;
        Entry.MovementRepathCount = 0;
        ActiveServices.Add(Entry);
        AssignedNotifications.Add(Entry);
    }

    // 앞사람마다 전체 대기 배열을 당기지 않고, 배정된 연속 구간을 한 번만 제거한다.
    // 배열 용량은 유지해 다음 방문객이 입장할 때 메모리를 다시 할당하지 않게 한다.
    Waiting.RemoveAt(0, AssignedCount, EAllowShrinking::No);

    // 모든 배열 이동을 먼저 끝낸 뒤 인덱스 변경과 서비스 배정 알림을 보낸다. 알림 중 재조회 또는
    // 재진입이 발생해도 반쯤 이동된 FIFO를 관찰하지 않게 하는 순서다.
    NotifyQueueIndices();
    for (const FRuntimeEntry& Assigned : AssignedNotifications)
    {
        const FRuntimeEntry* Current = ActiveServices.FindByPredicate(
            [&Assigned](const FRuntimeEntry& Entry)
            {
                return Entry.TicketId == Assigned.TicketId &&
                    Entry.ServiceIndex == Assigned.ServiceIndex &&
                    !Entry.bArrivedAtServicePoint;
            });
        if (!Current)
        {
            continue;
        }
        const FRuntimeEntry Notification = *Current;
        OnServiceAssigned.Broadcast(Notification.AgentId, Notification.TicketId, Notification.ServiceIndex);
        NotifyCurrentMovementTarget(Notification);
    }
}

FGuid UFFZSFacilityQueueComponent::GetOwnerFacilityId() const
{
    const AFFZSFacilityActor* Facility = Cast<AFFZSFacilityActor>(GetOwner());
    return Facility ? Facility->GetFacilityInstanceId() : FGuid();
}

void UFFZSFacilityQueueComponent::NotifyCurrentMovementTarget(const FRuntimeEntry& Entry)
{
    AActor* AgentActor = Entry.AgentActor.Get();
    if (!AgentActor || !AgentActor->GetClass()->ImplementsInterface(UFFZSQueueAgent::StaticClass()))
    {
        return;
    }

    // 한 Entry에는 Exit > Service > Queue 순으로 정확히 하나의 현재 이동 목표만 통지한다.
    if (Entry.bExiting && Entry.ExitIndex != INDEX_NONE)
    {
        IFFZSQueueAgent::Execute_OnFFZoneSystemExitRequested(
            AgentActor, GetOwnerFacilityId(), Entry.ExitIndex, Entry.PendingReleaseReason);
    }
    else if (Entry.ServiceIndex != INDEX_NONE && !Entry.bArrivedAtServicePoint)
    {
        IFFZSQueueAgent::Execute_OnFFZoneSystemServiceAssigned(
            AgentActor, GetOwnerFacilityId(), Entry.TicketId, Entry.ServiceIndex);
    }
    else if (Entry.AssignedQueueIndex != INDEX_NONE)
    {
        IFFZSQueueAgent::Execute_OnFFZoneSystemQueueIndexChanged(
            AgentActor, GetOwnerFacilityId(), Entry.TicketId, Entry.AssignedQueueIndex);
    }
}

// 인터페이스를 구현한 물리 Actor는 Exit 도착까지 Exiting에 남아 소유권을 유지한다.
// Actor가 없는 서버/테스트 항목은 즉시 Release하되, 마지막 점유자였다면 Drain도 함께 닫는다.
void UFFZSFacilityQueueComponent::BeginExit(FRuntimeEntry Entry, const EFFZSQueueReleaseReason Reason)
{
    Entry.bExiting = true;
    Entry.AssignedQueueIndex = INDEX_NONE;
    Entry.ServiceIndex = INDEX_NONE;
    Entry.ExitIndex = 0;
    Entry.PendingReleaseReason = Reason;
    Entry.MovementRepathCount = 0;

    AActor* AgentActor = Entry.AgentActor.Get();
    if (AgentActor && AgentActor->GetClass()->ImplementsInterface(UFFZSQueueAgent::StaticClass()))
    {
        Exiting.Add(Entry);
        const FRuntimeEntry Notification = Entry;
        NotifyCurrentMovementTarget(Notification);
    }
    else
    {
        FinalizeRelease(Entry);
    }
    if (bDrainRequested && !IsOccupied())
    {
        StateBeforeBlocked = EFFZSQueueState::Closed;
        SetQueueState(EFFZSQueueState::Closed);
    }
}

void UFFZSFacilityQueueComponent::FinalizeRelease(const FRuntimeEntry& Entry)
{
    OnAgentReleased.Broadcast(Entry.AgentId, Entry.TicketId, Entry.PendingReleaseReason);
    if (AActor* AgentActor = Entry.AgentActor.Get();
        AgentActor && AgentActor->GetClass()->ImplementsInterface(UFFZSQueueAgent::StaticClass()))
    {
        IFFZSQueueAgent::Execute_OnFFZoneSystemQueueReleased(
            AgentActor, GetOwnerFacilityId(), Entry.PendingReleaseReason);
    }
}

// 앞사람 이탈로 목표 위치가 바뀐 Agent는 새 슬롯에 다시 도착해야 한다. 인덱스 변경 시
// 이전 도착 플래그와 이동 재시도 횟수를 초기화하고, 배열 갱신이 끝난 뒤 이동 목표를 알린다.
void UFFZSFacilityQueueComponent::NotifyQueueIndices()
{
    TArray<FRuntimeEntry> Notifications;
    for (int32 Index = 0; Index < Waiting.Num(); ++Index)
    {
        FRuntimeEntry& Entry = Waiting[Index];
        if (Entry.AssignedQueueIndex == Index)
        {
            continue;
        }
        Entry.AssignedQueueIndex = Index;
        Entry.bArrivedAtAssignedSlot = false;
        Entry.MovementRepathCount = 0;
        Notifications.Add(Entry);
    }

    for (const FRuntimeEntry& Entry : Notifications)
    {
        NotifyCurrentMovementTarget(Entry);
    }
}

#undef LOCTEXT_NAMESPACE
