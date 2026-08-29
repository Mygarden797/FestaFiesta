// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_FestivalSettingsScreen.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/Core/FF_UIAccessibilitySubsystem.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UILog.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"
#include "UI/Settings/FF_RuntimeSettingsSubsystem.h"
#include "UI/Settings/FF_SettingSelectRow.h"
#include "UI/Settings/FF_SettingSliderRow.h"
#include "UI/Settings/FF_SettingToggleRow.h"

#define LOCTEXT_NAMESPACE "FestaFiestaFestivalSettings"

namespace
{
	const TCHAR* ToggleRowFallbackPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_SettingToggleRow.WBP_FF_SettingToggleRow_C");
	const TCHAR* SliderRowFallbackPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_SettingSliderRow.WBP_FF_SettingSliderRow_C");
	const TCHAR* SelectRowFallbackPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_SettingSelectRow.WBP_FF_SettingSelectRow_C");

	const FName Setting_WindowMode(TEXT("WindowMode"));
	const FName Setting_Resolution(TEXT("Resolution"));
	const FName Setting_ResolutionScale(TEXT("ResolutionScale"));
	const FName Setting_FrameRateLimit(TEXT("FrameRateLimit"));
	const FName Setting_GraphicsPreset(TEXT("GraphicsPreset"));
	const FName Setting_ViewDistance(TEXT("ViewDistance"));
	const FName Setting_AntiAliasing(TEXT("AntiAliasing"));
	const FName Setting_Shadow(TEXT("Shadow"));
	const FName Setting_GlobalIllumination(TEXT("GlobalIllumination"));
	const FName Setting_Reflection(TEXT("Reflection"));
	const FName Setting_PostProcessing(TEXT("PostProcessing"));
	const FName Setting_Texture(TEXT("Texture"));
	const FName Setting_Effects(TEXT("Effects"));
	const FName Setting_Foliage(TEXT("Foliage"));
	const FName Setting_Shading(TEXT("Shading"));

	FText FormatResolution(const FIntPoint& Resolution)
	{
		return FText::Format(LOCTEXT("ResolutionFormat", "{0} × {1}"),
			FText::AsNumber(Resolution.X), FText::AsNumber(Resolution.Y));
	}

	FText FormatPercent(const int32 Percent)
	{
		return FText::Format(LOCTEXT("PercentFormat", "{0}%"), FText::AsNumber(Percent));
	}
}

