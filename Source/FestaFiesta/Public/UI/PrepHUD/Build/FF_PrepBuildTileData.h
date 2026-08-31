// UTileView가 가상화해 소비할 시설 카드 데이터 객체를 선언한다.
// 카탈로그 항목은 값으로 복사하고 이미지 soft reference는 동기 로드하지 않는다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationTypes.h"
#include "FF_PrepBuildTileData.generated.h"

class UTexture2D;

/** UTileView의 ListItems에 넣는 표현 전용 UObject입니다. */
UCLASS(BlueprintType, Transient)
class FESTAFIESTA_API UFF_PrepBuildTileData final : public UObject
{
	GENERATED_BODY()

public:
	/** 카탈로그 스냅샷과 현재 잔액을 복사해 모든 표시 값을 한 번에 갱신합니다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Build")
	void Initialize(
		const FFF_BuildCatalogEntry& InEntry,
		int32 InGold,
		bool bInHasAuthoritativeGold = true);

	/** 잔액 이벤트에는 카탈로그 스냅샷을 다시 만들지 않고 상태만 갱신합니다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Build")
	void RefreshGold(int32 InGold, bool bInHasAuthoritativeGold = true);

	/** 네이티브 패널이 배치 요청 ID와 원본 soft reference를 읽는 불변 스냅샷입니다. */
	const FFF_BuildCatalogEntry& GetEntry() const { return Entry; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	FName GetFacilityDefinitionId() const { return Entry.FacilityDefinitionId; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	int32 GetInstallCost() const { return Entry.InstallCost; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	bool HasResolvedCost() const { return Entry.bHasResolvedInstallCost && Entry.InstallCost > 0; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	bool IsAffordable() const { return TileState == EFF_PrepBuildTileState::Available; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	EFF_PrepBuildTileState GetTileState() const { return TileState; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	FText GetCostText() const { return CostText; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	FText GetBuildLimitText() const { return BuildLimitText; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	FText GetStateLabelText() const { return StateLabelText; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	FText GetStateText() const { return StateText; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	TSoftObjectPtr<UTexture2D> GetThumbnail() const { return Entry.Thumbnail; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	EFFZSFacilityCategory GetCategory() const { return Entry.Category; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	FName GetMinorToken() const { return Entry.MinorToken; }

private:
	void RefreshDerivedValues();

	/** 원본 카탈로그 배열의 수명과 분리된 카드별 값 복사본입니다. */
	UPROPERTY(Transient)
	FFF_BuildCatalogEntry Entry;

	UPROPERTY(Transient)
	FText DisplayName;

	UPROPERTY(Transient)
	FText CostText;

	UPROPERTY(Transient)
	FText BuildLimitText;

	/** 타일 안에는 짧은 라벨만 표시하고, 전체 사유는 StateText로 안내합니다. */
	UPROPERTY(Transient)
	FText StateLabelText;

	UPROPERTY(Transient)
	FText StateText;

	UPROPERTY(Transient)
	EFF_PrepBuildTileState TileState = EFF_PrepBuildTileState::GoldUnavailable;

	int32 Gold = 0;
	bool bHasAuthoritativeGold = false;
};

