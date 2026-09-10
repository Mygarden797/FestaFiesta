#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSBuildableGridActor.generated.h"

class UFFZSBuildableGridComponent;
class UFFZSWorldSubsystem;
struct FFFZSCommand;

// Session-owned overview: stays visible before and during placement/moving.
UCLASS(Transient, NotBlueprintable)
class FF_ZONESYSTEM_API AFFZSBuildableGridActor final : public AActor
{
    GENERATED_BODY()

public:
    AFFZSBuildableGridActor();
    void UpdateGrid(UFFZSWorldSubsystem& WorldSystem, const FVector& SurfacePointWorld,
        const FVector& SurfaceNormalWorld, int32 GridStepCm, const FFFZSCommand* PlacementContext = nullptr);
    void HideGrid();

private:
    // A build area owns one visible grid; gameplay regions only provide local coordinates.
    UPROPERTY(Transient)
    TObjectPtr<UFFZSBuildableGridComponent> UnifiedGrid;

    FName UnifiedCoordinateZoneId = NAME_None;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UFFZSBuildableGridComponent>> ZoneGrids;
};
