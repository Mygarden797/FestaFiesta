// FF_RuntimeSettingsSubsystem.cpp
// 역할: 화면·음량·입력 등 사용자 설정을 읽고 저장하며 게임에 적용한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_RuntimeSettingsSubsystem.h"

#include "AudioDevice.h"
#include "AudioDeviceHandle.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ConfigCacheIni.h"
#include "UI/FF_UILog.h"

namespace
{
	const TCHAR* RuntimeSettingsSection = TEXT("/Script/FestaFiesta.RuntimeSettings");
	const TCHAR* MasterVolumeKey = TEXT("MasterVolume");
	const TCHAR* MusicVolumeKey = TEXT("MusicVolume");
	const TCHAR* EffectsVolumeKey = TEXT("EffectsVolume");
	const TCHAR* FireworksVolumeKey = TEXT("FireworksVolume");
	const TCHAR* UiVolumeKey = TEXT("UiVolume");
	const TCHAR* ZoomToCursorKey = TEXT("bZoomToCursor");
	const TCHAR* EdgeScrollKey = TEXT("bEdgeScrollEnabled");
}

const UFF_RuntimeSettingsDefaults* UFF_RuntimeSettingsDefaults::Get()
{
	return GetDefault<UFF_RuntimeSettingsDefaults>();
}

FFF_AudioVolumes UFF_RuntimeSettingsDefaults::MakeDefaultVolumes() const
{
	FFF_AudioVolumes Out;
	Out.Master = DefaultMasterVolume;
	Out.Music = DefaultMusicVolume;
	Out.Effects = DefaultEffectsVolume;
	Out.Fireworks = DefaultFireworksVolume;
	Out.Ui = DefaultUiVolume;
	return Out.Clamped();
}

void UFF_RuntimeSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadFromUserSettings();
	ApplyAudioSettings();

	/* 마스터 음량은 오디오 디바이스에 거는 값인데, 엔진이 LoadMap 에서 이 값을 1.0 으로 되돌린다.
	   그래서 메인 메뉴에서 줄여 두고 축제 레벨에 들어가면 원래 크기로 튀었다(2026-09-30 제보).
	   지금까지는 일시정지 허브를 열 때만 다시 걸어서, ESC 를 누르기 전까지 큰 소리가 났다.
	   새 월드의 액터가 준비된 시점은 그 리셋 이후이고 BeginPlay 이전이라, 첫 소리가 나기 전에 맞출 수 있다.
	   PIE 첫 월드처럼 LoadMap 을 거치지 않는 경우도 이 신호는 온다.
	*/
	WorldActorsInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(
		this, &UFF_RuntimeSettingsSubsystem::HandleWorldActorsInitialized);
}

void UFF_RuntimeSettingsSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldActorsInitializedHandle);
	WorldActorsInitializedHandle.Reset();
	Super::Deinitialize();
}

void UFF_RuntimeSettingsSubsystem::HandleWorldActorsInitialized(const FActorsInitializedParams& Params)
{
	// 에디터 미리보기 월드나 다른 PIE 인스턴스의 월드는 건드리지 않는다.
	UWorld* World = Params.World;
	if (!World || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	ApplyAudioSettingsToWorld(World);
}

UFF_RuntimeSettingsSubsystem* UFF_RuntimeSettingsSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::ReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UFF_RuntimeSettingsSubsystem>() : nullptr;
}

void UFF_RuntimeSettingsSubsystem::LoadFromUserSettings()
{
	const UFF_RuntimeSettingsDefaults* Defaults = UFF_RuntimeSettingsDefaults::Get();
	Volumes = Defaults ? Defaults->MakeDefaultVolumes() : FFF_AudioVolumes();
	bZoomToCursor = !Defaults || Defaults->bDefaultZoomToCursor;
	bEdgeScrollEnabled = !Defaults || Defaults->bDefaultEdgeScrollEnabled;

	if (GConfig)
	{
		GConfig->GetFloat(RuntimeSettingsSection, MasterVolumeKey, Volumes.Master, GGameUserSettingsIni);
		// 예전 ini 에는 갈래 키가 없다. 없으면 GetFloat 이 값을 건드리지 않아 위 기본값이 남는다.
		GConfig->GetFloat(RuntimeSettingsSection, MusicVolumeKey, Volumes.Music, GGameUserSettingsIni);
		GConfig->GetFloat(RuntimeSettingsSection, EffectsVolumeKey, Volumes.Effects, GGameUserSettingsIni);
		GConfig->GetFloat(RuntimeSettingsSection, FireworksVolumeKey, Volumes.Fireworks, GGameUserSettingsIni);
		GConfig->GetFloat(RuntimeSettingsSection, UiVolumeKey, Volumes.Ui, GGameUserSettingsIni);
		GConfig->GetBool(RuntimeSettingsSection, ZoomToCursorKey, bZoomToCursor, GGameUserSettingsIni);
		GConfig->GetBool(RuntimeSettingsSection, EdgeScrollKey, bEdgeScrollEnabled, GGameUserSettingsIni);
	}

	Volumes = Volumes.Clamped();
}

