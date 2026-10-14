// WBP owns the entire presentation. Native code only binds inputs and projects world data.
#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Widget.h"
#include "FF_MapOverviewModel.h"
#include "FF_MapOverviewWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTextureRenderTarget2D;
class UFFMapOverviewMarkerWidget;
class UFFMapOverviewSubsystem;
class SFFMapOverviewCanvas;

UCLASS(meta=(DisplayName="Map Overview Data Surface"))
class FF_MAPOVERVIEW_API UFFMapOverviewCanvas : public UWidget
{
    GENERATED_BODY()
public:
    // Geometry overlays only. The WBP owns map background, mask, borders and compass.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Geometry") bool bDrawVectorLandFallback = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Geometry") bool bDrawGroundFootprints = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Geometry") bool bDrawBoundary = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Geometry") bool bDrawViewFootprint = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Geometry") FLinearColor ZoneFillColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Geometry") FLinearColor RoadColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Geometry") FLinearColor TileColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Geometry") FLinearColor BoundaryColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Geometry") FLinearColor ViewFootprintColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Geometry", meta=(ClampMin="0.1")) float BoundaryThickness = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Geometry", meta=(ClampMin="0.1")) float ViewFootprintThickness = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Layout") FVector2D SurfaceSize = FVector2D(256, 256);

    void SetSnapshot(const TSharedPtr<FFFMapOverviewSnapshot>& InSnapshot);
    void SetOwner(class UFFMapOverviewWidget* InOwner);
    void RefreshPaint();
    virtual void SynchronizeProperties() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    TSharedPtr<FFFMapOverviewSnapshot> Snapshot;
    TSharedPtr<SFFMapOverviewCanvas> MapSlate;
    TWeakObjectPtr<class UFFMapOverviewWidget> Owner;
};

UCLASS(Blueprintable)
class FF_MAPOVERVIEW_API UFFMapOverviewWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void InitializeOverview(UFFMapOverviewSubsystem* InSubsystem);
    UFUNCTION(BlueprintCallable, Category="Map Overview") void RefreshMap();
    UFUNCTION(BlueprintPure, Category="Map Overview|Diagnostics") FBox GetLocalMapBounds() const;
    UFUNCTION(BlueprintPure, Category="Map Overview|Diagnostics") FVector GetLocalMapCenter() const;
    bool IsPointerOverMapControls() const;
    UFUNCTION(BlueprintPure, Category="Map Overview|Layout") float GetMapPanelRightInsetSlateUnits() const;
    void HandleMapPress(const FVector2D& NormalizedPoint);
    void NotifyInteraction();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map Overview|Markers") TSubclassOf<UFFMapOverviewMarkerWidget> FacilityMarkerClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Map Overview|Markers") TSubclassOf<UFFMapOverviewMarkerWidget> CameraMarkerClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Map Overview|Range", meta=(ClampMin="100.0", Units="cm"))
    double LocalMapDiameterCm = 10000.0;
    UFUNCTION(BlueprintImplementableEvent, Category="Map Overview")
    void OnOverviewStateChanged(bool bWholeGrid, bool bWholeMapView, bool bGridVisible, bool bBuildSession);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> MapPanel;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> TerrainImage;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UFFMapOverviewCanvas> MapSurface;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UCanvasPanel> MarkerLayer;

private:
    void RebuildStaticMap();
    void ReprojectLocalMap(const FVector& UserLocation);
    void RefreshCameraMarker();
    void RefreshMarkerWidgets();
    void RefreshTerrainImage();
    uint32 ComputeBoundaryFingerprint() const;

    TWeakObjectPtr<UFFMapOverviewSubsystem> Subsystem;
    TSharedPtr<FFFMapOverviewSnapshot> Snapshot;
    int64 CachedWorldRevision = MIN_int64;
    uint64 CachedRegistryRevision = MAX_uint64;
    uint32 CachedBoundaryFingerprint = 0;
    double NextBoundaryCheck = 0.0;
    int32 CachedUiState = INDEX_NONE;
    int32 CachedGeometryOptions = INDEX_NONE;
    TArray<FFFMapOverviewWorldPolygon> WorldPolygons;
    TArray<FFFMapOverviewLine> WorldBoundary;
    TArray<FFFMapOverviewFacilityMarker> WorldFacilityMarkers;
    UPROPERTY(Transient) TMap<FGuid, TObjectPtr<UFFMapOverviewMarkerWidget>> FacilityMarkers;
    UPROPERTY(Transient) TObjectPtr<UFFMapOverviewMarkerWidget> CameraMarker;
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> CurrentTerrainTexture;
};
