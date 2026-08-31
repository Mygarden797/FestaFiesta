// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Minigame/Surfing/FF_SurfingTypes.h"
#include "UI/Minigame/FF_SurfingCharacterActor.h"
#include "FF_SurfingSceneActor.generated.h"

class UChildActorComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UDirectionalLightComponent;
class UPointLightComponent;
class USceneCaptureComponent2D;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureRenderTarget2D;
struct FFF_SurfingVisualStyle;

/**
 * Lightweight 3D presentation hosted in an isolated area of the game world.
 *
 * Gameplay remains authoritative in UFF_SurfingRuntime. This actor owns only
 * render components and maps immutable normalized snapshots onto an ocean
 * plane, a surfer rig and pooled three-dimensional course props.
 */
UCLASS(NotBlueprintable, Transient)
class FESTAFIESTA_API AFF_SurfingSceneActor final : public AActor
{
	GENERATED_BODY()

public:
	AFF_SurfingSceneActor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void InitializeScene(
		const FFF_SurfingVisualStyle& VisualStyle,
		UTextureRenderTarget2D* RenderTarget);
	void ReleaseScene();
	void ApplySnapshot(const FFF_SurfingSnapshot& Snapshot);
	bool SetSurfingCharacterClass(
		TSubclassOf<AFF_SurfingCharacterActor> InCharacterClass);

	bool IsSceneReady() const;
	int32 GetVisibleCourseObjectCount() const;
	int32 GetRiderMeshPartCount() const;
	int32 GetCharacterRenderablePrimitiveCount() const;
	AFF_SurfingCharacterActor* GetSurfingCharacterActor() const;
	FVector GetPlayerSceneLocalLocation() const;
	FVector ArenaToSceneLocal(const FVector2D& PositionNormalized, float Height = 0.0f) const;
	FVector2D SceneLocalToArena(const FVector& SceneLocalPosition) const;
	bool DeprojectCapturePointToArena(
		const FVector2D& CaptureUv,
		FVector2D& OutArenaPosition) const;
	USceneCaptureComponent2D* GetSceneCaptureComponent() const { return SceneCapture; }

private:
	UMaterialInstanceDynamic* CreateColorMaterial(
		const FName MaterialName,
		const FLinearColor& Color);
	UMaterialInstanceDynamic* CreateOceanSurfaceMaterial(
		const FLinearColor& OceanColor,
		const FLinearColor& FoamColor);
	UStaticMeshComponent* CreateObjectPart(
		USceneComponent& Parent,
		int32 ObjectId,
		const TCHAR* PartName,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& RelativeLocation,
		const FRotator& RelativeRotation,
		const FVector& RelativeScale);
	USceneComponent* CreateCourseObject(
		const FFF_SurfingObjectSnapshot& ObjectSnapshot,
		float RadiusWorld);
	void DestroyCourseObject(int32 ObjectId);
	void SetCourseObjectVisible(USceneComponent& ObjectRoot, bool bVisible) const;
	UMaterialInterface* GetObjectMaterial(EFF_SurfingObjectType Type) const;
	float GetObjectHoverHeight(EFF_SurfingObjectType Type, float RadiusWorld) const;
	void ConfigureFixedVisuals();
	void UpdateWaterPresentation(float PresentationSeconds);
	void ApplyCharacterPresentationState(float BankDegrees);
	void RefreshCharacterCaptureActors();
	void ClearCharacterCaptureActors();

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UDirectionalLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> OceanFloor;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> OceanSurface;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> SkyBackdrop;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<USceneComponent> SurferRoot;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UChildActorComponent> SurfingCharacterHost;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TArray<TObjectPtr<UStaticMeshComponent>> WaveBands;

	/** Hard references keep the engine primitives in packaged builds. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BasicShapeMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ProjectSeaMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OceanMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OceanSurfaceMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkyMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FoamMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ObjectMaterials;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<USceneComponent>> CourseObjectRoots;

	UPROPERTY(Transient)
	TMap<int32, EFF_SurfingObjectType> CourseObjectTypes;

	TMap<int32, FVector> CourseObjectTargetLocations;
	TSet<int32> VisibleCourseObjectIdsLastFrame;
	TArray<TWeakObjectPtr<AActor>> CharacterCaptureActors;
	FFF_SurfingCharacterVisualStyle CharacterVisualStyle;

	float ArenaHalfForwardWorld = 1450.0f;
	float ArenaHorizontalMetricScale = 2.18f;
	FVector CameraFollowOffset = FVector(-1350.0, 0.0, 720.0);
	FVector CameraLookAtOffset = FVector(950.0, 0.0, 80.0);
	float CameraLateralFollowRatio = 0.62f;
	float PlayerPositionSmoothingSpeed = 10.0f;
	float PlayerRotationSmoothingSpeed = 8.0f;
	bool bResetPresentationLagOnDirectionChange = true;
	float CameraPositionSmoothingSpeed = 4.5f;
	float CameraRotationSmoothingSpeed = 5.5f;
	float CourseObjectSmoothingSpeed = 12.0f;
	float SurferBobAmplitudeCm = 7.0f;
	float TurnBankScale = 0.32f;
	float MaximumBankDegrees = 18.0f;
	float CharacterCaptureRefreshRemainingSeconds = 0.0f;
	float SceneSeconds = 0.0f;
	int32 PresentedEventSerial = 0;
	FVector PlayerTargetLocation = FVector(0.0, 0.0, 80.0);
	float PlayerTargetYaw = 0.0f;
	float LastPresentedLateralDirection = 0.0f;
	bool bHasPresentedLateralDirection = false;
	bool bHasPlayerTransform = false;
	bool bBoosting = false;
	bool bInvincible = false;
	bool bSceneInitialized = false;
};
