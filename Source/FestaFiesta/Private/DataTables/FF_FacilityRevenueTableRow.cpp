#include "DataTables/FF_FacilityRevenueTableRow.h"

bool FFF_FacilityRevenueTableRow::IsRowValid(TArray<FText>& OutErrors) const
{
	const int32 PreviousErrorCount = OutErrors.Num();
	if (RevenuePerUseGold < 0)
	{
		OutErrors.Add(NSLOCTEXT("FFFacilityRevenue", "NegativeRevenue", "1인 1회 매출은 0 이상이어야 합니다. 무료 시설은 0을 입력하세요."));
	}
	if (MinigameRewardAtMaxScoreGold < 0)
	{
		OutErrors.Add(NSLOCTEXT("FFFacilityRevenue", "NegativeMinigameReward", "미니게임 최대 보상은 0 이상이어야 합니다. 보상이 없는 시설은 0을 입력하세요."));
	}
	if (!FMath::IsFinite(CommissionRate) || CommissionRate < 0.f || CommissionRate > 1.f)
	{
		OutErrors.Add(NSLOCTEXT("FFFacilityRevenue", "InvalidCommissionRate", "수수료율은 0~1 사이의 유한한 값이어야 합니다. 12%는 0.12를 입력하세요."));
	}
	for (const auto& Entry : RevenueMultiplierByLevel)
	{
		if (Entry.Key < 1 || !FMath::IsFinite(Entry.Value) || Entry.Value < 0.f)
		{
			OutErrors.Add(NSLOCTEXT("FFFacilityRevenue", "InvalidLevelMultiplier", "레벨별 수익 배율의 레벨은 1 이상, 배율은 0 이상의 유한한 값이어야 합니다."));
		}
	}
	for (const auto& Entry : ProgramFeesPerPerson)
	{
		if (Entry.Key.IsNone() || Entry.Value < 0)
		{
			OutErrors.Add(NSLOCTEXT("FFFacilityRevenue", "InvalidProgramFee", "프로그램별 참가비의 Program Id는 비어 있을 수 없으며 참가비는 0 이상이어야 합니다."));
		}
	}
	return OutErrors.Num() == PreviousErrorCount;
}

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult FFF_FacilityRevenueTableRow::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	if (!IsRowValid(Errors))
	{
		for (const FText& Error : Errors)
		{
			Context.AddError(Error);
		}
		return EDataValidationResult::Invalid;
	}
	return EDataValidationResult::Valid;
}
#endif
