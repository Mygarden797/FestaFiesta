#pragma once

/**
 * @file FF_FFZSZoneActor.h
 * @brief 게임플레이 존의 안정 ID, 폴리곤 경계, 시설 허용 규칙, 인접 그래프, 밀도 상태를 소유한다.
 *
 * 흐름: 액터가 등록되고 배치가 경계를 읽으며, 소속 판정은 설정형 경계 여유를 적용하고 인구 표본은 유지 시간 기반 밀도를 갱신한다.
 * 불변식: 스케일 1, 격자 정렬 원점, 90도 단위 Yaw, 선형 폐곡선인 단순 폴리곤이어야 한다.
 * 실패/성능: 잘못된 제작 데이터는 모든 오류를 모아 거부하며 폴리곤 조회는 호출마다 정점 배열을 만든다.
 */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSZoneActor.generated.h"

class USceneComponent;
class USplineComponent;

/**
 * 편집 가능한 폴리곤 Gameplay Zone.
 *
 * 참고 패드나 252 m 작업 셀과 분리된 권한 게임 규칙 경계이며 ZoneId는 저장·그래프·점유의 안정 키다.
 */
UCLASS(Blueprintable)
class FF_ZONESYSTEM_API AFFZSGameplayZone : public AActor
{
    GENERATED_BODY()

public:
    AFFZSGameplayZone();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone|Components")
    /** 존 원점·90도 단위 Yaw를 소유하는 루트. 스케일은 항상 1이어야 한다. */
    TObjectPtr<USceneComponent> SceneRoot;

    /** 선형 폐곡선 spline. 각 점의 로컬 X/Y cm가 존 폴리곤 정점이며 자기 교차와 중복 변은 허용하지 않는다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zone|Components")
    TObjectPtr<USplineComponent> BoundarySpline;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Identity")
    /** 레지스트리·저장·인접 그래프에서 쓰는 비어 있지 않은 안정 ID다. 등록 후 직접 바꾸지 않는다. */
    FName ZoneId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Identity")
    /** UI 표시용 이름. 안정 식별에는 ZoneId를 사용한다. */
    FText DisplayName;

    /** 2025 근사 참고 오브젝트 연결 키. 권한 Gameplay Zone ID나 경계가 아니다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Identity")
    FName SourceReferenceId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Identity", meta = (ClampMin = "1"))
    /** 존 데이터 구조 마이그레이션 버전이다. */
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Identity", meta = (ClampMin = "1"))
    /** 같은 스키마 안에서 제작 경계·규칙이 바뀐 버전이다. */
    int32 ContentVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Boundary")
    /** 승인된 GIS 경계를 반영했는지 나타내는 제작 메타데이터다. true여도 런타임 검증은 생략하지 않는다. */
    bool bAuthoritativeGISBoundary = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Boundary", meta = (ClampMin = "1.0", Units = "cm"))
    /** 액터 원점 중심으로 허용하는 전체 수직 두께다. 히스테리시스는 이 범위를 확장하지 않는다. */
    float VerticalExtentCm = 2000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Rules")
    /** true면 모든 시설 카테고리를 허용하고 AllowedFacilityCategories를 무시한다. */
    bool bAllowAllFacilityCategories = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Rules", meta = (EditCondition = "!bAllowAllFacilityCategories"))
    /** bAllowAllFacilityCategories가 false일 때만 허용할 카테고리 집합이다. */
    TArray<EFFZSFacilityCategory> AllowedFacilityCategories;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Graph")
    /** 방향 없는 존 그래프 이웃 ID. 자기 참조·누락·비대칭 연결은 그래프 검증 오류다. */
    TArray<FName> NeighborZoneIds;

    /** 존 로컬 cm의 출입 기준점. 실제 경로 생성과 연결 가능성 책임은 Navigation 어댑터에 있다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Graph")
    TArray<FTransform> EntryExitAnchors;

    /** 양수면 밀도 계산에 사용할 공공 면적 m², 0이면 폴리곤 경계 면적을 사용한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Density", meta = (ClampMin = "0.0"))
    float UsablePublicAreaSquareMeters = 0.0f;

    /** 양수면 고정 SoftCapacity, 0이면 사용 면적 / Project Settings의 1인당 면적으로 내림 계산한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Zone|Density", meta = (ClampMin = "0"))
    int32 SoftCapacityOverride = 0;

    UFUNCTION(BlueprintPure, Category = "Zone")
    /** 현재 spline 점을 존 로컬 X/Y cm 배열로 복사한다. 호출마다 배열을 할당하므로 대량 반복 시 결과를 호출 측에서 캐시한다. */
    TArray<FVector2D> GetBoundaryPolygonLocalCm() const;

