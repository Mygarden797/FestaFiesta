// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_FestivalSettingsSubsystem.generated.h"

class AGameModeBase;
class APlayerController;
class UFF_FestivalSettingsScreen;

/**
 * 메인 축제 설정 화면의 프로젝트 배선.
 *
 * 기획자는 Project Settings > Game > FestaFiesta Festival Settings에서 진입 키, WBP, ZOrder와
 * 일시정지 여부를 변경한다. 값은 전용 설정 클래스에 모아 기존 UI Flow/Controller 설정을 오염시키지 않는다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Festival Settings"))
class FESTAFIESTA_API UFF_FestivalSettingsDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Settings")
	static const UFF_FestivalSettingsDeveloperSettings* Get();

	/** 메인 축제 화면에 표시할 설정 WBP. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Screen")
	TSoftClassPtr<UFF_FestivalSettingsScreen> SettingsScreenClass =
		TSoftClassPtr<UFF_FestivalSettingsScreen>(FSoftObjectPath(
			TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_FestivalSettings.WBP_FF_FestivalSettings_C")));

	/** 설정 화면을 열고 닫는 키. 기본값은 ESC와 F5다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Input")
	TArray<FKey> OpenCloseKeys = { EKeys::Escape, EKeys::F5 };

	/** 다른 HUD 위에 설정 화면을 표시할 순서. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Screen",
		meta = (ClampMin = "0", UIMin = "0"))
	int32 ScreenZOrder = 1000;

	/** 설정 화면이 열린 동안 축제 시뮬레이션을 엔진 Pause로 멈춘다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Behavior")
	bool bPauseGameWhileOpen = true;

	/** 디스플레이에서 제시할 최소 해상도. 장치 조회 결과는 이 값보다 작은 항목을 숨긴다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Display|Resolution",
		meta = (ClampMin = "640", UIMin = "640"))
	int32 MinimumResolutionWidth = 1024;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Display|Resolution",
		meta = (ClampMin = "360", UIMin = "360"))
	int32 MinimumResolutionHeight = 576;

	/** 장치가 해상도 목록을 돌려주지 않을 때와 창 모드에서 사용할 안전한 후보. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Display|Resolution")
	TArray<FIntPoint> FallbackResolutions = {
		FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080),
		FIntPoint(2560, 1440), FIntPoint(3840, 2160)
	};

	/** 해상도 스케일 선택지(%). 현재 값이 목록에 없으면 화면이 자동으로 추가한다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Display|Graphics")
	TArray<int32> ResolutionScalePercentOptions = { 50, 60, 67, 75, 80, 90, 100 };

	/** 프레임 제한 선택지. 0은 무제한이며 현재 값이 목록에 없으면 자동 추가한다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Display|Graphics")
	TArray<int32> FrameRateLimitOptions = { 0, 30, 60, 90, 120, 144, 165, 240 };
};

/**
 * 기존 팀원 클래스 수정 없이 ESC/F5 입력, WBP 수명, Pause 복원을 소유하는 독립 서브시스템.
 *
 * Festival PlayerController가 로그인할 때 공개 InputComponent에 키 바인딩을 추가한다. UI Flow 스택이나
 * Stage 배속을 건드리지 않으며, 닫을 때 자신이 건 Pause만 복원한다.
 */
UCLASS()
class FESTAFIESTA_API UFF_FestivalSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UFF_FestivalSettingsSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Settings")
	bool ToggleFestivalSettings(APlayerController* RequestingController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Settings")
	bool OpenFestivalSettings(APlayerController* RequestingController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Settings")
	bool CloseFestivalSettings();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Settings")
	bool IsFestivalSettingsOpen() const { return ActiveScreen.IsValid(); }

	/** 저장된 카메라 토글을 현재 Festival 카메라의 공개 설정에 반영한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Settings")
	void ApplyGameplaySettings(APlayerController* TargetController = nullptr);

private:
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void BindOpenCloseInput(APlayerController* Controller);
	void UnbindOpenCloseInput();
	void ApplyToFirstLocalPlayer();
	void RemoveConflictingDebugBindings(APlayerController* Controller) const;
	void RestoreLitViewModeIfNeeded(const FKey& PressedKey) const;

	UFUNCTION()
	void HandleOpenCloseInput(FKey PressedKey);

	UFUNCTION()
	void HandleCloseRequested();

	UFUNCTION()
	void HandleRuntimeSettingsChanged();

	FDelegateHandle PostLoginHandle;
	TArray<TWeakObjectPtr<APlayerController>> BoundControllers;
	TWeakObjectPtr<UFF_FestivalSettingsScreen> ActiveScreen;
	TWeakObjectPtr<APlayerController> ActiveController;
	bool bPausedGameForSettings = false;
};
