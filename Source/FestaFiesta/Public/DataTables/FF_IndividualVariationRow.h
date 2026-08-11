// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_IndividualVariationRow.generated.h"

/**
 * 고정 Seed 기반 개체 변주 특성 한 종. data/unreal/individual_variation_knobs_v0.csv 에서 Import as DataTable.
 * v0에서는 전부 삼각분포(MinValue, ModeValue, MaxValue)를 쓴다.
 *
 * AffectsField 는 이 특성이 실제로 곱해지는 필드/계수 이름을 적어둔 감사용 문자열이며
 * 런타임 로직은 이 열 자체를 참조하지 않는다. 실제 매핑은 FFF_IndividualVariationResolver
 * 와 data/metadata/spec/individual_variation_spec.md 5절에 있다.
 */
USTRUCT(BlueprintType)
struct FFF_IndividualVariationRow : public FTableRowBase
{
	GENERATED_BODY()

	// walk_speed_multiplier / patience_multiplier / crowd_tolerance_multiplier /
	// preference_strength_multiplier / stay_duration_multiplier / reevaluate_interval_multiplier / leisureliness_axis
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FName TraitKey;

	// v0에서는 전부 "triangular". 다른 분포를 추가할 때를 위해 문자열로 남겨둔다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution")
	FString DistributionType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution")
	float MinValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution")
	float ModeValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution")
	float MaxValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution")
	FString Unit;

	// Generation / Decision / RuntimeNoise 중 이 특성이 속하는 난수 목적 (TraitSeed 해시 재료)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Distribution")
	FName RandomPurpose;

	// 이 특성이 실제로 곱해지는 필드/계수 이름. 감사용, 런타임 로직에서 참조하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString AffectsField;

	// leisureliness_axis 와 균등난수 단계에서 블렌딩되는 대상 TraitKey. 없으면 "none"
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString CorrelatedWith;

	// confirmed / assumption / to_verify / draft_assumption
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString Status;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString Description;
};
