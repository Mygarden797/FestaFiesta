// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Minigame/FF_ClawMachineScreen.h"

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
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Viewport.h"
#include "InputCoreTypes.h"
#include "Minigame/FF_ClawMachineRuntime.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "UI/FF_UILog.h"
#include "UI/Minigame/FF_ClawMachineArena3D.h"
#include "UI/Minigame/FF_ClawMachineJoystickWidget.h"
#include "UI/Minigame/FF_MinigameUIStyle.h"

#define LOCTEXT_NAMESPACE "FFClawMachineScreen"

namespace
{
	float SmootherStep01(const float Value)
	{
		const float Alpha = FMath::Clamp(Value, 0.0f, 1.0f);
		return Alpha * Alpha * Alpha * (Alpha * (Alpha * 6.0f - 15.0f) + 10.0f);
	}

	float CalculateInspectionWeight(
		const FFF_ClawMachineSnapshot& Snapshot,
		const FFF_ClawMachineVisualStyle& Style)
	{
		const float PhaseAlpha = FMath::Clamp(Snapshot.PhaseAlpha, 0.0f, 1.0f);
		switch (Snapshot.Phase)
		{
		case EFF_ClawMachinePhase::Dropping:
			return SmootherStep01(PhaseAlpha / FMath::Max(0.1f, Style.CameraDropZoomCompletionRatio));
		case EFF_ClawMachinePhase::Closing:
			return 1.0f;
		case EFF_ClawMachinePhase::Lifting:
		{
			const float HoldRatio = FMath::Clamp(Style.CameraLiftCloseupHoldRatio, 0.0f, 0.9f);
			if (PhaseAlpha <= HoldRatio)
			{
				return 1.0f;
			}
			return 1.0f - SmootherStep01((PhaseAlpha - HoldRatio) / (1.0f - HoldRatio));
		}
		default:
			return 0.0f;
		}
	}

	float CalculateInspectionFocusZ(const FFF_ClawMachineSnapshot& Snapshot)
	{
		constexpr float RaisedHubZ = 425.0f;
		constexpr float LoweredHubZ = -52.0f;
		constexpr float ContactFocusOffsetZ = -58.0f;
		const float PhaseAlpha = SmootherStep01(Snapshot.PhaseAlpha);
		float HubZ = RaisedHubZ;
		switch (Snapshot.Phase)
		{
		case EFF_ClawMachinePhase::Dropping:
			HubZ = FMath::Lerp(RaisedHubZ, LoweredHubZ, PhaseAlpha);
			break;
		case EFF_ClawMachinePhase::Closing:
			HubZ = LoweredHubZ;
			break;
		case EFF_ClawMachinePhase::Lifting:
			HubZ = FMath::Lerp(LoweredHubZ, RaisedHubZ, PhaseAlpha);
			break;
		default:
			break;
		}
		return HubZ + ContactFocusOffsetZ;
	}

	void StyleText(UTextBlock* Text, const float Size, const FLinearColor& Color)
	{
		if (!Text)
		{
			return;
		}
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FMath::RoundToInt(Size);
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	}

	void PlaceOnCanvas(
		UCanvasPanel* Canvas,
		UWidget* Widget,
		const FAnchors& Anchors,
		const int32 ZOrder)
	{
		if (UCanvasPanelSlot* Slot = Canvas ? Canvas->AddChildToCanvas(Widget) : nullptr)
		{
			Slot->SetAnchors(Anchors);
			Slot->SetOffsets(FMargin(0.0f));
			Slot->SetAlignment(FVector2D::ZeroVector);
			Slot->SetZOrder(ZOrder);
		}
	}
}

UFF_ClawMachineScreen::UFF_ClawMachineScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VisualStyle.ArenaActorClass = AFF_ClawMachineArena3D::StaticClass();
	VisualStyle.JoystickWidgetClass = TSoftClassPtr<UFF_ClawMachineJoystickWidget>(FSoftObjectPath(
		TEXT("/Game/FestaFiesta/UI/Minigame/WBP_FF_ClawMachineJoystick."
			 "WBP_FF_ClawMachineJoystick_C")));
}

bool UFF_ClawMachineScreen::SetRuntime(UFF_ClawMachineRuntime* InRuntime)
{
	if (!IsValid(InRuntime))
	{
		Runtime = nullptr;
		SetSessionId(FGuid());
		bHasSnapshot = false;
		PresentedFeedbackSerial = 0;
		bHasPresentedPhase = false;
		RefreshPresentation();
		return false;
	}

	const FFF_ClawMachineSnapshot Snapshot = InRuntime->GetClawSnapshot();
	const FGuid RuntimeSessionId = Snapshot.Session.Context.SessionId;
	if (!RuntimeSessionId.IsValid()
		|| (GetSessionId().IsValid() && GetSessionId() != RuntimeSessionId))
	{
		return false;
	}
	if (!GetSessionId().IsValid())
	{
		SetSessionId(RuntimeSessionId);
	}
	if (Runtime != InRuntime)
	{
		PresentedFeedbackSerial = 0;
		bHasPresentedPhase = false;
	}
	Runtime = InRuntime;
	ApplySnapshot(Snapshot);
	return true;
}

bool UFF_ClawMachineScreen::BindRuntime(UFF_MinigameRuntime* InRuntime)
{
	return SetRuntime(Cast<UFF_ClawMachineRuntime>(InRuntime));
}

