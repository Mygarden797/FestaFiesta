// FFZAZoneAuthoringSubsystem.h
// 역할: Zone 저작 계층의 "월드 단위 관리자".
//
//  - 구획(Plot)·구역(District) 목록과 구획 잠금/해제 상태를 보관한다.
//  - 구획·구역이 하나라도 있으면 FF_ZoneSystem에 배치 정책(External Policy)을 등록해
//    "잠긴 구획에는 설치 불가", "구역이 허용하지 않는 시설 종류는 설치 불가"를 적용한다.
//  - Zone 셀 지도(FFFZAZoneCellMap)를 만들어 좌표 → Zone ID를 빠르게 조회한다(UI 호버 등).
//
// FF_ZoneSystem 레지스트리(권위 데이터)는 건드리지 않는다. 이 서브시스템은 그 위에 규칙을 "추가"만 한다.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_FFZSTypes.h"
#include "FFZAZoneCellMap.h"
#include "FFZAZoneAuthoringSubsystem.generated.h"

class AFFZAPlot;
class ULevel;
class AFFZADistrict;
class AFFZSGameplayZone;
class UFFZALayerPolicy;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFFZAPlotStateChangedSignature, FName, PlotId, bool, bUnlocked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFFZAAuthoringLayersChangedSignature);

UCLASS()
class FF_ZONEAUTHORING_API UFFZAZoneAuthoringSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

    // ---------- 구획/구역 레지스트리 (액터가 스스로 등록) ----------
    void RegisterPlot(AFFZAPlot* Plot);
    void UnregisterPlot(AFFZAPlot* Plot);
    void RegisterDistrict(AFFZADistrict* District);
    void UnregisterDistrict(AFFZADistrict* District);

    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring")
    TArray<AFFZAPlot*> GetPlots() const;

    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring")
    TArray<AFFZADistrict*> GetDistricts() const;

    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring")
    AFFZAPlot* FindPlot(FName PlotId) const;

    // ---------- 구획 잠금 상태 (경제/저장 시스템이 호출) ----------
    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring|Plot")
    bool IsPlotUnlocked(FName PlotId) const;

    // 액터 기준 조회(ID가 비었거나 중복돼도 그 구획 자신의 시작 상태를 쓴다). 판정 코드는 이 함수를 쓴다.
    bool IsPlotActorUnlocked(const AFFZAPlot& Plot) const;

    // 구획을 해제한다. 비용 차감 등은 호출하는 쪽(경제 시스템)이 먼저 처리한다. 상태가 바뀌면 true.
    UFUNCTION(BlueprintCallable, Category = "FF Zone Authoring|Plot")
    bool UnlockPlot(FName PlotId);

    UFUNCTION(BlueprintCallable, Category = "FF Zone Authoring|Plot")
    bool LockPlot(FName PlotId);

    // 저장용: 현재 해제된 구획 ID 목록.
    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring|Plot")
    TArray<FName> GetUnlockedPlotIds() const;

    // 불러오기용: 목록에 있는 구획만 해제 상태로 맞춘다(나머지는 잠금).
    UFUNCTION(BlueprintCallable, Category = "FF Zone Authoring|Plot")
    void SetUnlockedPlotIds(const TArray<FName>& PlotIds);

    UPROPERTY(BlueprintAssignable, Category = "FF Zone Authoring|Events")
    FFFZAPlotStateChangedSignature OnPlotStateChanged;

    // 구획/구역이 추가·삭제·상태 변경됐을 때(오버레이 갱신 신호).
    UPROPERTY(BlueprintAssignable, Category = "FF Zone Authoring|Events")
    FFFZAAuthoringLayersChangedSignature OnAuthoringLayersChanged;

    // ---------- 배치 규칙 ----------
    // FF_ZoneSystem 정책 콜백에서 호출된다. 읽기 전용.
    FFFZSPolicyDecision EvaluatePlacement(const FFFZSPolicyContext& Context) const;

    // UI 호버용 간단 판정: 이 점에 해당 종류 시설을 지을 수 있는지(구획/구역 규칙만, Zone·충돌은 제외).
    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring")
    bool CanBuildAtWorldLocation(const FVector& WorldLocation, EFFZSFacilityCategory Category, FText& OutReason) const;

    // ---------- Zone 셀 지도 ----------
    UFUNCTION(BlueprintCallable, Category = "FF Zone Authoring|Cell Map")
    void RebuildZoneCellMap(float CellSizeCm = 100.0f);

    // 좌표가 속한 Zone ID(셀 해상도 근사). 처음 호출 시 지도를 자동으로 만든다. Zone 밖이면 None.
    UFUNCTION(BlueprintPure, Category = "FF Zone Authoring|Cell Map")
    FName FindZoneIdAtWorldLocation(const FVector& WorldLocation) const;

    const FFFZAZoneCellMap& GetZoneCellMap() const;

    // Zone이 추가/삭제/편집되면 호출. 다음 조회 때 지도를 다시 만든다.
    void MarkZoneCellMapDirty() { bCellMapDirty = true; }
    const TArray<FName>& GetZoneCellMapIds() const;

    // 아무 Gameplay Zone(기존/저작형 공통)의 경계를 월드 XY(cm)로 얻는다.
    static void GetZoneWorldPolygon(const AFFZSGameplayZone& Zone, TArray<FVector2D>& OutPolygon);

    // 셀 지도용: 월드의 모든 Zone과 경계를 ZoneId 순으로 모은다.
    static void CollectWorldZones(const UWorld& World, TArray<AFFZSGameplayZone*>& OutZones,
        TArray<TArray<FVector2D>>& OutPolygons);

private:
    void EnsurePolicyRegistered();
    void SchedulePolicyRegistrationRetry();
    void HandleLevelChanged(ULevel* Level, UWorld* World);
    void UnregisterPolicyIfUnused();
    void BuildCellMapIfNeeded() const;
    void NotifyLayersChanged();

    TArray<TWeakObjectPtr<AFFZAPlot>> Plots;
    TArray<TWeakObjectPtr<AFFZADistrict>> Districts;

    // 구획 ID → 해제 여부. 값이 없으면 구획의 bStartsUnlocked를 따른다.
    TMap<FName, bool> PlotUnlockOverrides;

    UPROPERTY(Transient)
    TObjectPtr<UFFZALayerPolicy> LayerPolicy;
    bool bPolicyRegistered = false;
    bool bPolicyRetryScheduled = false;
    FDelegateHandle LevelAddedHandle;
    FDelegateHandle LevelRemovedHandle;

    mutable FFFZAZoneCellMap CellMap;
    mutable TArray<FName> CellMapZoneIds;
    mutable bool bCellMapDirty = true;
    float CellMapCellSizeCm = 100.0f;
};
