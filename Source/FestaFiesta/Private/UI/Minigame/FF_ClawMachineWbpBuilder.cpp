// Copyright Epic Games, Inc. All Rights Reserved.

// One-shot, non-destructive UMG authoring command for the claw-machine screen.
// Once created, WBP_FF_ClawMachine belongs to the UI designer: rerunning the
// command validates/reuses the package and never rewrites its widget tree.

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Viewport.h"
#include "HAL/IConsoleManager.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"
#include "UI/Minigame/FF_ClawMachineJoystickWidget.h"
#include "UI/Minigame/FF_ClawMachineScreen.h"
#include "WidgetBlueprint.h"

#define LOCTEXT_NAMESPACE "FFClawMachineWbpBuilder"

namespace FFClawMachineWbpBuilder
{
	using namespace FFUIWbpBuilder;

	const TCHAR* const LogPrefix = TEXT("FF_CLAW_MACHINE_UMG");
	const FString ScreenFolder(TEXT("/Game/FestaFiesta/UI/Minigame"));
	const FString ScreenPackage(ScreenFolder / TEXT("WBP_FF_ClawMachine"));
	const FString JoystickPackage(ScreenFolder / TEXT("WBP_FF_ClawMachineJoystick"));
	void StyleHudText(UTextBlock& Text, int32 FontSize, const FLinearColor& Color, FName Typeface);

