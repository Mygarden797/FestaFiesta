// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_FestivalSettingsScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "UI/Core/FF_PauseOptionsLightTealStyle.h"
#include "UI/Core/FF_UIAccessibilitySubsystem.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UILog.h"
#include "UI/Settings/FF_FestivalSettingsPolicy.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"
#include "UI/Settings/FF_KeyBindingPanel.h"
#include "UI/Settings/FF_KeyBindingRuntimeSubsystem.h"
#include "UI/Settings/FF_RuntimeSettingsSubsystem.h"
#include "UI/Settings/FF_SettingSelectRow.h"
#include "UI/Settings/FF_SettingSliderRow.h"
#include "UI/Settings/FF_SettingToggleRow.h"
#include "UnrealEngine.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "FestaFiestaFestivalSettings"

namespace
{
	const TCHAR* ToggleRowFallbackPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_SettingToggleRow.WBP_FF_SettingToggleRow_C");
	const TCHAR* SliderRowFallbackPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_SettingSliderRow.WBP_FF_SettingSliderRow_C");
	const TCHAR* SelectRowFallbackPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_SettingSelectRow.WBP_FF_SettingSelectRow_C");
	const TCHAR* KeyBindingPanelFallbackPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_KeyBindingPanel.WBP_FF_KeyBindingPanel_C");

	const FName Setting_WindowMode(TEXT("WindowMode"));
	const FName Setting_Resolution(TEXT("Resolution"));
	const FName Setting_ResolutionScale(TEXT("ResolutionScale"));
	const FName Setting_FrameRateLimit(TEXT("FrameRateLimit"));
	const FName Setting_GraphicsPreset(TEXT("GraphicsPreset"));

	const EFF_FestivalSettingsCategory AllCategories[] = {
		EFF_FestivalSettingsCategory::Basic,
		EFF_FestivalSettingsCategory::Account,
		EFF_FestivalSettingsCategory::Keyboard,
		EFF_FestivalSettingsCategory::Graphics,
		EFF_FestivalSettingsCategory::ExitGame
	};

	FText FormatResolution(const FIntPoint& Resolution)
	{
		return FText::Format(LOCTEXT("ResolutionFormat", "{0} × {1}"),
			FText::AsNumber(Resolution.X), FText::AsNumber(Resolution.Y));
	}

	FText FormatPercent(const int32 Percent)
	{
		return FText::Format(LOCTEXT("PercentFormat", "{0}%"), FText::AsNumber(Percent));
	}

	void SortResolutionOptions(TArray<FIntPoint>& Resolutions)
	{
		Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
		{
			const int64 PixelsA = static_cast<int64>(A.X) * A.Y;
			const int64 PixelsB = static_cast<int64>(B.X) * B.Y;
			if (PixelsA != PixelsB)
			{
				return PixelsA < PixelsB;
			}
			return A.X != B.X ? A.X < B.X : A.Y < B.Y;
		});
	}

	FSlateBrush MakeSolidBrush(const FLinearColor& Color, const float CornerRadius = 6.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(
			CornerRadius, CornerRadius, CornerRadius, CornerRadius);
		return Brush;
	}

	FSlateBrush MakeOutlinedBrush(const FLinearColor& Fill, const FLinearColor& Outline,
		const float CornerRadius = 4.f, const float OutlineWidth = 1.f)
	{
		FSlateBrush Brush = MakeSolidBrush(Fill, CornerRadius);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = OutlineWidth;
		return Brush;
	}

	FButtonStyle MakeCategoryTabStyle(const UFF_UIPalette& Palette, const bool bSelected)
	{
		const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(Palette);
		FButtonStyle Style;
		Style.Normal = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			bSelected ? FFPauseOptionsLightTeal::RectNavSelectedPath
				: FFPauseOptionsLightTeal::RectNavDefaultPath,
			bSelected ? Tokens.Teal : Tokens.PaperStrong,
			bSelected ? Tokens.TealDeep : Tokens.Line);
		Style.Hovered = bSelected
			? Style.Normal
			: FFPauseOptionsLightTeal::MakeRectButtonBrush(
				FFPauseOptionsLightTeal::RectNavHoverPath, Tokens.Hover, Tokens.TealSoft);
		Style.Pressed = bSelected
			? Style.Normal
			: FFPauseOptionsLightTeal::MakeRectButtonBrush(
				FFPauseOptionsLightTeal::RectNavPressedPath, Tokens.TealSoft, Tokens.TealDeep);
		Style.Disabled = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			FFPauseOptionsLightTeal::RectNavDisabledPath, Tokens.PaperSunken, Tokens.Line);
		Style.NormalPadding = FMargin(18.f, 10.f);
		Style.PressedPadding = FMargin(18.f, 11.f, 18.f, 9.f);
		return Style;
	}

	FSlateFontInfo DeriveFont(const UTextBlock* Source, const int32 Size, const FName Typeface = TEXT("Medium"))
	{
		FSlateFontInfo Font = Source ? Source->GetFont() : FSlateFontInfo();
		Font.Size = Size;
		Font.TypefaceFontName = Typeface;
		return Font;
	}
}

UFF_FestivalSettingsScreen::UFF_FestivalSettingsScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ScreenInputMode = EFF_ScreenInputMode::Menu;
	bShowMouseCursor = true;
	PopulateDefaultKeyboardGuide();
}

void UFF_FestivalSettingsScreen::PopulateDefaultKeyboardGuide()
{
	KeyboardGuideEntries.Reset();
	auto AddGuide = [this](const FText& Context, const FText& Keys, const FText& Action)
	{
		FFF_SettingsKeyGuideEntry& Entry = KeyboardGuideEntries.AddDefaulted_GetRef();
		Entry.Context = Context;
		Entry.Keys = Keys;
		Entry.Action = Action;
	};

	const FText Common = LOCTEXT("KeyContextCommon", "공통");
	const FText Camera = LOCTEXT("KeyContextCamera", "관찰 카메라");
	const FText Build = LOCTEXT("KeyContextBuild", "건축");
	const FText Character = LOCTEXT("KeyContextCharacter", "캐릭터");
	const FText Minigame = LOCTEXT("KeyContextMinigame", "미니게임");
	AddGuide(Common, LOCTEXT("KeyOptions", "ESC / F5"),
		LOCTEXT("KeyOptionsAction", "일시정지 메뉴 열기 / 이전으로"));
	AddGuide(Camera, LOCTEXT("KeyCameraMove", "WASD"), LOCTEXT("KeyCameraMoveAction", "카메라 이동"));
	AddGuide(Camera, LOCTEXT("KeyCameraZoom", "마우스 휠"), LOCTEXT("KeyCameraZoomAction", "확대 / 축소"));
	AddGuide(Camera, LOCTEXT("KeyCameraRotate", "Q / E"), LOCTEXT("KeyCameraRotateAction", "카메라 회전"));
	AddGuide(Camera, LOCTEXT("KeyCameraPick", "좌클릭"), LOCTEXT("KeyCameraPickAction", "시설 또는 대상 선택"));
	AddGuide(Camera, LOCTEXT("KeyCameraReset", "Home"), LOCTEXT("KeyCameraResetAction", "관찰 카메라 초기화"));
	AddGuide(Camera, LOCTEXT("KeyCameraToggleView", "Tab"), LOCTEXT("KeyCameraToggleViewAction", "관찰 / 캐릭터 시점 전환"));
	AddGuide(Build, LOCTEXT("KeyBuildToggle", "B"), LOCTEXT("KeyBuildToggleAction", "시설 카탈로그 열기 / 닫기"));
	AddGuide(Build, LOCTEXT("KeyBuildPause", "ESC / F5"), LOCTEXT("KeyBuildPauseAction", "건축 상태를 유지하고 일시정지 메뉴 열기"));
	AddGuide(Build, LOCTEXT("KeyBuildMove", "M"), LOCTEXT("KeyBuildMoveAction", "선택한 시설 이동 시작"));
	AddGuide(Build, LOCTEXT("KeyBuildRotate", "Q / E"), LOCTEXT("KeyBuildRotateAction", "왼쪽 / 오른쪽 스냅 회전"));
	AddGuide(Build, LOCTEXT("KeyBuildSmoothRotate", "Shift + Q / E"), LOCTEXT("KeyBuildSmoothRotateAction", "부드러운 자유 회전"));
	AddGuide(Build, LOCTEXT("KeyBuildConfirm", "좌클릭 / Enter"), LOCTEXT("KeyBuildConfirmAction", "설치 또는 이동 확정"));
	AddGuide(Build, LOCTEXT("KeyBuildCancel", "우클릭"), LOCTEXT("KeyBuildCancelAction", "선택 또는 배치 취소"));
	AddGuide(Build, LOCTEXT("KeyBuildGrid", "G"), LOCTEXT("KeyBuildGridAction", "배치 격자 표시 / 숨기기"));
	AddGuide(Build, LOCTEXT("KeyBuildPrecision", "Ctrl"), LOCTEXT("KeyBuildPrecisionAction", "누르는 동안 정밀 배치"));
	AddGuide(Build, LOCTEXT("KeyBuildFree", "Alt"), LOCTEXT("KeyBuildFreeAction", "누르는 동안 자유 배치"));
	AddGuide(Character, LOCTEXT("KeyCharacterMove", "WASD · Shift · Space"), LOCTEXT("KeyCharacterMoveAction", "이동 · 달리기 · 점프"));
	AddGuide(Character, LOCTEXT("KeyCharacterLook", "마우스 이동"), LOCTEXT("KeyCharacterLookAction", "캐릭터 시점 조작"));
	AddGuide(Minigame, LOCTEXT("KeyMinigameExit", "ESC"), LOCTEXT("KeyMinigameExitAction", "미니게임 나가기"));
	AddGuide(Minigame, LOCTEXT("KeyMinigameMove", "WASD / 방향키"), LOCTEXT("KeyMinigameMoveAction", "서핑·인형뽑기 이동"));
	AddGuide(Minigame, LOCTEXT("KeyMinigameAction", "Space / Enter / 좌클릭"), LOCTEXT("KeyMinigameActionAction", "부스트·집게·물총 사용"));
}

