// 카탈로그 스냅샷을 UTileView용 UObject 목록으로 조정하고 객체 풀을 재사용한다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"
#include "FF_PrepBuildPresentationModel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnPrepBuildPresentationChanged);

/**
 * 배치 패널의 프레젠테이션 모델입니다.
 *
 * 카탈로그 소유권은 BuildCatalogSubsystem에 남겨 두고, 이 객체는 UTileView 가상화에 필요한
 * UObject만 시설 ID별로 재사용합니다. 필터/잔액 갱신 때 카드 객체를 매번 재생성하지 않습니다.
 */
UCLASS(BlueprintType, Transient)
class FESTAFIESTA_API UFF_PrepBuildPresentationModel final : public UObject
{
	GENERATED_BODY()

public:
	/** 현재 카탈로그와 필터를 원자적으로 반영합니다. 입력 배열의 결정적 순서를 보존합니다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Build")
	void Reconcile(
		EFF_BuildCatalogLoadState LoadState,
		const TArray<FFF_BuildCatalogEntry>& Entries,
		const FFF_PrepBuildFilter& Filter,
		int32 Gold,
		bool bHasAuthoritativeGold,
		const FText& FailureText);

	/** 경제 잔액 이벤트용 경량 갱신입니다. TileData 객체 identity와 목록 순서를 보존합니다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Build")
	void RefreshGold(int32 Gold, bool bHasAuthoritativeGold = true);

	/** 다음 Reconcile에서 추천 배지를 적용하고, 필터를 통과한 추천 항목을 먼저 보여 줍니다. */
	void SetSynergyHighlightIds(const TArray<FName>& DefinitionIds);

	/** UListView/UTileView::SetListItems에 바로 넘길 수 있는 배열 복사본입니다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	TArray<UObject*> GetListItems() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	FFF_PrepBuildPresentationSnapshot GetSnapshot() const { return Snapshot; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	UFF_PrepBuildTileData* FindTile(FName FacilityDefinitionId) const;

	/** 네이티브 패널이 중간 TArray 복사 없이 읽는 현재 가시 항목입니다. */
	const TArray<TObjectPtr<UFF_PrepBuildTileData>>& GetVisibleTiles() const { return VisibleTiles; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI|Prep|Build")
	FFF_OnPrepBuildPresentationChanged OnPresentationChanged;

private:
	TSet<FName> SynergyHighlightIds;

	/** 필터에서 잠시 빠진 항목도 같은 카탈로그에 있으면 identity를 유지합니다. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UFF_PrepBuildTileData>> TilePool;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_PrepBuildTileData>> VisibleTiles;

	UPROPERTY(Transient)
	FFF_PrepBuildPresentationSnapshot Snapshot;
};

