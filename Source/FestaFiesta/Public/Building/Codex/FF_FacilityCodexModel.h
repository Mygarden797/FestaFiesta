// FF_FacilityCodexModel.h
// 역할: 조합 도감의 발견 여부·거리 조건·실제 효과를 계산하고 검색·요약한다.

#pragma once

#include "CoreMinimal.h"
#include "Building/Codex/FF_FacilityCodexTypes.h"
#include "Building/Synergy/FF_FacilitySynergyEvaluator.h"

/** Value model. Reuses the production revenue evaluator; it never invents a second distance rule. */
class FESTAFIESTA_API FFF_FacilityCodexModel
{
public:
	void SetRules(TArray<FFF_FacilitySynergyRule> InRules, float MaxTotalBonusPercent);
	void Evaluate(const FFFZSReadModel& World, const TSet<FName>& DiscoveredRuleIds);
	const TArray<FFF_FacilityCodexEntry>& GetEntries() const { return Entries; }
	const TSet<FName>& GetNewDiscoveries() const { return NewDiscoveries; }
	static FFF_FacilityCodexSummary Summarize(const TArray<FFF_FacilityCodexEntry>& Source);
	static bool MatchesFilter(const FFF_FacilityCodexEntry& Entry, const FFF_FacilityCodexFilter& Filter);
	/** Confirmed installation lifecycle; relocating an already installed facility does not undo discovery. */
	static bool IsInstalledFacility(const FFFZSFacilityReadModel& Facility);
	/** Eligible for the existing distance/bonus evaluator, independently of collection discovery. */
	static bool IsEligibleFacility(const FFFZSFacilityReadModel& Facility);

private:
	TArray<FFF_FacilitySynergyRule> Rules;
	TArray<FFF_FacilityCodexEntry> Entries;
	TSet<FName> NewDiscoveries;
	FFF_FacilitySynergyEvaluator Evaluator;
};