bool UFF_FestivalSettingsScreen::FSettingsDraft::Equals(const FSettingsDraft& Other) const
{
	return WindowMode == Other.WindowMode && Resolution == Other.Resolution &&
		ResolutionScalePercent == Other.ResolutionScalePercent &&
		FrameRateLimit == Other.FrameRateLimit && GraphicsPreset == Other.GraphicsPreset &&
		bVSync == Other.bVSync && FMath::IsNearlyEqual(MasterVolume, Other.MasterVolume) &&
		bZoomToCursor == Other.bZoomToCursor && bEdgeScroll == Other.bEdgeScroll &&
		bReduceMotion == Other.bReduceMotion;
}

void UFF_FestivalSettingsScreen::NativeConstruct()
{
	Super::NativeConstruct();
	// 생성된 WBP가 이전 네이티브 CDO 값을 직렬화했더라도 현재 입력 계약이 항상 우선한다.
	PopulateDefaultKeyboardGuide();

	// 구형 화면도 최소 기능을 잃지 않게 네 개 버튼을 새 분류의 호환 슬롯으로 사용한다.
	Button_Basic = Button_Basic ? Button_Basic : Button_Display;
	Button_Account = Button_Account ? Button_Account : Button_Audio;
	Button_Keyboard = Button_Keyboard ? Button_Keyboard : Button_Gameplay;
	Button_Graphics = Button_Graphics ? Button_Graphics : Button_Accessibility;
	Text_BasicCategory = Text_BasicCategory ? Text_BasicCategory : Text_DisplayCategory;
	Text_AccountCategory = Text_AccountCategory ? Text_AccountCategory : Text_AudioCategory;
	Text_KeyboardCategory = Text_KeyboardCategory ? Text_KeyboardCategory : Text_GameplayCategory;
	Text_GraphicsCategory = Text_GraphicsCategory ? Text_GraphicsCategory : Text_AccessibilityCategory;
	EnsureExitCategoryButton();

	for (const EFF_FestivalSettingsCategory Category : AllCategories)
	{
		if (UButton* Button = GetCategoryButton(Category))
		{
			switch (Category)
			{
			case EFF_FestivalSettingsCategory::Basic:
				Button->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleBasicClicked); break;
			case EFF_FestivalSettingsCategory::Account:
				Button->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleAccountClicked); break;
			case EFF_FestivalSettingsCategory::Keyboard:
				Button->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleKeyboardClicked); break;
			case EFF_FestivalSettingsCategory::Graphics:
				Button->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleGraphicsClicked); break;
			case EFF_FestivalSettingsCategory::ExitGame:
				Button->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleExitGameClicked); break;
			}
		}
	}

	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleBackClicked);
	}
	if (UButton* Back = GetBackButton())
	{
		Back->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleBackClicked);
	}
	if (UButton* Save = GetSaveButton())
	{
		Save->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleSaveClicked);
	}
	if (Text_Intro)
	{
		Text_Intro->SetText(LOCTEXT("ManualSaveIntro", "변경 사항은 저장을 눌러야 적용됩니다."));
	}
	if (Text_KeyHint)
	{
		RefreshOptionKeyHint();
	}
	if (UFF_KeyBindingRuntimeSubsystem* Runtime = ResolveKeyBindingRuntime())
	{
		if (!KeyBindingDraftChangedHandle.IsValid())
		{
			KeyBindingDraftChangedHandle = Runtime->OnKeyBindingDraftChanged().AddUObject(
				this, &ThisClass::HandleKeyBindingDraftChanged);
		}
	}
	RefreshCategoryVisuals();
	RefreshActionButtons();
}

void UFF_FestivalSettingsScreen::NativeDestruct()
{
	bIsTearingDown = true;
	if (bAwaitingVideoModeConfirmation)
	{
		RollbackVideoModePreview();
	}
	if (VideoModeTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(VideoModeTickerHandle);
		VideoModeTickerHandle.Reset();
	}

	for (const EFF_FestivalSettingsCategory Category : AllCategories)
	{
		if (UButton* Button = GetCategoryButton(Category))
		{
			Button->OnClicked.RemoveAll(this);
		}
	}
	if (Button_Close)
	{
		Button_Close->OnClicked.RemoveDynamic(this, &UFF_FestivalSettingsScreen::HandleBackClicked);
	}
	if (UButton* Back = GetBackButton())
	{
		Back->OnClicked.RemoveDynamic(this, &UFF_FestivalSettingsScreen::HandleBackClicked);
	}
	if (UButton* Save = GetSaveButton())
	{
		Save->OnClicked.RemoveDynamic(this, &UFF_FestivalSettingsScreen::HandleSaveClicked);
	}
	if (ActiveKeyBindingPanel)
	{
		ActiveKeyBindingPanel->CancelKeyCapture();
	}
	if (UFF_KeyBindingRuntimeSubsystem* Runtime = ResolveKeyBindingRuntime())
	{
		if (KeyBindingDraftChangedHandle.IsValid())
		{
			Runtime->OnKeyBindingDraftChanged().Remove(KeyBindingDraftChangedHandle);
			KeyBindingDraftChangedHandle.Reset();
		}
		Runtime->RollbackKeyBindingEdit();
	}
	ActiveKeyBindingPanel = nullptr;
	Super::NativeDestruct();
}

void UFF_FestivalSettingsScreen::NativeOnActivated()
{
	bIsTearingDown = false;
	CaptureDraft();
	SelectCategory(DefaultCategory);
	Super::NativeOnActivated();
}

void UFF_FestivalSettingsScreen::NativeOnDeactivated()
{
	if (ActiveKeyBindingPanel)
	{
		ActiveKeyBindingPanel->CancelKeyCapture();
	}
	if (bAwaitingVideoModeConfirmation)
	{
		RollbackVideoModePreview();
	}
	Super::NativeOnDeactivated();
}

UWidget* UFF_FestivalSettingsScreen::NativeGetDesiredFocusTarget() const
{
	if (UButton* Selected = GetCategoryButton(SelectedCategory))
	{
		return Selected;
	}
	return Super::NativeGetDesiredFocusTarget();
}

