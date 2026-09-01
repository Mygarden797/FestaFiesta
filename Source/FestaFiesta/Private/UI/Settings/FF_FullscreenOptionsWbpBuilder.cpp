// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR

// This builder exclusively owns WBP_FF_FullscreenOptions.  It intentionally does not touch
// the team's legacy settings WBP or the older pause-options scaffold.

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SafeZone.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
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
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaFullscreenOptionsWbpBuilder"

namespace FFFullscreenOptionsWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* const LogPrefix = TEXT("FF_FULLSCREEN_OPTIONS_UMG");
	const FString SettingsFolder(TEXT("/Game/FestaFiesta/UI/Settings"));
	const FString ScreenPackage(SettingsFolder / TEXT("WBP_FF_FullscreenOptions"));

	/**
	 * Only vertical rhythm is fixed. Width and body height are always resolved by UMG from the
	 * current viewport, so 4:3, 16:10, 16:9 and ultrawide windows all receive an edge-to-edge shell.
	 */
	struct FResponsiveMetrics
	{
		static constexpr float HeaderHeight = 88.f;
		static constexpr float CategoryHeight = 80.f;
		static constexpr float FooterHeight = 92.f;
		static constexpr float HorizontalInset = 48.f;
		static constexpr float BodyTopInset = 24.f;
		static constexpr float BodyBottomInset = 20.f;
		static constexpr float CategoryButtonHeight = 56.f;
		static constexpr float ActionButtonHeight = 58.f;
		// Category/footer content is authored once at this width and only scales down when a
		// compact or portrait viewport cannot preserve every action at its native size.
		static constexpr float AuthoredActionWidth = 1080.f;
		static constexpr float FooterContentHeight = FooterHeight - 24.f;
	};

	FSlateBrush MakeSolidBrush(const FLinearColor& Color, const float Radius = 0.f,
		const FLinearColor& Outline = FLinearColor::Transparent, const float OutlineWidth = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = OutlineWidth;
		return Brush;
	}

	void ConfigureSingleLineText(
		UTextBlock& Text,
		const FText& ToolTip,
		const ETextJustify::Type Justification = ETextJustify::Left)
	{
		Text.SetAutoWrapText(false);
		Text.SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Text.SetClipping(EWidgetClipping::ClipToBounds);
		Text.SetJustification(Justification);
		Text.SetToolTipText(ToolTip);
	}

	void ConfigureWrappedText(
		UTextBlock& Text,
		const ETextJustify::Type Justification = ETextJustify::Left)
	{
		Text.SetAutoWrapText(true);
		Text.SetClipping(EWidgetClipping::ClipToBounds);
		Text.SetJustification(Justification);
	}

	FButtonStyle MakeActionStyle(const UFF_UIPalette& Palette, const bool bPrimary)
	{
		FButtonStyle Style;
		Style.Normal = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			bPrimary ? FFPauseOptionsLightTeal::RectPrimaryPath
				: FFPauseOptionsLightTeal::RectSecondaryPath,
			bPrimary ? Palette.InGame.Teal : Palette.InGame.PaperSunken,
			bPrimary ? Palette.InGame.TealDeep : Palette.InGame.Line);
		Style.Hovered = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			bPrimary ? FFPauseOptionsLightTeal::RectPrimaryHoverPath
				: FFPauseOptionsLightTeal::RectNavHoverPath,
			bPrimary ? Palette.InGame.TealDeep : Palette.InGame.Hover,
			Palette.InGame.TealDeep);
		Style.Pressed = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			FFPauseOptionsLightTeal::RectNavPressedPath,
			Palette.InGame.TealDeep, Palette.InGame.TealDeep);
		Style.Disabled = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			FFPauseOptionsLightTeal::RectNavDisabledPath,
			Palette.InGame.Beige, Palette.InGame.Line);
		Style.NormalPadding = FMargin(16.f, 8.f);
		Style.PressedPadding = FMargin(16.f, 9.f, 16.f, 7.f);
		return Style;
	}

	FButtonStyle MakeCategoryStyle(const UFF_UIPalette& Palette)
	{
		FButtonStyle Style = MakeActionStyle(Palette, false);
		Style.Normal = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			FFPauseOptionsLightTeal::RectNavDefaultPath,
			Palette.InGame.Paper, Palette.InGame.Line);
		Style.NormalPadding = FMargin(18.f, 8.f);
		Style.PressedPadding = FMargin(18.f, 9.f, 18.f, 7.f);
		return Style;
	}

	UButton* MakeTextButton(UWidgetBlueprint& Blueprint, UWidgetTree& Tree,
		const FName ButtonName, const FName TextName, const FText& Label,
		const UFF_UIPalette& Palette, const bool bPrimary = false)
	{
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, true);
		UTextBlock* Text = MakeWidget<UTextBlock>(Blueprint, Tree, TextName, true);
		if (!Button || !Text)
		{
			return nullptr;
		}

		Button->SetStyle(MakeActionStyle(Palette, bPrimary));
		Button->SetClipping(EWidgetClipping::ClipToBounds);
		StyleText(*Text, 17, bPrimary ? Palette.InGame.White : Palette.InGame.Ink, TEXT("SemiBold"));
		Text->SetText(Label);
		ConfigureWrappedText(*Text, ETextJustify::Center);
		Text->SetToolTipText(Label);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->AddChild(Text)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		return Button;
	}

	bool BuildResponsiveScreen(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UBorder* ViewportSurface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ViewportSurface"));
		USafeZone* SafeZone = MakeWidget<USafeZone>(Blueprint, Tree, TEXT("SafeZone_Viewport"));
		UVerticalBox* Shell = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_ResponsiveShell"));

		USizeBox* HeaderBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Header"));
		UBorder* Header = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Header"));
		UHorizontalBox* HeaderContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* HeaderBrand = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HeaderBrand"));
		UTextBlock* HeaderTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HeaderTitle"));
		UTextBlock* KeyHint = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_KeyHint"), true);
		USizeBox* CloseBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Close"));
		UButton* Close = MakeTextButton(Blueprint, Tree, TEXT("Button_Close"), TEXT("Text_Close"),
			LOCTEXT("CloseGlyph", "×"), *Palette);

		USizeBox* CategoryBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_CategoryBar"));
		UBorder* CategoryBar = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_CategoryBar"));
		UScaleBox* CategoryScale = MakeWidget<UScaleBox>(
			Blueprint, Tree, TEXT("ScaleBox_CategoryActions"));
		USizeBox* CategoryDesign = MakeWidget<USizeBox>(
			Blueprint, Tree, TEXT("SizeBox_CategoryActionsDesign"));
		UHorizontalBox* Categories = MakeWidget<UHorizontalBox>(
			Blueprint, Tree, TEXT("HorizontalBox_Categories"), true);

		UBorder* Body = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_SettingsPanel"));
		UVerticalBox* BodyContent = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_SettingsContent"));
		UTextBlock* CategoryTitle = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_CategoryTitle"), true);
		UTextBlock* Intro = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Intro"), true);
		USizeBox* RuleBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_ContentRule"));
		UBorder* ContentRule = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ContentRule"));
		UScrollBox* RowsScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Rows"));
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Rows"), true);

		USizeBox* FooterBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Footer"));
		UBorder* Footer = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Footer"));
		UScaleBox* FooterScale = MakeWidget<UScaleBox>(
			Blueprint, Tree, TEXT("ScaleBox_FooterActions"));
		USizeBox* FooterDesign = MakeWidget<USizeBox>(
			Blueprint, Tree, TEXT("SizeBox_FooterActionsDesign"));
		UHorizontalBox* FooterContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Footer"));
		UTextBlock* Summary = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CurrentSummary"), true);
		USizeBox* BackBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_BackAction"));
		UButton* Back = MakeTextButton(Blueprint, Tree, TEXT("Button_Back"), TEXT("Text_Back"),
			LOCTEXT("Back", "이전으로"), *Palette);
		USizeBox* SaveBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_SaveAction"));
		UButton* Save = MakeTextButton(Blueprint, Tree, TEXT("Button_Save"), TEXT("Text_Save"),
			LOCTEXT("Save", "저장"), *Palette, true);

		if (!Root || !ViewportSurface || !SafeZone || !Shell
			|| !HeaderBox || !Header || !HeaderContent || !HeaderBrand || !HeaderTitle || !KeyHint || !CloseBox || !Close
			|| !CategoryBox || !CategoryBar || !CategoryScale || !CategoryDesign || !Categories
			|| !Body || !BodyContent || !CategoryTitle || !Intro || !RuleBox || !ContentRule || !RowsScroll || !Rows
			|| !FooterBox || !Footer || !FooterScale || !FooterDesign || !FooterContent
			|| !Summary || !BackBox || !Back || !SaveBox || !Save)
		{
			return false;
		}

		Tree.RootWidget = Root;

		// Opaque, full-viewport ownership. The world is never visible through gaps or aspect-ratio bars.
		ViewportSurface->SetBrush(MakeSolidBrush(Palette->InGame.Paper));
		ViewportSurface->SetVisibility(ESlateVisibility::Visible);
		StretchOnCanvas(*Root, *ViewportSurface);
		SafeZone->AddChild(Shell);
		StretchOnCanvas(*Root, *SafeZone);

		HeaderBox->SetHeightOverride(FResponsiveMetrics::HeaderHeight);
		Header->SetBrush(MakeSolidBrush(Palette->InGame.TealDeep));
		Header->SetClipping(EWidgetClipping::ClipToBounds);
		HeaderContent->SetClipping(EWidgetClipping::ClipToBounds);
		Header->SetPadding(FMargin(FResponsiveMetrics::HorizontalInset, 0.f));
		Header->AddChild(HeaderContent);
		HeaderBox->AddChild(Header);
		if (UVerticalBoxSlot* Slot = Shell->AddChildToVerticalBox(HeaderBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*HeaderBrand, 30, Palette->InGame.White, TEXT("SemiBold"));
		const FText HeaderBrandText = LOCTEXT("Brand", "FESTA FIESTA!");
		HeaderBrand->SetText(HeaderBrandText);
		ConfigureSingleLineText(*HeaderBrand, HeaderBrandText);
		StyleText(*HeaderTitle, 20, Palette->InGame.White, TEXT("SemiBold"));
		const FText HeaderTitleText = LOCTEXT("Title", "옵션");
		HeaderTitle->SetText(HeaderTitleText);
		ConfigureSingleLineText(*HeaderTitle, HeaderTitleText, ETextJustify::Center);
		StyleText(*KeyHint, 14, Palette->InGame.TealSoft, TEXT("Medium"));
		const FText KeyHintText = LOCTEXT("KeyHint", "ESC · F5  이전으로");
		KeyHint->SetText(KeyHintText);
		ConfigureSingleLineText(*KeyHint, KeyHintText, ETextJustify::Right);
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(HeaderBrand))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 28.f, 0.f));
		}
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(HeaderTitle))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(KeyHint))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
		}
		CloseBox->SetWidthOverride(52.f);
		CloseBox->SetHeightOverride(52.f);
		CloseBox->AddChild(Close);
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(CloseBox))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		CategoryBox->SetHeightOverride(FResponsiveMetrics::CategoryHeight);
		CategoryBox->SetClipping(EWidgetClipping::ClipToBounds);
		CategoryBar->SetBrush(MakeSolidBrush(
			Palette->InGame.StatusBar, 0.f, Palette->InGame.Line, 1.f));
		CategoryBar->SetClipping(EWidgetClipping::ClipToBounds);
		CategoryScale->SetStretch(EStretch::ScaleToFit);
		CategoryScale->SetStretchDirection(EStretchDirection::DownOnly);
		CategoryScale->SetClipping(EWidgetClipping::ClipToBounds);
		CategoryDesign->SetWidthOverride(FResponsiveMetrics::AuthoredActionWidth);
		CategoryDesign->SetHeightOverride(FResponsiveMetrics::CategoryButtonHeight);
		CategoryDesign->SetClipping(EWidgetClipping::ClipToBounds);
		Categories->SetClipping(EWidgetClipping::ClipToBounds);
		CategoryBar->SetPadding(FMargin(FResponsiveMetrics::HorizontalInset, 12.f));
		CategoryDesign->AddChild(Categories);
		CategoryScale->AddChild(CategoryDesign);
		CategoryBar->AddChild(CategoryScale);
		CategoryBox->AddChild(CategoryBar);
		if (UVerticalBoxSlot* Slot = Shell->AddChildToVerticalBox(CategoryBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}

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
			{ TEXT("Button_ExitGame"), TEXT("Text_ExitGameCategory"), LOCTEXT("Exit", "게임종료") }
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
			Button->SetStyle(MakeCategoryStyle(*Palette));
			ButtonBox->SetHeightOverride(FResponsiveMetrics::CategoryButtonHeight);
			ButtonBox->SetClipping(EWidgetClipping::ClipToBounds);
			ButtonBox->AddChild(Button);
			if (Definition.ButtonName == FName(TEXT("Button_ExitGame")))
			{
				if (UTextBlock* ExitText = Cast<UTextBlock>(Tree.FindWidget(Definition.TextName)))
				{
					ExitText->SetColorAndOpacity(Palette->InGame.Danger);
				}
			}
			if (UHorizontalBoxSlot* Slot = Categories->AddChildToHorizontalBox(ButtonBox))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				Slot->SetVerticalAlignment(VAlign_Center);
				Slot->SetPadding(FMargin(0.f, 0.f,
					Definition.ButtonName == FName(TEXT("Button_ExitGame")) ? 0.f : 8.f, 0.f));
			}
		}

		Body->SetBrush(MakeSolidBrush(Palette->InGame.Paper));
		Body->SetClipping(EWidgetClipping::ClipToBounds);
		BodyContent->SetClipping(EWidgetClipping::ClipToBounds);
		Body->SetPadding(FMargin(FResponsiveMetrics::HorizontalInset, FResponsiveMetrics::BodyTopInset,
			FResponsiveMetrics::HorizontalInset, FResponsiveMetrics::BodyBottomInset));
		Body->AddChild(BodyContent);
		if (UVerticalBoxSlot* Slot = Shell->AddChildToVerticalBox(Body))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		StyleText(*CategoryTitle, 31, Palette->InGame.Ink, TEXT("SemiBold"));
		const FText CategoryPreviewText = LOCTEXT("CategoryPreview", "기본설정");
		CategoryTitle->SetText(CategoryPreviewText);
		ConfigureSingleLineText(*CategoryTitle, CategoryPreviewText);
		if (UVerticalBoxSlot* Slot = BodyContent->AddChildToVerticalBox(CategoryTitle))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		StyleText(*Intro, 14, Palette->InGame.InkMuted, TEXT("Medium"));
		Intro->SetText(LOCTEXT("Intro", "변경 사항은 저장을 눌러야 적용됩니다."));
		ConfigureWrappedText(*Intro);
		if (UVerticalBoxSlot* Slot = BodyContent->AddChildToVerticalBox(Intro))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 5.f, 0.f, 18.f));
		}
		ContentRule->SetBrush(MakeSolidBrush(Palette->InGame.Line));
		RuleBox->SetHeightOverride(1.f);
		RuleBox->AddChild(ContentRule);
		if (UVerticalBoxSlot* Slot = BodyContent->AddChildToVerticalBox(RuleBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		RowsScroll->SetAnimateWheelScrolling(true);
		RowsScroll->SetWheelScrollMultiplier(56.f);
		RowsScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		RowsScroll->SetClipping(EWidgetClipping::ClipToBounds);
		Rows->SetClipping(EWidgetClipping::ClipToBounds);
		RowsScroll->AddChild(Rows);
		if (UVerticalBoxSlot* Slot = BodyContent->AddChildToVerticalBox(RowsScroll))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
			Slot->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
		}

		FooterBox->SetHeightOverride(FResponsiveMetrics::FooterHeight);
		FooterBox->SetClipping(EWidgetClipping::ClipToBounds);
		Footer->SetBrush(MakeSolidBrush(
			Palette->InGame.StatusBar, 0.f, Palette->InGame.Line, 1.f));
		Footer->SetClipping(EWidgetClipping::ClipToBounds);
		FooterScale->SetStretch(EStretch::ScaleToFit);
		FooterScale->SetStretchDirection(EStretchDirection::DownOnly);
		FooterScale->SetClipping(EWidgetClipping::ClipToBounds);
		FooterDesign->SetWidthOverride(FResponsiveMetrics::AuthoredActionWidth);
		FooterDesign->SetHeightOverride(FResponsiveMetrics::FooterContentHeight);
		FooterDesign->SetClipping(EWidgetClipping::ClipToBounds);
		FooterContent->SetClipping(EWidgetClipping::ClipToBounds);
		Footer->SetPadding(FMargin(FResponsiveMetrics::HorizontalInset, 12.f));
		FooterDesign->AddChild(FooterContent);
		FooterScale->AddChild(FooterDesign);
		Footer->AddChild(FooterScale);
		FooterBox->AddChild(Footer);
		if (UVerticalBoxSlot* Slot = Shell->AddChildToVerticalBox(FooterBox))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}

		StyleText(*Summary, 14, Palette->InGame.Ink, TEXT("SemiBold"));
		const FText SummaryPreview = LOCTEXT(
			"Summary", "저장된 설정  ·  전체 화면  ·  1920 × 1080  ·  음량 80%");
		Summary->SetText(SummaryPreview);
		ConfigureSingleLineText(*Summary, SummaryPreview);
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(Summary))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
		}
		BackBox->SetWidthOverride(196.f);
		BackBox->SetHeightOverride(FResponsiveMetrics::ActionButtonHeight);
		BackBox->SetClipping(EWidgetClipping::ClipToBounds);
		BackBox->AddChild(Back);
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(BackBox))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		}
		SaveBox->SetWidthOverride(216.f);
		SaveBox->SetHeightOverride(FResponsiveMetrics::ActionButtonHeight);
		SaveBox->SetClipping(EWidgetClipping::ClipToBounds);
		SaveBox->AddChild(Save);
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(SaveBox))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	bool HasResponsiveContract(const UWidgetBlueprint& Blueprint)
	{
		if (!Blueprint.WidgetTree)
		{
			return false;
		}
		auto Has = [&Blueprint](const FName Name, const UClass& ExpectedClass)
		{
			const UWidget* Widget = Blueprint.WidgetTree->FindWidget(Name);
			return Widget && Widget->IsA(&ExpectedClass);
		};
		return Has(TEXT("CanvasPanel_Root"), *UCanvasPanel::StaticClass())
			&& Has(TEXT("Border_ViewportSurface"), *UBorder::StaticClass())
			&& Has(TEXT("SafeZone_Viewport"), *USafeZone::StaticClass())
			&& Has(TEXT("VerticalBox_ResponsiveShell"), *UVerticalBox::StaticClass())
			&& Has(TEXT("ScaleBox_CategoryActions"), *UScaleBox::StaticClass())
			&& Has(TEXT("SizeBox_CategoryActionsDesign"), *USizeBox::StaticClass())
			&& Has(TEXT("HorizontalBox_Categories"), *UHorizontalBox::StaticClass())
			&& Has(TEXT("ScrollBox_Rows"), *UScrollBox::StaticClass())
			&& Has(TEXT("VerticalBox_Rows"), *UVerticalBox::StaticClass())
			&& Has(TEXT("Text_CategoryTitle"), *UTextBlock::StaticClass())
			&& Has(TEXT("Text_Intro"), *UTextBlock::StaticClass())
			&& Has(TEXT("Text_CurrentSummary"), *UTextBlock::StaticClass())
			&& Has(TEXT("ScaleBox_FooterActions"), *UScaleBox::StaticClass())
			&& Has(TEXT("SizeBox_FooterActionsDesign"), *USizeBox::StaticClass())
			&& Has(TEXT("Button_Basic"), *UButton::StaticClass())
			&& Has(TEXT("Button_Account"), *UButton::StaticClass())
			&& Has(TEXT("Button_Keyboard"), *UButton::StaticClass())
			&& Has(TEXT("Button_Graphics"), *UButton::StaticClass())
			&& Has(TEXT("Button_ExitGame"), *UButton::StaticClass())
			&& Has(TEXT("Button_Close"), *UButton::StaticClass())
			&& Has(TEXT("Button_Back"), *UButton::StaticClass())
			&& Has(TEXT("Button_Save"), *UButton::StaticClass());
	}

	bool RefreshExisting(UWidgetBlueprint& Blueprint)
	{
		if (!Blueprint.GeneratedClass || !Blueprint.WidgetTree || !Blueprint.WidgetTree->RootWidget
			|| !Blueprint.GeneratedClass->IsChildOf(UFF_FestivalSettingsScreen::StaticClass())
			|| Blueprint.WidgetTree->RootWidget->GetFName() != FName(TEXT("CanvasPanel_Root")))
		{
			return false;
		}

		UWidgetTree* OriginalTree = Blueprint.WidgetTree;
		const FName OriginalTreeName = OriginalTree->GetFName();
		const bool bOriginalTreeWasTransient = OriginalTree->HasAnyFlags(RF_Transient);
		const auto OriginalBindings = Blueprint.Bindings;
		const auto OriginalVariableMap = Blueprint.WidgetVariableNameToGuidMap;
		const FName DiscardedName = MakeUniqueObjectName(
			&Blueprint, UWidgetTree::StaticClass(), TEXT("DiscardedFullscreenOptionsTree"));
		if (!OriginalTree->Rename(*DiscardedName.ToString(), &Blueprint,
			REN_DontCreateRedirectors | REN_NonTransactional))
		{
			return false;
		}

		Blueprint.Modify();
		OriginalTree->SetFlags(RF_Transient);
		Blueprint.Bindings.Reset();
		Blueprint.WidgetVariableNameToGuidMap.Reset();
		Blueprint.WidgetTree = NewObject<UWidgetTree>(
			&Blueprint, UWidgetTree::StaticClass(), TEXT("WidgetTree"), RF_Transactional);

		auto Rollback = [&]()
		{
			if (UWidgetTree* FailedTree = Blueprint.WidgetTree)
			{
				const FName FailedName = MakeUniqueObjectName(
					&Blueprint, UWidgetTree::StaticClass(), TEXT("FailedFullscreenOptionsTree"));
				FailedTree->Rename(*FailedName.ToString(), &Blueprint,
					REN_DontCreateRedirectors | REN_NonTransactional);
				FailedTree->SetFlags(RF_Transient);
			}
			if (!bOriginalTreeWasTransient)
			{
				OriginalTree->ClearFlags(RF_Transient);
			}
			OriginalTree->Rename(*OriginalTreeName.ToString(), &Blueprint,
				REN_DontCreateRedirectors | REN_NonTransactional);
			Blueprint.WidgetTree = OriginalTree;
			Blueprint.Bindings = OriginalBindings;
			Blueprint.WidgetVariableNameToGuidMap = OriginalVariableMap;
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(&Blueprint);
			FKismetEditorUtilities::CompileBlueprint(&Blueprint);
		};

		if (!Blueprint.WidgetTree || !BuildResponsiveScreen(Blueprint, *Blueprint.WidgetTree)
			|| !HasResponsiveContract(Blueprint) || !SaveNewBlueprint(LogPrefix, Blueprint))
		{
			Rollback();
			return false;
		}
		return true;
	}

	void RefreshFullscreenOptions()
	{
		bool bSuccess = false;
		if (FPackageName::DoesPackageExist(ScreenPackage))
		{
			if (UWidgetBlueprint* Existing = LoadExistingBlueprint(ScreenPackage))
			{
				bSuccess = RefreshExisting(*Existing);
			}
		}
		else
		{
			bSuccess = EnsureWidgetBlueprint(LogPrefix, ScreenPackage, SettingsFolder,
				*UFF_FestivalSettingsScreen::StaticClass(),
				[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
				{
					return BuildResponsiveScreen(Blueprint, Tree);
				}) != nullptr;
		}

		if (bSuccess)
		{
			UE_LOG(LogFFUIWbpBuilder, Display,
				TEXT("%s_REFRESHED: 해상도 독립 전체 화면 옵션 WBP %s"),
				LogPrefix, *ScreenPackage);
		}
		else
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAILED: 해상도 독립 전체 화면 옵션 WBP %s"),
				LogPrefix, *ScreenPackage);
		}
	}

	FAutoConsoleCommand RefreshCommand(
		TEXT("FF.FullscreenOptions.Refresh"),
		TEXT("팀 에셋을 수정하지 않고 SafeZone/Fill 기반 전체 화면 옵션 WBP를 생성 또는 갱신한다."),
		FConsoleCommandDelegate::CreateStatic(&RefreshFullscreenOptions));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