void UFF_ClawMachineScreen::ShowResult(const FFF_MinigameResult& Result)
{
	if (GetSessionId().IsValid() && Result.SessionId != GetSessionId())
	{
		return;
	}
	DisplayedResult = Result;
	bHasResult = true;
	RefreshPresentation();
}

void UFF_ClawMachineScreen::ApplySnapshot(const FFF_ClawMachineSnapshot& Snapshot)
{
	if (GetSessionId().IsValid() && Snapshot.Session.Context.SessionId != GetSessionId())
	{
		return;
	}
	if (bHasPresentedPhase && Snapshot.Phase != PresentedPhase)
	{
		switch (Snapshot.Phase)
		{
		case EFF_ClawMachinePhase::Dropping:
			PlayMinigameSound(EFF_MinigameAudioEvent::ClawDropStarted);
			break;
		case EFF_ClawMachinePhase::Closing:
			PlayMinigameSound(EFF_MinigameAudioEvent::ClawClosing);
			break;
		case EFF_ClawMachinePhase::Releasing:
			PlayMinigameSound(EFF_MinigameAudioEvent::ClawRelease);
			break;
		default:
			break;
		}
	}
	PresentedPhase = Snapshot.Phase;
	bHasPresentedPhase = true;
	if (Snapshot.LatestFeedbackSerial > 0
		&& Snapshot.LatestFeedbackSerial != PresentedFeedbackSerial)
	{
		PresentedFeedbackSerial = Snapshot.LatestFeedbackSerial;
		switch (Snapshot.LatestFeedbackType)
		{
		case EFF_ClawMachineFeedbackType::PerfectGrab:
			PlayMinigameSound(EFF_MinigameAudioEvent::ClawPerfectCapture);
			break;
		case EFF_ClawMachineFeedbackType::Grabbed:
			PlayMinigameSound(EFF_MinigameAudioEvent::ClawPrizeCaptured);
			break;
		case EFF_ClawMachineFeedbackType::Slipped:
		case EFF_ClawMachineFeedbackType::EmptyDrop:
			PlayMinigameSound(EFF_MinigameAudioEvent::ClawPrizeMissed);
			break;
		default:
			break;
		}
		FeedbackRemainingSeconds = 1.5f;
	}
	DisplayedSnapshot = Snapshot;
	bHasSnapshot = true;
	RefreshPresentation();
}

TSharedRef<SWidget> UFF_ClawMachineScreen::RebuildWidget()
{
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		BuildNativeWidgetTree();
	}
	ResolveBindings();
	InitializeBindings();
	RefreshPresentation();
	return Super::RebuildWidget();
}

void UFF_ClawMachineScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ResolveBindings();
	InitializeBindings();
}

void UFF_ClawMachineScreen::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveBindings();
	EnsureJoystickWidget();
	InitializeBindings();
	TryInitializeArena();
	RefreshPresentation();
}

void UFF_ClawMachineScreen::NativeDestruct()
{
	if (Button_Drop) { Button_Drop->OnClicked.RemoveAll(this); }
	if (Button_Close) { Button_Close->OnClicked.RemoveAll(this); }
	if (Button_ResultClose) { Button_ResultClose->OnClicked.RemoveAll(this); }
	if (JoystickWidget)
	{
		JoystickWidget->OnInputAxisChanged.RemoveAll(this);
		JoystickWidget->OnDropRequested.RemoveAll(this);
	}
	if (Runtime && GetSessionId().IsValid())
	{
		Runtime->SetMovementInput(GetSessionId(), FVector2D::ZeroVector);
	}
	Runtime = nullptr;
	ArenaActor = nullptr;
	bArenaReady = false;
	bArenaConfigured = false;
	bCameraRigInitialized = false;
	bBindingsInitialized = false;
	bHasSnapshot = false;
	bHasResult = false;
	PresentedFeedbackSerial = 0;
	bHasPresentedPhase = false;
	bMoveLeft = bMoveRight = bMoveForward = bMoveBackward = false;
	AnalogMovement = FVector2D::ZeroVector;
	VirtualJoystickMovement = FVector2D::ZeroVector;
	JoystickWidget = nullptr;
	InitialFocusTarget = nullptr;
	SetSessionId(FGuid());
	Super::NativeDestruct();
}