FReply UFF_FestivalSettingsScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const UFF_FestivalSettingsDeveloperSettings* Settings =
		UFF_FestivalSettingsDeveloperSettings::Get();
	if (!InKeyEvent.IsRepeat() && Settings && Settings->OpenCloseKeys.Contains(InKeyEvent.GetKey()))
	{
		RequestNavigateBack();
		return FReply::Handled();
	}
	if (!InKeyEvent.IsRepeat()
		&& SelectedCategory == EFF_FestivalSettingsCategory::Keyboard
		&& ActiveKeyBindingPanel
		&& ActiveKeyBindingPanel->HandlePhysicalKey(InKeyEvent.GetKey()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UFF_FestivalSettingsScreen::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshButtonFocusPresentation();
	RefreshOptionKeyHint();
}

void UFF_FestivalSettingsScreen::RefreshOptionKeyHint()
{
	const UFF_FestivalSettingsDeveloperSettings* Settings =
		UFF_FestivalSettingsDeveloperSettings::Get();
	if (!Text_KeyHint || !Settings || Settings->OpenCloseKeys.IsEmpty()
		|| PresentedOpenCloseKeys == Settings->OpenCloseKeys)
	{
		return;
	}

	PresentedOpenCloseKeys = Settings->OpenCloseKeys;
	TArray<FString> Labels;
	for (const FKey& Key : PresentedOpenCloseKeys)
	{
		if (Key.IsValid())
		{
			Labels.AddUnique(Key.GetDisplayName(false).ToString());
		}
	}
	const FText Hint = Labels.IsEmpty()
		? LOCTEXT("OptionsBackHint", "옵션 키  이전으로")
		: FText::FromString(FString::Printf(
			TEXT("%s  %s"), *FString::Join(Labels, TEXT(" · ")), *LOCTEXT("BackSuffix", "이전으로").ToString()));
	Text_KeyHint->SetText(Hint);
	Text_KeyHint->SetToolTipText(Hint);
}

bool UFF_FestivalSettingsScreen::RequestNavigateBack()
{
	// Controller 바인딩과 Slate가 같은 옵션 키를 본 경우 한 프레임에 확인이 두 단계 진행되지 않게 한다.
	if (LastBackRequestFrame == GFrameCounter)
	{
		return false;
	}
	LastBackRequestFrame = GFrameCounter;
	if (ActiveKeyBindingPanel)
	{
		ActiveKeyBindingPanel->CancelKeyCapture();
	}

	// 종료 확인은 화면 안의 모달이다. ESC/F5/이전으로가 옵션 전체를 닫거나 변경 폐기 확인과
	// 겹치지 않고 먼저 "계속 플레이"와 같은 단계로 돌아가게 한다.
	if (bExitConfirmationArmed)
	{
		bExitConfirmationArmed = false;
		RebuildRows();
		if (Button_QuitAction)
		{
			Button_QuitAction->SetKeyboardFocus();
		}
		return false;
	}
	if (bAwaitingVideoModeConfirmation)
	{
		RollbackVideoModePreview();
		return false;
	}
	if (bDirty && !bDiscardConfirmationArmed)
	{
		bDiscardConfirmationArmed = true;
		bSavedNotice = false;
		RefreshSummary();
		RefreshActionButtons();
		return false;
	}

	if (UFF_KeyBindingRuntimeSubsystem* Runtime = ResolveKeyBindingRuntime())
	{
		Runtime->RollbackKeyBindingEdit();
	}
	OnCloseRequested.Broadcast();
	return true;
}

void UFF_FestivalSettingsScreen::ShowCategory(const EFF_FestivalSettingsCategory Category)
{
	DefaultCategory = Category;
	if (IsConstructed())
	{
		SelectCategory(Category);
		if (UButton* CategoryButton = GetCategoryButton(Category))
		{
			CategoryButton->SetFocus();
		}
	}
}

void UFF_FestivalSettingsScreen::CaptureDraft()
{
	if (UFF_KeyBindingRuntimeSubsystem* Runtime = ResolveKeyBindingRuntime())
	{
		Runtime->BeginKeyBindingEdit();
	}
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (UserSettings)
	{
		Draft.WindowMode = UserSettings->GetFullscreenMode();
		Draft.Resolution = UserSettings->GetScreenResolution();
		float Normalized = 1.f;
		float Current = 100.f;
		float Minimum = 50.f;
		float Maximum = 100.f;
		UserSettings->GetResolutionScaleInformationEx(Normalized, Current, Minimum, Maximum);
		Draft.ResolutionScalePercent = FMath::RoundToInt(Current);
		Draft.FrameRateLimit = UserSettings->GetFrameRateLimit() <= 0.f
			? 0 : FMath::RoundToInt(UserSettings->GetFrameRateLimit());
		Draft.GraphicsPreset = GetGraphicsPresetExcludingResolution(*UserSettings);
		Draft.bVSync = UserSettings->IsVSyncEnabled();
	}
	if (const UFF_RuntimeSettingsSubsystem* Runtime = UFF_RuntimeSettingsSubsystem::Get(this))
	{
		Draft.MasterVolume = Runtime->GetMasterVolume();
		Draft.bZoomToCursor = Runtime->IsZoomToCursorEnabled();
		Draft.bEdgeScroll = Runtime->IsEdgeScrollEnabled();
	}
	if (const UFF_UIAccessibilitySubsystem* Accessibility = UFF_UIAccessibilitySubsystem::Get(this))
	{
		Draft.bReduceMotion = Accessibility->IsReduceMotionEnabled();
	}

	// Saved는 디스크/UGameUserSettings에서 읽은 원본이다. 그 뒤 UI용 안전 후보로 Draft를
	// 정규화해야 비지원 ini 값이 "저장 필요"로 남고 사용자가 명시적으로 교정할 수 있다.
	Saved = Draft;
	bPreserveAppliedResolutionOption = false;
	if (UserSettings)
	{
		BuildResolutionOptions(*UserSettings);
	}

	bNormalizedUnsupportedVideoMode =
		Draft.WindowMode != Saved.WindowMode || Draft.Resolution != Saved.Resolution;
	bDraftCaptured = true;
	bDirty = !Draft.Equals(Saved)
		|| (ResolveKeyBindingRuntime() && ResolveKeyBindingRuntime()->HasPendingKeyBindingChanges());
	bSavedNotice = false;
	bDiscardConfirmationArmed = false;
	bExitConfirmationArmed = false;
	bQuitRequested = false;
	bHasPreviewRollbackVideoMode = false;
	RefreshSummary();
	RefreshActionButtons();
}

void UFF_FestivalSettingsScreen::SelectCategory(const EFF_FestivalSettingsCategory Category)
{
	if (bAwaitingVideoModeConfirmation)
	{
		return;
	}
	if (Category != EFF_FestivalSettingsCategory::ExitGame)
	{
		bExitConfirmationArmed = false;
	}
	SelectedCategory = Category;
	if (Text_Intro)
	{
		Text_Intro->SetText(Category == EFF_FestivalSettingsCategory::Keyboard
			? LOCTEXT("KeyboardSaveIntro", "키 배치는 즉시 미리보기되며 저장을 눌러야 확정됩니다.")
			: LOCTEXT("ManualSaveIntro", "변경 사항은 저장을 눌러야 적용됩니다."));
	}
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
	for (const EFF_FestivalSettingsCategory Category : AllCategories)
	{
		const bool bSelected = Category == SelectedCategory;
		if (UButton* Button = GetCategoryButton(Category))
		{
			Button->SetStyle(MakeCategoryTabStyle(*Palette, bSelected));
			Button->SetBackgroundColor(FLinearColor::White);
		}
		if (UTextBlock* Text = GetCategoryText(Category))
		{
			Text->SetText(GetCategoryLabel(Category));
			const FLinearColor Color = Category == EFF_FestivalSettingsCategory::ExitGame && !bSelected
				? Palette->InGame.Danger
				: (bSelected ? Palette->InGame.White : Palette->InGame.Ink);
			Text->SetColorAndOpacity(FSlateColor(Color));
		}
	}
}

void UFF_FestivalSettingsScreen::RebuildRows()
{
	if (!VerticalBox_Rows)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_FestivalSettingsScreen: VerticalBox_Rows가 없어 옵션 내용을 그릴 수 없다."));
		return;
	}
	if (ActiveKeyBindingPanel)
	{
		ActiveKeyBindingPanel->CancelKeyCapture();
	}
	ActiveKeyBindingPanel = nullptr;
	VerticalBox_Rows->ClearChildren();
	Button_QuitAction = nullptr;
	Button_QuitCancel = nullptr;
	Button_QuitConfirm = nullptr;
	Row_WindowMode = nullptr;
	Row_GraphicsPreset = nullptr;

	switch (SelectedCategory)
	{
	case EFF_FestivalSettingsCategory::Basic:
		BuildBasicRows();
		break;
	case EFF_FestivalSettingsCategory::Account:
		BuildAccountRows();
		break;
	case EFF_FestivalSettingsCategory::Keyboard:
		BuildKeyboardRows();
		break;
	case EFF_FestivalSettingsCategory::Graphics:
		if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			BuildGraphicsRows(*UserSettings);
		}
		break;
	case EFF_FestivalSettingsCategory::ExitGame:
		AddSectionHeader(LOCTEXT("ExitSection", "게임 세션"));
		AddInfoRow(
			bExitConfirmationArmed ? LOCTEXT("ExitConfirmTitle", "종료 확인") : LOCTEXT("ExitTitle", "바탕화면으로 종료"),
			bExitConfirmationArmed ? LOCTEXT("ExitConfirmValue", "저장되지 않은 진행 상황이 사라질 수 있습니다") : LOCTEXT("ExitValue", "Festa Fiesta! 종료"),
			LOCTEXT("ExitDescription", "옵션 변경은 먼저 저장할 수 있으며, 게임 진행 저장 여부는 현재 세션 상태를 따릅니다."));
		if (bExitConfirmationArmed)
		{
			Button_QuitCancel = AddActionButton(LOCTEXT("ExitCancel", "계속 플레이"), false);
			Button_QuitConfirm = AddActionButton(LOCTEXT("ExitConfirm", "게임 종료"), true);
			if (Button_QuitCancel)
			{
				Button_QuitCancel->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleQuitGameCancelled);
			}
			if (Button_QuitConfirm)
			{
				Button_QuitConfirm->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleQuitGameConfirmed);
			}
		}
		else
		{
			Button_QuitAction = AddActionButton(LOCTEXT("ExitAction", "게임 종료"), true);
			if (Button_QuitAction)
			{
				Button_QuitAction->OnClicked.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleQuitGameClicked);
			}
		}
		break;
	}

	UpdateDirtyState();
}

void UFF_FestivalSettingsScreen::BuildBasicRows()
{
	AddSectionHeader(LOCTEXT("BasicAudioSection", "오디오"));
	if (UFF_SettingSliderRow* Row = AddSliderRow(
		LOCTEXT("MasterVolume", "마스터 음량"),
		LOCTEXT("MasterVolumeDesc", "게임 전체 소리의 크기를 조절합니다"),
		Draft.MasterVolume, 0.f, 1.f, 0.05f))
	{
		Row->OnValueChanged.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleMasterVolumeChanged);
	}
	AddSectionHeader(LOCTEXT("BasicCameraSection", "카메라 조작"));
	if (UFF_SettingToggleRow* Row = AddToggleRow(
		LOCTEXT("ZoomToCursor", "커서 기준 줌"),
		LOCTEXT("ZoomToCursorDesc", "커서 아래 지점을 향해 확대합니다"), Draft.bZoomToCursor))
	{
		Row->OnValueChanged.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleZoomToCursorToggled);
	}
	if (UFF_SettingToggleRow* Row = AddToggleRow(
		LOCTEXT("EdgeScroll", "가장자리 스크롤"),
		LOCTEXT("EdgeScrollDesc", "커서를 화면 끝에 대면 카메라가 이동합니다"), Draft.bEdgeScroll))
	{
		Row->OnValueChanged.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleEdgeScrollToggled);
	}
	AddSectionHeader(LOCTEXT("BasicAccessibilitySection", "접근성"));
	if (UFF_SettingToggleRow* Row = AddToggleRow(
		LOCTEXT("ReduceMotion", "움직임 감소"),
		LOCTEXT("ReduceMotionDesc", "화면 전환과 패널 애니메이션을 줄입니다"), Draft.bReduceMotion))
	{
		Row->OnValueChanged.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleReduceMotionToggled);
	}
}

