// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Frontend/FF_OptionsScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "UI/Frontend/FF_FrontendCasualStyle.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "UI/Core/FF_UIAccessibilitySubsystem.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/FF_UILog.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"
#include "UI/Settings/FF_KeyBindingPanel.h"
#include "UI/Settings/FF_RuntimeSettingsSubsystem.h"
#include "UI/Settings/FF_SettingSelectRow.h"
#include "UI/Settings/FF_SettingSliderRow.h"
#include "UI/Settings/FF_SettingToggleRow.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

namespace
{
	/** 공유 설정 행의 이 화면 인스턴스만 꾸민다. ESC 쪽 WBP·기본값은 그대로 둔다. */
	void ApplyCasualRowStyle(UUserWidget* Row)
	{
		if (!Row || !Row->WidgetTree) return;
		const auto T = FFFrontendCasual::Get();
		TArray<UWidget*> Widgets;
		Row->WidgetTree->GetAllWidgets(Widgets);
		for (UWidget* Widget : Widgets)
		{
			if (UBorder* Border = Cast<UBorder>(Widget))
			{
				Border->SetBrush(FFFrontendCasual::Rounded(T.White, T.Line, 18.f));
				Border->SetBrushColor(FLinearColor::White);
			}
			if (UTextBlock* Text = Cast<UTextBlock>(Widget))
			{
				const bool bDescription = Text->GetName().Contains(TEXT("Description"));
				Text->SetColorAndOpacity(FSlateColor(bDescription ? T.InkMuted : T.Ink));
				FSlateFontInfo Font = Text->GetFont();
				Font.Size = bDescription ? 13 : 18;
				Text->SetFont(Font);
			}
			if (UButton* Button = Cast<UButton>(Widget))
			{
				FButtonStyle Style = Button->GetStyle();
				Style.SetNormal(FFFrontendCasual::Rounded(T.Hover, T.LineStrong, 12.f));
				Style.SetHovered(FFFrontendCasual::Rounded(T.TealSoft, T.Teal, 12.f, 2.f));
				Style.SetPressed(FFFrontendCasual::Rounded(T.TealSoft, T.TealDeep, 12.f, 2.f));
				Button->SetStyle(Style);
			}
			if (USlider* Slider = Cast<USlider>(Widget))
			{
				Slider->SetSliderBarColor(T.LineStrong);
				Slider->SetSliderHandleColor(T.Teal);
			}
			if (UCheckBox* Check = Cast<UCheckBox>(Widget))
			{
				FCheckBoxStyle Style = Check->GetWidgetStyle();
				Style.UncheckedImage = FFFrontendCasual::Rounded(T.White, T.LineStrong, 7.f, 2.f);
				Style.UncheckedImage.ImageSize = FVector2D(28, 28);
				Style.UncheckedHoveredImage = Style.UncheckedImage;
				Style.UncheckedPressedImage = Style.UncheckedImage;
				// 체크 표시는 기존 체크 텍스처를 보존하고 틴트·크기만 통일한다.
				Style.CheckedImage.TintColor = T.Teal;
				Style.CheckedImage.ImageSize = FVector2D(28, 28);
				Style.CheckedHoveredImage = Style.CheckedImage;
				Style.CheckedPressedImage = Style.CheckedImage;
				Check->SetWidgetStyle(Style);
			}
		}
	}
	// 옵션 분류 여섯 개. 구독·문구·되돌아보기가 같은 목록을 봐야 한다.
	const EFF_OptionCategory AllOptionCategories[] = {
		EFF_OptionCategory::Language,
		EFF_OptionCategory::Accessibility,
		EFF_OptionCategory::Sound,
		EFF_OptionCategory::Graphics,
		EFF_OptionCategory::Controls,
		EFF_OptionCategory::Credits
	};

	// WBP 에서 행 클래스를 지정하지 않았을 때의 대비책.
	// 🔴 이 문자열 경로는 쿠커의 참조 그래프에 잡히지 않는다. 실제 참조는 WBP 의
	//    `ToggleRowClass` 등이 만들고, 여기는 그 지정을 잊었을 때만 쓰인다.
	//    폴더는 `DirectoriesToAlwaysCook` 에 이미 들어 있다 (`docs/memory/prep-hud-reference.md` 5절).
	const TCHAR* ToggleRowFallbackPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_SettingToggleRow.WBP_FF_SettingToggleRow_C");
	const TCHAR* SliderRowFallbackPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_SettingSliderRow.WBP_FF_SettingSliderRow_C");
	const TCHAR* SelectRowFallbackPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_SettingSelectRow.WBP_FF_SettingSelectRow_C");
	const TCHAR* KeyBindingPanelFallbackPath =
		TEXT("/Game/FestaFiesta/UI/Settings/WBP_FF_KeyBindingPanel.WBP_FF_KeyBindingPanel_C");

	// 그래픽 선택 행의 식별자. 행 하나가 콜백 하나를 공유하므로 이름으로 구분한다.
	const FName Setting_WindowMode(TEXT("WindowMode"));
	const FName Setting_Resolution(TEXT("Resolution"));
	const FName Setting_ResolutionScale(TEXT("ResolutionScale"));
	const FName Setting_FrameRateLimit(TEXT("FrameRateLimit"));
	const FName Setting_QualityPreset(TEXT("QualityPreset"));

	/** 행 사이 간격. 값 하나라 여기 상수로 둔다 — 좌표는 디자이너가 소유한다. */
	constexpr float RowGap = 8.f;

	/**
	 * 이미 화면에 있는 글자에서 폰트를 베껴 크기만 바꾼다.
	 *
	 * 런타임에 만든 `UTextBlock` 은 폰트가 비어 있어 기본 폰트로 떨어진다. 그러면 같은 화면에서
	 * 분류 제목과 설명 글꼴이 다르게 보인다. 폰트 에셋 경로를 여기 박지 않는 이유는
	 * 폰트 교체가 마감 단계 항목이라 코드가 그 원본이 되면 안 되기 때문이다.
	 */
	FSlateFontInfo DeriveFont(const UTextBlock* Source, const int32 Size, const FName Typeface)
	{
		FSlateFontInfo Font = Source ? Source->GetFont() : FSlateFontInfo();
		Font.Size = Size;
		if (!Typeface.IsNone())
		{
			Font.TypefaceFontName = Typeface;
		}
		return Font;
	}
}

bool UFF_OptionsScreen::FDraft::Equals(const FDraft& Other) const
{
	return FMath::IsNearlyEqual(MasterVolume, Other.MasterVolume, 0.001f)
		&& bZoomToCursor == Other.bZoomToCursor
		&& bEdgeScroll == Other.bEdgeScroll;
}

