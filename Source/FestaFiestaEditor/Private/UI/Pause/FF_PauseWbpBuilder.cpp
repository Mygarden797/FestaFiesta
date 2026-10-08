// 에디터 전용 모듈: 이 제작 도구의 변경은 게임 실행 모듈과 별도로 컴파일된다.
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
#include "Engine/Texture2D.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UI/Settings/FF_FestivalOptionsStyle.h"
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
		static constexpr float FooterHeight = 72.f;
		static constexpr float MenuWidth = 320.f;
		static constexpr float MenuItemHeight = 80.f;
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
		const auto T = FFFestivalOptionsStyle::Get();
		const FLinearColor Danger = FFFestivalOptionsStyle::Hex(TEXT("A7463E"));
		FButtonStyle Style;
		Style.Normal = FFFestivalOptionsStyle::Rounded(bPrimary ? T.Green : T.Paper,
			bPrimary ? T.Green : T.LineStrong, 14.f);
		Style.Hovered = FFFestivalOptionsStyle::Rounded(bPrimary ? T.GreenDeep :
			(bDanger ? FFFestivalOptionsStyle::Hex(TEXT("FBEDEA")) : T.Hover),
			bDanger ? Danger : T.Green, 14.f, 2.f);
		Style.Pressed = FFFestivalOptionsStyle::Rounded(bPrimary ? T.GreenDeep : T.Selected,
			bDanger ? Danger : T.GreenDeep, 14.f, 2.f);
		Style.Disabled = FFFestivalOptionsStyle::Rounded(T.Disabled, T.Line, 14.f);
		Style.NormalPadding = FMargin(18.f, 10.f);
		Style.PressedPadding = FMargin(18.f, 11.f, 18.f, 9.f);
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
		FSlateBrush Brush = MakeSolidBrush(FLinearColor::Transparent, 14.f);
		Brush.OutlineSettings.Color = FSlateColor(Color);
		Brush.OutlineSettings.Width = 3.f;
		Ring->SetBrush(Brush);
		Ring->SetVisibility(ESlateVisibility::Collapsed);
		return Ring;
	}

	USizeBox* MakeMenuItem(UWidgetBlueprint& Blueprint, UWidgetTree& Tree,
		const FName ButtonName, const FName FocusName, const FName LabelName,
		const FText& Label, const FText& Description, const UFF_UIPalette& Palette,
		const bool bPrimary = false, const bool bDanger = false)
	{
		const auto T = FFFestivalOptionsStyle::Get();
		const FLinearColor Ink = bPrimary ? T.White :
			(bDanger ? FFFestivalOptionsStyle::Hex(TEXT("A7463E")) : T.Ink);
		auto Name = [&](const TCHAR* Suffix) { return FName(*(ButtonName.ToString() + Suffix)); };
		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, Name(TEXT("_Size")));
		UOverlay* Overlay = MakeWidget<UOverlay>(Blueprint, Tree, Name(TEXT("_Overlay")));
		UButton* Button = MakeWidget<UButton>(Blueprint, Tree, ButtonName, true);
		UHorizontalBox* Content = MakeWidget<UHorizontalBox>(Blueprint, Tree, Name(TEXT("_Content")));
		UVerticalBox* Copy = MakeWidget<UVerticalBox>(Blueprint, Tree, Name(TEXT("_Copy")));
		UTextBlock* Title = MakeText(Blueprint, Tree, LabelName, Label, 19, Ink, TEXT("SemiBold"), true);
		UTextBlock* Detail = MakeText(Blueprint, Tree,
			FName(*(LabelName.ToString() + TEXT("_Description"))), Description, 14,
			bPrimary ? T.White : T.Muted, TEXT("Medium"));
		UTextBlock* Chevron = MakeText(Blueprint, Tree, Name(TEXT("_Chevron")),
			FText::FromString(TEXT("›")), 22, Ink, TEXT("Medium"));
		UImage* Ring = MakeFocusRing(Blueprint, Tree, FocusName, bPrimary ? T.White : T.Green);
		if (!Root || !Overlay || !Button || !Content || !Copy || !Title || !Detail || !Chevron || !Ring) return nullptr;
		Root->SetMinDesiredHeight(FPauseLayoutMetrics::MenuItemHeight);
		Root->AddChild(Overlay);
		Button->SetStyle(MakeButtonStyle(Palette, bPrimary, bDanger));
		ConfigureWrappedText(*Title);
		ConfigureWrappedText(*Detail);
		Button->SetToolTipText(Description);
		Copy->AddChildToVerticalBox(Title);
		Copy->AddChildToVerticalBox(Detail)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(Copy))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(Chevron))
		{
			Slot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Button->AddChild(Content)))
		{
			Slot->SetPadding(FMargin(0.f));
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
			Slot->SetPadding(FMargin(3.f));
		}
		return Root;
	}

	UBorder* MakeStatCard(UWidgetBlueprint& Blueprint, UWidgetTree& Tree,
		const FName BorderName, const FName ValueName, const FText& Label,
		const FText& PreviewValue, const UFF_UIPalette& Palette)
	{
		const auto T = FFFestivalOptionsStyle::Get();
		UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, BorderName);
		UVerticalBox* Copy = MakeWidget<UVerticalBox>(Blueprint, Tree,
			FName(*(BorderName.ToString() + TEXT("_Copy"))));
		UTextBlock* Caption = MakeText(Blueprint, Tree,
			FName(*(BorderName.ToString() + TEXT("_Label"))), Label, 14, T.Muted, TEXT("Medium"));
		UTextBlock* Value = MakeText(Blueprint, Tree, ValueName, PreviewValue, 20, T.Ink, TEXT("SemiBold"), true);
		if (!Card || !Copy || !Caption || !Value) return nullptr;
		Card->SetBrush(FFFestivalOptionsStyle::Rounded(T.White, T.Line, 14.f));
		Card->SetPadding(FMargin(16.f, 14.f));
		ConfigureWrappedText(*Caption);
		ConfigureWrappedText(*Value);
		Copy->AddChildToVerticalBox(Caption);
		Copy->AddChildToVerticalBox(Value)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		Card->AddChild(Copy);
		return Card;
	}

	bool BuildPauseHub(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		if (!Palette) return false;
		const auto T = FFFestivalOptionsStyle::Get();
		UCanvasPanel* Root = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Root"));
		UBorder* Surface = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ViewportSurface"));
		USafeZone* SafeZone = MakeWidget<USafeZone>(Blueprint, Tree, TEXT("SafeZone_Viewport"));
		UCanvasPanel* Canvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("CanvasPanel_Responsive"));
		UVerticalBox* Shell = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_ResponsiveShell"));
		USizeBox* HeaderBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Header"));
		UBorder* Header = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Header"));
		UHorizontalBox* HeaderContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Header"));
		UTextBlock* HeaderTitle = MakeText(Blueprint, Tree, TEXT("Text_HeaderTitle"),
			LOCTEXT("PauseTitle", "일시정지"), 32, T.Ink, TEXT("Bold"));
		UTextBlock* Brand = MakeText(Blueprint, Tree, TEXT("Text_Brand"),
			LOCTEXT("Brand", "FESTA FIESTA!"), 18, T.Muted, TEXT("SemiBold"));
		USizeBox* BrandMarkBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_BrandMark"));
		UImage* BrandMark = MakeWidget<UImage>(Blueprint, Tree, TEXT("Image_BrandMark"));
		USizeBox* CloseBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Close"));
		UButton* Close = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseText = MakeText(Blueprint, Tree, TEXT("Text_Close"),
			LOCTEXT("Close", "×"), 20, T.Ink, TEXT("SemiBold"));

		UHorizontalBox* Main = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_MainContent"));
		USizeBox* Rail = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_MenuRail"));
		UBorder* MenuPanel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_MenuPanel"));
		UScrollBox* MenuScroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Menu"));
		UVerticalBox* Menu = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Menu"));
		UTextBlock* MenuTitle = MakeText(Blueprint, Tree, TEXT("Text_MenuTitle"),
			LOCTEXT("MenuTitle", "축제 운영 메뉴"), 16, T.Muted, TEXT("SemiBold"));
		USizeBox* Resume = MakeMenuItem(Blueprint, Tree, TEXT("Button_Resume"), TEXT("Image_FocusResume"), TEXT("Text_Resume"),
			LOCTEXT("Resume", "계속하기"), LOCTEXT("ResumeDescription", "축제 운영으로 돌아가기"), *Palette, true);
		USizeBox* Settings = MakeMenuItem(Blueprint, Tree, TEXT("Button_Settings"), TEXT("Image_FocusSettings"), TEXT("Text_Settings"),
			LOCTEXT("SystemSettings", "시스템 설정"), LOCTEXT("SystemSettingsDescription", "소리 · 화면 · 편의 설정"), *Palette);
		USizeBox* ReturnToAtlas = MakeMenuItem(Blueprint, Tree, TEXT("Button_ReturnToAtlas"), TEXT("Image_FocusReturnToAtlas"), TEXT("Text_ReturnToAtlas"),
			LOCTEXT("ReturnToAtlas", "축제 선택으로"), LOCTEXT("ReturnToAtlasDescription", "현재 플레이를 마치고 축제 선택"), *Palette);
		USizeBox* Exit = MakeMenuItem(Blueprint, Tree, TEXT("Button_Exit"), TEXT("Image_FocusExit"), TEXT("Text_Exit"),
			LOCTEXT("Exit", "게임 종료"), LOCTEXT("ExitDescription", "종료 전 한 번 더 확인"), *Palette, false, true);

		UBorder* ContextPanel = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ContextPanel"));
		UScrollBox* Scroll = MakeWidget<UScrollBox>(Blueprint, Tree, TEXT("ScrollBox_Context"));
		UVerticalBox* Context = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_Context"));
		UBorder* FestivalCard = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_CurrentFestival"));
		UVerticalBox* FestivalCopy = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_FestivalCopy"));
		UHorizontalBox* Meta = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_FestivalMeta"));
		UTextBlock* FestivalEyebrow = MakeText(Blueprint, Tree, TEXT("Text_FestivalEyebrow"),
			LOCTEXT("FestivalEyebrow", "진행 중인 축제"), 14, T.Muted, TEXT("Medium"));
		UTextBlock* FestivalName = MakeText(Blueprint, Tree, TEXT("Text_FestivalName"),
			LOCTEXT("FestivalPreview", "진행 중인 축제"), 28, T.Ink, TEXT("Bold"), true);
		UTextBlock* DayStage = MakeText(Blueprint, Tree, TEXT("Text_DayStage"),
			LOCTEXT("DayStagePreview", "DAY 1 · 개장 준비"), 16, T.GreenDeep, TEXT("SemiBold"), true);
		UTextBlock* Clock = MakeText(Blueprint, Tree, TEXT("Text_Clock"),
			LOCTEXT("ClockPreview", "현재 단계가 일시정지되었습니다"), 14, T.Muted, TEXT("Medium"), true);
		UHorizontalBox* Stats = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Stats"));
		UBorder* Gold = MakeStatCard(Blueprint, Tree, TEXT("Border_GoldCard"), TEXT("Text_Gold"),
			LOCTEXT("GoldLabel", "보유 자금"), FText::FromString(TEXT("—")), *Palette);
		UBorder* Visitors = MakeStatCard(Blueprint, Tree, TEXT("Border_VisitorsCard"), TEXT("Text_Visitors"),
			LOCTEXT("VisitorsLabel", "현재 방문객"), FText::FromString(TEXT("—")), *Palette);
		UBorder* Congestion = MakeStatCard(Blueprint, Tree, TEXT("Border_CongestionCard"), TEXT("Text_Congestion"),
			LOCTEXT("CongestionLabel", "현장 혼잡"), LOCTEXT("CongestionPreview", "정보 준비 중"), *Palette);

		UBorder* Selection = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_SelectionCard"));
		UVerticalBox* SelectionCopy = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_SelectionCopy"));
		UTextBlock* SelectionEyebrow = MakeText(Blueprint, Tree, TEXT("Text_SelectionEyebrow"),
			LOCTEXT("SelectionEyebrow", "축제 운영"), 14, T.Green, TEXT("SemiBold"), true);
		UTextBlock* SelectionTitle = MakeText(Blueprint, Tree, TEXT("Text_SelectionTitle"),
			LOCTEXT("SelectionTitle", "계속하기"), 20, T.Ink, TEXT("SemiBold"), true);
		UTextBlock* SelectionDescription = MakeText(Blueprint, Tree, TEXT("Text_SelectionDescription"),
			LOCTEXT("SelectionDescription", "메뉴를 닫고 진행 중인 축제로 돌아갑니다."), 15, T.Muted, TEXT("Medium"), true);
		UBorder* Guide = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_GuideCard"));
		UVerticalBox* GuideCopy = MakeWidget<UVerticalBox>(Blueprint, Tree, TEXT("VerticalBox_GuideCopy"));
		UTextBlock* GuideTitle = MakeText(Blueprint, Tree, TEXT("Text_GuideTitle"),
			LOCTEXT("GuideTitle", "빠른 조작"), 16, T.Ink, TEXT("SemiBold"));
		UTextBlock* GuideBody = MakeText(Blueprint, Tree, TEXT("Text_GuideBody"),
			LOCTEXT("GuideBody", "↑ ↓  메뉴 이동    Enter  선택\nESC · F5  계속 플레이"), 14, T.Muted, TEXT("Medium"), true);

		USizeBox* FooterBox = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("SizeBox_Footer"));
		UBorder* Footer = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Footer"));
		UHorizontalBox* FooterContent = MakeWidget<UHorizontalBox>(Blueprint, Tree, TEXT("HorizontalBox_Footer"));
		UTextBlock* KeyHint = MakeText(Blueprint, Tree, TEXT("Text_KeyHint"),
			LOCTEXT("KeyHint", "ESC · F5  계속 플레이"), 15, T.Ink, TEXT("SemiBold"), true);
		UTextBlock* PausedState = MakeText(Blueprint, Tree, TEXT("Text_PausedState"),
			LOCTEXT("PausedState", "●  게임 일시정지"), 15, T.GreenDeep, TEXT("SemiBold"));
		UTextBlock* SelectHint = MakeText(Blueprint, Tree, TEXT("Text_SelectHint"),
			LOCTEXT("SelectHint", "Enter  선택"), 15, T.Muted, TEXT("Medium"));

		if (!Root || !Surface || !SafeZone || !Canvas || !Shell || !HeaderBox || !Header || !HeaderContent
			|| !HeaderTitle || !Brand || !BrandMarkBox || !BrandMark || !CloseBox || !Close || !CloseText || !Main || !Rail
			|| !MenuPanel || !MenuScroll || !Menu || !MenuTitle || !Resume || !Settings || !ReturnToAtlas
			|| !Exit || !ContextPanel || !Scroll || !Context || !FestivalCard
			|| !FestivalCopy || !Meta || !FestivalEyebrow || !FestivalName || !DayStage || !Clock || !Stats
			|| !Gold || !Visitors || !Congestion || !Selection || !SelectionCopy || !SelectionEyebrow
			|| !SelectionTitle || !SelectionDescription || !Guide || !GuideCopy || !GuideTitle || !GuideBody
			|| !FooterBox || !Footer || !FooterContent || !KeyHint || !PausedState || !SelectHint) return false;

		Tree.RootWidget = Root;
		Surface->SetBrush(MakeSolidBrush(T.Background));
		StretchOnCanvas(*Root, *Surface);
		SafeZone->AddChild(Canvas);
		StretchOnCanvas(*Root, *SafeZone);
		StretchOnCanvas(*Canvas, *Shell);
		HeaderBox->SetHeightOverride(FPauseLayoutMetrics::HeaderHeight);
		Header->SetBrush(MakeSolidBrush(T.Paper));
		Header->SetPadding(FMargin(32.f, 0.f));
		Header->AddChild(HeaderContent);
		HeaderBox->AddChild(Header);
		Shell->AddChildToVerticalBox(HeaderBox);
		UTexture2D* BrandTexture = LoadObject<UTexture2D>(nullptr,
			TEXT("/Game/FestaFiesta/UI/Title/CleanCasual/Brand/T_FF_Title_CasualBright_Logo_512.T_FF_Title_CasualBright_Logo_512"));
		if (!BrandTexture) return false;
		BrandMark->SetBrushFromTexture(BrandTexture);
		BrandMark->SetOpacity(0.55f);
		BrandMark->SetVisibility(ESlateVisibility::HitTestInvisible);
		BrandMarkBox->SetWidthOverride(70.f);
		BrandMarkBox->SetHeightOverride(70.f);
		BrandMarkBox->AddChild(BrandMark);
		for (UTextBlock* Text : { HeaderTitle, Brand })
		{
			ConfigureSingleLineText(*Text, Text->GetText());
			UHorizontalBoxSlot* Slot = HeaderContent->AddChildToHorizontalBox(Text);
			Slot->SetSize(FSlateChildSize(Text == HeaderTitle ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(0.f, 0.f, Text == Brand ? 12.f : 24.f, 0.f));
			if (Text == Brand)
			{
				UHorizontalBoxSlot* MarkSlot = HeaderContent->AddChildToHorizontalBox(BrandMarkBox);
				MarkSlot->SetVerticalAlignment(VAlign_Center);
				MarkSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
			}
		}
		CloseBox->SetWidthOverride(44.f);
		CloseBox->SetHeightOverride(44.f);
		FButtonStyle CloseStyle = MakeButtonStyle(*Palette);
		CloseStyle.NormalPadding = FMargin(4.f);
		CloseStyle.PressedPadding = FMargin(4.f);
		Close->SetStyle(CloseStyle);
		Close->SetToolTipText(LOCTEXT("CloseTooltip", "메뉴를 닫고 계속 플레이"));
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Close->AddChild(CloseText)))
		{
			Slot->SetPadding(FMargin(0.f));
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
		CloseBox->AddChild(Close);
		HeaderContent->AddChildToHorizontalBox(CloseBox)->SetVerticalAlignment(VAlign_Center);

		UVerticalBoxSlot* MainSlot = Shell->AddChildToVerticalBox(Main);
		MainSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		MainSlot->SetPadding(FMargin(32.f, 24.f, 32.f, 0.f));
		Rail->SetWidthOverride(FPauseLayoutMetrics::MenuWidth);
		Rail->AddChild(MenuPanel);
		MenuPanel->SetBrush(MakeSolidBrush(FLinearColor::Transparent));
		MenuPanel->SetPadding(FMargin(0.f));
		MenuPanel->AddChild(MenuScroll);
		MenuScroll->SetAllowOverscroll(false);
		MenuScroll->SetAnimateWheelScrolling(false);
		MenuScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll);
		MenuScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
		MenuScroll->AddChild(Menu);
		Main->AddChildToHorizontalBox(Rail)->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
		Menu->AddChildToVerticalBox(MenuTitle)->SetPadding(FMargin(4.f, 0.f, 0.f, 12.f));
		const TArray<USizeBox*> MenuItems = { Resume, Settings, ReturnToAtlas, Exit };
		for (int32 Index = 0; Index < MenuItems.Num(); ++Index)
		{
			Menu->AddChildToVerticalBox(MenuItems[Index])->SetPadding(FMargin(3.f, 0.f, 3.f, 0.f));
			if (Index == MenuItems.Num() - 1) continue;

			USizeBox* DividerBox = MakeWidget<USizeBox>(Blueprint, Tree,
				FName(*FString::Printf(TEXT("SizeBox_MenuDivider_%d"), Index)));
			UBorder* Divider = MakeWidget<UBorder>(Blueprint, Tree,
				FName(*FString::Printf(TEXT("Border_MenuDivider_%d"), Index)));
			if (!DividerBox || !Divider) return false;
			Divider->SetBrush(MakeSolidBrush(T.Line));
			DividerBox->SetHeightOverride(1.f);
			DividerBox->AddChild(Divider);
			Menu->AddChildToVerticalBox(DividerBox)->SetPadding(FMargin(4.f, 12.f, 4.f, 12.f));
		}

		ContextPanel->SetBrush(FFFestivalOptionsStyle::Rounded(T.Paper, T.Line, 22.f));
		ContextPanel->SetPadding(FMargin(24.f));
		ContextPanel->AddChild(Scroll);
		Main->AddChildToHorizontalBox(ContextPanel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Scroll->SetAllowOverscroll(false);
		Scroll->SetAnimateWheelScrolling(false);
		Scroll->SetWheelScrollMultiplier(64.f);
		Scroll->SetScrollbarThickness(FVector2D(6.f));
		Scroll->AddChild(Context);
		FestivalCard->SetBrush(FFFestivalOptionsStyle::Rounded(T.White, T.Line, 16.f));
		FestivalCard->SetPadding(FMargin(20.f, 16.f));
		FestivalCard->AddChild(FestivalCopy);
		for (UTextBlock* Text : { FestivalEyebrow, FestivalName, DayStage, Clock, SelectionEyebrow,
			SelectionTitle, SelectionDescription, GuideTitle, GuideBody }) ConfigureWrappedText(*Text);
		FestivalCopy->AddChildToVerticalBox(FestivalEyebrow);
		FestivalCopy->AddChildToVerticalBox(FestivalName)->SetPadding(FMargin(0.f, 6.f, 0.f, 8.f));
		FestivalCopy->AddChildToVerticalBox(Meta);
		Meta->AddChildToHorizontalBox(DayStage)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Clock->SetJustification(ETextJustify::Right);
		Meta->AddChildToHorizontalBox(Clock)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Context->AddChildToVerticalBox(FestivalCard)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		for (UBorder* Card : { Gold, Visitors, Congestion })
		{
			UHorizontalBoxSlot* Slot = Stats->AddChildToHorizontalBox(Card);
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(0.f, 0.f, Card == Congestion ? 0.f : 12.f, 0.f));
		}
		Context->AddChildToVerticalBox(Stats)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		Selection->SetBrush(FFFestivalOptionsStyle::Rounded(T.White, T.Line, 16.f));
		Selection->SetPadding(FMargin(20.f, 16.f));
		Selection->AddChild(SelectionCopy);
		SelectionCopy->AddChildToVerticalBox(SelectionEyebrow);
		SelectionCopy->AddChildToVerticalBox(SelectionTitle)->SetPadding(FMargin(0.f, 5.f, 0.f, 6.f));
		SelectionCopy->AddChildToVerticalBox(SelectionDescription);
		Context->AddChildToVerticalBox(Selection)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
		Guide->SetBrush(FFFestivalOptionsStyle::Rounded(T.Background, T.Line, 14.f));
		Guide->SetPadding(FMargin(20.f, 12.f));
		Guide->AddChild(GuideCopy);
		GuideCopy->AddChildToVerticalBox(GuideTitle);
		GuideCopy->AddChildToVerticalBox(GuideBody)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		Context->AddChildToVerticalBox(Guide);

		FooterBox->SetHeightOverride(FPauseLayoutMetrics::FooterHeight);
		Footer->SetBrush(MakeSolidBrush(T.Background));
		Footer->SetPadding(FMargin(36.f, 0.f));
		Footer->AddChild(FooterContent);
		FooterBox->AddChild(Footer);
		Shell->AddChildToVerticalBox(FooterBox);
		for (UTextBlock* Text : { KeyHint, PausedState, SelectHint })
		{
			ConfigureSingleLineText(*Text, Text->GetText(), Text == KeyHint ? ETextJustify::Left :
				(Text == SelectHint ? ETextJustify::Right : ETextJustify::Center));
			UHorizontalBoxSlot* Slot = FooterContent->AddChildToHorizontalBox(Text);
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
			&& HasWidget(TEXT("VerticalBox_ResponsiveShell"), *UVerticalBox::StaticClass())
			&& HasWidget(TEXT("VerticalBox_Context"), *UVerticalBox::StaticClass())
			&& HasWidget(TEXT("Button_Resume"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_Close"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_Settings"), *UButton::StaticClass())
			&& HasWidget(TEXT("Button_ReturnToAtlas"), *UButton::StaticClass())
			&& HasWidget(TEXT("Image_FocusReturnToAtlas"), *UImage::StaticClass())
			&& HasWidget(TEXT("Text_ReturnToAtlas"), *UTextBlock::StaticClass())
			&& HasWidget(TEXT("Image_BrandMark"), *UImage::StaticClass())
			&& !Blueprint.WidgetTree->FindWidget(TEXT("Button_Keyboard"))
			&& !Blueprint.WidgetTree->FindWidget(TEXT("Button_Account"))
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