UFF_FestivalSettingsScreen::UFF_FestivalSettingsScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ScreenInputMode = EFF_ScreenInputMode::Menu;
	bShowMouseCursor = true;

	DisplayCategoryLabel = LOCTEXT("CategoryDisplay", "화면");
	AudioCategoryLabel = LOCTEXT("CategoryAudio", "오디오");
	GameplayCategoryLabel = LOCTEXT("CategoryGameplay", "게임플레이");
	AccessibilityCategoryLabel = LOCTEXT("CategoryAccessibility", "접근성");

	WindowModeLabel = LOCTEXT("WindowMode", "화면 모드");
	WindowModeDescription = LOCTEXT("WindowModeDesc", "전체 화면, 테두리 없는 창, 창 모드를 선택합니다");
	ResolutionLabel = LOCTEXT("Resolution", "해상도");
	ResolutionDescription = LOCTEXT("ResolutionDesc", "모니터에서 지원하는 출력 해상도를 선택합니다");
	ResolutionScaleLabel = LOCTEXT("ResolutionScale", "렌더링 해상도");
	ResolutionScaleDescription = LOCTEXT("ResolutionScaleDesc", "UI는 선명하게 유지하고 3D 장면의 렌더링 비율을 조절합니다");
	VSyncLabel = LOCTEXT("VSync", "수직 동기화");
	VSyncDescription = LOCTEXT("VSyncDesc", "화면 찢어짐을 줄입니다");
	DynamicResolutionLabel = LOCTEXT("DynamicResolution", "동적 해상도");
	DynamicResolutionDescription = LOCTEXT("DynamicResolutionDesc", "부하가 높을 때 렌더링 해상도를 자동 조절해 프레임을 유지합니다");
	FrameRateLimitLabel = LOCTEXT("FrameRateLimit", "최대 프레임");
	FrameRateLimitDescription = LOCTEXT("FrameRateLimitDesc", "GPU 사용량과 발열을 제어할 최대 FPS입니다");
	GraphicsPresetLabel = LOCTEXT("GraphicsPreset", "그래픽 품질 프리셋");
	GraphicsPresetDescription = LOCTEXT("GraphicsPresetDesc", "모든 세부 품질을 한 번에 설정합니다");
	ViewDistanceQualityLabel = LOCTEXT("ViewDistanceQuality", "가시 거리");
	ViewDistanceQualityDescription = LOCTEXT("ViewDistanceQualityDesc", "멀리 있는 오브젝트가 표시되는 거리를 조절합니다");
	AntiAliasingQualityLabel = LOCTEXT("AntiAliasingQuality", "안티앨리어싱");
	AntiAliasingQualityDescription = LOCTEXT("AntiAliasingQualityDesc", "오브젝트 가장자리의 계단 현상을 줄입니다");
	ShadowQualityLabel = LOCTEXT("ShadowQuality", "그림자");
	ShadowQualityDescription = LOCTEXT("ShadowQualityDesc", "그림자의 해상도와 표시 거리를 조절합니다");
	GlobalIlluminationQualityLabel = LOCTEXT("GlobalIlluminationQuality", "전역 조명");
	GlobalIlluminationQualityDescription = LOCTEXT("GlobalIlluminationQualityDesc", "간접광과 장면 조명 품질을 조절합니다");
	ReflectionQualityLabel = LOCTEXT("ReflectionQuality", "반사");
	ReflectionQualityDescription = LOCTEXT("ReflectionQualityDesc", "물과 반사 표면의 표현 품질을 조절합니다");
	PostProcessingQualityLabel = LOCTEXT("PostProcessingQuality", "후처리");
	PostProcessingQualityDescription = LOCTEXT("PostProcessingQualityDesc", "블룸, 피사계 심도 등 화면 효과 품질입니다");
	TextureQualityLabel = LOCTEXT("TextureQuality", "텍스처");
	TextureQualityDescription = LOCTEXT("TextureQualityDesc", "표면 이미지의 선명도와 메모리 사용량을 조절합니다");
	EffectsQualityLabel = LOCTEXT("EffectsQuality", "이펙트");
	EffectsQualityDescription = LOCTEXT("EffectsQualityDesc", "파티클과 시각 효과의 품질을 조절합니다");
	FoliageQualityLabel = LOCTEXT("FoliageQuality", "식생");
	FoliageQualityDescription = LOCTEXT("FoliageQualityDesc", "나무와 풀의 밀도 및 표시 거리를 조절합니다");
	ShadingQualityLabel = LOCTEXT("ShadingQuality", "셰이딩");
	ShadingQualityDescription = LOCTEXT("ShadingQualityDesc", "재질과 표면 음영의 세부 품질을 조절합니다");
	WindowModeOptionLabels = {
		LOCTEXT("WindowModeBorderless", "테두리 없는 창"),
		LOCTEXT("WindowModeFullscreen", "전체 화면"),
		LOCTEXT("WindowModeWindowed", "창 모드")
	};
	QualityLevelOptionLabels = {
		LOCTEXT("QualityLow", "낮음"), LOCTEXT("QualityMedium", "중간"),
		LOCTEXT("QualityHigh", "높음"), LOCTEXT("QualityEpic", "매우 높음"),
		LOCTEXT("QualityCinematic", "시네마틱")
	};
	CustomQualityLabel = LOCTEXT("QualityCustom", "사용자 지정");
	UnlimitedFrameRateLabel = LOCTEXT("FrameRateUnlimited", "무제한");
	MasterVolumeLabel = LOCTEXT("MasterVolume", "마스터 음량");
	MasterVolumeDescription = LOCTEXT("MasterVolumeDesc", "게임 전체 소리의 크기를 조절합니다");
	ZoomToCursorLabel = LOCTEXT("ZoomToCursor", "커서 기준 줌");
	ZoomToCursorDescription = LOCTEXT("ZoomToCursorDesc", "커서 아래 지점을 향해 확대합니다");
	EdgeScrollLabel = LOCTEXT("EdgeScroll", "가장자리 스크롤");
	EdgeScrollDescription = LOCTEXT("EdgeScrollDesc", "커서를 화면 끝에 대면 카메라가 이동합니다");
	ReduceMotionLabel = LOCTEXT("ReduceMotion", "움직임 감소");
	ReduceMotionDescription = LOCTEXT("ReduceMotionDesc", "화면 전환과 패널 애니메이션을 줄입니다");
}

void UFF_FestivalSettingsScreen::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Display)
	{
		Button_Display->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleDisplayClicked);
	}
	if (Button_Audio)
	{
		Button_Audio->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleAudioClicked);
	}
	if (Button_Gameplay)
	{
		Button_Gameplay->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleGameplayClicked);
	}
	if (Button_Accessibility)
	{
		Button_Accessibility->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleAccessibilityClicked);
	}
	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleCloseClicked);
	}
	if (Button_Reset)
	{
		Button_Reset->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleResetClicked);
	}
	if (Button_Done)
	{
		Button_Done->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleCloseClicked);
	}
}

void UFF_FestivalSettingsScreen::NativeDestruct()
{
	if (Button_Display)
	{
		Button_Display->OnClicked.RemoveDynamic(this, &UFF_FestivalSettingsScreen::HandleDisplayClicked);
	}
	if (Button_Audio)
	{
		Button_Audio->OnClicked.RemoveDynamic(this, &UFF_FestivalSettingsScreen::HandleAudioClicked);
	}
	if (Button_Gameplay)
	{
		Button_Gameplay->OnClicked.RemoveDynamic(this, &UFF_FestivalSettingsScreen::HandleGameplayClicked);
	}
	if (Button_Accessibility)
	{
		Button_Accessibility->OnClicked.RemoveDynamic(this, &UFF_FestivalSettingsScreen::HandleAccessibilityClicked);
	}
	if (Button_Close)
	{
		Button_Close->OnClicked.RemoveDynamic(this, &UFF_FestivalSettingsScreen::HandleCloseClicked);
	}
	if (Button_Reset)
	{
		Button_Reset->OnClicked.RemoveDynamic(this, &UFF_FestivalSettingsScreen::HandleResetClicked);
	}
	if (Button_Done)
	{
		Button_Done->OnClicked.RemoveDynamic(this, &UFF_FestivalSettingsScreen::HandleCloseClicked);
	}

	Super::NativeDestruct();
}

