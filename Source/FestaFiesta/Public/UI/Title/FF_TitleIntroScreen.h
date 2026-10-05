// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Audio/FF_AudioTypes.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_TitleIntroScreen.generated.h"

class UAudioComponent;
class UFileMediaSource;
class UImage;
class UMediaPlayer;
class UMediaTexture;
class USoundBase;
class UWidget;

/** 인트로가 끝나 타이틀 메뉴로 넘어갈 차례다. 어디로 갈지는 화면을 띄운 흐름 서브시스템이 정한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnTitleIntroFinished);

/**
 * 게임을 켜면 처음 보는 화면. **시작 화면 → 인트로 영상 → 가운데 우리 지구** 를 한 화면 안에서 넘기고
 * 잠깐 뒤 스스로 타이틀 메뉴로 넘어간다 (2026-10-02 사용자 지시: 「아무 키」 글자와 대기 없음).
 *
 * === 왜 화면 하나인가 ===
 * 단계를 각각 스택에 올리면 타이틀에서 `뒤로` 를 눌렀을 때 인트로로 되돌아간다.
 * 한 번 지나가면 끝나는 연출이라 **흐름에는 화면 하나로 보이게** 하고, 끝나면 흐름 서브시스템이
 * 타이틀을 올린 뒤 이 화면을 스택과 히스토리에서 뺀다.
 *
 * === 이어 붙이는 방법 ===
 * - 시작 화면과 영상 사이: 시작 화면 바탕이 검은색이고 영상도 검은 우주에서 열린다.
 *   영상 첫 프레임이 오기 전에는 MediaTexture 가 `ClearColor`(검은색)를 그린다.
 * - 영상과 지구 사이: 영상은 변신 빛이 퍼져 흰 빛 정점에서 끝난다. 그 흰 덮개(`Image_FlashCover`)를 걷으면서
 *   카메라 리그가 가운데 구도로 실시간 지구를 보여 준다 (`AFF_AtlasCameraRig::PlayIntroReveal`).
 * - 지구와 메뉴 사이: `RevealHoldSeconds` 뒤 타이틀이 올라가고 카메라가 타이틀 구도로 블렌드한다
 *   — 지구가 오른쪽으로 비켜 가는 움직임이 그것이다.
 *
 * === 소리 ===
 * 영상 MP4 에는 소리가 없다. `IntroSound`(영상 + 꼬리 약 2초)를 영상이 열리는 순간 따로 튼다 —
 * 그래야 화음이 영상 끝을 넘어 지구 공개까지 이어진다. 음량은 설정 창의 배경음 슬라이더를 따른다.
 *
 * === 🔴 배경음은 메뉴가 나올 때부터 ===
 * 프론트엔드 곡은 `UFF_AudioEventLink`(장원재)가 화면을 0.2초마다 보고 고른다 — 타이틀이면 「타이틀」, 나머지는 「로비」.
 * 그 코드는 `TitleIntro` 를 몰라 인트로 동안 로비 곡을 튼다(2026-10-05 Dev 병합 기준). 남의 코드를 고치지 않고
 * **공개 API 만 써서** 인트로가 떠 있는 동안 그 곡을 멈춰 둔다(`HoldStageAmbience`). 곡은 2초 페이드인이라 첫 틱에 멈추면 들리지 않는다.
 * 끝날 때 곡을 다시 틀지 않는다 — 화면이 타이틀로 바뀌면 오디오 연동이 타이틀 곡을 튼다.
 * 직접 다시 틀면 로비 곡이 잠깐 겹친 뒤 타이틀 곡으로 바뀐다(병합 전 구현이 그랬다).
 *
 * === 넘기기 ===
 * 시작 화면은 `SplashMinSecondsBeforeSkip` 뒤부터, 영상과 지구 공개는 바로 아무 키로 넘긴다.
 * 영상 파일이 없거나 열리지 않으면 기다리지 않고 지구 단계로 간다 — 인트로 때문에 게임이 막히면 안 된다.
 *
 * 🔴 **시간 정지 권한이 없다.** 프론트엔드라 멈출 시뮬레이션도 없다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Title Intro Screen"))
class FESTAFIESTA_API UFF_TitleIntroScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UFF_TitleIntroScreen(const FObjectInitializer& ObjectInitializer);

	/** 인트로가 끝났다(지구를 보여 준 뒤 자동, 또는 넘기기). 한 번만 울린다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnTitleIntroFinished OnIntroFinished;

	/** 지금 단계를 아무 키를 누른 것처럼 넘긴다. 콘솔 `FF.Intro.Skip` 과 자동 캡처가 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void RequestSkip();

	/** `Phase=Video t=1.23 Video=재생 중` 처럼 지금 단계를 한 줄로. 진단·자동 캡처용. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	FString DescribeState() const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnHandleBackAction() override;

	// --- 배선 (WBP_FF_TitleIntro 의 위젯 이름이 아래와 같아야 한다. 생성: FF.TitleIntroUMG / FF.ApplyTitleIntroDesign) ---

	/** 화면 전체 검은 바탕. 시작 화면과 영상 사이를 잇는다. 지구 단계에서 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_Backdrop;

	/** 영상. 브러시는 실행 중 MediaTexture 로 바꾼다. 화면을 덮도록 ScaleBox(ScaleToFill) 안에 둔다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_Video;

	/** 로고·팀 이름·안내 문구 묶음. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UWidget> Panel_Splash;

	/** 맨 위 흰 덮개. 영상의 마지막 흰 빛을 이어받아 걷힌다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_FlashCover;

	// --- 조정값 (WBP Class Defaults) ---

	/** 인트로 영상. `Content/` 기준 상대 경로. `Content/Movies` 는 패키징 때 자동으로 함께 나간다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Intro")
	FString IntroMovieFile = TEXT("Movies/FF_Intro_SpaceFireworks.mp4");

	/** 영상과 함께 트는 소리. 영상보다 길어 지구 공개까지 이어진다. 비우면 소리 없이 간다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Intro|Sound")
	TSoftObjectPtr<USoundBase> IntroSound;

	/** 곡 자체 음량. 실제 음량 = 이 값 × 설정의 배경음 비율 (마스터는 엔진이 곱한다). */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Intro|Sound", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float IntroSoundVolume = 1.f;

	/** 영상을 넘겼을 때 소리를 줄여 끄는 시간. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Intro|Sound", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SkipSoundFadeSeconds = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Intro|Splash", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SplashFadeInSeconds = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Intro|Splash", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SplashHoldSeconds = 2.4f;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Intro|Splash", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SplashFadeOutSeconds = 0.5f;

	/** 안내 문구를 읽을 최소 시간. 그 전에 누른 키는 무시한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Intro|Splash", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float SplashMinSecondsBeforeSkip = 1.0f;

	/** 영상이 이 시간 안에 재생을 시작하지 않으면 건너뛴다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Intro|Video", meta = (ClampMin = "0.5", ForceUnits = "s"))
	float VideoOpenTimeoutSeconds = 4.0f;

	/** 흰 덮개가 걷히는 시간. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Intro|Reveal", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RevealFadeSeconds = 0.6f;

	/** 덮개가 걷힌 뒤 가운데 지구를 보여 주는 시간. 끝나면 스스로 메뉴로 간다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Intro|Reveal", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RevealHoldSeconds = 1.3f;

private:
	enum class EPhase : uint8
	{
		Idle,
		Splash,
		Video,
		Reveal,
		Finished
	};

	void EnterSplash();
	void EnterVideo();
	void EnterReveal();
	void Finish();

	/** 아무 키·클릭이 모두 여기로 온다. 단계마다 의미가 다르다. */
	void HandleAnyInput();

	void CloseMedia();
	void PlayIntroSound();

	/** 단계 배경음이 돌고 있으면 기억하고 멈춘다. 인트로가 떠 있는 동안 매 틱 확인한다(레벨 BeginPlay 가 화면보다 늦다). */
	void HoldStageAmbience();

	/** 멈춤을 푼다. 곡은 직접 틀지 않고 오디오 연동이 다음 화면에 맞는 곡을 고르게 둔다. */
	void ReleaseStageAmbience();
	void FadeOutIntroSound(float Seconds);

	UFUNCTION()
	void HandleMediaOpened(FString OpenedUrl);

	UFUNCTION()
	void HandleMediaOpenFailed(FString FailedUrl);

	UFUNCTION()
	void HandleMediaEndReached();

	static void SetOpacity(UWidget* Widget, float Opacity);

	EPhase Phase = EPhase::Idle;

	/** 인트로가 멈춰 둔 단계 배경음. None 이면 멈춘 것이 없다. */
	EFF_AudioAmbience HeldAmbience = EFF_AudioAmbience::None;
	float PhaseSeconds = 0.f;
	bool bVideoStarted = false;

	UPROPERTY(Transient)
	TObjectPtr<UMediaPlayer> MediaPlayer;

	UPROPERTY(Transient)
	TObjectPtr<UFileMediaSource> MediaSource;

	UPROPERTY(Transient)
	TObjectPtr<UMediaTexture> MediaTexture;

	/** 월드에 떠 있는 2D 소리. 화면이 내려가도 꼬리는 끝까지 울리게 둔다. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> IntroAudio;
};
