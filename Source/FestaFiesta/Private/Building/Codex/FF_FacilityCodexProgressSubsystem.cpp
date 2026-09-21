#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"

#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	constexpr const TCHAR* ProgressSection = TEXT("FestaFiesta.FacilityCodex");
	constexpr const TCHAR* ProgressKey = TEXT("DiscoveredRuleIds");
	constexpr const TCHAR* ClaimedRewardsKey = TEXT("ClaimedDiscoveryRewardRuleIds");
	constexpr const TCHAR* RewardVersionKey = TEXT("DiscoveryRewardClaimVersion");
	constexpr int32 ManualRewardVersion = 1;
}

void UFF_FacilityCodexProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	DiscoveredRuleIds.Reset();
	ClaimedDiscoveryRewardRuleIds.Reset();
	if (!GConfig || GGameUserSettingsIni.IsEmpty()) { return; }
	TArray<FString> SavedIds;
	GConfig->GetArray(ProgressSection, ProgressKey, SavedIds, GGameUserSettingsIni);
	for (const FString& SavedId : SavedIds)
	{
		const FName RuleId(*SavedId.TrimStartAndEnd());
		if (!RuleId.IsNone()) { DiscoveredRuleIds.Add(RuleId); }
	}
	int32 RewardVersion = 0;
	GConfig->GetInt(ProgressSection, RewardVersionKey, RewardVersion, GGameUserSettingsIni);
	if (RewardVersion < ManualRewardVersion)
	{
		// Older profiles received rewards automatically when discoveries were recorded.
		// Migrate those IDs once so switching to manual collection cannot pay them again.
		ClaimedDiscoveryRewardRuleIds = DiscoveredRuleIds;
		SaveProgress();
	}
	else
	{
		SavedIds.Reset();
		GConfig->GetArray(ProgressSection, ClaimedRewardsKey, SavedIds, GGameUserSettingsIni);
		for (const FString& SavedId : SavedIds)
		{
			const FName RuleId(*SavedId.TrimStartAndEnd());
			if (DiscoveredRuleIds.Contains(RuleId)) { ClaimedDiscoveryRewardRuleIds.Add(RuleId); }
		}
	}
}

void UFF_FacilityCodexProgressSubsystem::Deinitialize()
{
	OnProgressChanged.Clear();
	Super::Deinitialize();
}

bool UFF_FacilityCodexProgressSubsystem::RecordDiscoveries(const TSet<FName>& RuleIds)
{
	const int32 PreviousCount = DiscoveredRuleIds.Num();
	for (const FName RuleId : RuleIds)
	{
		if (!RuleId.IsNone()) { DiscoveredRuleIds.Add(RuleId); }
	}
	if (DiscoveredRuleIds.Num() == PreviousCount) { return false; }
	SaveProgress();
	OnProgressChanged.Broadcast();
	return true;
}

bool UFF_FacilityCodexProgressSubsystem::ClaimDiscoveryReward(
	const FName RuleId, const int32 RewardGold, UFF_FestivalProgressionSubsystem* Economy)
{
	if (RuleId.IsNone() || RewardGold <= 0 || !DiscoveredRuleIds.Contains(RuleId)
		|| ClaimedDiscoveryRewardRuleIds.Contains(RuleId) || !Economy || !Economy->IsFestivalStarted())
	{
		return false;
	}
	// Commit before wallet/profile notifications. Reentrant listeners see the claim spent.
	ClaimedDiscoveryRewardRuleIds.Add(RuleId);
	SaveProgress();
	Economy->GrantDiscoveryGold(RewardGold);
	OnProgressChanged.Broadcast();
	return true;
}

void UFF_FacilityCodexProgressSubsystem::SaveProgress() const
{
	if (!GConfig || GGameUserSettingsIni.IsEmpty()) { return; }
	TArray<FString> SavedIds;
	SavedIds.Reserve(DiscoveredRuleIds.Num());
	for (const FName RuleId : DiscoveredRuleIds) { SavedIds.Add(RuleId.ToString()); }
	SavedIds.Sort();
	// Same local-player persistence boundary as build favorites. No project defaults or festival saves touched.
	GConfig->SetArray(ProgressSection, ProgressKey, SavedIds, GGameUserSettingsIni);
	SavedIds.Reset();
	SavedIds.Reserve(ClaimedDiscoveryRewardRuleIds.Num());
	for (const FName RuleId : ClaimedDiscoveryRewardRuleIds) { SavedIds.Add(RuleId.ToString()); }
	SavedIds.Sort();
	GConfig->SetArray(ProgressSection, ClaimedRewardsKey, SavedIds, GGameUserSettingsIni);
	GConfig->SetInt(ProgressSection, RewardVersionKey, ManualRewardVersion, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
