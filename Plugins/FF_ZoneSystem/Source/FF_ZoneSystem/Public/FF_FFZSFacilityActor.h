// FF_FFZSFacilityActor.h

// 검증된 시설 사양의 런타임 복사본, 표현 컴포넌트, 앵커, 대기열 수명주기를 묶는 액터다.
// 흐름: 월드 서브시스템이 초기화·배치·개장·폐장·제거를 명령하고 액터는 표현과 대기열 상태를 갱신.
// 불변식: 권한 변경은 서브시스템만 수행하고 액터 스케일은 1이며 점유 중에는 이동·제거할 수 없다.
// 실패/성능: 비주얼은 소프트 참조로 비동기 적재되고 액터 자체 Tick은 사용하지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSFacilityActor.generated.h"

struct FStreamableHandle;
class UBoxComponent;
class UChildActorComponent;
class AFFZSFacilityLightingRig;
class UAudioComponent;
class ULineBatchComponent;
class USceneComponent;
class USplineComponent;
class UStaticMeshComponent;
class UFFZSFacilityAnchorComponent;
class UFFZSFacilityQueueComponent;
class UFFZSWorldSubsystem;
#if WITH_DEV_AUTOMATION_TESTS
class FFFZSWorldCommandLifecycleSnapshotTest;
class FFFZSSnapshotPhaseReentrancyAndDrainIntentTest;
class FFFZSFacilityLevelPresentationTest;
class FFFZSFacilityLightingLifecycleTest;
#endif

UENUM(BlueprintType)
enum class EFFZSFacilitySelectionIndicatorState : uint8
{
    Hidden,
    Hovered,
    Selected
};

//데이터 주도 시설 런타임 인스턴스.
// 새 수치나 비주얼 차이는 FFFZSFacilitySpec으로 표현하고, 실제로 새로운 행동 능력이 필요할 때만 서브클래스를 만든다.
// 권한 수명주기 변경은 friend인 UFFZSWorldSubsystem만 호출한다.
 
UCLASS(Blueprintable)
class FF_ZONESYSTEM_API AFFZSFacilityActor : public AActor
{
    GENERATED_BODY()

public:
    AFFZSFacilityActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    TObjectPtr<USceneComponent> SceneRoot;
    // 시설 Transform의 루트. 초기화와 이동은 스케일을 항상 1로 되돌린다. 

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    TObjectPtr<UStaticMeshComponent> VisualMesh;
    // 기존 메시 콜리전으로 플레이어만 막는다. 배치 점유·NPC Nav는 FacilitySpec이 담당한다.

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    TObjectPtr<UAudioComponent> FacilityAudioComponent;
    // FacilitySpec.AudioSettings를 재생하는 비권한 3D 오디오 컴포넌트다.

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    TObjectPtr<UChildActorComponent> FacilityLightingComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    TObjectPtr<UBoxComponent> SelectionProxy;
    // 비주얼 충돌과 분리된 선택 전용 Query Box. 물리·Nav·Overlap에는 참여하지 않는다.

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    TObjectPtr<UBoxComponent> FootprintBounds;
    // 사양 footprint의 제작·디버그 표현. 권위 점유는 서브시스템 격자 맵이 판정한다. 

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    TObjectPtr<UBoxComponent> QueueAreaBounds;
    // 예약된 queue 셀 범위를 보여 주는 표현 컴포넌트다. 

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    TObjectPtr<USplineComponent> QueueSpline;
    // 서비스 지점에서 대기열 꼬리 방향으로 정의된 로컬 queue 경로다. 

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility|Components")
    TObjectPtr<UFFZSFacilityQueueComponent> QueueComponent;
    // 서비스 지점에서 대기열 꼬리 방향으로 정의된 로컬 queue 경로다. 


    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    bool CanMoveOrRemove() const;
    // 대기·서비스·출구 이동 에이전트가 하나도 없고 상태가 Planned·Validated·Closed·MovePending 중 하나인지 반환.
    // Active·Removed이거나 Exiting 소유권이 남아 있으면 false다.
    
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    FGuid GetFacilityInstanceId() const { return FacilityInstanceId; }
    // 저장·명령·전역 레지스트리에서 사용하는 런타임 시설 GUID를 반환.
    
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    FName GetZoneId() const { return ZoneId; }
    // 현재 점유가 귀속된 안정 Gameplay Zone ID를 반환. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    const FFFZSGridPlacement& GetGridPlacement() const { return GridPlacement; }
    // 현재 존 로컬 격자 원점과 정규화된 사분면 회전을 읽기 전용으로 반환.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    const FFFZSFacilitySpec& GetFacilitySpec() const { return FacilitySpec; }
    // 현재 단계의 크기·외형이 적용된 사양이다. QueueRules의 처리 시간은 기준값이며 GetEffectiveQueueRules로 읽는다.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    const FFFZSFacilitySpec& GetBaseFacilitySpec() const;
    // 설치 시 복사한 원본 사양. 이후 강화·이동·저장 복원에서 배율을 이 원본에 한 번만 적용한다.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility|Lighting")
    bool HasLightingRig() const { return !FacilitySpec.LightingSettings.LightingRigClass.IsNull(); }

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility|Lighting")
    FFFZSFacilityLightingState GetLightingState() const;

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility|Lighting")
    AFFZSFacilityLightingRig* GetLightingRig() const;

