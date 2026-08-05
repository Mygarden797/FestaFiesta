#pragma once

/**
 * @file FF_FFZSFacilityActor.h
 * @brief 검증된 시설 사양의 런타임 복사본, 표현 컴포넌트, 앵커, 대기열 수명주기를 묶는 액터다.
 *
 * 흐름: 월드 서브시스템이 초기화·배치·개장·폐장·제거를 명령하고 액터는 표현과 대기열 상태를 갱신한다.
 * 불변식: 권한 변경은 서브시스템만 수행하고 액터 스케일은 1이며 점유 중에는 이동·제거할 수 없다.
 * 실패/성능: 비주얼은 소프트 참조로 비동기 적재되고 액터 자체 Tick은 사용하지 않는다.
 */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSFacilityActor.generated.h"

struct FStreamableHandle;
class UBoxComponent;
class USceneComponent;
class USplineComponent;
class UStaticMeshComponent;
class UFFZSFacilityAnchorComponent;
class UFFZSFacilityQueueComponent;
class UFFZSWorldSubsystem;
#if WITH_DEV_AUTOMATION_TESTS
class FFFZSWorldCommandLifecycleSnapshotTest;
class FFFZSSnapshotPhaseReentrancyAndDrainIntentTest;
#endif

/**
 * 데이터 주도 시설 런타임 인스턴스.
 *
 * 새 수치나 비주얼 차이는 FFFZSFacilitySpec으로 표현하고, 실제로 새로운 행동 능력이 필요할 때만 서브클래스를 만든다.
 * 권한 수명주기 변경은 friend인 UFFZSWorldSubsystem만 호출한다.
 */
UCLASS(Blueprintable)
class FF_ZONESYSTEM_API AFFZSFacilityActor : public AActor
{
    GENERATED_BODY()

public:
    AFFZSFacilityActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    /** 시설 Transform의 루트. 초기화와 이동은 스케일을 항상 1로 되돌린다. */
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    /** FacilitySpec.VisualMesh를 비동기 표시하는 비권한 표현 캐시다. */
    TObjectPtr<UStaticMeshComponent> VisualMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    /** 사양 footprint의 제작·디버그 표현. 권위 점유는 서브시스템 격자 맵이 판정한다. */
    TObjectPtr<UBoxComponent> FootprintBounds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    /** 예약된 queue 셀 범위를 보여 주는 표현 컴포넌트다. */
    TObjectPtr<UBoxComponent> QueueAreaBounds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    /** 서비스 지점에서 대기열 꼬리 방향으로 정의된 로컬 queue 경로다. */
    TObjectPtr<USplineComponent> QueueSpline;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    /** 이 시설의 FIFO 대기·서비스·출구 상태를 소유한다. 변경은 월드 서브시스템을 통해서만 한다. */
    TObjectPtr<UFFZSFacilityQueueComponent> QueueComponent;


    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    /**
     * 대기·서비스·출구 이동 에이전트가 하나도 없고 상태가 Planned·Validated·Closed·MovePending 중 하나인지 반환한다.
     * Active·Removed이거나 Exiting 소유권이 남아 있으면 false다.
     */
    bool CanMoveOrRemove() const;

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    /** 저장·명령·전역 레지스트리에서 사용하는 런타임 시설 GUID를 반환한다. */
    FGuid GetFacilityInstanceId() const { return FacilityInstanceId; }

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    /** 현재 점유가 귀속된 안정 Gameplay Zone ID를 반환한다. */
    FName GetZoneId() const { return ZoneId; }

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    /** 현재 존 로컬 격자 원점과 정규화된 사분면 회전을 읽기 전용으로 반환한다. */
    const FFFZSGridPlacement& GetGridPlacement() const { return GridPlacement; }

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    /** 설치 때 복사한 사양을 반환한다. 원본 Data Asset의 이후 변경은 이 인스턴스에 자동 반영되지 않는다. */
    const FFFZSFacilitySpec& GetFacilitySpec() const { return FacilitySpec; }

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    /** 권한 시설 수명주기 상태를 반환한다. */
    EFFZSFacilityState GetFacilityState() const { return FacilityState; }

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    /** 관찰 전용 대기열 컴포넌트를 반환한다. 참가·이탈은 UFFZSWorldSubsystem API를 사용한다. */
    const UFFZSFacilityQueueComponent* GetQueueComponent() const { return QueueComponent; }
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    /** GIS·콘텐츠 경계 재검증 뒤 현재 위치가 더 이상 유효하지 않아 담당자 이동이 필요한지 반환한다. */
    bool NeedsRelocation() const { return bNeedsRelocation; }

