#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Building/Codex/FF_FacilityCodexModel.h"
#include "FF_FacilityCodexSubsystem.generated.h"

class UDataTable;
class UFFZSWorldSubsystem;
class UFF_BuildCatalogSubsystem;
class UFF_FacilityCodexProgressSubsystem;

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
	/** Explicit boundary refresh, useful after external registry reconstruction. Not a per-frame call. */
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Facility Codex")
	void Refresh();
	UPROPERTY(BlueprintAssignable, Category="FestaFiesta|Facility Codex")
	FFF_OnFacilityCodexChanged OnCodexChanged;

private:
	void BindGameInstance();
	void PublishEntries();
	UFUNCTION() void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);
	UFUNCTION() void HandleCatalogChanged();
	UFUNCTION() void HandleProgressChanged();
	UPROPERTY(Transient) TObjectPtr<UDataTable> LoadedTable;
	UPROPERTY(Transient) TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;
	UPROPERTY(Transient) TObjectPtr<UFF_BuildCatalogSubsystem> Catalog;
	UPROPERTY(Transient) TObjectPtr<UFF_FacilityCodexProgressSubsystem> Progress;
	UPROPERTY(Transient) TArray<FFF_FacilityCodexEntry> CachedEntries;
	FFF_FacilityCodexSummary CachedSummary;
	FFF_FacilityCodexModel Model;
	TSet<FName> SessionDiscoveries;
	TMap<FName, int32> EntryByRuleId;
	TSet<FGuid> EligibleInstanceIds;
	uint64 CachedRegistryRevision = MAX_uint64;
	bool bRefreshing = false;
};
