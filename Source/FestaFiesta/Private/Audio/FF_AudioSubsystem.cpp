// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/FF_AudioSubsystem.h"

#include "Audio/FF_AudioBankData.h"
#include "Audio/FF_AudioSettings.h"
#include "Camera/FF_FestivalGameMode.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UI/Settings/FF_RuntimeSettingsSubsystem.h"

DEFINE_LOG_CATEGORY(LogFestaFiestaAudio);

namespace
{
	// 크로스페이드가 끝난 뒤 이전 컴포넌트를 정리하기까지의 여유.
	// 페이드 시간에 딱 맞춰 지우면 마지막 순간이 잘려서 툭 끊긴다.
	constexpr float FadeCleanupPaddingSeconds = 0.25f;
}

bool UFF_AudioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UFF_AudioSubsystem* UFF_AudioSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World ? World->GetSubsystem<UFF_AudioSubsystem>() : nullptr;
}

void UFF_AudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RegisterDebugConsoleCommands();
	BindRuntimeSettings();
}

void UFF_AudioSubsystem::Deinitialize()
{
	UnbindRuntimeSettings();
	UnregisterDebugConsoleCommands();
	StopAmbience();
	Super::Deinitialize();
}

const UFF_AudioBankData* UFF_AudioSubsystem::ResolveBank() const
{
	// 1. 코드에서 직접 꽂은 것.
	if (const UFF_AudioBankData* Direct = BankOverride.LoadSynchronous())
	{
		return Direct;
	}

	// 2. 이 레벨의 GameMode 지정. 맵마다 분위기가 다를 수 있다.
	const UWorld* World = GetWorld();
	if (const AFF_FestivalGameMode* GameMode =
		World ? Cast<AFF_FestivalGameMode>(World->GetAuthGameMode()) : nullptr)
	{
		if (const UFF_AudioBankData* FromLevel = GameMode->AudioBankOverride.LoadSynchronous())
		{
			return FromLevel;
		}
	}

	// 3. 프로젝트 기본값.
	if (const UFF_AudioSettings* Settings = UFF_AudioSettings::Get())
	{
		if (const UFF_AudioBankData* FromSettings = Settings->DefaultAudioBank.LoadSynchronous())
		{
			return FromSettings;
		}
	}

	return nullptr;
}

USoundBase* UFF_AudioSubsystem::PickSound(const FFF_AudioCue& Cue) const
{
	// 변형이 있으면 그중에서 고른다. 기본 소리도 후보에 넣어 골고루 나오게 한다.
	if (Cue.Variations.Num() > 0)
	{
		const int32 Total = Cue.Variations.Num() + (Cue.Sound.IsNull() ? 0 : 1);
		const int32 Index = FMath::RandRange(0, Total - 1);

		if (Index < Cue.Variations.Num())
		{
			return Cue.Variations[Index].LoadSynchronous();
		}
	}
	return Cue.Sound.LoadSynchronous();
}

bool UFF_AudioSubsystem::CanPlay(const EFF_AudioEvent Event, const FFF_AudioCue*& OutCue)
{
	OutCue = nullptr;

	if (!bAudioEnabled)
	{
		return false;
	}

	const UFF_AudioBankData* Bank = ResolveBank();
	if (!Bank)
	{
		++MissingCues;
		return false;
	}

	const FFF_AudioCue* Cue = Bank->FindCue(Event);
	if (!Cue || !Cue->IsValid())
	{
		// 뱅크에 없는 이벤트다. 아직 소리를 안 붙인 것이므로 경고까지 할 일은 아니다.
		++MissingCues;
		return false;
	}

	// 같은 소리가 겹쳐 울리면 찢어지는 소리가 난다.
	// 불꽃이 한 번에 여러 발 터지거나 NPC 여럿이 동시에 줄을 설 때가 그렇다.
	if (Cue->MinRetriggerSeconds > 0.f)
	{
		const UWorld* World = GetWorld();
		const double Now = World ? World->GetTimeSeconds() : 0.0;

		if (const double* Last = LastPlayTimeSeconds.Find(Event))
		{
			if (Now - *Last < Cue->MinRetriggerSeconds)
			{
				++SuppressedCues;
				return false;
			}
		}
		LastPlayTimeSeconds.Add(Event, Now);
	}

	OutCue = Cue;
	return true;
}

