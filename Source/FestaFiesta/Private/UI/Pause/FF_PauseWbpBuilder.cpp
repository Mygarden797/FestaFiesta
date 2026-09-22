// FF_PauseWbpBuilder.cpp
// 역할: 일시정지 메뉴의 편집 가능한 화면 틀을 만든다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if WITH_EDITOR

// ESC/F5 1단계 일시정지 허브의 비파괴 WBP 스캐폴더. 콘솔 명령은 `FF.PauseUMG`다.

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
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
#include "UI/Pause/FF_PauseHubScreen.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPauseWbpBuilder"

namespace FFPauseWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* const PauseLogPrefix = TEXT("FF_PAUSE_UMG");
	const FString PauseFolder(TEXT("/Game/FestaFiesta/UI/Pause"));
	const FString PauseHubPackage(PauseFolder / TEXT("WBP_FF_PauseHub"));

	// Viewport-owned responsive layout.  Only heights and internal spacing are fixed;
	// width and body height always come from the live viewport/SafeZone.
	struct FPauseLayoutMetrics
	{
		static constexpr float HeaderHeight = 88.f;
		static constexpr float FooterHeight = 84.f;
		static constexpr float MenuWidthWeight = 0.34f;
		static constexpr float MenuItemHeight = 76.f;
		static constexpr float FestivalCardHeight = 168.f;
		static constexpr float StatsHeight = 78.f;
		static constexpr float GuideHeight = 126.f;
		// Keeps the fixed context-card rhythm valid; shorter viewports scroll this authored area.
		static constexpr float ContextMinContentHeight = 620.f;
		static constexpr float PanelRadius = 8.f;
		static constexpr float ControlRadius = 6.f;
	};

	FSlateBrush MakeSolidBrush(const FLinearColor& Color, const float Radius = 0.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		return Brush;
	}

	void PlaceResponsiveOnCanvas(
		UCanvasPanel& Canvas,
		UWidget& Widget,
		const FAnchors& Anchors,
		const FMargin& Offsets)
	{
		if (UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(&Widget))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetOffsets(Offsets);
			Slot->SetAlignment(FVector2D::ZeroVector);
			Slot->SetAutoSize(false);
		}
	}

	FSlateBrush MakePanelBrush(const FLinearColor& Fill, const FLinearColor& Outline,
		const float Radius = 6.f, const float OutlineWidth = 1.f)
	{
		FSlateBrush Brush = MakeSolidBrush(Fill, Radius);
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

	FButtonStyle MakeButtonStyle(
		const UFF_UIPalette& Palette, const bool bPrimary = false, const bool bDanger = false)
	{
		const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(Palette);
		FButtonStyle Style;
		Style.Normal = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			bDanger ? FFPauseOptionsLightTeal::RectDangerPath
				: (bPrimary ? FFPauseOptionsLightTeal::RectPrimaryPath
					: FFPauseOptionsLightTeal::RectNavDefaultPath),
			bDanger ? Tokens.Danger : (bPrimary ? Tokens.Teal : Tokens.PaperSunken),
			bDanger ? Tokens.Danger : (bPrimary ? Tokens.TealDeep : Tokens.Line));
		Style.Hovered = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			bDanger ? FFPauseOptionsLightTeal::RectDangerPath
				: (bPrimary ? FFPauseOptionsLightTeal::RectPrimaryHoverPath
					: FFPauseOptionsLightTeal::RectNavHoverPath),
			bDanger ? Tokens.Danger : (bPrimary ? Tokens.TealDeep : Tokens.Hover),
			Tokens.TealDeep);
		Style.Pressed = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			bDanger ? FFPauseOptionsLightTeal::RectDangerPath
				: FFPauseOptionsLightTeal::RectNavPressedPath,
			bDanger ? Tokens.Danger : Tokens.TealDeep, Tokens.TealDeep);
		Style.Disabled = FFPauseOptionsLightTeal::MakeRectButtonBrush(
			FFPauseOptionsLightTeal::RectNavDisabledPath, Tokens.Disabled, Tokens.Line);
		Style.NormalPadding = FMargin(20.f, 12.f);
		Style.PressedPadding = FMargin(20.f, 13.f, 20.f, 11.f);
		return Style;
	}

	UTextBlock* MakeText(UWidgetBlueprint& Blueprint, UWidgetTree& Tree, const FName Name,
		const FText& Text, const int32 Size, const FLinearColor& Color, const FName Typeface,
		const bool bVariable = false)
	{
		UTextBlock* Block = MakeWidget<UTextBlock>(Blueprint, Tree, Name, bVariable);
		if (Block)
		{
			StyleText(*Block, Size, Color, Typeface);
			Block->SetText(Text);
			Block->SetClipping(EWidgetClipping::ClipToBounds);
		}
		return Block;
	}

	UImage* MakeFocusRing(UWidgetBlueprint& Blueprint, UWidgetTree& Tree,
		const FName Name, const FLinearColor& Color)
	{
		UImage* Ring = MakeWidget<UImage>(Blueprint, Tree, Name, true);
		if (!Ring)
		{
			return nullptr;
		}
		FSlateBrush Brush = MakeSolidBrush(FLinearColor::Transparent, 6.f);
		Brush.OutlineSettings.Color = FSlateColor(Color);
		Brush.OutlineSettings.Width = 2.f;
		Ring->SetBrush(Brush);
		Ring->SetVisibility(ESlateVisibility::Collapsed);
		return Ring;
	}

	USizeBox* MakeMenuItem(UWidgetBlueprint& Blueprint, UWidgetTree& Tree,
		const FName ButtonName, const FName FocusName, const FName LabelName,
		const FText& Label, const FText& Description, const UFF_UIPalette& Palette,
		const bool bPrimary = false, const bool bDanger = false)
	{
		const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(Palette);
		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree,
			FName(*(ButtonName.ToString() + TEXT("_Size"))));
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree,
			FName(*(ButtonName.ToString() + TEXT("_Overlay"))));
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, true);
		UVerticalBox* Copy = MakeWidget<UVerticalBox>(Blueprint, Tree,
			FName(*(ButtonName.ToString() + TEXT("_Copy"))));
		UTextBlock* Title = MakeText(Blueprint, Tree, LabelName, Label, 20,
			(bPrimary || bDanger) ? Tokens.White : Tokens.Ink,
			TEXT("SemiBold"), true);
		UTextBlock* Detail = MakeText(Blueprint, Tree,
			FName(*(LabelName.ToString() + TEXT("_Description"))), Description, 13,
			(bPrimary || bDanger) ? Tokens.White : Tokens.InkMuted, TEXT("Medium"));
		UImage* Ring = MakeFocusRing(Blueprint, Tree, FocusName, Tokens.TealDeep);
		if (!Root || !Overlay || !Button || !Copy || !Title || !Detail || !Ring)
		{
			return nullptr;
		}

		Root->SetMinDesiredHeight(FPauseLayoutMetrics::MenuItemHeight);
		Root->SetClipping(EWidgetClipping::ClipToBounds);
		Root->AddChild(Overlay);
		Overlay->SetClipping(EWidgetClipping::ClipToBounds);
		Button->SetStyle(MakeButtonStyle(Palette, bPrimary, bDanger));
		Button->SetClipping(EWidgetClipping::ClipToBounds);
		Copy->SetClipping(EWidgetClipping::ClipToBounds);
		ConfigureWrappedText(*Title);
		Title->SetToolTipText(Label);
		ConfigureWrappedText(*Detail);
		if (UVerticalBoxSlot* Slot = Copy->AddChildToVerticalBox(Title))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		if (UVerticalBoxSlot* Slot = Copy->AddChildToVerticalBox(Detail))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
		}
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->AddChild(Copy)))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(Button))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		if (UOverlaySlot* Slot = Overlay->AddChildToOverlay(Ring))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		return Root;
	}

	UBorder* MakeStatCard(UWidgetBlueprint& Blueprint, UWidgetTree& Tree,
		const FName BorderName, const FName ValueName, const FText& Label,
		const FText& PreviewValue, const UFF_UIPalette& Palette)
	{
		const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(Palette);
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, BorderName);
		UHorizontalBox* Copy = MakeWidget<UHorizontalBox>(Blueprint, Tree,
			FName(*(BorderName.ToString() + TEXT("_Copy"))));
		UTextBlock* Caption = MakeText(Blueprint, Tree,
			FName(*(BorderName.ToString() + TEXT("_Label"))), Label, 14,
			Tokens.InkMuted, TEXT("SemiBold"));
		UTextBlock* Value = MakeText(Blueprint, Tree, ValueName, PreviewValue, 27,
			Tokens.Ink, TEXT("SemiBold"), true);
		if (!Card || !Copy || !Caption || !Value)
		{
			return nullptr;
		}
		Card->SetBrush(FFPauseOptionsLightTeal::MakeListRowBrush(Tokens));
		Card->SetClipping(EWidgetClipping::ClipToBounds);
		Card->SetPadding(FMargin(18.f, 0.f));
		Copy->SetClipping(EWidgetClipping::ClipToBounds);
		ConfigureSingleLineText(*Caption, Label);
		ConfigureSingleLineText(*Value, PreviewValue, ETextJustify::Right);
		if (UHorizontalBoxSlot* Slot = Copy->AddChildToHorizontalBox(Caption))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* Slot = Copy->AddChildToHorizontalBox(Value))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		Card->AddChild(Copy);
		return Card;
	}

	bool BuildPauseHub(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette)
		{
			return false;
		}
		const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(*Palette);

		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UBorder* ViewportSurface = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_ViewportSurface"));
		USafeZone* SafeZone = MakeWidget<USafeZone>(Blueprint, Tree, TEXT("SafeZone_Viewport"));
		UCanvasPanel* Design = MakeWidget<UCanvasPanel>(
			Blueprint, Tree, TEXT("CanvasPanel_Responsive"));

		UBorder* Header = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Header"));
		UHorizontalBox* HeaderContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* Brand = MakeText(Blueprint, Tree, TEXT("Text_Brand"),
			LOCTEXT("Brand", "FESTA FIESTA!"), 29, Tokens.White, TEXT("SemiBold"));
		UTextBlock* HeaderTitle = MakeText(Blueprint, Tree, TEXT("Text_HeaderTitle"),
			LOCTEXT("PauseTitle", "일시정지"), 20, Tokens.White, TEXT("SemiBold"));
		UTextBlock* HeaderHint = MakeText(Blueprint, Tree, TEXT("Text_HeaderHint"),
			LOCTEXT("HeaderHint", "ESC · F5  계속 플레이"), 15,
			Tokens.TealSoft, TEXT("Medium"), true);
		UButton* Close = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseText = MakeText(Blueprint, Tree, TEXT("Text_Close"),
			LOCTEXT("Close", "×"), 24, Tokens.Ink, TEXT("SemiBold"));

		UBorder* MenuPanel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_MenuPanel"));
		UScrollBox* MenuScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Menu"));
		UVerticalBox* Menu = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Menu"));
		UTextBlock* MenuEyebrow = MakeText(Blueprint, Tree, TEXT("Text_MenuEyebrow"),
			LOCTEXT("MenuEyebrow", "PAUSE MENU"), 12, Tokens.Teal, TEXT("SemiBold"));
		UTextBlock* MenuTitle = MakeText(Blueprint, Tree, TEXT("Text_MenuTitle"),
			LOCTEXT("MenuTitle", "축제 운영 메뉴"), 28, Tokens.Ink, TEXT("SemiBold"));

		USizeBox* Resume = MakeMenuItem(Blueprint, Tree,
			TEXT("Button_Resume"), TEXT("Image_FocusResume"), TEXT("Text_Resume"),
			LOCTEXT("Resume", "계속하기"),
			LOCTEXT("ResumeDescription", "축제 운영 화면으로 돌아갑니다"), *Palette, true);
		USizeBox* Settings = MakeMenuItem(Blueprint, Tree,
			TEXT("Button_Settings"), TEXT("Image_FocusSettings"), TEXT("Text_Settings"),
			LOCTEXT("SystemSettings", "시스템 설정"),
			LOCTEXT("SystemSettingsDescription", "기본·오디오·접근성·그래픽 설정"), *Palette);
		USizeBox* Keyboard = MakeMenuItem(Blueprint, Tree,
			TEXT("Button_Keyboard"), TEXT("Image_FocusKeyboard"), TEXT("Text_Keyboard"),
			LOCTEXT("Keyboard", "조작 가이드"),
			LOCTEXT("KeyboardDescription", "문맥별 키보드 역할을 확인합니다"), *Palette);
		USizeBox* Account = MakeMenuItem(Blueprint, Tree,
			TEXT("Button_Account"), TEXT("Image_FocusAccount"), TEXT("Text_Account"),
			LOCTEXT("Account", "계정 정보"),
			LOCTEXT("AccountDescription", "로컬 프로필과 빌드 정보를 확인합니다"), *Palette);
		UBorder* MenuDivider = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_MenuDivider"));
		USizeBox* Exit = MakeMenuItem(Blueprint, Tree,
			TEXT("Button_Exit"), TEXT("Image_FocusExit"), TEXT("Text_Exit"),
			LOCTEXT("Exit", "게임 종료"),
			LOCTEXT("ExitDescription", "확인 후 안전하게 게임을 종료합니다"), *Palette, false, true);
		USpacer* MenuSpacer = MakeWidget<USpacer>(Blueprint, Tree, TEXT("Spacer_MenuFlex"));

		UBorder* ContextPanel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ContextPanel"));
		UScrollBox* ContextScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Context"));
		USizeBox* ContextContentBox = MakeWidget<USizeBox>(
			Blueprint, Tree, TEXT("SizeBox_ContextContent"));
		UCanvasPanel* ContextCanvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Context"));
		UBorder* FestivalCard = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_CurrentFestival"));
		UVerticalBox* FestivalCopy = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_FestivalCopy"));
		UTextBlock* FestivalEyebrow = MakeText(Blueprint, Tree, TEXT("Text_FestivalEyebrow"),
			LOCTEXT("FestivalEyebrow", "CURRENT FESTIVAL · PAUSED"), 12, Tokens.TealSoft, TEXT("SemiBold"));
		UTextBlock* FestivalName = MakeText(Blueprint, Tree, TEXT("Text_FestivalName"),
			LOCTEXT("FestivalPreview", "진행 중인 축제"), 31, Tokens.White, TEXT("SemiBold"), true);
		UTextBlock* DayStage = MakeText(Blueprint, Tree, TEXT("Text_DayStage"),
			LOCTEXT("DayStagePreview", "DAY 1  ·  개장 준비"), 17, Tokens.TealSoft, TEXT("Medium"), true);
		UTextBlock* Clock = MakeText(Blueprint, Tree, TEXT("Text_Clock"),
			LOCTEXT("ClockPreview", "현재 단계가 일시정지되었습니다"), 15,
			Tokens.TealSoft, TEXT("Medium"), true);

		UBorder* GoldCard = MakeStatCard(Blueprint, Tree, TEXT("Border_GoldCard"), TEXT("Text_Gold"),
			LOCTEXT("GoldLabel", "보유 자금"), LOCTEXT("GoldPreview", "7,600 Gold"), *Palette);
		UBorder* VisitorsCard = MakeStatCard(Blueprint, Tree, TEXT("Border_VisitorsCard"), TEXT("Text_Visitors"),
			LOCTEXT("VisitorsLabel", "현재 방문객"), LOCTEXT("VisitorsPreview", "1,248명"), *Palette);
		UBorder* CongestionCard = MakeStatCard(Blueprint, Tree, TEXT("Border_CongestionCard"), TEXT("Text_Congestion"),
			LOCTEXT("CongestionLabel", "현장 혼잡"), LOCTEXT("CongestionPreview", "혼잡 없음"), *Palette);

		UBorder* SelectionCard = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_SelectionCard"));
		UVerticalBox* SelectionCopy = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_SelectionCopy"));
		UTextBlock* SelectionEyebrow = MakeText(Blueprint, Tree, TEXT("Text_SelectionEyebrow"),
			LOCTEXT("SelectionEyebrow", "RESUME"), 12, Tokens.Teal, TEXT("SemiBold"), true);
		UTextBlock* SelectionTitle = MakeText(Blueprint, Tree, TEXT("Text_SelectionTitle"),
			LOCTEXT("SelectionTitle", "계속하기"), 22, Tokens.Ink, TEXT("SemiBold"), true);
		UTextBlock* SelectionDescription = MakeText(Blueprint, Tree, TEXT("Text_SelectionDescription"),
			LOCTEXT("SelectionDescription", "일시정지 메뉴를 닫고 축제 운영 화면으로 돌아갑니다."),
			14, Tokens.InkMuted, TEXT("Medium"), true);

		UBorder* GuideCard = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_GuideCard"));
		UVerticalBox* GuideCopy = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_GuideCopy"));
		UTextBlock* GuideEyebrow = MakeText(Blueprint, Tree, TEXT("Text_GuideEyebrow"),
			LOCTEXT("GuideEyebrow", "QUICK GUIDE"), 12, Tokens.Teal, TEXT("SemiBold"));
		UTextBlock* GuideTitle = MakeText(Blueprint, Tree, TEXT("Text_GuideTitle"),
			LOCTEXT("GuideTitle", "빠른 조작"), 20, Tokens.Ink, TEXT("SemiBold"));
		UTextBlock* GuideBody = MakeText(Blueprint, Tree, TEXT("Text_GuideBody"),
			LOCTEXT("GuideBody",
				"↑ ↓  메뉴 이동    Enter  선택\nESC · F5  계속 플레이 / 이전으로"),
			14, Tokens.InkMuted, TEXT("Medium"), true);

		UBorder* Footer = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Footer"));
		UHorizontalBox* FooterContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Footer"));
		UTextBlock* KeyHint = MakeText(Blueprint, Tree, TEXT("Text_KeyHint"),
			LOCTEXT("KeyHint", "ESC · F5  계속 플레이 / 이전으로"), 16,
			Tokens.Ink, TEXT("SemiBold"), true);
		UTextBlock* PausedState = MakeText(Blueprint, Tree, TEXT("Text_PausedState"),
			LOCTEXT("PausedState", "●  게임 일시정지"), 15, Tokens.TealDeep, TEXT("SemiBold"));
		UTextBlock* SelectHint = MakeText(Blueprint, Tree, TEXT("Text_SelectHint"),
			LOCTEXT("SelectHint", "Enter  선택"), 15, Tokens.InkMuted, TEXT("Medium"));

		if (!Root || !ViewportSurface || !SafeZone || !Design
			|| !Header || !HeaderContent || !Brand || !HeaderTitle || !HeaderHint || !Close || !CloseText
			|| !MenuPanel || !MenuScroll || !Menu || !MenuEyebrow || !MenuTitle || !Resume || !Settings || !Keyboard
			|| !Account || !MenuDivider || !Exit || !MenuSpacer || !ContextPanel || !ContextScroll
			|| !ContextContentBox || !ContextCanvas || !FestivalCard
			|| !FestivalCopy || !FestivalEyebrow || !FestivalName || !DayStage || !Clock
			|| !GoldCard || !VisitorsCard || !CongestionCard || !SelectionCard || !SelectionCopy
			|| !SelectionEyebrow || !SelectionTitle || !SelectionDescription || !GuideCard || !GuideCopy
			|| !GuideEyebrow || !GuideTitle || !GuideBody || !Footer || !FooterContent || !KeyHint
			|| !PausedState || !SelectHint)
		{
			return false;
		}

		Tree.RootWidget = Root;
		// This layer owns every viewport pixel, including the area outside platform SafeZone.
		// Use the palette's opaque source color rather than translucent card tokens.
		ViewportSurface->SetBrush(MakeSolidBrush(Palette->InGame.PaperSunken));
		ViewportSurface->SetVisibility(ESlateVisibility::Visible);
		StretchOnCanvas(*Root, *ViewportSurface);

		SafeZone->AddChild(Design);
		StretchOnCanvas(*Root, *SafeZone);

		Header->SetBrush(FFPauseOptionsLightTeal::MakeHeaderBrush(Tokens));
		Header->SetClipping(EWidgetClipping::ClipToBounds);
		Header->SetPadding(FMargin(30.f, 0.f));
		HeaderContent->SetClipping(EWidgetClipping::ClipToBounds);
		Header->AddChild(HeaderContent);
		PlaceResponsiveOnCanvas(*Design, *Header,
			FAnchors(0.f, 0.f, 1.f, 0.f),
			FMargin(0.f, 0.f, 0.f, FPauseLayoutMetrics::HeaderHeight));
		ConfigureSingleLineText(*Brand, Brand->GetText());
		ConfigureSingleLineText(*HeaderTitle, HeaderTitle->GetText(), ETextJustify::Center);
		ConfigureSingleLineText(*HeaderHint, HeaderHint->GetText(), ETextJustify::Right);
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(Brand))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 30.f, 0.f));
		}
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(HeaderTitle))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(HeaderHint))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}
		Close->SetStyle(MakeButtonStyle(*Palette));
		Close->SetClipping(EWidgetClipping::ClipToBounds);
		ConfigureSingleLineText(*CloseText, CloseText->GetText(), ETextJustify::Center);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Close->AddChild(CloseText)))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(Close))
		{
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		MenuPanel->SetBrush(FFPauseOptionsLightTeal::MakeBodyBrush(Tokens));
		MenuPanel->SetClipping(EWidgetClipping::ClipToBounds);
		MenuPanel->SetPadding(FMargin(22.f, 22.f));
		Menu->SetClipping(EWidgetClipping::ClipToBounds);
		MenuScroll->SetAnimateWheelScrolling(true);
		MenuScroll->SetWheelScrollMultiplier(56.f);
		MenuScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		MenuScroll->SetClipping(EWidgetClipping::ClipToBounds);
		if (UScrollBoxSlot* Slot = Cast<UScrollBoxSlot>(MenuScroll->AddChild(Menu)))
		{
			// Fill tall viewports so the spacer still anchors Exit at the bottom; use desired
			// height and scrolling when the menu no longer fits at 720p/600p.
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		MenuPanel->AddChild(MenuScroll);
		PlaceResponsiveOnCanvas(*Design, *MenuPanel,
			FAnchors(0.f, 0.f, FPauseLayoutMetrics::MenuWidthWeight, 1.f),
			FMargin(0.f, FPauseLayoutMetrics::HeaderHeight, 0.f,
				FPauseLayoutMetrics::FooterHeight));
		ConfigureSingleLineText(*MenuEyebrow, MenuEyebrow->GetText());
		ConfigureWrappedText(*MenuTitle);
		MenuTitle->SetToolTipText(MenuTitle->GetText());
		Menu->AddChildToVerticalBox(MenuEyebrow);
		if (UVerticalBoxSlot* Slot = Menu->AddChildToVerticalBox(MenuTitle))
		{
			Slot->SetPadding(FMargin(0.f, 5.f, 0.f, 18.f));
		}
		UWidget* MenuItems[] = { Resume, Settings, Keyboard, Account };
		for (UWidget* Item : MenuItems)
		{
			if (UVerticalBoxSlot* Slot = Menu->AddChildToVerticalBox(Item))
			{
				Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
			}
		}
		if (UVerticalBoxSlot* Slot = Menu->AddChildToVerticalBox(MenuSpacer))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		MenuDivider->SetBrush(MakeSolidBrush(Tokens.Line));
		MenuDivider->SetPadding(FMargin(0.f, 1.f));
		if (UVerticalBoxSlot* Slot = Menu->AddChildToVerticalBox(MenuDivider))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			Slot->SetPadding(FMargin(0.f, 6.f, 0.f, 16.f));
		}
		Menu->AddChildToVerticalBox(Exit);

		ContextPanel->SetBrush(FFPauseOptionsLightTeal::MakeBodyBrush(Tokens));
		ContextPanel->SetClipping(EWidgetClipping::ClipToBounds);
		ContextScroll->SetAnimateWheelScrolling(true);
		ContextScroll->SetWheelScrollMultiplier(56.f);
		ContextScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		ContextScroll->SetClipping(EWidgetClipping::ClipToBounds);
		ContextContentBox->SetMinDesiredHeight(FPauseLayoutMetrics::ContextMinContentHeight);
		ContextContentBox->SetClipping(EWidgetClipping::ClipToBounds);
		ContextCanvas->SetClipping(EWidgetClipping::ClipToBounds);
		ContextContentBox->AddChild(ContextCanvas);
		if (UScrollBoxSlot* Slot = Cast<UScrollBoxSlot>(ContextScroll->AddChild(ContextContentBox)))
		{
			// Grow to the live body at desktop sizes, but never collapse below the card layout's
			// minimum. The ScrollBox exposes the full context on short windows.
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}
		ContextPanel->AddChild(ContextScroll);
		PlaceResponsiveOnCanvas(*Design, *ContextPanel,
			FAnchors(FPauseLayoutMetrics::MenuWidthWeight, 0.f, 1.f, 1.f),
			FMargin(0.f, FPauseLayoutMetrics::HeaderHeight, 0.f,
				FPauseLayoutMetrics::FooterHeight));

		FestivalCard->SetBrush(MakePanelBrush(Tokens.Teal, Tokens.TealDeep,
			FPauseLayoutMetrics::ControlRadius, 1.5f));
		FestivalCard->SetClipping(EWidgetClipping::ClipToBounds);
		FestivalCard->SetPadding(FMargin(24.f, 20.f));
		FestivalCopy->SetClipping(EWidgetClipping::ClipToBounds);
		ConfigureSingleLineText(*FestivalEyebrow, FestivalEyebrow->GetText());
		ConfigureSingleLineText(*FestivalName, FestivalName->GetText());
		ConfigureSingleLineText(*DayStage, DayStage->GetText());
		ConfigureSingleLineText(*Clock, Clock->GetText());
		FestivalCopy->AddChildToVerticalBox(FestivalEyebrow);
		if (UVerticalBoxSlot* Slot = FestivalCopy->AddChildToVerticalBox(FestivalName))
		{
			Slot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* Slot = FestivalCopy->AddChildToVerticalBox(DayStage))
		{
			Slot->SetPadding(FMargin(0.f, 9.f, 0.f, 0.f));
		}
		if (UVerticalBoxSlot* Slot = FestivalCopy->AddChildToVerticalBox(Clock))
		{
			Slot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		}
		FestivalCard->AddChild(FestivalCopy);
		PlaceResponsiveOnCanvas(*ContextCanvas, *FestivalCard,
			FAnchors(0.f, 0.f, 1.f, 0.f),
			FMargin(24.f, 24.f, 24.f, FPauseLayoutMetrics::FestivalCardHeight));
		PlaceResponsiveOnCanvas(*ContextCanvas, *GoldCard,
			FAnchors(0.f, 0.f, 1.f / 3.f, 0.f),
			FMargin(24.f, 204.f, 6.f, FPauseLayoutMetrics::StatsHeight));
		PlaceResponsiveOnCanvas(*ContextCanvas, *VisitorsCard,
			FAnchors(1.f / 3.f, 0.f, 2.f / 3.f, 0.f),
			FMargin(6.f, 204.f, 6.f, FPauseLayoutMetrics::StatsHeight));
		PlaceResponsiveOnCanvas(*ContextCanvas, *CongestionCard,
			FAnchors(2.f / 3.f, 0.f, 1.f, 0.f),
			FMargin(6.f, 204.f, 24.f, FPauseLayoutMetrics::StatsHeight));

		SelectionCard->SetBrush(FFPauseOptionsLightTeal::MakeInsetBrush(Tokens));
		SelectionCard->SetClipping(EWidgetClipping::ClipToBounds);
		SelectionCard->SetPadding(FMargin(20.f, 14.f));
		SelectionCopy->SetClipping(EWidgetClipping::ClipToBounds);
		ConfigureSingleLineText(*SelectionEyebrow, SelectionEyebrow->GetText());
		ConfigureWrappedText(*SelectionTitle);
		SelectionTitle->SetToolTipText(SelectionTitle->GetText());
		SelectionCopy->AddChildToVerticalBox(SelectionEyebrow);
		if (UVerticalBoxSlot* Slot = SelectionCopy->AddChildToVerticalBox(SelectionTitle))
		{
			Slot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		}
		ConfigureWrappedText(*SelectionDescription);
		if (UVerticalBoxSlot* Slot = SelectionCopy->AddChildToVerticalBox(SelectionDescription))
		{
			Slot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}
		SelectionCard->AddChild(SelectionCopy);
		PlaceResponsiveOnCanvas(*ContextCanvas, *SelectionCard,
			FAnchors(0.f, 0.f, 1.f, 1.f),
			FMargin(24.f, 296.f, 24.f, 158.f));

		GuideCard->SetBrush(FFPauseOptionsLightTeal::MakeInsetBrush(Tokens));
		GuideCard->SetClipping(EWidgetClipping::ClipToBounds);
		GuideCard->SetPadding(FMargin(20.f, 16.f));
		GuideCopy->SetClipping(EWidgetClipping::ClipToBounds);
		ConfigureSingleLineText(*GuideEyebrow, GuideEyebrow->GetText());
		ConfigureSingleLineText(*GuideTitle, GuideTitle->GetText());
		GuideCopy->AddChildToVerticalBox(GuideEyebrow);
		if (UVerticalBoxSlot* Slot = GuideCopy->AddChildToVerticalBox(GuideTitle))
		{
			Slot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		}
		ConfigureWrappedText(*GuideBody);
		if (UVerticalBoxSlot* Slot = GuideCopy->AddChildToVerticalBox(GuideBody))
		{
			Slot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}
		GuideCard->AddChild(GuideCopy);
		PlaceResponsiveOnCanvas(*ContextCanvas, *GuideCard,
			FAnchors(0.f, 1.f, 1.f, 1.f),
			FMargin(24.f, -142.f, 24.f, FPauseLayoutMetrics::GuideHeight));

		Footer->SetBrush(FFPauseOptionsLightTeal::MakeFooterBrush(Tokens));
		Footer->SetClipping(EWidgetClipping::ClipToBounds);
		Footer->SetPadding(FMargin(26.f, 0.f));
		FooterContent->SetClipping(EWidgetClipping::ClipToBounds);
		Footer->AddChild(FooterContent);
		PlaceResponsiveOnCanvas(*Design, *Footer,
			FAnchors(0.f, 1.f, 1.f, 1.f),
			FMargin(0.f, -FPauseLayoutMetrics::FooterHeight, 0.f,
				FPauseLayoutMetrics::FooterHeight));
		ConfigureSingleLineText(*KeyHint, KeyHint->GetText());
		ConfigureSingleLineText(*PausedState, PausedState->GetText(), ETextJustify::Center);
		ConfigureSingleLineText(*SelectHint, SelectHint->GetText(), ETextJustify::Right);
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(KeyHint))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(PausedState))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, 34.f, 0.f));
		}
		if (UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(SelectHint))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		return true;
	}

	bool HasPauseHubContract(const UWidgetBlueprint& Blueprint)
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
			&& HasWidget(TEXT("Border_ViewportSurface"), *UBorder::StaticClass())
			&& HasWidget(TEXT("SafeZone_Viewport"), *USafeZone::StaticClass())
			&& HasWidget(TEXT("CanvasPanel_Responsive"), *UCanvasPanel::StaticClass())
			&& HasWidget(TEXT("ScrollBox_Menu"), *UScrollBox::StaticClass())
			&& HasWidget(TEXT("ScrollBox_Context"), *UScrollBox::StaticClass())
			&& HasWidget(TEXT("SizeBox_ContextContent"), *USizeBox::StaticClass())
			&& HasWidget(TEXT("Button_Resume"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_Close"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_Settings"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_Keyboard"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_Account"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_Exit"), *UButton::StaticClass())
			&& HasWidget(TEXT("Text_FestivalName"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_Gold"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_Visitors"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_Congestion"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_SelectionEyebrow"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Text_SelectionDescription"), *UTextBlock::StaticClass())
			&& !Blueprint.WidgetTree->FindWidget(TEXT("ScaleBox_Viewport"))
			&& !Blueprint.WidgetTree->FindWidget(TEXT("SizeBox_Design1920x1080"))
			&& !Blueprint.WidgetTree->FindWidget(TEXT("BackgroundBlur_World"))
			&& !Blueprint.WidgetTree->FindWidget(TEXT("Border_Scrim"));
	}

	bool RefreshPauseHubBlueprint()
	{
		if (!FPackageName::DoesPackageExist(PauseHubPackage))
		{
			return EnsureWidgetBlueprint(PauseLogPrefix, PauseHubPackage, PauseFolder,
				*UFF_PauseHubScreen::StaticClass(),
				[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
				{
					return BuildPauseHub(Blueprint, Tree);
				}) != nullptr;
		}

		UWidgetBlueprint* Blueprint = LoadExistingBlueprint(PauseHubPackage);
		if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->WidgetTree
			|| !Blueprint->GeneratedClass->IsChildOf(UFF_PauseHubScreen::StaticClass())
			|| !Blueprint->WidgetTree->RootWidget
			|| Blueprint->WidgetTree->RootWidget->GetFName() != FName(TEXT("CanvasPanel_Root")))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REFRESH_SKIP: 소유권 또는 부모 계약이 다른 WBP는 덮어쓰지 않습니다."),
				PauseLogPrefix);
			return false;
		}

		UWidgetTree* OriginalTree = Blueprint->WidgetTree;
		const FName OriginalTreeName = OriginalTree->GetFName();
		const bool bOriginalTreeWasTransient = OriginalTree->HasAnyFlags(RF_Transient);
		const auto OriginalBindings = Blueprint->Bindings;
		const auto OriginalVariableMap = Blueprint->WidgetVariableNameToGuidMap;
		const FName DiscardedName = MakeUniqueObjectName(
			Blueprint, UWidgetTree::StaticClass(), TEXT("DiscardedPauseWidgetTree"));
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
					Blueprint, UWidgetTree::StaticClass(), TEXT("FailedPauseWidgetTree"));
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

		if (!Blueprint->WidgetTree || !BuildPauseHub(*Blueprint, *Blueprint->WidgetTree)
			|| !HasPauseHubContract(*Blueprint) || !SaveNewBlueprint(PauseLogPrefix, *Blueprint))
		{
			Rollback();
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_REFRESH_FAIL: 기존 Pause WidgetTree를 복구했습니다."), PauseLogPrefix);
			return false;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_REFRESHED: LightTeal 전면 Pause Hub 저장 완료."), PauseLogPrefix);
		return true;
	}

	void RunPauseUmg()
	{
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_BEGIN: 없는 일시정지 허브 WBP만 생성한다."), PauseLogPrefix);
		EnsureWidgetBlueprint(PauseLogPrefix, PauseHubPackage, PauseFolder,
			*UFF_PauseHubScreen::StaticClass(),
			[](UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
			{
				return BuildPauseHub(Blueprint, Tree);
			});
		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_END: 생성 완료. 레이아웃 마감은 WBP에서 비파괴 조정한다."), PauseLogPrefix);
	}

	void RefreshPauseUmg()
	{
		RefreshPauseHubBlueprint();
	}

	FAutoConsoleCommand PauseUmgCommand(
		TEXT("FF.PauseUMG"),
		TEXT("ESC/F5 LightTeal 일시정지 허브 WBP를 만든다. 기존 디자인은 덮어쓰지 않는다."),
		FConsoleCommandDelegate::CreateStatic(&RunPauseUmg));

	FAutoConsoleCommand PauseUmgRefreshCommand(
		TEXT("FF.PauseUMG.Refresh"),
		TEXT("builder-owned Pause Hub WBP를 명시적으로 LightTeal 레이아웃으로 갱신한다."),
		FConsoleCommandDelegate::CreateStatic(&RefreshPauseUmg));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
