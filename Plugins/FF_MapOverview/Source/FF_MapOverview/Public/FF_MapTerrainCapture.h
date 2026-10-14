#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FF_MapTerrainCapture.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/** A padded local terrain photograph reused while the player moves within it. */
UCLASS()
class FF_MAPOVERVIEW_API UFFMapTerrainCapture final : public UObject
{
    GENERATED_BODY()
public:
    void Refresh(UWorld& World, const FBox& Bounds, int64 Revision, const FVector& PlayerLocation);
    void Shutdown();
    UTextureRenderTarget2D* GetTexture() const { return Texture; }
    FBox GetBounds() const { return CapturedBounds; }
private:
    UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> Texture;
    FBox CapturedBounds = FBox(ForceInit);
    int64 CapturedRevision = MIN_int64;
    double NextCaptureTime = 0.0;
    double NextRevisionCaptureTime = 0.0;
    bool bNeedsWarmupCapture = true;
};
