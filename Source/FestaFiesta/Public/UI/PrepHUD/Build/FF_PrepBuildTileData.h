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
	FText GetCategoryPathText() const { return CategoryPathText; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	FString GetUniqueNameToken() const { return Entry.UniqueNameToken; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	int32 GetInstallCost() const { return Entry.InstallCost; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	bool HasResolvedCost() const { return Entry.HasValidInstallCost(); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build")
	bool IsAffordable() const { return TileState == EFF_PrepBuildTileState::Available; }

	/** 필수 설치 안내는 권위 상태의 스냅샷이며 가격·건축 한도·조합 추천과 독립적입니다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Required")
	bool IsRequiredFacility() const { return Entry.bRequiredForSetup; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Required")
	bool IsRequiredFacilitySatisfied() const { return Entry.bRequiredSetupSatisfied; }

	/** 현재 축제에서 편성 가능한 프로그램을 가진 시설입니다. 구매/편성 완료 여부와 별개입니다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Program")
	bool HasCompatiblePrograms() const { return Entry.CompatibleProgramCount > 0; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Required")
	int32 GetRequiredFacilityCurrentCount() const { return Entry.RequiredInstalledCount; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Required")
	int32 GetRequiredFacilityMinimumCount() const { return Entry.RequiredMinimumCount; }

	/** 조합 추천은 건축 가능 여부와 별개이며, 가격·건축 한도를 변경하지 않습니다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Synergy")
	bool IsSynergyHighlighted() const { return bSynergyHighlighted; }

	void SetSynergyHighlighted(bool bHighlighted) { bSynergyHighlighted = bHighlighted; }

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
	FText CategoryPathText;

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
	bool bSynergyHighlighted = false;
	bool bHasAuthoritativeGold = false;
};

