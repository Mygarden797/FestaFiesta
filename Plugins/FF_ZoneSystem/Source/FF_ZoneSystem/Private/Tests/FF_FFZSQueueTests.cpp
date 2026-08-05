/**
 * @file FF_FFZSQueueTests.cpp
 * @brief Queue FIFO, 서비스, Blocked/Drain/Cancel, Snapshot 복원 계약을 검증한다.
 *
 * 흐름: 가짜 Agent와 명시적 step을 사용해 시간·도착·서비스 순서를 결정론적으로 재현한다.
 * 불변식: 0초 timeout은 즉시 실행이 아니라 해당 자동 동작 비활성화이며 복원 후 순서·의도를 보존한다.
 * 실패/성능: 실제 Nav/프레임 시간에 의존하지 않아 같은 입력에서 같은 결과를 내야 한다.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSQueueComponent.h"

/*
 * 테스트 매트릭스
 * - FIFOArrivalAndCapacity: 물리 도착 게이트, 선착순, Full/중복 거절
 * - DrainAndEmergencyCancel: 정상 배수와 긴급 취소의 서로 다른 종료 정책
 * - SaveRestoreMovementState / SaveRestoreTickAndDrain: 이동 상태·Tick 불변식·실패 전 게시 방지
 * - EventStepTieAndDeltaIndependence: 동시 사건 우선순위와 Delta 분할 독립성
 * - BlockedAndMovementFailure: 큐 전체/Agent별 재경로 예산
 * World 의존성을 제거한 컴포넌트 단위 테스트이며, 물리 도착은 테스트 전용 bridge로만 호출한다.
 */
/** Narrow test-only bridge; production mutation methods remain private. */
struct FFFZSQueueTestAccess
{
    static void ConfigureAndOpen(UFFZSFacilityQueueComponent& Queue, const FFFZSQueueRules& Rules)
    {
        Queue.Configure(Rules);
        Queue.OpenQueue();
    }

    static bool ArriveAtQueueSlot(UFFZSFacilityQueueComponent& Queue, const FName AgentId, const int32 QueueIndex)
    {
        return Queue.ReportAgentArrivedAtQueueSlot(AgentId, Queue.GetQueueSlotWorldLocation(QueueIndex));
    }

    static bool ArriveAtService(UFFZSFacilityQueueComponent& Queue, const FName AgentId, const int32 ServiceIndex)
    {
        return Queue.ReportAgentArrivedAtServicePoint(AgentId, Queue.GetServiceWorldLocation(ServiceIndex));
    }
};

namespace
{
    UFFZSFacilityQueueComponent* MakeQueue(const FFFZSQueueRules& Rules)
    {
        UFFZSFacilityQueueComponent* Queue = NewObject<UFFZSFacilityQueueComponent>();
        FFFZSQueueTestAccess::ConfigureAndOpen(*Queue, Rules);
        return Queue;
    }

    bool ArriveAtQueueSlot(UFFZSFacilityQueueComponent& Queue, const FName AgentId, const int32 QueueIndex)
    {
        return FFFZSQueueTestAccess::ArriveAtQueueSlot(Queue, AgentId, QueueIndex);
    }

