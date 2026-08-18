// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_FacilityCostResolver.h"

#include "Building/FF_FacilityNamingLibrary.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"

bool FFF_FacilityCostResolver::ResolveCostKey(
	const FFFZSFacilitySpec& Spec,
	FName& OutCostKey,
	FString& OutFailureReason)
{
	OutCostKey = NAME_None;
	OutFailureReason.Reset();

	// 이 필드는 분류의 두 번째 원천이 아니라 특정 시설의 예외 가격 override다.
	if (!Spec.InstallCostKey.IsNone())
	{
		OutCostKey = Spec.InstallCostKey;
		return true;
	}

	FFF_FacilityNameParts Parts;
	if (!UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
		Spec.Category, Spec.FacilityDefinitionId, Parts, OutFailureReason))
	{
		if (OutFailureReason.IsEmpty())
		{
			OutFailureReason = TEXT("FacilityDefinitionId가 비어 있어 설치비 키를 만들 수 없다.");
		}
		return false;
	}

	OutCostKey = FName(*FString::Printf(TEXT("Cost.%s.%s"),
		*Parts.MajorToken.ToString(), *Parts.MinorToken.ToString()));
	return true;
}

bool FFF_FacilityCostResolver::FindInstallCost(
	const FFFZSFacilitySpec& Spec,
	const UDataTable* CostTable,
	int32& OutInstallCost,
	FName& OutMatchedRow,
	FString& OutFailureReason)
{
	OutInstallCost = 0;
	OutMatchedRow = NAME_None;
	OutFailureReason.Reset();

	if (!CostTable || CostTable->GetRowStruct() != FFF_FacilityCostRow::StaticStruct())
	{
		OutFailureReason = TEXT("시설 비용 DataTable이 없거나 행 구조체가 FFF_FacilityCostRow가 아니다.");
		return false;
	}

	FName ExactKey;
	if (!ResolveCostKey(Spec, ExactKey, OutFailureReason))
	{
		return false;
	}

	const FString Context = TEXT("FFF_FacilityCostResolver");
	if (const FFF_FacilityCostRow* ExactRow = CostTable->FindRow<FFF_FacilityCostRow>(ExactKey, Context, false))
	{
		OutInstallCost = ExactRow->InstallCost;
		OutMatchedRow = ExactKey;
		return true;
	}

	const FName MajorToken = UFF_FacilityNamingLibrary::GetMajorToken(Spec.Category);
	if (!MajorToken.IsNone())
	{
		const FName DefaultKey(*FString::Printf(TEXT("Cost.%s.Default"), *MajorToken.ToString()));
		if (const FFF_FacilityCostRow* DefaultRow = CostTable->FindRow<FFF_FacilityCostRow>(DefaultKey, Context, false))
		{
			OutInstallCost = DefaultRow->InstallCost;
			OutMatchedRow = DefaultKey;
			return true;
		}
	}

	OutFailureReason = FString::Printf(
		TEXT("설치비 행 '%s'와 카테고리 Default 행을 모두 찾지 못했다."), *ExactKey.ToString());
	return false;
}

int32 FFF_FacilityCostResolver::CalculateDemolitionRefund(
	const int32 InstallCost,
	const float RefundRate)
{
	return FMath::RoundToInt(
		FMath::Max(0, InstallCost) * FMath::Clamp(RefundRate, 0.0f, 1.0f));
}
