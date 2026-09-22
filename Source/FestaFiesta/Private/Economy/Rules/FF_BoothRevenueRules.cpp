#include "Economy/Rules/FF_BoothRevenueRules.h"
#include "Building/FF_FacilityNamingLibrary.h"
#include "DataTables/FF_FacilityRevenueTableRow.h"
#include "FF_FFZSTypes.h"

namespace
{
	bool HasInvalidRevenueSchema(const UDataTable* Table)
	{
		return Table && Table->GetRowStruct() != FFF_FacilityRevenueTableRow::StaticStruct();
	}

	const FFF_FacilityRevenueTableRow* FindFacilityRevenueRow(FName FacilityDefinitionId, const UDataTable* Table)
	{
		return Table && !HasInvalidRevenueSchema(Table)
			? Table->FindRow<FFF_FacilityRevenueTableRow>(FacilityDefinitionId, TEXT("FacilityRevenue"), false)
			: nullptr;
	}
}

int32 FFF_BoothRevenueRules::ResolveRevenuePerUse(
	const FFFZSFacilitySpec& Spec, const TMap<FName, int32>& LegacyRevenueTable,
	const UDataTable* FacilityRevenueTable)
{
	if (FacilityRevenueTable)
	{
		// 잘못된 표를 연결했을 때 유료 기본값으로 몰래 되돌아가지 않는다.
		if (FacilityRevenueTable->GetRowStruct() != FFF_FacilityRevenueTableRow::StaticStruct())
		{
			return 0;
		}
		if (const FFF_FacilityRevenueTableRow* Row = FacilityRevenueTable->FindRow<FFF_FacilityRevenueTableRow>(
			Spec.FacilityDefinitionId, TEXT("FacilityRevenue"), false))
		{
			return FMath::Max(0, Row->RevenuePerUseGold);
		}
	}

	if (Spec.bOverrideRevenuePerUseGold)
	{
		return FMath::Max(0, Spec.RevenuePerUseGold);
	}

	FFF_FacilityNameParts Parts;
	FString FailureReason;
	return UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
		Spec.Category, Spec.FacilityDefinitionId, Parts, FailureReason)
		? FindRevenuePerUse(LegacyRevenueTable, Parts.MinorToken) : 0;
}

bool FFF_BoothRevenueRules::HasFacilityRevenueRow(
	const FFFZSFacilitySpec& Spec, const UDataTable* FacilityRevenueTable)
{
	return FacilityRevenueTable
		&& FacilityRevenueTable->GetRowStruct() == FFF_FacilityRevenueTableRow::StaticStruct()
		&& FacilityRevenueTable->FindRow<FFF_FacilityRevenueTableRow>(
			Spec.FacilityDefinitionId, TEXT("FacilityRevenue"), false) != nullptr;
}

float FFF_BoothRevenueRules::ResolveCommissionRate(
	const FFFZSFacilitySpec& Spec, float LegacyRate, const UDataTable* FacilityRevenueTable)
{
	if (HasInvalidRevenueSchema(FacilityRevenueTable)) return 0.f;
	float Rate = LegacyRate;
	if (const FFF_FacilityRevenueTableRow* Row = FindFacilityRevenueRow(Spec.FacilityDefinitionId, FacilityRevenueTable);
		Row && Row->bOverrideCommissionRate)
	{
		Rate = Row->CommissionRate;
	}
	return FMath::IsFinite(Rate) && Rate >= 0.f && Rate <= 1.f ? Rate : 0.f;
}

float FFF_BoothRevenueRules::ResolveRevenueMultiplier(
	const FFFZSFacilitySpec& Spec, int32 Level, float LegacyMultiplier, const UDataTable* FacilityRevenueTable)
{
	if (Level < 1 || HasInvalidRevenueSchema(FacilityRevenueTable)) return 0.f;
	float Multiplier = LegacyMultiplier;
	if (const FFF_FacilityRevenueTableRow* Row = FindFacilityRevenueRow(Spec.FacilityDefinitionId, FacilityRevenueTable))
	{
		if (const float* Found = Row->RevenueMultiplierByLevel.Find(Level))
		{
			Multiplier = *Found;
		}
	}
	return FMath::IsFinite(Multiplier) && Multiplier >= 0.f ? Multiplier : 0.f;
}

int32 FFF_BoothRevenueRules::ResolveProgramFeePerPerson(
	FName FacilityDefinitionId, FName ProgramId, int32 LegacyFee, const UDataTable* FacilityRevenueTable)
{
	if (ProgramId.IsNone() || HasInvalidRevenueSchema(FacilityRevenueTable)) return 0;
	if (const FFF_FacilityRevenueTableRow* Row = FindFacilityRevenueRow(FacilityDefinitionId, FacilityRevenueTable))
	{
		if (const int32* Found = Row->ProgramFeesPerPerson.Find(ProgramId))
		{
			return FMath::Max(0, *Found);
		}
	}
	return FMath::Max(0, LegacyFee);
}

int32 FFF_BoothRevenueRules::ResolveMinigameRewardGold(
	FName FacilityDefinitionId, int32 LegacyReward, const UDataTable* FacilityRevenueTable)
{
	if (HasInvalidRevenueSchema(FacilityRevenueTable)) return 0;
	if (const FFF_FacilityRevenueTableRow* Row = FindFacilityRevenueRow(FacilityDefinitionId, FacilityRevenueTable);
		Row && Row->bOverrideMinigameRewardGold)
	{
		return FMath::Max(0, Row->MinigameRewardAtMaxScoreGold);
	}
	return FMath::Max(0, LegacyReward);
}

int32 FFF_BoothRevenueRules::FindRevenuePerUse(const TMap<FName, int32>& RevenueTable, FName MinorToken)
{
	const int32* Found = RevenueTable.Find(MinorToken);
	return Found ? FMath::Max(0, *Found) : 0;
}

int32 FFF_BoothRevenueRules::ResolveCommission(int32 RevenuePerUse, int32 MemberCount, float CommissionRate, double RevenueMultiplier)
{
	if (RevenuePerUse <= 0 || !FMath::IsFinite(CommissionRate) || CommissionRate <= 0.f
		|| !FMath::IsFinite(RevenueMultiplier) || RevenueMultiplier <= 0.f) return 0;
	
	if (!ensureMsgf(MemberCount > 0,
		TEXT("부스 이용 인원이 %d 이다. 파티 퇴장 신호가 빈 파티를 보냈다."), MemberCount))
	{
		return 0;
	}
	
	// 기본 수수료를 먼저 반올림하면 9.6 * 1.5가 15로 부풀어 오른다. 최종 14.4만 반올림한다.
	const double Commission = static_cast<double>(RevenuePerUse) * MemberCount
		* static_cast<double>(CommissionRate) * static_cast<double>(RevenueMultiplier);
	if (Commission >= static_cast<double>(MAX_int32)) return MAX_int32;
	return FMath::RoundToInt32(Commission);
}
