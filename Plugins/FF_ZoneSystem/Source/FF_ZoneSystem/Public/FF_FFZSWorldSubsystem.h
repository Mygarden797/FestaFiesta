// FF_FFZSWorldSubsystem.h

// 월드별 시설·존·점유·대기열 소유권·정책 트랜잭션·저장 상태의 단일 권한 경계다.
// 흐름: 명령 검증 → 정책 사전 검증 → 임시 적용 → 로컬 변경 → 정책 확정 → 이벤트 발행 순서로 처리.
// 불변식: 게임 스레드에서 한 번에 하나의 명령·복원·정책 콜백만 수행하고 모든 권한 변경은 이 서브시스템을 통과.
// 실패/성능: 실패는 점유와 정책을 원복하며, 기하 비용은 개별 1 cm 셀이 아니라 압축 사각형×존 수에 비례.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSWorldSubsystem.generated.h"

class AActor;
class AFFZSFacilityActor;
class AFFZSGameplayZone;
class AFFZSProtectedArea;

// 시설 한 개의 ZoneId와 서로 겹치지 않는 예약 사각형을 묶은 권위 점유 레코드.
// GUID별 한 레코드이므로 메모리는 물리 셀 수가 아니라 시설 수와 사각형 수에 비례.

struct FFFZSFacilityRegionOccupancy
{
    FName ZoneId = NAME_None;
    TArray<FFFZSCellRect> Regions;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFFZSWorldChangedSignature, const FFFZSWorldChangeEvent&, Event);
// 성공한 권한 명령이 확정되고 WorldRevision이 증가한 뒤 방송하는 불변 이벤트.

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFFZSWorldSnapshotRestoredSignature);
// 원자적 스냅샷 복원이 끝나 시설 Actor 레지스트리가 한 번에 교체됐음을 알리는 수명주기 이벤트.


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFFZSPlacementRejectedSignature, const FFFZSCommand&, Command, const FFFZSCommandResult&, Result);
// 명령이 실패 폐쇄됐을 때 원본 명령과 실패 결과를 방송한다. 상태 변경 성공 이벤트와 상호 배타적.


// 월드별 권한 레지스트리·격자 점유·명령 트랜잭션·대기열 소유권·저장·읽기 모델 경계.
// 모든 변경은 게임 스레드에서 직렬화되고 외부 정책 콜백·명령 실행·스냅샷 복원 중 재진입은 fail-closed한다.
//액터와 컴포넌트의 private 변경 API를 조정해 점유 맵, 영수증, 정책, 이벤트가 서로 어긋나지 않게 한다.

