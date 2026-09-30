// FF_RuntimeSettingsSubsystem.h
// 역할: 화면·음량·입력 등 사용자 설정을 읽고 저장하며 게임에 적용한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_RuntimeSettingsSubsystem.generated.h"

struct FActorsInitializedParams;

/* 소리 갈래별 음량. 마스터를 뺀 나머지는 마스터에 곱해지는 비율(0~1)이다.

   값을 하나의 묶음으로 두는 이유는 갈래가 늘어날 때 고칠 곳을 줄이려는 것이다.
   예전에는 갈래마다 getter·setter·저장 키·확정 함수 인자가 하나씩 늘었다. 묶어두면
   설정 화면은 사본 하나를 들고 있다가 통째로 넘기면 되고, 비교도 한 곳에서 한다.

   마스터는 엔진 전체 음량(SetTransientPrimaryVolume)이라 모든 소리에 먹는다.
   나머지는 축제 사운드(UFF_AudioSubsystem)가 재생할 때 곱하므로, 그 서브시스템을 거치지
   않는 소리(버튼 스타일에 박힌 클릭음 등)는 마스터만 따른다.
*/
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_AudioVolumes
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Master = 0.8f;

	// 축제 배경음, 피날레 곡, 엔딩곡.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Music = 1.f;

	// 건설, 대기열, 단계 전환처럼 운영 중에 나는 소리. 환경음도 여기에 묶는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Effects = 1.f;

	// 피날레 불꽃. 가장 크고 몰아서 터지므로 따로 줄일 수 있게 뺐다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Fireworks = 1.f;

	// 버튼 클릭·확인·오류 같은 화면 소리.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Ui = 1.f;

	// 모든 값을 0~1 로 자른 사본.
	FFF_AudioVolumes Clamped() const
	{
		FFF_AudioVolumes Out;
		Out.Master = FMath::Clamp(Master, 0.f, 1.f);
		Out.Music = FMath::Clamp(Music, 0.f, 1.f);
		Out.Effects = FMath::Clamp(Effects, 0.f, 1.f);
		Out.Fireworks = FMath::Clamp(Fireworks, 0.f, 1.f);
		Out.Ui = FMath::Clamp(Ui, 0.f, 1.f);
		return Out;
	}

	bool Equals(const FFF_AudioVolumes& Other, const float Tolerance = 0.001f) const
	{
		return FMath::IsNearlyEqual(Master, Other.Master, Tolerance)
			&& FMath::IsNearlyEqual(Music, Other.Music, Tolerance)
			&& FMath::IsNearlyEqual(Effects, Other.Effects, Tolerance)
			&& FMath::IsNearlyEqual(Fireworks, Other.Fireworks, Tolerance)
			&& FMath::IsNearlyEqual(Ui, Other.Ui, Tolerance);
	}
};

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

	// 아래 갈래들은 마스터에 곱해지는 비율이다. 기본 1 이면 마스터만 움직여도 예전과 같다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Audio",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DefaultMusicVolume = 1.f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Audio",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DefaultEffectsVolume = 1.f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Audio",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DefaultFireworksVolume = 1.f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Audio",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DefaultUiVolume = 1.f;

	// 위 값들을 음량 묶음으로. 초기화와 첫 실행이 같은 값을 쓰게 한 곳에서 만든다.
	FFF_AudioVolumes MakeDefaultVolumes() const;

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
	virtual void Deinitialize() override;

	static UFF_RuntimeSettingsSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Settings")
	float GetMasterVolume() const { return Volumes.Master; }

	// 갈래별 음량 전체. 마스터 외의 값은 UFF_AudioSubsystem 이 OnSettingsChanged 를 받아 적용한다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Settings")
	FFF_AudioVolumes GetAudioVolumes() const { return Volumes; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Settings")
	bool IsZoomToCursorEnabled() const { return bZoomToCursor; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Settings")
	bool IsEdgeScrollEnabled() const { return bEdgeScrollEnabled; }

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void SetMasterVolume(float NewValue);

	// 음량 묶음을 바로 적용하고 저장한다. 슬라이더마다 부르면 ini 를 계속 쓰므로 화면은 CommitAllSettings 를 쓴다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void SetAudioVolumes(const FFF_AudioVolumes& NewVolumes);

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

	// 음량 묶음과 카메라 옵션을 한 번에 확정한다. 두 설정 화면은 이쪽을 쓴다.
	// 위 CommitSettings 를 고치지 않고 새로 둔 이유는 그 노드를 쓰는 BP 가 핀이 바뀌어 깨지지 않게 하려는 것이다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void CommitAllSettings(const FFF_AudioVolumes& NewVolumes, bool bNewZoomToCursor, bool bNewEdgeScrollEnabled);

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

	// 레벨이 바뀌면 엔진이 마스터 음량을 1.0 으로 되돌리므로 새 월드마다 다시 건다.
	void HandleWorldActorsInitialized(const FActorsInitializedParams& Params);
	void ApplyAudioSettingsToWorld(UWorld* World) const;

	FDelegateHandle WorldActorsInitializedHandle;

	FFF_AudioVolumes Volumes;
	bool bZoomToCursor = true;
	bool bEdgeScrollEnabled = true;
};