void UFF_FestivalSettingsScreen::NativeOnActivated()
{
	SelectCategory(DefaultCategory);
	Super::NativeOnActivated();
}

UWidget* UFF_FestivalSettingsScreen::NativeGetDesiredFocusTarget() const
{
	if (UButton* SelectedButton = GetCategoryButton(SelectedCategory))
	{
		return SelectedButton;
	}
	return Super::NativeGetDesiredFocusTarget();
}

FReply UFF_FestivalSettingsScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const UFF_FestivalSettingsDeveloperSettings* Settings = UFF_FestivalSettingsDeveloperSettings::Get();
	if (Settings && Settings->OpenCloseKeys.Contains(InKeyEvent.GetKey()))
	{
		OnCloseRequested.Broadcast();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UFF_FestivalSettingsScreen::SelectCategory(EFF_FestivalSettingsCategory Category)
{
	SelectedCategory = Category;
	if (Text_CategoryTitle)
	{
		Text_CategoryTitle->SetText(GetCategoryLabel(Category));
	}
	RefreshCategoryVisuals();
	RebuildRows();
}

void UFF_FestivalSettingsScreen::RefreshCategoryVisuals()
{
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		return;
	}

	const EFF_FestivalSettingsCategory Categories[] = {
		EFF_FestivalSettingsCategory::Display,
		EFF_FestivalSettingsCategory::Audio,
		EFF_FestivalSettingsCategory::Gameplay,
		EFF_FestivalSettingsCategory::Accessibility
	};

	for (EFF_FestivalSettingsCategory Category : Categories)
	{
		const bool bSelected = Category == SelectedCategory;
		if (UButton* Button = GetCategoryButton(Category))
		{
			Button->SetBackgroundColor(bSelected ? Palette->InGame.TealDeep : Palette->InGame.White);
		}
		if (UTextBlock* Text = GetCategoryText(Category))
		{
			Text->SetText(GetCategoryLabel(Category));
			Text->SetColorAndOpacity(FSlateColor(bSelected ? Palette->InGame.White : Palette->InGame.Ink));
		}
	}
}

void UFF_FestivalSettingsScreen::RebuildRows()
{
	if (!VerticalBox_Rows)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_FestivalSettingsScreen: VerticalBox_Rows 바인딩이 없어 설정을 그릴 수 없다."));
		return;
	}

	VerticalBox_Rows->ClearChildren();
	QualityPresetRow.Reset();
	UFF_RuntimeSettingsSubsystem* RuntimeSettings = UFF_RuntimeSettingsSubsystem::Get(this);

	switch (SelectedCategory)
	{
	case EFF_FestivalSettingsCategory::Display:
		if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			BuildDisplayRows(*UserSettings);
		}
		break;

	case EFF_FestivalSettingsCategory::Audio:
		if (RuntimeSettings)
		{
			if (UFF_SettingSliderRow* Row = AddSliderRow(MasterVolumeLabel, MasterVolumeDescription,
				RuntimeSettings->GetMasterVolume(), 0.f, 1.f, 0.05f))
			{
				Row->OnValueChanged.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleMasterVolumeChanged);
			}
		}
		break;

	case EFF_FestivalSettingsCategory::Gameplay:
		if (RuntimeSettings)
		{
			if (UFF_SettingToggleRow* Row = AddToggleRow(ZoomToCursorLabel, ZoomToCursorDescription,
				RuntimeSettings->IsZoomToCursorEnabled()))
			{
				Row->OnValueChanged.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleZoomToCursorToggled);
			}
			if (UFF_SettingToggleRow* Row = AddToggleRow(EdgeScrollLabel, EdgeScrollDescription,
				RuntimeSettings->IsEdgeScrollEnabled()))
			{
				Row->OnValueChanged.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleEdgeScrollToggled);
			}
		}
		break;

	case EFF_FestivalSettingsCategory::Accessibility:
		if (UFF_UIAccessibilitySubsystem* Accessibility = UFF_UIAccessibilitySubsystem::Get(this))
		{
			if (UFF_SettingToggleRow* Row = AddToggleRow(ReduceMotionLabel, ReduceMotionDescription,
				Accessibility->IsReduceMotionEnabled()))
			{
				Row->OnValueChanged.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleReduceMotionToggled);
			}
		}
		break;
	}

	RefreshCurrentSummary();
}

