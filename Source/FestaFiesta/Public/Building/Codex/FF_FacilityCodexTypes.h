#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "FF_FacilityCodexTypes.generated.h"

class UTexture2D;

/** One authored, directional recipe. Discovery history and the present layout are independent. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilityCodexEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") FName RuleId;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") FName TargetFacilityId;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") FName PartnerFacilityId;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") FText TargetName;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") FText PartnerName;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") TSoftObjectPtr<UTexture2D> TargetThumbnail;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") TSoftObjectPtr<UTexture2D> PartnerThumbnail;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") EFFZSFacilityCategory TargetCategory = EFFZSFacilityCategory::Booth;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") float MaxDistanceMeters = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") float ConfiguredBonusPercent = 0.f;
	/** Largest contribution to a single currently installed beneficiary, after stacking and cap. */
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") float AppliedBonusPercent = 0.f;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") FName StackingGroup;
	/** Has had both distinct recipe facilities installed at least once, regardless of their distance. */
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") bool bDiscovered = false;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") bool bConditionMet = false;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") bool bEffectApplied = false;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") bool bTargetInstalled = false;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") bool bPartnerInstalled = false;
	/** Current completed installations include a distinct pair; A+A requires two different instance GUIDs. */
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") bool bHasInstalledPair = false;
	/** -1 means no pair is eligible for live effects (e.g. relocation required). Height is ignored. */
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") float ClosestDistanceMeters = -1.f;
	/** Number of beneficiary instances satisfying the distance requirement. */
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") int32 ConditionMetCount = 0;
	/** Number of beneficiary instances receiving a nonzero contribution from this recipe. */
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") int32 EffectAppliedCount = 0;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilityCodexSummary
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") int32 TotalCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") int32 DiscoveredCount = 0;
	/** Recipe counts, not sums of facility counts. */
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") int32 ConditionMetCount = 0;
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") int32 EffectAppliedCount = 0;
};

UENUM(BlueprintType)
enum class EFF_FacilityCodexFilterState : uint8
{
	All UMETA(DisplayName="전체"),
	Undiscovered UMETA(DisplayName="미발견"),
	Discovered UMETA(DisplayName="발견"),
	Active UMETA(DisplayName="효과 적용 중")
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilityCodexFilter
{
	GENERATED_BODY()
	/** None includes all facilities. Either side of the recipe may match. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facility Codex") FName FacilityDefinitionId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facility Codex") FString SearchText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facility Codex") EFF_FacilityCodexFilterState State = EFF_FacilityCodexFilterState::All;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facility Codex") bool bFilterByCategory = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Facility Codex") EFFZSFacilityCategory Category = EFFZSFacilityCategory::Booth;
};