void UFF_ClawMachineScreen::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bArenaReady || !bArenaConfigured)
	{
		TryInitializeArena();
	}
	UpdateMovementInput();
	if (IsValid(Runtime))
	{
		ApplySnapshot(Runtime->GetClawSnapshot());
	}
	UpdateOperatorCamera(InDeltaTime);
	FeedbackRemainingSeconds = FMath::Max(0.0f, FeedbackRemainingSeconds - FMath::Max(0.0f, InDeltaTime));
	if (Text_Feedback)
	{
		Text_Feedback->SetVisibility(FeedbackRemainingSeconds > 0.0f
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

FReply UFF_ClawMachineScreen::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::A || Key == EKeys::Left) { bMoveLeft = true; return FReply::Handled(); }
	if (Key == EKeys::D || Key == EKeys::Right) { bMoveRight = true; return FReply::Handled(); }
	if (Key == EKeys::W || Key == EKeys::Up) { bMoveForward = true; return FReply::Handled(); }
	if (Key == EKeys::S || Key == EKeys::Down) { bMoveBackward = true; return FReply::Handled(); }
	if (!InKeyEvent.IsRepeat()
		&& (Key == EKeys::SpaceBar || Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom))
	{
		HandleDropClicked();
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply UFF_ClawMachineScreen::NativeOnKeyUp(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::A || Key == EKeys::Left) { bMoveLeft = false; return FReply::Handled(); }
	if (Key == EKeys::D || Key == EKeys::Right) { bMoveRight = false; return FReply::Handled(); }
	if (Key == EKeys::W || Key == EKeys::Up) { bMoveForward = false; return FReply::Handled(); }
	if (Key == EKeys::S || Key == EKeys::Down) { bMoveBackward = false; return FReply::Handled(); }
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}

FReply UFF_ClawMachineScreen::NativeOnAnalogValueChanged(
	const FGeometry& InGeometry,
	const FAnalogInputEvent& InAnalogEvent)
{
	const FKey Key = InAnalogEvent.GetKey();
	const float Value = FMath::Abs(InAnalogEvent.GetAnalogValue()) >= 0.18f
		? InAnalogEvent.GetAnalogValue()
		: 0.0f;
	if (Key == EKeys::Gamepad_LeftX)
	{
		AnalogMovement.X = Value;
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_LeftY)
	{
		AnalogMovement.Y = -Value;
		return FReply::Handled();
	}
	return Super::NativeOnAnalogValueChanged(InGeometry, InAnalogEvent);
}

UWidget* UFF_ClawMachineScreen::NativeGetDesiredFocusTarget() const
{
	return (bHasResult && (Button_ResultClose || Button_Close))
		? static_cast<UWidget*>(Button_ResultClose ? Button_ResultClose.Get() : Button_Close.Get())
		: const_cast<UFF_ClawMachineScreen*>(this);
}

bool UFF_ClawMachineScreen::BuildNativeWidgetTree()
{
	if (!WidgetTree)
	{
		return false;
	}
	bBuiltNativeWidgetTree = true;
	const FFMinigameUIStyle::FTheme Theme = FFMinigameUIStyle::GetTheme();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root_ClawMachineNative"));
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Background"));
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_ClawMachine"));
	if (!Root || !Background || !Canvas)
	{
		return false;
	}
	WidgetTree->RootWidget = Root;
	Background->SetBrushColor(Theme.HeaderStrong);
	if (UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background))
	{
		BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
		BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UOverlaySlot* CanvasSlot = Root->AddChildToOverlay(Canvas))
	{
		CanvasSlot->SetHorizontalAlignment(HAlign_Fill);
		CanvasSlot->SetVerticalAlignment(VAlign_Fill);
	}

	Viewport_ClawMachine3D = WidgetTree->ConstructWidget<UViewport>(UViewport::StaticClass(), TEXT("Viewport_ClawMachine3D"));
	UBorder* ScreenWash = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_ScreenWash"));
	PlaceOnCanvas(Canvas, Viewport_ClawMachine3D, FAnchors(0.045f, 0.08f, 0.955f, 0.82f), -100);
	ScreenWash->SetBrushColor(VisualStyle.ScreenWashColor);
	ScreenWash->SetVisibility(ESlateVisibility::HitTestInvisible);
	PlaceOnCanvas(Canvas, ScreenWash, FAnchors(0.045f, 0.08f, 0.955f, 0.82f), -90);

	auto MakeCard = [this, Canvas](
		const TCHAR* BorderName,
		const TCHAR* TextName,
		const FAnchors& Anchors,
		TObjectPtr<UTextBlock>& OutText)
	{
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), BorderName);
		OutText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TextName);
		const FFMinigameUIStyle::FTheme CardTheme = FFMinigameUIStyle::GetTheme();
		FFMinigameUIStyle::ApplyCard(
			*Card,
			FFMinigameUIStyle::WithOpacity(CardTheme.Surface, 0.96f),
			FMargin(16.0f, 8.0f),
			CardTheme.Line);
		OutText->SetJustification(ETextJustify::Center);
		StyleText(OutText, 20.0f, CardTheme.Text);
		Card->SetContent(OutText);
		PlaceOnCanvas(Canvas, Card, Anchors, 20);
	};
	MakeCard(TEXT("Card_Time"), TEXT("Text_RemainingTime"), FAnchors(0.39f, 0.018f, 0.58f, 0.075f), Text_RemainingTime);
	MakeCard(TEXT("Card_Score"), TEXT("Text_Score"), FAnchors(0.72f, 0.02f, 0.91f, 0.075f), Text_Score);
	MakeCard(TEXT("Card_Attempts"), TEXT("Text_Attempts"), FAnchors(0.08f, 0.02f, 0.28f, 0.075f), Text_Attempts);

	UBorder* StatusCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Card_Status"));
	UVerticalBox* StatusColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column_Status"));
	Text_Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Status"));
	Progress_Phase = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Progress_Phase"));
	FFMinigameUIStyle::ApplyCard(
		*StatusCard,
		FFMinigameUIStyle::WithOpacity(Theme.SurfaceSunken, 0.96f),
		FMargin(18.0f, 8.0f),
		Theme.Line);
	Text_Status->SetJustification(ETextJustify::Center);
	StyleText(Text_Status, 17.0f, Theme.HeaderStrong);
	Progress_Phase->SetFillColorAndOpacity(Theme.Warning);
	StatusColumn->AddChildToVerticalBox(Text_Status);
	if (UVerticalBoxSlot* PhaseSlot = StatusColumn->AddChildToVerticalBox(Progress_Phase))
	{
		PhaseSlot->SetPadding(FMargin(8.0f, 5.0f, 8.0f, 0.0f));
	}
	StatusCard->SetContent(StatusColumn);
	PlaceOnCanvas(Canvas, StatusCard, FAnchors(0.34f, 0.72f, 0.66f, 0.815f), 24);

	Text_Feedback = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Feedback"));
	Text_Feedback->SetJustification(ETextJustify::Center);
	StyleText(Text_Feedback, 28.0f, Theme.Warning);
	PlaceOnCanvas(Canvas, Text_Feedback, FAnchors(0.28f, 0.14f, 0.72f, 0.22f), 30);

	UBorder* HintCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Card_Hint"));
	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Hint"));
	FFMinigameUIStyle::ApplyCard(
		*HintCard,
		FFMinigameUIStyle::WithOpacity(Theme.SurfaceSunken, 0.96f),
		FMargin(14.0f, 7.0f),
		Theme.Line);
	Hint->SetText(LOCTEXT("ClawHint", "WASD / 방향키 / 스틱: 집게 이동   ·   SPACE / A: 집게 내리기   ·   ESC: 나가기"));
	Hint->SetJustification(ETextJustify::Center);
	StyleText(Hint, 14.0f, Theme.TextMuted);
	HintCard->SetContent(Hint);
	PlaceOnCanvas(Canvas, HintCard, FAnchors(0.18f, 0.94f, 0.82f, 0.985f), 25);

	Button_Drop = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Drop"));
	Text_DropLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_DropLabel"));
	Text_DropLabel->SetText(LOCTEXT("DropButton", "집게 내리기  [SPACE]"));
	Text_DropLabel->SetJustification(ETextJustify::Center);
	StyleText(Text_DropLabel, 18.0f, Theme.White);
	FFMinigameUIStyle::ApplyButton(*Button_Drop, Theme, true);
	Button_Drop->SetContent(Text_DropLabel);
	PlaceOnCanvas(Canvas, Button_Drop, FAnchors(0.39f, 0.84f, 0.61f, 0.925f), 30);

	Button_Close = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Close"));
	Text_CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_CloseLabel"));
	Text_CloseLabel->SetText(LOCTEXT("CloseButton", "나가기  [ESC]"));
	Text_CloseLabel->SetJustification(ETextJustify::Center);
	StyleText(Text_CloseLabel, 13.0f, Theme.Text);
	FFMinigameUIStyle::ApplyButton(*Button_Close, Theme, false);
	Button_Close->SetContent(Text_CloseLabel);
	PlaceOnCanvas(Canvas, Button_Close, FAnchors(0.915f, 0.018f, 0.985f, 0.075f), 120);

	Panel_Result = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel_Result"));
	USizeBox* ResultSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Size_Result"));
	UBorder* ResultCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Card_Result"));
	UVerticalBox* ResultColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("Column_Result"));
	UTextBlock* ResultHeading = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_ResultHeading"));
	Text_Result = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Result"));
	USizeBox* ResultCtaSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("Size_ResultCta"));
	Button_ResultClose = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("Button_ResultClose"));
	Text_ResultCloseLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_ResultCloseLabel"));
	if (!Panel_Result || !ResultSize || !ResultCard || !ResultColumn || !ResultHeading
		|| !Text_Result || !ResultCtaSize || !Button_ResultClose || !Text_ResultCloseLabel)
	{
		return false;
	}
	Panel_Result->SetBrushColor(Theme.Scrim);
	ResultSize->SetWidthOverride(680.0f);
	ResultSize->SetHeightOverride(440.0f);
	FFMinigameUIStyle::ApplyCard(
		*ResultCard, Theme.Surface, FMargin(48.0f, 40.0f), Theme.LineStrong, 10.0f, 2.0f);
	ResultCard->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ResultHeading->SetText(LOCTEXT("ResultHeading", "SEA FRIENDS ARCADE  //  플레이 결과"));
	ResultHeading->SetJustification(ETextJustify::Center);
	StyleText(ResultHeading, 13.0f, Theme.HeaderStrong);
	Text_Result->SetJustification(ETextJustify::Center);
	Text_Result->SetAutoWrapText(true);
	StyleText(Text_Result, 27.0f, Theme.Text);
	ResultCtaSize->SetWidthOverride(240.0f);
	ResultCtaSize->SetHeightOverride(54.0f);
	Text_ResultCloseLabel->SetText(LOCTEXT("ResultClose", "확인  [ESC]"));
	Text_ResultCloseLabel->SetJustification(ETextJustify::Center);
	StyleText(Text_ResultCloseLabel, 15.0f, Theme.White);
	FFMinigameUIStyle::ApplyButton(*Button_ResultClose, Theme, true);
	Button_ResultClose->SetContent(Text_ResultCloseLabel);
	ResultCtaSize->SetContent(Button_ResultClose);
	if (UVerticalBoxSlot* HeadingSlot = ResultColumn->AddChildToVerticalBox(ResultHeading))
	{
		HeadingSlot->SetHorizontalAlignment(HAlign_Center);
		HeadingSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 22.0f));
	}
	if (UVerticalBoxSlot* TextSlot = ResultColumn->AddChildToVerticalBox(Text_Result))
	{
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	if (UVerticalBoxSlot* CtaSlot = ResultColumn->AddChildToVerticalBox(ResultCtaSize))
	{
		CtaSlot->SetHorizontalAlignment(HAlign_Center);
		CtaSlot->SetPadding(FMargin(0.0f, 22.0f, 0.0f, 0.0f));
	}
	else
	{
		return false;
	}
	ResultCard->SetContent(ResultColumn);
	ResultSize->SetContent(ResultCard);
	Panel_Result->SetContent(ResultSize);
	Panel_Result->SetHorizontalAlignment(HAlign_Center);
	Panel_Result->SetVerticalAlignment(VAlign_Center);
	Panel_Result->SetVisibility(ESlateVisibility::Collapsed);
	PlaceOnCanvas(Canvas, Panel_Result, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), 100);
	return HasRequiredBindings();
}

