#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_MapOverviewSubsystem.generated.h"

class AFFMapGridActor;
class AFFZSBuildableGridActor;
class UFFMapOverviewWidget;
class USceneComponent;
class UFFMapTerrainCapture;
class UTextureRenderTarget2D;
class UFFMapWholeGridButtonWidget;

// Optional presenter only. Construction authority and the existing input/session owner remain intact.
UCLASS()
class FF_MAPOVERVIEW_API UFFMapOverviewSubsystem final : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    UFFMapOverviewSubsystem();
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickableWhenPaused() const override { return true; }

    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map") bool IsWholeGridEnabled() const { return bWholeGrid; }
    UFUNCTION(BlueprintCallable, Category="FestaFiesta|Map") void SetWholeGridEnabled(bool bEnabled);
    UFUNCTION(BlueprintCallable, Category="FestaFiesta|Map") void ToggleWholeGrid();
    UFUNCTION(BlueprintCallable, Category="FestaFiesta|Map") void ToggleWholeMapView();
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map") bool IsWholeMapView() const;
    UFUNCTION(BlueprintCallable, Category="FestaFiesta|Map") void FocusMapLocation(FVector Location);
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map") FBox GetMapBounds() const { return MapBounds; }
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map") bool IsGridVisible() const;
    UFUNCTION(BlueprintCallable, Category="FestaFiesta|Map") void ToggleGridVisibility();
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map") bool IsBuildSessionActive() const;
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map") bool TryGetCameraFocus(FVector& OutFocus) const;
    void NotifyMapInteraction();
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map") UTextureRenderTarget2D* GetTerrainMapTexture() const;
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map") FBox GetTerrainMapBounds() const;
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map") bool TryGetPlayerMapLocation(FVector& OutLocation) const;
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map|Diagnostics") int32 GetGridCellCount() const;
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map|Diagnostics") int32 GetGridLineCount() const;
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map|Diagnostics") int32 GetGridTraceBudget() const;
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map|Diagnostics") double GetGridSpacingCm() const;
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map|Diagnostics") double GetGridZoomSpacingPixels() const;
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map|Diagnostics") int32 GetGridZoomStepCm() const;
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map|Diagnostics") int32 GetGridBaseStepCm() const;
    UFUNCTION(BlueprintPure, Category="FestaFiesta|Map|Diagnostics") double GetGridLastUpdateMs() const { return LastGridUpdateMs; }

    // Presentation diagnostics for automation / profiling, never construction state.
    AFFMapGridActor* GetGridActor() const { return GridActor; }
    UFFMapOverviewWidget* GetMapWidget() const { return MapWidget; }
    UFFMapWholeGridButtonWidget* GetWholeGridToolbarWidget() const { return WholeGridToolbarWidget; }

private:
    void DiscoverGridActor(AActor* Actor);
    void SetLegacyGridSuppressed(bool bSuppress);
    bool ResolveSurface(FVector& OutPoint, FVector& OutNormal) const;
    void RefreshPresentation();

    UPROPERTY(Transient) TObjectPtr<AFFMapGridActor> GridActor;
    UPROPERTY(Transient) TObjectPtr<UFFMapOverviewWidget> MapWidget;
    UPROPERTY(Transient) TObjectPtr<UFFMapWholeGridButtonWidget> WholeGridToolbarWidget;
    UPROPERTY(Transient) TObjectPtr<UFFMapTerrainCapture> TerrainCapture;
    // A hard class reference also makes the designer WBP and its brushes cook dependencies.
    UPROPERTY() TSubclassOf<UFFMapOverviewWidget> MapWidgetClass;
    UPROPERTY() TSubclassOf<UFFMapWholeGridButtonWidget> WholeGridToolbarClass;
    bool bTriedLoadWidgetClass = false;
    bool bTriedLoadToolbarClass = false;
    TArray<TWeakObjectPtr<AFFZSBuildableGridActor>> LegacyGridActors;
    TMap<TWeakObjectPtr<USceneComponent>, bool> SuppressedComponents;
    FDelegateHandle ActorSpawnHandle;
    FBox MapBounds = FBox(ForceInit);
    bool bWholeGrid = false;
    bool bForceRefresh = true;
    float RefreshAccumulator = 0.0f;
    float BoundsAccumulator = 1.0f;
    double LastGridUpdateMs = 0.0;
};
