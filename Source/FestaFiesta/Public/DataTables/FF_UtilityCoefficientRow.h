// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_UtilityCoefficientRow.generated.h"

/**
 * Utility AI 목적지 선택 계수 노브 한 개. data/unreal/utility_coefficients_v0.csv 에서 Import as DataTable.
 *
 * Status 가 assumption 인 계수는 실측으로 추정된 값이 아니라 밸런스·민감도 분석용 노브다.
 * (data/metadata/spec/utility_ai_spec.md 참조)
 */
USTRUCT(BlueprintType)
struct FFF_UtilityCoefficientRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coefficient")
	FName CoefficientId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coefficient")
	float DefaultValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coefficient")
	float MinValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coefficient")
	float MaxValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coefficient")
	FString Unit;

	// Utility 식에서 이 계수가 곱해지는 항 (예: distance_cost, waiting_cost)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Coefficient")
	FString Term;

	// confirmed / assumption / to_verify / derived
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString Status;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString SourceBasis;

	// primary / normalization / trigger / fixed_reference
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString SensitivityRole;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Provenance")
	FString Description;
};
