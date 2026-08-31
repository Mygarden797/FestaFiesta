// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/Audio/FF_MinigameAudioSubsystem.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundWaveProcedural.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFMinigameAudio, Log, All);

namespace FFMinigameAudio
{
	constexpr uint32 FallbackSampleRate = 24000;
	constexpr float MusicWatchdogIntervalSeconds = 0.25f;

	struct FFallbackToneSpec
	{
		float StartFrequencyHz = 440.0f;
		float EndFrequencyHz = 440.0f;
		float DurationSeconds = 0.12f;
		float HarmonicAmount = 0.12f;
		float NoiseAmount = 0.0f;
	};

	FFallbackToneSpec GetToneSpec(const EFF_MinigameAudioEvent Event)
	{
		switch (Event)
		{
		case EFF_MinigameAudioEvent::SessionStarted: return { 440.0f, 760.0f, 0.20f, 0.16f, 0.0f };
		case EFF_MinigameAudioEvent::SessionCompleted: return { 520.0f, 1040.0f, 0.42f, 0.22f, 0.0f };
		case EFF_MinigameAudioEvent::SessionCancelled: return { 360.0f, 180.0f, 0.20f, 0.08f, 0.0f };
		case EFF_MinigameAudioEvent::TargetSpawned: return { 620.0f, 860.0f, 0.08f, 0.10f, 0.0f };
		case EFF_MinigameAudioEvent::ShotFired: return { 260.0f, 120.0f, 0.10f, 0.08f, 0.18f };
		case EFF_MinigameAudioEvent::TargetHit: return { 680.0f, 820.0f, 0.12f, 0.18f, 0.0f };
		case EFF_MinigameAudioEvent::TargetPrecision: return { 820.0f, 1120.0f, 0.16f, 0.22f, 0.0f };
		case EFF_MinigameAudioEvent::TargetBullseye: return { 980.0f, 1480.0f, 0.21f, 0.28f, 0.0f };
		case EFF_MinigameAudioEvent::TargetMissed: return { 210.0f, 150.0f, 0.13f, 0.05f, 0.10f };
		case EFF_MinigameAudioEvent::TargetEscaped: return { 420.0f, 260.0f, 0.16f, 0.06f, 0.0f };
		case EFF_MinigameAudioEvent::BonusTargetHit: return { 1100.0f, 1760.0f, 0.26f, 0.30f, 0.0f };
		case EFF_MinigameAudioEvent::SurfObstacleHit: return { 150.0f, 90.0f, 0.20f, 0.08f, 0.34f };
		case EFF_MinigameAudioEvent::SurfObstacleSmashed: return { 180.0f, 460.0f, 0.22f, 0.10f, 0.26f };
		case EFF_MinigameAudioEvent::SurfCoinCollected: return { 900.0f, 1320.0f, 0.13f, 0.22f, 0.0f };
		case EFF_MinigameAudioEvent::SurfHeartCollected: return { 520.0f, 720.0f, 0.20f, 0.24f, 0.0f };
		case EFF_MinigameAudioEvent::SurfBoostCollected: return { 620.0f, 1180.0f, 0.18f, 0.20f, 0.0f };
		case EFF_MinigameAudioEvent::SurfBoostActivated: return { 180.0f, 880.0f, 0.30f, 0.18f, 0.08f };
		case EFF_MinigameAudioEvent::ClawDropStarted: return { 420.0f, 250.0f, 0.28f, 0.10f, 0.04f };
		case EFF_MinigameAudioEvent::ClawClosing: return { 240.0f, 150.0f, 0.16f, 0.14f, 0.10f };
		case EFF_MinigameAudioEvent::ClawRelease: return { 360.0f, 520.0f, 0.18f, 0.14f, 0.03f };
		case EFF_MinigameAudioEvent::ClawPrizeCaptured: return { 660.0f, 1040.0f, 0.28f, 0.24f, 0.0f };
		case EFF_MinigameAudioEvent::ClawPerfectCapture: return { 880.0f, 1560.0f, 0.36f, 0.30f, 0.0f };
		case EFF_MinigameAudioEvent::ClawPrizeMissed: return { 300.0f, 180.0f, 0.22f, 0.08f, 0.0f };
		default: return {};
		}
	}

}