void UFF_FestivalSettingsScreen::BuildDisplayRows(UGameUserSettings& UserSettings)
{
	BuildResolutionOptions(UserSettings);
	BuildResolutionScaleOptions(UserSettings);
	BuildFrameRateOptions(UserSettings);

	TArray<FText> WindowModes = WindowModeOptionLabels;
	if (WindowModes.Num() < 3)
	{
		WindowModes = {
			LOCTEXT("FallbackBorderless", "테두리 없는 창"),
			LOCTEXT("FallbackFullscreen", "전체 화면"),
			LOCTEXT("FallbackWindowed", "창 모드")
		};
	}
	else if (WindowModes.Num() > 3)
	{
		WindowModes.SetNum(3);
	}

	int32 WindowModeIndex = 0;
	switch (UserSettings.GetFullscreenMode())
	{
	case EWindowMode::Fullscreen: WindowModeIndex = 1; break;
	case EWindowMode::Windowed: WindowModeIndex = 2; break;
	case EWindowMode::WindowedFullscreen:
	default: WindowModeIndex = 0; break;
	}
	AddSelectRow(Setting_WindowMode, WindowModeLabel, WindowModeDescription, WindowModes, WindowModeIndex);

	TArray<FText> ResolutionLabels;
	ResolutionLabels.Reserve(ResolutionOptions.Num());
	for (const FIntPoint& Resolution : ResolutionOptions)
	{
		ResolutionLabels.Add(FormatResolution(Resolution));
	}
	AddSelectRow(Setting_Resolution, ResolutionLabel, ResolutionDescription, ResolutionLabels,
		FMath::Max(0, ResolutionOptions.IndexOfByKey(UserSettings.GetScreenResolution())));

	float CurrentScaleNormalized = 1.f;
	float CurrentScaleValue = 100.f;
	float MinScaleValue = 50.f;
	float MaxScaleValue = 100.f;
	UserSettings.GetResolutionScaleInformationEx(
		CurrentScaleNormalized, CurrentScaleValue, MinScaleValue, MaxScaleValue);
	const int32 CurrentScalePercent = FMath::RoundToInt(CurrentScaleValue);
	TArray<FText> ResolutionScaleLabels;
	ResolutionScaleLabels.Reserve(ResolutionScaleOptions.Num());
	for (const int32 Percent : ResolutionScaleOptions)
	{
		ResolutionScaleLabels.Add(FormatPercent(Percent));
	}
	AddSelectRow(Setting_ResolutionScale, ResolutionScaleLabel, ResolutionScaleDescription,
		ResolutionScaleLabels, FMath::Max(0, ResolutionScaleOptions.IndexOfByKey(CurrentScalePercent)));

	if (UFF_SettingToggleRow* Row = AddToggleRow(VSyncLabel, VSyncDescription, UserSettings.IsVSyncEnabled()))
	{
		Row->OnValueChanged.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleVSyncToggled);
	}
	if (UFF_SettingToggleRow* Row = AddToggleRow(DynamicResolutionLabel, DynamicResolutionDescription,
		UserSettings.IsDynamicResolutionEnabled()))
	{
		Row->OnValueChanged.AddUniqueDynamic(
			this, &UFF_FestivalSettingsScreen::HandleDynamicResolutionToggled);
	}

	TArray<FText> FrameRateLabels;
	FrameRateLabels.Reserve(FrameRateOptions.Num());
	for (const int32 Limit : FrameRateOptions)
	{
		FrameRateLabels.Add(Limit <= 0
			? UnlimitedFrameRateLabel
			: FText::Format(LOCTEXT("FrameRateFormat", "{0} FPS"), FText::AsNumber(Limit)));
	}
	const int32 CurrentFrameRate = UserSettings.GetFrameRateLimit() <= 0.f
		? 0 : FMath::RoundToInt(UserSettings.GetFrameRateLimit());
	AddSelectRow(Setting_FrameRateLimit, FrameRateLimitLabel, FrameRateLimitDescription,
		FrameRateLabels, FMath::Max(0, FrameRateOptions.IndexOfByKey(CurrentFrameRate)));

	const TArray<FText> PresetOptions = GetQualityLevelOptions(true);
	const int32 OverallQuality = UserSettings.GetOverallScalabilityLevel();
	QualityPresetRow = AddSelectRow(Setting_GraphicsPreset, GraphicsPresetLabel, GraphicsPresetDescription,
		PresetOptions, OverallQuality >= 0 && OverallQuality <= 4 ? OverallQuality : 5);

	const TArray<FText> QualityOptions = GetQualityLevelOptions(false);
	AddSelectRow(Setting_ViewDistance, ViewDistanceQualityLabel, ViewDistanceQualityDescription,
		QualityOptions, FMath::Clamp(UserSettings.GetViewDistanceQuality(), 0, 4));
	AddSelectRow(Setting_AntiAliasing, AntiAliasingQualityLabel, AntiAliasingQualityDescription,
		QualityOptions, FMath::Clamp(UserSettings.GetAntiAliasingQuality(), 0, 4));
	AddSelectRow(Setting_Shadow, ShadowQualityLabel, ShadowQualityDescription,
		QualityOptions, FMath::Clamp(UserSettings.GetShadowQuality(), 0, 4));
	AddSelectRow(Setting_GlobalIllumination, GlobalIlluminationQualityLabel, GlobalIlluminationQualityDescription,
		QualityOptions, FMath::Clamp(UserSettings.GetGlobalIlluminationQuality(), 0, 4));
	AddSelectRow(Setting_Reflection, ReflectionQualityLabel, ReflectionQualityDescription,
		QualityOptions, FMath::Clamp(UserSettings.GetReflectionQuality(), 0, 4));
	AddSelectRow(Setting_PostProcessing, PostProcessingQualityLabel, PostProcessingQualityDescription,
		QualityOptions, FMath::Clamp(UserSettings.GetPostProcessingQuality(), 0, 4));
	AddSelectRow(Setting_Texture, TextureQualityLabel, TextureQualityDescription,
		QualityOptions, FMath::Clamp(UserSettings.GetTextureQuality(), 0, 4));
	AddSelectRow(Setting_Effects, EffectsQualityLabel, EffectsQualityDescription,
		QualityOptions, FMath::Clamp(UserSettings.GetVisualEffectQuality(), 0, 4));
	AddSelectRow(Setting_Foliage, FoliageQualityLabel, FoliageQualityDescription,
		QualityOptions, FMath::Clamp(UserSettings.GetFoliageQuality(), 0, 4));
	AddSelectRow(Setting_Shading, ShadingQualityLabel, ShadingQualityDescription,
		QualityOptions, FMath::Clamp(UserSettings.GetShadingQuality(), 0, 4));
}

