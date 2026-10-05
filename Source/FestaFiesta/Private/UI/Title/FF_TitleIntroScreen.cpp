// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Title/FF_TitleIntroScreen.h"

#include "Audio/FF_AudioSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "FileMediaSource.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"
#include "Sound/SoundBase.h"
#include "UI/Atlas/FF_AtlasCameraRig.h"
#include "UI/Core/FF_UIAccessibilitySubsystem.h"
#include "UI/FF_UILog.h"
#include "UObject/UObjectIterator.h"

namespace FFTitleIntro
{
	/** 0~1 로 자른 선형 진행률. 길이가 0 이면 바로 1 이다. */
	float Progress(const float Seconds, const float Duration)
	{
		return Duration <= KINDA_SMALL_NUMBER ? 1.f : FMath::Clamp(Seconds / Duration, 0.f, 1.f);
	}

	const TCHAR* PhaseName(const uint8 Phase)
	{
		static const TCHAR* Names[] = { TEXT("Idle"), TEXT("Splash"), TEXT("Video"), TEXT("Reveal"), TEXT("Finished") };
		return Phase < UE_ARRAY_COUNT(Names) ? Names[Phase] : TEXT("?");
	}
}

UFF_TitleIntroScreen::UFF_TitleIntroScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 소리는 UI 폴더 안에 둔다. `/Game/FestaFiesta/UI` 는 DirectoriesToAlwaysCook 이라 패키징에서 빠지지 않는다.
	IntroSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(
		TEXT("/Game/FestaFiesta/UI/Title/Intro/S_FF_Intro_SpaceFireworks.S_FF_Intro_SpaceFireworks")));
}

void UFF_TitleIntroScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 화면 자체가 키를 받는다. 버튼이 없으니 포커스를 줄 곳이 자기뿐이다.
	SetIsFocusable(true);

	// 아이들이 전부 HitTestInvisible 이라 자기가 Visible 이어야 클릭이 들어온다.
	SetVisibility(ESlateVisibility::Visible);
}

void UFF_TitleIntroScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 스택이 위젯을 재사용할 수 있으므로 상태는 활성화 때마다 처음부터 세운다.
	EnterSplash();
}

void UFF_TitleIntroScreen::NativeOnDeactivated()
{
	// 소리는 끄지 않는다. 타이틀이 올라온 뒤에도 화음 꼬리가 이어지는 것이 의도다.
	CloseMedia();
	// 끝나기 전에 내려가도(레벨 전환 등) 배경음을 막아 둔 채 두지 않는다.
	ReleaseStageAmbience();
	Super::NativeOnDeactivated();
}

void UFF_TitleIntroScreen::NativeDestruct()
{
	// 해제는 조건부, 정리는 무조건.
	CloseMedia();
	if (MediaPlayer)
	{
		MediaPlayer->OnMediaOpened.RemoveDynamic(this, &UFF_TitleIntroScreen::HandleMediaOpened);
		MediaPlayer->OnMediaOpenFailed.RemoveDynamic(this, &UFF_TitleIntroScreen::HandleMediaOpenFailed);
		MediaPlayer->OnEndReached.RemoveDynamic(this, &UFF_TitleIntroScreen::HandleMediaEndReached);
	}
	Super::NativeDestruct();
}

UWidget* UFF_TitleIntroScreen::NativeGetDesiredFocusTarget() const
{
	return const_cast<UFF_TitleIntroScreen*>(this);
}

// --- 단계 ---

void UFF_TitleIntroScreen::EnterSplash()
{
	Phase = EPhase::Splash;
	PhaseSeconds = 0.f;
	bVideoStarted = false;

	if (Image_Backdrop) { Image_Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible); SetOpacity(Image_Backdrop, 1.f); }
	if (Image_Video) { Image_Video->SetVisibility(ESlateVisibility::Collapsed); }
	if (Panel_Splash) { Panel_Splash->SetVisibility(ESlateVisibility::HitTestInvisible); SetOpacity(Panel_Splash, 0.f); }
	if (Image_FlashCover) { Image_FlashCover->SetVisibility(ESlateVisibility::Collapsed); }

	UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 인트로 시작 — 시작 화면."), *GetName());
}