void UFF_MinigameAudioSubsystem::Deinitialize()
{
	StopMusic(false);
	ActiveSessionId.Invalidate();
	ActiveProfile = FFF_MinigameAudioProfile();
	LoadedEventSounds.Reset();
	LoadedEventConcurrency.Reset();
	LastPlaybackTimeSeconds.Reset();
	Super::Deinitialize();
}

bool UFF_MinigameAudioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::GamePreview;
}

void UFF_MinigameAudioSubsystem::BeginSession(
	const FGuid& SessionId,
	const FFF_MinigameAudioProfile& Profile)
{
	if (!SessionId.IsValid())
	{
		return;
	}
	if (ActiveSessionId == SessionId)
	{
		return;
	}

	StopMusic(false);
	ActiveSessionId = SessionId;
	ActiveProfile = Profile;
	bSessionFinished = false;
	bMusicStopRequested = false;
	LoadedEventSounds.Reset();
	LoadedEventConcurrency.Reset();
	LastPlaybackTimeSeconds.Reset();
	PreloadProfile();
	StartMusic();
	PlayEvent(SessionId, EFF_MinigameAudioEvent::SessionStarted);
}

bool UFF_MinigameAudioSubsystem::PlayEvent(
	const FGuid& SessionId,
	const EFF_MinigameAudioEvent Event)
{
	if (!IsSessionActive(SessionId) || !GetWorld())
	{
		return false;
	}

	FFF_MinigameSoundEvent DefaultSettings;
	const FFF_MinigameSoundEvent* Settings = ActiveProfile.EventSounds.Find(Event);
	if (!Settings)
	{
		Settings = &DefaultSettings;
	}
	if (Settings->VolumeMultiplier <= 0.0f || ActiveProfile.EffectsVolume <= 0.0f)
	{
		return false;
	}

	const double NowSeconds = FPlatformTime::Seconds();
	if (const double* LastTime = LastPlaybackTimeSeconds.Find(Event))
	{
		if (NowSeconds - *LastTime
			< static_cast<double>(Settings->MinRetriggerIntervalSeconds))
		{
			return false;
		}
	}

	const float Pitch = FMath::FRandRange(Settings->PitchMin, Settings->PitchMax);
	const float Volume = ActiveProfile.EffectsVolume * Settings->VolumeMultiplier;
	if (USoundBase* Sound = LoadedEventSounds.FindRef(Event))
	{
		UGameplayStatics::SpawnSound2D(
			GetWorld(),
			Sound,
			Volume,
			Pitch,
			0.0f,
			LoadedEventConcurrency.FindRef(Event),
			false,
			true);
		LastPlaybackTimeSeconds.Add(Event, NowSeconds);
		return true;
	}

	if (ActiveProfile.bEnableBuiltInFallbackTones
		&& Settings->bAllowBuiltInFallbackTone
		&& PlayBuiltInFallback(Event, Volume, Pitch))
	{
		LastPlaybackTimeSeconds.Add(Event, NowSeconds);
		return true;
	}
	return false;
}

void UFF_MinigameAudioSubsystem::FinishSession(
	const FGuid& SessionId,
	const bool bCompleted)
{
	if (!IsSessionActive(SessionId) || bSessionFinished)
	{
		return;
	}
	bSessionFinished = true;
	StopMusic(true);
	PlayEvent(
		SessionId,
		bCompleted
			? EFF_MinigameAudioEvent::SessionCompleted
			: EFF_MinigameAudioEvent::SessionCancelled);
}

void UFF_MinigameAudioSubsystem::EndSession(const FGuid& SessionId)
{
	if (!IsSessionActive(SessionId))
	{
		return;
	}
	StopMusic(false);
	ActiveSessionId.Invalidate();
	ActiveProfile = FFF_MinigameAudioProfile();
	bSessionFinished = false;
	LoadedEventSounds.Reset();
	LoadedEventConcurrency.Reset();
	LastPlaybackTimeSeconds.Reset();
}

bool UFF_MinigameAudioSubsystem::IsSessionActive(const FGuid& SessionId) const
{
	return SessionId.IsValid() && ActiveSessionId == SessionId;
}