UFF_OptionsScreen::UFF_OptionsScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 크레딧 기본값. **원천은 THIRD_PARTY_NOTICES.md 의 등록 목록이고 여기는 사본이다.**
	// 에셋을 새로 들일 때 그 문서와 이 목록을 같이 고친다.
	const auto AddCredit = [this](const FText& Asset, const FText& Usage, const FText& License)
	{
		FFF_OptionCreditEntry Entry;
		Entry.Asset = Asset;
		Entry.Usage = Usage;
		Entry.License = License;
		CreditEntries.Add(MoveTemp(Entry));
	};

	AddCredit(
		LOCTEXT("CreditSolarSystemScope", "Solar System Scope Planet Textures"),
		LOCTEXT("CreditSolarSystemScopeUse", "지구·구름·노멀·스페큘러 텍스처 (FestaFiesta용 수정)"),
		LOCTEXT("CreditCcBy40", "CC BY 4.0"));
	AddCredit(
		LOCTEXT("CreditPretendard", "Pretendard"),
		LOCTEXT("CreditPretendardUse", "한국어·공통 UI 폰트"),
		LOCTEXT("CreditOfl11", "SIL OFL 1.1"));
	AddCredit(
		LOCTEXT("CreditMichroma", "Michroma"),
		LOCTEXT("CreditMichromaUse", "영문 브랜드 워드마크 폰트"),
		LOCTEXT("CreditOfl11Michroma", "SIL OFL 1.1"));
	AddCredit(
		LOCTEXT("CreditDongle", "Dongle"),
		LOCTEXT("CreditDongleUse", "영문 브랜드 워드마크 비교용 폰트"),
		LOCTEXT("CreditOfl11Dongle", "SIL OFL 1.1"));
	AddCredit(
		LOCTEXT("CreditCopernicus", "Copernicus DEM GLO-30"),
		LOCTEXT("CreditCopernicusUse", "국가 지도 고도·산악 음영"),
		LOCTEXT("CreditCopernicusLicense", "Copernicus WorldDEM-30 User Licence"));
	AddCredit(
		LOCTEXT("CreditGeoBoundaries", "geoBoundaries KOR ADM1"),
		LOCTEXT("CreditGeoBoundariesUse", "행정구역 경계·권역 재구성"),
		LOCTEXT("CreditCcBy40Geo", "CC BY 4.0"));
}

#if WITH_EDITOR
void UFF_OptionsScreen::ApplyGeneratedRowClasses(
	TSubclassOf<UFF_SettingToggleRow> InToggleRowClass,
	TSubclassOf<UFF_SettingSliderRow> InSliderRowClass,
	TSubclassOf<UFF_SettingSelectRow> InSelectRowClass,
	TSubclassOf<UFF_KeyBindingPanel> InKeyBindingPanelClass)
{
	Modify();
	ToggleRowClass = InToggleRowClass;
	SliderRowClass = InSliderRowClass;
	SelectRowClass = InSelectRowClass;
	KeyBindingPanelClass = InKeyBindingPanelClass;
}
#endif // WITH_EDITOR

void UFF_OptionsScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 문구는 인스턴스에 한 번만 넣으면 된다.
	for (const EFF_OptionCategory Category : AllOptionCategories)
	{
		if (UFF_CommonButtonBase* Button = GetCategoryButton(Category))
		{
			// 🔴 `UEnum::GetDisplayValueAsText` 를 쓰지 않는다 — 패키징에서 영문이 뜬다.
			Button->SetButtonLabel(UFF_UIDisplayTextLibrary::GetOptionCategoryText(Category));

			// 고른 분류는 **누른 뒤에도 그 상태로 남아 있어야 한다.** 마우스를 치우면 표시가
			// 사라지던 것이 실기 지적 사항이었다 (2026-09-10). 표시는 CBS 의 Selected 브러시가 낸다.
			// `Toggleable` 을 켜는 이유는 그것이 꺼져 있으면 **이전 분류를 해제할 수 없기** 때문이다
			// (`UCommonButtonBase::SetIsSelected` 는 해제 경로에서 이 값을 본다).
			Button->SetIsSelectable(true);
			Button->SetIsToggleable(true);
		}
		else
		{
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("UFF_OptionsScreen: `%s` 분류 버튼 바인딩이 없다."),
				*UFF_UIDisplayTextLibrary::GetOptionCategoryText(Category).ToString());
		}
	}

	if (Button_Back)
	{
		Button_Back->SetButtonLabel(LOCTEXT("OptionsBack", "뒤로"));
	}

	if (Button_Apply)
	{
		Button_Apply->SetButtonLabel(LOCTEXT("OptionsApply", "적용"));
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_OptionsScreen: `Button_Apply` 가 없어 값 변경을 즉시 저장한다. "
				 "WBP 를 `FF.ApplyOptionsSetupStyle` 로 다시 만들면 `적용` 버튼이 생긴다."));
	}
}

void UFF_OptionsScreen::NativeConstruct()
{
	Super::NativeConstruct();

	// 🔴 구독은 `NativeConstruct` 다. CommonUI 스택이 위젯을 풀에서 재사용하므로
	// `NativeOnInitialized`(인스턴스당 한 번)에 구독하고 `NativeDestruct`(내려갈 때마다)에
	// 해제하면, 뒤로 갔다 다시 들어온 화면은 **버튼이 죽는다.**
	// 버튼이 자기를 넘겨주므로 핸들러 하나로 구분한다.
	for (const EFF_OptionCategory Category : AllOptionCategories)
	{
		if (UFF_CommonButtonBase* Button = GetCategoryButton(Category))
		{
			Button->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_OptionsScreen::HandleCategoryClicked);
		}
	}

	if (Button_Back)
	{
		Button_Back->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_OptionsScreen::HandleBackClicked);
	}

	if (Button_Apply)
	{
		Button_Apply->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_OptionsScreen::HandleApplyClicked);
	}
}

void UFF_OptionsScreen::NativeOnActivated()
{
	// 저장값을 먼저 읽는다. 행이 그 값을 초기값으로 쓴다.
	CaptureDraft();

	// 풀에서 다시 열린 화면은 마지막 분류를 복원하고, 최초 진입만 기본 분류를 쓴다.
	bSavedNotice = false;
	SelectCategory(bHasSelectedCategory ? SelectedCategory : DefaultCategory);

	// 포커스는 베이스가 NativeGetDesiredFocusTarget 으로 잡는다.
	// 그 함수가 SelectedCategory 를 보므로 반드시 SelectCategory 뒤에 Super 를 부른다.
	Super::NativeOnActivated();
}

