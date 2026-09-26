// FF_FFZSQueueComponent.h

// 시설 하나의 결정적 FIFO 대기·서비스·출구 이동 상태를 시뮬레이션.
// 흐름: Waiting → ServiceAssigned → 도착 보고 → 시간 서비스 → Exiting → 출구 도착 보고 → Release로 진행.
// 불변식: AgentId·TicketId·JoinSequence는 모든 배열에서 유일하고 Closed는 비어 있으며 Exiting도 전역 소유권을 유지.
// 실패/성능: 변경 API는 전역 중복을 막기 위해 서브시스템에만 열려 있고 비용은 이벤트 단계와 비-Closed Tick의 항목 수에 비례.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSQueueComponent.generated.h"

class AActor;
class AFFZSFacilityActor;
class UFFZSWorldSubsystem;
#if WITH_DEV_AUTOMATION_TESTS
struct FFFZSQueueTestAccess;
class FFFZSQueueFifoTest;
class FFFZSQueueClosePoliciesTest;
class FFFZSQueueSnapshotTest;
class FFFZSQueueSnapshotTickTest;
class FFFZSQueueEventStepTest;
class FFFZSQueueBlockedAndMovementFailureTest;
#endif

// 참가 확정 직후 안정 AgentId·TicketId·현재 슬롯 인덱스를 방송.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFFZSQueueJoinedSignature, FName, AgentId, FGuid, TicketId, int32, QueueIndex);
// 빈 서비스 슬롯을 FIFO 선두에게 배정했음을 방송한다. 서비스 시작 이벤트와 구분. 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFFZSServiceAssignedSignature, FName, AgentId, FGuid, TicketId, int32, ServiceIndex);
// 지정 서비스 지점 도착 보고가 허용된 뒤 실제 시간 서비스가 시작됐음을 방송.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFFZSServiceStartedSignature, FName, AgentId, FGuid, TicketId, int32, ServiceIndex);
// 출구 도착 또는 즉시 취소로 최종 소유권을 해제한 뒤 이유와 함께 방송.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FFFZSQueueReleasedSignature, FName, AgentId, FGuid, TicketId, EFFZSQueueReleaseReason, Reason);
// 큐 상태가 실제로 달라질 때 이전·새 상태를 한 번 방송.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFFZSQueueStateChangedSignature, EFFZSQueueState, PreviousState, EFFZSQueueState, NewState);
// Blocked 동안 재경로 시도 번호와 최대 횟수를 외부 Navigation 어댑터에 알린다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFFZSQueueRepathRequestedSignature, int32, AttemptNumber, int32, MaximumAttempts);

// 시설 하나의 결정적 FIFO 대기열·서비스 시뮬레이션.
// 상태 흐름은 Open/Full → Draining 또는 Blocked → Closed이며, 항목 흐름은 Waiting → ActiveServices
// → Exiting → 최종 해제다. 전역 AgentId 유일성을 지키려면 변경은 반드시 UFFZSWorldSubsystem을 통한다.