void UFF_FestivalSettingsScreen::BuildAccountRows()
{
	FString PlayerName;
	if (const APlayerController* Controller = GetOwningPlayer())
	{
		if (const APlayerState* PlayerState = Controller->GetPlayerState<APlayerState>())
		{
			PlayerName = PlayerState->GetPlayerName();
		}
	}
	if (PlayerName.IsEmpty())
	{
		PlayerName = LOCTEXT("LocalPlayerFallback", "로컬 플레이어").ToString();
	}

	const FString BuildVersion(FApp::GetBuildVersion());
	const FString ProjectVersion = BuildVersion.IsEmpty() ? TEXT("개발 빌드") : BuildVersion;
	AddSectionHeader(LOCTEXT("AccountPlayerSection", "플레이어"));
	AddInfoRow(LOCTEXT("AccountProfile", "프로필"), FText::FromString(PlayerName),
		LOCTEXT("AccountProfileDesc", "현재 게임 안에서 사용하는 로컬 플레이어 이름입니다"));
	AddInfoRow(LOCTEXT("AccountStatus", "계정 상태"), LOCTEXT("AccountLocal", "로컬 플레이"),
		LOCTEXT("AccountStatusDesc", "이 프로젝트에는 온라인 계정 연결 기능이 없습니다"));
	AddSectionHeader(LOCTEXT("AccountLocalSection", "로컬 환경"));
	AddInfoRow(LOCTEXT("AccountStorage", "설정 저장 위치"), LOCTEXT("AccountDevice", "이 기기"),
		LOCTEXT("AccountStorageDesc", "개인 옵션은 Windows 사용자 설정에 저장됩니다"));
	AddInfoRow(LOCTEXT("AccountVersion", "게임 버전"), FText::FromString(ProjectVersion),
		LOCTEXT("AccountVersionDesc", "실행 중인 Festa Fiesta! 빌드 버전입니다"));
}

void UFF_FestivalSettingsScreen::BuildKeyboardRows()
{
	const TSubclassOf<UFF_KeyBindingPanel> PanelClass = ResolveKeyBindingPanelClass();
	if (PanelClass)
	{
		if (UFF_KeyBindingPanel* Panel = CreateWidget<UFF_KeyBindingPanel>(this, PanelClass))
		{
			ActiveKeyBindingPanel = Panel;
			if (UVerticalBoxSlot* PanelSlot = VerticalBox_Rows->AddChildToVerticalBox(Panel))
			{
				PanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				PanelSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 16.f));
			}
			return;
		}
	}

	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("UFF_FestivalSettingsScreen: WBP_FF_KeyBindingPanel을 만들 수 없어 읽기 전용 안내로 대체한다."));
	FText PreviousContext;
	for (const FFF_SettingsKeyGuideEntry& Entry : KeyboardGuideEntries)
	{
		if (!Entry.Context.EqualTo(PreviousContext))
		{
			AddSectionHeader(Entry.Context);
			PreviousContext = Entry.Context;
		}
		AddInfoRow(Entry.Keys, Entry.Action, Entry.Context);
	}
}

void UFF_FestivalSettingsScreen::BuildGraphicsRows(UGameUserSettings& UserSettings)
{
	BuildResolutionOptions(UserSettings, bPreserveAppliedResolutionOption);
	BuildResolutionScaleOptions(UserSettings);
	BuildFrameRateOptions(UserSettings);

	const TArray<FText> WindowModes = {
		LOCTEXT("WindowModeBorderless", "테두리 없는 창"),
		LOCTEXT("WindowModeFullscreen", "전체 화면"),
		LOCTEXT("WindowModeWindowed", "창 모드")
	};
	int32 WindowModeIndex = 0;
	switch (Draft.WindowMode)
	{
	case EWindowMode::Fullscreen: WindowModeIndex = 1; break;
	case EWindowMode::Windowed: WindowModeIndex = 2; break;
	case EWindowMode::WindowedFullscreen:
	default: WindowModeIndex = 0; break;
	}
	AddSectionHeader(LOCTEXT("GraphicsDisplaySection", "디스플레이"));
	Row_WindowMode = AddSelectRow(Setting_WindowMode, LOCTEXT("WindowMode", "화면 모드"),
		LOCTEXT("WindowModeDesc", "전체 화면, 테두리 없는 창, 창 모드를 선택합니다"),
		WindowModes, WindowModeIndex);

	TArray<FText> ResolutionLabels;
	for (const FIntPoint& Resolution : ResolutionOptions)
	{
		ResolutionLabels.Add(FormatResolution(Resolution));
	}
	const int32 ResolutionIndex = FMath::Max(0, ResolutionOptions.IndexOfByKey(Draft.Resolution));
	AddSelectRow(Setting_Resolution, LOCTEXT("Resolution", "출력 해상도"),
		Draft.WindowMode == EWindowMode::WindowedFullscreen
			? LOCTEXT("ResolutionBorderlessDesc", "테두리 없는 창은 모니터의 바탕 화면 해상도를 사용합니다")
			: LOCTEXT("ResolutionDesc", "현재 화면 모드에서 실제로 지원되는 해상도만 표시합니다"),
		ResolutionLabels, ResolutionIndex);

	AddSectionHeader(LOCTEXT("GraphicsRenderingSection", "렌더링 및 성능"));
	TArray<FText> ScaleLabels;
	for (const int32 Percent : ResolutionScaleOptions)
	{
		ScaleLabels.Add(FormatPercent(Percent));
	}
	AddSelectRow(Setting_ResolutionScale, LOCTEXT("ResolutionScale", "렌더링 해상도"),
		LOCTEXT("ResolutionScaleDesc", "UI는 선명하게 유지하고 3D 장면의 렌더링 비율을 조절합니다"),
		ScaleLabels, FMath::Max(0, ResolutionScaleOptions.IndexOfByKey(Draft.ResolutionScalePercent)));

	// 사용자 지정은 기존 개별 품질이 섞여 있을 때만 현재 상태로 표시한다. 일단 표준
	// 프리셋을 고르면 행을 재구성해 다시 선택할 수 없는 표시 전용 상태로 유지한다.
	const bool bCurrentQualityIsCustom = Draft.GraphicsPreset < 0 || Draft.GraphicsPreset > 4;
	const TArray<FText> Presets = GetQualityLevelOptions(bCurrentQualityIsCustom);
	Row_GraphicsPreset = AddSelectRow(Setting_GraphicsPreset, LOCTEXT("GraphicsPreset", "그래픽 품질"),
		LOCTEXT("GraphicsPresetDesc", "가시 거리, 조명, 그림자, 텍스처 품질을 한 번에 설정합니다"),
		Presets, bCurrentQualityIsCustom ? 5 : Draft.GraphicsPreset);

	TArray<FText> FrameRateLabels;
	for (const int32 Limit : FrameRateOptions)
	{
		FrameRateLabels.Add(Limit <= 0
			? LOCTEXT("FrameRateUnlimited", "무제한")
			: FText::Format(LOCTEXT("FrameRateFormat", "{0} FPS"), FText::AsNumber(Limit)));
	}
	AddSelectRow(Setting_FrameRateLimit, LOCTEXT("FrameRateLimit", "최대 프레임"),
		LOCTEXT("FrameRateLimitDesc", "GPU 사용량과 발열을 제어할 최대 FPS입니다"),
		FrameRateLabels, FMath::Max(0, FrameRateOptions.IndexOfByKey(Draft.FrameRateLimit)));

	if (UFF_SettingToggleRow* Row = AddToggleRow(LOCTEXT("VSync", "수직 동기화"),
		LOCTEXT("VSyncDesc", "화면 찢어짐을 줄입니다"), Draft.bVSync))
	{
		Row->OnValueChanged.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleVSyncToggled);
	}
}

void UFF_FestivalSettingsScreen::BuildResolutionOptions(
	const UGameUserSettings& UserSettings,
	const bool bPreserveAppliedResolution)
{
	TArray<FIntPoint> DetectedFullscreen;
	TArray<FIntPoint> DetectedWindowed;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(DetectedFullscreen);
	UKismetSystemLibrary::GetConvenientWindowedResolutions(DetectedWindowed);

	const UFF_FestivalSettingsDeveloperSettings* Settings = UFF_FestivalSettingsDeveloperSettings::Get();
	const TArray<FIntPoint> Fallbacks = Settings ? Settings->FallbackResolutions : TArray<FIntPoint>();
	FIntPoint Desktop = UserSettings.GetDesktopResolution();
	if (Desktop.X <= 0 || Desktop.Y <= 0)
	{
		Desktop = GSystemResolution.ResX > 0 && GSystemResolution.ResY > 0
			? FIntPoint(GSystemResolution.ResX, GSystemResolution.ResY)
			: FIntPoint(1920, 1080);
	}
	ResolutionOptions = FF::SettingsPolicy::BuildResolutionOptions(
		Draft.WindowMode, DetectedFullscreen, DetectedWindowed, Fallbacks, Desktop, Draft.Resolution,
		Settings ? Settings->MinimumResolutionWidth : 1280,
		Settings ? Settings->MinimumResolutionHeight : 720);
	// 미리보기 취소 시에는 GSystemResolution으로 확인한, 직전에 실제 적용되어 있던 창 크기를
	// 정확히 보여준다. 최초 ini 캡처에서는 이 예외를 쓰지 않아 비지원 저장값이 목록을 오염시키지 않는다.
	if (bPreserveAppliedResolution && Draft.WindowMode != EWindowMode::WindowedFullscreen
		&& Draft.Resolution.X > 0 && Draft.Resolution.Y > 0)
	{
		ResolutionOptions.AddUnique(Draft.Resolution);
		SortResolutionOptions(ResolutionOptions);
	}
	if (!ResolutionOptions.IsEmpty())
	{
		const int32 Closest = FF::SettingsPolicy::FindClosestResolutionIndex(ResolutionOptions, Draft.Resolution);
		Draft.Resolution = ResolutionOptions[FMath::Clamp(Closest, 0, ResolutionOptions.Num() - 1)];
	}
}

