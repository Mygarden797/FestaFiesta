// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_AudioSettings.generated.h"

class UFF_AudioBankData;

/**
 * 축제 사운드의 프로젝트 기본값.
 * Project Settings > Game > FestaFiesta Audio 에서 지정한다.
 *
 * 레벨마다 분위기가 다를 수 있어 GameMode 쪽 지정이 우선한다. 여기 값은 그게 비어 있을 때
 * 쓰는 기본값이다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Audio"))
class FESTAFIESTA_API UFF_AudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Audio")
	static const UFF_AudioSettings* Get();

	// 이벤트별 소리 표. 비어 있으면 소리가 나지 않을 뿐 게임은 그대로 돈다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Audio",
		meta = (DisplayName = "Default Audio Bank"))
	TSoftObjectPtr<UFF_AudioBankData> DefaultAudioBank;
};
