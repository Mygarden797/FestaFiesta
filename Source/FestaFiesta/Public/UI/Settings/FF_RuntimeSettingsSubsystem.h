// FF_RuntimeSettingsSubsystem.h
// 역할: 화면·음량·입력 등 사용자 설정을 읽고 저장하며 게임에 적용한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_RuntimeSettingsSubsystem.generated.h"

/**
 * 플레이어별 런타임 설정의 프로젝트 기본값.
 *
 * 기획자는 Project Settings > Game > FestaFiesta Runtime Settings에서 기본값을 바꾼다.
 * 실제 플레이어 값은 GameUserSettings.ini에 저장되므로 팀의 DefaultGame.ini를 오염시키지 않는다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Runtime Settings"))
class FESTAFIESTA_API UFF_RuntimeSettingsDefaults : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Settings")
	static const UFF_RuntimeSettingsDefaults* Get();

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Audio",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DefaultMasterVolume = 0.8f;

	// 배경음·효과음은 마스터에 곱해지는 비율이다. 기본 1 이면 마스터만 움직여도 예전과 같다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Audio",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DefaultMusicVolume = 1.f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Audio",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DefaultEffectsVolume = 1.f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Gameplay|Camera")
	bool bDefaultZoomToCursor = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Gameplay|Camera")
	bool bDefaultEdgeScrollEnabled = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnRuntimeSettingsChanged);

/**
 * 화면 수명과 분리된 플레이어 개인 설정의 단일 소유자.
 *
 * UMG는 값을 보관하지 않고 이 서브시스템에서 읽고 변경을 요청한다. 따라서 설정 화면을 닫거나
 * 맵을 이동해도 값이 유지된다. 그래픽 설정은 엔진 UGameUserSettings가 소유하므로 여기서 중복 저장하지 않는다.
 */
UCLASS()
class FESTAFIESTA_API UFF_RuntimeSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	static UFF_RuntimeSettingsSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Settings")
	float GetMasterVolume() const { return MasterVolume; }

	// 배경음 비율(0~1). 실제 적용은 UFF_AudioSubsystem 이 OnSettingsChanged 를 받아 한다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Settings")
	float GetMusicVolume() const { return MusicVolume; }

	// 효과음 비율(0~1). UI·환경음도 여기에 묶인다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Settings")
	float GetEffectsVolume() const { return EffectsVolume; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Settings")
	bool IsZoomToCursorEnabled() const { return bZoomToCursor; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Settings")
	bool IsEdgeScrollEnabled() const { return bEdgeScrollEnabled; }

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void SetMasterVolume(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void SetMusicVolume(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void SetEffectsVolume(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void SetZoomToCursorEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void SetEdgeScrollEnabled(bool bEnabled);

	/**
	 * 옵션 화면의 임시 값을 한 번에 확정한다.
	 *
	 * 행마다 setter를 호출하면 슬라이더를 움직이는 동안 ini를 계속 쓰게 된다. 명시적 저장 UX는
	 * 이 함수로 세 값을 모아 적용하고, 실제 변경이 있을 때만 한 번 저장·방송한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void CommitSettings(float NewMasterVolume, bool bNewZoomToCursor, bool bNewEdgeScrollEnabled);

	// 배경음·효과음까지 다섯 값을 한 번에 확정한다. 두 설정 화면은 이쪽을 쓴다.
	// 위 CommitSettings 를 고치지 않고 새로 둔 이유는 그 노드를 쓰는 BP 가 핀이 바뀌어 깨지지 않게 하려는 것이다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void CommitAllSettings(float NewMasterVolume, float NewMusicVolume, float NewEffectsVolume,
		bool bNewZoomToCursor, bool bNewEdgeScrollEnabled);

	/** 프로젝트 기본값으로 돌아가고 한 번만 저장·방송한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void ResetToProjectDefaults();

	/** 새 월드의 오디오 디바이스가 준비된 뒤 현재 마스터 음량을 다시 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void ApplyAudioSettings();

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Settings")
	FFF_OnRuntimeSettingsChanged OnSettingsChanged;

private:
	void LoadFromUserSettings();
	void SaveToUserSettings() const;
	void NotifyChanged();

	float MasterVolume = 0.8f;
	float MusicVolume = 1.f;
	float EffectsVolume = 1.f;
	bool bZoomToCursor = true;
	bool bEdgeScrollEnabled = true;
};