void UFF_OptionsScreen::NativeDestruct()
{
	// 해제는 조건부, 정리는 무조건.
	for (const EFF_OptionCategory Category : AllOptionCategories)
	{
		if (UFF_CommonButtonBase* Button = GetCategoryButton(Category))
		{
			Button->OnFFButtonClicked.RemoveDynamic(this, &UFF_OptionsScreen::HandleCategoryClicked);
		}
	}

	if (Button_Back)
	{
		Button_Back->OnFFButtonClicked.RemoveDynamic(this, &UFF_OptionsScreen::HandleBackClicked);
	}

	if (Button_Apply)
	{
		Button_Apply->OnFFButtonClicked.RemoveDynamic(this, &UFF_OptionsScreen::HandleApplyClicked);
	}

	// 🔴 확인을 기다리는 중에 화면이 닫히면 되돌린다. 확정하지 않은 화면 모드를 남기면
	// 플레이어가 "유지" 를 누를 기회 없이 깨진 화면에 갇힐 수 있다.
	if (bAwaitingVideoConfirm)
	{
		RollbackVideoMode();
	}

	// 키 캡처 lease 는 위젯 소멸에 맡기지 않는다 (`UFF_KeyBindingPanel::CancelKeyCapture` 주석).
	if (UFF_KeyBindingPanel* Panel = ActiveKeyBindingPanel.Get())
	{
		Panel->CancelKeyCapture();
	}
	ActiveKeyBindingPanel = nullptr;

	Super::NativeDestruct();
}

UWidget* UFF_OptionsScreen::NativeGetDesiredFocusTarget() const
{
	if (UFF_CommonButtonBase* Selected = GetCategoryButton(SelectedCategory))
	{
		return Selected;
	}
	return Super::NativeGetDesiredFocusTarget();
}

FReply UFF_OptionsScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// 리바인딩 중에는 화면이 키를 먹지 않고 패널에 넘긴다. 포커스가 분류 버튼에 있어도 되게 하려는 것.
	if (UFF_KeyBindingPanel* Panel = ActiveKeyBindingPanel.Get())
	{
		if (Panel->IsCapturingKey() && Panel->HandlePhysicalKey(InKeyEvent.GetKey()))
		{
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UFF_OptionsScreen::SelectCategory(EFF_OptionCategory Category)
{
	SelectedCategory = Category;
	bHasSelectedCategory = true;

	// 부분 갱신을 하지 않는다. 여섯 개의 선택 상태를 매번 전체로 다시 선언한다.
	for (const EFF_OptionCategory Each : AllOptionCategories)
	{
		if (UFF_CommonButtonBase* Button = GetCategoryButton(Each))
		{
			// 클릭음은 버튼이 이미 냈다. 여기서 또 내면 두 번 울린다.
			Button->SetIsSelected(Each == SelectedCategory, /*bGiveClickFeedback=*/ false);
		}
	}

	if (Text_CategoryTitle)
	{
		Text_CategoryTitle->SetText(UFF_UIDisplayTextLibrary::GetOptionCategoryText(Category));
	}

	RebuildRows();
	RefreshDirtyState();
}

void UFF_OptionsScreen::RebuildRows()
{
	// 🔴 행을 지우기 **전에** 키 캡처를 해제한다. 캡처를 쥔 패널을 그냥 버리면
	//    입력 lease 가 위젯 소멸 시점까지 남아 다음 화면의 키가 먹지 않는다.
	if (UFF_KeyBindingPanel* Panel = ActiveKeyBindingPanel.Get())
	{
		Panel->CancelKeyCapture();
	}
	ActiveKeyBindingPanel = nullptr;

	if (Text_NotImplemented)
	{
		// 사유는 `AddPendingRow` 가 다시 켠다. 여기서는 무조건 접는다.
		Text_NotImplemented->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!VerticalBox_Rows)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_OptionsScreen: `VerticalBox_Rows` 바인딩이 없어 설정 행을 그리지 못했다."));
		return;
	}

	VerticalBox_Rows->ClearChildren();

	switch (SelectedCategory)
	{
	case EFF_OptionCategory::Language:		BuildLanguageRows();		break;
	case EFF_OptionCategory::Accessibility:	BuildAccessibilityRows();	break;
	case EFF_OptionCategory::Sound:			BuildSoundRows();			break;
	case EFF_OptionCategory::Graphics:		BuildGraphicsRows();		break;
	case EFF_OptionCategory::Controls:		BuildControlsRows();		break;
	case EFF_OptionCategory::Credits:		BuildCreditsRows();			break;
	}
}

void UFF_OptionsScreen::BuildLanguageRows()
{
	// 고를 것이 없을 때 선택 UI 를 만들지 않는다. 만져도 안 바뀌면 고장으로 보인다.
	AddInfoRow(
		LOCTEXT("OptionLanguageLabel", "표시 언어"),
		LOCTEXT("OptionLanguageValue", "한국어"),
		LOCTEXT("OptionLanguageDesc", "현지화 데이터가 아직 없어 한국어만 지원합니다"));
}

void UFF_OptionsScreen::BuildAccessibilityRows()
{
	const UFF_UIAccessibilitySubsystem* Accessibility = UFF_UIAccessibilitySubsystem::Get(this);
	if (!Accessibility)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_OptionsScreen: 접근성 서브시스템을 찾지 못했다. 접근성 행을 그리지 않았다."));
		AddPendingRow(LOCTEXT("OptionAccessibilityMissing",
			"접근성 설정을 읽지 못했습니다 — 로그를 확인하세요"));
		return;
	}

	// 값은 서브시스템이 소유한다. 이 화면은 현재값을 읽어 표시할 뿐이다.
	if (UFF_SettingToggleRow* Row = AddToggleRow(
		LOCTEXT("OptionReduceMotion", "움직임 감소"),
		LOCTEXT("OptionReduceMotionDesc", "화면 전환과 패널 애니메이션을 끕니다 (바꾸면 바로 반영됩니다)"),
		Accessibility->IsReduceMotionEnabled()))
	{
		Row->OnValueChanged.AddUniqueDynamic(this, &UFF_OptionsScreen::HandleReduceMotionToggled);
	}
}

void UFF_OptionsScreen::BuildSoundRows()
{
	if (UFF_SettingSliderRow* Row = AddSliderRow(
		LOCTEXT("OptionMasterVolume", "마스터 음량"),
		LOCTEXT("OptionMasterVolumeDesc", "게임 전체 소리의 크기입니다"),
		Draft.MasterVolume, 0.f, 1.f, 0.05f))
	{
		Row->OnValueChanged.AddUniqueDynamic(this, &UFF_OptionsScreen::HandleMasterVolumeChanged);
	}

	AddInfoRow(
		LOCTEXT("OptionSoundBusLabel", "개별 음량"),
		LOCTEXT("OptionSoundBusValue", "준비 중"),
		LOCTEXT("OptionSoundBusDesc", "BGM·효과음 분리는 사운드 담당의 SoundClass 구성이 들어온 뒤에 붙입니다"));
}