UCLASS(ClassGroup = (FFZoneSystem), meta = (BlueprintSpawnableComponent))
class FF_ZONESYSTEM_API UFFZSFacilityQueueComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFFZSFacilityQueueComponent();
    // Tick은 기본 비활성으로 시작하고 Configure·OpenQueue 이후 필요한 상태에서만 시뮬레이션한다. 

    UPROPERTY(BlueprintAssignable, Category = "FF Zone System|Queue")
    FFFZSQueueJoinedSignature OnAgentJoined;
    // 대기열 참가 확정 이벤트. UI·AI는 이를 관찰하되 권한 상태를 직접 바꾸지 않는다. 

    UPROPERTY(BlueprintAssignable, Category = "FF Zone System|Queue")
    FFFZSServiceAssignedSignature OnServiceAssigned;
    // 서비스 앵커 이동 요청 시점 이벤트.

    UPROPERTY(BlueprintAssignable, Category = "FF Zone System|Queue")
    FFFZSServiceStartedSignature OnServiceStarted;
    // 서비스 타이머 시작 시점 이벤트.

    UPROPERTY(BlueprintAssignable, Category = "FF Zone System|Queue")
    FFFZSQueueReleasedSignature OnAgentReleased;
    // 최종 해제 이벤트. WorldSubsystem이 이 이벤트로 전역 Agent 소유권을 지운다. 

    UPROPERTY(BlueprintAssignable, Category = "FF Zone System|Queue")
    FFFZSQueueStateChangedSignature OnQueueStateChanged;
    // Open·Full·Draining·Closed·Blocked 전환 관찰 이벤트다. 
    
    UPROPERTY(BlueprintAssignable, Category = "FF Zone System|Queue")
    FFFZSQueueRepathRequestedSignature OnRepathRequested;
    // 이동 장애 회복을 외부 경로 시스템에 요청하는 이벤트다. 



    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    EFFZSQueueState GetQueueState() const { return QueueState; }
    // 현재 참가 허용과 시뮬레이션 동작을 결정하는 큐 상태를 반환한다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    int32 GetWaitingCount() const { return Waiting.Num(); }
    // 서비스 전 Waiting 항목 수를 반환한다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    int32 GetActiveServiceCount() const { return ActiveServices.Num(); }
    // 서비스 배정 또는 진행 중인 항목 수를 반환한다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    int32 GetExitingCount() const { return Exiting.Num(); }
    // 출구로 이동 중이며 아직 전역 소유권이 남은 항목 수를 반환한다. 

    
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    bool IsOccupied() const { return !Waiting.IsEmpty() || !ActiveServices.IsEmpty() || !Exiting.IsEmpty(); }
    // Waiting·ActiveServices·Exiting 중 하나라도 있으면 true다. Exiting도 Exit 도착 전까지 이동·제거를 막는다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    bool ContainsAgent(FName AgentId) const;
    // 세 런타임 배열 어디든 AgentId가 있으면 true다. NAME_None은 false다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    FVector GetQueueSlotWorldLocation(int32 QueueIndex) const;
    // queue spline을 서비스 지점부터 SlotSpacingCm 간격으로 평가한 월드 위치를 반환한다. 범위 밖 입력은 안전한 대체 위치를 사용한다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    FVector GetServiceWorldLocation(int32 ServiceIndex) const;
    // Service 타입과 인덱스 앵커의 월드 위치를 반환한다. 찾지 못하면 시설 위치를 대체값으로 사용한다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    FVector GetExitWorldLocation(int32 ExitIndex = 0) const;
    // Exit 타입과 인덱스 앵커의 월드 위치를 반환한다. 찾지 못하면 시설 위치를 대체값으로 사용한다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    FFFZSQueueSnapshot CaptureSnapshot() const;
    // UObject 포인터를 제외한 큐 상태·항목·통계·drain/blocked 의도를 값 스냅샷으로 복사한다. 


    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    bool ValidateInvariants(TArray<FText>& OutErrors) const;
    // AgentId·TicketId·JoinSequence·서비스 인덱스 유일성, 상태와 배열의 일관성을 검사해 모든 오류를 누적한다.
    // 읽기 전용 진단이며 오류가 없을 때 true다.
    
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    const FFFZSQueueRules& GetRules() const { return Rules; }
    // 시설 사양에서 복사된 현재 큐 규칙을 읽기 전용으로 반환한다.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    int32 GetCompletedCount() const { return CompletedCount; }
    // 서비스 완료 이유로 최종 해제된 누적 수를 반환한다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    int32 GetAbandonedCount() const { return AbandonedCount; }
    // 대기 포기 이유로 최종 해제된 누적 수를 반환한다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    float GetAverageCompletedWaitSeconds() const;
    // 완료 수가 0이면 0, 아니면 완료 항목의 누적 대기 시간/완료 수를 반환.

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    // 비-Closed 상태에서 설정 간격만큼 이벤트 단위 시뮬레이션을 전진한다. 비용은 활성 항목 수와 발생 이벤트 수에 비례.