void UFF_ClawMachineScreen::ResolveBindings()
{
	if (!WidgetTree)
	{
		return;
	}
	Viewport_ClawMachine3D = Cast<UViewport>(WidgetTree->FindWidget(TEXT("Viewport_ClawMachine3D")));
	Text_RemainingTime = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_RemainingTime")));
	Text_Score = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Score")));
	Text_Attempts = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Attempts")));
	Text_Status = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Status")));
	Text_Feedback = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Feedback")));
	Progress_Phase = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("Progress_Phase")));
	Button_Drop = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_Drop")));
	Button_Close = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_Close")));
	Button_ResultClose = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_ResultClose")));
	Panel_Result = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Panel_Result")));
	Text_Result = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Result")));
	if (!Text_DropLabel) { Text_DropLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_DropLabel"))); }
	if (!Text_CloseLabel) { Text_CloseLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_CloseLabel"))); }
	if (!Text_ResultCloseLabel) { Text_ResultCloseLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_ResultCloseLabel"))); }
}

bool UFF_ClawMachineScreen::EnsureJoystickWidget()
{
	if (JoystickWidget)
	{
		return true;
	}
	if (!WidgetTree || !GetOwningPlayer())
	{
		return false;
	}

	UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("Canvas_ClawMachineHUD")));
	if (!Canvas)
	{
		Canvas = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("Canvas_ClawMachine")));
	}
	if (!Canvas)
	{
		return false;
	}

	UClass* JoystickClass = VisualStyle.JoystickWidgetClass.LoadSynchronous();
	if (!JoystickClass || !JoystickClass->IsChildOf(UFF_ClawMachineJoystickWidget::StaticClass()))
	{
		JoystickClass = UFF_ClawMachineJoystickWidget::StaticClass();
	}
	JoystickWidget = CreateWidget<UFF_ClawMachineJoystickWidget>(GetOwningPlayer(), JoystickClass);
	if (!JoystickWidget)
	{
		return false;
	}
	JoystickWidget->OnInputAxisChanged.AddDynamic(
		this,
		&UFF_ClawMachineScreen::HandleVirtualJoystickAxisChanged);
	JoystickWidget->OnDropRequested.AddDynamic(this, &UFF_ClawMachineScreen::HandleDropClicked);
	if (UCanvasPanelSlot* JoystickCanvasSlot = Canvas->AddChildToCanvas(JoystickWidget))
	{
		// Arcade control-deck placement: the panel covers only the cabinet apron and
		// never the prize floor or the claw contact area used for aiming.
		JoystickCanvasSlot->SetAnchors(FAnchors(0.385f, 0.912f));
		JoystickCanvasSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		JoystickCanvasSlot->SetPosition(FVector2D::ZeroVector);
		JoystickCanvasSlot->SetSize(FVector2D(260.0f, 136.0f));
		JoystickCanvasSlot->SetZOrder(42);
		return true;
	}

	JoystickWidget->OnInputAxisChanged.RemoveAll(this);
	JoystickWidget->OnDropRequested.RemoveAll(this);
	JoystickWidget = nullptr;
	return false;
}

