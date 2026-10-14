#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"
#include "FF_FFZSTypes.h"

class AActor;
class APlayerController;
class UUserWidget;
class UWorld;

// Header-only contract: the optional presentation plugin never depends on the game module.
// The game adapter is the only place that knows the existing HUD/build/camera classes.
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
