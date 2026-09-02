// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Audio/FF_AudioTypes.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FF_AudioBankData.generated.h"

// 어떤 순간에 어떤 소리를 낼지 모아둔 표.
//
// 애셋을 코드가 아니라 데이터로 두는 이유는, 소리는 들어보고 정해야 하는 값이기 때문이다.
// 볼륨이 조금 큰지 음높이가 어색한지는 화면 앞에서만 알 수 있고, 그때마다 빌드를 다시
// 하고 싶지 않다.
//
// 비어 있는 항목은 그냥 소리가 안 난다. 애셋이 아직 없는 단계에서도 게임이 돌아야 한다.
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_AudioBankData : public UDataAsset
{
	GENERATED_BODY()

public:
	// 순간별 효과음.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TMap<EFF_AudioEvent, FFF_AudioCue> Cues;

	// 상황별 배경음.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TMap<EFF_AudioAmbience, FFF_AudioAmbienceTrack> Ambiences;

	// 없으면 nullptr. 호출부가 조용히 넘어간다.
	const FFF_AudioCue* FindCue(EFF_AudioEvent Event) const
	{
		return Cues.Find(Event);
	}

	const FFF_AudioAmbienceTrack* FindAmbience(EFF_AudioAmbience Ambience) const
	{
		return Ambiences.Find(Ambience);
	}
};
