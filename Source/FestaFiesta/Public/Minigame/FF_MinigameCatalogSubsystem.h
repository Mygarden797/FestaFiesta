// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_MinigameCatalogSubsystem.generated.h"

class UFF_MinigameDefinition;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class EFF_MinigameCatalogLoadState : uint8
{
	NotRequested,
	Loading,
	Ready,
	Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnMinigameCatalogChanged);

/**
 * GameInstance-wide, asynchronously loaded routing table from facility IDs to
 * minigame definitions. Deterministic sorting makes duplicate handling stable.
 */
UCLASS()
class FESTAFIESTA_API UFF_MinigameCatalogSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Catalog")
	void RefreshCatalog();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Catalog")
	UFF_MinigameDefinition* FindDefinitionById(FName MinigameId) const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Catalog")
	UFF_MinigameDefinition* FindDefinitionForFacility(FName FacilityDefinitionId) const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Catalog")
	EFF_MinigameCatalogLoadState GetLoadState() const { return LoadState; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Catalog")
	bool IsCatalogReady() const { return LoadState == EFF_MinigameCatalogLoadState::Ready; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Catalog")
	int32 GetDefinitionCount() const { return Definitions.Num(); }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Minigame|Catalog")
	FFF_OnMinigameCatalogChanged OnCatalogChanged;

private:
	void HandleLoadCompleted(TSharedPtr<FStreamableHandle> CompletedHandle, uint64 Generation);
	void HandleLoadCanceled(TSharedPtr<FStreamableHandle> CanceledHandle, uint64 Generation);
	void RebuildCatalogFromLoadedAssets();
	void FinishRefresh(EFF_MinigameCatalogLoadState NewState);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_MinigameDefinition>> Definitions;

	TMap<FName, int32> DefinitionIndexById;
	TMap<FName, int32> DefinitionIndexByFacilityId;
	TArray<FPrimaryAssetId> RequestedAssetIds;
	TSharedPtr<FStreamableHandle> ActiveLoadHandle;
	uint64 RequestGeneration = 0;

	UPROPERTY(Transient)
	EFF_MinigameCatalogLoadState LoadState = EFF_MinigameCatalogLoadState::NotRequested;
};