bool UFF_AudioSubsystem::PlayEvent2D(const EFF_AudioEvent Event)
{
	const FFF_AudioCue* Cue = nullptr;
	if (!CanPlay(Event, Cue))
	{
		return false;
	}

	USoundBase* Sound = PickSound(*Cue);
	if (!Sound)
	{
		++MissingCues;
		return false;
	}

	UGameplayStatics::PlaySound2D(
		this, Sound,
		Cue->VolumeMultiplier * GetChannelVolume(ResolveChannel(Event, *Cue)),
		FMath::FRandRange(Cue->PitchMin, Cue->PitchMax),
		0.f, nullptr, nullptr, true);

	++PlayedCues;
	return true;
}

bool UFF_AudioSubsystem::PlayEventAtLocation(const EFF_AudioEvent Event, const FVector& WorldLocation)
{
	const FFF_AudioCue* Cue = nullptr;
	if (!CanPlay(Event, Cue))
	{
		return false;
	}

	USoundBase* Sound = PickSound(*Cue);
	if (!Sound)
	{
		++MissingCues;
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UGameplayStatics::PlaySoundAtLocation(
		World, Sound, WorldLocation, FRotator::ZeroRotator,
		Cue->VolumeMultiplier * GetChannelVolume(ResolveChannel(Event, *Cue)),
		FMath::FRandRange(Cue->PitchMin, Cue->PitchMax),
		0.f, nullptr,
		Cue->Concurrency.LoadSynchronous());

	++PlayedCues;
	return true;
}

void UFF_AudioSubsystem::SetAmbience(const EFF_AudioAmbience Ambience)
{
	// 같은 것을 다시 요청하면 무시한다.
	//
	// 이게 없으면 매 틱 상태를 확인해서 호출하는 코드가 붙었을 때 소리가 계속 처음부터
	// 다시 재생된다. 호출부가 조심하게 만드는 것보다 여기서 막는 편이 안전하다.
	if (CurrentAmbience == Ambience)
	{
		return;
	}

	if (!bAudioEnabled)
	{
		CurrentAmbience = Ambience;
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const UFF_AudioBankData* Bank = ResolveBank();
	const FFF_AudioAmbienceTrack* Track = Bank ? Bank->FindAmbience(Ambience) : nullptr;

	// 이전 곡을 페이드아웃시킨다. 즉시 끊으면 툭 하고 잘린다.
	float FadeOut = 1.f;
	if (AmbienceComponent)
	{
		// 페이드 중이던 게 또 있으면 그건 지금 정리한다. 두 개까지만 겹치게 둔다.
		if (FadingAmbienceComponent)
		{
			FadingAmbienceComponent->Stop();
			FadingAmbienceComponent = nullptr;
		}

		if (const FFF_AudioAmbienceTrack* Previous = Bank ? Bank->FindAmbience(CurrentAmbience) : nullptr)
		{
			FadeOut = FMath::Max(0.f, Previous->FadeOutSeconds);
		}

		AmbienceComponent->FadeOut(FadeOut, 0.f);
		FadingAmbienceComponent = AmbienceComponent;
		AmbienceComponent = nullptr;

		// 페이드가 끝나면 참조를 놓는다. 안 그러면 컴포넌트가 계속 살아 있다.
		World->GetTimerManager().SetTimer(
			FadingCleanupTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				FadingAmbienceComponent = nullptr;
			}),
			FadeOut + FadeCleanupPaddingSeconds, false);
	}

	CurrentAmbience = Ambience;

	if (!Track || !Track->IsValid())
	{
		// 이 상황에 붙은 곡이 없다. 이전 곡만 끄고 끝낸다.
		return;
	}

	USoundBase* Sound = Track->Sound.LoadSynchronous();
	if (!Sound)
	{
		return;
	}

	/* 곡 볼륨과 배경음 비율은 컴포넌트 볼륨에 두고, 페이드는 1 까지만 올린다.

	   예전에는 컴포넌트 볼륨과 페이드 목표에 곡 볼륨을 둘 다 넣어서 곡 볼륨이 제곱으로 먹었다.
	   0.5 로 두면 0.25 로 들렸다. 볼륨을 한 곳에 모아야 설정이 바뀌었을 때 그 값만 다시 걸 수 있다.
	*/
	AmbienceTrackVolume = Track->VolumeMultiplier;
	AmbienceComponent = UGameplayStatics::SpawnSound2D(
		World, Sound, AmbienceTrackVolume * GetChannelVolume(EFF_AudioChannel::Music),
		1.f, 0.f, nullptr, true, false);

	if (AmbienceComponent)
	{
		AmbienceComponent->FadeIn(FMath::Max(0.f, Track->FadeInSeconds), 1.f);
		AmbienceComponent->OnAudioFinishedNative.AddUObject(this, &UFF_AudioSubsystem::HandleAmbienceFinished);
	}
}