	FSlateBrush MakeJoystickBrush(
		const FLinearColor& Fill,
		const float Radius,
		const FLinearColor& Outline = FLinearColor::Transparent,
		const float OutlineWidth = 0.0f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			Radius, FSlateColor(Outline), OutlineWidth);
		return Brush;
	}

	FButtonStyle MakeJoystickDropStyle()
	{
		FButtonStyle Style;
		Style.Normal = MakeJoystickBrush(FLinearColor(0.96f, 0.15f, 0.12f, 1.0f), 35.0f);
		Style.Hovered = MakeJoystickBrush(FLinearColor(1.0f, 0.28f, 0.18f, 1.0f), 35.0f);
		Style.Pressed = MakeJoystickBrush(FLinearColor(0.72f, 0.055f, 0.045f, 1.0f), 35.0f);
		Style.Disabled = MakeJoystickBrush(FLinearColor(0.28f, 0.18f, 0.15f, 0.72f), 35.0f);
		Style.NormalPadding = FMargin(2.0f);
		Style.PressedPadding = FMargin(2.0f, 5.0f, 2.0f, 0.0f);
		return Style;
	}

	bool IsVariableWidget(const UWidgetBlueprint& Blueprint, const UWidget* Widget)
	{
		return Widget
			&& Widget->bIsVariable
			&& Blueprint.WidgetVariableNameToGuidMap.Contains(Widget->GetFName());
	}

	bool HasRequiredWidgetTree(const UWidgetBlueprint& Blueprint)
	{
		const UWidgetTree* Tree = Blueprint.WidgetTree;
		if (!Tree || !Tree->RootWidget)
		{
			return false;
		}

		const UViewport* Viewport = Cast<UViewport>(Tree->FindWidget(TEXT("Viewport_ClawMachine3D")));
		const UTextBlock* Time = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_RemainingTime")));
		const UTextBlock* Score = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Score")));
		const UTextBlock* Attempts = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Attempts")));
		const UTextBlock* Status = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Status")));
		const UTextBlock* Feedback = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Feedback")));
		const UProgressBar* Phase = Cast<UProgressBar>(Tree->FindWidget(TEXT("Progress_Phase")));
		const UButton* Drop = Cast<UButton>(Tree->FindWidget(TEXT("Button_Drop")));
		const UButton* Close = Cast<UButton>(Tree->FindWidget(TEXT("Button_Close")));
		const UBorder* Result = Cast<UBorder>(Tree->FindWidget(TEXT("Panel_Result")));
		const UTextBlock* ResultText = Cast<UTextBlock>(Tree->FindWidget(TEXT("Text_Result")));

		return IsVariableWidget(Blueprint, Viewport)
			&& IsVariableWidget(Blueprint, Time)
			&& IsVariableWidget(Blueprint, Score)
			&& IsVariableWidget(Blueprint, Attempts)
			&& IsVariableWidget(Blueprint, Status)
			&& IsVariableWidget(Blueprint, Feedback)
			&& IsVariableWidget(Blueprint, Phase)
			&& IsVariableWidget(Blueprint, Drop)
			&& IsVariableWidget(Blueprint, Close)
			&& IsVariableWidget(Blueprint, Result)
			&& IsVariableWidget(Blueprint, ResultText);
	}

	bool HasJoystickWidgetTree(const UWidgetBlueprint& Blueprint)
	{
		const UWidgetTree* Tree = Blueprint.WidgetTree;
		if (!Tree || !Tree->RootWidget)
		{
			return false;
		}
		const UCanvasPanel* Canvas = Cast<UCanvasPanel>(Tree->FindWidget(TEXT("Canvas_Joystick")));
		const UBorder* Knob = Cast<UBorder>(Tree->FindWidget(TEXT("Panel_JoystickKnob")));
		const UButton* Drop = Cast<UButton>(Tree->FindWidget(TEXT("Button_JoystickDrop")));
		return IsVariableWidget(Blueprint, Canvas)
			&& IsVariableWidget(Blueprint, Knob)
			&& IsVariableWidget(Blueprint, Drop);
	}

	bool BuildJoystickLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const FFF_ClawMachineVisualStyle& Style =
			GetDefault<UFF_ClawMachineScreen>()->VisualStyle;
		USizeBox* Root = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Size_JoystickRoot"));
		UBorder* Plate = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Panel_JoystickPlate"), true);
		UCanvasPanel* Canvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Canvas_Joystick"), true);
		UBorder* Track = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Panel_JoystickTrack"), true);
		UBorder* Knob = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Panel_JoystickKnob"), true);
		UButton* DropButton = MakeWidget<UButton>(Blueprint, Tree, TEXT("Button_JoystickDrop"), true);
		UTextBlock* MoveLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_JoystickMove"), true);
		UTextBlock* DropLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_JoystickDrop"), true);
		UTextBlock* KeyLabel = MakeWidget<UTextBlock>(Blueprint, Tree, TEXT("Text_JoystickKeys"), true);
		if (!Root || !Plate || !Canvas || !Track || !Knob || !DropButton
			|| !MoveLabel || !DropLabel || !KeyLabel)
		{
			return false;
		}

		Tree.RootWidget = Root;
		Root->SetWidthOverride(260.0f);
		Root->SetHeightOverride(136.0f);
		Plate->SetBrush(MakeJoystickBrush(
			FLinearColor(0.92f, 0.56f, 0.10f, 0.98f),
			12.0f,
			FLinearColor(1.0f, 0.78f, 0.24f, 0.92f),
			2.0f));
		Plate->SetBrushColor(FLinearColor::White);
		Plate->SetPadding(FMargin(4.0f));
		Track->SetBrush(MakeJoystickBrush(
			FLinearColor(0.012f, 0.035f, 0.045f, 1.0f),
			40.0f,
			FLinearColor(0.03f, 0.18f, 0.20f, 1.0f),
			3.0f));
		Track->SetBrushColor(FLinearColor::White);
		Knob->SetBrush(MakeJoystickBrush(
			Style.CoralColor,
			19.0f,
			FLinearColor(1.0f, 0.42f, 0.28f, 1.0f),
			2.0f));
		Knob->SetBrushColor(FLinearColor::White);
		DropButton->SetStyle(MakeJoystickDropStyle());
		MoveLabel->SetText(LOCTEXT("JoystickMove", "MOVE"));
		DropLabel->SetText(LOCTEXT("JoystickDrop", "DROP"));
		KeyLabel->SetText(LOCTEXT("JoystickKeys", "SPACE / A"));
		StyleHudText(*MoveLabel, 13, FLinearColor(0.02f, 0.08f, 0.09f, 1.0f), TEXT("Bold"));
		StyleHudText(*DropLabel, 14, FLinearColor::White, TEXT("Bold"));
		StyleHudText(*KeyLabel, 10, FLinearColor(0.02f, 0.08f, 0.09f, 1.0f), TEXT("SemiBold"));
		MoveLabel->SetJustification(ETextJustify::Center);
		DropLabel->SetJustification(ETextJustify::Center);
		KeyLabel->SetJustification(ETextJustify::Center);
		MoveLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		DropLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		KeyLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		DropButton->SetContent(DropLabel);

		auto PlaceFixed = [Canvas](
			UWidget& Widget,
			const FVector2D Position,
			const FVector2D Size,
			const int32 ZOrder) -> bool
		{
			if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(&Widget))
			{
				Slot->SetAnchors(FAnchors(0.0f));
				Slot->SetAlignment(FVector2D::ZeroVector);
				Slot->SetPosition(Position);
				Slot->SetSize(Size);
				Slot->SetZOrder(ZOrder);
				return true;
			}
			return false;
		};
		if (!PlaceFixed(*Track, FVector2D(20.0f, 36.0f), FVector2D(80.0f, 80.0f), 1)
			|| !PlaceFixed(*Knob, FVector2D(41.0f, 57.0f), FVector2D(38.0f, 38.0f), 3)
			|| !PlaceFixed(*DropButton, FVector2D(158.0f, 28.0f), FVector2D(70.0f, 70.0f), 3)
			|| !PlaceFixed(*MoveLabel, FVector2D(18.0f, 10.0f), FVector2D(84.0f, 18.0f), 4)
			|| !PlaceFixed(*KeyLabel, FVector2D(137.0f, 105.0f), FVector2D(112.0f, 18.0f), 4))
		{
			return false;
		}
		Plate->SetContent(Canvas);
		Root->SetContent(Plate);
		return HasJoystickWidgetTree(Blueprint);
	}

	void StyleHudText(
		UTextBlock& Text,
		const int32 FontSize,
		const FLinearColor& Color,
		const FName Typeface = TEXT("Bold"))
	{
		StyleText(Text, FontSize, Color, Typeface);
		Text.SetShadowOffset(FVector2D(1.0f, 2.0f));
		Text.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f));
	}

	bool BuildScreenLayout(UWidgetBlueprint& Blueprint, UWidgetTree& Tree)
	{
		const FFF_ClawMachineVisualStyle& Style =
			GetDefault<UFF_ClawMachineScreen>()->VisualStyle;
		const FLinearColor BackgroundColor(0.003f, 0.012f, 0.020f, 1.0f);
		const FLinearColor FrameColor(0.055f, 0.58f, 0.57f, 1.0f);
		const FLinearColor DeckColor(0.009f, 0.035f, 0.055f, 0.985f);
		const FLinearColor CardColor(0.020f, 0.090f, 0.112f, 0.98f);
		const FLinearColor CardMutedColor(0.014f, 0.052f, 0.072f, 0.98f);

		UOverlay* Root = MakeWidget<UOverlay>(Blueprint, Tree, TEXT("Overlay_ClawMachineRoot"));
		UBorder* Background = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_Background"), true);
		UCanvasPanel* Canvas = MakeWidget<UCanvasPanel>(Blueprint, Tree, TEXT("Canvas_ClawMachineHUD"));
		if (!Root || !Background || !Canvas)
		{
			return false;
		}
		Tree.RootWidget = Root;
		Background->SetBrushColor(BackgroundColor);
		Background->SetVisibility(ESlateVisibility::HitTestInvisible);

		auto StretchOnRoot = [Root](UWidget& Widget) -> bool
		{
			if (UOverlaySlot* OverlaySlot = Root->AddChildToOverlay(&Widget))
			{
				OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
				OverlaySlot->SetVerticalAlignment(VAlign_Fill);
				return true;
			}
			return false;
		};
		auto PlaceAnchored = [Canvas](
			UWidget& Widget,
			const FAnchors& Anchors,
			const int32 ZOrder) -> bool
		{
			if (UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(&Widget))
			{
				CanvasSlot->SetAnchors(Anchors);
				CanvasSlot->SetOffsets(FMargin(0.0f));
				CanvasSlot->SetAlignment(FVector2D::ZeroVector);
				CanvasSlot->SetZOrder(ZOrder);
				return true;
			}
			return false;
		};
		if (!StretchOnRoot(*Background) || !StretchOnRoot(*Canvas))
		{
			return false;
		}

		// Asymmetric arcade layout: the machine owns the visual hierarchy while a
		// dedicated right control rail keeps every changing value easy to scan.
		UBorder* ArenaFrame = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ArenaFrame"), true);
		UBorder* ArenaInset = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Border_ArenaInset"), true);
		UViewport* Viewport = MakeWidget<UViewport>(Blueprint, Tree, TEXT("Viewport_ClawMachine3D"), true);
		if (!ArenaFrame || !ArenaInset || !Viewport)
		{
			return false;
		}
		ArenaFrame->SetBrushColor(FrameColor);
		ArenaFrame->SetPadding(FMargin(2.0f));
		ArenaFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ArenaInset->SetBrushColor(BackgroundColor);
		ArenaInset->SetPadding(FMargin(7.0f));
		ArenaInset->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		ArenaInset->SetContent(Viewport);
		ArenaFrame->SetContent(ArenaInset);
		if (!PlaceAnchored(*ArenaFrame, FAnchors(0.035f, 0.110f, 0.735f, 0.925f), -100))
		{
			return false;
		}

		UBorder* HeaderLine = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_HeaderAccent"), true);
		if (!HeaderLine)
		{
			return false;
		}
		HeaderLine->SetBrushColor(Style.CoralColor);
		HeaderLine->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (!PlaceAnchored(*HeaderLine, FAnchors(0.035f, 0.091f, 0.185f, 0.097f), 18))
		{
			return false;
		}

		// Compact branded header; it never covers the 3D play field.
		UTextBlock* GameTitle = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_GameTitle"), true);
		UTextBlock* Eyebrow = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_GameEyebrow"), true);
		if (!GameTitle || !Eyebrow)
		{
			return false;
		}
		Eyebrow->SetText(LOCTEXT("GameEyebrow", "SEA FRIENDS ARCADE  /  CLAW 01"));
		Eyebrow->SetJustification(ETextJustify::Left);
		Eyebrow->SetVisibility(ESlateVisibility::HitTestInvisible);
		StyleHudText(*Eyebrow, 11, Style.AccentColor, TEXT("SemiBold"));
		GameTitle->SetText(LOCTEXT("GameTitle", "바다 친구 인형뽑기"));
		GameTitle->SetJustification(ETextJustify::Left);
		GameTitle->SetVisibility(ESlateVisibility::HitTestInvisible);
		StyleHudText(*GameTitle, 27, Style.PrimaryTextColor);
		if (!PlaceAnchored(*Eyebrow, FAnchors(0.038f, 0.018f, 0.340f, 0.044f), 20)
			|| !PlaceAnchored(*GameTitle, FAnchors(0.038f, 0.046f, 0.370f, 0.090f), 20))
		{
			return false;
		}

		UBorder* ControlDeck = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Border_ControlDeck"), true);
		if (!ControlDeck)
		{
			return false;
		}
		ControlDeck->SetBrushColor(DeckColor);
		ControlDeck->SetPadding(FMargin(0.0f));
		ControlDeck->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (!PlaceAnchored(*ControlDeck, FAnchors(0.755f, 0.110f, 0.965f, 0.925f), 5))
		{
			return false;
		}

		UTextBlock* RailTitle = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_RailTitle"), true);
		UTextBlock* RailSubtitle = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_RailSubtitle"), true);
		if (!RailTitle || !RailSubtitle)
		{
			return false;
		}
		RailTitle->SetText(LOCTEXT("RailTitle", "MACHINE STATUS"));
		RailTitle->SetJustification(ETextJustify::Left);
		RailTitle->SetVisibility(ESlateVisibility::HitTestInvisible);
		StyleHudText(*RailTitle, 15, Style.GoldColor);
		RailSubtitle->SetText(LOCTEXT("RailSubtitle", "집게 위치와 남은 기회를 확인하세요"));
		RailSubtitle->SetJustification(ETextJustify::Left);
		RailSubtitle->SetAutoWrapText(true);
		RailSubtitle->SetVisibility(ESlateVisibility::HitTestInvisible);
		StyleHudText(*RailSubtitle, 11, Style.SecondaryTextColor, TEXT("Regular"));
		if (!PlaceAnchored(*RailTitle, FAnchors(0.775f, 0.140f, 0.945f, 0.177f), 20)
			|| !PlaceAnchored(*RailSubtitle, FAnchors(0.775f, 0.180f, 0.945f, 0.225f), 20))
		{
			return false;
		}

		auto MakeStatCard = [&Blueprint, &Tree, &PlaceAnchored, &Style, CardColor](
			const FName CardName,
			const FName TextName,
			const FAnchors& Anchors) -> UTextBlock*
		{
			UBorder* Card = MakeWidget<UBorder>(Blueprint, Tree, CardName, true);
			UTextBlock* Text = MakeWidget<UTextBlock>(Blueprint, Tree, TextName, true);
			if (!Card || !Text)
			{
				return nullptr;
			}
			Card->SetBrushColor(CardColor);
			Card->SetPadding(FMargin(15.0f, 8.0f));
			Card->SetHorizontalAlignment(HAlign_Fill);
			Card->SetVerticalAlignment(VAlign_Center);
			Text->SetJustification(ETextJustify::Center);
			Text->SetVisibility(ESlateVisibility::HitTestInvisible);
			StyleHudText(*Text, 17, Style.PrimaryTextColor, TEXT("SemiBold"));
			Card->SetContent(Text);
			if (!PlaceAnchored(*Card, Anchors, 25))
			{
				return nullptr;
			}
			return Text;
		};

		UTextBlock* Time = MakeStatCard(
			TEXT("Card_Time"), TEXT("Text_RemainingTime"), FAnchors(0.775f, 0.245f, 0.945f, 0.315f));
		UTextBlock* Score = MakeStatCard(
			TEXT("Card_Score"), TEXT("Text_Score"), FAnchors(0.775f, 0.330f, 0.945f, 0.400f));
		UTextBlock* Attempts = MakeStatCard(
			TEXT("Card_Attempts"), TEXT("Text_Attempts"), FAnchors(0.775f, 0.415f, 0.945f, 0.485f));
		if (!Attempts || !Time || !Score)
		{
			return false;
		}

		UButton* CloseButton = MakeWidget<UButton>(
			Blueprint, Tree, TEXT("Button_Close"), true);
		UTextBlock* CloseLabel = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_CloseLabel"));
		if (!CloseButton || !CloseLabel)
		{
			return false;
		}
		CloseLabel->SetText(LOCTEXT("Close", "나가기  [ESC]"));
		CloseLabel->SetJustification(ETextJustify::Center);
		StyleHudText(*CloseLabel, 12, Style.SecondaryTextColor, TEXT("SemiBold"));
		CloseButton->SetBackgroundColor(CardMutedColor);
		CloseButton->SetContent(CloseLabel);
		if (!PlaceAnchored(*CloseButton, FAnchors(0.855f, 0.035f, 0.965f, 0.085f), 40))
		{
			return false;
		}

		// Feedback floats over the machine and never blocks interaction.
		UTextBlock* Feedback = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_Feedback"), true);
		if (!Feedback)
		{
			return false;
		}
		Feedback->SetJustification(ETextJustify::Center);
		Feedback->SetVisibility(ESlateVisibility::HitTestInvisible);
		StyleHudText(*Feedback, 30, Style.GoldColor);
		if (!PlaceAnchored(*Feedback, FAnchors(0.175f, 0.150f, 0.595f, 0.230f), 50))
		{
			return false;
		}

		// The live phase and primary action occupy the lower half of the control rail.
		UTextBlock* Status = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_Status"), true);
		UProgressBar* Phase = MakeWidget<UProgressBar>(
			Blueprint, Tree, TEXT("Progress_Phase"), true);
		UButton* DropButton = MakeWidget<UButton>(
			Blueprint, Tree, TEXT("Button_Drop"), true);
		UTextBlock* DropLabel = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_DropLabel"));
		if (!ControlDeck || !Status || !Phase || !DropButton || !DropLabel)
		{
			return false;
		}
		Status->SetJustification(ETextJustify::Center);
		Status->SetVisibility(ESlateVisibility::HitTestInvisible);
		StyleHudText(*Status, 17, Style.AccentColor, TEXT("SemiBold"));
		Phase->SetFillColorAndOpacity(Style.GoldColor);
		Phase->SetVisibility(ESlateVisibility::HitTestInvisible);
		DropLabel->SetText(LOCTEXT("Drop", "집게 내리기\nSPACE / A"));
		DropLabel->SetJustification(ETextJustify::Center);
		StyleHudText(*DropLabel, 18, BackgroundColor);
		DropButton->SetBackgroundColor(Style.GoldColor);
		DropButton->SetContent(DropLabel);
		if (!PlaceAnchored(*Status, FAnchors(0.775f, 0.535f, 0.945f, 0.590f), 20)
			|| !PlaceAnchored(*Phase, FAnchors(0.790f, 0.605f, 0.930f, 0.619f), 20)
			|| !PlaceAnchored(*DropButton, FAnchors(0.775f, 0.695f, 0.945f, 0.815f), 30))
		{
			return false;
		}

		UTextBlock* Hint = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_ControlHint"), true);
		if (!Hint)
		{
			return false;
		}
		Hint->SetText(LOCTEXT(
			"ControlHint",
			"WASD / 방향키 / 왼쪽 스틱  집게 이동     ·     SPACE / A  집게 내리기"));
		Hint->SetJustification(ETextJustify::Left);
		Hint->SetVisibility(ESlateVisibility::HitTestInvisible);
		StyleHudText(*Hint, 12, Style.SecondaryTextColor, TEXT("Regular"));
		if (!PlaceAnchored(*Hint, FAnchors(0.040f, 0.947f, 0.730f, 0.982f), 20))
		{
			return false;
		}

		// Result overlay remains in the same WBP so the designer owns every state.
		UBorder* ResultPanel = MakeWidget<UBorder>(
			Blueprint, Tree, TEXT("Panel_Result"), true);
		USizeBox* ResultSize = MakeWidget<USizeBox>(Blueprint, Tree, TEXT("Size_Result"));
		UBorder* ResultCard = MakeWidget<UBorder>(Blueprint, Tree, TEXT("Card_Result"), true);
		UTextBlock* ResultText = MakeWidget<UTextBlock>(
			Blueprint, Tree, TEXT("Text_Result"), true);
		if (!ResultPanel || !ResultSize || !ResultCard || !ResultText)
		{
			return false;
		}
		ResultPanel->SetBrushColor(FLinearColor(0.0f, 0.006f, 0.012f, 0.92f));
		ResultPanel->SetHorizontalAlignment(HAlign_Center);
		ResultPanel->SetVerticalAlignment(VAlign_Center);
		ResultPanel->SetVisibility(ESlateVisibility::Collapsed);
		ResultSize->SetWidthOverride(650.0f);
		ResultSize->SetHeightOverride(390.0f);
		ResultCard->SetBrushColor(CardColor);
		ResultCard->SetPadding(FMargin(48.0f));
		ResultText->SetJustification(ETextJustify::Center);
		ResultText->SetAutoWrapText(true);
		StyleHudText(*ResultText, 26, Style.PrimaryTextColor);
		ResultCard->SetContent(ResultText);
		ResultSize->SetContent(ResultCard);
		ResultPanel->SetContent(ResultSize);
		if (!PlaceAnchored(*ResultPanel, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), 100))
		{
			return false;
		}

		return HasRequiredWidgetTree(Blueprint);
	}

	void BuildClawMachineUI()
	{
		UWidgetBlueprint* JoystickBlueprint = EnsureWidgetBlueprint(
			LogPrefix,
			JoystickPackage,
			ScreenFolder,
			*UFF_ClawMachineJoystickWidget::StaticClass(),
			[](UWidgetBlueprint& NewBlueprint, UWidgetTree& Tree)
			{
				return BuildJoystickLayout(NewBlueprint, Tree);
			});
		if (!JoystickBlueprint
			|| JoystickBlueprint->Status == BS_Error
			|| !JoystickBlueprint->GeneratedClass
			|| !JoystickBlueprint->GeneratedClass->IsChildOf(UFF_ClawMachineJoystickWidget::StaticClass())
			|| !HasJoystickWidgetTree(*JoystickBlueprint))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: joystick WBP contract validation failed: %s"),
				LogPrefix, *JoystickPackage);
			return;
		}

		UWidgetBlueprint* Blueprint = EnsureWidgetBlueprint(
			LogPrefix,
			ScreenPackage,
			ScreenFolder,
			*UFF_ClawMachineScreen::StaticClass(),
			[](UWidgetBlueprint& NewBlueprint, UWidgetTree& Tree)
			{
				return BuildScreenLayout(NewBlueprint, Tree);
			});

		if (!Blueprint
			|| Blueprint->Status == BS_Error
			|| !Blueprint->GeneratedClass
			|| !Blueprint->GeneratedClass->IsChildOf(UFF_ClawMachineScreen::StaticClass())
			|| !HasRequiredWidgetTree(*Blueprint))
		{
			UE_LOG(LogFFUIWbpBuilder, Error,
				TEXT("%s_FAIL: parent/widget contract validation failed: %s"),
				LogPrefix, *ScreenPackage);
			return;
		}

		UE_LOG(LogFFUIWbpBuilder, Display,
			TEXT("%s_OK: designer-owned screen and joystick WBP are ready: %s, %s"),
			LogPrefix, *ScreenPackage, *JoystickPackage);
	}

	FAutoConsoleCommand BuildClawMachineUICommand(
		TEXT("FF.BuildClawMachineUI"),
		TEXT("Creates the missing designer-owned WBP_FF_ClawMachine without overwriting it."),
		FConsoleCommandDelegate::CreateStatic(&BuildClawMachineUI));
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
