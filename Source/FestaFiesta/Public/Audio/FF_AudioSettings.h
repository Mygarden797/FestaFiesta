// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_AudioSettings.generated.h"

class UFF_AudioBankData;

// 축제 사운드의 프로젝트 기본값.
// Project Settings > Game > FestaFiesta Audio 에서 지정한다.
//
// 레벨마다 분위기가 다를 수 있어 GameMode 쪽 지정이 우선한다. 여기 값은 그게 비어 있을 때
// 쓰는 기본값이다.
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

	/* 피날레 불꽃 터지는 소리를 소리가 날아오는 시간만큼 늦게 낸다.

	   실제 불꽃놀이는 빛이 먼저 보이고 쾅 소리가 뒤따른다. 소리는 초속 약 343m(20도 공기)라
	   300m 밖에서 터지면 0.9초쯤 늦는다. 번쩍임과 소리가 같은 프레임에 나면 불꽃이
	   코앞에서 터지는 것처럼 느껴져 바다 건너 멀리 있다는 거리감이 사라진다.
	*/
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Audio|Finale")
	bool bDelayFinaleBurstBySoundTravel = true;

	// 소리 속도(cm/s). 20도 공기 기준 약 34300.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Audio|Finale",
		meta = (ClampMin = "1000.0", EditCondition = "bDelayFinaleBurstBySoundTravel"))
	float SpeedOfSoundCmPerSecond = 34300.f;

	// 지연 상한(초). 카메라가 아주 멀리 있어도 소리가 연출 끝난 뒤로 밀리지 않게 한다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Audio|Finale",
		meta = (ClampMin = "0.0", EditCondition = "bDelayFinaleBurstBySoundTravel"))
	float MaxFinaleBurstSoundDelaySeconds = 3.f;

	// 거리(cm)에서 소리가 도착하기까지 걸리는 시간(초). 속도가 0 이하이면 지연하지 않는다.
	static float ComputeSoundTravelDelay(float DistanceCm, float SpeedCmPerSecond, float MaxDelaySeconds);
};
