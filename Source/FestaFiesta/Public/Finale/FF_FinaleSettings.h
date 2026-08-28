// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_FinaleSettings.generated.h"

class UFF_FinaleShowData;

/**
 * 피날레 연출의 프로젝트 기본값.
 * Project Settings > Game > FestaFiesta Finale 에서 지정한다.
 *
 * 레벨마다 연출이 다를 수 있어서 GameMode 쪽 지정이 우선한다. 여기 값은 그게 비어 있을 때
 * 쓰는 기본값이다. 새 레벨을 만들 때마다 매번 지정하지 않아도 되게 하려는 것이고,
 * 특별한 연출이 필요한 맵만 GameMode 에서 덮으면 된다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Finale"))
class FESTAFIESTA_API UFF_FinaleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Finale")
	static const UFF_FinaleSettings* Get();

	// 등급별 연출 수치. 비어 있으면 코드에 박힌 기본값으로 돈다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Finale", meta = (DisplayName = "Default Show Data"))
	TSoftObjectPtr<UFF_FinaleShowData> DefaultShowData;
};