int32 UFF_FestivalSettingsScreen::GetGraphicsPresetExcludingResolution(
	const UGameUserSettings& UserSettings) const
{
	const int32 Target = UserSettings.GetViewDistanceQuality();
	if (Target < 0 || Target > 4)
	{
		return INDEX_NONE;
	}
	return Target == UserSettings.GetAntiAliasingQuality()
		&& Target == UserSettings.GetShadowQuality()
		&& Target == UserSettings.GetGlobalIlluminationQuality()
		&& Target == UserSettings.GetReflectionQuality()
		&& Target == UserSettings.GetPostProcessingQuality()
		&& Target == UserSettings.GetTextureQuality()
		&& Target == UserSettings.GetVisualEffectQuality()
		&& Target == UserSettings.GetFoliageQuality()
		&& Target == UserSettings.GetShadingQuality()
		? Target : INDEX_NONE;
}

void UFF_FestivalSettingsScreen::BuildResolutionScaleOptions(const UGameUserSettings& UserSettings)
{
	ResolutionScaleOptions.Reset();
	float Normalized = 1.f;
	float Current = 100.f;
	float Minimum = 50.f;
	float Maximum = 100.f;
	UserSettings.GetResolutionScaleInformationEx(Normalized, Current, Minimum, Maximum);
	if (const UFF_FestivalSettingsDeveloperSettings* Settings = UFF_FestivalSettingsDeveloperSettings::Get())
	{
		for (const int32 Percent : Settings->ResolutionScalePercentOptions)
		{
			if (Percent >= FMath::FloorToInt(Minimum) && Percent <= FMath::CeilToInt(Maximum))
			{
				ResolutionScaleOptions.AddUnique(Percent);
			}
		}
	}
	ResolutionScaleOptions.AddUnique(FMath::Clamp(
		Draft.ResolutionScalePercent, FMath::FloorToInt(Minimum), FMath::CeilToInt(Maximum)));
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
	FrameRateOptions.AddUnique(FMath::Max(0, Draft.FrameRateLimit));
	FrameRateOptions.Sort();
}

UFF_SettingToggleRow* UFF_FestivalSettingsScreen::AddToggleRow(
	const FText& Label, const FText& Description, const bool bInitialValue)
{
	const TSubclassOf<UFF_SettingToggleRow> RowClass = ResolveToggleRowClass();
	UFF_SettingToggleRow* Row = RowClass ? CreateWidget<UFF_SettingToggleRow>(this, RowClass) : nullptr;
	if (!Row)
	{
		UE_LOG(LogFestaFiestaUI, Error, TEXT("UFF_FestivalSettingsScreen: 토글 행 생성 실패."));
		return nullptr;
	}
	Row->Configure(Label, Description, bInitialValue);
	if (UVerticalBoxSlot* RowLayout = VerticalBox_Rows->AddChildToVerticalBox(Row))
	{
		RowLayout->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}
	return Row;
}

UFF_SettingSliderRow* UFF_FestivalSettingsScreen::AddSliderRow(
	const FText& Label, const FText& Description, const float InitialValue,
	const float MinValue, const float MaxValue, const float StepSize)
{
	const TSubclassOf<UFF_SettingSliderRow> RowClass = ResolveSliderRowClass();
	UFF_SettingSliderRow* Row = RowClass ? CreateWidget<UFF_SettingSliderRow>(this, RowClass) : nullptr;
	if (!Row)
	{
		UE_LOG(LogFestaFiestaUI, Error, TEXT("UFF_FestivalSettingsScreen: 슬라이더 행 생성 실패."));
		return nullptr;
	}
	Row->Configure(Label, Description, InitialValue, MinValue, MaxValue, StepSize, true);
	if (UVerticalBoxSlot* RowLayout = VerticalBox_Rows->AddChildToVerticalBox(Row))
	{
		RowLayout->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}
	return Row;
}

UFF_SettingSelectRow* UFF_FestivalSettingsScreen::AddSelectRow(
	const FName SettingId, const FText& Label, const FText& Description,
	const TArray<FText>& Options, const int32 InitialIndex)
{
	const TSubclassOf<UFF_SettingSelectRow> RowClass = ResolveSelectRowClass();
	UFF_SettingSelectRow* Row = RowClass ? CreateWidget<UFF_SettingSelectRow>(this, RowClass) : nullptr;
	if (!Row)
	{
		UE_LOG(LogFestaFiestaUI, Error, TEXT("UFF_FestivalSettingsScreen: 선택 행 생성 실패."));
		return nullptr;
	}
	Row->Configure(SettingId, Label, Description, Options, InitialIndex);
	Row->OnSelectionChanged.AddUniqueDynamic(this, &UFF_FestivalSettingsScreen::HandleSelectionChanged);
	if (UVerticalBoxSlot* RowLayout = VerticalBox_Rows->AddChildToVerticalBox(Row))
	{
		RowLayout->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}
	return Row;
}

void UFF_FestivalSettingsScreen::AddInfoRow(
	const FText& Label, const FText& Value, const FText& Description)
{
	if (!WidgetTree || !VerticalBox_Rows)
	{
		return;
	}
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBox* Copy = WidgetTree->ConstructWidget<UVerticalBox>();
	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>();
	UTextBlock* DescriptionText = WidgetTree->ConstructWidget<UTextBlock>();
	UTextBlock* ValueText = WidgetTree->ConstructWidget<UTextBlock>();
	if (!Background || !Content || !Copy || !LabelText || !DescriptionText || !ValueText)
	{
		return;
	}

	Background->SetBrush(Palette
		? MakeOutlinedBrush(Palette->InGame.PaperSunken, Palette->InGame.Line, 6.f)
		: MakeSolidBrush(FLinearColor::Black));
	Background->SetPadding(FMargin(22.f, 12.f));
	Background->AddChild(Content);
	LabelText->SetText(Label);
	LabelText->SetFont(DeriveFont(Text_CategoryTitle, 18, TEXT("SemiBold")));
	LabelText->SetColorAndOpacity(Palette ? Palette->InGame.Ink : FLinearColor::White);
	DescriptionText->SetText(Description);
	DescriptionText->SetFont(DeriveFont(Text_CategoryTitle, 13));
	DescriptionText->SetColorAndOpacity(Palette ? Palette->InGame.InkMuted : FLinearColor::Gray);
	DescriptionText->SetVisibility(Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	ValueText->SetText(Value);
	ValueText->SetFont(DeriveFont(Text_CategoryTitle, 17, TEXT("SemiBold")));
	ValueText->SetColorAndOpacity(Palette ? Palette->InGame.TealDeep : FLinearColor::White);
	ValueText->SetJustification(ETextJustify::Right);
	ValueText->SetAutoWrapText(true);

	Copy->AddChildToVerticalBox(LabelText);
	Copy->AddChildToVerticalBox(DescriptionText);
	if (UHorizontalBoxSlot* CopyLayout = Content->AddChildToHorizontalBox(Copy))
	{
		CopyLayout->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CopyLayout->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* ValueLayout = Content->AddChildToHorizontalBox(ValueText))
	{
		ValueLayout->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ValueLayout->SetVerticalAlignment(VAlign_Center);
		ValueLayout->SetPadding(FMargin(24.f, 0.f, 8.f, 0.f));
	}
	if (UVerticalBoxSlot* RowLayout = VerticalBox_Rows->AddChildToVerticalBox(Background))
	{
		RowLayout->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}
}

void UFF_FestivalSettingsScreen::AddSectionHeader(const FText& Label)
{
	if (!WidgetTree || !VerticalBox_Rows)
	{
		return;
	}
	UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>();
	if (!Header)
	{
		return;
	}
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	Header->SetText(Label);
	Header->SetFont(DeriveFont(Text_CategoryTitle, 18, TEXT("SemiBold")));
	Header->SetColorAndOpacity(Palette ? Palette->InGame.TealDeep : FLinearColor::White);
	if (UVerticalBoxSlot* HeaderLayout = VerticalBox_Rows->AddChildToVerticalBox(Header))
	{
		HeaderLayout->SetPadding(FMargin(4.f, 12.f, 0.f, 8.f));
	}
}

UButton* UFF_FestivalSettingsScreen::AddActionButton(const FText& Label, const bool bDanger)
{
	if (!WidgetTree || !VerticalBox_Rows)
	{
		return nullptr;
	}
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
	if (!SizeBox || !Button || !Text)
	{
		return nullptr;
	}
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (UButton* StyleSource = bDanger ? GetSaveButton() : GetBackButton())
	{
		Button->SetStyle(StyleSource->GetStyle());
	}
	if (bDanger && Palette)
	{
		FButtonStyle DangerStyle = Button->GetStyle();
		DangerStyle.Normal = MakeOutlinedBrush(Palette->InGame.Danger, Palette->InGame.Danger, 6.f);
		DangerStyle.Hovered = MakeOutlinedBrush(Palette->InGame.Danger, Palette->InGame.White, 6.f, 1.5f);
		DangerStyle.Pressed = MakeOutlinedBrush(Palette->InGame.Danger, Palette->InGame.White, 6.f, 2.f);
		Button->SetStyle(DangerStyle);
	}
	Button->SetBackgroundColor(FLinearColor::White);
	Text->SetText(Label);
	Text->SetFont(DeriveFont(Text_CategoryTitle, 18, TEXT("SemiBold")));
	Text->SetColorAndOpacity(bDanger
		? (Palette ? Palette->InGame.White : FLinearColor::White)
		: (Palette ? Palette->InGame.Ink : FLinearColor::White));
	if (UButtonSlot* ButtonLayout = Cast<UButtonSlot>(Button->AddChild(Text)))
	{
		ButtonLayout->SetHorizontalAlignment(HAlign_Center);
		ButtonLayout->SetVerticalAlignment(VAlign_Center);
	}
	SizeBox->SetHeightOverride(68.f);
	SizeBox->AddChild(Button);
	if (UVerticalBoxSlot* RowLayout = VerticalBox_Rows->AddChildToVerticalBox(SizeBox))
	{
		RowLayout->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));
	}
	return Button;
}