    virtual void SetActorHiddenInGame(bool bNewHidden) override;

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility|Upgrade")
    bool HasFacilityLevels() const { return GetBaseFacilitySpec().bUsesFacilityLevels; }

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility|Upgrade")
    int32 GetUpgradeLevel() const { return UpgradeLevel; }

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility|Upgrade")
    float GetUpgradeServiceDurationMultiplier() const { return UpgradeServiceDurationMultiplier; }

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility|Upgrade")
    float GetUpgradeSizeMultiplier() const { return UpgradeSizeMultiplier; }

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility|Upgrade")
    float GetUpgradeRevenueMultiplier() const { return UpgradeRevenueMultiplier; }

    // Derived value only: shared definition data and the captured base spec stay immutable.
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility|Upgrade")
    FFFZSQueueRules GetEffectiveQueueRules() const;

    static bool IsValidUpgradeState(int32 Level, float ServiceDurationMultiplier, float SizeMultiplier = 1.0f, float RevenueMultiplier = 1.0f);
    static FFFZSQueueRules BuildEffectiveQueueRules(const FFFZSQueueRules& BaseRules, float ServiceDurationMultiplier);

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    EFFZSFacilityState GetFacilityState() const { return FacilityState; }
    // 권한 시설 수명주기 상태를 반환.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    const UFFZSFacilityQueueComponent* GetQueueComponent() const { return QueueComponent; }
    // 관찰 전용 대기열 컴포넌트를 반환한다. 참가·이탈은 UFFZSWorldSubsystem API를 사용.
    
    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    bool NeedsRelocation() const { return bNeedsRelocation; }
    // GIS·콘텐츠 경계 재검증 뒤 현재 위치가 더 이상 유효하지 않아 담당자 이동이 필요한지 반환.

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Facility|Presentation")
    void SetBuildModeRangeVisible(bool bVisible);
    // 건축 모드에서 footprint·clearance·queue 예약 범위를 정확한 압축 사각형 단위로 표시하거나 숨긴다.

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Facility|Presentation")
    void SetBuildModeActionSelected(bool bSelected);
    // 이동·철거 액션을 고르는 동안 선택된 시설의 footprint를 청록색 굵은 선으로 강조한다.

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Facility|Presentation")
    void SetSelectionIndicatorState(EFFZSFacilitySelectionIndicatorState NewState);
    // 실제 SelectionProxy를 표시해 Hover·정보 선택 상태와 클릭 가능한 범위를 같은 기하로 보여 준다.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility|Presentation")
    EFFZSFacilitySelectionIndicatorState GetSelectionIndicatorState() const
    {
        return SelectionIndicatorState;
    }

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Facility|Presentation",
        meta = (DeprecatedFunction, DeprecationMessage = "Use SetBuildModeActionSelected."))
    void SetBuildModeRemovalSelected(bool bSelected);
    // 기존 Blueprint 호환용 이름이다. 선택 표현은 SetBuildModeActionSelected와 동일하다.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility|Presentation")
    bool IsWorldPointInsideFootprint(const FVector& WorldPoint) const;
    // 충돌 설정에 의존하지 않고 월드 지면 점이 회전된 시설 footprint 안에 있는지 판정한다.
    
    const USplineComponent* GetQueueSpline() const { return QueueSpline; }
    // 대기 슬롯 월드 위치 계산에 쓰는 읽기 전용 spline을 반환.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    bool GetAnchorWorldTransform(EFFZSAnchorType Type, int32 Index, FTransform& OutTransform) const;
    // Type·Index가 일치하는 앵커의 월드 Transform을 OutTransform에 쓰고 true를 반환. 없으면 false며 출력은 보장하지 않는다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    FFFZSFacilityInstanceSnapshot CaptureInstanceSnapshot() const;
    // 액터와 대기열의 현재 값 스냅샷을 만든다. 월드 전체 시설·점유·영수증 저장/복원 경계는 WorldSubsystem API다. 

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Facility")
    FFFZSFacilityReadModel GetReadModel() const;
    // UI·분석용 비권한 요약값을 현재 상태에서 만든다. 반환값 수정은 런타임 상태에 영향을 주지 않는다. 

