// FF_FFZSQueueTests.cpp


// Queue FIFO, 서비스, Blocked/Drain/Cancel, Snapshot 복원 계약을 검증한다.
// 흐름: 가짜 Agent와 명시적 step을 사용해 시간·도착·서비스 순서를 결정론적으로 재현한다.
// 불변식: 0초 timeout은 즉시 실행이 아니라 해당 자동 동작 비활성화이며 복원 후 순서·의도를 보존한다.
// 실패/성능: 실제 Nav/프레임 시간에 의존하지 않아 같은 입력에서 같은 결과를 내야 한다.
 

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Misc/AutomationTest.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSQueueComponent.h"

// 테스트 매트릭스
 // FIFOArrivalAndCapacity: 물리 도착 게이트, 선착순, Full/중복 거절
 // DrainAndEmergencyCancel: 정상 배수와 긴급 취소의 서로 다른 종료 정책
 // SaveRestoreMovementState / SaveRestoreTickAndDrain: 이동 상태·Tick 불변식·실패 전 게시 방지
 // EventStepTieAndDeltaIndependence: 동시 사건 우선순위와 Delta 분할 독립성
 // BlockedAndMovementFailure: 큐 전체/Agent별 재경로 예산
 // World 의존성을 제거한 컴포넌트 단위 테스트이며, 물리 도착은 테스트 전용 bridge로만 호출한다.
 
  // 테스트에서만 사용하는 제한된 Bridge다. 운영 변경 Method는 Private으로 유지한다. 
struct FFFZSQueueTestAccess
{
    static void Configure(UFFZSFacilityQueueComponent& Queue, const FFFZSQueueRules& Rules)
    {
        Queue.Configure(Rules);
    }

    static void ConfigureAndOpen(UFFZSFacilityQueueComponent& Queue, const FFFZSQueueRules& Rules)
    {
        Queue.Configure(Rules);
        Queue.OpenQueue();
    }

    static EFFZSQueueJoinResult JoinQueue(
        UFFZSFacilityQueueComponent& Queue,
        const FName AgentId,
        FGuid& OutTicketId)
    {
        return Queue.JoinQueue(AgentId, nullptr, OutTicketId);
    }

    static bool RestoreSnapshot(
        UFFZSFacilityQueueComponent& Queue,
        const FFFZSQueueSnapshot& Snapshot,
        TArray<FText>& OutErrors)
    {
        return Queue.RestoreSnapshot(Snapshot, OutErrors);
    }

    static bool ArriveAtQueueSlot(UFFZSFacilityQueueComponent& Queue, const FName AgentId, const int32 QueueIndex)
    {
        return Queue.ReportAgentArrivedAtQueueSlot(AgentId, Queue.GetQueueSlotWorldLocation(QueueIndex));
    }

    static bool ArriveAtService(UFFZSFacilityQueueComponent& Queue, const FName AgentId, const int32 ServiceIndex)
    {
        return Queue.ReportAgentArrivedAtServicePoint(AgentId, Queue.GetServiceWorldLocation(ServiceIndex));
    }

    static bool ReportQueueArrival(UFFZSFacilityQueueComponent& Queue, const FName AgentId, const FVector Location)
    {
        return Queue.ReportAgentArrivedAtQueueSlot(AgentId, Location);
    }

    static bool ReportServiceArrival(UFFZSFacilityQueueComponent& Queue, const FName AgentId, const FVector Location)
    {
        return Queue.ReportAgentArrivedAtServicePoint(AgentId, Location);
    }

    static bool ReportExitArrival(UFFZSFacilityQueueComponent& Queue, const FName AgentId, const FVector Location)
    {
        return Queue.ReportAgentArrivedAtExit(AgentId, Location);
    }

    static bool ReportMovementFailed(UFFZSFacilityQueueComponent& Queue, const FName AgentId)
    {
        return Queue.ReportAgentMovementFailed(AgentId);
    }