private:
    friend class AFFZSFacilityActor;
    friend class UFFZSWorldSubsystem;
#if WITH_DEV_AUTOMATION_TESTS
    friend struct FFFZSQueueTestAccess;
    friend class FFFZSQueueFifoTest;
    friend class FFFZSQueueClosePoliciesTest;
    friend class FFFZSQueueSnapshotTest;
    friend class FFFZSQueueSnapshotTickTest;
    friend class FFFZSQueueEventStepTest;
    friend class FFFZSQueueBlockedAndMovementFailureTest;
#endif

    // 권한 변경은 private이다. WorldSubsystem이 시설 상태·전역 AgentId 소유권과 함께 직렬화해야
    // 중복 참가와 콜백 재진입 불일치를 막을 수 있다.
    
    void Configure(const FFFZSQueueRules& NewRules);
    // 검증된 시설 규칙을 복사하고 내부 시간·상태 기준을 준비한다. 점유 중 재구성은 상위 수명주기가 막는다. 
    
    void OpenQueue();
    // Closed·Draining 등에서 참가 가능한 Open/Full 상태로 갱신하고 서비스 슬롯을 채운다. 
    
    void BeginDrain();
    // 새 참가를 막고 기존 Waiting·Active·Exiting이 자연스럽게 끝나면 Closed로 전환한다.
    
    void EmergencyCancel();
    // 모든 항목을 즉시 취소 이유로 해제하고 Closed로 전환한다. 동기 콜백 재진입을 고려해 상태를 먼저 정리한다. 
    
    void SetBlocked(bool bBlocked);
    // 차단 시 이전 상태를 저장하고 Blocked로 전환한다. 해제 시 저장 상태를 복구하고 수용량을 다시 계산한다.
    
    EFFZSQueueJoinResult JoinQueue(FName AgentId, AActor* AgentActor, FGuid& OutTicketId);
    // Open이고 정원 여유가 있으며 ID가 유효·비중복일 때 새 Ticket과 JoinSequence를 만들고 참가시킨다.
    bool LeaveQueue(FName AgentId, EFFZSQueueReleaseReason Reason = EFFZSQueueReleaseReason::RemovedBySystem);
    // 항목 위치에 맞춰 출구 이동 또는 최종 해제를 시작한다. 존재하지 않는 ID면 false다.
    
    void AdvanceSimulation(float DeltaSeconds);
    // 대기·서비스·차단 타이머를 다음 의미 있는 이벤트까지 결정적으로 진행한다. 음수 시간은 전진하지 않는다. 
    
    bool ReportAgentArrivedAtQueueSlot(FName AgentId, FVector ActualWorldLocation);
    // 지정 슬롯과 허용 오차 안의 도착만 기록한다. 잘못된 단계·위치는 false다.

    bool ReportAgentArrivedAtServicePoint(FName AgentId, FVector ActualWorldLocation);
    // 지정 Service 앵커 도착을 검증하고 그때 서비스 타이머와 시작 콜백을 활성화.
    
    bool ReportAgentArrivedAtExit(FName AgentId, FVector ActualWorldLocation);
    // Exiting 항목의 Exit 도착을 검증하고 최종 해제·통계·소유권 콜백을 수행.
    
    bool ReportAgentMovementFailed(FName AgentId);
    // 현재 이동 실패를 Blocked 회복 흐름에 전달한다. 재경로 한도를 넘으면 규칙에 따라 해제.
    
    bool RebindAgentActor(FName AgentId, AActor* AgentActor);
    // 로드 후 같은 안정 AgentId를 반환하는 인터페이스 액터를 약한 참조로 다시 연결.
    
    // 구조·중복·용량 검사를 임시 배열에서 통과한 뒤 런타임 배열을 게시하고 최종 불변식을 검사.
    // 게시 후 최종 검사 실패는 이 컴포넌트만으로 이전 상태를 롤백하지 않는다.
    //월드 복원은 임시 시설 액터에서 호출하고 실패 액터를 폐기해 전체 원자성을 제공.
     
    bool RestoreSnapshot(const FFFZSQueueSnapshot& Snapshot, TArray<FText>& OutErrors);
    
    // Waiting·ActiveServices·Exiting 사이를 이동하는 내부 항목.
    //동일 AgentId·TicketId·JoinSequence는 세 배열 전체에서 정확히 한 번만 존재해야 한다.
    
    struct FRuntimeEntry
    {
        FGuid TicketId;
        FName AgentId = NAME_None;
        TWeakObjectPtr<AActor> AgentActor;
        // AI 액터 수명을 소유하지 않는 선택적 바인딩. 스냅샷에는 저장하지 않고 로드 후 재연결
        
        float WaitSeconds = 0.0f;
        float ServiceElapsedSeconds = 0.0f;
        int64 JoinSequence = 0;
        bool bArrivedAtAssignedSlot = false;
        // 현재 QueueIndex 슬롯 도착을 검증했는지 나타낸다. 인덱스 변경 시 다시 false다. 
        
        bool bArrivedAtServicePoint = false;
        // ServiceIndex 도착 뒤 true이며 서비스 시간 누적의 전제다.
        
        bool bExiting = false;
        // Exit 이동 단계이며 true인 동안 전역 대기열 소유권이 유지된다.
        int32 MovementRepathCount = 0;
        int32 AssignedQueueIndex = INDEX_NONE;
        int32 ServiceIndex = INDEX_NONE;
        int32 ExitIndex = INDEX_NONE;
       
        EFFZSQueueReleaseReason PendingReleaseReason = EFFZSQueueReleaseReason::RemovedBySystem;
        // Exit 도착 뒤 최종 방송·통계에 사용할 예약 해제 이유다. 
    };

    UPROPERTY(VisibleAnywhere, Category = "Queue")
    FFFZSQueueRules Rules;

    UPROPERTY(VisibleAnywhere, Category = "Queue")
    EFFZSQueueState QueueState = EFFZSQueueState::Closed;
    
    EFFZSQueueState StateBeforeBlocked = EFFZSQueueState::Closed;
    // 일시 Blocked가 해제될 때 복원할 의미 상태다. 

    TArray<FRuntimeEntry> Waiting;
    // JoinSequence 오름차순 FIFO 대기 항목이다. 

    TArray<FRuntimeEntry> ActiveServices;
    // 유일한 ServiceIndex를 배정받은 이동 또는 서비스 중 항목이다. 

    TArray<FRuntimeEntry> Exiting;
    // ExitIndex로 이동 중이며 최종 해제를 기다리는 항목이다. 

    int64 NextJoinSequence = 1;
    // 다음 참가에 부여할 단조 증가 FIFO 순서다. 복원 후 기존 최대값보다 커야 한다. 
    int32 CompletedCount = 0;
    int32 AbandonedCount = 0;
    float TotalCompletedWaitSeconds = 0.0f;
    
    bool bDrainRequested = false;
    // 기존 이용자가 모두 빠진 즉시 Closed가 되어야 하는 일반 폐장 의도다.

    float BlockedElapsedSeconds = 0.0f;
    // 현재 차단 상태에서 누적한 시간이다. 해제·복원 규칙의 입력이다. 

    int32 RepathAttemptCount = 0;
    // 현재 차단 구간에서 요청한 재경로 횟수다. MaxRepathAttempts를 넘지 않는다. 

    void SetQueueState(EFFZSQueueState NewState);
    void RefreshOpenOrFullState();
    void FillServiceSlots();
    FGuid GetOwnerFacilityId() const;
    void NotifyCurrentMovementTarget(const FRuntimeEntry& Entry);
    void BeginExit(FRuntimeEntry Entry, EFFZSQueueReleaseReason Reason);
    void FinalizeRelease(const FRuntimeEntry& Entry);
    void NotifyQueueIndices();
};