    bool ArriveAtService(UFFZSFacilityQueueComponent& Queue, const FName AgentId, const int32 ServiceIndex)
    {
        return FFFZSQueueTestAccess::ArriveAtService(Queue, AgentId, ServiceIndex);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueFifoTest, "FF_ZoneSystem.Unit.Queue.FIFOArrivalAndCapacity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSQueueFifoTest::RunTest(const FString& Parameters)
{
    FFFZSQueueRules Rules;
    Rules.MaxWaitingAgents = 2;
    Rules.ServiceCapacity = 1;
    // Arrange: 대기 2명/서비스 1칸으로 Full과 FIFO 승격을 가장 작은 상태 공간에서 만든다.
    Rules.ServiceDurationSeconds = 4.0f;
    Rules.AbandonAfterSeconds = 30.0f;
    UFFZSFacilityQueueComponent* Queue = MakeQueue(Rules);

    // Act/Assert: 참가만으로 서비스가 시작되지 않고 Queue/Service 두 도착 확인을 순서대로 요구한다.
    FGuid TicketA;
    FGuid TicketB;
    FGuid TicketC;
    FGuid TicketD;
    TestEqual(TEXT("A joins waiting"), Queue->JoinQueue(TEXT("Agent.A"), nullptr, TicketA), EFFZSQueueJoinResult::Joined);
    TestEqual(TEXT("Service does not start before physical slot arrival"), Queue->GetActiveServiceCount(), 0);
    TestTrue(TEXT("A reaches FIFO slot"), ArriveAtQueueSlot(*Queue, TEXT("Agent.A"), 0));
    TestEqual(TEXT("A is assigned to service"), Queue->GetActiveServiceCount(), 1);
    TestTrue(TEXT("A reaches service anchor"), ArriveAtService(*Queue, TEXT("Agent.A"), 0));

    TestEqual(TEXT("B joins waiting"), Queue->JoinQueue(TEXT("Agent.B"), nullptr, TicketB), EFFZSQueueJoinResult::Joined);
    TestEqual(TEXT("C joins waiting"), Queue->JoinQueue(TEXT("Agent.C"), nullptr, TicketC), EFFZSQueueJoinResult::Joined);
    TestTrue(TEXT("B reaches queue head"), ArriveAtQueueSlot(*Queue, TEXT("Agent.B"), 0));
    TestTrue(TEXT("C reaches second slot"), ArriveAtQueueSlot(*Queue, TEXT("Agent.C"), 1));
    TestEqual(TEXT("Waiting list reaches max"), Queue->GetWaitingCount(), 2);
    TestEqual(TEXT("D is rejected when full"), Queue->JoinQueue(TEXT("Agent.D"), nullptr, TicketD), EFFZSQueueJoinResult::Full);
    TestEqual(TEXT("Duplicate Agent is rejected"), Queue->JoinQueue(TEXT("Agent.B"), nullptr, TicketD), EFFZSQueueJoinResult::DuplicateAgent);

    // Act/Assert: A 완료 뒤 B만 승격되고, 위치가 당겨진 C는 새 슬롯에 다시 도착해야 한다.
    Queue->AdvanceSimulation(4.0f);
    TestEqual(TEXT("Exactly one service completed"), Queue->GetCompletedCount(), 1);
    TestEqual(TEXT("B starts next by FIFO"), Queue->CaptureSnapshot().Entries.FilterByPredicate(
        [](const FFFZSQueueEntrySnapshot& Entry) { return Entry.AgentId == TEXT("Agent.B") && Entry.bInService; }).Num(), 1);
    TestEqual(TEXT("C shifted to head and must re-arrive"), Queue->CaptureSnapshot().Entries.FilterByPredicate(
        [](const FFFZSQueueEntrySnapshot& Entry) { return Entry.AgentId == TEXT("Agent.C") && !Entry.bArrivedAtAssignedSlot; }).Num(), 1);

    TArray<FText> Errors;
    TestTrue(TEXT("Queue invariants hold"), Queue->ValidateInvariants(Errors));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueClosePoliciesTest, "FF_ZoneSystem.Unit.Queue.DrainAndEmergencyCancel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSQueueClosePoliciesTest::RunTest(const FString& Parameters)
{
    FFFZSQueueRules Rules;
    // Arrange: 서비스 중 1명과 대기 선두 1명을 만들어 정상 Drain이 실제로 진행되게 한다.
    Rules.MaxWaitingAgents = 4;
    Rules.ServiceCapacity = 1;
    Rules.ServiceDurationSeconds = 1.0f;
    Rules.AbandonAfterSeconds = 30.0f;
    UFFZSFacilityQueueComponent* Queue = MakeQueue(Rules);

    FGuid Ticket;
    Queue->JoinQueue(TEXT("Drain.A"), nullptr, Ticket);
    ArriveAtQueueSlot(*Queue, TEXT("Drain.A"), 0);
    ArriveAtService(*Queue, TEXT("Drain.A"), 0);
    Queue->JoinQueue(TEXT("Drain.B"), nullptr, Ticket);
    ArriveAtQueueSlot(*Queue, TEXT("Drain.B"), 0);
    // Act/Assert: 정상 종료는 신규 참가만 막고 기존 FIFO를 끝까지 서비스한다.
    Queue->BeginDrain();
    TestEqual(TEXT("Normal close drains"), Queue->GetQueueState(), EFFZSQueueState::Draining);
    TestEqual(TEXT("New joins are rejected while draining"), Queue->JoinQueue(TEXT("Drain.C"), nullptr, Ticket), EFFZSQueueJoinResult::NotOpen);
    Queue->AdvanceSimulation(1.0f);
    TestTrue(TEXT("B reaches service after FIFO promotion"), ArriveAtService(*Queue, TEXT("Drain.B"), 0));
    Queue->AdvanceSimulation(1.0f);
    TestEqual(TEXT("Drain ends closed"), Queue->GetQueueState(), EFFZSQueueState::Closed);
    TestFalse(TEXT("Drain leaves no occupants"), Queue->IsOccupied());

    Queue->OpenQueue();
    // Act/Assert: 재개 후 긴급 종료는 대기/서비스를 즉시 Cancel 경로로 비운다.
    Queue->JoinQueue(TEXT("Emergency.A"), nullptr, Ticket);
    Queue->JoinQueue(TEXT("Emergency.B"), nullptr, Ticket);
    Queue->EmergencyCancel();
    TestEqual(TEXT("Emergency close is immediate"), Queue->GetQueueState(), EFFZSQueueState::Closed);
    TestFalse(TEXT("Emergency cancel releases everyone"), Queue->IsOccupied());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueSnapshotTest, "FF_ZoneSystem.Unit.Queue.SaveRestoreMovementState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSQueueSnapshotTest::RunTest(const FString& Parameters)
{
    FFFZSQueueRules Rules;
    // Arrange: 서비스 진행 시간, 대기 도착 상태, FIFO 순서가 모두 존재하는 원본을 만든다.
    Rules.MaxWaitingAgents = 3;
    Rules.ServiceCapacity = 1;
    Rules.ServiceDurationSeconds = 10.0f;

    UFFZSFacilityQueueComponent* Original = MakeQueue(Rules);
    FGuid Ticket;
    Original->JoinQueue(TEXT("Save.A"), nullptr, Ticket);
    ArriveAtQueueSlot(*Original, TEXT("Save.A"), 0);
    ArriveAtService(*Original, TEXT("Save.A"), 0);
    Original->JoinQueue(TEXT("Save.B"), nullptr, Ticket);
    ArriveAtQueueSlot(*Original, TEXT("Save.B"), 0);
    Original->AdvanceSimulation(2.0f);

    UFFZSFacilityQueueComponent* Restored = NewObject<UFFZSFacilityQueueComponent>();
    Restored->Configure(Rules);
    TArray<FText> Errors;
    // Act/Assert: 새 컴포넌트에 복원해 개수뿐 아니라 물리 도착 플래그와 불변식을 비교한다.
    const FFFZSQueueSnapshot Saved = Original->CaptureSnapshot();
    TestTrue(TEXT("Snapshot restores"), Restored->RestoreSnapshot(Saved, Errors));
    TestEqual(TEXT("Service count preserved"), Restored->GetActiveServiceCount(), Original->GetActiveServiceCount());
    TestEqual(TEXT("Waiting count preserved"), Restored->GetWaitingCount(), Original->GetWaitingCount());
    TestEqual(TEXT("Movement arrival state preserved"), Restored->CaptureSnapshot().Entries[0].bArrivedAtAssignedSlot, Saved.Entries[0].bArrivedAtAssignedSlot);
    TestTrue(TEXT("Restored invariants hold"), Restored->ValidateInvariants(Errors));

    // Act/Assert: 게시 전 구조 검증에서 잡히는 오류는 기존 라이브 큐를 지우지 않아야 한다.
    FFFZSQueueSnapshot Invalid = Saved;
    Invalid.Entries[0].AssignedQueueIndex = 99;
    const int32 BeforeCount = Restored->CaptureSnapshot().Entries.Num();
    TestFalse(TEXT("Invalid snapshot is rejected"), Restored->RestoreSnapshot(Invalid, Errors));
    TestEqual(TEXT("Failed restore does not erase live queue"), Restored->CaptureSnapshot().Entries.Num(), BeforeCount);

    const FFFZSQueueSnapshot BeforeDuplicateSequence = Restored->CaptureSnapshot();
    FFFZSQueueSnapshot DuplicateSequence = Saved;
    DuplicateSequence.Entries[1].JoinSequence = DuplicateSequence.Entries[0].JoinSequence;
    TestFalse(TEXT("Duplicate JoinSequence is rejected before commit"),
        Restored->RestoreSnapshot(DuplicateSequence, Errors));
    const FFFZSQueueSnapshot AfterDuplicateSequence = Restored->CaptureSnapshot();
    TestEqual(TEXT("Duplicate-sequence failure preserves entry count"),
        AfterDuplicateSequence.Entries.Num(), BeforeDuplicateSequence.Entries.Num());
    TestEqual(TEXT("Duplicate-sequence failure preserves first Agent"),
        AfterDuplicateSequence.Entries[0].AgentId, BeforeDuplicateSequence.Entries[0].AgentId);
    TestEqual(TEXT("Duplicate-sequence failure preserves queue state"),
        AfterDuplicateSequence.State, BeforeDuplicateSequence.State);

    // Arrange/Act/Assert: 물리 Exit 소유권도 저장되며 도착 보고 후에만 최종 해제되는지 확인한다.
    FFFZSQueueSnapshot ExitingSnapshot;
    ExitingSnapshot.State = EFFZSQueueState::Draining;
    ExitingSnapshot.StateBeforeBlocked = EFFZSQueueState::Draining;
    ExitingSnapshot.bDrainRequested = true;
    FFFZSQueueEntrySnapshot& ExitingEntry = ExitingSnapshot.Entries.AddDefaulted_GetRef();
    ExitingEntry.TicketId = FGuid::NewGuid();
    ExitingEntry.AgentId = TEXT("Save.Exiting");
    ExitingEntry.JoinSequence = 1;
    ExitingEntry.bExiting = true;
    ExitingEntry.ExitIndex = 0;
    ExitingEntry.PendingReleaseReason = EFFZSQueueReleaseReason::ServiceCompleted;
    TestTrue(TEXT("Physical Exit state restores"), Restored->RestoreSnapshot(ExitingSnapshot, Errors));
    TestEqual(TEXT("Restored Exit remains occupied"), Restored->GetExitingCount(), 1);
    TestTrue(TEXT("Exit arrival finalizes restored Agent"),
        Restored->ReportAgentArrivedAtExit(TEXT("Save.Exiting"), Restored->GetExitWorldLocation()));
    TestFalse(TEXT("Draining queue closes after final Exit arrival"), Restored->IsOccupied());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueSnapshotTickTest, "FF_ZoneSystem.Unit.Queue.SaveRestoreTickAndDrain", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSQueueSnapshotTickTest::RunTest(const FString& Parameters)
{
    FFFZSQueueRules Rules;
    Rules.MaxWaitingAgents = 2;
    Rules.ServiceCapacity = 1;
    Rules.ServiceDurationSeconds = 3.0f;

    // Arrange: 서비스 1초가 진행된 활성 Snapshot으로 남은 시간과 Tick 복원을 동시에 관찰한다.
    UFFZSFacilityQueueComponent* ActiveSource = MakeQueue(Rules);
    FGuid Ticket;
    TestEqual(TEXT("Agent joins source queue"),
        ActiveSource->JoinQueue(TEXT("TickRestore.A"), nullptr, Ticket), EFFZSQueueJoinResult::Joined);
    TestTrue(TEXT("Agent reaches source queue slot"),
        ArriveAtQueueSlot(*ActiveSource, TEXT("TickRestore.A"), 0));
    TestTrue(TEXT("Agent reaches source service point"),
        ArriveAtService(*ActiveSource, TEXT("TickRestore.A"), 0));
    ActiveSource->AdvanceSimulation(1.0f);
    const FFFZSQueueSnapshot ActiveSnapshot = ActiveSource->CaptureSnapshot();

    UFFZSFacilityQueueComponent* Restored = NewObject<UFFZSFacilityQueueComponent>();
    Restored->Configure(Rules);
    // Act/Assert: Closed로 생성된 대상이 Active 복원 후 Tick을 켜고, Drain 완료 후 다시 꺼야 한다.
    TestFalse(TEXT("New closed queue starts with Tick disabled"), Restored->IsComponentTickEnabled());

    TArray<FText> Errors;
    TestTrue(TEXT("Active snapshot restores"), Restored->RestoreSnapshot(ActiveSnapshot, Errors));
    TestEqual(TEXT("Active state is restored"), Restored->GetQueueState(), EFFZSQueueState::Open);
    TestTrue(TEXT("Restored active queue enables Tick"), Restored->IsComponentTickEnabled());

    Restored->BeginDrain();
    TestEqual(TEXT("Restored queue enters draining state"), Restored->GetQueueState(), EFFZSQueueState::Draining);
    TestTrue(TEXT("Draining keeps Tick enabled"), Restored->IsComponentTickEnabled());
    Restored->AdvanceSimulation(2.0f);
    TestEqual(TEXT("Restored service completes after remaining duration"), Restored->GetCompletedCount(), 1);
    TestEqual(TEXT("Completed drain closes restored queue"), Restored->GetQueueState(), EFFZSQueueState::Closed);
    TestFalse(TEXT("Completed drain disables Tick"), Restored->IsComponentTickEnabled());
    TestFalse(TEXT("Completed drain has no occupants"), Restored->IsOccupied());

    // Arrange/Act/Assert: 반대 방향인 Closed Snapshot 복원도 Tick 파생 상태를 수리해야 한다.
    UFFZSFacilityQueueComponent* ClosedSource = NewObject<UFFZSFacilityQueueComponent>();
    ClosedSource->Configure(Rules);
    const FFFZSQueueSnapshot ClosedSnapshot = ClosedSource->CaptureSnapshot();

    Restored->OpenQueue();
    TestTrue(TEXT("Reopened queue enables Tick"), Restored->IsComponentTickEnabled());
    TestTrue(TEXT("Closed snapshot restores"), Restored->RestoreSnapshot(ClosedSnapshot, Errors));
    TestEqual(TEXT("Closed state is restored"), Restored->GetQueueState(), EFFZSQueueState::Closed);
    TestFalse(TEXT("Restored closed queue disables Tick"), Restored->IsComponentTickEnabled());

    Restored->OpenQueue();
    // Act/Assert: 헤더 단계에서 거절되는 Snapshot은 현재 State와 Tick을 그대로 보존한다.
    const FFFZSQueueSnapshot BeforeInvalidRestore = Restored->CaptureSnapshot();
    FFFZSQueueSnapshot InvalidSnapshot = ClosedSnapshot;
    InvalidSnapshot.CompletedCount = -1;
    TestFalse(TEXT("Invalid snapshot is rejected before commit"),
        Restored->RestoreSnapshot(InvalidSnapshot, Errors));
    TestEqual(TEXT("Rejected snapshot preserves active state"),
        Restored->GetQueueState(), BeforeInvalidRestore.State);
    TestTrue(TEXT("Rejected snapshot preserves active Tick"), Restored->IsComponentTickEnabled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueEventStepTest, "FF_ZoneSystem.Unit.Queue.EventStepTieAndDeltaIndependence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSQueueEventStepTest::RunTest(const FString& Parameters)
{
    FFFZSQueueRules Rules;
    Rules.MaxWaitingAgents = 2;
    Rules.ServiceCapacity = 1;
    Rules.ServiceDurationSeconds = 3.0f;
    // Arrange: 서비스 완료와 다음 Agent의 포기 시각을 정확히 3초로 맞춰 tie를 만든다.
    Rules.AbandonAfterSeconds = 3.0f;

    const auto Setup = [&Rules]()
    {
        UFFZSFacilityQueueComponent* Queue = MakeQueue(Rules);
        FGuid Ticket;
        Queue->JoinQueue(TEXT("Tie.A"), nullptr, Ticket);
        ArriveAtQueueSlot(*Queue, TEXT("Tie.A"), 0);
        ArriveAtService(*Queue, TEXT("Tie.A"), 0);
        Queue->JoinQueue(TEXT("Tie.B"), nullptr, Ticket);
        ArriveAtQueueSlot(*Queue, TEXT("Tie.B"), 0);
        return Queue;
    };

    // Act: 같은 총 3초를 한 번과 세 번으로 나눠 진행한다.
    UFFZSFacilityQueueComponent* OneStep = Setup();
    OneStep->AdvanceSimulation(3.0f);
    UFFZSFacilityQueueComponent* ThreeSteps = Setup();
    ThreeSteps->AdvanceSimulation(1.0f);
    ThreeSteps->AdvanceSimulation(1.0f);
    ThreeSteps->AdvanceSimulation(1.0f);

    // Assert: 완료 우선순위와 최종 점유 상태가 Delta 분할 방식에 의존하지 않아야 한다.
    TestEqual(TEXT("One-step service completion"), OneStep->GetCompletedCount(), 1);
    TestEqual(TEXT("Split-step service completion"), ThreeSteps->GetCompletedCount(), 1);
    TestEqual(TEXT("Service completion wins abandonment tie"), OneStep->GetAbandonedCount(), 0);
    TestEqual(TEXT("Delta partition preserves abandonment"), OneStep->GetAbandonedCount(), ThreeSteps->GetAbandonedCount());
    TestEqual(TEXT("B promoted in both simulations"), OneStep->GetActiveServiceCount(), ThreeSteps->GetActiveServiceCount());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueBlockedAndMovementFailureTest, "FF_ZoneSystem.Unit.Queue.BlockedAndMovementFailure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSQueueBlockedAndMovementFailureTest::RunTest(const FString& Parameters)
{
    FFFZSQueueRules Rules;
    Rules.MaxWaitingAgents = 2;
    // Arrange: 큐 전체 Blocked 재경로와 Agent 이동 재시도 예산을 각각 2회로 고정한다.
    Rules.BlockedTimeoutSeconds = 5.0f;
    Rules.MaxRepathAttempts = 2;

    // Act/Assert: Closed는 Blocked를 통해 우회 개방되지 않는 fail-closed 상태다.
    UFFZSFacilityQueueComponent* Closed = NewObject<UFFZSFacilityQueueComponent>();
    Closed->Configure(Rules);
    Closed->SetBlocked(true);
    TestEqual(TEXT("Closed queue cannot be opened through Blocked"), Closed->GetQueueState(), EFFZSQueueState::Closed);

    UFFZSFacilityQueueComponent* Queue = MakeQueue(Rules);
    Queue->SetBlocked(true);
    // Act/Assert: 큰 Delta도 제한된 두 재경로 사건을 모두 내고 Unblock 시 이전 Open을 복원한다.
    Queue->AdvanceSimulation(12.0f);
    TestEqual(TEXT("Large Delta emits every bounded repath event"), Queue->CaptureSnapshot().RepathAttemptCount, 2);
    Queue->SetBlocked(false);
    TestEqual(TEXT("Unblock restores open state"), Queue->GetQueueState(), EFFZSQueueState::Open);

    FGuid Ticket;
    // Act/Assert: Agent별 세 번째 실패는 추가 재경로 대신 시스템 해제로 전환된다.
    Queue->JoinQueue(TEXT("Move.Fail"), nullptr, Ticket);
    TestTrue(TEXT("First movement failure requests repath"), Queue->ReportAgentMovementFailed(TEXT("Move.Fail")));
    TestTrue(TEXT("Second movement failure requests repath"), Queue->ReportAgentMovementFailed(TEXT("Move.Fail")));
    TestTrue(TEXT("Third movement failure releases Agent"), Queue->ReportAgentMovementFailed(TEXT("Move.Fail")));
    TestFalse(TEXT("Released Agent no longer occupies queue"), Queue->ContainsAgent(TEXT("Move.Fail")));
    return true;
}

#endif