void UFF_FestivalSettingsScreen::BuildResolutionOptions(const UGameUserSettings& UserSettings)
{
	ResolutionOptions.Reset();
	const UFF_FestivalSettingsDeveloperSettings* Settings = UFF_FestivalSettingsDeveloperSettings::Get();
	const int32 MinimumWidth = Settings ? Settings->MinimumResolutionWidth : 1024;
	const int32 MinimumHeight = Settings ? Settings->MinimumResolutionHeight : 576;

	auto AddResolution = [this, MinimumWidth, MinimumHeight](const FIntPoint& Resolution, bool bForce)
	{
		if (Resolution.X <= 0 || Resolution.Y <= 0 ||
			(!bForce && (Resolution.X < MinimumWidth || Resolution.Y < MinimumHeight)))
		{
			return;
		}
		ResolutionOptions.AddUnique(Resolution);
	};

	TArray<FIntPoint> DetectedResolutions;
	if (UKismetSystemLibrary::GetSupportedFullscreenResolutions(DetectedResolutions))
	{
		for (const FIntPoint& Resolution : DetectedResolutions)
		{
			AddResolution(Resolution, false);
		}
	}
	DetectedResolutions.Reset();
	if (UKismetSystemLibrary::GetConvenientWindowedResolutions(DetectedResolutions))
	{
		for (const FIntPoint& Resolution : DetectedResolutions)
		{
			AddResolution(Resolution, false);
		}
	}
	if (Settings)
	{
		for (const FIntPoint& Resolution : Settings->FallbackResolutions)
		{
			AddResolution(Resolution, false);
		}
	}
	AddResolution(UserSettings.GetScreenResolution(), true);

	ResolutionOptions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		const int64 PixelsA = static_cast<int64>(A.X) * A.Y;
		const int64 PixelsB = static_cast<int64>(B.X) * B.Y;
		return PixelsA == PixelsB ? A.X < B.X : PixelsA < PixelsB;
	});
}

void UFF_FestivalSettingsScreen::BuildResolutionScaleOptions(const UGameUserSettings& UserSettings)
{
	ResolutionScaleOptions.Reset();
	float CurrentNormalized = 1.f;
	float CurrentValue = 100.f;
	float MinimumValue = 50.f;
	float MaximumValue = 100.f;
	UserSettings.GetResolutionScaleInformationEx(CurrentNormalized, CurrentValue, MinimumValue, MaximumValue);

	if (const UFF_FestivalSettingsDeveloperSettings* Settings = UFF_FestivalSettingsDeveloperSettings::Get())
	{
		for (const int32 Percent : Settings->ResolutionScalePercentOptions)
		{
			if (Percent >= FMath::FloorToInt(MinimumValue) && Percent <= FMath::CeilToInt(MaximumValue))
			{
				ResolutionScaleOptions.AddUnique(Percent);
			}
		}
	}
	ResolutionScaleOptions.AddUnique(FMath::RoundToInt(CurrentValue));
	ResolutionScaleOptions.Sort();
}

void UFF_FestivalSettingsScreen::BuildFrameRateOptions(const UGameUserSettings& UserSettings)
{
	FrameRateOptions.Reset();
	if (const UFF_FestivalSettingsDeveloperSettings* Settings = UFF_FestivalSettingsDeveloperSettings::Get())
	{
		for (const int32 Limit : Settings->FrameRateLimitOptions)
		{
			FrameRateOptions.AddUnique(FMath::Max(0, Limit));
		}
	}
	const int32 CurrentLimit = UserSettings.GetFrameRateLimit() <= 0.f
		? 0 : FMath::RoundToInt(UserSettings.GetFrameRateLimit());
	FrameRateOptions.AddUnique(CurrentLimit);
	FrameRateOptions.Sort();
}

UFF_SettingToggleRow* UFF_FestivalSettingsScreen::AddToggleRow(
	const FText& Label, const FText& Description, bool bInitialValue)
{
	const TSubclassOf<UFF_SettingToggleRow> RowClass = ResolveToggleRowClass();
	UFF_SettingToggleRow* Row = RowClass ? CreateWidget<UFF_SettingToggleRow>(this, RowClass) : nullptr;
	if (!Row)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_FestivalSettingsScreen: 토글 행을 만들지 못했다. ToggleRowClass를 확인한다."));
		return nullptr;
	}
	Row->Configure(Label, Description, bInitialValue);
	VerticalBox_Rows->AddChild(Row);
	return Row;
}