void UFF_MinigameAudioSubsystem::PreloadProfile()
{
	LoadedMusic = ActiveProfile.Music.IsNull()
		? nullptr
		: ActiveProfile.Music.LoadSynchronous();
	LoadedMusicConcurrency = ActiveProfile.MusicConcurrency.IsNull()
		? nullptr
		: ActiveProfile.MusicConcurrency.LoadSynchronous();

	for (const TPair<EFF_MinigameAudioEvent, FFF_MinigameSoundEvent>& Pair
		: ActiveProfile.EventSounds)
	{
		if (!Pair.Value.Sound.IsNull())
		{
			if (USoundBase* Sound = Pair.Value.Sound.LoadSynchronous())
			{
				LoadedEventSounds.Add(Pair.Key, Sound);
			}
		}
		if (!Pair.Value.Concurrency.IsNull())
		{
			if (USoundConcurrency* Concurrency = Pair.Value.Concurrency.LoadSynchronous())
			{
				LoadedEventConcurrency.Add(Pair.Key, Concurrency);
			}
		}
	}
}

void UFF_MinigameAudioSubsystem::StartMusic()
{
	if (!LoadedMusic || ActiveProfile.MusicVolume <= 0.0f || !GetWorld())
	{
		return;
	}

	MusicComponent = UGameplayStatics::CreateSound2D(
		GetWorld(),
		LoadedMusic,
		ActiveProfile.MusicVolume,
		1.0f,
		0.0f,
		LoadedMusicConcurrency,
		false,
		false);
	if (!MusicComponent)
	{
		return;
	}
	bMusicStopRequested = false;
	MusicComponent->OnAudioFinishedNative.AddUObject(
		this,
		&ThisClass::HandleMusicFinished);
	if (ActiveProfile.MusicFadeInSeconds > 0.0f)
	{
		MusicComponent->FadeIn(
			ActiveProfile.MusicFadeInSeconds,
			1.0f);
	}
	else
	{
		MusicComponent->Play();
	}
	StartMusicWatchdog();

	UE_LOG(
		LogFFMinigameAudio,
		Log,
		TEXT("Minigame music started. Session=%s Sound=%s LoopDuringSession=%s"),
		*ActiveSessionId.ToString(),
		*GetNameSafe(LoadedMusic),
		ActiveProfile.bLoopMusicDuringSession ? TEXT("true") : TEXT("false"));
}

void UFF_MinigameAudioSubsystem::StartMusicWatchdog()
{
	UWorld* World = GetWorld();
	if (!World || !ActiveProfile.bLoopMusicDuringSession)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		MusicWatchdogTimer,
		this,
		&ThisClass::CheckMusicPlayback,
		FFMinigameAudio::MusicWatchdogIntervalSeconds,
		true);
}

void UFF_MinigameAudioSubsystem::StopMusicWatchdog()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MusicWatchdogTimer);
	}
}

void UFF_MinigameAudioSubsystem::CheckMusicPlayback()
{
	const bool bShouldBePlaying = ActiveSessionId.IsValid()
		&& !bSessionFinished
		&& !bMusicStopRequested
		&& ActiveProfile.bLoopMusicDuringSession
		&& IsValid(LoadedMusic)
		&& ActiveProfile.MusicVolume > 0.0f;
	if (!bShouldBePlaying)
	{
		StopMusicWatchdog();
		return;
	}

	if (!IsValid(MusicComponent))
	{
		MusicComponent = nullptr;
		UE_LOG(
			LogFFMinigameAudio,
			Warning,
			TEXT("Minigame music component was lost during an active session; recreating it. Session=%s Sound=%s"),
			*ActiveSessionId.ToString(),
			*GetNameSafe(LoadedMusic));
		StartMusic();
		return;
	}

	if (!MusicComponent->IsPlaying())
	{
		UE_LOG(
			LogFFMinigameAudio,
			Warning,
			TEXT("Minigame music stopped unexpectedly; restarting from the beginning. Session=%s Sound=%s"),
			*ActiveSessionId.ToString(),
			*GetNameSafe(LoadedMusic));
		MusicComponent->Play(0.0f);
	}
}

void UFF_MinigameAudioSubsystem::StopMusic(const bool bAllowFade)
{
	StopMusicWatchdog();

	if (!IsValid(MusicComponent))
	{
		MusicComponent = nullptr;
		LoadedMusic = nullptr;
		LoadedMusicConcurrency = nullptr;
		bMusicStopRequested = false;
		return;
	}

	bMusicStopRequested = true;
	if (bAllowFade
		&& ActiveProfile.MusicFadeOutSeconds > 0.0f
		&& MusicComponent->IsPlaying())
	{
		MusicComponent->FadeOut(ActiveProfile.MusicFadeOutSeconds, 0.0f);
		return;
	}

	UAudioComponent* ComponentToDestroy = MusicComponent;
	MusicComponent = nullptr;
	DestroyMusicComponent(ComponentToDestroy, true);
	LoadedMusic = nullptr;
	LoadedMusicConcurrency = nullptr;
	bMusicStopRequested = false;
}

