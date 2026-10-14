// FF_FacilityCodexProgressSubsystem.cpp
// 역할: 축제별 발견·보상 기록을 저장하고 새 축제에서 초기화한다.

#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"

#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/ConfigCacheIni.h"
#include "Save/FF_SaveArchive.h"
#include "Save/FF_SaveSubsystem.h"
#include "Subsystems/SubsystemCollection.h"

namespace
{
	constexpr const TCHAR* ProgressSection = TEXT("FestaFiesta.FacilityCodex");
	constexpr const TCHAR* IntroductionSeenKey = TEXT("bHasSeenIntroduction");
	TArray<FName> SortedIds(const TSet<FName>& Ids)
	{
		TArray<FName> Result = Ids.Array();
		Result.Sort(FNameLexicalLess());
		return Result;
	}
}

// 발견·수령 기록은 슬롯에서만 복원한다. 전역 설정에는 첫 안내 여부만 남긴다.
void UFF_FacilityCodexProgressSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bHasSeenIntroduction = false;
	DiscoveredRuleIds.Reset();
	ClaimedDiscoveryRewardRuleIds.Reset();
	ClaimedCollectionRewardIds.Reset();
	if (GConfig && !GGameUserSettingsIni.IsEmpty())
	{
		GConfig->GetBool(ProgressSection, IntroductionSeenKey, bHasSeenIntroduction, GGameUserSettingsIni);
	}
	Festival = Collection.InitializeDependency<UFF_FestivalProgressionSubsystem>();
	Save = Collection.InitializeDependency<UFF_SaveSubsystem>();
	if (Festival)
	{
		// StartFestival also covers restarts using the same festival definition.
		Festival->OnFestivalDefinitionChanged.AddUniqueDynamic(this, &ThisClass::ResetForNewFestival);
	}
	if (Save) { Save->RegisterSaveable(this); }
}

void UFF_FacilityCodexProgressSubsystem::ResetForNewFestival()
{
	DiscoveredRuleIds.Reset();
	ClaimedDiscoveryRewardRuleIds.Reset();
	ClaimedCollectionRewardIds.Reset();
	OnProgressChanged.Broadcast();
}

void UFF_FacilityCodexProgressSubsystem::WriteSaveData(TArray<uint8>& OutBytes) const
{
	FFF_FacilityCodexProgressSave State;
	State.DiscoveredRuleIds = SortedIds(DiscoveredRuleIds);
	State.ClaimedDiscoveryRewardRuleIds = SortedIds(ClaimedDiscoveryRewardRuleIds);
	State.ClaimedCollectionRewardIds = SortedIds(ClaimedCollectionRewardIds);
	UFF_SaveArchive::WriteStruct(OutBytes, State);
}

void UFF_FacilityCodexProgressSubsystem::ReadSaveData(const TArray<uint8>& InBytes)
{
	FFF_FacilityCodexProgressSave State;
	const bool bLoaded = UFF_SaveArchive::ReadStruct(InBytes, State) && State.Version == 1;
	// Replacement, never a merge: switching slots or loading a legacy slot must not
	// inherit discoveries/claims from the festival that was previously in memory.
	DiscoveredRuleIds.Reset();
	ClaimedDiscoveryRewardRuleIds.Reset();
	ClaimedCollectionRewardIds.Reset();
	if (bLoaded)
	{
		for (const FName RuleId : State.DiscoveredRuleIds)
		{
			if (!RuleId.IsNone()) { DiscoveredRuleIds.Add(RuleId); }
		}
		for (const FName RuleId : State.ClaimedDiscoveryRewardRuleIds)
		{
			if (DiscoveredRuleIds.Contains(RuleId)) { ClaimedDiscoveryRewardRuleIds.Add(RuleId); }
		}
		for (const FName MilestoneId : State.ClaimedCollectionRewardIds)
		{
			if (!MilestoneId.IsNone()) { ClaimedCollectionRewardIds.Add(MilestoneId); }
		}
	}
	OnProgressChanged.Broadcast();
}

