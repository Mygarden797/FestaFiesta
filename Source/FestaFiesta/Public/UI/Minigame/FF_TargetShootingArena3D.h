// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_TargetShootingArena3D.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Self-contained 3D shooting gallery rendered by the TargetShooting UMG viewport.
 *
 * Engine primitives keep the scene package-safe while still providing real depth,
 * perspective, lighting, shadows, a layered target plate and a first-person weapon.
 * Gameplay remains normalized and authoritative in UFF_TargetShootingRuntime.
 */
UCLASS(NotBlueprintable, Transient)
class FESTAFIESTA_API AFF_TargetShootingArena3D final : public AActor
{
	GENERATED_BODY()

public:
	AFF_TargetShootingArena3D();

	/** Maps the runtime's normalized play-area coordinate onto the target wall. */
	static FVector NormalizedToTargetPlane(const FVector2D& NormalizedPoint, float DepthX = 440.0f);

	void SetTargetPresentation(
		bool bVisible,
		const FVector2D& CenterNormalized,
		const FVector2D& RadiusNormalized,
		float PrecisionRatio,
		float BullseyeRatio,
		const FLinearColor& OuterColor,
		const FLinearColor& PrecisionColor,
		const FLinearColor& BullseyeColor,
		float HighlightStrength);

	void SetProjectilePresentation(
		bool bVisible,
		const FVector2D& PositionNormalized,
		const FVector2D& AimNormalized,
		float TravelAlpha,
		const FLinearColor& CoreColor,
		const FLinearColor& GlowColor);

	void SetWeaponPresentation(bool bVisible, float RecoilAlpha, float MuzzleFlashAlpha);

private:
	UStaticMeshComponent* CreatePrimitive(
		FName Name,
		UStaticMesh* Mesh,
		const FVector& Location,
		const FRotator& Rotation,
		const FVector& Scale,
		bool bCastShadow = true);
	UMaterialInstanceDynamic* ApplyColor(UStaticMeshComponent* Mesh, const FLinearColor& Color);
	void SetTargetPartTransform(
		UStaticMeshComponent* Mesh,
		const FVector& Center,
		float HorizontalRadius,
		float VerticalRadius,
		float RadiusRatio,
		float DepthX);
	void SetTargetPartsVisible(bool bVisible);

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ArenaMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> NavyEnvironment;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SteelEnvironment;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CyanLights;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> AmberLights;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> TargetArm;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> TargetCarrier;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> TargetCarrierInset;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> TargetBacking;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> TargetOuter;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> TargetPrecision;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> TargetBullseye;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TargetBolts;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ProjectileCore;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ProjectileGlow;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ProjectileTrail;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> WeaponBody;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MuzzleFlash;

	FVector WeaponRestOffset = FVector::ZeroVector;
	bool bMaterialsInitialized = false;
};
