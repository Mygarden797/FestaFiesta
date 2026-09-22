// FF_FacilitySynergySubsystem.h
// 역할: 시설 변경 시 조합 색인을 갱신하고 수익 배율을 재사용 가능한 캐시로 제공한다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Building/Synergy/FF_FacilitySynergyEvaluator.h"
#include "FF_FFZSTypes.h"
#include "FF_FacilitySynergySubsystem.generated.h"

class UFFZSWorldSubsystem;
class UDataTable;

/** 설치·이동·철거·복원 때 무효화, 첫 조회 때 재구성. Tick/결제별 전체 검색 없음. */
UCLASS()
class FESTAFIESTA_API UFF_FacilitySynergySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	/** Catalog suggestions from enabled valid rules, including when the selected type is the helper.
	 * This is a type relationship, not a promise of an in-range or uncapped live bonus. */
	UFUNCTION(BlueprintPure, Category="FestaFiesta|시설 조합")
	TArray<FName> GetRelatedFacilityDefinitionIds(FName DefinitionId) const;
	UFUNCTION(BlueprintPure, Category="FestaFiesta|시설 조합")
	float GetBonusPercent(FGuid FacilityInstanceId) const;
	UFUNCTION(BlueprintPure, Category="FestaFiesta|시설 조합")
	float GetRevenueMultiplier(FGuid FacilityInstanceId) const;
	/** A moved/resized beneficiary only; shares live evaluation without building reverse UI entries. */
	float GetRevenueMultiplierAt(FGuid FacilityInstanceId, const FVector& ProposedWorldLocation) const;
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|시설 조합")
	void BuildPlacementPreview(FName DefinitionId, const FVector& WorldLocation,
		FGuid MovingInstanceId, FFF_FacilitySynergyPreview& OutPreview) const;
	uint64 GetRevision() const;

private:
	UPROPERTY(Transient) TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;
	UPROPERTY(Transient) TObjectPtr<UDataTable> LoadedTable;
	mutable FFF_FacilitySynergyEvaluator Evaluator;
	mutable TMap<FGuid, float> RevenueCache;
	mutable int64 CachedWorldRevision = -1;
	mutable uint64 CachedRegistryRevision = MAX_uint64;
	mutable uint64 Revision = 1;
	mutable bool bIndexDirty = true;
	void EnsureIndex() const;
	UFUNCTION() void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);
	UFUNCTION() void HandleSnapshotRestored();
};