void UFF_AudioSubsystem::HandleAmbienceFinished(UAudioComponent* Component)
{
	/* 지금 곡이 끝난 것만 다시 튼다.

	   곡을 갈아탈 때 이전 곡은 페이드아웃 칸으로 옮겨지고, 끌 때는 참조를 먼저 비운다.
	   그 곡들도 끝났다고 알려오는데, 다시 틀면 꺼진 곡이 되살아난다.
	*/
	if (!Component || Component != AmbienceComponent || !bAudioEnabled)
	{
		return;
	}

	// 엔딩곡은 한 번만 흐른다. 연출이 끝나기 전에 곡이 끝나도 되풀이하면 끝났다는 느낌이 사라진다.
	if (CurrentAmbience == EFF_AudioAmbience::FinaleEnding)
	{
		return;
	}

	Component->Play(0.f);
}

bool UFF_AudioSubsystem::HasAmbienceTrack(const EFF_AudioAmbience Ambience) const
{
	const UFF_AudioBankData* Bank = ResolveBank();
	const FFF_AudioAmbienceTrack* Track = Bank ? Bank->FindAmbience(Ambience) : nullptr;
	return Track && Track->IsValid();
}

float UFF_AudioSubsystem::GetChannelVolume(const EFF_AudioChannel Channel) const
{
	// 설정 시스템이 없는 맵(테스트 등)에서는 비율을 걸지 않는다. 소리가 안 나는 것보다 낫다.
	const UFF_RuntimeSettingsSubsystem* Runtime = UFF_RuntimeSettingsSubsystem::Get(this);
	if (!Runtime)
	{
		return 1.f;
	}

	const FFF_AudioVolumes Volumes = Runtime->GetAudioVolumes();
	switch (Channel)
	{
	case EFF_AudioChannel::Music:     return Volumes.Music;
	case EFF_AudioChannel::Fireworks: return Volumes.Fireworks;
	case EFF_AudioChannel::Ui:        return Volumes.Ui;
	default:                          return Volumes.Effects;
	}
}

EFF_AudioChannel UFF_AudioSubsystem::ResolveChannel(const EFF_AudioEvent Event, const FFF_AudioCue& Cue)
{
	if (Cue.Channel != EFF_AudioChannel::Effects)
	{
		return Cue.Channel;
	}

	switch (Event)
	{
	case EFF_AudioEvent::FinaleBurst:
	case EFF_AudioEvent::FinaleShowStarted:
	case EFF_AudioEvent::FinaleShowEnded:
		return EFF_AudioChannel::Fireworks;

	case EFF_AudioEvent::UiClick:
	case EFF_AudioEvent::UiHover:
	case EFF_AudioEvent::UiConfirm:
	case EFF_AudioEvent::UiCancel:
	case EFF_AudioEvent::UiError:
		return EFF_AudioChannel::Ui;

	default:
		return EFF_AudioChannel::Effects;
	}
}

void UFF_AudioSubsystem::BindRuntimeSettings()
{
	if (UFF_RuntimeSettingsSubsystem* Runtime = UFF_RuntimeSettingsSubsystem::Get(this))
	{
		Runtime->OnSettingsChanged.AddUniqueDynamic(this, &UFF_AudioSubsystem::HandleRuntimeSettingsChanged);
	}
}

void UFF_AudioSubsystem::UnbindRuntimeSettings()
{
	// 설정 시스템은 게임 인스턴스라 월드보다 오래 산다. 안 끊으면 사라진 월드의 이 객체를 계속 부른다.
	if (UFF_RuntimeSettingsSubsystem* Runtime = UFF_RuntimeSettingsSubsystem::Get(this))
	{
		Runtime->OnSettingsChanged.RemoveDynamic(this, &UFF_AudioSubsystem::HandleRuntimeSettingsChanged);
	}
}

