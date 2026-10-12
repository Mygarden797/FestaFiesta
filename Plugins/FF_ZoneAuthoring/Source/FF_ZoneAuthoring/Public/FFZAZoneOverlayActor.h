// FFZAZoneOverlayActor.h
// 역할: 건축 모드에서 바닥에 "어디에 지을 수 있는지"를 색으로 깔아 보여 주는 오버레이.
//
//  - Zone: 정의 에셋 색(반투명) + 외곽선
//  - 잠긴 구획(Plot): 회색
//  - 구역(District): 구역 색 외곽선(+옅은 채우기)
//  - 보호 영역(Protected Area, 통로/건설 금지): 빨강
//
// 바닥 높이를 따라 붙으므로 경사·언덕에서도 떠 보이거나 묻히지 않는다.
// 레벨에 하나만 놓으면 된다. 게임 중에는 기본으로 숨겨져 있다가 건축 모드에 들어가면 자동으로 켜진다.
//
// 건축 모드 자동 연동은 "느슨한 결합"이다. 게임 모듈(FestaFiesta)의 건축 서브시스템을
// 클래스 경로와 이벤트 이름으로 찾아 연결하므로 다른 팀원 코드를 수정하거나 include하지 않는다.
// 찾지 못하면 조용히 넘어가며, 그때는 블루프린트에서 SetOverlayVisible을 호출하면 된다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"
#include "FFZAZoneOverlayActor.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;
class USceneComponent;

UCLASS(meta = (DisplayName = "FF Zone Overlay"))
class FF_ZONEAUTHORING_API AFFZAZoneOverlayActor : public AActor
{
    GENERATED_BODY()

public:
    AFFZAZoneOverlayActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Overlay")
    TObjectPtr<USceneComponent> SceneRoot;

    // 메시 데이터는 레벨 파일에 저장하지 않는다(Transient). 에디터/게임에서 필요할 때 다시 만든다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Overlay")
    TObjectPtr<UProceduralMeshComponent> OverlayMesh;

    // ---------- 무엇을 그릴까 ----------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Layers")
    bool bShowZones = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Layers")
    bool bShowLockedPlots = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Layers")
    bool bShowDistricts = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Layers")
    bool bShowProtectedAreas = true;

    // ---------- 모양 ----------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Look", meta = (
        ToolTip = "반투명 + 정점 색을 쓰는 머티리얼입니다. 플러그인의 Content/Python 스크립트로 자동 생성할 수 있습니다."))
    TSoftObjectPtr<UMaterialInterface> OverlayMaterial;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FillOpacity = 0.22f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DistrictFillOpacity = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Look", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float OutlineOpacity = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Look", meta = (ClampMin = "0.0", Units = "cm"))
    float OutlineWidthCm = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Look", meta = (ClampMin = "0.0", Units = "cm",
        ToolTip = "바닥보다 이만큼 띄워 깜빡임(Z-fighting)을 막습니다."))
    float SurfaceLiftCm = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Look")
    FLinearColor ProtectedAreaColor = FLinearColor(1.0f, 0.10f, 0.10f, 1.0f);

    // ---------- 품질/성능 ----------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Quality", meta = (
        ToolTip = "켜면 지면을 따라 붙습니다(라인 트레이스 사용). 끄면 각 영역 액터 높이에 평평하게 깝니다."))
    bool bFollowGround = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Quality", meta = (ClampMin = "50.0", Units = "cm",
        ToolTip = "삼각형 한 변의 최대 길이입니다. 작을수록 굴곡을 잘 따르지만 무거워집니다."))
    float MaxTriangleEdgeCm = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Quality", meta = (ClampMin = "100.0", Units = "cm"))
    float GroundTraceHalfHeightCm = 5000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Quality", meta = (ClampMin = "0"))
    int32 MaxGroundTraces = 40000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Quality", meta = (ClampMin = "1000"))
    int32 MaxTriangles = 200000;

    // ---------- 보이기/숨기기 ----------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Visibility")
    bool bShowInEditor = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Visibility")
    bool bVisibleOnBeginPlay = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Visibility|Build Mode Link", meta = (
        ToolTip = "켜면 게임의 건축 모드 상태 이벤트를 찾아 자동으로 켜고 끕니다(Closed=숨김, 그 외=보임)."))
    bool bAutoShowInBuildMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Visibility|Build Mode Link", meta = (
        MetaClass = "/Script/Engine.WorldSubsystem",
        ToolTip = "건축 모드 서브시스템 클래스 경로입니다. 클래스 이름이 바뀌면 여기만 고치면 됩니다."))
    FSoftClassPath BuildModeSubsystemClass = FSoftClassPath(TEXT("/Script/FestaFiesta.FF_BuildModeSubsystem"));

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Overlay|Visibility|Build Mode Link", meta = (
        ToolTip = "상태 변경 이벤트 이름입니다. (이전 상태, 새 상태) 두 개의 1바이트 enum 인자를 가져야 합니다."))
    FName BuildModeStateEventName = TEXT("OnBuildModeStateChanged");

    // 모든 레이어를 다시 그린다(지면 높이도 새로 잰다).
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Overlay")
    void RebuildOverlay();

    UFUNCTION(BlueprintCallable, Category = "Overlay")
    void SetOverlayVisible(bool bVisible);

    UFUNCTION(BlueprintPure, Category = "Overlay")
    bool IsOverlayVisible() const;

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostRegisterAllComponents() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 건축 모드 이벤트 수신(인자는 EFF_BuildModeState를 1바이트로 받는다. 0 = Closed).
    UFUNCTION()
    void HandleBuildModeStateChanged(uint8 PreviousState, uint8 NewState);

    UFUNCTION()
    void HandleAuthoringLayersChanged();

    void RebuildInternal(bool bResetGroundCache);
    void BindBuildMode();
    void UnbindBuildMode();
    UMaterialInterface* ResolveMaterial() const;
    double SampleGroundHeight(const FVector2D& Point, double FallbackZ);

    TMap<FIntPoint, double> GroundCache;
    int32 GroundTracesUsed = 0;
    TArray<TWeakObjectPtr<AActor>> GroundIgnoredActors;

    TWeakObjectPtr<UObject> BoundBuildModeSubsystem;
    bool bLayersDelegateBound = false;
};