void UFF_TitleIntroScreen::EnterVideo()
{
	Phase = EPhase::Video;
	PhaseSeconds = 0.f;
	bVideoStarted = false;

	if (Panel_Splash) { Panel_Splash->SetVisibility(ESlateVisibility::Collapsed); }

	const FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() / IntroMovieFile);
	if (IntroMovieFile.IsEmpty() || !FPaths::FileExists(FullPath))
	{
		// 영상이 없어도 게임은 시작돼야 한다. 다른 사람 PC 에 아직 파일이 없을 수 있다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 인트로 영상이 없어 건너뛴다: %s"), *GetName(), *FullPath);
		EnterReveal();
		return;
	}

	if (!MediaPlayer)
	{
		MediaPlayer = NewObject<UMediaPlayer>(this, TEXT("IntroMediaPlayer"), RF_Transient);
		MediaPlayer->PlayOnOpen = true;
		MediaPlayer->OnMediaOpened.AddUniqueDynamic(this, &UFF_TitleIntroScreen::HandleMediaOpened);
		MediaPlayer->OnMediaOpenFailed.AddUniqueDynamic(this, &UFF_TitleIntroScreen::HandleMediaOpenFailed);
		MediaPlayer->OnEndReached.AddUniqueDynamic(this, &UFF_TitleIntroScreen::HandleMediaEndReached);
	}
	if (!MediaTexture)
	{
		MediaTexture = NewObject<UMediaTexture>(this, TEXT("IntroMediaTexture"), RF_Transient);
		// 첫 프레임이 디코딩되기 전에는 이 색을 그린다. 시작 화면 바탕·영상 첫 장면과 같은 검은색이라 이음매가 없다.
		MediaTexture->AutoClear = true;
		MediaTexture->ClearColor = FLinearColor::Black;
		MediaTexture->SetMediaPlayer(MediaPlayer);
		MediaTexture->UpdateResource();
	}
	if (!MediaSource)
	{
		MediaSource = NewObject<UFileMediaSource>(this, TEXT("IntroMediaSource"), RF_Transient);
	}
	MediaSource->SetFilePath(FullPath);

	if (Image_Video)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.SetResourceObject(MediaTexture);
		Brush.SetImageSize(FVector2D(1920.f, 1080.f));
		Image_Video->SetBrush(Brush);
		Image_Video->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (!MediaPlayer->OpenSource(MediaSource))
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] 인트로 영상을 열지 못해 건너뛴다: %s"), *GetName(), *FullPath);
		EnterReveal();
		return;
	}
	UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 인트로 영상 여는 중: %s"), *GetName(), *FullPath);
}

void UFF_TitleIntroScreen::EnterReveal()
{
	if (Phase == EPhase::Reveal || Phase == EPhase::Finished)
	{
		return;
	}
	Phase = EPhase::Reveal;
	PhaseSeconds = 0.f;
	CloseMedia();

	// 영상의 마지막 흰 빛을 덮개가 이어받는다. 그 아래에서 바탕과 영상을 치우고 월드의 지구를 드러낸다.
	if (Image_FlashCover) { Image_FlashCover->SetVisibility(ESlateVisibility::HitTestInvisible); SetOpacity(Image_FlashCover, 1.f); }
	if (Image_Video) { Image_Video->SetVisibility(ESlateVisibility::Collapsed); }
	if (Image_Backdrop) { Image_Backdrop->SetVisibility(ESlateVisibility::Collapsed); }
	if (Panel_Splash) { Panel_Splash->SetVisibility(ESlateVisibility::Collapsed); }

	if (AFF_AtlasCameraRig* Rig = AFF_AtlasCameraRig::Find(this, /*bLogIfMissing=*/false))
	{
		Rig->PlayIntroReveal();
	}
	UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 인트로 — 우리 지구 공개, %.1f초 뒤 메뉴로."), *GetName(),
		RevealFadeSeconds + RevealHoldSeconds);
}

void UFF_TitleIntroScreen::Finish()
{
	if (Phase == EPhase::Finished)
	{
		return;
	}
	Phase = EPhase::Finished;
	PhaseSeconds = 0.f;
	CloseMedia();
	UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 인트로 끝 — 타이틀로."), *GetName());
	// 메뉴가 나오는 이 순간부터 배경음.
	ReleaseStageAmbience();
	OnIntroFinished.Broadcast();
}