void UFF_ClawMachineScreen::InitializeBindings()
{
	if (bBindingsInitialized || !HasRequiredBindings())
	{
		return;
	}
	Button_Drop->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDropClicked);
	Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	if (Button_ResultClose)
	{
		Button_ResultClose->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
	InitialFocusTarget = this;
	bBindingsInitialized = true;
}

bool UFF_ClawMachineScreen::HasRequiredBindings() const
{
	return Viewport_ClawMachine3D && Text_RemainingTime && Text_Score && Text_Attempts
		&& Text_Status && Text_Feedback && Progress_Phase && Button_Drop && Button_Close
		&& Panel_Result && Text_Result;
}

bool UFF_ClawMachineScreen::TryInitializeArena()
{
	if (bArenaReady && IsValid(ArenaActor))
	{
		// UCommonActivatableWidget constructs before the minigame runtime is bound.
		// The arena can therefore exist while still containing only its CDO/default
		// components. Configure it as soon as the runtime arrives instead of treating
		// "spawned" and "configured" as the same state.
		if (!bArenaConfigured && IsValid(Runtime))
		{
			ArenaActor->ConfigureMachine(
				Cast<UFF_ClawMachineDefinition>(Runtime->GetDefinition()),
				Runtime->GetClawSnapshot());
			bArenaConfigured = true;
			UE_LOG(LogFestaFiestaUI, Log,
				TEXT("[%s] 인형뽑기 3D 아레나 지연 설정을 완료했습니다."), *GetName());
		}
		return true;
	}
	if (!Viewport_ClawMachine3D || !Viewport_ClawMachine3D->GetViewportWorld())
	{
		return false;
	}
	Viewport_ClawMachine3D->SetBackgroundColor(VisualStyle.ViewportBackgroundColor);
	Viewport_ClawMachine3D->SetEnableAdvancedFeatures(true);
	Viewport_ClawMachine3D->SetLightIntensity(FMath::Max(0.1f, VisualStyle.KeyLightIntensity));
	Viewport_ClawMachine3D->SetSkyIntensity(FMath::Max(0.0f, VisualStyle.SkyLightIntensity));
	const FVector GameplayCameraLocation = ResolveGameplayCameraLocation();
	const FRotator GameplayCameraRotation = ResolveGameplayCameraRotation();
	Viewport_ClawMachine3D->SetViewLocation(GameplayCameraLocation);
	Viewport_ClawMachine3D->SetViewRotation(GameplayCameraRotation);
	CurrentCameraLocation = GameplayCameraLocation;
	CurrentCameraRotation = GameplayCameraRotation;
	bCameraRigInitialized = true;
	Viewport_ClawMachine3D->SetShowFlag(TEXT("Grid"), false);
	Viewport_ClawMachine3D->SetShowFlag(TEXT("SelectionOutline"), false);
	Viewport_ClawMachine3D->SetShowFlag(TEXT("MotionBlur"), false);
	Viewport_ClawMachine3D->SetShowFlag(TEXT("EyeAdaptation"), false);
	Viewport_ClawMachine3D->SetShowFlag(TEXT("Bloom"), false);
	Viewport_ClawMachine3D->SetShowFlag(TEXT("Fog"), false);
	Viewport_ClawMachine3D->SetShowFlag(TEXT("VolumetricFog"), false);
	UClass* ArenaClass = VisualStyle.ArenaActorClass
		? VisualStyle.ArenaActorClass.Get()
		: AFF_ClawMachineArena3D::StaticClass();
	ArenaActor = Cast<AFF_ClawMachineArena3D>(Viewport_ClawMachine3D->Spawn(ArenaClass));
	bArenaReady = IsValid(ArenaActor);
	bArenaConfigured = false;
	if (bArenaReady && IsValid(Runtime))
	{
		ArenaActor->ConfigureMachine(
			Cast<UFF_ClawMachineDefinition>(Runtime->GetDefinition()),
			Runtime->GetClawSnapshot());
		bArenaConfigured = true;
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("[%s] 인형뽑기 3D 아레나 초기 설정을 완료했습니다."), *GetName());
	}
	else if (!bArenaReady)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 인형뽑기 3D 아레나 생성에 실패했습니다."), *GetName());
	}
	return bArenaReady;
}

