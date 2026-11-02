#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"
#include "FF_FFZSTypes.h"

class AActor;
class APlayerController;
class UUserWidget;
class UWorld;

// 게임의 구현 심볼을 링크하지 않는 헤더 전용 계약이다. 선택적 지도 플러그인은 이 인터페이스만 읽고,
// 실제 HUD·건축·카메라 클래스에 대한 접근은 FF_MapOverviewGameBridgeSubsystem에서 담당한다.
class IFFMapOverviewBridge : public IModularFeature
{
public:
    static FName FeatureName() { return TEXT("FFMapOverviewBridge"); }
    static IFFMapOverviewBridge* Find()
    {
        IModularFeatures& Features = IModularFeatures::Get();
        return Features.IsModularFeatureAvailable(FeatureName())
            ? &Features.GetModularFeature<IFFMapOverviewBridge>(FeatureName()) : nullptr;
    }

    virtual bool IsHudContextReady(UWorld* World) const = 0;
    virtual bool IsInputBlocked(UWorld* World) const = 0;
    virtual bool MountHudWidget(UWorld* World, UUserWidget* Widget) = 0;
    virtual bool MountWholeGridToolbar(UWorld* World, UUserWidget* Widget) = 0;
    virtual void UnmountWholeGridToolbar(UWorld* World, UUserWidget* Widget) = 0;
    virtual bool IsBuildSessionActive(UWorld* World) const = 0;
    virtual bool IsGridVisible(UWorld* World) const = 0;
    virtual void SetGridVisible(UWorld* World, bool bVisible) = 0;
    virtual int32 GetVisualGridStepCm(UWorld* World) const = 0;
    virtual bool TryGetPlacementContext(UWorld* World, FFFZSCommand& OutCommand) = 0;
    virtual void NotifyMapInteraction(UWorld* World) = 0;
    virtual bool TryGetCameraFocus(APlayerController* Controller, FVector& OutFocus) const = 0;
    virtual bool TryGetPlayerMapLocation(APlayerController* Controller, FVector& OutLocation) const = 0;
    virtual void StopCameraMotion(APlayerController* Controller) = 0;
    virtual bool FocusMapPoint(APlayerController* Controller, const FVector& Location) = 0;
    virtual bool EnterMapView(APlayerController* Controller, AActor* Target) = 0;
    virtual bool ExitMapView(APlayerController* Controller, AActor* PreviousTarget) = 0;
};