void UFF_TitleIntroScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	PhaseSeconds += InDeltaTime;

	if (Phase != EPhase::Idle && Phase != EPhase::Finished)
	{
		HoldStageAmbience();
	}

	switch (Phase)
	{
	case EPhase::Splash:
	{
		const float FadeOutStart = SplashFadeInSeconds + SplashHoldSeconds;
		const float Opacity = PhaseSeconds < FadeOutStart
			? FFTitleIntro::Progress(PhaseSeconds, SplashFadeInSeconds)
			: 1.f - FFTitleIntro::Progress(PhaseSeconds - FadeOutStart, SplashFadeOutSeconds);
		SetOpacity(Panel_Splash, Opacity);
		if (PhaseSeconds >= FadeOutStart + SplashFadeOutSeconds)
		{
			EnterVideo();
		}
		break;
	}
	case EPhase::Video:
		// 열기에 성공했다는 신호가 끝내 안 오면 기다리지 않는다.
		if (!bVideoStarted && PhaseSeconds > VideoOpenTimeoutSeconds)
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("[%s] 인트로 영상이 %.1f초 안에 열리지 않아 건너뛴다."), *GetName(), VideoOpenTimeoutSeconds);
			EnterReveal();
		}
		break;
	case EPhase::Reveal:
	{
		const UFF_UIAccessibilitySubsystem* Access = UFF_UIAccessibilitySubsystem::Get(this);
		const float FadeSeconds = Access && Access->IsReduceMotionEnabled() ? 0.f : RevealFadeSeconds;
		SetOpacity(Image_FlashCover, 1.f - FFTitleIntro::Progress(PhaseSeconds, FadeSeconds));
		if (Image_FlashCover && PhaseSeconds >= FadeSeconds)
		{
			Image_FlashCover->SetVisibility(ESlateVisibility::Collapsed);
		}
		// 가운데 지구를 잠깐 보여 준 뒤 스스로 메뉴로 간다. 글자가 없으니 기다리게 하지 않는다.
		if (PhaseSeconds >= FadeSeconds + RevealHoldSeconds)
		{
			Finish();
		}
		break;
	}
	default:
		break;
	}
}

// --- 입력 ---

void UFF_TitleIntroScreen::RequestSkip()
{
	HandleAnyInput();
}

void UFF_TitleIntroScreen::HandleAnyInput()
{
	switch (Phase)
	{
	case EPhase::Splash:
		if (PhaseSeconds >= SplashMinSecondsBeforeSkip)
		{
			EnterVideo();
		}
		break;
	case EPhase::Video:
		FadeOutIntroSound(SkipSoundFadeSeconds);
		EnterReveal();
		break;
	case EPhase::Reveal:
		// 영상 넘기기 키를 길게 눌러 곧바로 메뉴까지 가 버리지 않게 잠깐 막는다.
		if (PhaseSeconds > 0.3f)
		{
			Finish();
		}
		break;
	default:
		break;
	}
}

FReply UFF_TitleIntroScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!InKeyEvent.IsRepeat())
	{
		HandleAnyInput();
	}
	return FReply::Handled();
}

FReply UFF_TitleIntroScreen::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	HandleAnyInput();
	return FReply::Handled();
}

bool UFF_TitleIntroScreen::NativeOnHandleBackAction()
{
	// 기본 동작은 자기를 내리는 것이다. 그러면 스택이 비어 화면이 통째로 사라진다.
	HandleAnyInput();
	return true;
}

// --- 영상·소리 ---

void UFF_TitleIntroScreen::HandleMediaOpened(FString OpenedUrl)
{
	bVideoStarted = true;
	PlayIntroSound();
	UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 인트로 영상 재생 (%s, %.2f초)."), *GetName(), *OpenedUrl,
		MediaPlayer ? MediaPlayer->GetDuration().GetTotalSeconds() : 0.0);
}

void UFF_TitleIntroScreen::HandleMediaOpenFailed(FString FailedUrl)
{
	UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] 인트로 영상 열기 실패: %s"), *GetName(), *FailedUrl);
	if (Phase == EPhase::Video)
	{
		EnterReveal();
	}
}

void UFF_TitleIntroScreen::HandleMediaEndReached()
{
	if (Phase == EPhase::Video)
	{
		EnterReveal();
	}
}

void UFF_TitleIntroScreen::PlayIntroSound()
{
	USoundBase* Sound = IntroSound.LoadSynchronous();
	if (!Sound)
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] 인트로 소리가 없어 소리 없이 간다: %s"), *GetName(),
			*IntroSound.ToString());
		return;
	}

	// 설정 창의 배경음 비율을 따른다. 마스터는 엔진이 이미 곱한다.
	float Volume = IntroSoundVolume;
	if (const UFF_AudioSubsystem* Audio = UFF_AudioSubsystem::Get(this))
	{
		if (!Audio->IsAudioEnabled())
		{
			return;
		}
		Volume *= Audio->GetChannelVolume(EFF_AudioChannel::Music);
	}
	FadeOutIntroSound(0.f);
	IntroAudio = UGameplayStatics::SpawnSound2D(this, Sound, Volume);
}

