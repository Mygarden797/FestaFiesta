// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_ArchetypeRow.generated.h"

/**
 * 생애주기 아키타입 한 종. data/unreal/npc_archetypes_v0.csv 에서 Import as DataTable.
 * ArchetypeLabelDraft 는 UI 노출용 표시 명칭 후보이며 UI 담당자와 논의 후 확정한다.
 *
 * *DeltaPp 필드는 Tour_Stat 2018 연령별 활동 응답률의 전체 평균 대비 percentage-point
 * 차이다. 효용계수(beta)가 아니다. Utility 계산 시 lambda_affinity 로 스케일한 뒤
 * FFF_CompanionModifierRow 의 동일 카테고리 값과 합산해서 쓴다.
 * (data/metadata/utility_ai_spec.md 참조)
 */
USTRUCT(BlueprintType)
struct FFF_ArchetypeRow : public FTableRowBase
{
	GENERATED_BODY()

	// life_stage_teen / life_stage_20s / life_stage_30_40s / life_stage_50s / life_stage_60plus
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Archetype")
	FName ArchetypeId;

	// 가칭. UI 노출 명칭
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Archetype")
	FString ArchetypeLabelDraft;

	// 시나리오 A: 방문 성향이 세그먼트별로 같다고 가정 (= 사하구 주민 구성)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population")
	float PopulationSharePct_ResidentNeutral = 0.f;

	// 시나리오 B: 사하구 관광지 관측 구성을 그대로 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Population")
	float PopulationSharePct_DatalabObserved = 0.f;

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

	// 부스 선호가 아니라 이 아키타입의 전반적 축제 참여 성향 기준값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float FestivalEngagementBaselineDeltaPp = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float CraftExperienceDeltaPp = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float ExplorationWanderDeltaPp = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preference")
	float NightlifeEntertainmentDeltaPp = 0.f;

	// 이 아키타입의 Tour_Stat 대리 그룹 (예: age_30s+age_40s). 감사용, 런타임 로직에서 참조하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString TourStatProxyGroups;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString TopVisitReason1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString TopVisitReason1Pct;

	// 연령 구간 해상도 불일치 등 이 Row 해석 시 반드시 알아야 할 경고
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString ResolutionNote;

	// confirmed / assumption / to_verify / draft_assumption
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString DataStatus;
};