    UFUNCTION(BlueprintPure, Category = "Zone")
    /** 월드 점이 수직 범위와 정확한 XY 폴리곤 안 또는 0.1 cm 경계 허용 안에 있는지 반환한다. */
    bool ContainsWorldPoint(const FVector& WorldPoint) const;

    UFUNCTION(BlueprintPure, Category = "Zone")
    /**
     * 정확한 내부 또는 XY 폴리곤 변에서 HysteresisCm 이내인지 반환한다.
     * 이전 존 유지에만 사용하며 수직 범위나 다른 존의 선점 경계를 넓히지 않는다.
     */
    bool ContainsWorldPointWithHysteresis(const FVector& WorldPoint, float HysteresisCm) const;

    UFUNCTION(BlueprintPure, Category = "Zone|Grid")
    /** 중앙 계약(1 Cell=1 cm)으로 존 로컬 셀 좌하단을 월드 위치로 변환한다. 새 호출부는 이 API를 사용한다. */
    FVector GridCellCornerToWorld1Cm(FIntPoint Cell) const;

    UFUNCTION(BlueprintPure, Category = "Zone|Grid")
    /** 월드 위치를 중앙 1 cm 격자의 포함 셀로 내림 변환한다. */
    FIntPoint WorldToGridCell1Cm(const FVector& WorldLocation) const;

    UFUNCTION(BlueprintPure, Category = "Zone|Grid", meta = (DeprecatedFunction, DeprecationMessage = "Cell size is fixed at 1 cm. Use GridCellCornerToWorld1Cm."))
    /** 구 Blueprint/C++ 호환용이다. CellSizeCm 입력은 무시하고 중앙 1 cm 계약만 사용한다. */
    FVector GridCellCornerToWorld(FIntPoint Cell, float CellSizeCm) const;

    UFUNCTION(BlueprintPure, Category = "Zone|Rules")
    /** 전체 허용 플래그 또는 명시 목록에 따라 시설 카테고리 허용 여부를 반환한다. */
    bool IsCategoryAllowed(EFFZSFacilityCategory Category) const;

    UFUNCTION(BlueprintPure, Category = "Zone|Density")
    /** 로컬 폴리곤에 신발끈 공식을 적용해 절대 면적 m²를 반환한다. 정점이 3개 미만이면 0이다. */
    float GetBoundaryAreaSquareMeters() const;

    UFUNCTION(BlueprintPure, Category = "Zone|Density")
    /** 양수 오버라이드를 우선하고 없으면 면적/1인당 면적을 내림한 0 이상의 수용량을 반환한다. */
    int32 GetSoftCapacity() const;

    UFUNCTION(BlueprintCallable, Category = "Zone|Density")
    /**
     * 음수 입력을 0으로 보정하고 밀도 상태를 갱신한다.
     * Caution은 즉시 상승하고 Warning·Critical은 설정된 연속 유지 시간 뒤 상승하며 기준 하락 시 즉시 재평가한다.
     */
    void UpdatePopulationSample(int32 Population, float DeltaSeconds);

    UFUNCTION(BlueprintPure, Category = "Zone|Density")
    /** UI·분석용 ID·수용량·보고 인구·비율·밀도 상태의 값 복사본을 반환한다. */
    FFFZSZoneReadModel GetReadModel() const;

    UFUNCTION(CallInEditor, Category = "Zone|Presets")
    /** 내장 Accepted ID의 이름·참고 키·버전을 적용한다. 이미 등록된 액터나 알 수 없는 ID면 키 불일치를 막고 false다. */
    bool ApplyAcceptedZonePreset(FName AcceptedZoneId);

    UFUNCTION(BlueprintPure, Category = "Zone|Validation")
    /** ID·spline·단순 폴리곤·Transform·용량·인접 ID의 모든 제작 오류를 OutErrors에 모으고 오류가 없을 때 true다. */
    bool ValidateZoneData(TArray<FText>& OutErrors) const;

protected:
    /** 데이터 계약과 ZoneId 유일성을 거쳐 월드 서브시스템에 등록한다. */
    virtual void BeginPlay() override;
    /** 레벨 전환·파괴 시 월드 레지스트리에서 해제한다. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** 외부 시뮬레이션이 마지막으로 보고한 0 이상의 인구 표본이다. */
    int32 ReportedPopulation = 0;
    /** 유지 시간 규칙까지 적용한 현재 밀도 단계다. */
    EFFZSDensityState DensityState = EFFZSDensityState::Normal;
    /** Warning 비율 이상을 연속 유지한 누적 초다. 기준 아래면 0으로 초기화한다. */
    float WarningHeldSeconds = 0.0f;
    /** Critical 비율 이상을 연속 유지한 누적 초다. 기준 아래면 0으로 초기화한다. */
    float CriticalHeldSeconds = 0.0f;
};
