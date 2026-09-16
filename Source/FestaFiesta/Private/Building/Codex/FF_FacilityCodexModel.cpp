#include "Building/Codex/FF_FacilityCodexModel.h"

#define LOCTEXT_NAMESPACE "FFFacilityCodexModel"

void FFF_FacilityCodexModel::SetRules(TArray<FFF_FacilitySynergyRule> InRules, const float MaxTotalBonusPercent)
{
	Rules = MoveTemp(InRules);
	Rules.RemoveAll([](const FFF_FacilitySynergyRule& Rule)
	{
		return Rule.Id.IsNone() || !Rule.Row.bEnabled || !Rule.Row.IsUsable();
	});
	// The production evaluator uses this exact stable order for its RuleIndex values.
	Rules.Sort([](const FFF_FacilitySynergyRule& A, const FFF_FacilitySynergyRule& B) { return A.Id.LexicalLess(B.Id); });
	Evaluator.SetRules(Rules, MaxTotalBonusPercent);
	Entries.Reset(Rules.Num());
	for (const FFF_FacilitySynergyRule& Rule : Rules)
	{
		FFF_FacilityCodexEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.RuleId = Rule.Id;
		Entry.DisplayName = Rule.Row.DisplayName.IsEmpty() ? LOCTEXT("UnnamedRecipe", "시설 조합") : Rule.Row.DisplayName;
		Entry.TargetFacilityId = Rule.Row.TargetFacilityId;
		Entry.PartnerFacilityId = Rule.Row.PartnerFacilityId;
		Entry.MaxDistanceMeters = Rule.Row.MaxDistanceMeters;
		Entry.ConfiguredBonusPercent = Rule.Row.RevenueBonusPercent;
		Entry.StackingGroup = Rule.Row.StackingGroup;
	}
}

bool FFF_FacilityCodexModel::IsInstalledFacility(const FFFZSFacilityReadModel& Facility)
{
	if (!Facility.FacilityInstanceId.IsValid() || Facility.FacilityDefinitionId.IsNone()) { return false; }
	// ExecuteInstall commits Validated. Active/Closed/MovePending are subsequent operating states.
	// RevalidateFacilitiesInZone can flag those existing installations without cancelling installation.
	switch (Facility.State)
	{
	case EFFZSFacilityState::Validated:
	case EFFZSFacilityState::Active:
	case EFFZSFacilityState::Closed:
	case EFFZSFacilityState::MovePending:
		return true;
	default:
		return false;
	}
}

bool FFF_FacilityCodexModel::IsEligibleFacility(const FFFZSFacilityReadModel& Facility)
{
	// Match UFF_FacilitySynergySubsystem eligibility, including Closed and MovePending facilities.
	return Facility.FacilityInstanceId.IsValid() && !Facility.FacilityDefinitionId.IsNone()
		&& !Facility.WorldLocation.ContainsNaN() && !Facility.bNeedsRelocation
		&& Facility.State != EFFZSFacilityState::Planned && Facility.State != EFFZSFacilityState::Removed;
}

