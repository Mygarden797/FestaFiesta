// 시설 조합 도감의 수집 진행도 보상 기준과 Gold 금액을 설정한다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Building/Codex/FF_FacilityCodexTypes.h"
#include "FF_FacilityCodexSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="FestaFiesta 시설 조합 도감"))
class FESTAFIESTA_API UFF_FacilityCodexSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UFF_FacilityCodexSettings();
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	/** IDs identify profile claims and must remain stable after release. Only current recipes count. */
	UPROPERTY(EditAnywhere, Config, Category="수집 진행도", meta=(TitleProperty="MilestoneId", DisplayName="수집 보상 단계", ToolTip="필요한 발견 조합 수와 Gold 보상을 설정합니다. 지급된 단계의 ID는 유지해야 중복 지급을 방지합니다. 빈 배열은 수집 보상을 끕니다."))
	TArray<FFF_FacilityCodexCollectionReward> CollectionRewards;
	/** Skips invalid, duplicate, or unreachable stages, retaining the first authored stage per ID/count. */
	TArray<FFF_FacilityCodexCollectionReward> GetValidCollectionRewards(int32 TotalRecipeCount) const;
};
