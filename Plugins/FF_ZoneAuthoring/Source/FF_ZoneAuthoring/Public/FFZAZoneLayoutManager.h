// FFZAZoneLayoutManager.h
// 역할: 레벨에 하나 놓고 쓰는 "Zone 작업 도구 상자"(에디터 전용 액터, 게임 빌드에는 포함되지 않음).
//
// 디테일 패널의 버튼으로:
//  0. 한 번에 정리+검사
//  1. Zone / 구획 / 구역 / 오버레이 만들기
//  2. 정의 일괄 적용, Transform 일괄 정리, 경계 용접, 이웃 자동 채우기, Project Zone Ids 동기화
//  3. 전체 검사(겹침·이웃 누락·ID 중복·설정 누락)를 Map Check 창과 아래 목록에 보고
//  4. 통합 건설 영역(Build Area)·오버레이 다시 계산
//
// 모든 버튼은 Ctrl+Z로 되돌릴 수 있다(Project Settings 파일 저장은 예외).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FFZAZoneLayoutManager.generated.h"

class UFFZAZoneDefinition;
class UBillboardComponent;
class USceneComponent;

UCLASS(meta = (DisplayName = "FF Zone Layout Manager"),
    HideCategories = (Rendering, Physics, Collision, Input, HLOD, LOD, Cooking, Replication, Networking, DataLayers, WorldPartition, LevelInstance))
class FF_ZONEAUTHORING_API AFFZAZoneLayoutManager : public AActor
{
    GENERATED_BODY()

public:
    AFFZAZoneLayoutManager();

    virtual bool IsEditorOnly() const override { return true; }

    UPROPERTY(VisibleAnywhere, Category = "Components")
    TObjectPtr<USceneComponent> SceneRoot;

#if WITH_EDITORONLY_DATA
    // 뷰포트에서 클릭해 선택할 수 있도록 아이콘을 표시한다.
    UPROPERTY()
    TObjectPtr<UBillboardComponent> SpriteComponent;
#endif

    // ===================== 0. 한 번에 =====================

    // 정의 적용 → Transform 정리 → 경계 용접 → 이웃 자동 채우기 → 건설 영역/오버레이 갱신 → 전체 검사.
    UFUNCTION(CallInEditor, Category = "0. One Click")
    void FixAndValidateAll();

    // ===================== 1. 만들기 =====================

    UPROPERTY(EditAnywhere, Category = "1. Create", meta = (
        ToolTip = "Create Zone 버튼으로 만들 Zone의 종류입니다."))
    TObjectPtr<UFFZAZoneDefinition> NewZoneDefinition;

    UPROPERTY(EditAnywhere, Category = "1. Create", meta = (
        ToolTip = "새로 만드는 Zone/구획/구역의 처음 크기(m)입니다. 이 매니저 위치가 왼쪽 아래 모서리가 됩니다."))
    FVector2D NewAreaSizeMeters = FVector2D(30.0, 30.0);

    UFUNCTION(CallInEditor, Category = "1. Create")
    void CreateZone();

    UFUNCTION(CallInEditor, Category = "1. Create")
    void CreatePlot();

    UFUNCTION(CallInEditor, Category = "1. Create")
    void CreateDistrict();

    // 레벨에 오버레이가 없으면 하나 만든다.
    UFUNCTION(CallInEditor, Category = "1. Create")
    void CreateOverlay();

    // ===================== 2. 정리 =====================

    UPROPERTY(EditAnywhere, Category = "2. Fix", meta = (ClampMin = "0.0", Units = "cm",
        ToolTip = "Weld Shared Boundaries가 이 거리 안의 꼭짓점을 이웃 Zone 경계에 붙입니다. 너무 크면 모양이 크게 바뀝니다."))
    float WeldToleranceCm = 50.0f;

    UPROPERTY(EditAnywhere, Category = "2. Fix", meta = (
        ToolTip = "켜면 Auto Fill Neighbors가 손으로 넣은 이웃을 지우지 않고 추가만 합니다."))
    bool bKeepManualNeighbors = false;

    UPROPERTY(EditAnywhere, Category = "2. Fix", meta = (
        ToolTip = "켜면 Sync Project Zone Ids가 정의 에셋이 없는 ID를 Project Settings 목록에서 지웁니다. 다른 맵 전용 ID가 있다면 끄세요."))
    bool bRemoveIdsWithoutDefinition = false;

    UFUNCTION(CallInEditor, Category = "2. Fix")
    void ApplyAllDefinitions();

    UFUNCTION(CallInEditor, Category = "2. Fix")
    void NormalizeAllTransforms();

    UFUNCTION(CallInEditor, Category = "2. Fix")
    void WeldSharedBoundaries();

    UFUNCTION(CallInEditor, Category = "2. Fix")
    void AutoFillNeighbors();

    // 모든 Zone Definition 에셋의 ID를 Project Settings > FF Zone System > Project Zone Ids에 등록하고 저장한다.
    UFUNCTION(CallInEditor, Category = "2. Fix")
    void SyncProjectZoneIds();

    // ===================== 3. 검사 =====================

    UPROPERTY(EditAnywhere, Category = "3. Check", meta = (ClampMin = "10.0", Units = "cm",
        ToolTip = "겹침/이웃 분석에 쓰는 칸 크기입니다. 작을수록 정밀하지만 느립니다."))
    float AnalysisCellSizeCm = 100.0f;

    UFUNCTION(CallInEditor, Category = "3. Check")
    void ValidateAll();

    UPROPERTY(VisibleAnywhere, Transient, Category = "3. Check")
    int32 LastErrorCount = 0;

    UPROPERTY(VisibleAnywhere, Transient, Category = "3. Check")
    int32 LastWarningCount = 0;

    UPROPERTY(VisibleAnywhere, Transient, Category = "3. Check")
    TArray<FString> LastReport;

    // ===================== 4. 갱신 =====================

    UFUNCTION(CallInEditor, Category = "4. Refresh")
    void RefreshBuildAreaAndOverlays();

private:
#if WITH_EDITOR
    void ApplyAllDefinitionsInternal(int32& OutCount);
    void NormalizeAllTransformsInternal(int32& OutCount);
    int32 WeldSharedBoundariesInternal();
    int32 AutoFillNeighborsInternal();
    void RefreshInternal();
    void ValidateInternal();
    FVector GetSpawnLocation() const;
    FName MakeUniqueAreaId(const TCHAR* Prefix) const;
    void SelectInEditor(AActor* Actor) const;
#endif
};