    /** 대기 슬롯 월드 위치 계산에 쓰는 읽기 전용 spline을 반환한다. */
    const USplineComponent* GetQueueSpline() const { return QueueSpline; }

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    /** Type·Index가 일치하는 앵커의 월드 Transform을 OutTransform에 쓰고 true를 반환한다. 없으면 false며 출력은 보장하지 않는다. */
    bool GetAnchorWorldTransform(EFFZSAnchorType Type, int32 Index, FTransform& OutTransform) const;

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    /** 액터와 대기열의 현재 값 스냅샷을 만든다. 월드 전체 시설·점유·영수증 저장/복원 경계는 WorldSubsystem API다. */
    FFFZSFacilityInstanceSnapshot CaptureInstanceSnapshot() const;

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    /** UI·분석용 비권한 요약값을 현재 상태에서 만든다. 반환값 수정은 런타임 상태에 영향을 주지 않는다. */
    FFFZSFacilityReadModel GetReadModel() const;

protected:
    /** 런타임 표현을 준비하고 큐 상태 변경 콜백을 연결한다. */
    virtual void BeginPlay() override;
    /** 비동기 메시 로드를 취소하고 큐 콜백 연결을 정리한다. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    friend class UFFZSWorldSubsystem;
    friend class UFFZSFacilityQueueComponent;
#if WITH_DEV_AUTOMATION_TESTS
    friend class FFFZSWorldCommandLifecycleSnapshotTest;
    friend class FFFZSSnapshotPhaseReentrancyAndDrainIntentTest;
#endif

        /**
     * 권한 수명주기 변경은 의도적으로 월드 서브시스템에만 공개한다.
     * 점유 맵·명령 영수증·외부 정책과 액터 상태가 함께 바뀌어야 하기 때문이다.
     */
    /** 검증된 ID·사양·배치를 복사하고 표현·앵커·큐 규칙을 재구성한다. 입력이 무효면 상태를 바꾸지 않고 false다. */
    bool InitializeFacility(FGuid NewInstanceId, const FFFZSFacilitySpec& NewSpec, FName NewZoneId, FFFZSGridPlacement NewPlacement, const FTransform& WorldTransform, EFFZSFacilityState InitialState = EFFZSFacilityState::Validated);
    /** 비점유 이동 가능 상태에서 존·격자·Transform을 교체하고 Validated로 전환한다. */
    bool ApplyPlacement(FName NewZoneId, FFFZSGridPlacement NewPlacement, const FTransform& WorldTransform);
    /** Validated·Closed·MovePending 시설을 Active로 전환하고 큐를 연다. 그 외 상태는 false다. */
    bool OpenFacility();
    /** 일반 폐장은 기존 이용자를 drain하고, 긴급 폐장은 즉시 취소한다. 제거된 시설에는 아무 작업도 하지 않는다. */
    void CloseFacility(bool bEmergency);
    /** 비점유 시설을 Removed로 만들고 충돌·표시를 끈다. 점유 중이면 아무 작업도 하지 않는다. */
    void MarkRemoved();
    /** 월드 서브시스템 전용 변경 포인터. 게임플레이 코드는 공개 const 접근자를 사용한다. */
    UFFZSFacilityQueueComponent* GetMutableQueueComponent() const { return QueueComponent; }
    /** 권위 GIS 재검증 플래그다. 인스턴스는 유지되며 자동으로 삭제되지 않는다. */
    void SetNeedsRelocation(bool bNewNeedsRelocation) { bNeedsRelocation = bNewNeedsRelocation; }

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    /** 레지스트리·명령·저장에서 사용하는 권한 인스턴스 ID다. */
    FGuid FacilityInstanceId;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    /** 설치 시점에 값 복사된 권한 사양이다. */
    FFFZSFacilitySpec FacilitySpec;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    /** 현재 점유 키가 속한 존 ID다. */
    FName ZoneId = NAME_None;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    /** 현재 존 로컬 배치다. QuarterTurns는 항상 0..3이다. */
    FFFZSGridPlacement GridPlacement;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    /** 시설과 큐 동작을 제한하는 권한 수명주기 상태다. */
    EFFZSFacilityState FacilityState = EFFZSFacilityState::Planned;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    /** 경계 재검증 실패를 보존하는 담당자 조치 플래그다. 자동 삭제에는 사용하지 않는다. */
    bool bNeedsRelocation = false;


    UPROPERTY(Transient)
    /** FacilitySpec.Anchors에서 재생성 가능한 표현 컴포넌트 캐시다. */
    TArray<TObjectPtr<UFFZSFacilityAnchorComponent>> RuntimeAnchors;

    /** 일반 폐장 콜백 재진입 뒤 큐가 완전히 비면 MovePending으로 전환하려는 의도 플래그다. */
    bool bSetMovePendingWhenDrained = false;
    /** 소프트 비주얼 메시 비동기 요청의 수명과 취소를 관리한다. */
    TSharedPtr<FStreamableHandle> VisualLoadHandle;

    void RebuildRuntimePresentation();
    void RequestVisualMesh();
    void ApplyLoadedVisualMesh();
    void RebuildAnchors();

    void HandleQueueStateChanged(EFFZSQueueState PreviousState, EFFZSQueueState NewState);
};
