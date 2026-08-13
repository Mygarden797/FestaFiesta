// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_FestivalSettings.generated.h"

class UFF_FestivalDefinition;

/**
 * 프로젝트가 시작할 기본 축제 구성을 선택하는 전역 설정.
 * Project Settings > Game > FestaFiesta Festival에서 기본 UFF_FestivalDefinition을 지정한다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Festival"))
class FESTAFIESTA_API UFF_FestivalSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// 현재 프로젝트의 축제 설정 기본 객체를 반환한다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Festival")
	static const UFF_FestivalSettings* Get();

	// 부팅 시 시작할 축제 정의. 비어 있으면 축제 진행 상태를 초기화하지 않는다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Startup", meta = (DisplayName = "Default Festival"))
	TSoftObjectPtr<UFF_FestivalDefinition> DefaultFestival;
};