    static void AdvanceSimulation(UFFZSFacilityQueueComponent& Queue, const float DeltaSeconds)
    {
        Queue.AdvanceSimulation(DeltaSeconds);
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
    TestEqual(TEXT("A가 대기열에 참가한다"), Queue->JoinQueue(TEXT("Agent.A"), nullptr, TicketA), EFFZSQueueJoinResult::Joined);
    TestEqual(TEXT("실제 슬롯에 도착하기 전에는 서비스가 시작되지 않는다"), Queue->GetActiveServiceCount(), 0);
    TestTrue(TEXT("A가 FIFO 슬롯에 도착한다"), ArriveAtQueueSlot(*Queue, TEXT("Agent.A"), 0));
    TestEqual(TEXT("A가 서비스에 배정된다"), Queue->GetActiveServiceCount(), 1);
    TestTrue(TEXT("A가 서비스 앵커에 도착한다"), ArriveAtService(*Queue, TEXT("Agent.A"), 0));

    TestEqual(TEXT("B가 대기열에 참가한다"), Queue->JoinQueue(TEXT("Agent.B"), nullptr, TicketB), EFFZSQueueJoinResult::Joined);
    TestEqual(TEXT("C가 대기열에 참가한다"), Queue->JoinQueue(TEXT("Agent.C"), nullptr, TicketC), EFFZSQueueJoinResult::Joined);
    TestTrue(TEXT("B가 대기열 맨 앞에 도착한다"), ArriveAtQueueSlot(*Queue, TEXT("Agent.B"), 0));
    TestTrue(TEXT("C가 두 번째 슬롯에 도착한다"), ArriveAtQueueSlot(*Queue, TEXT("Agent.C"), 1));
    TestEqual(TEXT("대기 목록이 최대 인원에 도달한다"), Queue->GetWaitingCount(), 2);
    TestEqual(TEXT("대기열이 가득 차면 D의 참가가 거부된다"), Queue->JoinQueue(TEXT("Agent.D"), nullptr, TicketD), EFFZSQueueJoinResult::Full);
    TestEqual(TEXT("중복 에이전트가 거부된다"), Queue->JoinQueue(TEXT("Agent.B"), nullptr, TicketD), EFFZSQueueJoinResult::DuplicateAgent);

    // Act/Assert: A 완료 뒤 B만 승격되고, 위치가 당겨진 C는 새 슬롯에 다시 도착해야 한다.
    Queue->AdvanceSimulation(4.0f);
    TestEqual(TEXT("서비스가 정확히 하나 완료된다"), Queue->GetCompletedCount(), 1);
    TestEqual(TEXT("FIFO 순서에 따라 B가 다음 서비스를 시작한다"), Queue->CaptureSnapshot().Entries.FilterByPredicate(
        [](const FFFZSQueueEntrySnapshot& Entry) { return Entry.AgentId == TEXT("Agent.B") && Entry.bInService; }).Num(), 1);
    TestEqual(TEXT("C가 맨 앞으로 이동하여 다시 도착해야 한다"), Queue->CaptureSnapshot().Entries.FilterByPredicate(
        [](const FFFZSQueueEntrySnapshot& Entry) { return Entry.AgentId == TEXT("Agent.C") && !Entry.bArrivedAtAssignedSlot; }).Num(), 1);

    TArray<FText> Errors;
    TestTrue(TEXT("대기열 불변 조건이 유지된다"), Queue->ValidateInvariants(Errors));
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
    TestEqual(TEXT("일반 종료 시 남은 대기열을 모두 처리한다"), Queue->GetQueueState(), EFFZSQueueState::Draining);
    TestEqual(TEXT("배수 중에는 신규 참가가 거부된다"), Queue->JoinQueue(TEXT("Drain.C"), nullptr, Ticket), EFFZSQueueJoinResult::NotOpen);
    Queue->AdvanceSimulation(1.0f);
    TestTrue(TEXT("FIFO 승격 후 B가 서비스 위치에 도착한다"), ArriveAtService(*Queue, TEXT("Drain.B"), 0));
    Queue->AdvanceSimulation(1.0f);
    TestEqual(TEXT("배수가 끝나면 닫힘 상태가 된다"), Queue->GetQueueState(), EFFZSQueueState::Closed);
    TestFalse(TEXT("배수 후 점유자가 남지 않는다"), Queue->IsOccupied());

    Queue->OpenQueue();
    // Act/Assert: 재개 후 긴급 종료는 대기/서비스를 즉시 Cancel 경로로 비운다.
    Queue->JoinQueue(TEXT("Emergency.A"), nullptr, Ticket);
    Queue->JoinQueue(TEXT("Emergency.B"), nullptr, Ticket);
    Queue->EmergencyCancel();
    TestEqual(TEXT("긴급 종료가 즉시 적용된다"), Queue->GetQueueState(), EFFZSQueueState::Closed);
    TestFalse(TEXT("긴급 취소가 모든 점유자를 해제한다"), Queue->IsOccupied());
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
    TestTrue(TEXT("스냅샷이 복원된다"), Restored->RestoreSnapshot(Saved, Errors));
    TestEqual(TEXT("서비스 인원 수가 보존된다"), Restored->GetActiveServiceCount(), Original->GetActiveServiceCount());
    TestEqual(TEXT("대기 인원 수가 보존된다"), Restored->GetWaitingCount(), Original->GetWaitingCount());
    TestEqual(TEXT("이동 도착 상태가 보존된다"), Restored->CaptureSnapshot().Entries[0].bArrivedAtAssignedSlot, Saved.Entries[0].bArrivedAtAssignedSlot);
    TestTrue(TEXT("복원된 대기열의 불변 조건이 유지된다"), Restored->ValidateInvariants(Errors));

    // Act/Assert: 게시 전 구조 검증에서 잡히는 오류는 기존 라이브 큐를 지우지 않아야 한다.
    FFFZSQueueSnapshot Invalid = Saved;
    Invalid.Entries[0].AssignedQueueIndex = 99;
    const int32 BeforeCount = Restored->CaptureSnapshot().Entries.Num();
    TestFalse(TEXT("유효하지 않은 스냅샷이 거부된다"), Restored->RestoreSnapshot(Invalid, Errors));
    TestEqual(TEXT("복원 실패가 현재 대기열을 지우지 않는다"), Restored->CaptureSnapshot().Entries.Num(), BeforeCount);

    const FFFZSQueueSnapshot BeforeDuplicateSequence = Restored->CaptureSnapshot();
    FFFZSQueueSnapshot DuplicateSequence = Saved;
    DuplicateSequence.Entries[1].JoinSequence = DuplicateSequence.Entries[0].JoinSequence;
    TestFalse(TEXT("중복된 JoinSequence는 커밋 전에 거부된다"),
        Restored->RestoreSnapshot(DuplicateSequence, Errors));
    const FFFZSQueueSnapshot AfterDuplicateSequence = Restored->CaptureSnapshot();
    TestEqual(TEXT("중복 시퀀스 실패 후 항목 수가 보존된다"),
        AfterDuplicateSequence.Entries.Num(), BeforeDuplicateSequence.Entries.Num());
    TestEqual(TEXT("중복 시퀀스 실패 후 첫 번째 에이전트가 보존된다"),
        AfterDuplicateSequence.Entries[0].AgentId, BeforeDuplicateSequence.Entries[0].AgentId);
    TestEqual(TEXT("중복 시퀀스 실패 후 대기열 상태가 보존된다"),
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
    TestTrue(TEXT("물리적 출구 상태가 복원된다"), Restored->RestoreSnapshot(ExitingSnapshot, Errors));
    TestEqual(TEXT("복원된 출구가 계속 점유 상태로 유지된다"), Restored->GetExitingCount(), 1);
    TestTrue(TEXT("출구 도착이 복원된 에이전트의 처리를 완료한다"),
        Restored->ReportAgentArrivedAtExit(TEXT("Save.Exiting"), Restored->GetExitWorldLocation()));
    TestFalse(TEXT("최종 출구 도착 후 배수 중인 대기열이 닫힌다"), Restored->IsOccupied());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSQueueSnapshotSafetyLimitsTest,
    "FF_ZoneSystem.Unit.Queue.SnapshotSafetyLimitsAndSequenceExhaustion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueSnapshotSafetyLimitsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FFFZSQueueRules Rules;
    Rules.MaxWaitingAgents = 4;
    Rules.ServiceCapacity = 1;
    UFFZSFacilityQueueComponent* Queue = MakeQueue(Rules);
    FGuid Ticket;
    TestEqual(TEXT("The baseline queue accepts one entry"),
        FFFZSQueueTestAccess::JoinQueue(
            *Queue, TEXT("Snapshot.Safe"), Ticket),
        EFFZSQueueJoinResult::Joined);
    const FFFZSQueueSnapshot Stable = Queue->CaptureSnapshot();

    TArray<FText> Errors;
    const auto ExpectRejectedWithoutMutation =
        [this, Queue, &Stable, &Errors](
            const TCHAR* Label,
            TFunctionRef<void(FFFZSQueueSnapshot&)> Corrupt)
    {
        FFFZSQueueSnapshot Invalid = Stable;
        Corrupt(Invalid);
        TestFalse(*FString::Printf(TEXT("%s is rejected"), Label),
            FFFZSQueueTestAccess::RestoreSnapshot(
                *Queue, Invalid, Errors));

        const FFFZSQueueSnapshot Current = Queue->CaptureSnapshot();
        TestEqual(*FString::Printf(TEXT("%s preserves the entry count"), Label),
            Current.Entries.Num(), Stable.Entries.Num());
        TestEqual(*FString::Printf(TEXT("%s preserves queue state"), Label),
            Current.State, Stable.State);
        TestEqual(*FString::Printf(TEXT("%s preserves the next sequence"), Label),
            Current.NextJoinSequence, Stable.NextJoinSequence);
        TestTrue(*FString::Printf(TEXT("%s preserves the existing agent"), Label),
            Current.Entries.Num() == Stable.Entries.Num() &&
            !Current.Entries.IsEmpty() &&
            Current.Entries[0].AgentId == Stable.Entries[0].AgentId);
    };

    ExpectRejectedWithoutMutation(
        TEXT("A snapshot above the global entry cap"),
        [](FFFZSQueueSnapshot& Snapshot)
        {
            Snapshot.Entries.SetNum(
                UFFZSDeveloperSettings::MaxQueueRuntimeEntries + 1);
        });
    ExpectRejectedWithoutMutation(
        TEXT("MAX_int64 as an entry JoinSequence"),
        [](FFFZSQueueSnapshot& Snapshot)
        {
            Snapshot.Entries[0].JoinSequence = MAX_int64;
        });
    ExpectRejectedWithoutMutation(
        TEXT("NaN blocked elapsed time"),
        [](FFFZSQueueSnapshot& Snapshot)
        {
            Snapshot.BlockedElapsedSeconds =
                std::numeric_limits<float>::quiet_NaN();
        });
    ExpectRejectedWithoutMutation(
        TEXT("NaN entry wait time"),
        [](FFFZSQueueSnapshot& Snapshot)
        {
            Snapshot.Entries[0].WaitSeconds =
                std::numeric_limits<float>::quiet_NaN();
        });

    UFFZSFacilityQueueComponent* NearExhaustion =
        NewObject<UFFZSFacilityQueueComponent>();
    FFFZSQueueTestAccess::Configure(*NearExhaustion, Rules);
    FFFZSQueueSnapshot NearExhaustionSnapshot;
    NearExhaustionSnapshot.State = EFFZSQueueState::Open;
    NearExhaustionSnapshot.StateBeforeBlocked = EFFZSQueueState::Open;
    NearExhaustionSnapshot.NextJoinSequence = MAX_int64 - 1;
    TestTrue(TEXT("The last representable assignable sequence restores"),
        FFFZSQueueTestAccess::RestoreSnapshot(
            *NearExhaustion, NearExhaustionSnapshot, Errors));

    FGuid LastTicket;
    TestEqual(TEXT("The last assignable sequence can be consumed exactly once"),
        FFFZSQueueTestAccess::JoinQueue(
            *NearExhaustion, TEXT("Sequence.Last"), LastTicket),
        EFFZSQueueJoinResult::Joined);
    const FFFZSQueueSnapshot Exhausted = NearExhaustion->CaptureSnapshot();
    TestTrue(TEXT("The final joined entry receives MAX_int64 minus one"),
        Exhausted.Entries.Num() == 1 &&
        Exhausted.Entries[0].JoinSequence == MAX_int64 - 1);
    TestEqual(TEXT("The next-sequence sentinel becomes MAX_int64"),
        Exhausted.NextJoinSequence, static_cast<int64>(MAX_int64));

    FGuid RejectedTicket = FGuid::NewGuid();
    UFFZSFacilityQueueComponent* RestoredExhausted =
        NewObject<UFFZSFacilityQueueComponent>();
    FFFZSQueueTestAccess::Configure(*RestoredExhausted, Rules);
    TestTrue(TEXT("The exhausted sentinel round-trips through restore"),
        FFFZSQueueTestAccess::RestoreSnapshot(
            *RestoredExhausted, Exhausted, Errors));
    TestEqual(TEXT("The restored exhausted queue remains fail-closed"),
        FFFZSQueueTestAccess::JoinQueue(
            *RestoredExhausted, TEXT("Sequence.RestoredRejected"), RejectedTicket),
        EFFZSQueueJoinResult::NotOpen);

    RejectedTicket = FGuid::NewGuid();
    TestEqual(TEXT("A queue with an exhausted sequence space fails closed"),
        FFFZSQueueTestAccess::JoinQueue(
            *NearExhaustion, TEXT("Sequence.Rejected"), RejectedTicket),
        EFFZSQueueJoinResult::NotOpen);
    TestFalse(TEXT("Sequence exhaustion never publishes a ticket"),
        RejectedTicket.IsValid());
    TestEqual(TEXT("Sequence exhaustion does not append a partial entry"),
        NearExhaustion->CaptureSnapshot().Entries.Num(), 1);
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
    TestEqual(TEXT("에이전트가 원본 대기열에 참가한다"),
        ActiveSource->JoinQueue(TEXT("TickRestore.A"), nullptr, Ticket), EFFZSQueueJoinResult::Joined);
    TestTrue(TEXT("에이전트가 원본 대기열 슬롯에 도착한다"),
        ArriveAtQueueSlot(*ActiveSource, TEXT("TickRestore.A"), 0));
    TestTrue(TEXT("에이전트가 원본 서비스 지점에 도착한다"),
        ArriveAtService(*ActiveSource, TEXT("TickRestore.A"), 0));
    ActiveSource->AdvanceSimulation(1.0f);
    const FFFZSQueueSnapshot ActiveSnapshot = ActiveSource->CaptureSnapshot();

    UFFZSFacilityQueueComponent* Restored = NewObject<UFFZSFacilityQueueComponent>();
    Restored->Configure(Rules);
    // Act/Assert: Closed로 생성된 대상이 Active 복원 후 Tick을 켜고, Drain 완료 후 다시 꺼야 한다.
    TestFalse(TEXT("새로 생성된 닫힌 대기열은 틱이 비활성화된 상태로 시작한다"), Restored->IsComponentTickEnabled());

    TArray<FText> Errors;
    TestTrue(TEXT("활성 스냅샷이 복원된다"), Restored->RestoreSnapshot(ActiveSnapshot, Errors));
    TestEqual(TEXT("활성 상태가 복원된다"), Restored->GetQueueState(), EFFZSQueueState::Open);
    TestTrue(TEXT("복원된 활성 대기열이 틱을 활성화한다"), Restored->IsComponentTickEnabled());

    Restored->BeginDrain();
    TestEqual(TEXT("복원된 대기열이 배수 상태로 전환된다"), Restored->GetQueueState(), EFFZSQueueState::Draining);
    TestTrue(TEXT("배수 중에는 틱이 활성 상태로 유지된다"), Restored->IsComponentTickEnabled());
    Restored->AdvanceSimulation(2.0f);
    TestEqual(TEXT("남은 시간이 지나면 복원된 서비스가 완료된다"), Restored->GetCompletedCount(), 1);
    TestEqual(TEXT("배수가 완료되면 복원된 대기열이 닫힌다"), Restored->GetQueueState(), EFFZSQueueState::Closed);
    TestFalse(TEXT("배수 완료 후 틱이 비활성화된다"), Restored->IsComponentTickEnabled());
    TestFalse(TEXT("배수 완료 후 점유자가 남지 않는다"), Restored->IsOccupied());

    // Arrange/Act/Assert: 반대 방향인 Closed Snapshot 복원도 Tick 파생 상태를 수리해야 한다.
    UFFZSFacilityQueueComponent* ClosedSource = NewObject<UFFZSFacilityQueueComponent>();
    ClosedSource->Configure(Rules);
    const FFFZSQueueSnapshot ClosedSnapshot = ClosedSource->CaptureSnapshot();

    Restored->OpenQueue();
    TestTrue(TEXT("다시 연 대기열이 틱을 활성화한다"), Restored->IsComponentTickEnabled());
    TestTrue(TEXT("닫힌 상태의 스냅샷이 복원된다"), Restored->RestoreSnapshot(ClosedSnapshot, Errors));
    TestEqual(TEXT("닫힌 상태가 복원된다"), Restored->GetQueueState(), EFFZSQueueState::Closed);
    TestFalse(TEXT("복원된 닫힌 대기열이 틱을 비활성화한다"), Restored->IsComponentTickEnabled());

    Restored->OpenQueue();
    // Act/Assert: 헤더 단계에서 거절되는 Snapshot은 현재 State와 Tick을 그대로 보존한다.
    const FFFZSQueueSnapshot BeforeInvalidRestore = Restored->CaptureSnapshot();
    FFFZSQueueSnapshot InvalidSnapshot = ClosedSnapshot;
    InvalidSnapshot.CompletedCount = -1;
    TestFalse(TEXT("유효하지 않은 스냅샷이 커밋 전에 거부된다"),
        Restored->RestoreSnapshot(InvalidSnapshot, Errors));
    TestEqual(TEXT("스냅샷이 거부되어도 활성 상태가 보존된다"),
        Restored->GetQueueState(), BeforeInvalidRestore.State);
    TestTrue(TEXT("스냅샷이 거부되어도 활성 틱이 보존된다"), Restored->IsComponentTickEnabled());
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
    TestEqual(TEXT("단일 스텝에서 서비스가 완료된다"), OneStep->GetCompletedCount(), 1);
    TestEqual(TEXT("분할 스텝에서 서비스가 완료된다"), ThreeSteps->GetCompletedCount(), 1);
    TestEqual(TEXT("서비스 완료와 이탈이 동시에 발생하면 서비스 완료가 우선한다"), OneStep->GetAbandonedCount(), 0);
    TestEqual(TEXT("델타를 분할해도 이탈 결과가 보존된다"), OneStep->GetAbandonedCount(), ThreeSteps->GetAbandonedCount());
    TestEqual(TEXT("두 시뮬레이션 모두에서 B가 승격된다"), OneStep->GetActiveServiceCount(), ThreeSteps->GetActiveServiceCount());
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
    TestEqual(TEXT("닫힌 대기열은 차단 상태를 거쳐 열리지 않는다"), Closed->GetQueueState(), EFFZSQueueState::Closed);

    UFFZSFacilityQueueComponent* Queue = MakeQueue(Rules);
    Queue->SetBlocked(true);
    // Act/Assert: 큰 Delta도 제한된 두 재경로 사건을 모두 내고 Unblock 시 이전 Open을 복원한다.
    Queue->AdvanceSimulation(12.0f);
    TestEqual(TEXT("큰 델타에서도 제한된 모든 경로 재탐색 이벤트가 발생한다"), Queue->CaptureSnapshot().RepathAttemptCount, 2);
    Queue->SetBlocked(false);
    TestEqual(TEXT("차단을 해제하면 열림 상태가 복원된다"), Queue->GetQueueState(), EFFZSQueueState::Open);

    FGuid Ticket;
    // Act/Assert: Agent별 세 번째 실패는 추가 재경로 대신 시스템 해제로 전환된다.
    Queue->JoinQueue(TEXT("Move.Fail"), nullptr, Ticket);
    TestTrue(TEXT("첫 번째 이동 실패가 경로 재탐색을 요청한다"), Queue->ReportAgentMovementFailed(TEXT("Move.Fail")));
    TestTrue(TEXT("두 번째 이동 실패가 경로 재탐색을 요청한다"), Queue->ReportAgentMovementFailed(TEXT("Move.Fail")));
    TestTrue(TEXT("세 번째 이동 실패가 에이전트를 해제한다"), Queue->ReportAgentMovementFailed(TEXT("Move.Fail")));
    TestFalse(TEXT("해제된 에이전트가 더 이상 대기열을 점유하지 않는다"), Queue->ContainsAgent(TEXT("Move.Fail")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSQueueNonFiniteArrivalAndRepathBoundaryTest,
    "FF_ZoneSystem.Unit.Queue.NonFiniteArrivalAndRepathBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueNonFiniteArrivalAndRepathBoundaryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FFFZSQueueRules Rules;
    Rules.MaxWaitingAgents = 2;
    Rules.ServiceCapacity = 1;
    Rules.ServiceDurationSeconds = 0.25f;
    Rules.MaxRepathAttempts = 2;
    UFFZSFacilityQueueComponent* Queue = MakeQueue(Rules);

    FGuid Ticket;
    TestEqual(TEXT("The arrival test agent joins"),
        FFFZSQueueTestAccess::JoinQueue(*Queue, TEXT("Arrival.NonFinite"), Ticket),
        EFFZSQueueJoinResult::Joined);
    const FVector NaNLocation(
        std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0);
    const FVector InfiniteLocation(
        std::numeric_limits<double>::infinity(), 0.0, 0.0);
    TestFalse(TEXT("A NaN queue-slot arrival is rejected"),
        FFFZSQueueTestAccess::ReportQueueArrival(*Queue, TEXT("Arrival.NonFinite"), NaNLocation));
    TestTrue(TEXT("A valid queue-slot arrival still succeeds afterwards"),
        ArriveAtQueueSlot(*Queue, TEXT("Arrival.NonFinite"), 0));
    TestFalse(TEXT("An infinite service arrival is rejected"),
        FFFZSQueueTestAccess::ReportServiceArrival(*Queue, TEXT("Arrival.NonFinite"), InfiniteLocation));
    TestTrue(TEXT("A valid service arrival still succeeds afterwards"),
        ArriveAtService(*Queue, TEXT("Arrival.NonFinite"), 0));
    FFFZSQueueTestAccess::AdvanceSimulation(*Queue, Rules.ServiceDurationSeconds);
    TestEqual(TEXT("A null test agent completes without a physical exit callback"),
        Queue->GetCompletedCount(), 1);

    UFFZSFacilityQueueComponent* ExitQueue = NewObject<UFFZSFacilityQueueComponent>();
    FFFZSQueueTestAccess::Configure(*ExitQueue, Rules);
    FFFZSQueueSnapshot ExitSnapshot;
    ExitSnapshot.State = EFFZSQueueState::Open;
    ExitSnapshot.StateBeforeBlocked = EFFZSQueueState::Open;
    FFFZSQueueEntrySnapshot& ExitEntry = ExitSnapshot.Entries.AddDefaulted_GetRef();
    ExitEntry.TicketId = FGuid::NewGuid();
    ExitEntry.AgentId = TEXT("Exit.NonFinite");
    ExitEntry.JoinSequence = 1;
    ExitEntry.bExiting = true;
    ExitEntry.ExitIndex = 0;
    ExitEntry.PendingReleaseReason = EFFZSQueueReleaseReason::ServiceCompleted;
    TArray<FText> ExitErrors;
    TestTrue(TEXT("A physical exit state restores for arrival validation"),
        FFFZSQueueTestAccess::RestoreSnapshot(*ExitQueue, ExitSnapshot, ExitErrors));
    TestFalse(TEXT("A NaN exit arrival is rejected"),
        FFFZSQueueTestAccess::ReportExitArrival(*ExitQueue, TEXT("Exit.NonFinite"), NaNLocation));
    TestEqual(TEXT("A rejected exit report preserves the exiting agent"),
        ExitQueue->GetExitingCount(), 1);

    UFFZSFacilityQueueComponent* RepathQueue = MakeQueue(Rules);
    TestEqual(TEXT("The repath-boundary agent joins"),
        FFFZSQueueTestAccess::JoinQueue(*RepathQueue, TEXT("Repath.Boundary"), Ticket),
        EFFZSQueueJoinResult::Joined);
    FFFZSQueueSnapshot Boundary = RepathQueue->CaptureSnapshot();
    Boundary.Entries[0].MovementRepathCount = Rules.MaxRepathAttempts;
    TArray<FText> Errors;
    TestTrue(TEXT("A snapshot exactly at the repath limit restores"),
        FFFZSQueueTestAccess::RestoreSnapshot(*RepathQueue, Boundary, Errors));
    TestTrue(TEXT("The next movement failure releases without increment overflow"),
        FFFZSQueueTestAccess::ReportMovementFailed(*RepathQueue, TEXT("Repath.Boundary")));
    TestFalse(TEXT("The repath-boundary agent is released fail-closed"),
        RepathQueue->ContainsAgent(TEXT("Repath.Boundary")));
    return true;
}

#endif
