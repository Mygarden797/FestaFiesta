// FF_FacilityCodexProgressSubsystem.h
// 역할: 축제별 발견·보상 기록을 저장하고 새 축제에서 초기화한다.

#pragma once

#include "CoreMinimal.h"
#include "Save/FF_Saveable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_FacilityCodexProgressSubsystem.generated.h"

class UFF_FestivalProgressionSubsystem;
class UFF_SaveSubsystem;

/** Only irreversible festival progress; onboarding remains a local profile preference. */
USTRUCT()
struct FFF_FacilityCodexProgressSave
{
	GENERATED_BODY()
	UPROPERTY() int32 Version = 1;
	UPROPERTY() TArray<FName> DiscoveredRuleIds;
	UPROPERTY() TArray<FName> ClaimedDiscoveryRewardRuleIds;
	UPROPERTY() TArray<FName> ClaimedCollectionRewardIds;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnFacilityCodexProgressChanged);

/** Festival collection history. Survives map travel, restores with its slot, resets on a new festival. */
UCLASS()
class FESTAFIESTA_API UFF_FacilityCodexProgressSubsystem : public UGameInstanceSubsystem, public IFF_Saveable
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual FName GetSaveKey() const override { return TEXT("FacilityCodex"); }
	virtual void WriteSaveData(TArray<uint8>& OutBytes) const override;
	virtual void ReadSaveData(const TArray<uint8>& InBytes) override;
	/** One boundary for discoveries and both reward types; keeps first-open guidance. */
	UFUNCTION()
	void ResetForNewFestival();
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
	/** Records confirmed discoveries for the active festival; never grants gold or writes config. */
	bool RecordDiscoveries(const TSet<FName>& RuleIds);
	/** Called with an authored amount by the world codex. Failed/duplicate claims do not consume a reward. */
	bool ClaimDiscoveryReward(FName RuleId, int32 RewardGold, UFF_FestivalProgressionSubsystem* Economy);
	/** Authored IDs come from the current rule table, excluding removed recipes. */
	bool ClaimCollectionReward(FName MilestoneId, int32 RequiredDiscoveries, int32 RewardGold,
		const TSet<FName>& AuthoredRuleIds, UFF_FestivalProgressionSubsystem* Economy);
	UPROPERTY(BlueprintAssignable, Category="FestaFiesta|Facility Codex")
	FFF_OnFacilityCodexProgressChanged OnProgressChanged;

private:
	UPROPERTY(Transient) bool bHasSeenIntroduction = false;
	UPROPERTY(Transient) TSet<FName> DiscoveredRuleIds;
	UPROPERTY(Transient) TSet<FName> ClaimedDiscoveryRewardRuleIds;
	UPROPERTY(Transient) TSet<FName> ClaimedCollectionRewardIds;
	UPROPERTY(Transient) TObjectPtr<UFF_FestivalProgressionSubsystem> Festival;
	UPROPERTY(Transient) TObjectPtr<UFF_SaveSubsystem> Save;
	void SaveIntroduction() const;
};
