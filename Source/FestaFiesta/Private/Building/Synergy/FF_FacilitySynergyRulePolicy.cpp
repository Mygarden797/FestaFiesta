// FF_FacilitySynergyRulePolicy.cpp
// 역할: 데이터표 검증과 조합 참여 기준을 한곳에서 처리해 수익과 도감의 판정 차이를 방지한다.
#include "Building/Synergy/FF_FacilitySynergyRulePolicy.h"

#include "Engine/DataTable.h"

TArray<FFF_FacilitySynergyRule> FFFacilitySynergyRulePolicy::ReadTable(
	const UDataTable* Table, TArray<FString>& OutWarnings)
{
	OutWarnings.Reset();
	TArray<FFF_FacilitySynergyRule> Rules;
	if (!Table) { return Rules; }
	if (Table->GetRowStruct() != FFF_FacilitySynergyRow::StaticStruct())
	{
		OutWarnings.Add(TEXT("시설 조합 데이터표가 FF_FacilitySynergyRow 형식이 아닙니다."));
		return Rules;
	}
	Rules.Reserve(Table->GetRowMap().Num());
	for (const auto& Pair : Table->GetRowMap())
	{
		const FFF_FacilitySynergyRow& Row = *reinterpret_cast<const FFF_FacilitySynergyRow*>(Pair.Value);
		if (!Row.bEnabled) { continue; }
		FString Error;
		if (!Pair.Key.IsNone() && Row.IsUsable(&Error)) { Rules.Add({Pair.Key, Row}); }
		else { OutWarnings.Add(FString::Printf(TEXT("시설 조합 '%s' 사용 안 함: %s"), *Pair.Key.ToString(), *Error)); }
	}
	Normalize(Rules);
	return Rules;
}

void FFFacilitySynergyRulePolicy::Normalize(TArray<FFF_FacilitySynergyRule>& Rules)
{
	Rules.RemoveAll([](const FFF_FacilitySynergyRule& Rule)
	{
		return Rule.Id.IsNone() || !Rule.Row.bEnabled || !Rule.Row.IsUsable();
	});
	Rules.Sort([](const FFF_FacilitySynergyRule& A, const FFF_FacilitySynergyRule& B)
	{
		return A.Id.LexicalLess(B.Id);
	});
}

bool FFFacilitySynergyRulePolicy::IsEligibleFacility(const FFFZSFacilityReadModel& Facility)
{
	return Facility.FacilityInstanceId.IsValid() && !Facility.FacilityDefinitionId.IsNone()
		&& !Facility.WorldLocation.ContainsNaN() && !Facility.bNeedsRelocation
		&& Facility.State != EFFZSFacilityState::Planned && Facility.State != EFFZSFacilityState::Removed;
}
