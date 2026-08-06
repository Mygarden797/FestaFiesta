// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_GroupChildCompositionRow.generated.h"

/**
 * 그룹 크기별 아동(만 15세 미만) 수 분포의 한 칸.
 * data/unreal/group_child_composition_v0.csv 에서 Import as DataTable.
 *
 * 2025 국민여행조사 문항 A6A("일행 중 만 15세 미만은 몇 명이었습니까?")를 WT_DOM 가중 집계한 값이다.
 * 그룹 크기를 뽑은 뒤 이 표에서 아동 수를 바로 뽑으면 된다.
 *
 * 사용 순서:
 *  1. FFF_GroupSizeDistributionRow 로 GroupSize 를 뽑는다
 *  2. 같은 ScopeId·GroupSize 의 행들에서 ShareWithinGroupSizePct 로 ChildCount 를 뽑는다
 *  3. 그 그룹의 ChildCount 명에게 bIsDependentChildSlot = true 를 세우고, 남은 인원은 성인으로 둔다
 *     (리더는 항상 성인 — group_composition_rules_v0 의 leader_must_be_adult)
 *
 * 주의: ShareWithinGroupSizePct 는 **그 GroupSize 안에서** 합계 100 이다. 전체 합이 아니다.
 */
USTRUCT(BlueprintType)
struct FFF_GroupChildCompositionRow : public FTableRowBase
{
	GENERATED_BODY()

	// tourism_daytrip(기본) / tourism_all / tourism_overnight
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Composition")
	FName ScopeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Composition")
	int32 GroupSize = 0;

	// 이 그룹 안의 만 15세 미만 인원수 (0 이면 전원 성인)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Composition")
	int32 ChildCount = 0;

	// 같은 ScopeId + GroupSize 안에서 합계 100
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Composition")
	float ShareWithinGroupSizePct = 0.f;

	// confirmed / assumption / to_verify / derived
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString Status;
};
