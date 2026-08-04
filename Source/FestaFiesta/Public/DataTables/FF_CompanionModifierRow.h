// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_CompanionModifierRow.generated.h"

/**
 * 동반유형(1인/2인/3인 이상) 오버레이 보정값. data/unreal/companion_type_modifier_v0.csv 에서 Import as DataTable.
 *
 * 모든 생애주기 아키타입(FFF_ArchetypeRow)에 동일한 공식으로 적용되는 독립 오버레이다.
 * 나이×동반유형 교차표가 없어 생애주기별로 다르게 적용하지 않는다(독립가정).
 * *DeltaPp 필드는 효용계수가 아니라 전체 대비 percentage-point 차이다.
 */
USTRUCT(BlueprintType)
struct FFF_CompanionModifierRow : public FTableRowBase
{
	GENERATED_BODY()

	// solo / pair / group_3_plus
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	FName CompanionType;

	// 가칭. UI 노출 명칭
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	FString CompanionLabelDraft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float AmbiencePhotoZoneDeltaPp = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float FoodStallDeltaPp = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float ActivityGameDeltaPp = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float RestAreaDeltaPp = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float MarketGoodsDeltaPp = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float PerformanceStageDeltaPp = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float FestivalEngagementBaselineDeltaPp = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float CraftExperienceDeltaPp = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float ExplorationWanderDeltaPp = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float NightlifeEntertainmentDeltaPp = 0.f;

	// 전체 대비 편차가 가장 큰 방문지 선택 이유 상위 2개 (참고용, 로직에서 참조하지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString TopReasonDelta1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString TopReasonDelta2;

	// confirmed / assumption / to_verify / draft_assumption
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString DataStatus;
};
