// FF_SettingsWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

// 메인 축제 설정 화면의 WBP 스캐폴더. `FF.SettingsUMG`는 기존 디자인을 보존하고,
// 명시적인 `FF.SettingsUMG.Refresh`만 이 빌더가 소유한 PauseOptions 4종을 다시 만든다.

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UI/Core/FF_PauseOptionsLightTealStyle.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/Settings/FF_FestivalSettingsScreen.h"
#include "UI/Settings/FF_SettingSelectRow.h"
#include "UI/Settings/FF_SettingSliderRow.h"
#include "UI/Settings/FF_SettingToggleRow.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaSettingsWbpBuilder"

namespace FFSettingsWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* const SettingsLogPrefix = TEXT("FF_SETTINGS_UMG");
	const FString SettingsFolder(TEXT("/Game/FestaFiesta/UI/Settings"));
	const FString ToggleRowPackage(SettingsFolder / TEXT("WBP_FF_SettingToggleRow"));
	const FString SliderRowPackage(SettingsFolder / TEXT("WBP_FF_SettingSliderRow"));
	const FString SelectRowPackage(SettingsFolder / TEXT("WBP_FF_SettingSelectRow"));
	// 기존 WBP_FF_FestivalSettings는 디자이너 산출물로 보존한다. 새 계약은 별도 에셋에만 생성한다.
	const FString ScreenPackage(SettingsFolder / TEXT("WBP_FF_PauseOptions"));

	// Pause Hub와 같은 1920x1080 프레임을 쓰는 설정 화면의 단일 레이아웃 원천.
	// 디자이너가 밀도나 비율을 바꿀 때 흩어진 매직 넘버를 찾지 않도록 여기만 조정한다.
	struct FSettingsLayoutMetrics
	{
		static constexpr float DesignWidth = 1920.f;
		static constexpr float DesignHeight = 1080.f;
		static constexpr float SafeX = 72.f;
		static constexpr float HeaderY = 40.f;
		static constexpr float HeaderWidth = 1776.f;
		static constexpr float HeaderHeight = 72.f;
		static constexpr float CategoryY = 128.f;
		static constexpr float CategoryBarHeight = 76.f;
		static constexpr float BodyY = 220.f;
		static constexpr float BodyHeight = 692.f;
		static constexpr float NavigationWidth = 300.f; // Legacy WBP fallback only.
		static constexpr float MainX = 72.f;
		static constexpr float MainWidth = HeaderWidth;
		static constexpr float PanelInset = 28.f;
		static constexpr float MainContentWidth = MainWidth - (PanelInset * 2.f);
		static constexpr float FooterY = 932.f;
		static constexpr float FooterHeight = 88.f;
		static constexpr float RowHeight = 92.f;
		static constexpr float CategoryHeight = 58.f;
		static constexpr float RadiusPanel = 8.f;
		static constexpr float RadiusControl = 6.f;
	};

	FSlateBrush MakeSolidBrush(const FLinearColor& Color, const float CornerRadius = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);
		return Brush;
	}

	FSlateBrush MakePanelBrush(const FLinearColor& Fill, const FLinearColor& Outline,
		const float CornerRadius = 4.f, const float OutlineWidth = 1.f)
	{
		FSlateBrush Brush = MakeSolidBrush(Fill, CornerRadius);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = OutlineWidth;
		return Brush;
	}

	FButtonStyle MakeButtonStyle(const UFF_UIPalette& Palette, bool bPrimary = false)
	{
		const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(Palette);
		FButtonStyle Style;
		Style.Normal = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			bPrimary ? FFPauseOptionsLightTeal::RectPrimaryPath
				: FFPauseOptionsLightTeal::RectSecondaryPath,
			bPrimary ? Tokens.Teal : Tokens.PaperSunken,
			bPrimary ? Tokens.TealDeep : Tokens.Line);
		Style.Hovered = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			bPrimary ? FFPauseOptionsLightTeal::RectPrimaryHoverPath
				: FFPauseOptionsLightTeal::RectNavHoverPath,
			bPrimary ? Tokens.TealDeep : Tokens.Hover, Tokens.TealDeep);
		Style.Pressed = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			FFPauseOptionsLightTeal::RectNavPressedPath, Tokens.TealDeep, Tokens.TealDeep);
		Style.Disabled = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			FFPauseOptionsLightTeal::RectNavDisabledPath, Tokens.Disabled, Tokens.Line);
		Style.NormalPadding = FMargin(16.f, 10.f);
		Style.PressedPadding = FMargin(16.f, 11.f, 16.f, 9.f);
		return Style;
	}

	FButtonStyle MakeCategoryButtonStyle(const UFF_UIPalette& Palette)
	{
		FButtonStyle Style = MakeButtonStyle(Palette);
		Style.NormalPadding = FMargin(18.f, 10.f);
		Style.PressedPadding = FMargin(18.f, 11.f, 18.f, 9.f);
		return Style;
	}

	UImage* MakeFocusRing(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const FLinearColor& Color)
	{
		UImage* Ring = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_FocusRing"), true);
		if (Ring)
		{
			Ring->SetBrush(MakeSolidBrush(FLinearColor::Transparent, 6.f));
			FSlateBrush Brush = Ring->GetBrush();
			Brush.OutlineSettings.Color = FSlateColor(Color);
			Brush.OutlineSettings.Width = 2.f;
			Ring->SetBrush(Brush);
			Ring->SetVisibility(ESlateVisibility::Hidden);
		}
		return Ring;
	}

	bool BuildToggleRow(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"));
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UBorder* Background = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Background"));
		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Content"));
		UVerticalBox* Copy = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Copy"));
		UTextBlock* Label = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		UTextBlock* Description = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Description"), true);
		UCheckBox* CheckBox = MakeWidget<UCheckBox>(Blueprint, Tree, TEXT("CheckBox_Value"), true);
		UImage* FocusRing = MakeFocusRing(Blueprint, Tree, Palette->InGame.TealDeep);

		if (!Root || !Overlay || !Background || !Content || !Copy || !Label || !Description || !CheckBox || !FocusRing)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetHeightOverride(FSettingsLayoutMetrics::RowHeight);
		Root->AddChild(Overlay);
		Background->SetBrush(FFPauseOptionsLightTeal::MakeListRowBrush(
			FFPauseOptionsLightTeal::Get(*Palette)));
		Background->SetPadding(FMargin(22.f, 12.f));
		Background->AddChild(Content);
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(Background))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		StyleText(*Label, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		Label->SetText(LOCTEXT("TogglePreviewLabel", "설정 이름"));
		StyleText(*Description, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		Description->SetText(LOCTEXT("TogglePreviewDescription", "설정에 대한 설명"));
		Copy->AddChildToVerticalBox(Label);
		Copy->AddChildToVerticalBox(Description);
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(Copy))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(CheckBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(20.f, 0.f, 8.f, 0.f));
		}
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(FocusRing))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		return true;
	}

	bool BuildSliderRow(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"));
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UBorder* Background = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Background"));
		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Content"));
		UVerticalBox* Copy = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Copy"));
		UTextBlock* Label = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		UTextBlock* Description = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Description"), true);
		USizeBox* SliderBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Slider"));
		USlider* Slider = MakeWidget<USlider>(Blueprint, Tree, TEXT("Slider_Value"), true);
		UTextBlock* Value = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Value"), true);
		UImage* FocusRing = MakeFocusRing(Blueprint, Tree, Palette->InGame.TealDeep);

		if (!Root || !Overlay || !Background || !Content || !Copy || !Label || !Description
			|| !SliderBox || !Slider || !Value || !FocusRing)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetHeightOverride(FSettingsLayoutMetrics::RowHeight);
		Root->AddChild(Overlay);
		Background->SetBrush(FFPauseOptionsLightTeal::MakeListRowBrush(
			FFPauseOptionsLightTeal::Get(*Palette)));
		Background->SetPadding(FMargin(22.f, 12.f));
		Background->AddChild(Content);
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(Background))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		StyleText(*Label, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		Label->SetText(LOCTEXT("SliderPreviewLabel", "설정 이름"));
		StyleText(*Description, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		Description->SetText(LOCTEXT("SliderPreviewDescription", "설정에 대한 설명"));
		Copy->AddChildToVerticalBox(Label);
		Copy->AddChildToVerticalBox(Description);
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(Copy))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		Slider->SetSliderBarColor(Palette->InGame.TealSoft);
		Slider->SetSliderHandleColor(Palette->InGame.TealDeep);
		SliderBox->SetWidthOverride(300.f);
		SliderBox->SetHeightOverride(36.f);
		SliderBox->AddChild(Slider);
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(SliderBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(20.f, 0.f, 16.f, 0.f));
		}

		StyleText(*Value, 17, Palette->InGame.Ink, TEXT("SemiBold"));
		Value->SetText(LOCTEXT("SliderPreviewValue", "100%"));
		Value->SetJustification(ETextJustify::Right);
		Value->SetMinDesiredWidth(72.f);
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(Value))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(FocusRing))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		return true;
	}

	UButton* MakeTextButton(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const FName ButtonName,
		const FName TextName, const FText& Label, const UFF_UIPalette& Palette, bool bPrimary = false)
	{
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, true);
		UTextBlock* Text = MakeWidget<UTextBlock>(Blueprint, Tree, TextName, true);
		if (!Button || !Text)
		{
			return nullptr;
		}
		Button->SetStyle(MakeButtonStyle(Palette, bPrimary));
		StyleText(*Text, 18, bPrimary ? Palette.InGame.White : Palette.InGame.Ink, TEXT("SemiBold"));
		Text->SetText(Label);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->AddChild(Text)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Button;
	}

	bool BuildSelectRow(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Root"));
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_Root"));
		UBorder* Background = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Background"));
		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Content"));
		UVerticalBox* Copy = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Copy"));
		UTextBlock* Label = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Label"), true);
		UTextBlock* Description = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Description"), true);
		UHorizontalBox* Selector = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Selector"));
		UButton* Previous = MakeTextButton(Blueprint, Tree, TEXT("Button_Previous"), TEXT("Text_Previous"),
			LOCTEXT("PreviousGlyph", "‹"), *Palette);
		UTextBlock* Value = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Value"), true);
		UButton* Next = MakeTextButton(Blueprint, Tree, TEXT("Button_Next"), TEXT("Text_Next"),
			LOCTEXT("NextGlyph", "›"), *Palette);
		UImage* FocusRing = MakeFocusRing(Blueprint, Tree, Palette->InGame.TealDeep);

		if (!Root || !Overlay || !Background || !Content || !Copy || !Label || !Description ||
			!Selector || !Previous || !Value || !Next || !FocusRing)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetHeightOverride(FSettingsLayoutMetrics::RowHeight);
		Root->AddChild(Overlay);
		Background->SetBrush(FFPauseOptionsLightTeal::MakeListRowBrush(
			FFPauseOptionsLightTeal::Get(*Palette)));
		Background->SetPadding(FMargin(22.f, 12.f));
		Background->AddChild(Content);
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(Background))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		StyleText(*Label, 18, Palette->InGame.Ink, TEXT("SemiBold"));
		Label->SetText(LOCTEXT("SelectPreviewLabel", "설정 이름"));
		StyleText(*Description, 13, Palette->InGame.InkMuted, TEXT("Medium"));
		Description->SetText(LOCTEXT("SelectPreviewDescription", "선택형 설정에 대한 설명"));
		Copy->AddChildToVerticalBox(Label);
		Copy->AddChildToVerticalBox(Description);
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(Copy))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		StyleText(*Value, 17, Palette->InGame.Ink, TEXT("SemiBold"));
		Value->SetText(LOCTEXT("SelectPreviewValue", "1920 × 1080"));
		Value->SetJustification(ETextJustify::Center);
		Value->SetMinDesiredWidth(220.f);
		if (UHorizontalBoxSlot* Slot = Selector->AddChildToHorizontalBox(Previous))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* Slot = Selector->AddChildToHorizontalBox(Value))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(14.f, 0.f));
		}
		if (UHorizontalBoxSlot* Slot = Selector->AddChildToHorizontalBox(Next))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(Selector))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(20.f, 0.f, 8.f, 0.f));
		}
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(FocusRing))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		return true;
	}

	[[maybe_unused]] bool BuildLegacyScreen(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UBorder* Scrim = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Scrim"));
		USafeZone* SafeZone = MakeWidget<USafeZone>(Blueprint, Tree, TEXT("SafeZone_Viewport"));
		UScaleBox* ViewportScale = MakeWidget<UScaleBox>(Blueprint, Tree, TEXT("ScaleBox_Viewport"));
		USizeBox* DesignFrame = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Design1920x1080"));
		UCanvasPanel* DesignCanvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Design"));
		UBorder* Header = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Header"));
		UHorizontalBox* HeaderContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* HeaderBrand = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HeaderBrand"));
		UTextBlock* HeaderTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HeaderTitle"));
		UTextBlock* KeyHint = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_KeyHint"));
		UButton* HeaderClose = MakeTextButton(Blueprint, Tree, TEXT("Button_Close"), TEXT("Text_Close"),
			LOCTEXT("CloseGlyph", "×"), *Palette);
		UBorder* Panel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_SettingsPanel"));
		UCanvasPanel* PanelCanvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_SettingsPanel"));
		UTextBlock* Intro = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Intro"), true);
		UVerticalBox* Categories = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Categories"), true);
		UBorder* Divider = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Divider"));
		UTextBlock* CategoryTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CategoryTitle"), true);
		UScrollBox* RowsScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Rows"));
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Rows"), true);
		UBorder* SummaryPanel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_CurrentSummary"));
		UTextBlock* Summary = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CurrentSummary"), true);
		UButton* Back = MakeTextButton(Blueprint, Tree, TEXT("Button_Back"), TEXT("Text_Back"),
			LOCTEXT("Back", "이전으로"), *Palette);
		UButton* Save = MakeTextButton(Blueprint, Tree, TEXT("Button_Save"), TEXT("Text_Save"),
			LOCTEXT("Save", "저장"), *Palette, true);

		if (!Root || !Scrim || !SafeZone || !ViewportScale || !DesignFrame || !DesignCanvas
			|| !Header || !HeaderContent || !HeaderBrand || !HeaderTitle || !KeyHint
			|| !HeaderClose || !Panel || !PanelCanvas || !Intro || !Categories || !Divider || !CategoryTitle
			|| !RowsScroll || !Rows || !SummaryPanel || !Summary || !Back || !Save)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Scrim->SetBrush(MakeSolidBrush(Palette->InGame.Scrim));
		StretchOnCanvas(*Root, *Scrim);

		// 모든 좌표는 1920x1080 한 곳에서 관리한다. 실제 뷰포트에서는 안전 영역 안에
		// 종횡비를 유지해 축소/확대하므로 패키징 후 해상도가 달라도 버튼이 잘리지 않는다.
		ViewportScale->SetStretch(EStretch::ScaleToFit);
		ViewportScale->SetStretchDirection(EStretchDirection::Both);
		DesignFrame->SetWidthOverride(1920.f);
		DesignFrame->SetHeightOverride(1080.f);
		DesignFrame->AddChild(DesignCanvas);
		ViewportScale->AddChild(DesignFrame);
		SafeZone->AddChild(ViewportScale);
		StretchOnCanvas(*Root, *SafeZone);

		Header->SetBrush(MakeSolidBrush(Palette->InGame.TealDeep));
		Header->SetPadding(FMargin(38.f, 0.f));
		Header->AddChild(HeaderContent);
		PlaceOnCanvas(*DesignCanvas, *Header, 0.f, 0.f, 1920.f, 86.f);

		StyleText(*HeaderBrand, 28, Palette->InGame.White, TEXT("SemiBold"));
		HeaderBrand->SetText(LOCTEXT("Brand", "FESTA FIESTA!"));
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(HeaderBrand))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 34.f, 0.f));
		}
		StyleText(*HeaderTitle, 24, Palette->InGame.White, TEXT("SemiBold"));
		HeaderTitle->SetText(LOCTEXT("OptionsTitle", "옵션"));
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(HeaderTitle))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		StyleText(*KeyHint, 16, Palette->InGame.TealSoft, TEXT("Medium"));
		KeyHint->SetText(LOCTEXT("KeyHint", "ESC · F5  이전으로"));
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(KeyHint))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(HeaderClose))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		Panel->SetBrush(MakeSolidBrush(Palette->InGame.Paper, 10.f));
		Panel->SetPadding(FMargin(0.f));
		Panel->AddChild(PanelCanvas);
		PlaceOnCanvas(*DesignCanvas, *Panel, 120.f, 112.f, 1680.f, 852.f);

		StyleText(*Intro, 16, Palette->InGame.InkMuted, TEXT("Medium"));
		Intro->SetText(LOCTEXT("ManualSave",
			"변경 사항은 저장 버튼을 눌러야 적용됩니다. 저장하지 않고 이전으로 가면 변경 내용이 취소됩니다."));
		PlaceOnCanvas(*PanelCanvas, *Intro, 44.f, 28.f, 1540.f, 34.f);

		struct FCategoryDefinition
		{
			FName ButtonName;
			FName TextName;
			FText Label;
		};
		const FCategoryDefinition CategoryDefinitions[] = {
			{ TEXT("Button_Basic"), TEXT("Text_BasicCategory"), LOCTEXT("Basic", "기본설정") },
			{ TEXT("Button_Account"), TEXT("Text_AccountCategory"), LOCTEXT("Account", "계정정보") },
			{ TEXT("Button_Keyboard"), TEXT("Text_KeyboardCategory"), LOCTEXT("Keyboard", "키보드 키 배치") },
			{ TEXT("Button_Graphics"), TEXT("Text_GraphicsCategory"), LOCTEXT("Graphics", "그래픽") },
			{ TEXT("Button_ExitGame"), TEXT("Text_ExitGameCategory"), LOCTEXT("ExitGame", "게임종료") }
		};
		for (const FCategoryDefinition& Definition : CategoryDefinitions)
		{
			UButton* Button = MakeTextButton(Blueprint, Tree, Definition.ButtonName, Definition.TextName,
				Definition.Label, *Palette);
			USizeBox* ButtonBox = MakeWidget<USizeBox>(Blueprint, Tree,
				FName(*(Definition.ButtonName.ToString() + TEXT("_Size"))));
			if (!Button || !ButtonBox)
			{
				return false;
			}
			Button->SetStyle(MakeCategoryButtonStyle(*Palette));
			ButtonBox->SetHeightOverride(82.f);
			ButtonBox->AddChild(Button);
			if (Definition.ButtonName == FName(TEXT("Button_ExitGame")))
			{
				if (UTextBlock* ExitLabel = Cast<UTextBlock>(Tree.FindWidget(Definition.TextName)))
				{
					// 의미색만으로 구분하지 않고 "게임종료"라는 명시적 문구와 함께 쓴다.
					ExitLabel->SetColorAndOpacity(Palette->InGame.Danger);
				}
			}
			if (UVerticalBoxSlot* Slot = Categories->AddChildToVerticalBox(ButtonBox))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
			}
		}
		PlaceOnCanvas(*PanelCanvas, *Categories, 44.f, 92.f, 300.f, 510.f);

		Divider->SetBrush(MakeSolidBrush(Palette->InGame.Line));
		PlaceOnCanvas(*PanelCanvas, *Divider, 380.f, 88.f, 2.f, 620.f);

		StyleText(*CategoryTitle, 30, Palette->InGame.Ink, TEXT("SemiBold"));
		CategoryTitle->SetText(LOCTEXT("BasicPreview", "기본설정"));
		PlaceOnCanvas(*PanelCanvas, *CategoryTitle, 430.f, 88.f, 1110.f, 52.f);
		RowsScroll->SetAnimateWheelScrolling(true);
		RowsScroll->SetWheelScrollMultiplier(64.f);
		RowsScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		RowsScroll->AddChild(Rows);
		PlaceOnCanvas(*PanelCanvas, *RowsScroll, 430.f, 156.f, 1110.f, 448.f);

		SummaryPanel->SetBrush(MakeSolidBrush(Palette->InGame.StatusBar, 6.f));
		SummaryPanel->SetPadding(FMargin(18.f, 10.f));
		SummaryPanel->AddChild(Summary);
		StyleText(*Summary, 15, Palette->InGame.TealDeep, TEXT("Medium"));
		Summary->SetText(LOCTEXT("SummaryPreview", "저장된 설정  ·  1920 × 1080  ·  전체 화면  ·  음량 80%"));
		PlaceOnCanvas(*PanelCanvas, *SummaryPanel, 430.f, 628.f, 1110.f, 58.f);

		PlaceOnCanvas(*PanelCanvas, *Back, 1012.f, 744.f, 240.f, 70.f);
		PlaceOnCanvas(*PanelCanvas, *Save, 1300.f, 744.f, 240.f, 70.f);
		return true;
	}

	/**
	 * Pause Hub와 한 세트로 보이는 전체 화면 설정 레이아웃.
	 *
	 * 레퍼런스의 상단 대분류와 넓은 설정 표, 고정 하단 액션을 LightTeal로 번역한다.
	 * 런타임 코드는 기존 BindWidget 이름만 보므로
	 * 기능 로직과 레이아웃을 독립적으로 유지할 수 있다.
	 */
	bool BuildScreenV2(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}
		const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(*Palette);

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UBackgroundBlur* Blur = MakeWidget<UBackgroundBlur>(Blueprint, Tree, TEXT("BackgroundBlur_World"));
		UBorder* Scrim = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Scrim"));
		USafeZone* SafeZone = MakeWidget<USafeZone>(Blueprint, Tree, TEXT("SafeZone_Viewport"));
		UScaleBox* ViewportScale = MakeWidget<UScaleBox>(Blueprint, Tree, TEXT("ScaleBox_Viewport"));
		USizeBox* DesignFrame = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Design1920x1080"));
		UCanvasPanel* Design = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Design"));

		UBorder* Header = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Header"));
		UHorizontalBox* HeaderContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* HeaderBrand = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HeaderBrand"));
		UTextBlock* HeaderTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HeaderTitle"));
		UTextBlock* KeyHint = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_KeyHint"), true);
		UButton* HeaderClose = MakeTextButton(Blueprint, Tree, TEXT("Button_Close"), TEXT("Text_Close"),
			LOCTEXT("CloseGlyphV2", "×"), *Palette);

		UBorder* CategoryBar = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_CategoryBar"));
		UHorizontalBox* CategoryBarContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_CategoryBar"));
		UHorizontalBox* Categories = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Categories"), true);

		UBorder* Panel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_SettingsPanel"));
		UCanvasPanel* PanelCanvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_SettingsPanel"));
		UTextBlock* CategoryTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CategoryTitle"), true);
		UTextBlock* Intro = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Intro"), true);
		UBorder* ContentRule = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ContentRule"));
		UScrollBox* RowsScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Rows"));
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Rows"), true);

		UBorder* Footer = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Footer"));
		UHorizontalBox* FooterContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Footer"));
		UTextBlock* Summary = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CurrentSummary"), true);
		UTextBlock* FooterStatus = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_FooterStatus"));
		UTextBlock* FooterShortcut = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_FooterShortcut"));
		USizeBox* BackBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_BackAction"));
		UButton* Back = MakeTextButton(Blueprint, Tree, TEXT("Button_Back"), TEXT("Text_Back"),
			LOCTEXT("BackV2", "이전으로"), *Palette);
		USizeBox* SaveBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_SaveAction"));
		UButton* Save = MakeTextButton(Blueprint, Tree, TEXT("Button_Save"), TEXT("Text_Save"),
			LOCTEXT("SaveV2", "저장"), *Palette, true);

		if (!Root || !Blur || !Scrim || !SafeZone || !ViewportScale || !DesignFrame || !Design
			|| !Header || !HeaderContent || !HeaderBrand || !HeaderTitle || !KeyHint || !HeaderClose
			|| !CategoryBar || !CategoryBarContent || !Categories || !Panel || !PanelCanvas
			|| !CategoryTitle || !Intro || !ContentRule || !RowsScroll || !Rows
			|| !Footer || !FooterContent || !Summary || !FooterStatus
			|| !FooterShortcut || !BackBox || !Back || !SaveBox || !Save)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Blur->SetBlurStrength(18.f);
		Blur->SetApplyAlphaToBlur(false);
		Blur->SetLowQualityFallbackBrush(MakeSolidBrush(Tokens.Scrim));
		Blur->SetVisibility(ESlateVisibility::HitTestInvisible);
		StretchOnCanvas(*Root, *Blur);
		Scrim->SetBrush(MakeSolidBrush(Tokens.Scrim));
		StretchOnCanvas(*Root, *Scrim);

		ViewportScale->SetStretch(EStretch::ScaleToFit);
		ViewportScale->SetStretchDirection(EStretchDirection::Both);
		DesignFrame->SetWidthOverride(FSettingsLayoutMetrics::DesignWidth);
		DesignFrame->SetHeightOverride(FSettingsLayoutMetrics::DesignHeight);
		DesignFrame->AddChild(Design);
		ViewportScale->AddChild(DesignFrame);
		SafeZone->AddChild(ViewportScale);
		StretchOnCanvas(*Root, *SafeZone);

		Header->SetBrush(FFPauseOptionsLightTeal::MakeHeaderBrush(Tokens));
		Header->SetPadding(FMargin(30.f, 0.f));
		Header->AddChild(HeaderContent);
		PlaceOnCanvas(*Design, *Header, FSettingsLayoutMetrics::SafeX, FSettingsLayoutMetrics::HeaderY,
			FSettingsLayoutMetrics::HeaderWidth, FSettingsLayoutMetrics::HeaderHeight);
		StyleText(*HeaderBrand, 29, Tokens.White, TEXT("SemiBold"));
		HeaderBrand->SetText(LOCTEXT("BrandV2", "FESTA FIESTA!"));
		StyleText(*HeaderTitle, 21, Tokens.White, TEXT("SemiBold"));
		HeaderTitle->SetText(LOCTEXT("OptionsTitleV2", "옵션"));
		StyleText(*KeyHint, 15, Tokens.TealSoft, TEXT("Medium"));
		KeyHint->SetText(LOCTEXT("KeyHintV2", "ESC · F5  이전으로"));
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(HeaderBrand))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 28.f, 0.f));
		}
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(HeaderTitle))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(KeyHint))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(HeaderClose))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		CategoryBar->SetBrush(FFPauseOptionsLightTeal::MakeMenuRailBrush(Tokens));
		CategoryBar->SetPadding(FMargin(18.f, 9.f));
		CategoryBar->AddChild(CategoryBarContent);
		PlaceOnCanvas(*Design, *CategoryBar, FSettingsLayoutMetrics::SafeX, FSettingsLayoutMetrics::CategoryY,
			FSettingsLayoutMetrics::HeaderWidth, FSettingsLayoutMetrics::CategoryBarHeight);
		struct FCategoryDefinition
		{
			FName ButtonName;
			FName TextName;
			FText Label;
		};
		const FCategoryDefinition CategoryDefinitions[] = {
			{ TEXT("Button_Basic"), TEXT("Text_BasicCategory"), LOCTEXT("BasicV2", "기본설정") },
			{ TEXT("Button_Account"), TEXT("Text_AccountCategory"), LOCTEXT("AccountV2", "계정정보") },
			{ TEXT("Button_Keyboard"), TEXT("Text_KeyboardCategory"), LOCTEXT("KeyboardV2", "키보드 키 배치") },
			{ TEXT("Button_Graphics"), TEXT("Text_GraphicsCategory"), LOCTEXT("GraphicsV2", "그래픽") },
			{ TEXT("Button_ExitGame"), TEXT("Text_ExitGameCategory"), LOCTEXT("ExitGameV2", "게임종료") }
		};
		for (const FCategoryDefinition& Definition : CategoryDefinitions)
		{
			UButton* Button = MakeTextButton(Blueprint, Tree, Definition.ButtonName, Definition.TextName,
				Definition.Label, *Palette);
			USizeBox* ButtonBox = MakeWidget<USizeBox>(Blueprint, Tree,
				FName(*(Definition.ButtonName.ToString() + TEXT("_Size"))));
			if (!Button || !ButtonBox)
			{
				return false;
			}
			Button->SetStyle(MakeCategoryButtonStyle(*Palette));
			ButtonBox->SetHeightOverride(FSettingsLayoutMetrics::CategoryHeight);
			ButtonBox->AddChild(Button);
			if (Definition.ButtonName == FName(TEXT("Button_ExitGame")))
			{
				if (UTextBlock* ExitLabel = Cast<UTextBlock>(Tree.FindWidget(Definition.TextName)))
				{
					ExitLabel->SetColorAndOpacity(Palette->InGame.Danger);
				}
			}
			if (UHorizontalBoxSlot* Slot = Categories->AddChildToHorizontalBox(ButtonBox))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				Slot->SetVerticalAlignment(VAlign_Center);
				Slot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			}
		}
		if (UHorizontalBoxSlot* Slot = CategoryBarContent->AddChildToHorizontalBox(Categories))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		Panel->SetBrush(FFPauseOptionsLightTeal::MakeBodyBrush(Tokens));
		Panel->AddChild(PanelCanvas);
		PlaceOnCanvas(*Design, *Panel, FSettingsLayoutMetrics::MainX, FSettingsLayoutMetrics::BodyY,
			FSettingsLayoutMetrics::MainWidth, FSettingsLayoutMetrics::BodyHeight);
		StyleText(*CategoryTitle, 30, Tokens.Ink, TEXT("SemiBold"));
		CategoryTitle->SetText(LOCTEXT("BasicPreviewV2", "기본설정"));
		PlaceOnCanvas(*PanelCanvas, *CategoryTitle, FSettingsLayoutMetrics::PanelInset, 24.f,
			FSettingsLayoutMetrics::MainContentWidth, 42.f);
		StyleText(*Intro, 14, Tokens.InkMuted, TEXT("Medium"));
		Intro->SetText(LOCTEXT("ManualSaveV2", "변경 사항은 저장을 눌러야 적용됩니다."));
		Intro->SetAutoWrapText(true);
		PlaceOnCanvas(*PanelCanvas, *Intro, FSettingsLayoutMetrics::PanelInset, 72.f,
			FSettingsLayoutMetrics::MainContentWidth, 34.f);
		ContentRule->SetBrush(MakeSolidBrush(Tokens.Line));
		PlaceOnCanvas(*PanelCanvas, *ContentRule, FSettingsLayoutMetrics::PanelInset, 116.f,
			FSettingsLayoutMetrics::MainContentWidth, 1.f);
		RowsScroll->SetAnimateWheelScrolling(true);
		RowsScroll->SetWheelScrollMultiplier(56.f);
		RowsScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		RowsScroll->AddChild(Rows);
		PlaceOnCanvas(*PanelCanvas, *RowsScroll, FSettingsLayoutMetrics::PanelInset, 136.f,
			FSettingsLayoutMetrics::MainContentWidth, 528.f);


		Footer->SetBrush(FFPauseOptionsLightTeal::MakeFooterBrush(Tokens));
		Footer->SetPadding(FMargin(24.f, 0.f));
		Footer->AddChild(FooterContent);
		PlaceOnCanvas(*Design, *Footer, FSettingsLayoutMetrics::SafeX, FSettingsLayoutMetrics::FooterY,
			FSettingsLayoutMetrics::HeaderWidth, FSettingsLayoutMetrics::FooterHeight);
		StyleText(*Summary, 14, Tokens.Ink, TEXT("SemiBold"));
		Summary->SetText(LOCTEXT("SummaryPreviewV2", "저장된 설정  ·  1920 × 1080  ·  음량 80%"));
		Summary->SetAutoWrapText(false);
		StyleText(*FooterStatus, 14, Tokens.TealDeep, TEXT("SemiBold"));
		FooterStatus->SetText(LOCTEXT("FooterStatus", "●  설정 변경은 저장 전까지 적용되지 않습니다"));
		StyleText(*FooterShortcut, 14, Tokens.InkMuted, TEXT("Medium"));
		FooterShortcut->SetText(LOCTEXT("FooterShortcut", "ESC · F5  이전"));
		BackBox->SetWidthOverride(204.f);
		BackBox->SetHeightOverride(60.f);
		BackBox->AddChild(Back);
		SaveBox->SetWidthOverride(224.f);
		SaveBox->SetHeightOverride(60.f);
		SaveBox->AddChild(Save);
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(Summary))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(FooterStatus))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 26.f, 0.f));
		}
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(FooterShortcut))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
		}
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(BackBox))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		}
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(SaveBox))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	bool EnsureScreenRowsScrollable(UWidgetBlueprint& Blueprint)
	{
		if (!Blueprint.WidgetTree)
		{
			return false;
		}

		UVerticalBox* Rows = Cast<UVerticalBox>(Blueprint.WidgetTree->FindWidget(TEXT("VerticalBox_Rows")));
		if (!Rows)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: VerticalBox_Rows가 없어 스크롤 컨테이너를 추가하지 못했다."), SettingsLogPrefix);
			return false;
		}
		if (UScrollBox* ExistingScroll = Cast<UScrollBox>(Rows->GetParent()))
		{
			if (!Blueprint.WidgetVariableNameToGuidMap.Contains(ExistingScroll->GetFName()))
			{
				Blueprint.OnVariableAdded(ExistingScroll->GetFName());
				UE_LOG(LogFFUIWbpBuilder, Display,
					TEXT("%s_REPAIR: ScrollBox_Rows 변수 GUID를 복구했다."), SettingsLogPrefix);
				return SaveNewBlueprint(SettingsLogPrefix, Blueprint);
			}
			return true;
		}

		UCanvasPanel* ParentCanvas = Cast<UCanvasPanel>(Rows->GetParent());
		UCanvasPanelSlot* ExistingSlot = Cast<UCanvasPanelSlot>(Rows->Slot);
		if (!ParentCanvas || !ExistingSlot)
		{
			UE_LOG(LogFFUIWbpBuilder, Warning,
				TEXT("%s_SKIP: 디자이너가 VerticalBox_Rows 구조를 변경해 자동 스크롤 마이그레이션을 건너뛴다."),
				SettingsLogPrefix);
			return true;
		}

		const FAnchorData Layout = ExistingSlot->GetLayout();
		const bool bAutoSize = ExistingSlot->GetAutoSize();
		const int32 ZOrder = ExistingSlot->GetZOrder();
		ParentCanvas->RemoveChild(Rows);

		UScrollBox* RowsScroll = Blueprint.WidgetTree->ConstructWidget<UScrollBox>(
			UScrollBox::StaticClass(), TEXT("ScrollBox_Rows"));
		if (!RowsScroll)
		{
			return false;
		}
		Blueprint.OnVariableAdded(RowsScroll->GetFName());
		RowsScroll->SetAnimateWheelScrolling(true);
		RowsScroll->SetWheelScrollMultiplier(64.f);
		RowsScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		RowsScroll->AddChild(Rows);
		if (UCanvasPanelSlot* NewSlot = ParentCanvas->AddChildToCanvas(RowsScroll))
		{
			NewSlot->SetLayout(Layout);
			NewSlot->SetAutoSize(bAutoSize);
			NewSlot->SetZOrder(ZOrder);
		}
		else
		{
			return false;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_MIGRATE: 설정 행 영역을 ScrollBox로 확장했다."), SettingsLogPrefix);
		return SaveNewBlueprint(SettingsLogPrefix, Blueprint);
	}

	bool EnsureScreenShortcutHint(UWidgetBlueprint& Blueprint)
	{
		UTextBlock* KeyHint = Blueprint.WidgetTree
			? Cast<UTextBlock>(Blueprint.WidgetTree->FindWidget(TEXT("Text_KeyHint")))
			: nullptr;
		if (!KeyHint)
		{
			UE_LOG(LogFFUIWbpBuilder, Warning,
				TEXT("%s_SKIP: Text_KeyHint가 없어 F5 안내 마이그레이션을 건너뛴다."),
				SettingsLogPrefix);
			return true;
		}

		const FText Expected = LOCTEXT("KeyHint", "ESC · F5  이전으로");
		if (KeyHint->GetText().EqualTo(Expected))
		{
			return true;
		}

		KeyHint->SetText(Expected);
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_MIGRATE: 설정 화면 키 안내에 F5를 추가했다."), SettingsLogPrefix);
		return SaveNewBlueprint(SettingsLogPrefix, Blueprint);
	}

	bool HasSettingsScreenContract(const UWidgetBlueprint& Blueprint)
	{
		if (!Blueprint.WidgetTree)
		{
			return false;
		}
		auto HasWidget = [&Blueprint](const FName Name, const UClass& ExpectedClass)
		{
			const UWidget* Widget = Blueprint.WidgetTree->FindWidget(Name);
			return Widget && Widget->IsA(&ExpectedClass);
		};
		return HasWidget(TEXT("CanvasPanel_Root"), *UCanvasPanel::StaticClass())
			&& HasWidget(TEXT("SizeBox_Design1920x1080"), *USizeBox::StaticClass())
			&& HasWidget(TEXT("Button_Basic"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_Account"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_Keyboard"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_Graphics"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_ExitGame"), *UButton::StaticClass())
			&& HasWidget(TEXT("Text_BasicCategory"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_AccountCategory"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_KeyboardCategory"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_GraphicsCategory"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_ExitGameCategory"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_Intro"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_CategoryTitle"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_CurrentSummary"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_KeyHint"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("SafeZone_Viewport"), *USafeZone::StaticClass())
			&& HasWidget(TEXT("ScaleBox_Viewport"), *UScaleBox::StaticClass())
			&& HasWidget(TEXT("HorizontalBox_Categories"), *UHorizontalBox::StaticClass())
			&& HasWidget(TEXT("ScrollBox_Rows"), *UScrollBox::StaticClass())
			&& HasWidget(TEXT("VerticalBox_Rows"), *UVerticalBox::StaticClass())
			&& HasWidget(TEXT("Button_Close"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_Back"), *UButton::StaticClass())
			&& HasWidget(TEXT("Text_Back"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Button_Save"), *UButton::StaticClass())
			&& HasWidget(TEXT("Text_Save"), *UTextBlock::StaticClass());
	}

	/** 명시적 Refresh에서만 호출한다. 실패하면 기존 트리와 바인딩을 메모리에서 즉시 복구한다. */
	bool RefreshBuilderOwnedBlueprint(
		const FString& PackageName,
		UClass& ParentClass,
		const FName ExpectedRootName,
		FBuildLayout BuildLayout,
		TFunction<bool(const UWidgetBlueprint&)> Validate)
	{
		if (!FPackageName::DoesPackageExist(PackageName))
		{
			return EnsureWidgetBlueprint(SettingsLogPrefix, PackageName, SettingsFolder,
				ParentClass, MoveTemp(BuildLayout)) != nullptr;
		}

		UWidgetBlueprint* Blueprint = LoadExistingBlueprint(PackageName);
		if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->WidgetTree
			|| !Blueprint->GeneratedClass->IsChildOf(&ParentClass)
			|| !Blueprint->WidgetTree->RootWidget
			|| Blueprint->WidgetTree->RootWidget->GetFName() != ExpectedRootName)
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REFRESH_SKIP: 소유권 또는 부모 계약이 다른 WBP는 덮어쓰지 않는다: %s"),
				SettingsLogPrefix, *PackageName);
			return false;
		}

		UWidgetTree* OriginalTree = Blueprint->WidgetTree;
		const FName OriginalTreeName = OriginalTree->GetFName();
		const bool bOriginalTreeWasTransient = OriginalTree->HasAnyFlags(RF_Transient);
		const auto OriginalBindings = Blueprint->Bindings;
		const auto OriginalVariableMap = Blueprint->WidgetVariableNameToGuidMap;
		const FName DiscardedName = MakeUniqueObjectName(
			Blueprint, UWidgetTree::StaticClass(), TEXT("DiscardedSettingsWidgetTree"));
		if (!OriginalTree->Rename(*DiscardedName.ToString(), Blueprint,
			REN_DontCreateRedirectors | REN_NonTransactional))
		{
			return false;
		}

		Blueprint->Modify();
		OriginalTree->SetFlags(RF_Transient);
		Blueprint->Bindings.Reset();
		Blueprint->WidgetVariableNameToGuidMap.Reset();
		Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, UWidgetTree::StaticClass(),
			TEXT("WidgetTree"), RF_Transactional);

		auto Rollback = [&]()
		{
			if (UWidgetTree* FailedTree = Blueprint->WidgetTree)
			{
				const FName FailedName = MakeUniqueObjectName(
					Blueprint, UWidgetTree::StaticClass(), TEXT("FailedSettingsWidgetTree"));
				FailedTree->Rename(*FailedName.ToString(), Blueprint,
					REN_DontCreateRedirectors | REN_NonTransactional);
				FailedTree->SetFlags(RF_Transient);
			}
			if (!bOriginalTreeWasTransient)
			{
				OriginalTree->ClearFlags(RF_Transient);
			}
			OriginalTree->Rename(*OriginalTreeName.ToString(), Blueprint,
				REN_DontCreateRedirectors | REN_NonTransactional);
			Blueprint->WidgetTree = OriginalTree;
			Blueprint->Bindings = OriginalBindings;
			Blueprint->WidgetVariableNameToGuidMap = OriginalVariableMap;
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		};

		if (!Blueprint->WidgetTree || !BuildLayout(*Blueprint, *Blueprint->WidgetTree)
			|| !Validate(*Blueprint) || !SaveNewBlueprint(SettingsLogPrefix, *Blueprint))
		{
			Rollback();
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REFRESH_FAIL: 기존 WidgetTree를 복구했다: %s"),
				SettingsLogPrefix, *PackageName);
			return false;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_REFRESHED: 전체 화면 설정 레이아웃 저장 완료: %s"),
			SettingsLogPrefix, *PackageName);
		return true;
	}

	void RefreshSettingsUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_REFRESH_BEGIN: builder-owned 설정 WBP 4종을 갱신한다."), SettingsLogPrefix);

		const auto RootOnly = [](const UWidgetBlueprint& Blueprint)
		{
			return Blueprint.WidgetTree && Blueprint.WidgetTree->RootWidget != nullptr;
		};
		const bool bToggle = RefreshBuilderOwnedBlueprint(ToggleRowPackage,
			*UFF_SettingToggleRow::StaticClass(), TEXT("SizeBox_Root"), BuildToggleRow, RootOnly);
		const bool bSlider = RefreshBuilderOwnedBlueprint(SliderRowPackage,
			*UFF_SettingSliderRow::StaticClass(), TEXT("SizeBox_Root"), BuildSliderRow, RootOnly);
		const bool bSelect = RefreshBuilderOwnedBlueprint(SelectRowPackage,
			*UFF_SettingSelectRow::StaticClass(), TEXT("SizeBox_Root"), BuildSelectRow, RootOnly);
		const bool bScreen = RefreshBuilderOwnedBlueprint(ScreenPackage,
			*UFF_FestivalSettingsScreen::StaticClass(), TEXT("CanvasPanel_Root"), BuildScreenV2,
			HasSettingsScreenContract);

		if (bToggle && bSlider && bSelect && bScreen)
		{
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_REFRESH_END: 4종 갱신 완료"), SettingsLogPrefix);
		}
		else
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REFRESH_END: 일부 갱신 실패"), SettingsLogPrefix);
		}
	}

	void RunSettingsUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display, TEXT("%s_BEGIN: 없는 WBP만 생성한다."), SettingsLogPrefix);

		EnsureWidgetBlueprint(SettingsLogPrefix, ToggleRowPackage, SettingsFolder,
			*UFF_SettingToggleRow::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildToggleRow(Blueprint, Tree); });
		EnsureWidgetBlueprint(SettingsLogPrefix, SliderRowPackage, SettingsFolder,
			*UFF_SettingSliderRow::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildSliderRow(Blueprint, Tree); });
		EnsureWidgetBlueprint(SettingsLogPrefix, SelectRowPackage, SettingsFolder,
			*UFF_SettingSelectRow::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildSelectRow(Blueprint, Tree); });
		UWidgetBlueprint* ScreenBlueprint = EnsureWidgetBlueprint(SettingsLogPrefix, ScreenPackage, SettingsFolder,
			*UFF_FestivalSettingsScreen::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildScreenV2(Blueprint, Tree); });
		if (ScreenBlueprint)
		{
			EnsureScreenRowsScrollable(*ScreenBlueprint);
			EnsureScreenShortcutHint(*ScreenBlueprint);
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_END: 생성 완료. 이후 레이아웃·문구·행 클래스는 WBP Class Defaults에서 조정한다."),
			SettingsLogPrefix);
	}

	FAutoConsoleCommand SettingsUmgCommand(
		TEXT("FF.SettingsUMG"),
		TEXT("LightTeal 전체 화면 옵션과 토글/슬라이더/선택형 WBP를 만든다. 기존 디자인은 보존한다."),
		FConsoleCommandDelegate::CreateStatic(&RunSettingsUmg));

	FAutoConsoleCommand SettingsUmgRefreshCommand(
		TEXT("FF.SettingsUMG.Refresh"),
		TEXT("builder-owned PauseOptions와 설정 행 WBP를 전체 화면 LightTeal 템플릿으로 명시적 갱신한다."),
		FConsoleCommandDelegate::CreateStatic(&RefreshSettingsUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
