#pragma once

#include "CoreMinimal.h"
#include "FF_FacilityUpgradeTypes.generated.h"

/** One immutable quote. The purchase endpoint always validates it again. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilityUpgradePreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	FGuid FacilityInstanceId;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	int32 CurrentLevel = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	int32 TargetLevel = 1;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	int32 MaxLevel = 3;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	int32 CostGold = 0;
	/** Signed balance after the quoted cost. Negative means the amount still needed, not an allowed debt. */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	int32 GoldAfterUpgrade = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	float CurrentServiceDurationSeconds = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	float NextServiceDurationSeconds = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	float CurrentThroughputPerMinute = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	float NextThroughputPerMinute = 0.f;
	/** Per-person estimated settlement, using the same final rounding as an actual party payment. */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade|Revenue")
	int32 BaseRevenuePerUseGold = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade|Revenue")
	int32 CurrentRevenuePerUseGold = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade|Revenue")
	int32 NextRevenuePerUseGold = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade|Revenue")
	float CurrentRevenueMultiplier = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade|Revenue")
	float NextRevenueMultiplier = 1.f;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade|Revenue")
	bool bRevenueAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	FIntPoint CurrentFootprintCentimeters = FIntPoint::ZeroValue;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	FIntPoint NextFootprintCentimeters = FIntPoint::ZeroValue;
	/** False removes all level/upgrade UI for facilities without a level system. */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	bool bHasLevels = false;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	bool bSupported = false;
	/** A resolved authored next tier and price; zero cost is a valid quote. */
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	bool bHasUpgradeQuote = false;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	bool bCanUpgrade = false;
	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	FText FailureReason;
};

/** Gold pricing shared by the runtime quote and purchase validation. */
namespace FFFacilityUpgradeRules
{
	FESTAFIESTA_API bool ResolveCost(int32 InstallCost, float CostMultiplier, int32& OutCost);
}
