// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/FF_AudioSettings.h"

const UFF_AudioSettings* UFF_AudioSettings::Get()
{
	return GetDefault<UFF_AudioSettings>();
}
