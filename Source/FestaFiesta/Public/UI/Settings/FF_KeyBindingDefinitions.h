// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * Player-remappable input domains.
 *
 * Camera and Character mappings may intentionally reuse a key because their
 * Enhanced Input contexts are mutually exclusive. System mappings are always
 * available and therefore conflict with every other domain in the settings UI.
 */
enum class EFF_KeyBindingContext : uint8
{
	System,
	Camera,
	Character
};

/** Immutable project-authored metadata for one player-mappable row. */
struct FESTAFIESTA_API FFF_KeyBindingDefinition
{
	FName MappingName;
	FText ActionLabel;
	FKey DefaultKey;
	/** Input Action object name used to locate the corresponding mapping in the source IMC. */
	FName SourceActionName;
	EFF_KeyBindingContext Context = EFF_KeyBindingContext::System;
};

/**
 * Single source of truth shared by the settings UI and the runtime input bridge.
 * Keeping these definitions in code avoids mutating team-owned IMC assets merely
 * to attach Player Mappable metadata.
 */
namespace FFKeyBindingDefinitions
{
	FESTAFIESTA_API const TArray<FFF_KeyBindingDefinition>& GetAll();
	FESTAFIESTA_API const FFF_KeyBindingDefinition* Find(FName MappingName);
	FESTAFIESTA_API bool IsSystemMapping(FName MappingName);
	FESTAFIESTA_API FText GetContextLabel(EFF_KeyBindingContext Context);
}

