// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
// FF_FullscreenOptionsWbpBuilder.cpp
// 역할: 전체 화면 옵션 메뉴의 저장된 위젯 구성을 만든다.

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
#include "Components/Image.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UI/Settings/FF_FestivalOptionsStyle.h"
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
		static constexpr float FooterHeight = 104.f;
		static constexpr float HorizontalInset = 32.f;
		static constexpr float NavigationWidth = 244.f;
		static constexpr float PanelInset = 24.f;
		static constexpr float CategoryButtonHeight = 48.f;
		static constexpr float ActionButtonHeight = 56.f;
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
		const auto T = FFFestivalOptionsStyle::Get();
		FButtonStyle Style;
		Style.Normal = FFFestivalOptionsStyle::Rounded(
			bPrimary ? T.Green : T.Paper, bPrimary ? T.Green : T.LineStrong);
		Style.Hovered = FFFestivalOptionsStyle::Rounded(
			bPrimary ? T.GreenDeep : T.Hover, T.Green, 12.f, 2.f);
		Style.Pressed = FFFestivalOptionsStyle::Rounded(
			bPrimary ? T.GreenDeep : T.Selected, T.GreenDeep, 12.f, 2.f);
		Style.Disabled = FFFestivalOptionsStyle::Rounded(
			bPrimary ? T.Muted : T.Disabled, T.LineStrong);
		Style.NormalPadding = FMargin(18.f, 10.f);
		Style.PressedPadding = FMargin(18.f, 11.f, 18.f, 9.f);
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
		const auto T = FFFestivalOptionsStyle::Get();
		StyleText(*Text, 18, bPrimary ? T.White : T.Ink, TEXT("SemiBold"));
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
		if (!Palette) return false;
		const auto T = FFFestivalOptionsStyle::Get();

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UBorder* Surface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ViewportSurface"));
		USafeZone* SafeZone = MakeWidget<USafeZone>(Blueprint, Tree, TEXT("SafeZone_Viewport"));
		UVerticalBox* Shell = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_ResponsiveShell"));
		USizeBox* HeaderBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Header"));
		UBorder* Header = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Header"));
		UHorizontalBox* HeaderContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* Title = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HeaderTitle"));
		UTextBlock* Tagline = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_HeaderBrand"));
		USizeBox* BrandMarkBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_BrandMark"));
		UImage* BrandMark = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_BrandMark"));
		USizeBox* CloseBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Close"));
		UButton* Close = MakeTextButton(Blueprint, Tree, TEXT("Button_Close"), TEXT("Text_Close"),
			LOCTEXT("CloseGlyph", "×"), *Palette);

		UHorizontalBox* Main = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_MainContent"));
		USizeBox* Rail = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_CategoryRail"));
		UScrollBox* NavigationScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Navigation"));
		UVerticalBox* Categories = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Categories"), true);
		UBorder* Body = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_SettingsPanel"));
		UVerticalBox* BodyContent = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_SettingsContent"));
		UTextBlock* CategoryTitle = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CategoryTitle"), true);
		UTextBlock* Intro = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_Intro"), true);
		USizeBox* RuleBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_ContentRule"));
		UBorder* Rule = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ContentRule"));
		UScrollBox* RowsScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Rows"));
		UVerticalBox* Rows = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Rows"), true);

		USizeBox* FooterBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Footer"));
		UBorder* Footer = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Footer"));
		UHorizontalBox* FooterContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Footer"));
		USizeBox* SaveBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_SaveAction"));
		UButton* Save = MakeTextButton(Blueprint, Tree, TEXT("Button_Save"), TEXT("Text_Save"),
			LOCTEXT("Save", "저장"), *Palette, true);
		USizeBox* BackBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_BackAction"));
		UButton* Back = MakeTextButton(Blueprint, Tree, TEXT("Button_Back"), TEXT("Text_Back"),
			LOCTEXT("Back", "이전으로"), *Palette);
		UVerticalBox* FooterStatus = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_FooterStatus"));
		UTextBlock* Summary = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_CurrentSummary"), true);
		UTextBlock* KeyHint = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_KeyHint"), true);

		if (!Root || !Surface || !SafeZone || !Shell || !HeaderBox || !Header || !HeaderContent
			|| !Title || !Tagline || !BrandMarkBox || !BrandMark || !CloseBox || !Close || !Main || !Rail || !NavigationScroll || !Categories
			|| !Body || !BodyContent || !CategoryTitle || !Intro || !RuleBox || !Rule || !RowsScroll || !Rows
			|| !FooterBox || !Footer || !FooterContent || !SaveBox || !Save || !BackBox || !Back
			|| !FooterStatus || !Summary || !KeyHint) return false;

		Tree.RootWidget = Root;
		Surface->SetBrush(MakeSolidBrush(T.Background));
		Surface->SetVisibility(ESlateVisibility::Visible);
		StretchOnCanvas(*Root, *Surface);
		SafeZone->AddChild(Shell);
		StretchOnCanvas(*Root, *SafeZone);

		HeaderBox->SetHeightOverride(FResponsiveMetrics::HeaderHeight);
		Header->SetBrush(MakeSolidBrush(T.Paper));
		Header->SetPadding(FMargin(FResponsiveMetrics::HorizontalInset, 0.f));
		Header->AddChild(HeaderContent);
		HeaderBox->AddChild(Header);
		Shell->AddChildToVerticalBox(HeaderBox);
		StyleText(*Title, 32, T.Ink, TEXT("Bold"));
		Title->SetText(LOCTEXT("Title", "옵션"));
		ConfigureSingleLineText(*Title, Title->GetText());
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(Title))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		StyleText(*Tagline, 16, T.Muted, TEXT("Medium"));
		Tagline->SetText(LOCTEXT("Tagline", "나에게 맞는 축제 여행을 준비하세요"));
		ConfigureSingleLineText(*Tagline, Tagline->GetText(), ETextJustify::Right);
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(Tagline))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(16.f, 0.f, 12.f, 0.f));
		}
		// Reuse the reference options screen's flower without modifying the shared texture.
		UTexture2D* BrandTexture = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/FestaFiesta/UI/Title/CleanCasual/Brand/T_FF_Title_CasualBright_Logo_512.T_FF_Title_CasualBright_Logo_512"));
		if (!BrandTexture) return false;
		BrandMark->SetBrushFromTexture(BrandTexture);
		BrandMark->SetOpacity(0.55f);
		BrandMark->SetVisibility(ESlateVisibility::HitTestInvisible);
		BrandMarkBox->SetWidthOverride(70.f);
		BrandMarkBox->SetHeightOverride(70.f);
		BrandMarkBox->AddChild(BrandMark);
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(BrandMarkBox))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		}
		Close->SetToolTipText(LOCTEXT("CloseTooltip", "이전 화면으로 돌아가기 (ESC / F5)"));
		FButtonStyle CloseStyle = Close->GetStyle();
		CloseStyle.NormalPadding = FMargin(4.f);
		CloseStyle.PressedPadding = FMargin(4.f, 5.f, 4.f, 3.f);
		Close->SetStyle(CloseStyle);
		CloseBox->SetWidthOverride(44.f);
		CloseBox->SetHeightOverride(44.f);
		CloseBox->AddChild(Close);
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(CloseBox))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		// A persistent sidebar keeps category selection separate from the scrollable settings.
		if (UVerticalBoxSlot* Slot = Shell->AddChildToVerticalBox(Main))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(FResponsiveMetrics::HorizontalInset, 24.f,
				FResponsiveMetrics::HorizontalInset, 0.f));
		}
		Rail->SetWidthOverride(FResponsiveMetrics::NavigationWidth);
		NavigationScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		NavigationScroll->SetAllowOverscroll(false);
		NavigationScroll->SetAnimateWheelScrolling(false);
		NavigationScroll->SetWheelScrollMultiplier(54.f);
		NavigationScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
		NavigationScroll->AddChild(Categories);
		Rail->AddChild(NavigationScroll);
		if (UHorizontalBoxSlot* Slot = Main->AddChildToHorizontalBox(Rail))
		{
			Slot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
		}

		struct FCategoryDefinition { FName ButtonName; FName TextName; FText Label; const TCHAR* Icon; };
		const FCategoryDefinition Definitions[] = {
			{ TEXT("Button_Language"), TEXT("Text_LanguageCategory"), LOCTEXT("Language", "언어"), TEXT("Language") },
			{ TEXT("Button_Accessibility"), TEXT("Text_AccessibilityCategory"), LOCTEXT("Accessibility", "접근성"), TEXT("Accessibility") },
			{ TEXT("Button_Sound"), TEXT("Text_SoundCategory"), LOCTEXT("Sound", "사운드"), TEXT("Sound") },
			{ TEXT("Button_Graphics"), TEXT("Text_GraphicsCategory"), LOCTEXT("Graphics", "그래픽"), TEXT("Graphics") },
			{ TEXT("Button_Keyboard"), TEXT("Text_KeyboardCategory"), LOCTEXT("Keyboard", "조작"), TEXT("Controls") },
			{ TEXT("Button_Credits"), TEXT("Text_CreditsCategory"), LOCTEXT("Credits", "크레딧"), TEXT("Info") },
			{ TEXT("Button_Basic"), TEXT("Text_BasicCategory"), LOCTEXT("Basic", "기본설정"), TEXT("Spark") },
			{ TEXT("Button_Account"), TEXT("Text_AccountCategory"), LOCTEXT("Account", "계정정보"), TEXT("Folder") },
			{ TEXT("Button_ExitGame"), TEXT("Text_ExitGameCategory"), LOCTEXT("Exit", "게임종료"), TEXT("Back") }
		};
		for (const FCategoryDefinition& Definition : Definitions)
		{
			UButton* Button = MakeTextButton(Blueprint, Tree, Definition.ButtonName, Definition.TextName,
				Definition.Label, *Palette);
			USizeBox* ButtonBox = MakeWidget<USizeBox>(Blueprint, Tree,
				FName(*(Definition.ButtonName.ToString() + TEXT("_Size"))));
			UHorizontalBox* ButtonContent = MakeWidget<UHorizontalBox>(Blueprint, Tree,
				FName(*(Definition.ButtonName.ToString() + TEXT("_Content"))));
			UImage* Icon = MakeWidget<UImage>(Blueprint, Tree,
				FName(*(Definition.ButtonName.ToString() + TEXT("_Icon"))));
			USizeBox* IconBox = MakeWidget<USizeBox>(Blueprint, Tree,
				FName(*(Definition.ButtonName.ToString() + TEXT("_IconSize"))));
			UTextBlock* Chevron = MakeWidget<UTextBlock>(Blueprint, Tree,
				FName(*(Definition.ButtonName.ToString() + TEXT("_Chevron"))));
			if (!Button || !ButtonBox || !ButtonContent || !Icon || !IconBox || !Chevron) return false;
			UTextBlock* Label = Cast<UTextBlock>(Button->GetContent());
			if (!Label) return false;
			Button->ClearChildren();
			FButtonStyle CategoryStyle = Button->GetStyle();
			CategoryStyle.NormalPadding = FMargin(16.f, 6.f);
			CategoryStyle.PressedPadding = FMargin(16.f, 7.f, 16.f, 5.f);
			Button->SetStyle(CategoryStyle);
			// Reuse the existing line icons without changing their shared texture assets.
			Icon->SetColorAndOpacity(T.Ink);
			const FString IconPackage = FString(TEXT("/Game/FestaFiesta/UI/Frontend/Art/T_FF_")) + Definition.Icon;
			if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *MakeObjectPath(IconPackage)))
			{
				Icon->SetBrushFromTexture(Texture);
			}
			else
			{
				Icon->SetVisibility(ESlateVisibility::Hidden);
			}
			IconBox->SetWidthOverride(24.f);
			IconBox->SetHeightOverride(24.f);
			IconBox->AddChild(Icon);
			if (UHorizontalBoxSlot* Slot = ButtonContent->AddChildToHorizontalBox(IconBox))
			{
				Slot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
				Slot->SetVerticalAlignment(VAlign_Center);
			}
			ConfigureSingleLineText(*Label, Definition.Label);
			if (UHorizontalBoxSlot* Slot = ButtonContent->AddChildToHorizontalBox(Label))
			{
				Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				Slot->SetVerticalAlignment(VAlign_Center);
			}
			StyleText(*Chevron, 22, T.GreenDeep, TEXT("Medium"));
			Chevron->SetText(FText::FromString(TEXT("›")));
			if (UHorizontalBoxSlot* Slot = ButtonContent->AddChildToHorizontalBox(Chevron))
			{
				Slot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
				Slot->SetVerticalAlignment(VAlign_Center);
			}
			if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->AddChild(ButtonContent)))
			{
				Slot->SetPadding(FMargin(0.f));
				Slot->SetHorizontalAlignment(HAlign_Fill);
				Slot->SetVerticalAlignment(VAlign_Center);
			}
			ButtonBox->SetHeightOverride(FResponsiveMetrics::CategoryButtonHeight);
			ButtonBox->AddChild(Button);
			if (UVerticalBoxSlot* Slot = Categories->AddChildToVerticalBox(ButtonBox))
			{
				Slot->SetPadding(FMargin(4.f, 3.f, 4.f, 3.f));
			}
		}

		Body->SetBrush(FFFestivalOptionsStyle::Rounded(T.Paper, T.Line, 22.f));
		Body->SetPadding(FMargin(FResponsiveMetrics::PanelInset));
		Body->AddChild(BodyContent);
		if (UHorizontalBoxSlot* Slot = Main->AddChildToHorizontalBox(Body))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		StyleText(*CategoryTitle, 28, T.Ink, TEXT("Bold"));
		CategoryTitle->SetText(LOCTEXT("CategoryPreview", "기본설정"));
		ConfigureSingleLineText(*CategoryTitle, CategoryTitle->GetText());
		BodyContent->AddChildToVerticalBox(CategoryTitle);
		StyleText(*Intro, 15, T.Muted, TEXT("Medium"));
		Intro->SetText(LOCTEXT("Intro", "변경 사항은 저장을 눌러야 적용됩니다."));
		ConfigureWrappedText(*Intro);
		if (UVerticalBoxSlot* Slot = BodyContent->AddChildToVerticalBox(Intro))
		{
			Slot->SetPadding(FMargin(0.f, 5.f, 0.f, 16.f));
		}
		Rule->SetBrush(MakeSolidBrush(T.LineStrong));
		RuleBox->SetHeightOverride(1.f);
		RuleBox->AddChild(Rule);
		BodyContent->AddChildToVerticalBox(RuleBox);
		RowsScroll->SetAnimateWheelScrolling(false);
		RowsScroll->SetWheelScrollMultiplier(64.f);
		RowsScroll->SetAllowOverscroll(false);
		RowsScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		RowsScroll->SetScrollbarThickness(FVector2D(6.f, 6.f));
		RowsScroll->SetClipping(EWidgetClipping::ClipToBounds);
		RowsScroll->AddChild(Rows);
		if (UVerticalBoxSlot* Slot = BodyContent->AddChildToVerticalBox(RowsScroll))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		}

		// Actions never scroll away. Dynamic confirmations remain readable beside the buttons.
		FooterBox->SetHeightOverride(FResponsiveMetrics::FooterHeight);
		Footer->SetBrush(MakeSolidBrush(T.Background));
		Footer->SetPadding(FMargin(FResponsiveMetrics::HorizontalInset, 16.f));
		Footer->AddChild(FooterContent);
		FooterBox->AddChild(Footer);
		Shell->AddChildToVerticalBox(FooterBox);
		// Let confirmation labels grow naturally instead of wrapping inside a fixed-width action.
		CastChecked<UTextBlock>(Save->GetContent())->SetAutoWrapText(false);
		CastChecked<UTextBlock>(Back->GetContent())->SetAutoWrapText(false);
		SaveBox->SetMinDesiredWidth(184.f);
		SaveBox->SetHeightOverride(FResponsiveMetrics::ActionButtonHeight);
		SaveBox->AddChild(Save);
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(SaveBox))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(4.f, 0.f, 16.f, 0.f));
		}
		BackBox->SetMinDesiredWidth(208.f);
		BackBox->SetHeightOverride(FResponsiveMetrics::ActionButtonHeight);
		BackBox->AddChild(Back);
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(BackBox))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		StyleText(*Summary, 15, T.Ink, TEXT("SemiBold"));
		Summary->SetText(LOCTEXT("Summary", "저장된 설정  ·  전체 화면  ·  1920 × 1080"));
		ConfigureWrappedText(*Summary, ETextJustify::Right);
		FooterStatus->AddChildToVerticalBox(Summary);
		StyleText(*KeyHint, 14, T.Muted, TEXT("Medium"));
		KeyHint->SetText(LOCTEXT("KeyHint", "ESC · F5  이전으로"));
		ConfigureSingleLineText(*KeyHint, KeyHint->GetText(), ETextJustify::Right);
		if (UVerticalBoxSlot* Slot = FooterStatus->AddChildToVerticalBox(KeyHint))
		{
			Slot->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
		}
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(FooterStatus))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(32.f, 0.f, 4.f, 0.f));
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
			&& Has(TEXT("HorizontalBox_MainContent"), *UHorizontalBox::StaticClass())
			&& Has(TEXT("SizeBox_CategoryRail"), *USizeBox::StaticClass())
			&& Has(TEXT("VerticalBox_Categories"), *UVerticalBox::StaticClass())
			&& Has(TEXT("ScrollBox_Rows"), *UScrollBox::StaticClass())
			&& Has(TEXT("VerticalBox_Rows"), *UVerticalBox::StaticClass())
			&& Has(TEXT("Text_CategoryTitle"), *UTextBlock::StaticClass())
			&& Has(TEXT("Text_Intro"), *UTextBlock::StaticClass())
			&& Has(TEXT("Text_CurrentSummary"), *UTextBlock::StaticClass())
			&& Has(TEXT("HorizontalBox_Footer"), *UHorizontalBox::StaticClass())
			&& Has(TEXT("Button_Basic"), *UButton::StaticClass())
			&& Has(TEXT("Button_Language"), *UButton::StaticClass())
			&& Has(TEXT("Button_Accessibility"), *UButton::StaticClass())
			&& Has(TEXT("Button_Sound"), *UButton::StaticClass())
			&& Has(TEXT("Button_Credits"), *UButton::StaticClass())
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
