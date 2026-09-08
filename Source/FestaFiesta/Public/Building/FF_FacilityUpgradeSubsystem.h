#pragma once

#include "CoreMinimal.h"
#include "Building/FF_FacilityUpgradeTypes.h"
#include "FF_FFZSExternalPolicy.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_FacilityUpgradeSubsystem.generated.h"

class UFFZSWorldSubsystem;
class UFF_FestivalProgressionSubsystem;

/** Purchase facade AND authoritative policy: even direct Zone commands must pay the authored price. No Tick. */
UCLASS()
class FESTAFIESTA_API UFF_FacilityUpgradeSubsystem final : public UWorldSubsystem, public IFFZSExternalPolicy
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	/** Returns true if the facility exists; bCanUpgrade and FailureReason describe purchase availability. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Facility Upgrade")
	bool GetUpgradePreview(FGuid FacilityInstanceId, FFF_FacilityUpgradePreview& OutPreview) const;

	/** ExpectedCurrentLevel is the level actually shown by the UI; a stale click never buys another level. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Upgrade")
	bool TryUpgradeFacility(FGuid FacilityInstanceId, int32 ExpectedCurrentLevel, FText& OutFailureReason);

	virtual FName GetFFZoneSystemPolicyName_Implementation() const override;
	virtual EFFZSPolicyDomain GetFFZoneSystemPolicyDomain_Implementation() const override;
	virtual FFFZSPolicyDecision PreValidateFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) const override;
	virtual FFFZSPolicyDecision ApplyTentativeFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;
	virtual void CommitFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;
	virtual void RollbackFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;

private:
	UFF_FestivalProgressionSubsystem* ResolveProgression() const;
	FFFZSPolicyDecision ValidatePurchase(const FFFZSPolicyContext& Context, int32& OutCost) const;
	UFUNCTION()
	void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);

	UPROPERTY(Transient)
	TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;
	FGuid PendingPurchaseCommand;
	bool bPolicyRegistered = false;
	bool bCommitInFlight = false;
};