void UFF_MinigameAudioSubsystem::HandleMusicFinished(UAudioComponent* FinishedComponent)
{
	if (!IsValid(FinishedComponent) || FinishedComponent != MusicComponent)
	{
		return;
	}

	const bool bShouldContinue = ActiveSessionId.IsValid()
		&& !bSessionFinished
		&& !bMusicStopRequested
		&& ActiveProfile.bLoopMusicDuringSession;
	if (bShouldContinue)
	{
		UE_LOG(
			LogFFMinigameAudio,
			Verbose,
			TEXT("Minigame music reached its end; restarting for the active session. Session=%s Sound=%s"),
			*ActiveSessionId.ToString(),
			*GetNameSafe(LoadedMusic));
		FinishedComponent->Play(0.0f);
		return;
	}

	StopMusicWatchdog();
	MusicComponent = nullptr;
	DestroyMusicComponent(FinishedComponent, false);
	LoadedMusic = nullptr;
	LoadedMusicConcurrency = nullptr;
	bMusicStopRequested = false;
}

void UFF_MinigameAudioSubsystem::DestroyMusicComponent(
	UAudioComponent* Component,
	const bool bStopFirst)
{
	if (!IsValid(Component))
	{
		return;
	}

	Component->OnAudioFinishedNative.RemoveAll(this);
	if (bStopFirst && Component->IsPlaying())
	{
		Component->Stop();
	}
	Component->DestroyComponent();
}

bool UFF_MinigameAudioSubsystem::PlayBuiltInFallback(
	const EFF_MinigameAudioEvent Event,
	const float VolumeMultiplier,
	const float PitchMultiplier)
{
	if (!GetWorld())
	{
		return false;
	}

	const FFMinigameAudio::FFallbackToneSpec Spec = FFMinigameAudio::GetToneSpec(Event);
	const int32 SampleCount = FMath::Max(
		1,
		FMath::RoundToInt(Spec.DurationSeconds * FFMinigameAudio::FallbackSampleRate));
	TArray<int16> Samples;
	Samples.SetNumUninitialized(SampleCount);

	FRandomStream NoiseStream(static_cast<int32>(Event) * 7919 + 17);
	double Phase = 0.0;
	for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
	{
		const float Alpha = static_cast<float>(SampleIndex)
			/ static_cast<float>(FMath::Max(1, SampleCount - 1));
		const float Frequency = FMath::Lerp(Spec.StartFrequencyHz, Spec.EndFrequencyHz, Alpha);
		Phase += 2.0 * UE_PI * static_cast<double>(Frequency)
			/ static_cast<double>(FFMinigameAudio::FallbackSampleRate);
		const float Attack = FMath::Clamp(Alpha / 0.08f, 0.0f, 1.0f);
		const float Release = FMath::Clamp((1.0f - Alpha) / 0.28f, 0.0f, 1.0f);
		const float Envelope = Attack * Release;
		const float Fundamental = FMath::Sin(static_cast<float>(Phase));
		const float Harmonic = FMath::Sin(static_cast<float>(Phase * 2.0));
		const float Noise = NoiseStream.FRandRange(-1.0f, 1.0f);
		const float Mixed = Fundamental
			+ Harmonic * Spec.HarmonicAmount
			+ Noise * Spec.NoiseAmount;
		Samples[SampleIndex] = static_cast<int16>(FMath::Clamp(
			Mixed * Envelope * 10500.0f,
			-32767.0f,
			32767.0f));
	}

	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	if (!Wave)
	{
		return false;
	}
	Wave->NumChannels = 1;
	Wave->SetSampleRate(FFMinigameAudio::FallbackSampleRate);
	Wave->SetNumFrames(SampleCount);
	Wave->Duration = Spec.DurationSeconds;
	Wave->SoundGroup = SOUNDGROUP_UI;
	Wave->bLooping = false;
	Wave->QueueAudio(
		reinterpret_cast<const uint8*>(Samples.GetData()),
		Samples.Num() * sizeof(int16));

	return UGameplayStatics::SpawnSound2D(
		GetWorld(),
		Wave,
		VolumeMultiplier,
		PitchMultiplier,
		0.0f,
		nullptr,
		false,
		true) != nullptr;
}
