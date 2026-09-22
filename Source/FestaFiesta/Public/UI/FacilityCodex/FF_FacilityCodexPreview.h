#pragma once

#include "CoreMinimal.h"
#include "Components/Viewport.h"
#include "FF_FacilityCodexPreview.generated.h"

class AStaticMeshActor;
class UStaticMesh;
class USkyLightComponent;
class UTextureCube;
struct FStreamableHandle;

/** A display-only mesh in UViewport's isolated world; never spawns a facility/gameplay actor. */
UCLASS(meta=(DisplayName="FF Facility Codex Preview"))
class FESTAFIESTA_API UFF_FacilityCodexPreview final : public UViewport
{
	GENERATED_BODY()
public:
	UFF_FacilityCodexPreview(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category="Facility Codex")
	void ShowMesh(TSoftObjectPtr<UStaticMesh> Mesh);

	UFUNCTION(BlueprintCallable, Category="Facility Codex")
	void ResetPreview();

	/** Relative rotation in degrees around the centered model. */
	UFUNCTION(BlueprintCallable, Category="Facility Codex")
	void RotatePreview(float YawDelta);

	/** Positive delta zooms in; a typical button step is 0.15. */
	UFUNCTION(BlueprintCallable, Category="Facility Codex")
	void SetZoom(float Delta);

	UFUNCTION(BlueprintCallable, Category="Facility Codex")
	void ResetView();

	/** Call from the owning screen's tick; only a changed, laid-out size updates the view. */
	void RefreshViewForSize();

	UFUNCTION(BlueprintPure, Category="Facility Codex")
	bool IsPreviewReady() const;

	UFUNCTION(BlueprintPure, Category="Facility Codex")
	UStaticMesh* GetLoadedMesh() const { return LoadedMesh; }

	TSoftObjectPtr<UStaticMesh> GetRequestedMesh() const { return RequestedMesh; }
	float GetPreviewYaw() const { return PreviewYaw; }
	float GetPreviewZoom() const { return PreviewZoom; }

	/** The owner chooses whether to reveal the viewport or keep its authored thumbnail. */
	FSimpleMulticastDelegate OnPreviewReadyChanged;

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void CancelLoad();
	void LoadRequestedMesh();
	void InstallMesh(UStaticMesh* Mesh, uint64 Serial);
	void ClearActor();
	void ApplyView();
	void ConfigurePreviewLighting();
	void UpdatePreviewLighting();
	void SetReady(bool bReady);

	/** A serialized hard reference keeps this small runtime Engine texture in the cook. */
	UPROPERTY() TObjectPtr<UTextureCube> PreviewSkyCubemap;
	UPROPERTY(Transient) TSoftObjectPtr<UStaticMesh> RequestedMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> LoadedMesh;
	UPROPERTY(Transient) TWeakObjectPtr<AStaticMeshActor> PreviewActor;
	UPROPERTY(Transient) TWeakObjectPtr<USkyLightComponent> PreviewSkyLight;
	TSharedPtr<FStreamableHandle> LoadHandle;
	uint64 RequestSerial = 0;
	FVector MeshCenter = FVector::ZeroVector;
	FVector2D LastViewportSize = FVector2D::ZeroVector;
	double MeshScale = 1.0;
	float PreviewYaw = 0.f;
	float PreviewZoom = 1.f;
	bool bPreviewReady = false;
	bool bSkyCapturePending = false;
};
