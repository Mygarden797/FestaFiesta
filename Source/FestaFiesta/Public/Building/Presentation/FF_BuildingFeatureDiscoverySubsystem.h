#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_BuildingFeatureDiscoverySubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnBuildingFeatureSeen, FName, FeatureId);

/** One-time Building UI explanations, stored with the local user's preferences. */
UCLASS()
class FESTAFIESTA_API UFF_BuildingFeatureDiscoverySubsystem : public UGameInstanceSubsystem
{

	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	bool HasSeenFeature(FName FeatureId) const;
	static FName LightingControlsFeatureId() { return TEXT("LightingControls"); }
	static FName FacilityCodexFeatureId() { return TEXT("FacilityCodex"); }
	/** Call after the player confirms the explanation, not simply when it is shown. */
	bool MarkFeatureSeen(FName FeatureId);
	UPROPERTY(BlueprintAssignable, Category = "Building|First Use")
	FFF_OnBuildingFeatureSeen OnFeatureSeen;

private:
	UPROPERTY(Transient) TSet<FName> SeenFeatureIds;
};