TSubclassOf<UFF_SettingToggleRow> UFF_FestivalSettingsScreen::ResolveToggleRowClass() const
{
	return ToggleRowClass ? ToggleRowClass : TSubclassOf<UFF_SettingToggleRow>(
		LoadClass<UFF_SettingToggleRow>(nullptr, ToggleRowFallbackPath));
}

TSubclassOf<UFF_SettingSliderRow> UFF_FestivalSettingsScreen::ResolveSliderRowClass() const
{
	return SliderRowClass ? SliderRowClass : TSubclassOf<UFF_SettingSliderRow>(
		LoadClass<UFF_SettingSliderRow>(nullptr, SliderRowFallbackPath));
}

TSubclassOf<UFF_SettingSelectRow> UFF_FestivalSettingsScreen::ResolveSelectRowClass() const
{
	return SelectRowClass ? SelectRowClass : TSubclassOf<UFF_SettingSelectRow>(
		LoadClass<UFF_SettingSelectRow>(nullptr, SelectRowFallbackPath));
}

TSubclassOf<UFF_KeyBindingPanel> UFF_FestivalSettingsScreen::ResolveKeyBindingPanelClass() const
{
	return KeyBindingPanelClass ? KeyBindingPanelClass : TSubclassOf<UFF_KeyBindingPanel>(
		LoadClass<UFF_KeyBindingPanel>(nullptr, KeyBindingPanelFallbackPath));
}

TArray<FText> UFF_FestivalSettingsScreen::GetQualityLevelOptions(const bool bIncludeCustom) const
{
	TArray<FText> Options = {
		LOCTEXT("QualityLow", "낮음"), LOCTEXT("QualityMedium", "중간"),
		LOCTEXT("QualityHigh", "높음"), LOCTEXT("QualityEpic", "매우 높음"),
		LOCTEXT("QualityCinematic", "시네마틱")
	};
	if (bIncludeCustom)
	{
		Options.Add(LOCTEXT("QualityCustom", "사용자 지정"));
	}
	return Options;
}

void UFF_FestivalSettingsScreen::UpdateDirtyState()
{
	if (!bDraftCaptured)
	{
		return;
	}
	const UFF_KeyBindingRuntimeSubsystem* KeyRuntime = ResolveKeyBindingRuntime();
	bDirty = !Draft.Equals(Saved)
		|| (KeyRuntime && KeyRuntime->HasPendingKeyBindingChanges());
	if (bDirty)
	{
		bSavedNotice = false;
		// 확인 문구를 띄운 뒤 다시 조작했다면 사용자의 의도가 바뀐 것이다.
		// 다음 이전 요청에서 다시 한 번 확인하도록 이전 확인을 무효화한다.
		bDiscardConfirmationArmed = false;
	}
	else
	{
		bDiscardConfirmationArmed = false;
	}
	RefreshSummary();
	RefreshActionButtons();
}

void UFF_FestivalSettingsScreen::HandleKeyBindingDraftChanged(const bool bHasPendingChanges)
{
	(void)bHasPendingChanges;
	if (!bIsTearingDown)
	{
		UpdateDirtyState();
	}
}

UFF_KeyBindingRuntimeSubsystem* UFF_FestivalSettingsScreen::ResolveKeyBindingRuntime() const
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	return LocalPlayer ? LocalPlayer->GetSubsystem<UFF_KeyBindingRuntimeSubsystem>() : nullptr;
}

void UFF_FestivalSettingsScreen::RefreshSummary()
{
	if (!Text_CurrentSummary)
	{
		return;
	}
	auto SetSummary = [this](const FText& Value)
	{
		Text_CurrentSummary->SetText(Value);
		Text_CurrentSummary->SetToolTipText(Value);
	};
	if (bAwaitingVideoModeConfirmation)
	{
		SetSummary(FText::Format(
			LOCTEXT("VideoModeConfirmationSummary", "해상도 변경 확인  ·  {0}초 안에 저장을 눌러 유지하세요"),
			FText::AsNumber(FMath::Max(0, FMath::CeilToInt(VideoModeSecondsRemaining)))));
		return;
	}
	if (bDiscardConfirmationArmed)
	{
		SetSummary(
			LOCTEXT("DiscardConfirmationSummary", "저장되지 않은 변경을 버릴까요?  ·  이전으로를 다시 누르면 돌아갑니다"));
		return;
	}
	if (bNormalizedUnsupportedVideoMode && bDirty)
	{
		SetSummary(FText::Format(
			LOCTEXT("NormalizedVideoModeSummary", "지원하지 않는 저장 해상도를 {0}(으)로 보정했습니다  ·  저장을 눌러 적용하세요"),
			FormatResolution(Draft.Resolution)));
		return;
	}

	FText WindowMode;
	switch (Draft.WindowMode)
	{
	case EWindowMode::Fullscreen: WindowMode = LOCTEXT("SummaryFullscreen", "전체 화면"); break;
	case EWindowMode::Windowed: WindowMode = LOCTEXT("SummaryWindowed", "창 모드"); break;
	case EWindowMode::WindowedFullscreen:
	default: WindowMode = LOCTEXT("SummaryBorderless", "테두리 없는 창"); break;
	}
	const TArray<FText> QualityOptions = GetQualityLevelOptions(false);
	const FText Quality = QualityOptions.IsValidIndex(Draft.GraphicsPreset)
		? QualityOptions[Draft.GraphicsPreset] : LOCTEXT("SummaryCustom", "사용자 지정");
	const FText Prefix = bDirty
		? LOCTEXT("SummaryPending", "저장되지 않은 변경")
		: (bSavedNotice ? LOCTEXT("SummarySavedNow", "저장 완료") : LOCTEXT("SummarySaved", "저장된 설정"));
	SetSummary(FText::Format(
		LOCTEXT("CurrentSummaryFormat", "{0}  ·  {1}  ·  {2}  ·  {3}  ·  음량 {4}"),
		Prefix, FormatResolution(Draft.Resolution), WindowMode, Quality, FormatPercent(FMath::RoundToInt(Draft.MasterVolume * 100.f))));
}

void UFF_FestivalSettingsScreen::RefreshActionButtons()
{
	UButton* Back = GetBackButton();
	UButton* Save = GetSaveButton();
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(VideoModeSecondsRemaining));
	SetButtonLabel(Back, Text_Back,
		bAwaitingVideoModeConfirmation ? LOCTEXT("BackRevert", "해상도 되돌리기")
		: (bDiscardConfirmationArmed ? LOCTEXT("BackDiscard", "변경 버리고 이전으로") : LOCTEXT("Back", "이전으로")));
	SetButtonLabel(Save, Text_Save,
		bAwaitingVideoModeConfirmation
			? FText::Format(LOCTEXT("SaveKeepResolution", "해상도 유지 ({0})"), FText::AsNumber(Seconds))
			: LOCTEXT("Save", "저장"));
	if (Save)
	{
		Save->SetIsEnabled(bAwaitingVideoModeConfirmation || bDirty);
	}
}

void UFF_FestivalSettingsScreen::RefreshButtonFocusPresentation()
{
	// 전역 RenderFocusRule=Never이므로 일반 UButton에도 직접 포커스 신호를 준다. 행 위젯은
	// UFF_FocusableWidgetBase의 링을 사용하고, 화면 고정 버튼은 작은 스케일 변화로 위치를 알린다.
	auto RefreshButton = [](UButton* Button)
	{
		if (Button)
		{
			Button->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
			const float Scale = Button->HasAnyUserFocus() ? 1.045f : 1.f;
			Button->SetRenderScale(FVector2D(Scale, Scale));
		}
	};
	for (const EFF_FestivalSettingsCategory Category : AllCategories)
	{
		RefreshButton(GetCategoryButton(Category));
	}
	RefreshButton(Button_Close);
	RefreshButton(GetBackButton());
	RefreshButton(GetSaveButton());
	RefreshButton(Button_QuitAction);
	RefreshButton(Button_QuitCancel);
	RefreshButton(Button_QuitConfirm);
}

void UFF_FestivalSettingsScreen::HandleSaveClicked()
{
	if (bAwaitingVideoModeConfirmation)
	{
		ConfirmVideoModeAndCommit();
		return;
	}
	if (!bDirty)
	{
		return;
	}
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!UserSettings)
	{
		return;
	}
	if (bExitConfirmationArmed)
	{
		// 하단 저장은 종료 확인과 독립된 전역 동작이다. 저장/해상도 확인을 시작하기 전에
		// 종료 모달을 닫아 두 확인 상태와 Back 의미가 겹치지 않게 한다.
		bExitConfirmationArmed = false;
		RebuildRows();
	}

	// Setter가 현재 값을 덮기 전에 실제 창 상태를 보관한다. 취소/타임아웃은 오래된
	// LastConfirmed 값이 아니라 이 스냅샷으로 명시적으로 돌아간다.
	CaptureActiveVideoModeForRollback(*UserSettings);
	UserSettings->SetFullscreenMode(Draft.WindowMode);
	UserSettings->SetScreenResolution(Draft.Resolution);
	if (UserSettings->IsScreenResolutionDirty() || UserSettings->IsFullscreenModeDirty())
	{
		BeginVideoModePreview(*UserSettings);
		return;
	}
	bHasPreviewRollbackVideoMode = false;
	CommitDraft();
}