void UFF_TitleIntroScreen::HoldStageAmbience()
{
	UFF_AudioSubsystem* Audio = UFF_AudioSubsystem::Get(this);
	if (!Audio || Audio->GetCurrentAmbience() == EFF_AudioAmbience::None)
	{
		return;
	}
	HeldAmbience = Audio->GetCurrentAmbience();
	Audio->StopAmbience();
	UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 인트로 동안 배경음을 멈춰 둔다 (%s)."), *GetName(),
		*UEnum::GetValueAsString(HeldAmbience));
}

void UFF_TitleIntroScreen::ReleaseStageAmbience()
{
	if (HeldAmbience == EFF_AudioAmbience::None)
	{
		return;
	}
	const EFF_AudioAmbience Resume = HeldAmbience;
	HeldAmbience = EFF_AudioAmbience::None;
	if (UFF_AudioSubsystem* Audio = UFF_AudioSubsystem::Get(this))
	{
		// 그 사이 다른 곡이 걸렸으면(단계가 바뀌었으면) 그쪽을 존중한다.
		if (Audio->GetCurrentAmbience() == EFF_AudioAmbience::None)
		{
			Audio->SetAmbience(Resume);
		}
	}
	UE_LOG(LogFestaFiestaUI, Log, TEXT("[%s] 배경음 다시 시작 (%s)."), *GetName(), *UEnum::GetValueAsString(Resume));
}

void UFF_TitleIntroScreen::FadeOutIntroSound(const float Seconds)
{
	if (UAudioComponent* Audio = IntroAudio.Get(); Audio && Audio->IsPlaying())
	{
		if (Seconds > 0.f)
		{
			Audio->FadeOut(Seconds, 0.f);
		}
		else
		{
			Audio->Stop();
		}
	}
}

void UFF_TitleIntroScreen::CloseMedia()
{
	if (MediaPlayer && !MediaPlayer->GetUrl().IsEmpty())
	{
		MediaPlayer->Close();
	}
}

void UFF_TitleIntroScreen::SetOpacity(UWidget* Widget, const float Opacity)
{
	if (Widget)
	{
		Widget->SetRenderOpacity(FMath::Clamp(Opacity, 0.f, 1.f));
	}
}

FString UFF_TitleIntroScreen::DescribeState() const
{
	return FString::Printf(TEXT("Phase=%s t=%.2f Video=%s Sound=%s"),
		FFTitleIntro::PhaseName(static_cast<uint8>(Phase)), PhaseSeconds,
		MediaPlayer && MediaPlayer->IsPlaying() ? TEXT("재생 중") : (bVideoStarted ? TEXT("열림") : TEXT("없음")),
		IntroAudio && IntroAudio->IsPlaying() ? TEXT("재생 중") : TEXT("없음"));
}

// =============================================================================
// 콘솔 명령 — 개발용
// =============================================================================
#if !UE_BUILD_SHIPPING

namespace FFTitleIntroConsole
{
	void ForEachActive(UWorld* World, TFunctionRef<void(UFF_TitleIntroScreen&)> Visit)
	{
		for (TObjectIterator<UFF_TitleIntroScreen> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsActivated())
			{
				Visit(**It);
			}
		}
	}

	void Skip(const TArray<FString>& Args, UWorld* World)
	{
		ForEachActive(World, [](UFF_TitleIntroScreen& Screen)
		{
			Screen.RequestSkip();
			UE_LOG(LogFestaFiestaUI, Display, TEXT("FF.Intro.Skip: %s"), *Screen.DescribeState());
		});
	}

	void Status(const TArray<FString>& Args, UWorld* World)
	{
		ForEachActive(World, [](UFF_TitleIntroScreen& Screen)
		{
			UE_LOG(LogFestaFiestaUI, Display, TEXT("FF.Intro.Status: %s"), *Screen.DescribeState());
		});
	}

	FAutoConsoleCommandWithWorldAndArgs SkipCommand(
		TEXT("FF.Intro.Skip"),
		TEXT("인트로의 지금 단계를 아무 키를 누른 것처럼 넘긴다 (시작 화면 → 영상 → 지구 → 타이틀)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Skip));

	FAutoConsoleCommandWithWorldAndArgs StatusCommand(
		TEXT("FF.Intro.Status"),
		TEXT("인트로 화면이 지금 어느 단계인지 찍는다."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Status));
}

#endif // !UE_BUILD_SHIPPING
