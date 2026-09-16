// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "FF_FacilityInfoTypes.generated.h"

class UTexture2D;
struct FFF_FacilityUpgradePreview;
struct FFF_FacilitySynergyPreview;

/** The gameplay context that opened the shared facility inspector. */
UENUM(BlueprintType)
enum class EFF_FacilityInfoContext : uint8
{
	None UMETA(Hidden),
	Build UMETA(DisplayName = "Build Mode"),
	Live UMETA(DisplayName = "Live Operation")
};

/** Local inspector navigation; changing pages never issues a gameplay command. */
UENUM(BlueprintType)
enum class EFF_FacilityInfoPage : uint8
{
	Overview,
	Operation,
	Upgrade,
	RemoveConfirmation,
	/** Append only: existing WBP navigation values stay stable. */
	Synergy
};

/** One authored combination; repeated nearby instances are summarized in this row. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilityInfoSynergyEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="FestaFiesta|Facility Info|Synergy") FName RuleId;
	/** 실제 건축 카드의 식별자입니다. 표시 이름에서 추론하지 않습니다. */
	UPROPERTY(BlueprintReadOnly, Category="FestaFiesta|Facility Info|Synergy") FName PartnerFacilityDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category="FestaFiesta|Facility Info|Synergy") FText TitleText;
	UPROPERTY(BlueprintReadOnly, Category="FestaFiesta|Facility Info|Synergy") FText PartnerText;
	UPROPERTY(BlueprintReadOnly, Category="FestaFiesta|Facility Info|Synergy") FText EffectText;
	UPROPERTY(BlueprintReadOnly, Category="FestaFiesta|Facility Info|Synergy") FText ConditionText;
	UPROPERTY(BlueprintReadOnly, Category="FestaFiesta|Facility Info|Synergy") FText StatusText;
	UPROPERTY(BlueprintReadOnly, Category="FestaFiesta|Facility Info|Synergy") bool bActive = false;
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
	FName FacilityDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	bool bIsFavorite = false;

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

	/** Current one-person settlement estimate, visible even when the facility has no levels. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	FText FacilityRevenueText;

	UPROPERTY(BlueprintReadOnly, Category="FestaFiesta|Facility Info|Synergy")
	FText SynergySummaryText;
	UPROPERTY(BlueprintReadOnly, Category="FestaFiesta|Facility Info|Synergy")
	TArray<FFF_FacilityInfoSynergyEntry> SynergyEntries;

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

	/** Number of committed facilities in this world that share FacilityDefinitionId. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Build Limit")
	int32 CurrentInstancesInWorld = 0;

	/** Data-authored world cap. Zero means this facility definition has no count limit. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Build Limit")
	int32 MaxInstancesPerWorld = 0;

	/** Localized current/max summary. Unlimited definitions still show the committed count. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Build Limit")
	FText InstanceLimitText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Build Limit")
	bool bInstanceLimitReached = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText QueueCapacityText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info")
	FText ServiceDurationText;

	/** The level the player reviewed; the purchase command rejects a stale level. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	int32 CurrentUpgradeLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	FText FacilityLevelText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	FText UpgradeEffectText;

	/** Parallel, labelled current/quoted values for the game panel; never parsed from display text. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	FText UpgradeCurrentText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	FText UpgradeNextText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	FText UpgradeSizeEffectText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	FText UpgradeRevenueEffectText;

	/** Facilities without authored levels have neither a level label nor an upgrade section. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	bool bHasFacilityLevels = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	FText UpgradeCostText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	FText UpgradeActionText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	FText UpgradeDisabledReasonText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	bool bShowUpgradeDetails = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	bool bShowUpgradeAction = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	bool bCanUpgrade = false;

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

	/** Lighting is independently editable in Build and Live for facilities with an authored rig. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Lighting")
	bool bHasLightingRig = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Lighting")
	bool bCanEditLighting = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Lighting")
	FFFZSFacilityLightingState LightingState;

	/** True when the inspector should expose today's paid-program state for this facility. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Program")
	bool bShowProgramSection = false;

	/** At least one of today's reservations targets this installed facility. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Program")
	bool bHasScheduledProgram = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Program")
	FText ProgramStatusText;

	/** Flattened one-line-per-program summary: display name, relative time, and purchase cost. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Program")
	FText ScheduledProgramSummaryText;

	/** Build-context action visibility. Live keeps an existing assignment read-only. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Program")
	bool bShowProgramScheduleAction = false;

	/** Opens the picker when compatible cards can be previewed; each confirm action is gated again. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Program")
	bool bCanScheduleProgram = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Program")
	FText ProgramScheduleActionText;

	/** Empty while enabled; otherwise a player-facing explanation for the disabled CTA. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Program")
	FText ProgramScheduleDisabledReasonText;

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
	/** Uses current-location preview truth, with eligibility guarding hypothetical relocation bonuses. */
	FESTAFIESTA_API void ApplySynergyPreview(const FFF_FacilitySynergyPreview* Preview,
		bool bSelectionEligible, float SelectedBonusPercent, const TMap<FName, FText>& FacilityNames,
		FFF_FacilityInfoViewData& ViewData);
	/** Replaces all upgrade fields, including when the next snapshot is unavailable or read-only. */
	FESTAFIESTA_API void ApplyUpgradePreview(
		const FFF_FacilityUpgradePreview* Preview, FFF_FacilityInfoViewData& ViewData);
	FESTAFIESTA_API FText GetCategoryText(EFFZSFacilityCategory Category);
	FESTAFIESTA_API FText GetFacilityStateText(EFFZSFacilityState State);
	FESTAFIESTA_API FText FormatFootprint(const FIntPoint& SizeCentimeters);
	FESTAFIESTA_API FText FormatDistanceMeters(int32 Centimeters);
	FESTAFIESTA_API FText FormatGold(bool bAvailable, int32 Amount);
	FESTAFIESTA_API FText FormatProgramTimeRange(float StartMinutes, float EndMinutes);
	FESTAFIESTA_API FText FormatScheduledProgramSummary(
		const FText& DisplayName,
		float StartMinutes,
		float EndMinutes,
		bool bCostAvailable,
		int32 CostGold);
	FESTAFIESTA_API FText FormatExpectedWait(bool bAvailable, float Seconds);
	FESTAFIESTA_API FText FormatThroughput(bool bAvailable, float PeoplePerMinute);
	FESTAFIESTA_API FText GetOperationStateText(
		bool bQueueSupported,
		bool bQueueAvailable,
		bool bIsOpen,
		bool bAcceptingNew);
}