void UFF_FestivalSettingsScreen::CaptureActiveVideoModeForRollback(
	const UGameUserSettings& UserSettings)
{
	PreviewRollbackWindowMode = UserSettings.GetFullscreenMode();
	PreviewRollbackResolution = UserSettings.GetScreenResolution();
	if (GEngine && GEngine->GameViewport)
	{
		const TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
		if (Window.IsValid())
		{
			PreviewRollbackWindowMode = Window->GetWindowMode();
		}
	}
	if (GSystemResolution.ResX > 0 && GSystemResolution.ResY > 0)
	{
		PreviewRollbackResolution = FIntPoint(GSystemResolution.ResX, GSystemResolution.ResY);
	}
	if (PreviewRollbackResolution.X <= 0 || PreviewRollbackResolution.Y <= 0)
	{
		const FIntPoint Desktop = UserSettings.GetDesktopResolution();
		PreviewRollbackResolution = Desktop.X > 0 && Desktop.Y > 0
			? Desktop : FIntPoint(1920, 1080);
	}
	bHasPreviewRollbackVideoMode = true;
}

void UFF_FestivalSettingsScreen::BeginVideoModePreview(UGameUserSettings& UserSettings)
{
	UserSettings.ApplyResolutionSettings(false);
	bAwaitingVideoModeConfirmation = true;
	VideoModeSecondsRemaining = FMath::Clamp(VideoModeConfirmationSeconds, 5.f, 30.f);
	SetNavigationEnabled(false);
	if (VideoModeTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(VideoModeTickerHandle);
	}
	VideoModeTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFF_FestivalSettingsScreen::TickVideoModeConfirmation), 0.1f);
	RefreshSummary();
	RefreshActionButtons();
}

bool UFF_FestivalSettingsScreen::TickVideoModeConfirmation(const float DeltaSeconds)
{
	if (!bAwaitingVideoModeConfirmation)
	{
		VideoModeTickerHandle.Reset();
		return false;
	}
	VideoModeSecondsRemaining -= FMath::Max(0.f, DeltaSeconds);
	if (VideoModeSecondsRemaining <= 0.f)
	{
		VideoModeTickerHandle.Reset();
		RollbackVideoModePreview();
		return false;
	}
	RefreshSummary();
	RefreshActionButtons();
	return true;
}

void UFF_FestivalSettingsScreen::ConfirmVideoModeAndCommit()
{
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!UserSettings)
	{
		RollbackVideoModePreview();
		return;
	}
	if (VideoModeTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(VideoModeTickerHandle);
		VideoModeTickerHandle.Reset();
	}

	bool bActualVideoModeAdjusted = false;
	// OS가 요청을 거부할 수 있으므로 실제 창 모드를 먼저 확정한 뒤 그 모드의 실제 크기를 저장한다.
	if (GEngine && GEngine->GameViewport)
	{
		const TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
		if (Window.IsValid() && Window->GetWindowMode() != Draft.WindowMode)
		{
			bActualVideoModeAdjusted = true;
			Draft.WindowMode = Window->GetWindowMode();
			UserSettings->SetFullscreenMode(Draft.WindowMode);
		}
	}
	const FIntPoint ActualResolution(GSystemResolution.ResX, GSystemResolution.ResY);
	if (ActualResolution.X > 0 && ActualResolution.Y > 0 && ActualResolution != Draft.Resolution)
	{
		bActualVideoModeAdjusted = true;
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("설정 해상도 %dx%d 대신 실제 %dx%d가 적용되어 실제 값을 저장한다."),
			Draft.Resolution.X, Draft.Resolution.Y, ActualResolution.X, ActualResolution.Y);
		Draft.Resolution = ActualResolution;
		UserSettings->SetScreenResolution(ActualResolution);
	}
	UserSettings->ConfirmVideoMode();
	bAwaitingVideoModeConfirmation = false;
	bHasPreviewRollbackVideoMode = false;
	SetNavigationEnabled(true);
	CommitDraft();
	if (bActualVideoModeAdjusted)
	{
		// 실제 적용된 비표준 창 크기도 이후 그래픽 탭에서 정확히 표시한다.
		bPreserveAppliedResolutionOption = true;
		if (SelectedCategory == EFF_FestivalSettingsCategory::Graphics)
		{
			// 실제 적용값이 요청과 달랐다면 행도 즉시 실제값으로 다시 그려 summary/저장값과 맞춘다.
			RebuildRows();
			if (Row_WindowMode)
			{
				Row_WindowMode->SetKeyboardFocus();
			}
		}
	}
}

void UFF_FestivalSettingsScreen::RollbackVideoModePreview()
{
	if (VideoModeTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(VideoModeTickerHandle);
		VideoModeTickerHandle.Reset();
	}
	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		if (!bHasPreviewRollbackVideoMode)
		{
			// 방어 경로: 정상 흐름에서는 HandleSaveClicked가 항상 실제 상태를 먼저 캡처한다.
			Draft.WindowMode = Saved.WindowMode;
			Draft.Resolution = Saved.Resolution;
			bPreserveAppliedResolutionOption = false;
			BuildResolutionOptions(*UserSettings);
			PreviewRollbackWindowMode = Draft.WindowMode;
			PreviewRollbackResolution = Draft.Resolution;
		}
		UserSettings->SetFullscreenMode(PreviewRollbackWindowMode);
		UserSettings->SetScreenResolution(PreviewRollbackResolution);
		UserSettings->ApplyResolutionSettings(false);

		// 적용 요청 자체도 OS가 보정할 수 있다. 복원 직후의 실제값을 다시 읽어 UI와 엔진 상태를
		// 일치시키고, LastConfirmed는 메모리에서만 안전값으로 갱신한다(SaveSettings는 호출하지 않는다).
		CaptureActiveVideoModeForRollback(*UserSettings);
		Draft.WindowMode = PreviewRollbackWindowMode;
		Draft.Resolution = PreviewRollbackResolution;
		UserSettings->SetFullscreenMode(Draft.WindowMode);
		UserSettings->SetScreenResolution(Draft.Resolution);
		UserSettings->ConfirmVideoMode();
		bPreserveAppliedResolutionOption = true;
	}
	bHasPreviewRollbackVideoMode = false;
	bAwaitingVideoModeConfirmation = false;
	VideoModeSecondsRemaining = 0.f;
	SetNavigationEnabled(true);
	if (!bIsTearingDown)
	{
		UpdateDirtyState();
		if (SelectedCategory == EFF_FestivalSettingsCategory::Graphics)
		{
			RebuildRows();
		}
		if (UButton* Back = GetBackButton())
		{
			Back->SetKeyboardFocus();
		}
	}
}

void UFF_FestivalSettingsScreen::CommitDraft()
{
	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		UserSettings->SetFullscreenMode(Draft.WindowMode);
		UserSettings->SetScreenResolution(Draft.Resolution);
		if (Draft.GraphicsPreset >= 0 && Draft.GraphicsPreset <= 4)
		{
			UserSettings->SetOverallScalabilityLevel(Draft.GraphicsPreset);
		}
		// 프리셋은 ResolutionQuality도 덮어쓰므로 사용자가 고른 렌더링 비율을 마지막에 둔다.
		UserSettings->SetResolutionScaleValueEx(Draft.ResolutionScalePercent);
		UserSettings->SetFrameRateLimit(static_cast<float>(Draft.FrameRateLimit));
		UserSettings->SetVSyncEnabled(Draft.bVSync);
		UserSettings->ApplyNonResolutionSettings();
		UserSettings->SaveSettings();
	}
	if (UFF_RuntimeSettingsSubsystem* Runtime = UFF_RuntimeSettingsSubsystem::Get(this))
	{
		Runtime->CommitSettings(Draft.MasterVolume, Draft.bZoomToCursor, Draft.bEdgeScroll);
	}
	if (UFF_UIAccessibilitySubsystem* Accessibility = UFF_UIAccessibilitySubsystem::Get(this))
	{
		Accessibility->SetReduceMotionEnabled(Draft.bReduceMotion);
	}
	bool bKeyBindingsCommitted = true;
	if (UFF_KeyBindingRuntimeSubsystem* Runtime = ResolveKeyBindingRuntime())
	{
		bKeyBindingsCommitted = Runtime->CommitKeyBindingEdit();
	}

	Saved = Draft;
	bDirty = !bKeyBindingsCommitted
		|| (ResolveKeyBindingRuntime() && ResolveKeyBindingRuntime()->HasPendingKeyBindingChanges());
	bSavedNotice = !bDirty;
	bNormalizedUnsupportedVideoMode = false;
	bPreserveAppliedResolutionOption = false;
	bDiscardConfirmationArmed = false;
	RefreshSummary();
	RefreshActionButtons();
	if (bDirty)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_FestivalSettingsScreen: 키 설정 저장을 완료하지 못해 변경 상태를 유지한다."));
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Log, TEXT("UFF_FestivalSettingsScreen: 사용자가 옵션을 저장했다."));
	}
}

void UFF_FestivalSettingsScreen::SetNavigationEnabled(const bool bEnabled)
{
	for (const EFF_FestivalSettingsCategory Category : AllCategories)
	{
		if (UButton* Button = GetCategoryButton(Category))
		{
			Button->SetIsEnabled(bEnabled);
		}
	}
	if (VerticalBox_Rows)
	{
		VerticalBox_Rows->SetIsEnabled(bEnabled);
	}
}

