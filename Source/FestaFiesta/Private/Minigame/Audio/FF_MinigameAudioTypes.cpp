// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/Audio/FF_MinigameAudioTypes.h"

bool FFF_MinigameSoundEvent::IsValid(FString* OutFailureReason) const
{
	auto Fail = [OutFailureReason](const TCHAR* Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (!FMath::IsFinite(VolumeMultiplier) || VolumeMultiplier < 0.0f)
	{
		return Fail(TEXT("event volume must be finite and non-negative"));
	}
	if (!FMath::IsFinite(PitchMin) || !FMath::IsFinite(PitchMax)
		|| PitchMin < 0.125f || PitchMax > 4.0f || PitchMin > PitchMax)
	{
		return Fail(TEXT("event pitch range must satisfy 0.125 <= min <= max <= 4.0"));
	}
	if (!FMath::IsFinite(MinRetriggerIntervalSeconds)
		|| MinRetriggerIntervalSeconds < 0.0f)
	{
		return Fail(TEXT("event retrigger interval must be finite and non-negative"));
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}
	return true;
}

bool FFF_MinigameAudioProfile::IsValid(FString* OutFailureReason) const
{
	auto Fail = [OutFailureReason](const FString& Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (!FMath::IsFinite(MusicVolume) || MusicVolume < 0.0f
		|| !FMath::IsFinite(EffectsVolume) || EffectsVolume < 0.0f)
	{
		return Fail(TEXT("music and effects volumes must be finite and non-negative"));
	}
	if (!FMath::IsFinite(MusicFadeInSeconds) || MusicFadeInSeconds < 0.0f
		|| !FMath::IsFinite(MusicFadeOutSeconds) || MusicFadeOutSeconds < 0.0f)
	{
		return Fail(TEXT("music fade times must be finite and non-negative"));
	}

	for (const TPair<EFF_MinigameAudioEvent, FFF_MinigameSoundEvent>& Pair : EventSounds)
	{
		FString EventFailure;
		if (!Pair.Value.IsValid(&EventFailure))
		{
			return Fail(FString::Printf(
				TEXT("audio event %d is invalid: %s"),
				static_cast<int32>(Pair.Key),
				*EventFailure));
		}
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}
	return true;
}