UCLASS()
class FF_ZONESYSTEM_API UFFZSWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    // InitialPhase 설정을 읽고 이미 월드에 존재하는 존·보호 영역·유효 시설을 등록한다. 월드당 한 번 호출.
    
    virtual void Deinitialize() override;
    //모든 약한 레지스트리·점유·소유권·영수증을 비운다. 외부 액터 수명은 소유하지 않는다.
    
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    // Game·PIE·Editor·EditorPreview·GamePreview 월드에서만 서브시스템 생성을 허용한다
    
    UPROPERTY(BlueprintAssignable, Category = "FF Zone System|Events")
    FFFZSWorldChangedSignature OnWorldChanged;
    // 확정된 Install·Move·Remove·수명주기 변경을 UI·외부 시스템이 관찰하는 이벤트다.

    UPROPERTY(BlueprintAssignable, Category = "FF Zone System|Events")
    FFFZSWorldSnapshotRestoredSignature OnWorldSnapshotRestored;
    // 명령 이벤트로 표현할 수 없는 전체 시설 레지스트리 복원이 끝난 뒤 한 번 방송한다.
    
    UPROPERTY(BlueprintAssignable, Category = "FF Zone System|Events")
    FFFZSPlacementRejectedSignature OnPlacementRejected;
    // 검증·정책·단계·멱등성 실패를 UI가 설명할 수 있도록 제공하는 이벤트다.

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Registry")
    bool RegisterZone(AFFZSGameplayZone* Zone);
    // 유효한 ZoneId와 전체 존 데이터 계약을 검사해 유일 키로 등록한다. 재진입·중복·무효 데이터면 false다.
    
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Registry")
    bool UnregisterZone(AFFZSGameplayZone* Zone);
    // 같은 포인터로 등록된 존만 제거한다. 해당 존의 시설은 자동 삭제하거나 이동하지 않는다. 
    
    bool RegisterFacility(AFFZSFacilityActor* Facility);
    // 내부 생성·복원 흐름에서 유효 GUID 시설을 등록하고 예약 셀·Agent 소유권을 재구성한다. 일반 게임 코드용 API가 아니다.
    


    void UnregisterFacility(AFFZSFacilityActor* Facility);
    //같은 시설 포인터의 레지스트리·점유·대기열 소유권 연결을 정리한다. 권한 제거 명령을 대신하지 않는다.
    

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Registry")
    bool RegisterProtectedArea(AFFZSProtectedArea* ProtectedArea);
    // 제작 데이터가 유효한 보호 영역을 중복 없이 등록한다. 실패하면 배치 검증 범위에 포함되지 않는다.
    
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Registry")
    bool UnregisterProtectedArea(AFFZSProtectedArea* ProtectedArea);
    // 일치하는 보호 영역 약한 참조를 제거하고 실제 제거 여부를 반환한다
    

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Registry")
    AFFZSGameplayZone* FindZone(FName ZoneId) const;
    // 현재 유효한 등록 존을 반환한다. 키가 없거나 액터가 소멸했으면 nullptr.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Registry")
    bool HasRegisteredZones() const;
    // 전체 시설 읽기 모델을 만들지 않고 유효한 등록 Zone이 하나라도 있는지 확인한다.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Registry")
    AFFZSFacilityActor* FindFacility(FGuid FacilityInstanceId) const;
    // 현재 유효한 등록 시설을 반환한다. 잘못된 GUID나 소멸한 액터면 nullptr. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Registry")
    AFFZSFacilityActor* FindFacilityAtWorldPoint(
        const FVector& WorldPoint,
        FName ZoneId = NAME_None) const;
    // 등록된 시설 중 월드 지점이 footprint 안에 있는 가장 가까운 시설을 반환한다.
    // ZoneId가 비어 있으면 현재 등록 Zone 경계에서 해석하며, 같은 거리에서는 InstanceId로 결정한다.

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Workflow")
    bool SetChangePhase(EFFZSChangePhase NewPhase);
    // 재진입이 아니고 enum 값이 유효할 때 운영 단계를 교체한다. 이 설정 자체는 WorldRevision을 증가시키지 않는다.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Workflow")
    EFFZSChangePhase GetChangePhase() const { return CurrentPhase; }
    // 현재 명령 허용 규칙에 쓰는 운영 단계를 반환.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Workflow")
    int64 GetWorldRevision() const { return WorldRevision; }
    // 성공한 권한 명령마다 증가하는 낙관적 동시성 리비전을 반환.
    
    // 권한 상태를 바꾸지 않고 로컬 데이터·기하·점유·지면·충돌·내비·보호 영역을 검증.
    // 로컬 검증이 성공한 경우에만 등록 정책 스냅샷의 PreValidate를 호출.
    // @note 결과는 호출 시점 스냅샷이므로 ExecuteCommand가 최신 상태에서 다시 검증한.

    FFFZSPlacementValidation ValidatePlacement(const FFFZSCommand& Command) const;
     // Install·Move·Remove·Open·Close·EmergencyClose의 유일한 공개 변경 경로.
     // CommandId 지문 멱등성, ExpectedWorldRevision, 단계, 로컬 검증, 정책 트랜잭션을 적용한다.
     //실패하면 권한 상태를 확정하지 않고 FFFZSCommandResult로 이유를 반환한다.
    
    FFFZSCommandResult ExecuteCommand(const FFFZSCommand& Command);

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Policy")
    bool RegisterExternalPolicy(UObject* PolicyProvider);
    // 인터페이스·비어 있지 않은 유일 이름·도메인 규칙을 검사해 정책을 등록. 콜백/명령 중에는 false다.

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Policy")
    bool UnregisterExternalPolicy(UObject* PolicyProvider);
    // 명령·복원·정책 콜백 밖에서 일치 제공자를 제거한다. 진행 중 명령은 시작 시 정책 스냅샷을 유지한다. 
    
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Policy")
    bool ValidateIntegrationReadiness(TArray<FText>& OutErrors, bool bRequireAllProductionDomains = true) const;
    // 정책 이름·기본 도메인 유일성을 검사하고 요청 시 Navigation부터 Experience까지 6개 운영 어댑터 존재를 확인한다. 모든 오류를 누적한다.

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Queue")
    EFFZSQueueJoinResult JoinFacilityQueue(FGuid FacilityInstanceId, FName AgentId, AActor* AgentActor, FGuid& OutTicketId);
    // 전역 AgentId 소유권을 먼저 예약한 뒤 활성 시설 큐에 참가시킨다.
    // AgentActor가 있으면 IFFZSQueueAgent의 ID가 입력과 같아야 하며 실패 시 OutTicketId는 무효다.
    
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Queue")
    bool SetFacilityQueueBlocked(FGuid FacilityInstanceId, bool bBlocked);
    // Navigation·Safety 어댑터가 활성 시설 접근 차단을 설정하는 권한 진입점. 성공 시 큐의 실제 Blocked 여부가 요청과 일치.

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Queue")
    EFFZSQueueJoinResult JoinFacilityQueueByActor(FGuid FacilityInstanceId, AActor* AgentActor, FGuid& OutTicketId);
    // 인터페이스 액터에서 안정 AgentId를 읽어 JoinFacilityQueue를 호출한다. 무효·미구현 액터는 InvalidAgent다.

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Queue")
    bool LeaveCurrentFacilityQueue(FName AgentId, EFFZSQueueReleaseReason Reason = EFFZSQueueReleaseReason::RemovedBySystem);
    // 전역 소유권으로 현재 시설을 찾아 이탈을 시작한다. 물리 에이전트는 Exit 도착 전까지 소유권을 유지할 수 있다.

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Queue")
    bool ReportQueueSlotArrival(FName AgentId, FVector ActualWorldLocation);
    // 현재 배정 queue 슬롯과 허용 오차를 큐에 검증시킨다. 잘못된 단계·시설·위치면 false다.

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Queue")
    bool ReportServicePointArrival(FName AgentId, FVector ActualWorldLocation);
    // 배정 Service 앵커 도착을 검증해 실제 서비스 타이머를 시작한다. 단순 서비스 배정만으로는 시작되지 않는다
    
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Queue")
    bool ReportExitPointArrival(FName AgentId, FVector ActualWorldLocation);
    // Exiting 에이전트가 지정 Exit 앵커에 도착했을 때만 최종 해제하고 전역 대기열 소유권을 제거한다.

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Queue")
    bool ReportQueueMovementFailed(FName AgentId);
    // 현재 이동 실패를 해당 큐의 차단·재경로·해제 규칙에 전달한다. 소유권이 없으면 false다.

    
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Queue")
    bool RebindQueueAgentActor(FName AgentId, AActor* AgentActor);
    // 로드 후 약한 액터 연결을 복원한다. AgentActor는 IFFZSQueueAgent를 구현하고 같은 안정 AgentId를 반환해야 한다

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Queue")
    FGuid FindAgentQueueFacility(FName AgentId) const;
    // AgentId가 Waiting·Active·Exiting으로 소유한 시설 GUID를 반환한다. 없으면 무효 GUID다.
    
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Zone Membership")
    FName ResolveZoneAtWorldLocation(const FVector& WorldLocation, FName PreviousZoneId = NAME_None) const;
    //이전 존이 있고 점이 설정 가능한 히스테리시스 여유 안에 있으면 그 존을 유지한다.
    // 아니면 ZoneId 사전순으로 정확한 경계를 검사하며 어느 존에도 없으면 NAME_None이다.
    
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Zone")
    void ReportZonePopulation(FName ZoneId, int32 Population, float DeltaSeconds);
    // 외부 시뮬레이션의 인구 표본을 존에 전달한다. 음수 값은 존에서 0으로 보정하고 무효 ID는 무시.
    
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Read Model")
    FFFZSReadModel BuildReadModel() const;
    // WorldRevision과 유효 존·시설 요약을 ID 기준으로 결정적 정렬한 값 복사본으로 반환.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Save")
    FFFZSWorldSnapshot CaptureWorldSnapshot() const;
    //Schema 4의 1 cm 압축 점유 계약·운영 단계·리비전·시설/큐·완료 영수증을 결정적 순서로 캡처.
    //제작 존·보호 영역·외부 정책·외부 시스템 저장 데이터는 포함하지 않는다.
    
    // Schema 4를 검증하고, 알려진 100 cm Schema 3은 안전 검사 후 메모리상 Schema 4로 변환한 뒤 시설과 운영 상태를 교체한다.
    // 제작 존·보호 영역·등록 정책은 유지하며 게임 스레드 밖이나 명령·정책 콜백 중에는 실패한다.
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Save")
    bool RestoreWorldSnapshot(const FFFZSWorldSnapshot& Snapshot, TArray<FText>& OutErrors);

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Validation")
    bool ValidateZoneGraph(TArray<FText>& OutErrors) const;
    // 프로젝트 ID 목록, 각 존 데이터, 이웃 존재·대칭성, 보호 영역 LinkedZone을 검사해 모든 오류를 누적.
    
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Validation")
    int32 RevalidateFacilitiesInZone(FName ZoneId);
    // GIS·콘텐츠 경계 변경 뒤 해당 존 시설을 현재 규칙으로 다시 검증한다. 인스턴스는 보존하고 실패 항목만 NeedsRelocation으로 표시.

