#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "FF_MapOverviewViewTarget.generated.h"

/** Local, transient overview camera. The original pawn stays possessed and keeps its saved view. */
UCLASS(NotPlaceable, Transient)
class AFFMapOverviewViewTarget : public ACameraActor
{
    GENERATED_BODY()

public:
    AFFMapOverviewViewTarget();

    bool InitializeMapView(APlayerController* Controller, const FBox& Bounds, const FRotator& Rotation);
    bool RefreshMapView(const FBox& Bounds);
    bool RestorePreviousView();
    virtual void Tick(float DeltaSeconds) override;

    TWeakObjectPtr<APlayerController> MapController;
    TWeakObjectPtr<AActor> PreviousViewTarget;
    bool bUsedNativeBridge = false;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool FitViewport();
    FBox MapBounds = FBox(ForceInit);
    FRotator MapRotation = FRotator(-62.0, 0.0, 0.0);
    FIntPoint FittedViewportSize = FIntPoint::ZeroValue;
    float FittedViewportScale = 0.0f;
    float FittedRightInsetSlateUnits = 0.0f;
    bool bRestored = false;
};
