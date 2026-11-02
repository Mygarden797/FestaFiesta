// FF_FacilityModel.h
// 역할: 조합 도감의 발견 여부·거리 조건·실제 효과를 계산하고 검색·요약한다.

#pragma once

#include "CoreMinimal.h"
#include "Building/FacilityCollection/FF_FacilityTypes.h"
#include "Building/Synergy/FF_FacilitySynergyEvaluator.h"

/**
 * 시설 조합 도감의 계산 모델. 영구 발견 기록과 현재 배치의 효과를 별도로 계산한다.
 * 거리·중첩·상한 규칙은 실제 수익 평가기를 재사용하므로 화면과 경제 결과가 같은 기준을 따른다.
 */
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
	TArray<FFF_FacilityCodexEntry> Entries;
	TSet<FName> NewDiscoveries;
	FFF_FacilitySynergyEvaluator Evaluator;
};
