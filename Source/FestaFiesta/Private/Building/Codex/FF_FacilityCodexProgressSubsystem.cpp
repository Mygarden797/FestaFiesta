#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
	constexpr const TCHAR* ProgressSection = TEXT("FestaFiesta.FacilityCodex");
	constexpr const TCHAR* ProgressKey = TEXT("DiscoveredRuleIds");
}

void UFF_FacilityCodexProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	DiscoveredRuleIds.Reset();
	if (!GConfig || GGameUserSettingsIni.IsEmpty()) { return; }
	TArray<FString> SavedIds;
	GConfig->GetArray(ProgressSection, ProgressKey, SavedIds, GGameUserSettingsIni);
	for (const FString& SavedId : SavedIds)
	{
		const FName RuleId(*SavedId.TrimStartAndEnd());
		if (!RuleId.IsNone()) { DiscoveredRuleIds.Add(RuleId); }
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
	// One write per newly discovered batch. Reopening the UI and repeated matches never write.
	SaveProgress();
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
	GConfig->Flush(false, GGameUserSettingsIni);
}
