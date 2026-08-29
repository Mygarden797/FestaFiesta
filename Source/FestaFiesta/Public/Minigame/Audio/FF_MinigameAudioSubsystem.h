// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/Audio/FF_MinigameAudioTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "FF_MinigameAudioSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class USoundConcurrency;

/**
 * World-scoped audio boundary for all facility minigames.
 *
 * The coordinator owns session/music lifetime and presentation code emits stable
 * semantic events. Audio assets, MetaSounds and concurrency policies remain data.
 */
UCLASS()
class FESTAFIESTA_API UFF_MinigameAudioSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	/** Starts one profile and preloads its soft audio references before gameplay events arrive. */
	void BeginSession(const FGuid& SessionId, const FFF_MinigameAudioProfile& Profile);

	/** Emits a single event for the active session. Safe to call every time a new event serial is observed. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Audio")
	bool PlayEvent(const FGuid& SessionId, EFF_MinigameAudioEvent Event);

	/** Stops music and emits the exactly-once completed/cancelled sting. */
	void FinishSession(const FGuid& SessionId, bool bCompleted);

	/** Releases the profile. One-shot components remain auto-destroying. */
	void EndSession(const FGuid& SessionId);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Audio")
	bool IsSessionActive(const FGuid& SessionId) const;

private:
	void PreloadProfile();
	void StartMusic();
	void StartMusicWatchdog();
	void StopMusicWatchdog();
	void CheckMusicPlayback();
	void StopMusic(bool bAllowFade);
	void HandleMusicFinished(UAudioComponent* FinishedComponent);
	void DestroyMusicComponent(UAudioComponent* Component, bool bStopFirst);
	bool PlayBuiltInFallback(
		EFF_MinigameAudioEvent Event,
		float VolumeMultiplier,
		float PitchMultiplier);

	FGuid ActiveSessionId;
	FFF_MinigameAudioProfile ActiveProfile;
	bool bSessionFinished = false;
	bool bMusicStopRequested = false;
	FTimerHandle MusicWatchdogTimer;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedMusic;

	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> LoadedMusicConcurrency;

	UPROPERTY(Transient)
	TMap<EFF_MinigameAudioEvent, TObjectPtr<USoundBase>> LoadedEventSounds;

	UPROPERTY(Transient)
	TMap<EFF_MinigameAudioEvent, TObjectPtr<USoundConcurrency>> LoadedEventConcurrency;

	TMap<EFF_MinigameAudioEvent, double> LastPlaybackTimeSeconds;
};