FVector UFF_ClawMachineScreen::ResolveGameplayCameraLocation() const
{
	return VisualStyle.bUseCloseGameplayView
		? VisualStyle.CloseGameplayCameraLocation
		: VisualStyle.CameraLocation;
}

FRotator UFF_ClawMachineScreen::ResolveGameplayCameraRotation() const
{
	return VisualStyle.bUseCloseGameplayView
		? VisualStyle.CloseGameplayCameraRotation
		: VisualStyle.CameraRotation;
}

void UFF_ClawMachineScreen::RefreshPresentation()
{
	if (!HasRequiredBindings())
	{
		return;
	}
	const bool bPlaying = bHasSnapshot
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing
		&& !bHasResult;
	const bool bShowResult = bHasResult || (bHasSnapshot && !bPlaying);
	const int32 RemainingAttempts = bHasSnapshot
		? FMath::Max(0, DisplayedSnapshot.MaxAttempts - DisplayedSnapshot.AttemptsUsed)
		: 0;
	Text_RemainingTime->SetText(FText::Format(
		LOCTEXT("TimeFormat", "남은 시간  {0}초"),
		FText::AsNumber(FMath::CeilToInt(bHasSnapshot ? DisplayedSnapshot.Session.RemainingSeconds : 0.0f))));
	Text_Score->SetText(FText::Format(
		LOCTEXT("ScoreFormat", "점수  {0}"),
		FText::AsNumber(bHasSnapshot ? DisplayedSnapshot.Session.Score : 0)));
	Text_Attempts->SetText(FText::Format(
		LOCTEXT("AttemptFormat", "남은 기회  {0} / {1}"),
		FText::AsNumber(RemainingAttempts),
		FText::AsNumber(bHasSnapshot ? DisplayedSnapshot.MaxAttempts : 0)));
	Text_Status->SetText(GetPhaseText(bHasSnapshot ? DisplayedSnapshot.Phase : EFF_ClawMachinePhase::Aiming));
	Progress_Phase->SetPercent(bHasSnapshot && DisplayedSnapshot.Phase != EFF_ClawMachinePhase::Aiming
		? DisplayedSnapshot.PhaseAlpha
		: 0.0f);
	Text_Feedback->SetText(GetFeedbackText());
	Button_Drop->SetIsEnabled(bPlaying
		&& DisplayedSnapshot.Phase == EFF_ClawMachinePhase::Aiming
		&& RemainingAttempts > 0);
	if (JoystickWidget)
	{
		JoystickWidget->SetIsEnabled(
			bPlaying && DisplayedSnapshot.Phase == EFF_ClawMachinePhase::Aiming);
	}
	if (Text_DropLabel)
	{
		Text_DropLabel->SetText(DisplayedSnapshot.Phase == EFF_ClawMachinePhase::Aiming
			? LOCTEXT("DropReady", "집게 내리기  [SPACE]")
			: LOCTEXT("DropBusy", "기계 작동 중..."));
	}
	UButton* ResultActionButton = Button_ResultClose ? Button_ResultClose.Get() : Button_Close.Get();
	Panel_Result->SetVisibility(
		bShowResult ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	Button_Close->SetVisibility(
		bShowResult && Button_ResultClose
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	if (UCanvasPanelSlot* CloseSlot = Cast<UCanvasPanelSlot>(Button_Close->Slot))
	{
		CloseSlot->SetAnchors(bShowResult && !Button_ResultClose
			? FAnchors(0.42f, 0.68f, 0.58f, 0.735f)
			: FAnchors(0.855f, 0.035f, 0.965f, 0.085f));
		CloseSlot->SetOffsets(FMargin(0.0f));
		CloseSlot->SetZOrder(bShowResult && !Button_ResultClose ? 110 : 40);
	}
	InitialFocusTarget = bShowResult && ResultActionButton
		? static_cast<UWidget*>(ResultActionButton)
		: static_cast<UWidget*>(this);
	if (bHasResult)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Grade"), DisplayedResult.GradeText.IsEmpty()
			? LOCTEXT("Complete", "완료")
			: DisplayedResult.GradeText);
		Arguments.Add(TEXT("Score"), DisplayedResult.Score);
		Arguments.Add(TEXT("Reward"), DisplayedResult.RewardGold);
		Arguments.Add(TEXT("Captured"), DisplayedSnapshot.CapturedCount);
		Arguments.Add(TEXT("Attempts"), DisplayedSnapshot.MaxAttempts);
		Text_Result->SetText(FText::Format(
			LOCTEXT("ResultFormat", "{Grade} 등급\n\n인형 {Captured}개 획득 / {Attempts}회\n최종 점수  {Score}\n축제 골드  +{Reward}"),
			Arguments));
	}
	if (bShowResult && !bResultFocusApplied && GetOwningLocalPlayer() && ResultActionButton)
	{
		ResultActionButton->SetFocus();
		bResultFocusApplied = true;
	}
	else if (!bShowResult)
	{
		bResultFocusApplied = false;
	}
	if (bArenaReady && ArenaActor && bHasSnapshot)
	{
		ArenaActor->ApplyMachineSnapshot(DisplayedSnapshot);
	}
}