void UFF_OptionsScreen::BuildGraphicsRows()
{
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!UserSettings)
	{
		AddPendingRow(LOCTEXT("OptionGraphicsMissing",
			"그래픽 설정을 읽지 못했습니다 — 로그를 확인하세요"));
		return;
	}

	if (UFF_SettingSelectRow* Row = AddSelectRow(
		Setting_WindowMode,
		LOCTEXT("OptionWindowMode", "창 모드"),
		LOCTEXT("OptionWindowModeDesc", "전체 화면 창은 다른 창으로 전환이 빠릅니다"),
		GetWindowModeOptions(),
		static_cast<int32>(GraphicsDraft.WindowMode)))
	{
		Row->OnSelectionChanged.AddUniqueDynamic(
			this, &UFF_OptionsScreen::HandleGraphicsSelectionChanged);
	}

	TArray<FText> ResolutionTexts;
	ResolutionTexts.Reserve(ResolutionOptions.Num());
	for (const FIntPoint& Option : ResolutionOptions)
	{
		ResolutionTexts.Add(FText::Format(
			LOCTEXT("OptionResolutionFormat", "{0} × {1}"),
			FText::AsNumber(Option.X), FText::AsNumber(Option.Y)));
	}
	if (UFF_SettingSelectRow* Row = AddSelectRow(
		Setting_Resolution,
		LOCTEXT("OptionResolution", "해상도"),
		LOCTEXT("OptionResolutionDesc", "목록은 Project Settings 의 해상도 후보에서 옵니다"),
		ResolutionTexts,
		ResolutionOptions.IndexOfByKey(GraphicsDraft.Resolution)))
	{
		Row->OnSelectionChanged.AddUniqueDynamic(
			this, &UFF_OptionsScreen::HandleGraphicsSelectionChanged);
	}

	TArray<FText> ScaleTexts;
	ScaleTexts.Reserve(ResolutionScaleOptions.Num());
	for (const int32 Option : ResolutionScaleOptions)
	{
		ScaleTexts.Add(FText::Format(
			LOCTEXT("OptionPercentFormat", "{0}%"), FText::AsNumber(Option)));
	}
	if (UFF_SettingSelectRow* Row = AddSelectRow(
		Setting_ResolutionScale,
		LOCTEXT("OptionResolutionScale", "해상도 스케일"),
		LOCTEXT("OptionResolutionScaleDesc", "낮추면 성능이 오르고 화면이 부드러워집니다"),
		ScaleTexts,
		ResolutionScaleOptions.IndexOfByKey(GraphicsDraft.ResolutionScalePercent)))
	{
		Row->OnSelectionChanged.AddUniqueDynamic(
			this, &UFF_OptionsScreen::HandleGraphicsSelectionChanged);
	}

	TArray<FText> FrameRateTexts;
	FrameRateTexts.Reserve(FrameRateOptions.Num());
	for (const int32 Option : FrameRateOptions)
	{
		FrameRateTexts.Add(Option <= 0
			? LOCTEXT("OptionFrameRateUnlimited", "무제한")
			: FText::Format(LOCTEXT("OptionFrameRateFormat", "{0} FPS"), FText::AsNumber(Option)));
	}
	if (UFF_SettingSelectRow* Row = AddSelectRow(
		Setting_FrameRateLimit,
		LOCTEXT("OptionFrameRateLimit", "프레임 제한"),
		LOCTEXT("OptionFrameRateLimitDesc", "제한을 두면 발열과 소음이 줄어듭니다"),
		FrameRateTexts,
		FrameRateOptions.IndexOfByKey(GraphicsDraft.FrameRateLimit)))
	{
		Row->OnSelectionChanged.AddUniqueDynamic(
			this, &UFF_OptionsScreen::HandleGraphicsSelectionChanged);
	}

	const TArray<FText> QualityTexts = GetQualityPresetOptions();
	if (UFF_SettingSelectRow* Row = AddSelectRow(
		Setting_QualityPreset,
		LOCTEXT("OptionQualityPreset", "품질 프리셋"),
		LOCTEXT("OptionQualityPresetDesc", "그림자·안티에일리어싱·이펙트 단계를 한 번에 바꿉니다"),
		QualityTexts,
		GraphicsDraft.QualityPreset >= 0 ? GraphicsDraft.QualityPreset : QualityTexts.Num() - 1))
	{
		Row->OnSelectionChanged.AddUniqueDynamic(
			this, &UFF_OptionsScreen::HandleGraphicsSelectionChanged);
	}

	if (UFF_SettingToggleRow* Row = AddToggleRow(
		LOCTEXT("OptionVSync", "수직 동기화"),
		LOCTEXT("OptionVSyncDesc", "화면 찢김을 막지만 입력이 조금 늦어집니다"),
		GraphicsDraft.bVSync))
	{
		Row->OnValueChanged.AddUniqueDynamic(this, &UFF_OptionsScreen::HandleVSyncToggled);
	}

	// 되돌림 규칙을 화면에 적는다. 모르면 15초 뒤에 설정이 저절로 풀린 것처럼 보인다.
	AddInfoRow(
		LOCTEXT("OptionGraphicsConfirmLabel", "해상도·창 모드"),
		LOCTEXT("OptionGraphicsConfirmValue", "적용 뒤 15초 안에 `유지`"),
		LOCTEXT("OptionGraphicsConfirmDesc",
			"잘못 고르면 화면이 보이지 않을 수 있어, 누르지 않으면 이전 설정으로 되돌립니다"));
}

void UFF_OptionsScreen::BuildControlsRows()
{
	if (UFF_SettingToggleRow* Row = AddToggleRow(
		LOCTEXT("OptionZoomToCursor", "커서 기준 줌"),
		LOCTEXT("OptionZoomToCursorDesc", "커서 아래 지점을 향해 확대합니다"),
		Draft.bZoomToCursor))
	{
		Row->OnValueChanged.AddUniqueDynamic(this, &UFF_OptionsScreen::HandleZoomToCursorToggled);
	}

	if (UFF_SettingToggleRow* Row = AddToggleRow(
		LOCTEXT("OptionEdgeScroll", "가장자리 스크롤"),
		LOCTEXT("OptionEdgeScrollDesc", "커서를 화면 끝에 대면 카메라가 이동합니다"),
		Draft.bEdgeScroll))
	{
		Row->OnValueChanged.AddUniqueDynamic(this, &UFF_OptionsScreen::HandleEdgeScrollToggled);
	}

	// 키 배치는 한광훈님 패널을 그대로 얹는다. 패널이 스스로 프로필을 읽고 행을 만든다.
	const TSubclassOf<UFF_KeyBindingPanel> PanelClass = ResolveKeyBindingPanelClass();
	if (!PanelClass)
	{
		AddPendingRow(LOCTEXT("OptionControlsNoPanel",
			"키 배치 패널을 불러오지 못했습니다 — WBP 의 `KeyBindingPanelClass` 를 확인하세요"));
		return;
	}

	UFF_KeyBindingPanel* Panel = CreateWidget<UFF_KeyBindingPanel>(this, PanelClass);
	if (!Panel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_OptionsScreen: 키 배치 패널 생성 실패 (%s)."), *PanelClass->GetName());
		return;
	}

	if (UVerticalBoxSlot* Layout = VerticalBox_Rows->AddChildToVerticalBox(Panel))
	{
		Layout->SetPadding(FMargin(0.f, RowGap, 0.f, 0.f));
	}
	ActiveKeyBindingPanel = Panel;
}