protected:
    virtual void BeginPlay() override;
    // 런타임 표현을 준비하고 큐 상태 변경 콜백을 연결한다. 
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    // 비동기 메시 로드를 취소하고 큐 콜백 연결을 정리한다. 

private:
    friend class UFFZSWorldSubsystem;
    friend class UFFZSFacilityQueueComponent;
#if WITH_DEV_AUTOMATION_TESTS
    friend class FFFZSWorldCommandLifecycleSnapshotTest;
    friend class FFFZSSnapshotPhaseReentrancyAndDrainIntentTest;
    friend class FFFZSFacilityLevelPresentationTest;
    friend class FFFZSFacilityLightingLifecycleTest;
#endif

    // 권한 수명주기 변경은 의도적으로 월드 서브시스템에만 공개한다.
    // 점유 맵·명령 영수증·외부 정책과 액터 상태가 함께 바뀌어야 하기 때문이다.
    
    bool InitializeFacility(FGuid NewInstanceId, const FFFZSFacilitySpec& NewSpec, FName NewZoneId, FFFZSGridPlacement NewPlacement, const FTransform& WorldTransform, EFFZSFacilityState InitialState = EFFZSFacilityState::Validated, int32 InitialUpgradeLevel = 1, float InitialServiceDurationMultiplier = 1.0f, float InitialSizeMultiplier = 1.0f, float InitialRevenueMultiplier = 1.0f);
    // 검증된 ID·사양·배치를 복사하고 표현·앵커·큐 규칙을 재구성한다. 입력이 무효면 상태를 바꾸지 않고 false다. 

    // 월드가 새 점유 영역을 검증한 뒤 호출한다. 모든 실패 검사를 변경 전에 완료한다.
    bool ApplyUpgradeState(int32 NewLevel, float NewServiceDurationMultiplier, float NewSizeMultiplier, float NewRevenueMultiplier = 1.0f);
    
    bool ApplyPlacement(FName NewZoneId, FFFZSGridPlacement NewPlacement, const FTransform& WorldTransform);
    // 비점유 이동 가능 상태에서 존·격자·Transform을 교체하고 Validated로 전환한다. 
    
    bool OpenFacility();
    // Validated·Closed·MovePending 시설을 Active로 전환하고 큐를 연다. 그 외 상태는 false다. 
    
    void CloseFacility(bool bEmergency);
    // 일반 폐장은 기존 이용자를 drain하고, 긴급 폐장은 즉시 취소한다. 제거된 시설에는 아무 작업도 하지 않는다. 
    
    void MarkRemoved();
    // 비점유 시설을 Removed로 만들고 충돌·표시를 끈다. 점유 중이면 아무 작업도 하지 않는다. 
    
    UFFZSFacilityQueueComponent* GetMutableQueueComponent() const { return QueueComponent; }
    // 월드 서브시스템 전용 변경 포인터. 게임플레이 코드는 공개 const 접근자를 사용한다. 
    
    void SetNeedsRelocation(bool bNewNeedsRelocation) { bNeedsRelocation = bNewNeedsRelocation; }
    // 권위 GIS 재검증 플래그다. 인스턴스는 유지되며 자동으로 삭제되지 않는다. 

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    FGuid FacilityInstanceId;
    // 레지스트리·명령·저장에서 사용하는 권한 인스턴스 ID다. 

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    FFFZSFacilitySpec BaseFacilitySpec;
    // 설치 시점에 값 복사된 원본 사양이며 저장 스냅샷의 FacilitySpec도 이 값이다.

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    FFFZSFacilitySpec FacilitySpec;
    // BaseFacilitySpec과 현재 단계에서 파생한 크기·외형 캐시다. 기존 배치 Actor의 원본 호환 필드이기도 하다.

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    int32 UpgradeLevel = 1;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    float UpgradeServiceDurationMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    float UpgradeSizeMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    float UpgradeRevenueMultiplier = 1.0f;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Lighting")
    bool bHasLightingOverride = false;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Lighting")
    FFFZSFacilityLightingState LightingState;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    FName ZoneId = NAME_None;
    // 현재 점유 키가 속한 존 ID다. 

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    FFFZSGridPlacement GridPlacement;
    // 현재 존 로컬 배치다. YawCentidegrees는 항상 0..35999이고 레거시 QuarterTurns는 0이다.

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    EFFZSFacilityState FacilityState = EFFZSFacilityState::Planned;
    // 시설과 큐 동작을 제한하는 권한 수명주기 상태다. 

    UPROPERTY(VisibleAnywhere, Category = "Facility|Runtime")
    bool bNeedsRelocation = false;
    // 경계 재검증 실패를 보존하는 담당자 조치 플래그다. 자동 삭제에는 사용하지 않는다. 
    
    UPROPERTY(Transient)
    // FacilitySpec.Anchors에서 재생성 가능한 표현 컴포넌트 캐시다. 
    TArray<TObjectPtr<UFFZSFacilityAnchorComponent>> RuntimeAnchors;

    UPROPERTY(VisibleAnywhere, Category = "Facility|Components", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<ULineBatchComponent> BuildModeRangeLines;
    // clearance·queue 압축 사각형을 한 Scene Proxy로 묶는 비권위 범위 표현이다.

    UPROPERTY(VisibleAnywhere, Category = "Facility|Navigation", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> NavigationObstacleBounds;
    // 등록된 시설의 고형 footprint만 NavArea_Null로 반영하는 Nav 전용 Box다.

    UPROPERTY(VisibleAnywhere, Category = "Facility|Navigation", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> QueueLaneNavCostBounds;
    // 예약된 Queue 레인을 고비용 NavArea로 반영하는 Nav 전용 Box다. footprint와 달리 통행을
    // 막지 않고 비용만 올린다 — 막으면 대기열 슬롯이 NavMesh 밖으로 나가 배치 검증이 깨진다.

    bool bBuildModeRangeVisible = false;
    bool bBuildModeActionSelected = false;
    EFFZSFacilitySelectionIndicatorState SelectionIndicatorState =
        EFFZSFacilitySelectionIndicatorState::Hidden;

    // 일반 폐장 콜백 재진입 뒤 큐가 완전히 비면 MovePending으로 전환하려는 의도 플래그다. 
    bool bSetMovePendingWhenDrained = false;
    // 소프트 비주얼 메시 비동기 요청의 수명과 취소를 관리한다. 
    TSharedPtr<FStreamableHandle> VisualLoadHandle;
    uint32 VisualLoadGeneration = 0;
    // 등록된 시설에만 메시 이동 충돌을 허용한다. 숨겨진 복원 staging과 늦은 로드 완료를 구분한다.
    bool bPlayerCollisionEnabled = false;
    // 시설 오디오 의존성은 메시와 독립적으로 교체·취소한다.
    TSharedPtr<FStreamableHandle> AudioLoadHandle;
    uint32 AudioLoadGeneration = 0;
    bool bAudioStopRequested = false;

    TSharedPtr<FStreamableHandle> LightingLoadHandle;
    uint32 LightingLoadGeneration = 0;

    void RebuildRuntimePresentation();
    void RebuildSelectionProxy(const FVector& SelectionExtent);
    void SetPlayerCollisionEnabled(bool bEnabled);
    void RefreshPlayerCollision();
    void SetNavigationObstacleEnabled(bool bEnabled);
    void ApplyQueueLaneNavCostGeometry();
    void RequestVisualMesh();
    void RequestVisualMeshAsset(const FSoftObjectPath& AssetPath, uint32 LoadGeneration, bool bAllowBaseFallback);
    void ApplyLoadedVisualMesh(uint32 LoadGeneration, FSoftObjectPath AssetPath, bool bAllowBaseFallback);
    void ApplyVisualMesh(UStaticMesh* Mesh);
    void RequestFacilityAudio();
    void RequestFacilityLighting();
    void ApplyLoadedFacilityLighting(uint32 LoadGeneration);
    void DestroyFacilityLighting();
    void UpdateFacilityLighting();
    void ApplyFacilityLightingOverride(const FFFZSFacilityLightingState& State, bool bHasOverride);
    void ApplyLoadedFacilityAudio(uint32 LoadGeneration);
    void UpdateFacilityAudioPlayback(bool bAllowFade = true);
    void StopFacilityAudio(bool bAllowFade);
    bool IsFacilityAudioPlaybackAllowed() const;
    void HandleFacilityAudioFinished(UAudioComponent* FinishedComponent);
    void RebuildAnchors();
    void RebuildBuildModeRangePresentation();
    void DestroyBuildModeRangePresentation();

    void HandleQueueStateChanged(EFFZSQueueState PreviousState, EFFZSQueueState NewState);
};