void UFF_RuntimeSettingsSubsystem::SaveToUserSettings() const
{
	if (!GConfig)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_RuntimeSettingsSubsystem: GConfig가 없어 개인 설정을 저장하지 못했다."));
		return;
	}

	GConfig->SetFloat(RuntimeSettingsSection, MasterVolumeKey, Volumes.Master, GGameUserSettingsIni);
	GConfig->SetFloat(RuntimeSettingsSection, MusicVolumeKey, Volumes.Music, GGameUserSettingsIni);
	GConfig->SetFloat(RuntimeSettingsSection, EffectsVolumeKey, Volumes.Effects, GGameUserSettingsIni);
	GConfig->SetFloat(RuntimeSettingsSection, FireworksVolumeKey, Volumes.Fireworks, GGameUserSettingsIni);
	GConfig->SetFloat(RuntimeSettingsSection, UiVolumeKey, Volumes.Ui, GGameUserSettingsIni);
	GConfig->SetBool(RuntimeSettingsSection, ZoomToCursorKey, bZoomToCursor, GGameUserSettingsIni);
	GConfig->SetBool(RuntimeSettingsSection, EdgeScrollKey, bEdgeScrollEnabled, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void UFF_RuntimeSettingsSubsystem::NotifyChanged()
{
	SaveToUserSettings();
	OnSettingsChanged.Broadcast();
}

void UFF_RuntimeSettingsSubsystem::SetMasterVolume(float NewValue)
{
	NewValue = FMath::Clamp(NewValue, 0.f, 1.f);
	if (FMath::IsNearlyEqual(Volumes.Master, NewValue))
	{
		return;
	}

	Volumes.Master = NewValue;
	ApplyAudioSettings();
	NotifyChanged();
}

void UFF_RuntimeSettingsSubsystem::SetAudioVolumes(const FFF_AudioVolumes& NewVolumes)
{
	const FFF_AudioVolumes Clamped = NewVolumes.Clamped();
	if (Volumes.Equals(Clamped))
	{
		return;
	}

	// 마스터만 엔진에 건다. 나머지는 소리를 재생하는 쪽(UFF_AudioSubsystem)이 방송을 받아 곱한다.
	Volumes = Clamped;
	ApplyAudioSettings();
	NotifyChanged();
}

void UFF_RuntimeSettingsSubsystem::SetZoomToCursorEnabled(bool bEnabled)
{
	if (bZoomToCursor == bEnabled)
	{
		return;
	}

	bZoomToCursor = bEnabled;
	NotifyChanged();
}

void UFF_RuntimeSettingsSubsystem::SetEdgeScrollEnabled(bool bEnabled)
{
	if (bEdgeScrollEnabled == bEnabled)
	{
		return;
	}

	bEdgeScrollEnabled = bEnabled;
	NotifyChanged();
}

void UFF_RuntimeSettingsSubsystem::CommitSettings(
	float NewMasterVolume,
	bool bNewZoomToCursor,
	bool bNewEdgeScrollEnabled)
{
	// 예전 세 값 버전. 마스터 외의 갈래는 지금 값을 그대로 넘긴다.
	FFF_AudioVolumes NewVolumes = Volumes;
	NewVolumes.Master = NewMasterVolume;
	CommitAllSettings(NewVolumes, bNewZoomToCursor, bNewEdgeScrollEnabled);
}

void UFF_RuntimeSettingsSubsystem::CommitAllSettings(
	const FFF_AudioVolumes& NewVolumes,
	bool bNewZoomToCursor,
	bool bNewEdgeScrollEnabled)
{
	const FFF_AudioVolumes Clamped = NewVolumes.Clamped();
	const bool bChanged =
		!Volumes.Equals(Clamped) ||
		bZoomToCursor != bNewZoomToCursor ||
		bEdgeScrollEnabled != bNewEdgeScrollEnabled;
	if (!bChanged)
	{
		return;
	}

	Volumes = Clamped;
	bZoomToCursor = bNewZoomToCursor;
	bEdgeScrollEnabled = bNewEdgeScrollEnabled;
	ApplyAudioSettings();
	NotifyChanged();
}

void UFF_RuntimeSettingsSubsystem::ResetToProjectDefaults()
{
	const UFF_RuntimeSettingsDefaults* Defaults = UFF_RuntimeSettingsDefaults::Get();
	Volumes = Defaults ? Defaults->MakeDefaultVolumes() : FFF_AudioVolumes();
	bZoomToCursor = !Defaults || Defaults->bDefaultZoomToCursor;
	bEdgeScrollEnabled = !Defaults || Defaults->bDefaultEdgeScrollEnabled;

	ApplyAudioSettings();
	NotifyChanged();
}

void UFF_RuntimeSettingsSubsystem::ApplyAudioSettings()
{
	// 마스터만 엔진 전체에 건다. 배경음·효과음은 재생하는 쪽이 방송을 받아 곱한다.
	// 사운드 클래스 애셋 없이 돌게 하려는 것이다. 애셋을 만들고 모든 소리에 지정하는 일이 끝나기 전에도 슬라이더가 먹어야 한다.
	ApplyAudioSettingsToWorld(GetWorld());
}

void UFF_RuntimeSettingsSubsystem::ApplyAudioSettingsToWorld(UWorld* World) const
{
	// 게임 인스턴스가 막 만들어진 시점에는 월드가 없다. 그 경우는 첫 월드가 준비될 때 위 신호가 채운다.
	if (!World)
	{
		return;
	}

	const FAudioDeviceHandle AudioDeviceHandle = World->GetAudioDevice();
	if (FAudioDevice* AudioDevice = AudioDeviceHandle.GetAudioDevice())
	{
		AudioDevice->SetTransientPrimaryVolume(Volumes.Master);
	}
}