void UFF_FacilityCodexProgressSubsystem::Deinitialize()
{
	if (Festival) { Festival->OnFestivalDefinitionChanged.RemoveDynamic(this, &ThisClass::ResetForNewFestival); }
	if (Save) { Save->UnregisterSaveable(this); }
	Festival = nullptr;
	Save = nullptr;
	OnProgressChanged.Clear();
	Super::Deinitialize();
}

bool UFF_FacilityCodexProgressSubsystem::MarkIntroductionSeen()
{
	if (bHasSeenIntroduction) { return false; }
	bHasSeenIntroduction = true;
	SaveIntroduction();
	OnProgressChanged.Broadcast();
	return true;
}

// 새 조합만 기록하고 실제 변화가 있을 때 화면에 알린다. 디스크 저장은 기존 슬롯 저장 경로에 맡긴다.
bool UFF_FacilityCodexProgressSubsystem::RecordDiscoveries(const TSet<FName>& RuleIds)
{
	if (Save && Save->IsRestoring()) { return false; }
	const int32 PreviousCount = DiscoveredRuleIds.Num();
	for (const FName RuleId : RuleIds)
	{
		if (!RuleId.IsNone()) { DiscoveredRuleIds.Add(RuleId); }
	}
	if (DiscoveredRuleIds.Num() == PreviousCount) { return false; }
	OnProgressChanged.Broadcast();
	return true;
}

// 발견했고 아직 받지 않은 보상만 지급한다. 수령 기록을 먼저 확정하여 이벤트 재진입을 방어한다.
bool UFF_FacilityCodexProgressSubsystem::ClaimDiscoveryReward(
	const FName RuleId, const int32 RewardGold, UFF_FestivalProgressionSubsystem* Economy)
{
	if (RuleId.IsNone() || RewardGold <= 0 || !DiscoveredRuleIds.Contains(RuleId)
		|| ClaimedDiscoveryRewardRuleIds.Contains(RuleId) || !Economy || !Economy->IsFestivalStarted()
		|| (Save && Save->IsRestoring()))
	{
		return false;
	}
	// Commit before wallet notifications. Reentrant listeners see the claim spent.
	ClaimedDiscoveryRewardRuleIds.Add(RuleId);
	Economy->GrantDiscoveryGold(RewardGold);
	OnProgressChanged.Broadcast();
	return true;
}

bool UFF_FacilityCodexProgressSubsystem::ClaimCollectionReward(const FName MilestoneId,
	const int32 RequiredDiscoveries, const int32 RewardGold, const TSet<FName>& AuthoredRuleIds,
	UFF_FestivalProgressionSubsystem* Economy)
{
	if (!IsInGameThread() || MilestoneId.IsNone() || RequiredDiscoveries <= 0 || RewardGold <= 0
		|| ClaimedCollectionRewardIds.Contains(MilestoneId) || !Economy || !Economy->IsFestivalStarted()
		|| (Save && Save->IsRestoring()))
	{
		return false;
	}
	int32 CurrentDiscoveries = 0;
	for (const FName RuleId : AuthoredRuleIds)
	{
		if (!RuleId.IsNone() && DiscoveredRuleIds.Contains(RuleId)) { ++CurrentDiscoveries; }
	}
	if (CurrentDiscoveries < RequiredDiscoveries) { return false; }
	// Commit before wallet listeners run. The same claim state is captured with the wallet in the slot.
	ClaimedCollectionRewardIds.Add(MilestoneId);
	Economy->GrantDiscoveryGold(RewardGold);
	OnProgressChanged.Broadcast();
	return true;
}

// 안내는 사용자 취향이므로 축제 슬롯과 별개로 한 번만 저장한다.
void UFF_FacilityCodexProgressSubsystem::SaveIntroduction() const
{
	if (!GConfig || GGameUserSettingsIni.IsEmpty()) { return; }
	GConfig->SetBool(ProgressSection, IntroductionSeenKey, bHasSeenIntroduction, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
