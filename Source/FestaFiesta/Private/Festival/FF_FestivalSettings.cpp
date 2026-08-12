// Copyright Epic Games, Inc. All Rights Reserved.

#include "Festival/FF_FestivalSettings.h"

const UFF_FestivalSettings* UFF_FestivalSettings::Get()
{
	return GetDefault<UFF_FestivalSettings>();
}
