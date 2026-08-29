// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Minigame/ClawMachine/FF_ClawMachineTypes.h"
#include "FF_ClawMachineArena3D.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UFF_ClawMachineDefinition;

/**
 * Lightweight real-time 3D claw cabinet for UMG's isolated UViewport world.
 *
 * Native primitives are a complete, package-safe fallback. Designers can either assign
 * per-prize meshes/materials on the definition or replace the whole actor with a Blueprint child.
 */
UCLASS(Blueprintable, Transient, meta = (DisplayName = "FF Claw Machine 3D Arena"))
class FESTAFIESTA_API AFF_ClawMachineArena3D : public AActor
{
	GENERATED_BODY()

public:
	AFF_ClawMachineArena3D();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FestaFiesta|Minigame|Claw Machine|3D")
	void ConfigureMachine(
		const UFF_ClawMachineDefinition* Definition,
		const FFF_ClawMachineSnapshot& InitialSnapshot);
	virtual void ConfigureMachine_Implementation(
		const UFF_ClawMachineDefinition* Definition,
		const FFF_ClawMachineSnapshot& InitialSnapshot);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "FestaFiesta|Minigame|Claw Machine|3D")
	void ApplyMachineSnapshot(const FFF_ClawMachineSnapshot& Snapshot);
	virtual void ApplyMachineSnapshot_Implementation(const FFF_ClawMachineSnapshot& Snapshot);

	/** Drives the visible cabinet joystick; it is presentation-only and never changes gameplay. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Claw Machine|3D")
	void SetOperatorInput(FVector2D InputAxis);

	static FVector NormalizedToPrizeFloor(const FVector2D& PositionNormalized);

	/** Front-most cabinet surface used by the presentation camera's anti-clipping solver. */
	static constexpr float GetCabinetFrontPlaneX() { return -420.0f; }

private:
	UStaticMeshComponent* CreatePrimitive(
		FName Name,
		UStaticMesh* Mesh,
		const FVector& Location,
		const FRotator& Rotation,
		const FVector& Scale,
		bool bCastShadow = true);
	UStaticMeshComponent* CreateRuntimePrimitive(
		FName Name,
		UStaticMesh* Mesh,
		const FVector& Scale);
	UMaterialInstanceDynamic* ApplyColor(
		UStaticMeshComponent* Mesh,
		const FLinearColor& Color,
		float Roughness = 0.48f,
		float Metallic = 0.05f);
	void BuildPrizeComponents(
		const UFF_ClawMachineDefinition* Definition,
		const FFF_ClawMachineSnapshot& Snapshot);
	void UpdateClaw(const FFF_ClawMachineSnapshot& Snapshot);
	void SimulateMechanicalPresentation(float DeltaSeconds);
	void ApplySimulatedClaw(const FFF_ClawMachineSnapshot& Snapshot);
	void UpdateOperatorControls(float DeltaSeconds);
	void UpdatePrizes(const FFF_ClawMachineSnapshot& Snapshot);
	static float CalculateTargetHubZ(const FFF_ClawMachineSnapshot& Snapshot);

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "FestaFiesta|Minigame|Claw Machine|3D|Art",
		meta = (AllowPrivateAccess = "true", DisplayName = "기본 기계 머티리얼"))
	TObjectPtr<UMaterialInterface> ArenaMaterial;

	/** ImageGen-backed interior panel material imported by the content bootstrap script. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
		Category = "FestaFiesta|Minigame|Claw Machine|3D|Art",
		meta = (AllowPrivateAccess = "true", DisplayName = "내부 배경 머티리얼"))
	TObjectPtr<UMaterialInterface> BackdropMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ConeMesh;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CabinetTealParts;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CabinetCoralParts;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CabinetCreamParts;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CabinetGoldParts;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BackdropPanel;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Carriage;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Cable;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ClawHub;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> JoystickStem;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> JoystickBall;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ClawTines;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ClawTips;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PrizeBodies;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PrizeAccents;

	TWeakObjectPtr<const UFF_ClawMachineDefinition> ConfiguredDefinition;
	FFF_ClawMachineRules ResolvedRules;
	FFF_ClawMachineSnapshot TargetSnapshot;
	FVector SimulatedGantryLocation = FVector::ZeroVector;
	FVector GantryVelocity = FVector::ZeroVector;
	float SimulatedHubZ = 0.0f;
	float HubVerticalVelocity = 0.0f;
	FVector2D CableSwingRadians = FVector2D::ZeroVector;
	FVector2D CableSwingVelocity = FVector2D::ZeroVector;
	FVector SimulatedHubLocation = FVector::ZeroVector;
	FVector SimulatedHeldPrizeLocation = FVector::ZeroVector;
	FVector HeldPrizeVelocity = FVector::ZeroVector;
	FVector2D TargetOperatorInput = FVector2D::ZeroVector;
	FVector2D SmoothedOperatorInput = FVector2D::ZeroVector;
	bool bBaseMaterialsInitialized = false;
	bool bHasTargetSnapshot = false;
	bool bPhysicalStateInitialized = false;
	bool bWasCarryingPrize = false;
};