void FFF_FacilityCodexModel::Evaluate(const FFFZSReadModel& World, const TSet<FName>& DiscoveredRuleIds)
{
	NewDiscoveries.Reset();
	TArray<FFF_FacilitySynergyPoint> Points;
	Points.Reserve(World.Facilities.Num());
	TSet<FGuid> SeenIds;
	TMap<FName, int32> InstalledCounts;
	for (const FFFZSFacilityReadModel& Facility : World.Facilities)
	{
		if (!IsInstalledFacility(Facility) || SeenIds.Contains(Facility.FacilityInstanceId)) { continue; }
		SeenIds.Add(Facility.FacilityInstanceId);
		++InstalledCounts.FindOrAdd(Facility.FacilityDefinitionId);
		if (IsEligibleFacility(Facility))
		{
			Points.Add({Facility.FacilityInstanceId, Facility.FacilityDefinitionId, Facility.WorldLocation});
		}
	}
	Evaluator.SetFacilities(Points);
	for (FFF_FacilityCodexEntry& Entry : Entries)
	{
		Entry.bDiscovered = DiscoveredRuleIds.Contains(Entry.RuleId);
		Entry.bConditionMet = false;
		Entry.bEffectApplied = false;
		const int32 TargetCount = InstalledCounts.FindRef(Entry.TargetFacilityId);
		const int32 PartnerCount = InstalledCounts.FindRef(Entry.PartnerFacilityId);
		Entry.bTargetInstalled = TargetCount > 0;
		Entry.bPartnerInstalled = PartnerCount > 0;
		Entry.bHasInstalledPair = Entry.TargetFacilityId == Entry.PartnerFacilityId
			? TargetCount >= 2 : TargetCount > 0 && PartnerCount > 0;
		if (Entry.bHasInstalledPair)
		{
			if (!Entry.bDiscovered) { NewDiscoveries.Add(Entry.RuleId); }
			Entry.bDiscovered = true;
		}
		Entry.AppliedBonusPercent = 0.f;
		Entry.ClosestDistanceMeters = -1.f;
		Entry.ConditionMetCount = 0;
		Entry.EffectAppliedCount = 0;
	}
	for (const FFF_FacilitySynergyPoint& Point : Points)
	{
		const FFF_FacilitySynergyEvaluation Result = Evaluator.Evaluate(Point);
		for (const FFF_FacilitySynergyMatch& Match : Result.Matches)
		{
			if (!Entries.IsValidIndex(Match.RuleIndex)) { continue; }
			FFF_FacilityCodexEntry& Entry = Entries[Match.RuleIndex];
			if (Match.bHasPartner)
			{
				const float Distance = static_cast<float>(FVector::Dist2D(Point.WorldLocation, Match.Partner.WorldLocation) / 100.);
				if (Entry.ClosestDistanceMeters < 0.f || Distance < Entry.ClosestDistanceMeters) { Entry.ClosestDistanceMeters = Distance; }
			}
			if (Match.bInRange)
			{
				Entry.bConditionMet = true;
				++Entry.ConditionMetCount;
			}
			if (Match.AppliedPercent > 0.f)
			{
				Entry.bEffectApplied = true;
				++Entry.EffectAppliedCount;
				Entry.AppliedBonusPercent = FMath::Max(Entry.AppliedBonusPercent, Match.AppliedPercent);
			}
		}
	}
}

FFF_FacilityCodexSummary FFF_FacilityCodexModel::Summarize(const TArray<FFF_FacilityCodexEntry>& Source)
{
	FFF_FacilityCodexSummary Summary;
	Summary.TotalCount = Source.Num();
	for (const FFF_FacilityCodexEntry& Entry : Source)
	{
		Summary.DiscoveredCount += Entry.bDiscovered ? 1 : 0;
		Summary.ConditionMetCount += Entry.bConditionMet ? 1 : 0;
		Summary.EffectAppliedCount += Entry.bEffectApplied ? 1 : 0;
	}
	return Summary;
}

bool FFF_FacilityCodexModel::MatchesFilter(const FFF_FacilityCodexEntry& Entry, const FFF_FacilityCodexFilter& Filter)
{
	if (!Filter.FacilityDefinitionId.IsNone() && Entry.TargetFacilityId != Filter.FacilityDefinitionId
		&& Entry.PartnerFacilityId != Filter.FacilityDefinitionId) { return false; }
	if (Filter.bFilterByCategory && Entry.TargetCategory != Filter.Category) { return false; }
	if (Filter.State == EFF_FacilityCodexFilterState::Undiscovered && Entry.bDiscovered) { return false; }
	if (Filter.State == EFF_FacilityCodexFilterState::Discovered && !Entry.bDiscovered) { return false; }
	if (Filter.State == EFF_FacilityCodexFilterState::Active && !Entry.bEffectApplied) { return false; }
	const FString Query = Filter.SearchText.TrimStartAndEnd();
	return Query.IsEmpty() || Entry.DisplayName.ToString().Contains(Query)
		|| Entry.TargetName.ToString().Contains(Query) || Entry.PartnerName.ToString().Contains(Query);
}

#undef LOCTEXT_NAMESPACE
