// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "FF_FacilityInfoTypes.generated.h"

class UTexture2D;

/** The gameplay context that opened the shared facility inspector. */
UENUM(BlueprintType)
enum class EFF_FacilityInfoContext : uint8
{
	None UMETA(Hidden),
	Build UMETA(DisplayName = "Build Mode"),
	Live UMETA(DisplayName = "Live Operation")
};

/**
 * Immutable UI snapshot for one installed facility.
 *
 * Gameplay systems remain authoritative. The widget only renders this flattened value object,
 * so Blueprint designers never need to query Zone, Queue, Economy, or Stage subsystems directly.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilityInfoViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FGuid FacilityInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	EFF_FacilityInfoContext Context = EFF_FacilityInfoContext::None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText ContextTitle;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText CategoryText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText ZoneText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText FacilityStateText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText OperationStateText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText FootprintText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText ClearanceText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText InstallCostText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText RecoveryCostText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText QueueCapacityText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText ServiceDurationText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText WaitingPartiesText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText OccupancyText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText RemainingServiceText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText ExpectedWaitText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText ThroughputText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText FeedbackText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText ThumbnailPlaceholderText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	TObjectPtr<UTexture2D> Thumbnail = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	bool bNeedsRelocation = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	bool bCanMove = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	bool bCanRemove = false;

	/** True when a data-authored minigame is mapped to this installed facility type. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	bool bHasMinigame = false;

	/** The button is visible for mapped facilities, but enabled only while the session may start. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	bool bCanStartMinigame = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText MinigameActionText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	bool bFeedbackIsError = false;
};

/** Pure formatting policy shared by runtime presentation and automation tests. */
namespace FFFacilityInfoModel
{
	FESTAFIESTA_API FText GetCategoryText(EFFZSFacilityCategory Category);
	FESTAFIESTA_API FText GetFacilityStateText(EFFZSFacilityState State);
	FESTAFIESTA_API FText FormatFootprint(const FIntPoint& SizeCentimeters);
	FESTAFIESTA_API FText FormatDistanceMeters(int32 Centimeters);
	FESTAFIESTA_API FText FormatGold(bool bAvailable, int32 Amount);
	FESTAFIESTA_API FText FormatExpectedWait(bool bAvailable, float Seconds);
	FESTAFIESTA_API FText FormatThroughput(bool bAvailable, float PeoplePerMinute);
	FESTAFIESTA_API FText GetOperationStateText(
		bool bQueueSupported,
		bool bQueueAvailable,
		bool bIsOpen,
		bool bAcceptingNew);
}
