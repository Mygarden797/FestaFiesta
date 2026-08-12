// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_BuildCatalogEconomyReadModel.generated.h"

class UFF_FestivalProgressionSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFF_OnBuildCatalogGoldChanged,
	int32,
	NewGold);

/**
 * 건축 카탈로그가 사용하는 경제 읽기 모델입니다.
 *
 * 팀 공용 UI ViewModel을 변경하지 않으면서도 위젯이 경제 소유자에게 직접 결합되지 않도록,
 * Progression의 실제 Gold를 같은 GameInstance 안에서 구독해 읽기 전용으로 중계합니다.
 */
UCLASS()
class FESTAFIESTA_API UFF_BuildCatalogEconomyReadModel : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Economy")
	int32 GetGold() const { return Gold; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Economy")
	bool HasAuthoritativeGold() const { return bHasAuthoritativeGold; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Building|Economy")
	FFF_OnBuildCatalogGoldChanged OnGoldChanged;

private:
	UFUNCTION()
	void HandleProgressionGoldChanged(int32 NewGold);

	UPROPERTY()
	TObjectPtr<UFF_FestivalProgressionSubsystem> ProgressionSubsystem;

	int32 Gold = 0;
	bool bHasAuthoritativeGold = false;
};
