// FF_FFZSPlacementPreviewActor.h

// UI 입력을 권한 상태로 만들지 않고 배치 결과와 고스트 메시를 미리 보여 주는 액터다.
// 흐름: UI가 PreviewCommand를 갱신하고 RefreshPreview로 검증한 뒤 CommitPreview가 같은 명령을 다시 실행.
// 불변식: 미리보기 성공은 커밋 성공 보장이 아니며 커밋 시 최신 월드 리비전과 정책을 다시 검증.
//  실패/성능: 자동 갱신과 압축 영역 디버그 그리기는 비용이 있으므로 필요한 제작 도구에서만 제한적으로 사용.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSPlacementPreviewActor.generated.h"

struct FStreamableHandle;
class USceneComponent;
class UStaticMeshComponent;
class ULineBatchComponent;

// 권위 1cm 저장 격자와 별개인 사용자용 로컬 격자의 순수 계산 규칙이다.
// Actor/World 없이 자동화 테스트할 수 있어 음수 좌표, 잘못된 설정과 대형 footprint 회귀를 막는다.
namespace FFZSPlacementGridModel
{
    constexpr int32 DefaultVisualGridStepCm = 100;
    constexpr int32 MinimumVisualGridStepCm = 25;
    constexpr int32 MaximumVisualGridStepCm = 1000;

    inline int32 SanitizeGridStepCm(const int32 StepCm)
    {
        return StepCm >= MinimumVisualGridStepCm && StepCm <= MaximumVisualGridStepCm
            ? StepCm
            : DefaultVisualGridStepCm;
    }

    inline double AlignDownToGrid(const double CoordinateCm, const int32 StepCm)
    {
        const int32 SafeStepCm = SanitizeGridStepCm(StepCm);
        return FMath::FloorToDouble(CoordinateCm / SafeStepCm) * SafeStepCm;
    }

    inline double AlignUpToGrid(const double CoordinateCm, const int32 StepCm)
    {
        const int32 SafeStepCm = SanitizeGridStepCm(StepCm);
        return FMath::CeilToDouble(CoordinateCm / SafeStepCm) * SafeStepCm;
    }

    inline float ResolveGridHalfExtentCm(
        const int32 FootprintAxisCm,
        const float PaddingCm,
        const float MaximumRadiusCm,
        const int32 StepCm)
    {
        const int32 SafeStepCm = SanitizeGridStepCm(StepCm);
        const float SafePaddingCm = FMath::IsFinite(PaddingCm) ? FMath::Max(0.0f, PaddingCm) : 600.0f;
        const float SafeMaximumRadiusCm = FMath::IsFinite(MaximumRadiusCm) && MaximumRadiusCm >= SafeStepCm
            ? MaximumRadiusCm
            : 3600.0f;
        const float FootprintHalfExtentCm = FMath::Max(0, FootprintAxisCm) * 0.5f;
        const float MinimumHalfExtentCm = FMath::Max(
            FootprintHalfExtentCm,
            SafeStepCm * 2.0f);
        const float DesiredHalfExtentCm = FMath::Max(
            FootprintHalfExtentCm + SafePaddingCm,
            MinimumHalfExtentCm);
        // 반경 제한은 주변 여백을 줄이는 소프트 제한이다. 대형 시설 자체를 잘라 내면 가장
        // 정렬이 어려운 외곽을 볼 수 없으므로 footprint 반경만큼은 항상 보장한다.
        return FMath::Max(MinimumHalfExtentCm, FMath::Min(DesiredHalfExtentCm, SafeMaximumRadiusCm));
    }

    inline bool IsMajorGridCoordinate(
        const double CoordinateCm,
        const int32 MajorIntervalCm)
    {
        if (MajorIntervalCm <= 0 || !FMath::IsFinite(CoordinateCm))
        {
            return false;
        }
        const double Remainder = FMath::Abs(FMath::Fmod(CoordinateCm, MajorIntervalCm));
        return Remainder <= 0.01 || FMath::Abs(Remainder - MajorIntervalCm) <= 0.01;
    }

    inline float ResolveEdgeOpacity(
        const double CoordinateCm,
        const double CenterCm,
        const double HalfExtentCm)
    {
        if (!FMath::IsFinite(CoordinateCm) || !FMath::IsFinite(CenterCm) ||
            !FMath::IsFinite(HalfExtentCm) || HalfExtentCm <= UE_SMALL_NUMBER)
        {
            return 0.25f;
        }
        const double NormalizedDistance = FMath::Clamp(
            FMath::Abs(CoordinateCm - CenterCm) / HalfExtentCm,
            0.0,
            1.0);
        if (NormalizedDistance <= 0.65)
        {
            return 1.0f;
        }
        const float FadeAlpha = static_cast<float>((NormalizedDistance - 0.65) / 0.35);
        return FMath::Lerp(1.0f, 0.25f, FadeAlpha);
    }
}

// UI와 독립적인 비권한 배치 고스트.
// UI가 입력과 수명주기를 소유하고 이 액터는 검증 결과·비주얼·디버그 표시만 보관.
 
