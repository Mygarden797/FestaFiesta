// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UDataTable;

/**
 * 3층 합성: 아키타입 delta + 동반유형 delta를 카테고리별로 더하고, 개체 변주
 * preference_strength_multiplier를 곱해 FFF_UtilityDestinationSelector::Choose가
 * 바로 쓸 수 있는 PreferenceVectorByCategory를 만든다. (utility_ai_spec.md 3.3절,
 * individual_variation_spec.md 5절)
 */
class FFF_PreferenceVectorBuilder
{
public:
	static TMap<FName, float> Build(
		FName ArchetypeId,
		FName CompanionType,
		float PreferenceStrengthMultiplier,
		const UDataTable* ArchetypeTable,
		const UDataTable* CompanionModifierTable);
};
