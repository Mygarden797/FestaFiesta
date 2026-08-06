// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_GroupCompositionRuleRow.generated.h"

/**
 * 그룹 구성 규칙 한 개. data/unreal/group_composition_rules_v0.csv 에서 Import as DataTable.
 *
 * population_group_generation_spec.md 의 규칙 중 실측 분포로 대체되지 않고 남은 것만 담는다.
 * 규칙을 마크다운이 아니라 데이터로 두는 목적은, 규칙이 바뀔 때 Crowd 쪽 코드를 고치지 않아도
 * 되게 하는 것이다.
 *
 * Value 는 전부 문자열이다 — ValueType 을 보고 파싱한다:
 *   int        정수            (예: group_size_cap = "6")
 *   bool       "true"/"false"  (예: leader_must_be_adult)
 *   enum       고정 문자열      (예: destination_decision_scope = "leader_only")
 *   list       세미콜론 구분    (예: group_shared_traits = "walk_speed_multiplier;stay_duration_multiplier")
 *   table_ref  다른 DataTable  (예: child_composition_table = "group_child_composition_v0")
 *
 * Status 가 assumption 인 규칙은 실측 근거가 없다 — 바뀔 수 있으니 전제로 삼지 말 것.
 */
USTRUCT(BlueprintType)
struct FFF_GroupCompositionRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	FName RuleId;

	// ValueType 에 따라 파싱한다. 숫자여도 문자열로 들어 있다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	FString Value;

	// int / bool / enum / list / table_ref
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	FString ValueType;

	// confirmed / assumption / to_verify / derived
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString Status;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString SourceBasis;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString Description;
};
