// FF_SettingsWbpBuilder.cpp

#include "CoreMinimal.h"

#if WITH_EDITOR

// 메인 축제 설정 화면의 비파괴 WBP 스캐폴더. 에디터 콘솔 명령은 `FF.SettingsUMG`다.
// 기존 에셋이 있으면 절대 덮어쓰지 않으므로 디자이너가 이후 손본 결과가 보존된다.

#include "Blueprint/WidgetTree.h"
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
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/IConsoleManager.h"
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
	const FString ScreenPackage(SettingsFolder / TEXT("WBP_FF_FestivalSettings"));

	FSlateBrush MakeSolidBrush(const FLinearColor& Color, const float CornerRadius = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);
		return Brush;
	}

	FButtonStyle MakeButtonStyle(const UFF_UIPalette& Palette, bool bPrimary = false)
	{
		FButtonStyle Style;
		Style.Normal = MakeSolidBrush(bPrimary ? Palette.InGame.Teal : Palette.InGame.PaperSunken, 6.f);
		Style.Hovered = MakeSolidBrush(bPrimary ? Palette.InGame.TealDeep : Palette.InGame.Hover, 6.f);
		Style.Pressed = MakeSolidBrush(Palette.InGame.LineStrong, 6.f);
		Style.Disabled = MakeSolidBrush(Palette.InGame.Beige, 6.f);
		Style.NormalPadding = FMargin(16.f, 10.f);
		Style.PressedPadding = FMargin(16.f, 11.f, 16.f, 9.f);
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
		Root->SetHeightOverride(92.f);
		Root->AddChild(Overlay);
		Background->SetBrush(MakeSolidBrush(Palette->InGame.PaperSunken, 6.f));
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
		Root->SetHeightOverride(92.f);
		Root->AddChild(Overlay);
		Background->SetBrush(MakeSolidBrush(Palette->InGame.PaperSunken, 6.f));
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
		Root->SetHeightOverride(92.f);
		Root->AddChild(Overlay);
		Background->SetBrush(MakeSolidBrush(Palette->InGame.PaperSunken, 6.f));
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

	bool BuildScreen(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UBorder* Scrim = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Scrim"));
		UBorder* Header = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Header"));
		UHorizontalBox* HeaderContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* HeaderBrand = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HeaderBrand"));
		UTextBlock* HeaderTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HeaderTitle"));
		UTextBlock* KeyHint = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_KeyHint"));
		UButton* HeaderClose = MakeTextButton(Blueprint, Tree, TEXT("Button_Close"), TEXT("Text_Close"),
			LOCTEXT("CloseGlyph", "×"), *Palette);
		UBorder* Panel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_SettingsPanel"));
		UCanvasPanel* PanelCanvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_SettingsPanel"));
		UTextBlock* Intro = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Intro"));
		UVerticalBox* Categories = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Categories"));
		UBorder* Divider = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Divider"));
		UTextBlock* CategoryTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CategoryTitle"), true);
		UScrollBox* RowsScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Rows"));
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Rows"), true);
		UBorder* SummaryPanel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_CurrentSummary"));
		UTextBlock* Summary = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CurrentSummary"), true);
		UButton* Reset = MakeTextButton(Blueprint, Tree, TEXT("Button_Reset"), TEXT("Text_Reset"),
			LOCTEXT("Reset", "기본값 복원"), *Palette);
		UButton* FooterClose = MakeTextButton(Blueprint, Tree, TEXT("Button_Done"), TEXT("Text_FooterClose"),
			LOCTEXT("Done", "설정 완료"), *Palette, true);

		if (!Root || !Scrim || !Header || !HeaderContent || !HeaderBrand || !HeaderTitle || !KeyHint
			|| !HeaderClose || !Panel || !PanelCanvas || !Intro || !Categories || !Divider || !CategoryTitle
			|| !RowsScroll || !Rows || !SummaryPanel || !Summary || !Reset || !FooterClose)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Scrim->SetBrush(MakeSolidBrush(Palette->InGame.Scrim));
		StretchOnCanvas(*Root, *Scrim);

		Header->SetBrush(MakeSolidBrush(Palette->InGame.TealDeep));
		Header->SetPadding(FMargin(38.f, 0.f));
		Header->AddChild(HeaderContent);
		PlaceOnCanvas(*Root, *Header, 0.f, 0.f, 1920.f, 86.f);

		StyleText(*HeaderBrand, 28, Palette->InGame.White, TEXT("SemiBold"));
		HeaderBrand->SetText(LOCTEXT("Brand", "FESTA FIESTA"));
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(HeaderBrand))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 34.f, 0.f));
		}
		StyleText(*HeaderTitle, 24, Palette->InGame.White, TEXT("SemiBold"));
		HeaderTitle->SetText(LOCTEXT("SettingsTitle", "설정"));
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(HeaderTitle))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		StyleText(*KeyHint, 16, Palette->InGame.TealSoft, TEXT("Medium"));
		KeyHint->SetText(LOCTEXT("KeyHint", "ESC / F5  닫기"));
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
		PlaceOnCanvas(*Root, *Panel, 120.f, 112.f, 1680.f, 852.f);

		StyleText(*Intro, 16, Palette->InGame.InkMuted, TEXT("Medium"));
		Intro->SetText(LOCTEXT("AutoSave", "변경 사항은 이 기기에 즉시 적용되고 자동 저장됩니다."));
		PlaceOnCanvas(*PanelCanvas, *Intro, 44.f, 28.f, 1540.f, 34.f);

		struct FCategoryDefinition
		{
			FName ButtonName;
			FName TextName;
			FText Label;
		};
		const FCategoryDefinition CategoryDefinitions[] = {
			{ TEXT("Button_Display"), TEXT("Text_DisplayCategory"), LOCTEXT("Display", "화면") },
			{ TEXT("Button_Audio"), TEXT("Text_AudioCategory"), LOCTEXT("Audio", "오디오") },
			{ TEXT("Button_Gameplay"), TEXT("Text_GameplayCategory"), LOCTEXT("Gameplay", "게임플레이") },
			{ TEXT("Button_Accessibility"), TEXT("Text_AccessibilityCategory"), LOCTEXT("Accessibility", "접근성") }
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
			ButtonBox->SetHeightOverride(92.f);
			ButtonBox->AddChild(Button);
			if (UVerticalBoxSlot* Slot = Categories->AddChildToVerticalBox(ButtonBox))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
				Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
			}
		}
		PlaceOnCanvas(*PanelCanvas, *Categories, 44.f, 92.f, 300.f, 500.f);

		Divider->SetBrush(MakeSolidBrush(Palette->InGame.Line));
		PlaceOnCanvas(*PanelCanvas, *Divider, 380.f, 88.f, 2.f, 620.f);

		StyleText(*CategoryTitle, 30, Palette->InGame.Ink, TEXT("SemiBold"));
		CategoryTitle->SetText(LOCTEXT("GameplayPreview", "게임플레이"));
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
		Summary->SetText(LOCTEXT("SummaryPreview", "현재 적용  ·  1920 × 1080  ·  전체 화면  ·  음량 80%"));
		PlaceOnCanvas(*PanelCanvas, *SummaryPanel, 430.f, 628.f, 1110.f, 58.f);

		PlaceOnCanvas(*PanelCanvas, *Reset, 44.f, 744.f, 260.f, 70.f);
		PlaceOnCanvas(*PanelCanvas, *FooterClose, 1300.f, 744.f, 240.f, 70.f);
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
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree) { return BuildScreen(Blueprint, Tree); });
		if (ScreenBlueprint)
		{
			EnsureScreenRowsScrollable(*ScreenBlueprint);
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_END: 생성 완료. 이후 레이아웃·문구·행 클래스는 WBP Class Defaults에서 조정한다."),
			SettingsLogPrefix);
	}

	FAutoConsoleCommand SettingsUmgCommand(
		TEXT("FF.SettingsUMG"),
		TEXT("메인 축제 Light Teal 설정 화면과 토글/슬라이더/선택형 WBP를 만든다. 기존 디자인은 보존한다."),
		FConsoleCommandDelegate::CreateStatic(&RunSettingsUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
