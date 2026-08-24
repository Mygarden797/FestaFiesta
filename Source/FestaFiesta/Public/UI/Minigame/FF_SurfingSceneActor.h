// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Minigame/Surfing/FF_SurfingTypes.h"
#include "FF_SurfingSceneActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
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

	void InitializeScene(
		const FFF_SurfingVisualStyle& VisualStyle,
		UTextureRenderTarget2D* RenderTarget);
	void ReleaseScene();
	void ApplySnapshot(const FFF_SurfingSnapshot& Snapshot);

	bool IsSceneReady() const;
	int32 GetVisibleCourseObjectCount() const;
	int32 GetRiderMeshPartCount() const;
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

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> OceanFloor;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> OceanSurface;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<USceneComponent> SurferRoot;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> Surfboard;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<USceneComponent> RiderRoot;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> RiderBody;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> RiderHead;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> RiderLeftArm;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> RiderRightArm;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> RiderLeftLeg;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> RiderRightLeg;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> WakeLeft;

	UPROPERTY(VisibleAnywhere, Category = "Surfing 3D")
	TObjectPtr<UStaticMeshComponent> WakeRight;

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
	TObjectPtr<UMaterialInstanceDynamic> FoamMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoardMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoostBoardMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RiderMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RiderSkinMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ObjectMaterials;

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<USceneComponent>> CourseObjectRoots;

	UPROPERTY(Transient)
	TMap<int32, EFF_SurfingObjectType> CourseObjectTypes;

	TMap<int32, FVector> CourseObjectTargetLocations;

	float ArenaHalfForwardWorld = 1450.0f;
	float ArenaHorizontalMetricScale = 2.18f;
	FVector CameraFollowOffset = FVector(-1450.0, 0.0, 900.0);
	FVector CameraLookAtOffset = FVector(480.0, 0.0, 70.0);
	float SceneSeconds = 0.0f;
	float EventPulseRemainingSeconds = 0.0f;
	int32 PresentedEventSerial = 0;
	FVector PlayerTargetLocation = FVector(0.0, 0.0, 80.0);
	float PlayerTargetYaw = 0.0f;
	bool bHasPlayerTransform = false;
	bool bBoosting = false;
	bool bInvincible = false;
	bool bSceneInitialized = false;
};