void UFF_FestivalSettingsScreen::HandleSelectionChanged(const FName SettingId, const int32 SelectedIndex)
{
	if (bAwaitingVideoModeConfirmation)
	{
		return;
	}
	if (SettingId == Setting_WindowMode && SelectedIndex >= 0 && SelectedIndex <= 2)
	{
		const EWindowMode::Type Modes[] = {
			EWindowMode::WindowedFullscreen, EWindowMode::Fullscreen, EWindowMode::Windowed
		};
		Draft.WindowMode = Modes[SelectedIndex];
		bPreserveAppliedResolutionOption = false;
		bNormalizedUnsupportedVideoMode = false;
		UpdateDirtyState();
		RebuildRows();
		if (Row_WindowMode)
		{
			Row_WindowMode->SetKeyboardFocus();
		}
		return;
	}
	if (SettingId == Setting_Resolution && ResolutionOptions.IsValidIndex(SelectedIndex))
	{
		Draft.Resolution = ResolutionOptions[SelectedIndex];
		bPreserveAppliedResolutionOption = false;
		bNormalizedUnsupportedVideoMode = false;
	}
	else if (SettingId == Setting_ResolutionScale && ResolutionScaleOptions.IsValidIndex(SelectedIndex))
	{
		Draft.ResolutionScalePercent = ResolutionScaleOptions[SelectedIndex];
	}
	else if (SettingId == Setting_FrameRateLimit && FrameRateOptions.IsValidIndex(SelectedIndex))
	{
		Draft.FrameRateLimit = FrameRateOptions[SelectedIndex];
	}
	else if (SettingId == Setting_GraphicsPreset && SelectedIndex >= 0 && SelectedIndex <= 4)
	{
		Draft.GraphicsPreset = SelectedIndex;
		UpdateDirtyState();
		RebuildRows();
		if (Row_GraphicsPreset)
		{
			Row_GraphicsPreset->SetKeyboardFocus();
		}
		return;
	}
	else
	{
		return;
	}
	UpdateDirtyState();
}

void UFF_FestivalSettingsScreen::HandleVSyncToggled(const bool bEnabled)
{
	if (bAwaitingVideoModeConfirmation)
	{
		return;
	}
	Draft.bVSync = bEnabled;
	UpdateDirtyState();
}

void UFF_FestivalSettingsScreen::HandleMasterVolumeChanged(const float Value)
{
	if (bAwaitingVideoModeConfirmation)
	{
		return;
	}
	Draft.MasterVolume = FMath::Clamp(Value, 0.f, 1.f);
	UpdateDirtyState();
}

void UFF_FestivalSettingsScreen::HandleZoomToCursorToggled(const bool bEnabled)
{
	if (bAwaitingVideoModeConfirmation)
	{
		return;
	}
	Draft.bZoomToCursor = bEnabled;
	UpdateDirtyState();
}

void UFF_FestivalSettingsScreen::HandleEdgeScrollToggled(const bool bEnabled)
{
	if (bAwaitingVideoModeConfirmation)
	{
		return;
	}
	Draft.bEdgeScroll = bEnabled;
	UpdateDirtyState();
}

void UFF_FestivalSettingsScreen::HandleReduceMotionToggled(const bool bEnabled)
{
	if (bAwaitingVideoModeConfirmation)
	{
		return;
	}
	Draft.bReduceMotion = bEnabled;
	UpdateDirtyState();
}

void UFF_FestivalSettingsScreen::HandleQuitGameClicked()
{
	bExitConfirmationArmed = true;
	RebuildRows();
	if (Button_QuitCancel)
	{
		Button_QuitCancel->SetKeyboardFocus();
	}
}

void UFF_FestivalSettingsScreen::HandleQuitGameCancelled()
{
	bExitConfirmationArmed = false;
	RebuildRows();
	if (Button_QuitAction)
	{
		Button_QuitAction->SetKeyboardFocus();
	}
}

void UFF_FestivalSettingsScreen::HandleQuitGameConfirmed()
{
	if (bQuitRequested || !bExitConfirmationArmed)
	{
		return;
	}
	bQuitRequested = true;
	if (UFF_KeyBindingRuntimeSubsystem* Runtime = ResolveKeyBindingRuntime())
	{
		Runtime->RollbackKeyBindingEdit();
	}
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UFF_FestivalSettingsScreen::HandleBasicClicked() { SelectCategory(EFF_FestivalSettingsCategory::Basic); }
void UFF_FestivalSettingsScreen::HandleAccountClicked() { SelectCategory(EFF_FestivalSettingsCategory::Account); }
void UFF_FestivalSettingsScreen::HandleKeyboardClicked() { SelectCategory(EFF_FestivalSettingsCategory::Keyboard); }
void UFF_FestivalSettingsScreen::HandleGraphicsClicked() { SelectCategory(EFF_FestivalSettingsCategory::Graphics); }
void UFF_FestivalSettingsScreen::HandleExitGameClicked() { SelectCategory(EFF_FestivalSettingsCategory::ExitGame); }
void UFF_FestivalSettingsScreen::HandleBackClicked() { RequestNavigateBack(); }

void UFF_FestivalSettingsScreen::EnsureExitCategoryButton()
{
	if (Button_ExitGame || (!HorizontalBox_Categories && !VerticalBox_Categories) || !WidgetTree)
	{
		return;
	}
	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>();
	Button_ExitGame = WidgetTree->ConstructWidget<UButton>();
	Text_ExitGameCategory = WidgetTree->ConstructWidget<UTextBlock>();
	if (!SizeBox || !Button_ExitGame || !Text_ExitGameCategory)
	{
		return;
	}
	if (Button_Basic)
	{
		Button_ExitGame->SetStyle(Button_Basic->GetStyle());
	}
	Text_ExitGameCategory->SetFont(DeriveFont(Text_BasicCategory, 18, TEXT("SemiBold")));
	Text_ExitGameCategory->SetText(GetCategoryLabel(EFF_FestivalSettingsCategory::ExitGame));
	if (UButtonSlot* ButtonLayout = Cast<UButtonSlot>(Button_ExitGame->AddChild(Text_ExitGameCategory)))
	{
		ButtonLayout->SetHorizontalAlignment(HAlign_Center);
		ButtonLayout->SetVerticalAlignment(VAlign_Center);
	}
	SizeBox->SetHeightOverride(HorizontalBox_Categories ? 58.f : 82.f);
	SizeBox->AddChild(Button_ExitGame);
	if (HorizontalBox_Categories)
	{
		if (UHorizontalBoxSlot* CategoryLayout = HorizontalBox_Categories->AddChildToHorizontalBox(SizeBox))
		{
			CategoryLayout->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			CategoryLayout->SetVerticalAlignment(VAlign_Center);
			CategoryLayout->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
	}
	else if (UVerticalBoxSlot* CategoryLayout = VerticalBox_Categories->AddChildToVerticalBox(SizeBox))
	{
		CategoryLayout->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}
}

UButton* UFF_FestivalSettingsScreen::GetCategoryButton(const EFF_FestivalSettingsCategory Category) const
{
	switch (Category)
	{
	case EFF_FestivalSettingsCategory::Basic: return Button_Basic;
	case EFF_FestivalSettingsCategory::Account: return Button_Account;
	case EFF_FestivalSettingsCategory::Keyboard: return Button_Keyboard;
	case EFF_FestivalSettingsCategory::Graphics: return Button_Graphics;
	case EFF_FestivalSettingsCategory::ExitGame: return Button_ExitGame;
	}
	return nullptr;
}

UTextBlock* UFF_FestivalSettingsScreen::GetCategoryText(const EFF_FestivalSettingsCategory Category) const
{
	switch (Category)
	{
	case EFF_FestivalSettingsCategory::Basic: return Text_BasicCategory;
	case EFF_FestivalSettingsCategory::Account: return Text_AccountCategory;
	case EFF_FestivalSettingsCategory::Keyboard: return Text_KeyboardCategory;
	case EFF_FestivalSettingsCategory::Graphics: return Text_GraphicsCategory;
	case EFF_FestivalSettingsCategory::ExitGame: return Text_ExitGameCategory;
	}
	return nullptr;
}

UButton* UFF_FestivalSettingsScreen::GetBackButton() const
{
	return Button_Back ? Button_Back.Get() : Button_Reset.Get();
}

UButton* UFF_FestivalSettingsScreen::GetSaveButton() const
{
	return Button_Save ? Button_Save.Get() : Button_Done.Get();
}

FText UFF_FestivalSettingsScreen::GetCategoryLabel(const EFF_FestivalSettingsCategory Category) const
{
	switch (Category)
	{
	case EFF_FestivalSettingsCategory::Basic: return LOCTEXT("CategoryBasic", "기본설정");
	case EFF_FestivalSettingsCategory::Account: return LOCTEXT("CategoryAccount", "계정정보");
	case EFF_FestivalSettingsCategory::Keyboard: return LOCTEXT("CategoryKeyboard", "키보드 키 배치");
	case EFF_FestivalSettingsCategory::Graphics: return LOCTEXT("CategoryGraphics", "그래픽");
	case EFF_FestivalSettingsCategory::ExitGame: return LOCTEXT("CategoryExitGame", "게임종료");
	}
	return FText::GetEmpty();
}

void UFF_FestivalSettingsScreen::SetButtonLabel(
	UButton* Button, UTextBlock* BoundText, const FText& Label) const
{
	if (!Button)
	{
		return;
	}
	UTextBlock* Text = BoundText;
	if (!Text && Button->GetChildrenCount() > 0)
	{
		Text = Cast<UTextBlock>(Button->GetChildAt(0));
	}
	if (Text)
	{
		Text->SetText(Label);
	}
}

#undef LOCTEXT_NAMESPACE
