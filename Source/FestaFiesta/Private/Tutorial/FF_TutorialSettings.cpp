// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_TutorialSettings.h"

const UFF_TutorialSettings* UFF_TutorialSettings::Get()
{
	return GetDefault<UFF_TutorialSettings>();
}