void UFF_ClawMachineScreen::UpdateOperatorCamera(const float DeltaSeconds)
{
	if (!Viewport_ClawMachine3D || !bArenaReady || !bCameraRigInitialized)
	{
		return;
	}
	const FVector GameplayCameraLocation = ResolveGameplayCameraLocation();
	const FRotator GameplayCameraRotation = ResolveGameplayCameraRotation();
	if (!VisualStyle.bEnableOperatorCamera
		|| !bHasSnapshot
		|| DisplayedSnapshot.Phase == EFF_ClawMachinePhase::Aiming
		|| DisplayedSnapshot.Phase == EFF_ClawMachinePhase::RoundComplete)
	{
		// Aiming is a hard camera lock. WASD/analog input moves only the gantry and
		// can never introduce tracking, roll or residual interpolation into the view.
		CurrentCameraLocation = GameplayCameraLocation;
		CurrentCameraRotation = GameplayCameraRotation;
		Viewport_ClawMachine3D->SetViewLocation(CurrentCameraLocation);
		Viewport_ClawMachine3D->SetViewRotation(CurrentCameraRotation);
		return;
	}

	const float InspectionWeight = CalculateInspectionWeight(DisplayedSnapshot, VisualStyle);
	const FVector2D DropDelta = DisplayedSnapshot.DropPositionNormalized - FVector2D(0.5f, 0.5f);
	const FVector DropFloor = AFF_ClawMachineArena3D::NormalizedToPrizeFloor(
		DisplayedSnapshot.DropPositionNormalized);
	FVector InspectionLocation = GameplayCameraLocation + FVector(
		VisualStyle.CameraActionDollyCm + DropDelta.Y * VisualStyle.CameraDepthTrackingCm,
		DropDelta.X * VisualStyle.CameraLateralTrackingCm,
		-DropDelta.Y * VisualStyle.CameraDepthTrackingCm * 0.20f);
	// The reference-style camera begins close to the front glass. Clamp the dolly
	// against a semantic cabinet plane so designer tuning can never enter geometry.
	const float ClosestSafeCameraX = AFF_ClawMachineArena3D::GetCabinetFrontPlaneX()
		- FMath::Max(80.0f, VisualStyle.CameraMinimumCabinetStandoffCm);
	InspectionLocation.X = FMath::Min(InspectionLocation.X, ClosestSafeCameraX);
	const FVector FocusPoint(DropFloor.X, DropFloor.Y, CalculateInspectionFocusZ(DisplayedSnapshot));
	const FRotator UnclampedInspectionRotation = (FocusPoint - InspectionLocation).Rotation();
	const FRotator InspectionDelta = (UnclampedInspectionRotation - GameplayCameraRotation).GetNormalized();
	const FRotator ClampedInspectionRotation = GameplayCameraRotation + FRotator(
		FMath::Clamp(
			InspectionDelta.Pitch,
			-VisualStyle.CameraMaxPitchDegrees,
			VisualStyle.CameraMaxPitchDegrees),
		FMath::Clamp(
			InspectionDelta.Yaw,
			-VisualStyle.CameraMaxYawDegrees,
			VisualStyle.CameraMaxYawDegrees),
		0.0f);
	const FVector TargetLocation = FMath::Lerp(
		GameplayCameraLocation,
		InspectionLocation,
		InspectionWeight);
	const FRotator TargetRotation = FQuat::Slerp(
		GameplayCameraRotation.Quaternion(),
		ClampedInspectionRotation.Quaternion(),
		InspectionWeight).Rotator();
	const float SafeDeltaSeconds = FMath::Clamp(DeltaSeconds, 0.0f, 0.05f);
	CurrentCameraLocation = FMath::VInterpTo(
		CurrentCameraLocation,
		TargetLocation,
		SafeDeltaSeconds,
		FMath::Max(1.0f, VisualStyle.CameraFollowSpeed));
	CurrentCameraRotation = FMath::RInterpTo(
		CurrentCameraRotation,
		TargetRotation,
		SafeDeltaSeconds,
		FMath::Max(1.0f, VisualStyle.CameraFollowSpeed));
	Viewport_ClawMachine3D->SetViewLocation(CurrentCameraLocation);
	Viewport_ClawMachine3D->SetViewRotation(CurrentCameraRotation);
}

