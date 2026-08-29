// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/ClawMachine/FF_ClawMachineTypes.h"

namespace FFClawMachineRules
{
	FESTAFIESTA_API bool IsNormalizedCoordinate(const FVector2D& Point);
	FESTAFIESTA_API float CalculateAlignment(
		const FVector2D& ClawPoint,
		const FFF_ClawMachinePrizeSnapshot& Prize,
		const FFF_ClawMachinePrizeTuning& Tuning,
		float MachineGripRadius);
	FESTAFIESTA_API float CalculateGripQuality(
		float Alignment,
		float GripPower,
		float PrizeDifficulty,
		float DeterministicVariance);
	FESTAFIESTA_API float CalculatePhysicalGripQuality(
		float Alignment,
		float GripPower,
		const FFF_ClawMachinePrizeTuning& Tuning,
		float DeterministicVariance);
	FESTAFIESTA_API int32 AddScoreSaturated(int32 CurrentScore, int32 Delta);
}