UFF_SettingSliderRow* UFF_FestivalSettingsScreen::AddSliderRow(const FText& Label, const FText& Description,
	float InitialValue, float MinValue, float MaxValue, float StepSize)
{
	const TSubclassOf<UFF_SettingSliderRow> RowClass = ResolveSliderRowClass();
	UFF_SettingSliderRow* Row = RowClass ? CreateWidget<UFF_SettingSliderRow>(this, RowClass) : nullptr;
	if (!Row)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_FestivalSettingsScreen: 슬라이더 행을 만들지 못했다. SliderRowClass를 확인한다."));
		return nullptr;
	}
	Row->Configure(Label, Description, InitialValue, MinValue, MaxValue, StepSize, true);
	VerticalBox_Rows->AddChild(Row);
	return Row;
}

UFF_SettingSelectRow* UFF_FestivalSettingsScreen::AddSelectRow(FName SettingId, const FText& Label,
	const FText& Description, const TArray<FText>& Options, int32 InitialIndex)
{
	const TSubclassOf<UFF_SettingSelectRow> RowClass = ResolveSelectRowClass();
	UFF_SettingSelectRow* Row = RowClass ? CreateWidget<UFF_SettingSelectRow>(this, RowClass) : nullptr;
	if (!Row)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_FestivalSettingsScreen: 선택 행을 만들지 못했다. SelectRowClass를 확인한다."));
		return nullptr;
	}

	Row->Configure(SettingId, Label, Description, Options, InitialIndex);
	Row->OnSelectionChanged.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleSelectionChanged);
	VerticalBox_Rows->AddChild(Row);
	return Row;
}

TSubclassOf<UFF_SettingToggleRow> UFF_FestivalSettingsScreen::ResolveToggleRowClass() const
{
	if (ToggleRowClass)
	{
		return ToggleRowClass;
	}
	return TSubclassOf<UFF_SettingToggleRow>(
		LoadClass<UFF_SettingToggleRow>(nullptr, ToggleRowFallbackPath));
}

TSubclassOf<UFF_SettingSliderRow> UFF_FestivalSettingsScreen::ResolveSliderRowClass() const
{
	if (SliderRowClass)
	{
		return SliderRowClass;
	}
	return TSubclassOf<UFF_SettingSliderRow>(
		LoadClass<UFF_SettingSliderRow>(nullptr, SliderRowFallbackPath));
}

TSubclassOf<UFF_SettingSelectRow> UFF_FestivalSettingsScreen::ResolveSelectRowClass() const
{
	if (SelectRowClass)
	{
		return SelectRowClass;
	}
	return TSubclassOf<UFF_SettingSelectRow>(
		LoadClass<UFF_SettingSelectRow>(nullptr, SelectRowFallbackPath));
}

TArray<FText> UFF_FestivalSettingsScreen::GetQualityLevelOptions(bool bIncludeCustom) const
{
	TArray<FText> Options = QualityLevelOptionLabels;
	if (Options.Num() < 5)
	{
		Options = {
			LOCTEXT("FallbackQualityLow", "낮음"), LOCTEXT("FallbackQualityMedium", "중간"),
			LOCTEXT("FallbackQualityHigh", "높음"), LOCTEXT("FallbackQualityEpic", "매우 높음"),
			LOCTEXT("FallbackQualityCinematic", "시네마틱")
		};
	}
	else if (Options.Num() > 5)
	{
		Options.SetNum(5);
	}
	if (bIncludeCustom)
	{
		Options.Add(CustomQualityLabel.IsEmpty()
			? LOCTEXT("FallbackQualityCustom", "사용자 지정") : CustomQualityLabel);
	}
	return Options;
}

void UFF_FestivalSettingsScreen::RefreshCurrentSummary()
{
	if (!Text_CurrentSummary)
	{
		return;
	}

	const UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	const UFF_RuntimeSettingsSubsystem* RuntimeSettings = UFF_RuntimeSettingsSubsystem::Get(this);
	if (!UserSettings || !RuntimeSettings)
	{
		Text_CurrentSummary->SetText(FText::GetEmpty());
		return;
	}

	const FIntPoint Resolution = UserSettings->GetScreenResolution();
	FText WindowMode;
	switch (UserSettings->GetFullscreenMode())
	{
	case EWindowMode::Fullscreen: WindowMode = LOCTEXT("SummaryFullscreen", "전체 화면"); break;
	case EWindowMode::Windowed: WindowMode = LOCTEXT("SummaryWindowed", "창 모드"); break;
	case EWindowMode::WindowedFullscreen:
	default: WindowMode = LOCTEXT("SummaryBorderless", "테두리 없는 창"); break;
	}
	const int32 OverallQuality = UserSettings->GetOverallScalabilityLevel();
	const TArray<FText> QualityOptions = GetQualityLevelOptions(false);
	const FText Quality = QualityOptions.IsValidIndex(OverallQuality)
		? QualityOptions[OverallQuality]
		: (CustomQualityLabel.IsEmpty() ? LOCTEXT("SummaryCustom", "사용자 지정") : CustomQualityLabel);
	const int32 FrameRate = UserSettings->GetFrameRateLimit() <= 0.f
		? 0 : FMath::RoundToInt(UserSettings->GetFrameRateLimit());
	const FText FrameRateText = FrameRate <= 0
		? UnlimitedFrameRateLabel
		: FText::Format(LOCTEXT("SummaryFrameRate", "{0} FPS"), FText::AsNumber(FrameRate));
	FNumberFormattingOptions PercentOptions;
	PercentOptions.SetMaximumFractionalDigits(0);

	Text_CurrentSummary->SetText(FText::Format(
		LOCTEXT("CurrentSummaryFormat", "현재 적용  ·  {0} × {1}  ·  {2}  ·  {3}  ·  {4}  ·  음량 {5}"),
		FText::AsNumber(Resolution.X), FText::AsNumber(Resolution.Y), WindowMode, Quality,
		FrameRateText, FText::AsPercent(RuntimeSettings->GetMasterVolume(), &PercentOptions)));
}

