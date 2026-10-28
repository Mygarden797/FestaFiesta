// FF_FacilityCodexSubsystem.h
// 역할: 월드·카탈로그·발견 기록 변경을 도감 화면과 보상 청구로 연결한다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "Building/Codex/FF_FacilityCodexModel.h"
#include "FF_FacilityCodexSubsystem.generated.h"

class UDataTable;
class UFFZSWorldSubsystem;
class UFF_BuildCatalogSubsystem;
class UFF_FacilityCodexProgressSubsystem;
class UFF_FestivalProgressionSubsystem;
class UFF_SaveSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnFacilityCodexChanged);

/** Event-driven collection projection. No tick, polling, actor search or asset load on UI getters. */
UCLASS()
class FESTAFIESTA_API UFF_FacilityCodexSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	TArray<FFF_FacilityCodexEntry> GetEntries() const { return CachedEntries; }
	const TArray<FFF_FacilityCodexEntry>& GetEntriesView() const { return CachedEntries; }
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	FFF_FacilityCodexSummary GetSummary() const { return CachedSummary; }
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	bool FindEntry(FName RuleId, FFF_FacilityCodexEntry& OutEntry) const;
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	TArray<FFF_FacilityCodexEntry> GetFilteredEntries(const FFF_FacilityCodexFilter& Filter) const;
	/** Claims a discovered recipe's authored reward once per festival. */
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Facility Codex")
	bool ClaimDiscoveryReward(FName RuleId);
	/** Global collection progress, independent of the screen's category/search filters. */
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	TArray<FFF_FacilityCodexCollectionMilestone> GetCollectionMilestones() const;
	/** Earned recipe or collection rewards still awaiting a claim, including during startup. */
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	bool HasUnclaimedRewards() const;
	/** Claims one configured collection stage once per festival, using the same Gold wallet. */
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Facility Codex")
	bool ClaimCollectionReward(FName MilestoneId);
	/** Explicit boundary refresh, useful after external registry reconstruction. Not a per-frame call. */
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Facility Codex")
	void Refresh();
	UPROPERTY(BlueprintAssignable, Category="FestaFiesta|Facility Codex")
	FFF_OnFacilityCodexChanged OnCodexChanged;

private:
	void BindGameInstance();
	void PublishEntries();
	void CompleteStartup();
	UFUNCTION() void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);
	UFUNCTION() void HandleCatalogChanged();
	UFUNCTION() void HandleProgressChanged();
	UFUNCTION() void HandleSaveLoaded(FName SlotId);
	UPROPERTY(Transient) TObjectPtr<UDataTable> LoadedTable;
	UPROPERTY(Transient) TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;
	UPROPERTY(Transient) TObjectPtr<UFF_BuildCatalogSubsystem> Catalog;
	UPROPERTY(Transient) TObjectPtr<UFF_FacilityCodexProgressSubsystem> Progress;
	UPROPERTY(Transient) TObjectPtr<UFF_FestivalProgressionSubsystem> Economy;
	UPROPERTY(Transient) TObjectPtr<UFF_SaveSubsystem> Save;
	UPROPERTY(Transient) TArray<FFF_FacilityCodexEntry> CachedEntries;
	FFF_FacilityCodexSummary CachedSummary;
	FFF_FacilityCodexModel Model;
	TSet<FName> SessionDiscoveries;
	TMap<FName, int32> EntryByRuleId;
	TSet<FGuid> EligibleInstanceIds;
	TSet<FGuid> UnreferencedInstanceIds;
	TSet<FName> ReferencedDefinitionIds;
	uint64 CachedRegistryRevision = MAX_uint64;
	FTimerHandle StartupRewardTimer;
	bool bReadyToClaimRewards = false;
	bool bRefreshing = false;
};
