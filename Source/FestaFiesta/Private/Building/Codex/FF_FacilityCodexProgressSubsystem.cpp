// FF_FacilityCodexProgressSubsystem.cpp
// 역할: 발견한 조합과 받은 보상을 사용자 설정에 저장하여 중복 보상을 막는다.

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

// 사용자별 발견·수령 기록을 읽는다. 자동 보상 방식의 옛 기록은 이미 받은 보상으로 한 번만 이전한다.
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

// 새 조합만 저장하고 실제 변화가 있을 때 화면에 알린다. 같은 발견을 여러 번 기록해도 보상은 늘지 않는다.
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

// 발견했고 아직 받지 않은 보상만 지급한다. 수령 기록을 먼저 확정하여 이벤트 재진입을 방어한다.
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

// 집합을 정렬된 ID 목록으로 저장해 실행 순서와 관계없이 사용자 설정 결과를 안정적으로 유지한다.
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