void UFF_OptionsScreen::BuildCreditsRows()
{
	if (CreditEntries.IsEmpty())
	{
		AddPendingRow(LOCTEXT("OptionCreditsEmpty",
			"크레딧 목록이 비었습니다 — THIRD_PARTY_NOTICES.md 의 등록 목록을 WBP 에 채우세요"));
		return;
	}

	// 라이선스가 요구하는 표기 문구다. 지우면 CC BY 4.0 조건을 어긴다.
	AddInfoRow(
		LOCTEXT("OptionCreditNoticeLabel", "표기"),
		LOCTEXT("OptionCreditNoticeValue",
			"행성 텍스처: Solar System Scope(INOVE), CC BY 4.0. FestaFiesta용으로 수정됨."),
		LOCTEXT("OptionCreditNoticeDesc", "전체 출처·라이선스 원문은 배포물의 THIRD_PARTY_NOTICES 에 있습니다"));

	for (const FFF_OptionCreditEntry& Entry : CreditEntries)
	{
		AddInfoRow(Entry.Asset, Entry.License, Entry.Usage);
	}
}

UFF_CommonButtonBase* UFF_OptionsScreen::GetCategoryButton(EFF_OptionCategory Category) const
{
	switch (Category)
	{
	case EFF_OptionCategory::Language:		return Button_Language;
	case EFF_OptionCategory::Accessibility:	return Button_Accessibility;
	case EFF_OptionCategory::Sound:			return Button_Sound;
	case EFF_OptionCategory::Graphics:		return Button_Graphics;
	case EFF_OptionCategory::Controls:		return Button_Controls;
	case EFF_OptionCategory::Credits:		return Button_Credits;
	}
	return nullptr;
}

TSubclassOf<UFF_SettingToggleRow> UFF_OptionsScreen::ResolveToggleRowClass() const
{
	if (ToggleRowClass)
	{
		return ToggleRowClass;
	}
	return LoadClass<UFF_SettingToggleRow>(nullptr, ToggleRowFallbackPath);
}

TSubclassOf<UFF_SettingSliderRow> UFF_OptionsScreen::ResolveSliderRowClass() const
{
	if (SliderRowClass)
	{
		return SliderRowClass;
	}
	return LoadClass<UFF_SettingSliderRow>(nullptr, SliderRowFallbackPath);
}

TSubclassOf<UFF_KeyBindingPanel> UFF_OptionsScreen::ResolveKeyBindingPanelClass() const
{
	if (KeyBindingPanelClass)
	{
		return KeyBindingPanelClass;
	}
	return LoadClass<UFF_KeyBindingPanel>(nullptr, KeyBindingPanelFallbackPath);
}

UFF_SettingToggleRow* UFF_OptionsScreen::AddToggleRow(
	const FText& Label, const FText& Description, const bool bInitialValue)
{
	const TSubclassOf<UFF_SettingToggleRow> RowClass = ResolveToggleRowClass();
	if (!RowClass || !VerticalBox_Rows)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_OptionsScreen: 토글 행 클래스를 찾지 못해 `%s` 를 그리지 못했다."), *Label.ToString());
		return nullptr;
	}

	UFF_SettingToggleRow* Row = CreateWidget<UFF_SettingToggleRow>(this, RowClass);
	if (!Row)
	{
		return nullptr;
	}

	Row->Configure(Label, Description, bInitialValue);
	ApplyCasualRowStyle(Row);
	if (UVerticalBoxSlot* Layout = VerticalBox_Rows->AddChildToVerticalBox(Row))
	{
		Layout->SetPadding(FMargin(0.f, 0.f, 0.f, RowGap));
	}
	return Row;
}

UFF_SettingSliderRow* UFF_OptionsScreen::AddSliderRow(
	const FText& Label, const FText& Description, const float InitialValue,
	const float MinValue, const float MaxValue, const float StepSize)
{
	const TSubclassOf<UFF_SettingSliderRow> RowClass = ResolveSliderRowClass();
	if (!RowClass || !VerticalBox_Rows)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_OptionsScreen: 슬라이더 행 클래스를 찾지 못해 `%s` 를 그리지 못했다."), *Label.ToString());
		return nullptr;
	}

	UFF_SettingSliderRow* Row = CreateWidget<UFF_SettingSliderRow>(this, RowClass);
	if (!Row)
	{
		return nullptr;
	}

	Row->Configure(Label, Description, InitialValue, MinValue, MaxValue, StepSize);
	ApplyCasualRowStyle(Row);
	if (UVerticalBoxSlot* Layout = VerticalBox_Rows->AddChildToVerticalBox(Row))
	{
		Layout->SetPadding(FMargin(0.f, 0.f, 0.f, RowGap));
	}
	return Row;
}

void UFF_OptionsScreen::AddInfoRow(const FText& Label, const FText& Value, const FText& Description)
{
	if (!VerticalBox_Rows || !WidgetTree)
	{
		return;
	}

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();

	UTextBlock* Line = WidgetTree->ConstructWidget<UTextBlock>();
	if (!Line)
	{
		return;
	}

	Line->SetText(FText::Format(LOCTEXT("OptionInfoRowFormat", "{0} — {1}"), Label, Value));
	Line->SetFont(DeriveFont(Text_CategoryTitle, 18, TEXT("Medium")));
	if (Palette)
	{
		Line->SetColorAndOpacity(FSlateColor(Palette->InGame.Ink));
	}
	Line->SetAutoWrapText(true);
	if (UVerticalBoxSlot* Layout = VerticalBox_Rows->AddChildToVerticalBox(Line))
	{
		Layout->SetPadding(FMargin(0.f, 0.f, 0.f, Description.IsEmpty() ? RowGap : 2.f));
	}

	if (Description.IsEmpty())
	{
		return;
	}

	UTextBlock* Sub = WidgetTree->ConstructWidget<UTextBlock>();
	if (!Sub)
	{
		return;
	}

	Sub->SetText(Description);
	Sub->SetFont(DeriveFont(Text_CategoryTitle, 14, TEXT("Regular")));
	if (Palette)
	{
		Sub->SetColorAndOpacity(FSlateColor(Palette->InGame.InkMuted));
	}
	Sub->SetAutoWrapText(true);
	if (UVerticalBoxSlot* Layout = VerticalBox_Rows->AddChildToVerticalBox(Sub))
	{
		Layout->SetPadding(FMargin(0.f, 0.f, 0.f, RowGap));
	}
}

