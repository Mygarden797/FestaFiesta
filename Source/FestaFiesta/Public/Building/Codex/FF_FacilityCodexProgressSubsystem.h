// FF_FacilityCodexProgressSubsystem.h
// 역할: 발견한 조합과 받은 보상을 사용자 설정에 저장하여 중복 보상을 막는다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_FacilityCodexProgressSubsystem.generated.h"

class UFF_FestivalProgressionSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnFacilityCodexProgressChanged);

/** Local profile collection history. Survives map changes and restarts; independent of festival slots. */
UCLASS()
class FESTAFIESTA_API UFF_FacilityCodexProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	/** First-open guidance belongs to the local profile, not an individual festival save. */
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	bool HasSeenIntroduction() const { return bHasSeenIntroduction; }
	/** Records a successfully shown introduction once and refreshes every launcher. */
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Facility Codex")
	bool MarkIntroductionSeen();
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	bool IsDiscovered(FName RuleId) const { return DiscoveredRuleIds.Contains(RuleId); }
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	TSet<FName> GetDiscoveredRuleIds() const { return DiscoveredRuleIds; }
	const TSet<FName>& GetDiscoveredRuleIdsView() const { return DiscoveredRuleIds; }
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	bool IsDiscoveryRewardClaimed(FName RuleId) const { return ClaimedDiscoveryRewardRuleIds.Contains(RuleId); }
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	bool IsCollectionRewardClaimed(FName MilestoneId) const { return ClaimedCollectionRewardIds.Contains(MilestoneId); }
	/** Persists confirmed discoveries independently of the economy; never grants gold. */
	bool RecordDiscoveries(const TSet<FName>& RuleIds);
	/** Called with an authored amount by the world codex. Failed/duplicate claims do not consume a reward. */
	bool ClaimDiscoveryReward(FName RuleId, int32 RewardGold, UFF_FestivalProgressionSubsystem* Economy);
	/** Authored IDs come from the current rule table, excluding stale profile discoveries. */
	bool ClaimCollectionReward(FName MilestoneId, int32 RequiredDiscoveries, int32 RewardGold,
		const TSet<FName>& AuthoredRuleIds, UFF_FestivalProgressionSubsystem* Economy);
	UPROPERTY(BlueprintAssignable, Category="FestaFiesta|Facility Codex")
	FFF_OnFacilityCodexProgressChanged OnProgressChanged;

private:
	UPROPERTY(Transient) bool bHasSeenIntroduction = false;
	UPROPERTY(Transient) TSet<FName> DiscoveredRuleIds;
	UPROPERTY(Transient) TSet<FName> ClaimedDiscoveryRewardRuleIds;
	UPROPERTY(Transient) TSet<FName> ClaimedCollectionRewardIds;
	void SaveProgress() const;
};
