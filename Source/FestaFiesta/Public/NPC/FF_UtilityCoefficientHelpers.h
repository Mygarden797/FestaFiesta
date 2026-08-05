// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UDataTable;

/** utility_coefficients_v0 DataTable에서 계수 하나를 읽는 공용 헬퍼. Row 이름 = CoefficientId. */
class FFF_UtilityCoefficientHelpers
{
public:
	static float GetCoefficient(const UDataTable* UtilityCoefficientTable, FName CoefficientId, float DefaultValue);
};
