#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "FF_FFZSTypes.h"
#include "FF_BuildLayoutPresetSave.generated.h"

/** A design stores geometry and facility definitions, never live IDs, money or NPCs. */
USTRUCT()
struct FFF_BuildLayoutEntry
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FFFZSFacilitySpec Spec;
	UPROPERTY(SaveGame) FVector RelativeCenter = FVector::ZeroVector;
	UPROPERTY(SaveGame) float WorldYawDegrees = 0.f;
};

USTRUCT()
struct FFF_BuildLayoutPreset
{
	GENERATED_BODY()
	UPROPERTY(SaveGame) FString Name;
	UPROPERTY(SaveGame) TArray<FFF_BuildLayoutEntry> Entries;
};

UCLASS()
class FESTAFIESTA_API UFF_BuildLayoutPresetSave : public USaveGame
{
	GENERATED_BODY()
public:
	static constexpr int32 CurrentVersion = 1;
	static constexpr int32 MaxPresets = 32;
	static constexpr int32 MaxFacilities = 128;
	UPROPERTY(SaveGame) int32 Version = CurrentVersion;
	UPROPERTY(SaveGame) TArray<FFF_BuildLayoutPreset> Presets;
};
