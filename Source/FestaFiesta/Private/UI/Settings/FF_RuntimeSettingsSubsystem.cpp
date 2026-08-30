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
	bZoomToCursor = !Defaults || Defaults->bDefaultZoomToCursor;
	bEdgeScrollEnabled = !Defaults || Defaults->bDefaultEdgeScrollEnabled;

	if (GConfig)
	{
		GConfig->GetFloat(RuntimeSettingsSection, MasterVolumeKey, MasterVolume, GGameUserSettingsIni);
		GConfig->GetBool(RuntimeSettingsSection, ZoomToCursorKey, bZoomToCursor, GGameUserSettingsIni);
		GConfig->GetBool(RuntimeSettingsSection, EdgeScrollKey, bEdgeScrollEnabled, GGameUserSettingsIni);
	}

	MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
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

void UFF_RuntimeSettingsSubsystem::ResetToProjectDefaults()
{
	const UFF_RuntimeSettingsDefaults* Defaults = UFF_RuntimeSettingsDefaults::Get();
	MasterVolume = Defaults ? FMath::Clamp(Defaults->DefaultMasterVolume, 0.f, 1.f) : 0.8f;
	bZoomToCursor = !Defaults || Defaults->bDefaultZoomToCursor;
	bEdgeScrollEnabled = !Defaults || Defaults->bDefaultEdgeScrollEnabled;

	ApplyAudioSettings();
	NotifyChanged();
}

void UFF_RuntimeSettingsSubsystem::ApplyAudioSettings()
{
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