void UFF_OptionsScreen::AddPendingRow(const FText& Reason)
{
	// 바인딩된 사유 글자가 있으면 그것을 쓴다 — 디자이너에서 자리를 잡아 둔 글자다.
	if (Text_NotImplemented)
	{
		Text_NotImplemented->SetText(Reason);
		Text_NotImplemented->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	AddInfoRow(LOCTEXT("OptionPendingLabel", "준비 중"), Reason);
}

void UFF_OptionsScreen::CaptureDraft()
{
	const UFF_RuntimeSettingsSubsystem* Runtime = UFF_RuntimeSettingsSubsystem::Get(this);
	if (!Runtime)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_OptionsScreen: 런타임 설정 서브시스템을 찾지 못했다. 기본값을 보여준다."));
		// 그래픽은 다른 소유자(엔진)라 여기서 멈추지 않는다.
		CaptureGraphicsDraft();
		return;
	}

	Saved.MasterVolume = Runtime->GetMasterVolume();
	Saved.bZoomToCursor = Runtime->IsZoomToCursorEnabled();
	Saved.bEdgeScroll = Runtime->IsEdgeScrollEnabled();
	Draft = Saved;
	bDirty = false;
	bDiscardArmed = false;

	CaptureGraphicsDraft();
}

void UFF_OptionsScreen::CommitDraft()
{
	UFF_RuntimeSettingsSubsystem* Runtime = UFF_RuntimeSettingsSubsystem::Get(this);
	if (!Runtime)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_OptionsScreen: 런타임 설정 서브시스템이 없어 사운드·조작을 저장하지 못했다."));
		CommitGraphicsDraft();
		RefreshDirtyState();
		return;
	}

	// 세 값을 한 번에 넘긴다. 행마다 setter 를 부르면 슬라이더를 끌 때마다 ini 를 쓴다.
	Runtime->CommitSettings(Draft.MasterVolume, Draft.bZoomToCursor, Draft.bEdgeScroll);

	// 그래픽은 엔진이 소유한다. 화면 모드가 바뀌면 여기서 15초 확인이 시작된다.
	CommitGraphicsDraft();

	Saved = Draft;
	bSavedNotice = true;
	bDiscardArmed = false;
	RefreshDirtyState();
}

void UFF_OptionsScreen::RefreshDirtyState()
{
	bDirty = !Draft.Equals(Saved) || !GraphicsDraft.Equals(SavedGraphics);
	if (bDirty)
	{
		bSavedNotice = false;
	}

	if (Button_Apply)
	{
		// 🔴 **끄지 않는다.** 비활성 버튼은 불투명도 0.35 로 흐려져 밝은 바닥 띠에서 거의 보이지
		// 않았다 (2026-09-10 실기 지적). 지금 저장할 것이 있는지는 아래 안내 문구가 말한다.
		// 바꾼 값이 없을 때 눌러도 같은 값을 다시 쓰는 것뿐이라 해로운 동작이 아니다.
		Button_Apply->SetIsEnabled(true);
	}

	if (!Text_ApplyHint)
	{
		return;
	}

	// 🔴 상태를 색으로만 알리지 않는다 (AT-011). 글로 적는다.
	FText Hint;
	if (bAwaitingVideoConfirm)
	{
		// 되돌림까지 남은 시간을 초로 적는다. 색이 아니라 글로 알린다 (AT-011).
		Hint = FText::Format(
			LOCTEXT("OptionsVideoConfirm",
				"화면 모드를 바꿨습니다 — {0}초 안에 `유지` 를 누르지 않으면 이전 설정으로 돌아갑니다"),
			FText::AsNumber(FMath::Max(PresentedConfirmSeconds, 0)));
	}
	else if (bDiscardArmed)
	{
		Hint = LOCTEXT("OptionsDiscardArmed", "저장하지 않은 변경이 있습니다 — `뒤로` 를 한 번 더 누르면 버립니다");
	}
	else if (bDirty)
	{
		Hint = LOCTEXT("OptionsDirty", "바꾼 값이 있습니다 — `적용` 을 누르면 저장합니다");
	}
	else if (bSavedNotice)
	{
		Hint = LOCTEXT("OptionsSaved", "저장했습니다");
	}

	Text_ApplyHint->SetText(Hint);
	Text_ApplyHint->SetVisibility(
		Hint.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UFF_OptionsScreen::HandleCategoryClicked(UFF_CommonButtonBase* Button)
{
	for (const EFF_OptionCategory Category : AllOptionCategories)
	{
		if (GetCategoryButton(Category) == Button)
		{
			// 분류를 옮기는 것은 값을 버리는 것이 아니다. draft 는 그대로 들고 간다.
			bDiscardArmed = false;
			SelectCategory(Category);
			return;
		}
	}
}

void UFF_OptionsScreen::HandleBackClicked(UFF_CommonButtonBase* Button)
{
	// 저장하지 않은 변경을 말없이 버리지 않는다. 한 번 물어보고 두 번째에 나간다.
	if (bDirty && !bDiscardArmed)
	{
		bDiscardArmed = true;
		RefreshDirtyState();
		return;
	}

	OnBackRequested.Broadcast();
}

void UFF_OptionsScreen::HandleApplyClicked(UFF_CommonButtonBase* Button)
{
	// 확인 중에는 같은 버튼이 `유지` 다.
	if (bAwaitingVideoConfirm)
	{
		ConfirmVideoMode();
		return;
	}

	CommitDraft();
}

void UFF_OptionsScreen::HandleReduceMotionToggled(const bool bEnabled)
{
	// 접근성만 즉시 반영이다. 값을 직접 들고 있지 않고 소유자에게 요청한다.
	if (UFF_UIAccessibilitySubsystem* Accessibility = UFF_UIAccessibilitySubsystem::Get(this))
	{
		Accessibility->SetReduceMotionEnabled(bEnabled);
	}
}

void UFF_OptionsScreen::HandleMasterVolumeChanged(const float Value)
{
	Draft.MasterVolume = Value;
	bDiscardArmed = false;

	if (ShouldCommitImmediately())
	{
		CommitDraft();
		return;
	}
	RefreshDirtyState();
}

void UFF_OptionsScreen::HandleZoomToCursorToggled(const bool bEnabled)
{
	Draft.bZoomToCursor = bEnabled;
	bDiscardArmed = false;

	if (ShouldCommitImmediately())
	{
		CommitDraft();
		return;
	}
	RefreshDirtyState();
}

void UFF_OptionsScreen::HandleEdgeScrollToggled(const bool bEnabled)
{
	Draft.bEdgeScroll = bEnabled;
	bDiscardArmed = false;

	if (ShouldCommitImmediately())
	{
		CommitDraft();
		return;
	}
	RefreshDirtyState();
}


// =============================================================================
// 그래픽 — 값은 엔진 `UGameUserSettings` 가 소유한다
// =============================================================================

bool UFF_OptionsScreen::FGraphicsDraft::Equals(const FGraphicsDraft& Other) const
{
	return WindowMode == Other.WindowMode
		&& Resolution == Other.Resolution
		&& ResolutionScalePercent == Other.ResolutionScalePercent
		&& FrameRateLimit == Other.FrameRateLimit
		&& QualityPreset == Other.QualityPreset
		&& bVSync == Other.bVSync;
}

bool UFF_OptionsScreen::FGraphicsDraft::DiffersInVideoMode(const FGraphicsDraft& Other) const
{
	return WindowMode != Other.WindowMode || Resolution != Other.Resolution;
}

void UFF_OptionsScreen::CaptureGraphicsDraft()
{
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!UserSettings)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_OptionsScreen: GameUserSettings 를 얻지 못했다. 그래픽 값을 읽지 않았다."));
		return;
	}

	SavedGraphics.WindowMode = UserSettings->GetFullscreenMode();
	SavedGraphics.Resolution = UserSettings->GetScreenResolution();
	SavedGraphics.ResolutionScalePercent =
		FMath::RoundToInt(UserSettings->GetResolutionScaleNormalized() * 100.f);
	SavedGraphics.FrameRateLimit = FMath::RoundToInt(UserSettings->GetFrameRateLimit());
	SavedGraphics.QualityPreset = UserSettings->GetOverallScalabilityLevel();
	SavedGraphics.bVSync = UserSettings->IsVSyncEnabled();

	GraphicsDraft = SavedGraphics;
	RebuildGraphicsOptionLists();
}

