#include "UI/PrepHUD/Panels/FF_PrepRecommendedFacilities.h"

#include "Building/FF_FacilityNamingLibrary.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"

bool FFF_PrepRecommendedFacilityRules::Matches(EFF_PrepRecommendedFacilityKind Kind,
	EFFZSFacilityCategory Category, FName DefinitionId) const
{
	FFF_FacilityNameParts Parts;
	FString Failure;
	UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(Category, DefinitionId, Parts, Failure);
	const FFF_NeedReliefFlags Relief = FFF_ZoneSystemBridge::GetNeedReliefFlags(Category, DefinitionId);
	switch (Kind)
	{
	case EFF_PrepRecommendedFacilityKind::Lighting: return Category == EFFZSFacilityCategory::Lighting;
	case EFF_PrepRecommendedFacilityKind::Food: return Relief.bRelievesHunger;
	case EFF_PrepRecommendedFacilityKind::Toilet: return Category == EFFZSFacilityCategory::Toilet;
	case EFF_PrepRecommendedFacilityKind::Entertainment:
		return Relief.bRelievesStimulation && !FFF_ZoneSystemBridge::IsMainStageFacility(Category, DefinitionId);
	case EFF_PrepRecommendedFacilityKind::Rest: return Category == EFFZSFacilityCategory::Rest;
	case EFF_PrepRecommendedFacilityKind::Drink:
		return DrinkFacilityIds.Contains(DefinitionId) || (Category == EFFZSFacilityCategory::Booth && DrinkBoothMinorTokens.Contains(Parts.MinorToken));
	case EFF_PrepRecommendedFacilityKind::FirstAid:
		return Category == EFFZSFacilityCategory::Safety && Parts.MinorToken == TEXT("FirstAid");
	case EFF_PrepRecommendedFacilityKind::FireSafety:
		return Category == EFFZSFacilityCategory::Safety && Parts.MinorToken == TEXT("FireSafety");
	default: return false;
	}
}

FFF_PrepRecommendedFacilitiesView FFPrepRecommendedFacilities::Evaluate(
	const FFFZSReadModel& Model, const FFF_PrepRecommendedFacilityRules& Rules, bool bAvailable)
{
	FFF_PrepRecommendedFacilitiesView Result;
	Result.bAvailable = bAvailable;
	if (!bAvailable) return Result;
	for (const auto Kind : { EFF_PrepRecommendedFacilityKind::Lighting, EFF_PrepRecommendedFacilityKind::Food,
		EFF_PrepRecommendedFacilityKind::Toilet, EFF_PrepRecommendedFacilityKind::Entertainment,
		EFF_PrepRecommendedFacilityKind::Rest, EFF_PrepRecommendedFacilityKind::Drink,
		EFF_PrepRecommendedFacilityKind::FirstAid, EFF_PrepRecommendedFacilityKind::FireSafety })
	{
		const bool bPresent = Model.Facilities.ContainsByPredicate([&](const FFFZSFacilityReadModel& Facility)
		{
			// 필수 시설 원본과 같은 설치 상태. 셋업에서 아직 개장하지 않은 Closed도 설치된 시설이다.
			const bool bInstalled = Facility.State == EFFZSFacilityState::Validated
				|| Facility.State == EFFZSFacilityState::Active || Facility.State == EFFZSFacilityState::Closed
				|| Facility.State == EFFZSFacilityState::MovePending;
			return bInstalled && !Facility.bNeedsRelocation
				&& Rules.Matches(Kind, Facility.Category, Facility.FacilityDefinitionId);
		});
		if (!bPresent) Result.MissingKinds.Add(Kind);
	}
	return Result;
}
