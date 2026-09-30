// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/FF_AudioSettings.h"

const UFF_AudioSettings* UFF_AudioSettings::Get()
{
	return GetDefault<UFF_AudioSettings>();
}

float UFF_AudioSettings::ComputeSoundTravelDelay(
	const float DistanceCm, const float SpeedCmPerSecond, const float MaxDelaySeconds)
{
	if (SpeedCmPerSecond <= 0.f || DistanceCm <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp(DistanceCm / SpeedCmPerSecond, 0.f, FMath::Max(0.f, MaxDelaySeconds));
}
