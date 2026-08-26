// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_FacilityCostRow.generated.h"

/**
 * 시설 1종의 설치비. data/unreal/facility_costs_v0.csv 에서 Import as DataTable.
 * 표준 RowName 은 "Cost.<대분류>.<소분류>.<고유이름>" 형태다
 * (예: Cost.Booth.FoodStall.DadaPochaFoodTruck).
 *
 * FacilityDefinitionId가 분류의 단일 원천이다. UFF_FacilityNamingLibrary로 대·소분류와 고유이름을
 * 파싱해 표준 키를 만든다. FFFZSFacilitySpec::InstallCostKey가 명시된 경우에는 기존 계약대로
 * 그 명시 키가 특정 시설의 예외 가격 override로 가장 먼저 적용된다.
 *
 * 자동 조회 순서: 표준 4-part 키 → 구형 3-part 키("Cost.<대분류>.<소분류>")
 *                 → 카테고리 폴백("Cost.<대분류>.Default") → 설치 거부 + Error.
 * 명시 InstallCostKey 조회 실패 시에는 자동 키로 바꾸지 않고, 기존 동작대로 카테고리 Default로 폴백한다.
 * [참고] Default 행은 아직 4-part 행이 없는 구형·추가 시설을 위한 지원 경로다.
 *        폴백 성공 자체는 유효한 호환 동작이므로 반복 Warning을 남기지 않는다.
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
