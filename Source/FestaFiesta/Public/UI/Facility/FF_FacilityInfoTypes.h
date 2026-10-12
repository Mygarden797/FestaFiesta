// FF_FacilityInfoTypes.h
// 역할: 시설 정보 화면에 전달할 값과 비용·상태·수입의 표시 규칙을 정의한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "FF_FacilityInfoTypes.generated.h"

class UTexture2D;
struct FFF_FacilityInstanceRevenue;
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
	Synergy,
	Lighting,
	/** Separate pages keep planning and operation facts visible without scrolling. */
	Revenue,
	Programs,
	/** Staff assignment is separate from the basic facility facts. Append to preserve serialized pages. */
	Staff
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

/** Individually bindable comparison values, formatted from the authoritative upgrade quote. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilityUpgradeColumnData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade") FText Revenue;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade") FText ServiceTime;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade") FText Throughput;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade") FText Footprint;
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

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Staff")
	bool bShowStaffSection = false;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Staff")
	int32 StaffCapacity = 0;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Staff")
	int32 AssignedStaffCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Staff")
	FText StaffSummaryText;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Staff")
	FText StaffAssignmentHint;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Staff")
	bool bCanAssignStaff = false;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Staff")
	bool bCanUnassignStaff = false;

	/** Current ordinary-use estimates, empty when no positive settlement is available. Independent of levels. */
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

	/** Compatibility payload for older Blueprints; the removed safety margin is never presented. */
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
	UPROPERTY(BlueprintReadOnly, Category="FestaFiesta|Facility Info") FText QueueCapacityValueText;
	UPROPERTY(BlueprintReadOnly, Category="FestaFiesta|Facility Info") FText ServiceCapacityValueText;

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

	/** Individually editable UMG cells; produced from the same authoritative quote as legacy text. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	FFF_FacilityUpgradeColumnData UpgradeCurrentValues;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade")
	FFF_FacilityUpgradeColumnData UpgradeNextValues;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade") FText UpgradePriceText;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade") FText UpgradeBalanceText;
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Upgrade") FText UpgradeBalanceLabel;

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

	// Existing widget binding name; the operational metric displays seconds per team.
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

	/**
	 * Revenue actually earned by this installed facility instance. Unlike the ledger's
	 * per-definition rows, two booths of the same type are kept apart here.
	 * Hidden when the section is inapplicable or the facility has neither income capability nor past earnings.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	bool bShowRevenueSection = false;

	/** False when the facility has neither direct income capability nor past earnings. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	bool bFacilityGeneratesRevenue = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	int32 TodayRevenueGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	int32 TotalRevenueGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	int32 TodayPaymentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	int32 TotalPaymentCount = 0;

	/** Share of today's festival-wide revenue, 0..100. Zero while the festival has no revenue yet. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	float TodayRevenueSharePercent = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	FText TodayRevenueText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	FText TotalRevenueText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	FText RevenuePaymentCountText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	FText RevenueShareText;

	/** Player-facing note (no sales yet, economy not ready). Empty hides it. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	FText RevenueStatusText;

	/** One-line today/total summary for compact or legacy designer layouts. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Revenue")
	FText RevenueSummaryText;

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
	/**
	 * Projects one installed facility's actual earnings. A null Revenue means the festival economy
	 * is not running yet; a zeroed record means no payment has been settled for this instance.
	 * bSectionApplicable is false for lighting and ground surfaces, which never take payments.
	 * Income capability or positive past earnings keeps the section visible even before today's first sale.
	 */
	FESTAFIESTA_API void ApplyFacilityRevenue(
		const FFF_FacilityInstanceRevenue* Revenue,
		bool bSectionApplicable,
		bool bGeneratesDirectRevenue,
		int32 FestivalTodayRevenueGold,
		FFF_FacilityInfoViewData& ViewData);
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
	FESTAFIESTA_API FText FormatServiceDuration(bool bAvailable, float SecondsPerTeam);
	FESTAFIESTA_API FText FormatThroughput(bool bAvailable, float TeamsPerMinute);
	FESTAFIESTA_API FText GetOperationStateText(
		bool bQueueSupported,
		bool bQueueAvailable,
		bool bIsOpen,
		bool bAcceptingNew);
}
