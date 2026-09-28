// FF_FestivalSettingsSubsystem.h
// 역할: 일시정지·설정 화면의 열기·닫기, 입력 차단과 기존 건설 화면 복구를 관리한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/Pause/FF_PauseHubScreen.h"
#include "FF_FestivalSettingsSubsystem.generated.h"

class AGameModeBase;
class APlayerController;
class UFF_BuildModeSubsystem;
class UFF_FacilityInfoSubsystem;
class UFF_FestivalSettingsScreen;
class UFF_UIRootLayout;
enum class EFF_FestivalSettingsCategory : uint8;

/**
 * 메인 축제 설정 화면의 프로젝트 배선.
 *
 * 기획자는 Project Settings > Game > FestaFiesta Festival Settings에서 진입 키, 두 WBP와
 * 일시정지 여부를 변경한다. 화면 순서는 RootLayout ScreenStack이 단독 소유한다.
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
			TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_FullscreenOptions.WBP_FF_FullscreenOptions_C")));

	/** ESC/F5가 먼저 여는 일시정지 허브 WBP. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Screen")
	TSoftClassPtr<UFF_PauseHubScreen> PauseHubScreenClass =
		TSoftClassPtr<UFF_PauseHubScreen>(FSoftObjectPath(
			TEXT("/Game/FestaFiesta/UI/Pause/WBP_FF_PauseHub.WBP_FF_PauseHub_C")));

	/** 설정 화면을 열고 닫는 키. ESC와 F5는 동일한 옵션 전환 계약을 사용한다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Input")
	TArray<FKey> OpenCloseKeys = { EKeys::Escape, EKeys::F5 };

	/** 설정 화면이 열린 동안 축제 시뮬레이션을 엔진 Pause로 멈춘다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Behavior")
	bool bPauseGameWhileOpen = true;

	/** 디스플레이에서 제시할 최소 해상도. 장치 조회 결과는 이 값보다 작은 항목을 숨긴다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Display|Resolution",
		meta = (ClampMin = "640", UIMin = "640"))
	int32 MinimumResolutionWidth = 1280;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Display|Resolution",
		meta = (ClampMin = "360", UIMin = "360"))
	int32 MinimumResolutionHeight = 720;

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
 * ESC/F5 입력, Hub/Settings 스택 수명과 Pause/Modal 복원을 소유하는 독립 서브시스템.
 *
 * Festival PlayerController가 로그인할 때 공개 InputComponent에 키 바인딩을 추가한다. 화면은
 * RootLayout ScreenStack에 정확한 인스턴스로 Push/Remove하고, 닫을 때 자신이 건 Pause만 복원한다.
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

	/** ESC/F5가 여는 1단계 일시정지 허브. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Settings")
	bool OpenPauseMenu(APlayerController* RequestingController = nullptr);

	/** Settings가 열려 있으면 허브로, 허브에서는 게임으로 안전하게 돌아간다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Settings")
	bool ClosePauseMenu();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Settings")
	bool IsPauseMenuOpen() const { return ActiveHub.IsValid() || ActiveScreen.IsValid(); }

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
	bool HasPauseSession() const;
	void BindOpenCloseInput(APlayerController* Controller);
	void UnbindOpenCloseInput();
	void ApplyToFirstLocalPlayer();
	void RemoveConflictingDebugBindings(APlayerController* Controller) const;
	bool ShouldDeferOpenToActiveGameplayMode(const APlayerController& Controller) const;
	bool OpenPauseMenuInternal(APlayerController* RequestingController, const FKey& TriggerKey);
	bool OpenSettingsLayer(TOptional<EFF_FestivalSettingsCategory> Category = {});
	bool CloseSettingsLayer();
	bool ClosePauseSession();
	void ForceClosePauseSession();
	void AcquirePauseSession(APlayerController& Controller);
	void ReleasePauseSession();
	bool SuspendActiveBuildMode(APlayerController& Controller);
	void ResumeSuspendedBuildMode();
	bool SuppressFacilityPresentation(APlayerController& Controller);
	void RestoreSuppressedFacilityPresentation();
	APlayerController* ResolveLocalController(APlayerController* RequestedController) const;
	UFF_UIRootLayout* ResolveRootLayout(APlayerController& Controller);

	UFUNCTION()
	void HandleOpenCloseInput(FKey PressedKey);

	UFUNCTION()
	void HandlePauseHubAction(EFF_PauseHubAction Action);

	UFUNCTION()
	void HandlePauseHubReleased();

	UFUNCTION()
	void HandleCloseRequested();

	UFUNCTION()
	void HandleRuntimeSettingsChanged();

	FDelegateHandle PostLoginHandle;
	TArray<TWeakObjectPtr<APlayerController>> BoundControllers;
	/** UI Flow가 의도적으로 자동 주입되지 않는 테스트 맵에서만 이 서브시스템이 소유하는 독립 RootLayout. */
	TWeakObjectPtr<UFF_UIRootLayout> StandaloneRootLayout;
	TWeakObjectPtr<UFF_UIRootLayout> ActiveRootLayout;
	TWeakObjectPtr<UFF_PauseHubScreen> ActiveHub;
	TWeakObjectPtr<UFF_FestivalSettingsScreen> ActiveScreen;
	TWeakObjectPtr<APlayerController> ActiveController;
	// Pause Hub 아래에서 상태·프리뷰를 유지한 채 입력만 빌려온 건축 세션. 종료 때 정확히 한 번 복원한다.
	TWeakObjectPtr<UFF_BuildModeSubsystem> SuspendedBuildMode;
	// RootLayout보다 높은 전역 시설 UI를 숨긴 Pause 세션 lease. 건축/Live 모두 같은 owner가 복원한다.
	TWeakObjectPtr<UFF_FacilityInfoSubsystem> SuppressedFacilityInfo;
	bool bPausedGameForPauseMenu = false;
	bool bOwnsModalUiBlock = false;
	bool bTransitionInProgress = false;
	uint64 LastStateTransitionFrame = MAX_uint64;
};
