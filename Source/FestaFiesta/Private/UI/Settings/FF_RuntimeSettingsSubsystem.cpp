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
	const TCHAR* ZoomToCursorKey = TEXT("bZoomToCursor");
	const TCHAR* EdgeScrollKey = TEXT("bEdgeScrollEnabled");
}

const UFF_RuntimeSettingsDefaults* UFF_RuntimeSettingsDefaults::Get()
{
	return GetDefault<UFF_RuntimeSettingsDefaults>();
}

void UFF_RuntimeSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadFromUserSettings();
	ApplyAudioSettings();
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
	MasterVolume = Defaults ? Defaults->DefaultMasterVolume : 0.8f;
	MusicVolume = Defaults ? Defaults->DefaultMusicVolume : 1.f;
	EffectsVolume = Defaults ? Defaults->DefaultEffectsVolume : 1.f;
	bZoomToCursor = !Defaults || Defaults->bDefaultZoomToCursor;
	bEdgeScrollEnabled = !Defaults || Defaults->bDefaultEdgeScrollEnabled;

	if (GConfig)
	{
		GConfig->GetFloat(RuntimeSettingsSection, MasterVolumeKey, MasterVolume, GGameUserSettingsIni);
		// 예전 ini 에는 이 두 키가 없다. 없으면 GetFloat 이 값을 건드리지 않아 위 기본값이 남는다.
		GConfig->GetFloat(RuntimeSettingsSection, MusicVolumeKey, MusicVolume, GGameUserSettingsIni);
		GConfig->GetFloat(RuntimeSettingsSection, EffectsVolumeKey, EffectsVolume, GGameUserSettingsIni);
		GConfig->GetBool(RuntimeSettingsSection, ZoomToCursorKey, bZoomToCursor, GGameUserSettingsIni);
		GConfig->GetBool(RuntimeSettingsSection, EdgeScrollKey, bEdgeScrollEnabled, GGameUserSettingsIni);
	}

	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.f, 1.f);
	EffectsVolume = FMath::Clamp(EffectsVolume, 0.f, 1.f);
}

void UFF_RuntimeSettingsSubsystem::SaveToUserSettings() const
{
	if (!GConfig)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_RuntimeSettingsSubsystem: GConfig가 없어 개인 설정을 저장하지 못했다."));
		return;
	}

	GConfig->SetFloat(RuntimeSettingsSection, MasterVolumeKey, MasterVolume, GGameUserSettingsIni);
	GConfig->SetFloat(RuntimeSettingsSection, MusicVolumeKey, MusicVolume, GGameUserSettingsIni);
	GConfig->SetFloat(RuntimeSettingsSection, EffectsVolumeKey, EffectsVolume, GGameUserSettingsIni);
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
	if (FMath::IsNearlyEqual(MasterVolume, NewValue))
	{
		return;
	}

	MasterVolume = NewValue;
	ApplyAudioSettings();
	NotifyChanged();
}

void UFF_RuntimeSettingsSubsystem::SetMusicVolume(float NewValue)
{
	NewValue = FMath::Clamp(NewValue, 0.f, 1.f);
	if (FMath::IsNearlyEqual(MusicVolume, NewValue))
	{
		return;
	}

	// 엔진에 직접 걸지 않는다. 배경음을 재생하는 쪽(UFF_AudioSubsystem)이 방송을 받아 곱한다.
	MusicVolume = NewValue;
	NotifyChanged();
}

void UFF_RuntimeSettingsSubsystem::SetEffectsVolume(float NewValue)
{
	NewValue = FMath::Clamp(NewValue, 0.f, 1.f);
	if (FMath::IsNearlyEqual(EffectsVolume, NewValue))
	{
		return;
	}

	EffectsVolume = NewValue;
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
	// 예전 세 값 버전. 배경음·효과음은 지금 값을 그대로 넘긴다.
	CommitAllSettings(NewMasterVolume, MusicVolume, EffectsVolume, bNewZoomToCursor, bNewEdgeScrollEnabled);
}

void UFF_RuntimeSettingsSubsystem::CommitAllSettings(
	float NewMasterVolume,
	float NewMusicVolume,
	float NewEffectsVolume,
	bool bNewZoomToCursor,
	bool bNewEdgeScrollEnabled)
{
	NewMasterVolume = FMath::Clamp(NewMasterVolume, 0.f, 1.f);
	NewMusicVolume = FMath::Clamp(NewMusicVolume, 0.f, 1.f);
	NewEffectsVolume = FMath::Clamp(NewEffectsVolume, 0.f, 1.f);
	const bool bChanged =
		!FMath::IsNearlyEqual(MasterVolume, NewMasterVolume) ||
		!FMath::IsNearlyEqual(MusicVolume, NewMusicVolume) ||
		!FMath::IsNearlyEqual(EffectsVolume, NewEffectsVolume) ||
		bZoomToCursor != bNewZoomToCursor ||
		bEdgeScrollEnabled != bNewEdgeScrollEnabled;
	if (!bChanged)
	{
		return;
	}

	MasterVolume = NewMasterVolume;
	MusicVolume = NewMusicVolume;
	EffectsVolume = NewEffectsVolume;
	bZoomToCursor = bNewZoomToCursor;
	bEdgeScrollEnabled = bNewEdgeScrollEnabled;
	ApplyAudioSettings();
	NotifyChanged();
}

void UFF_RuntimeSettingsSubsystem::ResetToProjectDefaults()
{
	const UFF_RuntimeSettingsDefaults* Defaults = UFF_RuntimeSettingsDefaults::Get();
	MasterVolume = Defaults ? FMath::Clamp(Defaults->DefaultMasterVolume, 0.f, 1.f) : 0.8f;
	MusicVolume = Defaults ? FMath::Clamp(Defaults->DefaultMusicVolume, 0.f, 1.f) : 1.f;
	EffectsVolume = Defaults ? FMath::Clamp(Defaults->DefaultEffectsVolume, 0.f, 1.f) : 1.f;
	bZoomToCursor = !Defaults || Defaults->bDefaultZoomToCursor;
	bEdgeScrollEnabled = !Defaults || Defaults->bDefaultEdgeScrollEnabled;

	ApplyAudioSettings();
	NotifyChanged();
}

void UFF_RuntimeSettingsSubsystem::ApplyAudioSettings()
{
	// 마스터만 엔진 전체에 건다. 배경음·효과음은 재생하는 쪽이 방송을 받아 곱한다.
	// 사운드 클래스 애셋 없이 돌게 하려는 것이다. 애셋을 만들고 모든 소리에 지정하는 일이 끝나기 전에도 슬라이더가 먹어야 한다.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FAudioDeviceHandle AudioDeviceHandle = World->GetAudioDevice();
	if (FAudioDevice* AudioDevice = AudioDeviceHandle.GetAudioDevice())
	{
		AudioDevice->SetTransientPrimaryVolume(MasterVolume);
	}
}
