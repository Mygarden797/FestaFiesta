// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_SurfingCharacterActor.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Small, presentation-only palette shared by native and Blueprint surfing characters.
 *
 * The owning minigame deliberately passes colours instead of scene/runtime objects so a
 * replacement character remains portable and cannot accidentally become gameplay-authoritative.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SurfingCharacterVisualStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfing Character")
	FLinearColor BoardColor = FLinearColor(1.0f, 0.52f, 0.16f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfing Character")
	FLinearColor BoostBoardColor = FLinearColor(1.0f, 0.92f, 0.24f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfing Character")
	FLinearColor OutfitColor = FLinearColor(0.95f, 0.12f, 0.035f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfing Character")
	FLinearColor SkinColor = FLinearColor(0.95f, 0.55f, 0.28f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfing Character")
	FLinearColor WakeColor = FLinearColor(0.72f, 0.98f, 0.96f, 1.0f);
};

/** Immutable presentation state supplied by the surfing scene once per simulation update. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SurfingCharacterPresentationState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfing Character")
	bool bBoosting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfing Character")
	bool bInvincible = false;

	/** Lean requested by the scene. The wrapper clamps it to a presentation-safe range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfing Character", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
	float BankDegrees = 0.0f;

	/** A new positive serial starts one short, deterministic event pulse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfing Character", meta = (ClampMin = "0"))
	int32 EventSerial = 0;
};

/**
 * Replaceable visual contract for the surfing minigame.
 *
 * Movement and collision stay in the surfing runtime/scene. The scene hosts one of these
 * actors with a ChildActorComponent and drives it through the non-virtual public wrappers.
 * Blueprint subclasses only implement the narrow presentation hooks below.
 */
UCLASS(Abstract, Blueprintable, Transient)
class FESTAFIESTA_API AFF_SurfingCharacterActor : public AActor
{
	GENERATED_BODY()

public:
	AFF_SurfingCharacterActor();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Initializes (or cleanly reinitializes) this presentation instance. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Surfing Character")
	bool InitializeCharacter(const FFF_SurfingCharacterVisualStyle& InVisualStyle);

	/** Stores a sanitized state and immediately notifies the presentation implementation. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Surfing Character")
	void ApplyPresentationState(
		const FFF_SurfingCharacterPresentationState& InPresentationState);

	/** Advances presentation-only animation. This actor intentionally never ticks itself. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Surfing Character")
	void AdvancePresentation(float DeltaSeconds);

	/** Releases presentation resources/state before a ChildActor replacement is installed. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Surfing Character")
	void ReleaseCharacter();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Surfing Character")
	bool IsCharacterReady() const { return bInitializationAttempted && bCharacterReady; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Surfing Character")
	bool HasRenderableCharacter() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Surfing Character")
	int32 GetRenderablePrimitiveCount() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Surfing Character")
	int32 GetRiderMeshPartCount() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Surfing Character")
	FFF_SurfingCharacterVisualStyle GetCharacterVisualStyle() const
	{
		return CharacterVisualStyle;
	}

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Surfing Character")
	FFF_SurfingCharacterPresentationState GetPresentationState() const
	{
		return PresentationState;
	}

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Surfing Character")
	float GetPresentationSeconds() const { return PresentationSeconds; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Surfing Character")
	float GetEventPulseAlpha() const;

	/** Returns this visual actor and all recursively attached presentation actors. */
	void GetPresentationActors(TArray<AActor*>& OutActors) const;

protected:
	/** Root exposed to replacement Blueprints as their required attachment parent. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Surfing Character")
	TObjectPtr<USceneComponent> CharacterRoot;

	/** Enforces capture isolation on every primitive currently owned by this actor. */
	void EnforcePrimitiveIsolation();

	UFUNCTION(BlueprintNativeEvent, Category = "FestaFiesta|Minigame|Surfing Character")
	void OnInitializeCharacter(const FFF_SurfingCharacterVisualStyle& InVisualStyle);
	virtual void OnInitializeCharacter_Implementation(
		const FFF_SurfingCharacterVisualStyle& InVisualStyle);

	UFUNCTION(BlueprintNativeEvent, Category = "FestaFiesta|Minigame|Surfing Character")
	void OnPresentationStateChanged(
		const FFF_SurfingCharacterPresentationState& InPresentationState);
	virtual void OnPresentationStateChanged_Implementation(
		const FFF_SurfingCharacterPresentationState& InPresentationState);

	UFUNCTION(BlueprintNativeEvent, Category = "FestaFiesta|Minigame|Surfing Character")
	void OnAdvancePresentation(float DeltaSeconds, float InPresentationSeconds);
	virtual void OnAdvancePresentation_Implementation(
		float DeltaSeconds,
		float InPresentationSeconds);

	UFUNCTION(BlueprintNativeEvent, Category = "FestaFiesta|Minigame|Surfing Character")
	void OnReleaseCharacter();
	virtual void OnReleaseCharacter_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "FestaFiesta|Minigame|Surfing Character")
	bool EvaluateCharacterReady() const;
	virtual bool EvaluateCharacterReady_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, Category = "FestaFiesta|Minigame|Surfing Character")
	int32 EvaluateRiderMeshPartCount() const;
	virtual int32 EvaluateRiderMeshPartCount_Implementation() const;

private:
	FFF_SurfingCharacterVisualStyle CharacterVisualStyle;
	FFF_SurfingCharacterPresentationState PresentationState;
	float PresentationSeconds = 0.0f;
	float EventPulseRemainingSeconds = 0.0f;
	bool bInitializationAttempted = false;
	bool bCharacterReady = false;
	bool bReleaseInProgress = false;
};

/**
 * Packaged-build-safe native fallback made entirely from engine primitive meshes.
 * Projects can replace it with a Blueprint subclass of AFF_SurfingCharacterActor.
 */
UCLASS(NotBlueprintable, Transient)
class FESTAFIESTA_API AFF_SurfingPrimitiveCharacterActor final
	: public AFF_SurfingCharacterActor
{
	GENERATED_BODY()

public:
	AFF_SurfingPrimitiveCharacterActor();

protected:
	virtual void OnInitializeCharacter_Implementation(
		const FFF_SurfingCharacterVisualStyle& InVisualStyle) override;
	virtual void OnPresentationStateChanged_Implementation(
		const FFF_SurfingCharacterPresentationState& InPresentationState) override;
	virtual void OnAdvancePresentation_Implementation(
		float DeltaSeconds,
		float InPresentationSeconds) override;
	virtual void OnReleaseCharacter_Implementation() override;
	virtual bool EvaluateCharacterReady_Implementation() const override;
	virtual int32 EvaluateRiderMeshPartCount_Implementation() const override;

private:
	UMaterialInstanceDynamic* CreateColorMaterial(
		const FName MaterialBaseName,
		const FLinearColor& Color);
	void ConfigureGeometry();
	void ConfigurePrimitive(UStaticMeshComponent& MeshComponent, bool bCastShadow);

	UPROPERTY(VisibleAnywhere, Category = "Surfing Character")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, Category = "Surfing Character")
	TObjectPtr<UStaticMeshComponent> Surfboard;

	UPROPERTY(VisibleAnywhere, Category = "Surfing Character")
	TObjectPtr<USceneComponent> RiderRoot;

	UPROPERTY(VisibleAnywhere, Category = "Surfing Character")
	TObjectPtr<UStaticMeshComponent> RiderBody;

	UPROPERTY(VisibleAnywhere, Category = "Surfing Character")
	TObjectPtr<UStaticMeshComponent> RiderHead;

	UPROPERTY(VisibleAnywhere, Category = "Surfing Character")
	TObjectPtr<UStaticMeshComponent> RiderLeftArm;

	UPROPERTY(VisibleAnywhere, Category = "Surfing Character")
	TObjectPtr<UStaticMeshComponent> RiderRightArm;

	UPROPERTY(VisibleAnywhere, Category = "Surfing Character")
	TObjectPtr<UStaticMeshComponent> RiderLeftLeg;

	UPROPERTY(VisibleAnywhere, Category = "Surfing Character")
	TObjectPtr<UStaticMeshComponent> RiderRightLeg;

	UPROPERTY(VisibleAnywhere, Category = "Surfing Character")
	TObjectPtr<UStaticMeshComponent> WakeLeft;

	UPROPERTY(VisibleAnywhere, Category = "Surfing Character")
	TObjectPtr<UStaticMeshComponent> WakeRight;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SphereMesh;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BasicShapeMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoardMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoostBoardMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OutfitMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkinMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WakeMaterial;
};