void UFF_FestivalSettingsScreen::HandleDisplayClicked() { SelectCategory(EFF_FestivalSettingsCategory::Display); }
void UFF_FestivalSettingsScreen::HandleAudioClicked() { SelectCategory(EFF_FestivalSettingsCategory::Audio); }
void UFF_FestivalSettingsScreen::HandleGameplayClicked() { SelectCategory(EFF_FestivalSettingsCategory::Gameplay); }
void UFF_FestivalSettingsScreen::HandleAccessibilityClicked() { SelectCategory(EFF_FestivalSettingsCategory::Accessibility); }
void UFF_FestivalSettingsScreen::HandleCloseClicked() { OnCloseRequested.Broadcast(); }

void UFF_FestivalSettingsScreen::HandleResetClicked()
{
	if (UFF_RuntimeSettingsSubsystem* RuntimeSettings = UFF_RuntimeSettingsSubsystem::Get(this))
	{
		RuntimeSettings->ResetToProjectDefaults();
	}
	if (UFF_UIAccessibilitySubsystem* Accessibility = UFF_UIAccessibilitySubsystem::Get(this))
	{
		Accessibility->SetReduceMotionEnabled(false);
	}
	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		UserSettings->SetToDefaults();
		UserSettings->ApplySettings(false);
		UserSettings->ConfirmVideoMode();
		UserSettings->SaveSettings();
	}
	RebuildRows();
}

void UFF_FestivalSettingsScreen::HandleVSyncToggled(bool bEnabled)
{
	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		UserSettings->SetVSyncEnabled(bEnabled);
		UserSettings->ApplyNonResolutionSettings();
		UserSettings->SaveSettings();
	}
	RefreshCurrentSummary();
}

void UFF_FestivalSettingsScreen::HandleDynamicResolutionToggled(bool bEnabled)
{
	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		UserSettings->SetDynamicResolutionEnabled(bEnabled);
		UserSettings->ApplyNonResolutionSettings();
		UserSettings->SaveSettings();
	}
	RefreshCurrentSummary();
}

void UFF_FestivalSettingsScreen::HandleMasterVolumeChanged(float Value)
{
	if (UFF_RuntimeSettingsSubsystem* Settings = UFF_RuntimeSettingsSubsystem::Get(this))
	{
		Settings->SetMasterVolume(Value);
	}
	RefreshCurrentSummary();
}

