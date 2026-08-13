// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_FacilityCostRow.generated.h"

/**
 * 시설 1종의 설치비. data/unreal/facility_costs_v0.csv 에서 Import as DataTable.
 * RowName 은 "Cost.<대분류>.<소분류>" 형태다 (예: Cost.Booth.FoodStall).
 *
 * FacilityDefinitionId가 분류의 단일 원천이다. UFF_FacilityNamingLibrary로 대·소분류를 파싱해
 * 기본 키를 만들고, FFFZSFacilitySpec::InstallCostKey가 명시된 경우에만 예외 가격 override로 우선한다.
 *
 * 조회 실패 시: 정확한 키 → 카테고리 폴백("Cost.<대분류>.Default") → 설치 거부 + Error.
 * [참고] 폴백은 예외가 아니라 공용 시설(Toilet·Info·Rest·Safety·PhotoSpot)의 정상 경로다.
 *        그 5개는 소분류 행이 없어서, 폴백 도달 자체를 Warning 으로 남기면 정상 설치마다 쌓인다.
 *
 * [참고] MorningCheck 에 시설 이동 기능이 없고, Setup 중 재배치는 추가 비용 없이 허용한다.
 */
USTRUCT(BlueprintType)
struct FFF_FacilityCostRow : public FTableRowBase
{
	GENERATED_BODY()

	// 설치비 (Gold). 0 이하면 저작 오류 (공짜 시설은 없다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (ClampMin = "1"))
	int32 InstallCost = 0;
};