void UFF_OptionsScreen::RebuildGraphicsOptionLists()
{
	// 목록의 원천은 Project Settings > Game > FestaFiesta Festival Settings 다.
	// 한광훈님 ESC 옵션과 **같은 설정을 읽는다** — 두 화면이 다른 목록을 보이면 안 된다.
	const UFF_FestivalSettingsDeveloperSettings* Settings = UFF_FestivalSettingsDeveloperSettings::Get();

	ResolutionOptions.Reset();
	if (Settings)
	{
		for (const FIntPoint& Candidate : Settings->FallbackResolutions)
		{
			if (Candidate.X >= Settings->MinimumResolutionWidth &&
				Candidate.Y >= Settings->MinimumResolutionHeight)
			{
				ResolutionOptions.AddUnique(Candidate);
			}
		}
	}
	// 지금 값이 목록에 없으면 넣는다. 없으면 "선택할 수 없는 현재 상태" 가 되어 버린다.
	ResolutionOptions.AddUnique(SavedGraphics.Resolution);
	ResolutionOptions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X * A.Y < B.X * B.Y;
	});

	ResolutionScaleOptions.Reset();
	if (Settings)
	{
		ResolutionScaleOptions = Settings->ResolutionScalePercentOptions;
	}
	if (ResolutionScaleOptions.IsEmpty())
	{
		ResolutionScaleOptions = { 50, 75, 100 };
	}
	ResolutionScaleOptions.AddUnique(SavedGraphics.ResolutionScalePercent);
	ResolutionScaleOptions.Sort();

	FrameRateOptions.Reset();
	if (Settings)
	{
		FrameRateOptions = Settings->FrameRateLimitOptions;
	}
	if (FrameRateOptions.IsEmpty())
	{
		FrameRateOptions = { 0, 60, 120 };
	}
	FrameRateOptions.AddUnique(SavedGraphics.FrameRateLimit);
	FrameRateOptions.Sort();
}

TArray<FText> UFF_OptionsScreen::GetWindowModeOptions() const
{
	// 순서가 `EWindowMode` 의 값 순서(0 Fullscreen · 1 WindowedFullscreen · 2 Windowed)와 같아야 한다.
	return {
		LOCTEXT("OptionWindowModeFullscreen", "전체 화면"),
		LOCTEXT("OptionWindowModeBorderless", "전체 화면 창"),
		LOCTEXT("OptionWindowModeWindowed", "창")
	};
}

TArray<FText> UFF_OptionsScreen::GetQualityPresetOptions() const
{
	TArray<FText> Options = {
		LOCTEXT("OptionQualityLow", "낮음"),
		LOCTEXT("OptionQualityMedium", "중간"),
		LOCTEXT("OptionQualityHigh", "높음"),
		LOCTEXT("OptionQualityEpic", "매우 높음"),
		LOCTEXT("OptionQualityCinematic", "시네마틱")
	};

	// 개별 항목을 따로 만진 상태를 프리셋으로 속이지 않는다.
	if (GraphicsDraft.QualityPreset < 0)
	{
		Options.Add(LOCTEXT("OptionQualityCustom", "사용자 지정"));
	}
	return Options;
}

void UFF_OptionsScreen::CommitGraphicsDraft()
{
	UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!UserSettings)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_OptionsScreen: GameUserSettings 가 없어 그래픽을 반영하지 못했다."));
		return;
	}

	const bool bVideoModeChanged = GraphicsDraft.DiffersInVideoMode(SavedGraphics);

	// 화면 모드가 아닌 것들은 되돌릴 필요가 없다. 먼저 확정한다.
	UserSettings->SetResolutionScaleValueEx(static_cast<float>(GraphicsDraft.ResolutionScalePercent));
	UserSettings->SetFrameRateLimit(static_cast<float>(GraphicsDraft.FrameRateLimit));
	UserSettings->SetVSyncEnabled(GraphicsDraft.bVSync);
	if (GraphicsDraft.QualityPreset >= 0 && GraphicsDraft.QualityPreset != SavedGraphics.QualityPreset)
	{
		// 프리셋은 개별 항목을 전부 덮어쓴다. 그래서 값이 실제로 바뀔 때만 부른다.
		UserSettings->SetOverallScalabilityLevel(GraphicsDraft.QualityPreset);
	}
	UserSettings->ApplyNonResolutionSettings();

	if (!bVideoModeChanged)
	{
		UserSettings->SaveSettings();
		SavedGraphics = GraphicsDraft;
		return;
	}

	UserSettings->SetFullscreenMode(GraphicsDraft.WindowMode);
	UserSettings->SetScreenResolution(GraphicsDraft.Resolution);
	// false — 커맨드라인 재정의를 다시 읽으면 방금 고른 값이 무시될 수 있다.
	UserSettings->ApplyResolutionSettings(false);

	BeginVideoModeConfirmation();
}