void UFF_FestivalSettingsScreen::HandleSelectionChanged(FName SettingId, int32 SelectedIndex)
{
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!UserSettings)
	{
		return;
	}

	bool bResolutionSetting = false;
	bool bNonResolutionSetting = false;
	bool bPresetChanged = false;
	bool bIndividualQualityChanged = false;

	if (SettingId == Setting_WindowMode && SelectedIndex >= 0 && SelectedIndex <= 2)
	{
		const EWindowMode::Type Modes[] = {
			EWindowMode::WindowedFullscreen, EWindowMode::Fullscreen, EWindowMode::Windowed
		};
		UserSettings->SetFullscreenMode(Modes[SelectedIndex]);
		bResolutionSetting = true;
	}
	else if (SettingId == Setting_Resolution && ResolutionOptions.IsValidIndex(SelectedIndex))
	{
		UserSettings->SetScreenResolution(ResolutionOptions[SelectedIndex]);
		bResolutionSetting = true;
	}
	else if (SettingId == Setting_ResolutionScale && ResolutionScaleOptions.IsValidIndex(SelectedIndex))
	{
		UserSettings->SetResolutionScaleValueEx(ResolutionScaleOptions[SelectedIndex]);
		bNonResolutionSetting = true;
	}
	else if (SettingId == Setting_FrameRateLimit && FrameRateOptions.IsValidIndex(SelectedIndex))
	{
		UserSettings->SetFrameRateLimit(static_cast<float>(FrameRateOptions[SelectedIndex]));
		bNonResolutionSetting = true;
	}
	else if (SettingId == Setting_GraphicsPreset && SelectedIndex >= 0 && SelectedIndex <= 4)
	{
		UserSettings->SetOverallScalabilityLevel(SelectedIndex);
		bNonResolutionSetting = true;
		bPresetChanged = true;
	}
	else if (SelectedIndex >= 0 && SelectedIndex <= 4)
	{
		if (SettingId == Setting_ViewDistance)
		{
			UserSettings->SetViewDistanceQuality(SelectedIndex);
		}
		else if (SettingId == Setting_AntiAliasing)
		{
			UserSettings->SetAntiAliasingQuality(SelectedIndex);
		}
		else if (SettingId == Setting_Shadow)
		{
			UserSettings->SetShadowQuality(SelectedIndex);
		}
		else if (SettingId == Setting_GlobalIllumination)
		{
			UserSettings->SetGlobalIlluminationQuality(SelectedIndex);
		}
		else if (SettingId == Setting_Reflection)
		{
			UserSettings->SetReflectionQuality(SelectedIndex);
		}
		else if (SettingId == Setting_PostProcessing)
		{
			UserSettings->SetPostProcessingQuality(SelectedIndex);
		}
		else if (SettingId == Setting_Texture)
		{
			UserSettings->SetTextureQuality(SelectedIndex);
		}
		else if (SettingId == Setting_Effects)
		{
			UserSettings->SetVisualEffectQuality(SelectedIndex);
		}
		else if (SettingId == Setting_Foliage)
		{
			UserSettings->SetFoliageQuality(SelectedIndex);
		}
		else if (SettingId == Setting_Shading)
		{
			UserSettings->SetShadingQuality(SelectedIndex);
		}
		else
		{
			return;
		}
		bNonResolutionSetting = true;
		bIndividualQualityChanged = true;
	}
	else
	{
		// `사용자 지정`은 현재 상태를 설명하는 항목이며 자체로 값을 덮어쓰지 않는다.
		return;
	}

	if (bResolutionSetting)
	{
		UserSettings->ApplyResolutionSettings(false);
		UserSettings->ConfirmVideoMode();
	}
	else if (bNonResolutionSetting)
	{
		UserSettings->ApplyNonResolutionSettings();
	}
	UserSettings->SaveSettings();

	if (bPresetChanged)
	{
		// 프리셋은 아래 세부 행 전체를 바꾸므로 같은 데이터로 다시 그린다.
		RebuildRows();
		return;
	}
	if (bIndividualQualityChanged)
	{
		if (UFF_SettingSelectRow* PresetRow = QualityPresetRow.Get())
		{
			PresetRow->Configure(Setting_GraphicsPreset, GraphicsPresetLabel, GraphicsPresetDescription,
				GetQualityLevelOptions(true), 5);
		}
	}
	RefreshCurrentSummary();
}

void UFF_FestivalSettingsScreen::HandleZoomToCursorToggled(bool bEnabled)
{
	if (UFF_RuntimeSettingsSubsystem* Settings = UFF_RuntimeSettingsSubsystem::Get(this))
	{
		Settings->SetZoomToCursorEnabled(bEnabled);
	}
}

void UFF_FestivalSettingsScreen::HandleEdgeScrollToggled(bool bEnabled)
{
	if (UFF_RuntimeSettingsSubsystem* Settings = UFF_RuntimeSettingsSubsystem::Get(this))
	{
		Settings->SetEdgeScrollEnabled(bEnabled);
	}
}

void UFF_FestivalSettingsScreen::HandleReduceMotionToggled(bool bEnabled)
{
	if (UFF_UIAccessibilitySubsystem* Accessibility = UFF_UIAccessibilitySubsystem::Get(this))
	{
		Accessibility->SetReduceMotionEnabled(bEnabled);
	}
}

UButton* UFF_FestivalSettingsScreen::GetCategoryButton(EFF_FestivalSettingsCategory Category) const
{
	switch (Category)
	{
	case EFF_FestivalSettingsCategory::Display: return Button_Display;
	case EFF_FestivalSettingsCategory::Audio: return Button_Audio;
	case EFF_FestivalSettingsCategory::Gameplay: return Button_Gameplay;
	case EFF_FestivalSettingsCategory::Accessibility: return Button_Accessibility;
	}
	return nullptr;
}

UTextBlock* UFF_FestivalSettingsScreen::GetCategoryText(EFF_FestivalSettingsCategory Category) const
{
	switch (Category)
	{
	case EFF_FestivalSettingsCategory::Display: return Text_DisplayCategory;
	case EFF_FestivalSettingsCategory::Audio: return Text_AudioCategory;
	case EFF_FestivalSettingsCategory::Gameplay: return Text_GameplayCategory;
	case EFF_FestivalSettingsCategory::Accessibility: return Text_AccessibilityCategory;
	}
	return nullptr;
}

FText UFF_FestivalSettingsScreen::GetCategoryLabel(EFF_FestivalSettingsCategory Category) const
{
	switch (Category)
	{
	case EFF_FestivalSettingsCategory::Display: return DisplayCategoryLabel;
	case EFF_FestivalSettingsCategory::Audio: return AudioCategoryLabel;
	case EFF_FestivalSettingsCategory::Gameplay: return GameplayCategoryLabel;
	case EFF_FestivalSettingsCategory::Accessibility: return AccessibilityCategoryLabel;
	}
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