void UFF_AudioSubsystem::HandleRuntimeSettingsChanged()
{
	if (AmbienceComponent)
	{
		AmbienceComponent->SetVolumeMultiplier(AmbienceTrackVolume * GetChannelVolume(EFF_AudioChannel::Music));
	}
}

void UFF_AudioSubsystem::StopAmbience()
{
	if (AmbienceComponent)
	{
		// 참조를 먼저 비운다. Stop 이 곧바로 "끝났다" 를 알리는데, 그때 지금 곡으로 보이면 다시 튼다.
		UAudioComponent* Stopping = AmbienceComponent;
		AmbienceComponent = nullptr;
		Stopping->Stop();
	}
	if (FadingAmbienceComponent)
	{
		FadingAmbienceComponent->Stop();
		FadingAmbienceComponent = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FadingCleanupTimer);
	}

	CurrentAmbience = EFF_AudioAmbience::None;
}

void UFF_AudioSubsystem::SetAudioEnabled(const bool bEnabled)
{
	bAudioEnabled = bEnabled;

	if (!bEnabled)
	{
		// 배경음은 끊되 현재 상황은 기억해둔다. 다시 켰을 때 이어갈 수 있게.
		const EFF_AudioAmbience Remembered = CurrentAmbience;
		StopAmbience();
		CurrentAmbience = Remembered;
		return;
	}

	// 다시 켜면 기억해둔 배경음을 되살린다.
	const EFF_AudioAmbience Resume = CurrentAmbience;
	CurrentAmbience = EFF_AudioAmbience::None;
	SetAmbience(Resume);
}

void UFF_AudioSubsystem::RegisterDebugConsoleCommands()
{
	// 콘솔 이름은 프로세스 전역인데 이 서브시스템은 월드마다 생긴다.
	// 이름 충돌·해제 규칙은 FFF_DebugConsoleCommands 가 맡는다.
	DebugCommands.Add(
		TEXT("FF.Audio.Stat"),
		TEXT("사운드 재생 상태와 사용 중인 뱅크를 출력한다."),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			const UFF_AudioBankData* Bank = ResolveBank();
			UE_LOG(LogFestaFiestaAudio, Display,
				TEXT("[사운드] 켜짐=%s 재생=%d 누락=%d 억제=%d 배경음=%d"),
				bAudioEnabled ? TEXT("예") : TEXT("아니오"),
				PlayedCues, MissingCues, SuppressedCues, static_cast<int32>(CurrentAmbience));
			UE_LOG(LogFestaFiestaAudio, Display,
				TEXT("  뱅크: %s / 등록된 효과음 %d개 / 배경음 %d개"),
				Bank ? *Bank->GetName() : TEXT("없음"),
				Bank ? Bank->Cues.Num() : 0,
				Bank ? Bank->Ambiences.Num() : 0);
		}));

	DebugCommands.Add(
		TEXT("FF.Audio.Play"),
		TEXT("이벤트 번호로 소리를 재생한다. 사용법: FF.Audio.Play 10"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() == 0)
			{
				UE_LOG(LogFestaFiestaAudio, Warning,
					TEXT("[사운드] 이벤트 번호가 필요하다. EFF_AudioEvent 값을 넣을 것."));
				return;
			}
			const int32 Value = FCString::Atoi(*Args[0]);
			const bool bPlayed = PlayEvent2D(static_cast<EFF_AudioEvent>(Value));
			UE_LOG(LogFestaFiestaAudio, Display,
				TEXT("[사운드] 이벤트 %d 재생 %s"), Value, bPlayed ? TEXT("성공") : TEXT("실패"));
		}));

	DebugCommands.Add(
		TEXT("FF.Audio.Mute"),
		TEXT("소리를 끄거나 켠다. 사용법: FF.Audio.Mute 1"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			const bool bMute = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0;
			SetAudioEnabled(!bMute);
			UE_LOG(LogFestaFiestaAudio, Display,
				TEXT("[사운드] %s"), bMute ? TEXT("음소거") : TEXT("켜짐"));
		}));
}

void UFF_AudioSubsystem::UnregisterDebugConsoleCommands()
{
	// 내가 건 것만 지운다. 규칙은 FFF_DebugConsoleCommands 주석 참고.
	DebugCommands.UnregisterAll();
}