void UFF_OptionsScreen::BeginVideoModeConfirmation()
{
	bAwaitingVideoConfirm = true;
	VideoConfirmSecondsRemaining = VideoModeConfirmationSeconds;
	PresentedConfirmSeconds = -1;

	// 같은 버튼이 `유지` 가 된다. 확인창을 한 겹 얹지 않는 이유는 화면이 이미 깨져 보일 수 있어서
	// 그 위에 새 화면을 띄우면 더 알기 어렵기 때문이다.
	if (Button_Apply)
	{
		Button_Apply->SetButtonLabel(LOCTEXT("OptionsKeepVideoMode", "유지"));
		Button_Apply->SetIsEnabled(true);
	}

	RefreshDirtyState();
}

void UFF_OptionsScreen::ConfirmVideoMode()
{
	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		UserSettings->ConfirmVideoMode();
		UserSettings->SaveSettings();
	}

	SavedGraphics = GraphicsDraft;
	bAwaitingVideoConfirm = false;
	VideoConfirmSecondsRemaining = 0.f;
	PresentedConfirmSeconds = -1;
	bSavedNotice = true;

	if (Button_Apply)
	{
		Button_Apply->SetButtonLabel(LOCTEXT("OptionsApply", "적용"));
	}

	RefreshDirtyState();
}

void UFF_OptionsScreen::RollbackVideoMode()
{
	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		// 엔진이 들고 있는 "마지막으로 확정된 모드" 로 돌아간다. 그것이 우리가 읽어 둔 값과
		// 같은지 믿지 않고, 우리 사본으로 한 번 더 못을 박는다.
		UserSettings->RevertVideoMode();
		UserSettings->SetFullscreenMode(SavedGraphics.WindowMode);
		UserSettings->SetScreenResolution(SavedGraphics.Resolution);
		UserSettings->ApplyResolutionSettings(false);
		UserSettings->ConfirmVideoMode();
		UserSettings->SaveSettings();
	}

	GraphicsDraft = SavedGraphics;
	bAwaitingVideoConfirm = false;
	VideoConfirmSecondsRemaining = 0.f;
	PresentedConfirmSeconds = -1;

	if (Button_Apply)
	{
		Button_Apply->SetButtonLabel(LOCTEXT("OptionsApply", "적용"));
	}

	// 되돌린 값이 행에도 보여야 한다.
	if (SelectedCategory == EFF_OptionCategory::Graphics)
	{
		RebuildRows();
	}
	RefreshDirtyState();
}

void UFF_OptionsScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bAwaitingVideoConfirm)
	{
		return;
	}

	VideoConfirmSecondsRemaining -= InDeltaTime;
	if (VideoConfirmSecondsRemaining <= 0.f)
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("UFF_OptionsScreen: 화면 모드 확인 시간이 지나 이전 설정으로 되돌렸다."));
		RollbackVideoMode();
		return;
	}

	// 남은 초가 바뀔 때만 문구를 다시 만든다.
	const int32 Seconds = FMath::CeilToInt(VideoConfirmSecondsRemaining);
	if (Seconds != PresentedConfirmSeconds)
	{
		PresentedConfirmSeconds = Seconds;
		RefreshDirtyState();
	}
}

void UFF_OptionsScreen::HandleGraphicsSelectionChanged(const FName SettingId, const int32 SelectedIndex)
{
	bDiscardArmed = false;

	if (SettingId == Setting_WindowMode)
	{
		GraphicsDraft.WindowMode = static_cast<EWindowMode::Type>(
			FMath::Clamp(SelectedIndex, 0, static_cast<int32>(EWindowMode::Windowed)));
	}
	else if (SettingId == Setting_Resolution)
	{
		if (ResolutionOptions.IsValidIndex(SelectedIndex))
		{
			GraphicsDraft.Resolution = ResolutionOptions[SelectedIndex];
		}
	}
	else if (SettingId == Setting_ResolutionScale)
	{
		if (ResolutionScaleOptions.IsValidIndex(SelectedIndex))
		{
			GraphicsDraft.ResolutionScalePercent = ResolutionScaleOptions[SelectedIndex];
		}
	}
	else if (SettingId == Setting_FrameRateLimit)
	{
		if (FrameRateOptions.IsValidIndex(SelectedIndex))
		{
			GraphicsDraft.FrameRateLimit = FrameRateOptions[SelectedIndex];
		}
	}
	else if (SettingId == Setting_QualityPreset)
	{
		// `사용자 지정` 은 목록 끝에만 있고 고를 수 있는 값이 아니다.
		GraphicsDraft.QualityPreset = SelectedIndex <= 4 ? SelectedIndex : -1;
	}

	RefreshDirtyState();
}

void UFF_OptionsScreen::HandleVSyncToggled(const bool bEnabled)
{
	GraphicsDraft.bVSync = bEnabled;
	bDiscardArmed = false;
	RefreshDirtyState();
}

TSubclassOf<UFF_SettingSelectRow> UFF_OptionsScreen::ResolveSelectRowClass() const
{
	if (SelectRowClass)
	{
		return SelectRowClass;
	}
	return LoadClass<UFF_SettingSelectRow>(nullptr, SelectRowFallbackPath);
}

UFF_SettingSelectRow* UFF_OptionsScreen::AddSelectRow(
	const FName SettingId, const FText& Label, const FText& Description,
	const TArray<FText>& Options, const int32 InitialIndex)
{
	const TSubclassOf<UFF_SettingSelectRow> RowClass = ResolveSelectRowClass();
	if (!RowClass || !VerticalBox_Rows)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_OptionsScreen: 선택 행 클래스를 찾지 못해 `%s` 를 그리지 못했다."), *Label.ToString());
		return nullptr;
	}

	UFF_SettingSelectRow* Row = CreateWidget<UFF_SettingSelectRow>(this, RowClass);
	if (!Row)
	{
		return nullptr;
	}

	Row->Configure(SettingId, Label, Description, Options, InitialIndex);
	ApplyCasualRowStyle(Row);
	if (UVerticalBoxSlot* Layout = VerticalBox_Rows->AddChildToVerticalBox(Row))
	{
		Layout->SetPadding(FMargin(0.f, 0.f, 0.f, RowGap));
	}
	return Row;
}

#undef LOCTEXT_NAMESPACE
