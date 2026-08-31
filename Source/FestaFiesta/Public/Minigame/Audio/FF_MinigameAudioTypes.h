// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "FF_MinigameAudioTypes.generated.h"

class USoundBase;
class USoundConcurrency;

/** Stable presentation events shared by native screens, Blueprints and authored audio profiles. */
UENUM(BlueprintType)
enum class EFF_MinigameAudioEvent : uint8
{
	SessionStarted = 0 UMETA(DisplayName = "공통 - 게임 시작"),
	SessionCompleted = 1 UMETA(DisplayName = "공통 - 게임 완료"),
	SessionCancelled = 2 UMETA(DisplayName = "공통 - 게임 취소"),

	TargetSpawned = 10 UMETA(DisplayName = "슈팅 - 과녁 등장"),
	ShotFired = 11 UMETA(DisplayName = "슈팅 - 물총 발사"),
	TargetHit = 12 UMETA(DisplayName = "슈팅 - 명중"),
	TargetPrecision = 13 UMETA(DisplayName = "슈팅 - 정밀 명중"),
	TargetBullseye = 14 UMETA(DisplayName = "슈팅 - 정중앙 명중"),
	TargetMissed = 15 UMETA(DisplayName = "슈팅 - 빗나감"),
	TargetEscaped = 16 UMETA(DisplayName = "슈팅 - 과녁 시간 초과"),
	BonusTargetHit = 17 UMETA(DisplayName = "슈팅 - 보너스 과녁 명중"),

	SurfObstacleHit = 30 UMETA(DisplayName = "서핑 - 장애물 충돌"),
	SurfObstacleSmashed = 31 UMETA(DisplayName = "서핑 - 장애물 파괴"),
	SurfCoinCollected = 32 UMETA(DisplayName = "서핑 - 코인 획득"),
	SurfHeartCollected = 33 UMETA(DisplayName = "서핑 - 하트 획득"),
	SurfBoostCollected = 34 UMETA(DisplayName = "서핑 - 부스트 획득"),
	SurfBoostActivated = 35 UMETA(DisplayName = "서핑 - 부스트 사용"),

	ClawDropStarted = 50 UMETA(DisplayName = "인형뽑기 - 집게 하강"),
	ClawClosing = 51 UMETA(DisplayName = "인형뽑기 - 집게 닫힘"),
	ClawRelease = 52 UMETA(DisplayName = "인형뽑기 - 상품 배출"),
	ClawPrizeCaptured = 53 UMETA(DisplayName = "인형뽑기 - 상품 획득"),
	ClawPerfectCapture = 54 UMETA(DisplayName = "인형뽑기 - 퍼펙트 획득"),
	ClawPrizeMissed = 55 UMETA(DisplayName = "인형뽑기 - 획득 실패")
};

/** One replaceable Sound Wave, Sound Cue or MetaSound entry. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_MinigameSoundEvent
{
	GENERATED_BODY()

	/** Accepts every USoundBase asset type, including Sound Cue and MetaSound Source. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "사운드",
		meta = (DisplayName = "사운드 에셋", ToolTip = "Sound Wave, Sound Cue 또는 MetaSound Source를 지정합니다. 비워 두면 아래 폴백 톤 설정을 사용합니다."))
	TSoftObjectPtr<USoundBase> Sound;

	/** Optional per-event concurrency override. The asset's own concurrency is used when empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "사운드",
		meta = (DisplayName = "동시 재생 정책", ToolTip = "연타 제한이나 우선순위가 필요할 때 Sound Concurrency 에셋을 지정합니다."))
	TSoftObjectPtr<USoundConcurrency> Concurrency;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "사운드",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0", DisplayName = "볼륨"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "사운드",
		meta = (ClampMin = "0.125", ClampMax = "4.0", UIMin = "0.8", UIMax = "1.2", DisplayName = "최소 피치"))
	float PitchMin = 0.97f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "사운드",
		meta = (ClampMin = "0.125", ClampMax = "4.0", UIMin = "0.8", UIMax = "1.2", DisplayName = "최대 피치"))
	float PitchMax = 1.03f;

	/** A local guard in addition to Unreal Sound Concurrency. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "사운드",
		meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s", DisplayName = "최소 재생 간격"))
	float MinRetriggerIntervalSeconds = 0.025f;

	/** Keeps a usable sound pass before final audio assets are authored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "사운드",
		meta = (DisplayName = "내장 폴백 톤 허용", ToolTip = "사운드 에셋이 비어 있거나 로드에 실패하면 이벤트별 내장 톤을 재생합니다."))
	bool bAllowBuiltInFallbackTone = true;

	bool IsValid(FString* OutFailureReason = nullptr) const;
};

/** Per-minigame audio profile embedded in the minigame Definition Data Asset. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_MinigameAudioProfile
{
	GENERATED_BODY()

	/** Optional background music owned for the complete minigame session. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "배경 음악",
		meta = (DisplayName = "배경 음악", ToolTip = "미니게임을 플레이하는 동안 사용할 Sound Wave, Sound Cue 또는 MetaSound Source입니다."))
	TSoftObjectPtr<USoundBase> Music;

	/** The subsystem also restarts one-shot assets, while asset-authored loops remain sample-accurate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "배경 음악",
		meta = (DisplayName = "게임 중 반복 재생", ToolTip = "켜면 미니게임이 끝날 때까지 배경 음악을 반복합니다. Sound Cue/MetaSound 내부 루프가 없는 일반 음원도 자동으로 다시 시작합니다."))
	bool bLoopMusicDuringSession = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "배경 음악",
		meta = (DisplayName = "배경 음악 동시 재생 정책"))
	TSoftObjectPtr<USoundConcurrency> MusicConcurrency;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "배경 음악",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0", DisplayName = "배경 음악 볼륨"))
	float MusicVolume = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "효과음",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0", DisplayName = "효과음 전체 볼륨"))
	float EffectsVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "배경 음악",
		meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s", DisplayName = "페이드 인 시간"))
	float MusicFadeInSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "배경 음악",
		meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s", DisplayName = "페이드 아웃 시간"))
	float MusicFadeOutSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "효과음",
		meta = (DisplayName = "이벤트별 효과음", ToolTip = "키는 코드와 공유되는 안정적인 이벤트 ID입니다. 값의 사운드 에셋만 교체하면 코드 수정 없이 반영됩니다."))
	TMap<EFF_MinigameAudioEvent, FFF_MinigameSoundEvent> EventSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "효과음",
		meta = (DisplayName = "내장 폴백 톤 사용", ToolTip = "최종 사운드 에셋이 없는 이벤트에 가벼운 내장 톤을 재생합니다. 전체 음향 납품 후 끌 수 있습니다."))
	bool bEnableBuiltInFallbackTones = true;

	bool IsValid(FString* OutFailureReason = nullptr) const;
};