UCLASS(Blueprintable)
class FF_ZONESYSTEM_API AFFZSPlacementPreviewActor : public AActor
{
    GENERATED_BODY()

public:
    AFFZSPlacementPreviewActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
    TObjectPtr<USceneComponent> SceneRoot;
    // 미리보기 Transform의 루트. 권한 시설 액터나 점유 셀을 만들지 않는다. 

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
    TObjectPtr<UStaticMeshComponent> PreviewMesh;
    // 충돌과 권한이 없는 고스트 표현 컴포넌트다. 

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
    TObjectPtr<ULineBatchComponent> RangeLines;
    // footprint·clearance·queue 사각형을 한 Scene Proxy로 묶어 Shipping에서도 유지하는 선 표현이다.

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
    TObjectPtr<ULineBatchComponent> PlacementGridLines;
    // 배치·이동 중 대상 주변에만 나타나는 사용자용 로컬 격자다. 권위 점유 셀이나 충돌에는 관여하지 않는다.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview",
        meta = (DevelopmentOnly, DeprecatedProperty,
            DeprecationMessage = "Set this only from MakeInstallCommandFromDefinition."))
    FFFZSCommand PreviewCommand;
    // 다음 RefreshPreview·CommitPreview에 사용할 전체 명령 복사본이다. UI 변경 뒤 새로 검증해야 한다. 
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
    bool bAutoRefreshPreview = false;
    // false가 기본이다. UI는 값이 바뀔 때 RefreshPreview를 호출하고 자동 갱신은 필요한 제작 도구에서만 켠다. 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview", meta = (ClampMin = "0.02", ClampMax = "1.0", Units = "s"))
    float AutoRefreshIntervalSeconds = 0.1f;
    // 자동 모드에서 검증을 다시 실행하는 간격. 최소 0.02초이며 짧을수록 지면·충돌·정책 비용이 커진다. 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview")
    bool bDrawDebugCells = true;
    // 검증된 footprint·clearance·queue 압축 영역을 지속 선 컴포넌트로 표시할지 정한다.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Grid")
    bool bDrawPlacementGrid = false;
    // BuildModeSubsystem의 배치·이동 상태와 G 토글이 제어하는 로컬 격자 표시 여부다.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Grid",
        meta = (ClampMin = "25", ClampMax = "1000", Units = "cm"))
    int32 PlacementGridStepCm = FFZSPlacementGridModel::DefaultVisualGridStepCm;
    // 사용자에게 보이는 선 간격이다. 권위 저장 좌표의 1cm 계약은 바꾸지 않는다.

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview", meta = (ClampMin = "0.0", Units = "s"))
    float DebugDrawLifetimeSeconds = 0.0f;
    // 이전 DrawDebug 기반 호출자와의 직렬화 호환용 값이다. 지속 선 컴포넌트에서는 사용하지 않는다.
    
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Preview")
    FFFZSPlacementValidation RefreshPreview();
    // 현재 PreviewCommand를 읽기 전용 검증하고 LastValidation·Transform·메시·디버그 표시를 갱신.
    // @return 호출 시점의 결과. 이후 월드 변경으로 즉시 오래된 결과가 될 수 있다.

    // 권위 검증·메시·예약 영역은 건드리지 않고 로컬 격자만 다시 만든다. G 표시 토글처럼
    // 시각 상태만 바뀐 입력에서 외부 정책과 전체 배치 검증을 중복 실행하지 않게 한다.
    void RefreshPlacementGridVisual();

    // 커서가 가리킨 실제 지면의 점과 법선을 격자 표현에만 제공한다. 저장 좌표와 시설 Transform은
    // 기존 Zone 1cm 계약을 유지하고, 선만 완만한 지면 평면을 따라가도록 한다.
    void SetPlacementGridSurface(const FVector& WorldPoint, const FVector& WorldNormal);
    
    
    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Preview")
    void SetAutoRefreshEnabled(bool bEnabled);
    // 자동 갱신 Tick을 켜거나 끈다. 꺼도 수동 RefreshPreview와 CommitPreview는 사용할 수 있다. 

    UFUNCTION(BlueprintCallable, Category = "FF Zone System|Development",
        meta = (DevelopmentOnly, DeprecatedFunction,
            DeprecationMessage = "Production placement must use the catalog-backed Build Mode."))
    FFFZSCommandResult CommitPreview();
    //권한 UFFZSWorldSubsystem에 현재 명령 실행을 요청한다.
    //미리보기 후에도 최신 리비전·로컬 검증·외부 정책을 모두 다시 통과해야 한다.

    UFUNCTION(BlueprintPure, Category = "FF Zone System|Preview")
    const FFFZSPlacementValidation& GetLastValidation() const { return LastValidation; }
    // 마지막 RefreshPreview 결과의 읽기 전용 참조를 반환한다. 아직 갱신하지 않았다면 기본 실패 상태다. 
    
protected:
    virtual void BeginPlay() override;
    // 자동 갱신과 초기 표현에 필요한 런타임 상태를 준비.
    
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    // 진행 중인 소프트 메시 로드 핸들을 해제해 늦은 콜백을 막는다. 
 
    virtual void Tick(float DeltaSeconds) override;
    // 자동 갱신이 켜진 동안만 누적 간격에 맞춰 RefreshPreview를 호출한다. 

private:
    UPROPERTY(VisibleAnywhere, Category = "Preview")
    FFFZSPlacementValidation LastValidation;
    // UI 표시용 마지막 검증 캐시. 권한 판정이나 저장 상태가 아니다. 
    
    FSoftObjectPath RequestedVisualPath;
    // 비동기 완료가 현재 요청과 같은지 확인하는 메시 경로 토큰이다. 
    
    TSharedPtr<FStreamableHandle> VisualLoadHandle; 
    // 미리보기 메시 비동기 로드 수명을 보관한다. 

    FVector PlacementGridSurfacePointWorld = FVector::ZeroVector;
    FVector PlacementGridSurfaceNormalWorld = FVector::UpVector;
    bool bHasPlacementGridSurface = false;

    void RequestPreviewMesh();
    void ApplyLoadedPreviewMesh();
    void RefreshRangePresentation();
    void RefreshPlacementGridPresentation();
};