void UFF_ClawMachineScreen::UpdateMovementInput()
{
	if (!Runtime || !GetSessionId().IsValid())
	{
		return;
	}
	FVector2D Movement = BuildDigitalMovementAxis();
	if (!VirtualJoystickMovement.IsNearlyZero())
	{
		Movement = VirtualJoystickMovement;
	}
	if (!AnalogMovement.IsNearlyZero())
	{
		Movement = AnalogMovement;
	}
	if (!bHasSnapshot || DisplayedSnapshot.Phase != EFF_ClawMachinePhase::Aiming)
	{
		Movement = FVector2D::ZeroVector;
		VirtualJoystickMovement = FVector2D::ZeroVector;
		if (JoystickWidget)
		{
			JoystickWidget->CancelInput();
		}
	}
	Movement = Movement.GetClampedToMaxSize(1.0f);
	if (JoystickWidget)
	{
		JoystickWidget->SetDisplayedInputAxis(Movement);
	}
	if (ArenaActor)
	{
		ArenaActor->SetOperatorInput(Movement);
	}
	Runtime->SetMovementInput(GetSessionId(), Movement);
}

FVector2D UFF_ClawMachineScreen::BuildDigitalMovementAxis() const
{
	return FVector2D(
		(bMoveRight ? 1.0f : 0.0f) - (bMoveLeft ? 1.0f : 0.0f),
		(bMoveBackward ? 1.0f : 0.0f) - (bMoveForward ? 1.0f : 0.0f));
}

FText UFF_ClawMachineScreen::GetPhaseText(const EFF_ClawMachinePhase Phase)
{
	switch (Phase)
	{
	case EFF_ClawMachinePhase::Aiming: return LOCTEXT("PhaseAiming", "집게를 인형 중앙에 맞추세요");
	case EFF_ClawMachinePhase::Dropping: return LOCTEXT("PhaseDropping", "집게 하강 중");
	case EFF_ClawMachinePhase::Closing: return LOCTEXT("PhaseClosing", "집게 닫는 중");
	case EFF_ClawMachinePhase::Lifting: return LOCTEXT("PhaseLifting", "인형 들어 올리는 중");
	case EFF_ClawMachinePhase::Returning: return LOCTEXT("PhaseReturning", "출구로 이동 중");
	case EFF_ClawMachinePhase::Releasing: return LOCTEXT("PhaseReleasing", "결과 확인 중");
	case EFF_ClawMachinePhase::RoundComplete: return LOCTEXT("PhaseComplete", "플레이 완료");
	default: return FText::GetEmpty();
	}
}

FText UFF_ClawMachineScreen::GetFeedbackText() const
{
	if (!bHasSnapshot)
	{
		return FText::GetEmpty();
	}
	switch (DisplayedSnapshot.LatestFeedbackType)
	{
	case EFF_ClawMachineFeedbackType::PerfectGrab:
		return FText::Format(LOCTEXT("PerfectFeedback", "퍼펙트!  +{0}"), FText::AsNumber(DisplayedSnapshot.LatestScoreDelta));
	case EFF_ClawMachineFeedbackType::Grabbed:
		return FText::Format(LOCTEXT("GrabFeedback", "인형 획득!  +{0}"), FText::AsNumber(DisplayedSnapshot.LatestScoreDelta));
	case EFF_ClawMachineFeedbackType::Slipped: return LOCTEXT("SlipFeedback", "아쉽게 놓쳤어요!");
	case EFF_ClawMachineFeedbackType::EmptyDrop: return LOCTEXT("EmptyFeedback", "인형 중앙을 더 정확히 노려보세요");
	default: return FText::GetEmpty();
	}
}

void UFF_ClawMachineScreen::HandleDropClicked()
{
	if (Runtime && GetSessionId().IsValid())
	{
		Runtime->SubmitDrop(GetSessionId());
		ApplySnapshot(Runtime->GetClawSnapshot());
		if (GetOwningLocalPlayer())
		{
			SetFocus();
		}
	}
}

void UFF_ClawMachineScreen::HandleCloseClicked()
{
	RequestClose();
}

void UFF_ClawMachineScreen::HandleVirtualJoystickAxisChanged(const FVector2D InputAxis)
{
	VirtualJoystickMovement = bHasSnapshot
		&& DisplayedSnapshot.Phase == EFF_ClawMachinePhase::Aiming
		? InputAxis.GetClampedToMaxSize(1.0f)
		: FVector2D::ZeroVector;
}

#undef LOCTEXT_NAMESPACE
