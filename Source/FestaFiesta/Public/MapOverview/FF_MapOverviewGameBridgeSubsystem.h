#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "MapOverview/FF_MapOverviewBridge.h"
#include "FF_MapOverviewGameBridgeSubsystem.generated.h"

class AFFZSPlacementPreviewActor;

// 실행 중인 건축 툴바에만 전체 지도 버튼을 붙인다. 해제할 때 원래 폭과 슬롯 배치를 복구한다.
namespace FFMapOverviewToolbar
{
    FESTAFIESTA_API bool AttachAfterGridToRow(UUserWidget& Tools, UUserWidget& Addon);
    FESTAFIESTA_API void DetachFromRow(UUserWidget& Tools, UUserWidget& Addon);
}

/**
 * 지도 플러그인의 추상 계약을 프로젝트의 HUD·건축·카메라 API로 연결한다.
 * 엔진 수명이 월드보다 길기 때문에 아래 캐시는 약한 참조만 보관하며, 종료 시 모듈 기능 등록을 해제한다.
 */
UCLASS()
class FESTAFIESTA_API UFF_MapOverviewGameBridgeSubsystem final : public UEngineSubsystem,
    public IFFMapOverviewBridge
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool IsHudContextReady(UWorld* World) const override;
    virtual bool IsInputBlocked(UWorld* World) const override;
    virtual bool MountHudWidget(UWorld* World, UUserWidget* Widget) override;
    virtual bool MountWholeGridToolbar(UWorld* World, UUserWidget* Widget) override;
    virtual void UnmountWholeGridToolbar(UWorld* World, UUserWidget* Widget) override;
    virtual bool IsBuildSessionActive(UWorld* World) const override;
    virtual bool IsGridVisible(UWorld* World) const override;
    virtual void SetGridVisible(UWorld* World, bool bVisible) override;
    virtual int32 GetVisualGridStepCm(UWorld* World) const override;
    virtual bool TryGetPlacementContext(UWorld* World, FFFZSCommand& OutCommand) override;
    virtual void NotifyMapInteraction(UWorld* World) override;
    virtual bool TryGetCameraFocus(APlayerController* Controller, FVector& OutFocus) const override;
    virtual bool TryGetPlayerMapLocation(APlayerController* Controller, FVector& OutLocation) const override;
    virtual void StopCameraMotion(APlayerController* Controller) override;
    virtual bool FocusMapPoint(APlayerController* Controller, const FVector& Location) override;
    virtual bool EnterMapView(APlayerController* Controller, AActor* Target) override;
    virtual bool ExitMapView(APlayerController* Controller, AActor* PreviousTarget) override;
private:
    TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<UUserWidget>> ToolbarCache;
    TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<UUserWidget>> ToolbarAddons;
    TMap<TWeakObjectPtr<UWorld>, TWeakObjectPtr<AFFZSPlacementPreviewActor>> PreviewCache;
    TMap<TWeakObjectPtr<APlayerController>, uint8> PreviousViewModes;
    TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<AActor>> MapViewTargets;
    TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<APawn>> MapViewCharacterPawns;
};
