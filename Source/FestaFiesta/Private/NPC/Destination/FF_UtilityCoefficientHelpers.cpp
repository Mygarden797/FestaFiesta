// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Destination/FF_UtilityCoefficientHelpers.h"
#include "DataTables/FF_UtilityCoefficientRow.h"
#include "Engine/DataTable.h"

float FFF_UtilityCoefficientHelpers::GetCoefficient(const UDataTable* UtilityCoefficientTable, FName CoefficientId, float DefaultValue)
{
	if (!UtilityCoefficientTable)
	{
		return DefaultValue;
	}
	const FFF_UtilityCoefficientRow* Row = UtilityCoefficientTable->FindRow<FFF_UtilityCoefficientRow>(CoefficientId, TEXT("FFF_UtilityCoefficientHelpers::GetCoefficient"), /*bWarnIfRowMissing=*/false);
	return Row ? Row->DefaultValue : DefaultValue;
}