private:
    TMap<FName, TWeakObjectPtr<AFFZSGameplayZone>> ZonesById;
    // 안정 ZoneId → 제작 존 약한 참조의 권한 레지스트리다.
    
    TMap<FGuid, TWeakObjectPtr<AFFZSFacilityActor>> FacilitiesById;
    //FacilityInstanceId → 플러그인 시설 약한 참조의 권한 레지스트리다
    
    TMap<FGuid, FFFZSFacilityRegionOccupancy> OccupiedFacilityRegions;
    //Facility GUID → ZoneId+압축 예약 영역 점유 인덱스. 사양과 배치에서 언제든 재구성 가능해야 한다.
    
    TArray<TWeakObjectPtr<AFFZSProtectedArea>> ProtectedAreas;
    //배치 박스 교차 검사 대상의 비소유 약한 목록이다
    
    TArray<TWeakObjectPtr<UObject>> ExternalPolicies;
    // 등록 순서가 트랜잭션 적용 순서인 정책 제공자 약한 목록이다. 명령마다 스냅샷한다.

    TMap<FName, FGuid> AgentQueueOwnership;
    // Waiting·Active·Exiting 전체에서 AgentId 하나가 소유할 수 있는 시설 하나를 기록
    
    TMap<FGuid, FFFZSCommandReceipt> CommandReceipts;
    // CommandId → 전체 지문과 결과의 멱등성 영수증이다. 동일 ID·다른 지문은 거부한다.
    
    TSet<FGuid> CommandReceiptFacilityReferences;
    // 저장할 성공 결과가 참조하는 시설 GUID 집합으로 제거 뒤 UObject 포인터 재연결 판단에 쓴다.
    
    int64 WorldRevision = 0;
    // 성공한 권한 명령 확정 횟수. 실패·미리보기·단계 변경은 증가시키지 않는다.

    EFFZSChangePhase CurrentPhase = EFFZSChangePhase::InitialSetup;
    // Install·Move·Remove 허용 시기를 제한하고 스냅샷에 저장되는 운영 단계다. 
    
    bool bIsRestoringSnapshot = false;
    // 전체 복원 중 다른 권한 변경을 차단하는 범위 가드다. 
    
    mutable bool bIsInvokingExternalPolicy = false;
    // const PreValidate를 포함한 정책 콜백 재진입과 레지스트리 변경을 차단하는 범위 가드다
    
    FGuid InFlightCommandId;
    // 현재 실행 중 CommandId. 유효한 동안 공개 권한 변경은 fail-closed한다. 

    FString InFlightCommandFingerprint;
    // 재진입 동일 ID의 페이로드 불일치를 판별하는 현재 명령 전체 지문이다.

    TOptional<FFFZSWorldChangeEvent> DeferredWorldChangeEvent;
    // 정책 Commit이 끝날 때까지 방송을 미루는 성공 이벤트다. 외부에서 부분 상태를 관찰하지 못하게 한다. 
    
    bool IsAuthorityMutationBlocked() const;
    // 게임 스레드 밖·복원·정책 콜백·명령 실행 중이면 true인 공통 fail-closed 차단기다. 

    FFFZSPlacementValidation ValidatePlacementLocal(const FFFZSCommand& Command, const AFFZSFacilityActor* IgnoreFacility) const;
    // 외부 정책 없이 플러그인 소유 데이터·기하·점유·월드 검사만 수행한다. Move는 기존 시설 점유를 IgnoreFacility로 제외한다. 
    FTransform BuildPlacementTransform(const AFFZSGameplayZone& Zone, const FFFZSFacilitySpec& Spec, const FFFZSGridPlacement& Placement) const;
    void ValidateGround(const AFFZSGameplayZone& Zone, const FFFZSFacilitySpec& Spec, const AFFZSFacilityActor* IgnoreFacility, FFFZSPlacementValidation& InOutValidation) const;
    void ValidateWorldCollision(const AFFZSGameplayZone& Zone, const FFFZSFacilitySpec& Spec, const AFFZSFacilityActor* IgnoreFacility, FFFZSPlacementValidation& InOutValidation) const;
    void ValidateNavigation(const FFFZSFacilitySpec& Spec, FFFZSPlacementValidation& InOutValidation) const;
    void ValidateProtectedAreas(FName ZoneId, const AFFZSGameplayZone& Zone, const FFFZSFacilitySpec& Spec, FFFZSPlacementValidation& InOutValidation) const;
    
    bool RunPolicyPreValidation(const FFFZSPolicyContext& Context, FFFZSPolicyDecision& OutDecision) const;
    // 정책 목록 스냅샷을 순서대로 읽기 검증하고 첫 거부를 반환한다. 콜백 재진입은 거부다.
    
    bool ApplyTentativePolicies(const FFFZSPolicyContext& Context, TArray<TWeakObjectPtr<UObject>>& OutAppliedPolicies, FFFZSPolicyDecision& OutDecision);
    // 임시 적용 순서를 기록한다. 실패한 정책도 부분 적용 Rollback을 위해 OutAppliedPolicies에 포함
    
    void CommitPolicies(const FFFZSPolicyContext& Context, const TArray<TWeakObjectPtr<UObject>>& AppliedPolicies);
    // 로컬 변경 성공 후 같은 순서로 no-fail Commit을 호출

    void RollbackPolicies(const FFFZSPolicyContext& Context, const TArray<TWeakObjectPtr<UObject>>& AppliedPolicies);
    // 실패 시 적용 목록을 역순으로 멱등 Rollback한다.

    FFFZSCommandResult ExecuteInstall(const FFFZSCommand& Command, const FFFZSPlacementValidation& Validation);
    FFFZSCommandResult ExecuteMove(const FFFZSCommand& Command, const FFFZSPlacementValidation& Validation);
    FFFZSCommandResult ExecuteRemove(const FFFZSCommand& Command);
    FFFZSCommandResult ExecuteLifecycleCommand(const FFFZSCommand& Command);
    FFFZSCommandResult MakeFailure(EFFZSPlacementFailure Failure, const FText& Message) const;
    FFFZSCommandResult FinalizeSuccess(const FFFZSCommand& Command, AFFZSFacilityActor* Facility, FName PreviousZoneId);

    void OccupyFacilityRegions(const AFFZSFacilityActor& Facility);
    void ReleaseFacilityRegions(const AFFZSFacilityActor& Facility);
    bool AreRegionsOccupiedByOther(FName ZoneId, const TArray<FFFZSCellRect>& Regions, FGuid IgnoreFacilityId) const;
    void RefreshWorldActors() const;

    bool ValidateSnapshotBeforeRestore(const FFFZSWorldSnapshot& Snapshot, TArray<FText>& OutErrors) const;
    // 액터를 바꾸기 전에 스냅샷 전체의 스키마·참조·점유·큐·영수증 불변식을 검사

    bool RestoreWorldSnapshotUnchecked(const FFFZSWorldSnapshot& Snapshot, TArray<FText>& OutErrors);
    // 사전 검증된 스냅샷을 적용하는 내부 단계. 공개 Restore의 재진입 가드와 오류 복구 경계 안에서만 호출.

    UFUNCTION()
    void HandleAgentReleased(FName AgentId, FGuid TicketId, EFFZSQueueReleaseReason Reason);
    // 최종 해제 이벤트를 받아 현재 소유 시설과 일치할 때 AgentQueueOwnership을 제거.
};
