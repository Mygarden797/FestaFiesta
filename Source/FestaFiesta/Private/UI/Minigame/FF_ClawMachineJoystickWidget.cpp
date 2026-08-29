// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Minigame/FF_ClawMachineJoystickWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "FFClawMachineJoystickWidget"

namespace
{
	constexpr float NativeJoystickWidth = 260.0f;
	constexpr float NativeJoystickHeight = 136.0f;
	constexpr float KnobTravelPixels = 25.0f;
	const FVector2D NativeTrackCenter(60.0f, 76.0f);

	FSlateBrush MakeRoundedBrush(
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

	FButtonStyle MakeDropButtonStyle()
	{
		FButtonStyle Style;
		Style.Normal = MakeRoundedBrush(FLinearColor(0.96f, 0.15f, 0.12f, 1.0f), 35.0f);
		Style.Hovered = MakeRoundedBrush(FLinearColor(1.0f, 0.28f, 0.18f, 1.0f), 35.0f);
		Style.Pressed = MakeRoundedBrush(FLinearColor(0.72f, 0.055f, 0.045f, 1.0f), 35.0f);
		Style.Disabled = MakeRoundedBrush(FLinearColor(0.28f, 0.18f, 0.15f, 0.72f), 35.0f);
		Style.NormalPadding = FMargin(2.0f);
		Style.PressedPadding = FMargin(2.0f, 5.0f, 2.0f, 0.0f);
		return Style;
	}

	void ConfigureCanvasSlot(
		UWidget& Widget,
		const FVector2D Position,
		const FVector2D Size,
		const int32 ZOrder)
	{
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Widget.Slot))
		{
			Slot->SetAnchors(FAnchors(0.0f));
			Slot->SetAlignment(FVector2D::ZeroVector);
			Slot->SetPosition(Position);
			Slot->SetSize(Size);
			Slot->SetZOrder(ZOrder);
		}
	}

	void ConfigureText(UTextBlock& Text, const int32 FontSize, const FLinearColor& Color)
	{
		FSlateFontInfo Font = Text.GetFont();
		Font.Size = FontSize;
		Text.SetFont(Font);
		Text.SetColorAndOpacity(Color);
		Text.SetJustification(ETextJustify::Center);
		Text.SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text.SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.62f));
	}
}

UFF_ClawMachineJoystickWidget::UFF_ClawMachineJoystickWidget(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

TSharedRef<SWidget> UFF_ClawMachineJoystickWidget::RebuildWidget()
{
	if ((!WidgetTree || !WidgetTree->RootWidget) && !BuildNativeWidgetTree())
	{
		return Super::RebuildWidget();
	}
	ResolveBindings();
	return Super::RebuildWidget();
}

void UFF_ClawMachineJoystickWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveBindings();
	if (Button_JoystickDrop)
	{
		Button_JoystickDrop->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDropPressed);
	}
}

void UFF_ClawMachineJoystickWidget::NativeDestruct()
{
	ReleasePointer();
	if (Button_JoystickDrop) { Button_JoystickDrop->OnClicked.RemoveAll(this); }
	Canvas_Joystick = nullptr;
	Panel_JoystickKnob = nullptr;
	Button_JoystickDrop = nullptr;
	Super::NativeDestruct();
}

void UFF_ClawMachineJoystickWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const float SafeDeltaSeconds = FMath::Clamp(InDeltaTime, 0.0f, 0.05f);
	SmoothedInputAxis = FMath::Vector2DInterpTo(
		SmoothedInputAxis,
		TargetInputAxis,
		SafeDeltaSeconds,
		16.0f);
	if (Panel_JoystickKnob)
	{
		if (UCanvasPanelSlot* KnobSlot = Cast<UCanvasPanelSlot>(Panel_JoystickKnob->Slot))
		{
			const FVector2D KnobSize = KnobSlot->GetSize();
			KnobSlot->SetPosition(
				NativeTrackCenter - KnobSize * 0.5f + SmoothedInputAxis * KnobTravelPixels);
		}
	}
}

void UFF_ClawMachineJoystickWidget::SetDisplayedInputAxis(FVector2D InputAxis)
{
	TargetInputAxis = InputAxis.GetClampedToMaxSize(1.0f);
}

void UFF_ClawMachineJoystickWidget::CancelInput()
{
	ReleasePointer();
}

FReply UFF_ClawMachineJoystickWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	bPointerCaptured = true;
	ApplyPointerAxis(InGeometry, InMouseEvent);
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UFF_ClawMachineJoystickWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bPointerCaptured)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}
	ApplyPointerAxis(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UFF_ClawMachineJoystickWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (!bPointerCaptured || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}
	ReleasePointer();
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UFF_ClawMachineJoystickWidget::NativeOnTouchStarted(
	const FGeometry& InGeometry,
	const FPointerEvent& InGestureEvent)
{
	bPointerCaptured = true;
	ApplyPointerAxis(InGeometry, InGestureEvent);
	return FReply::Handled();
}

FReply UFF_ClawMachineJoystickWidget::NativeOnTouchMoved(
	const FGeometry& InGeometry,
	const FPointerEvent& InGestureEvent)
{
	if (!bPointerCaptured)
	{
		return Super::NativeOnTouchMoved(InGeometry, InGestureEvent);
	}
	ApplyPointerAxis(InGeometry, InGestureEvent);
	return FReply::Handled();
}

FReply UFF_ClawMachineJoystickWidget::NativeOnTouchEnded(
	const FGeometry& InGeometry,
	const FPointerEvent& InGestureEvent)
{
	if (!bPointerCaptured)
	{
		return Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
	}
	ReleasePointer();
	return FReply::Handled();
}

bool UFF_ClawMachineJoystickWidget::BuildNativeWidgetTree()
{
	if (!WidgetTree)
	{
		return false;
	}
	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Size_JoystickRoot"));
	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel_JoystickPlate"));
	Canvas_Joystick = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_Joystick"));
	UBorder* Track = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel_JoystickTrack"));
	Panel_JoystickKnob = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Panel_JoystickKnob"));
	Button_JoystickDrop = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_JoystickDrop"));
	UTextBlock* MoveLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_JoystickMove"));
	UTextBlock* DropLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_JoystickDrop"));
	UTextBlock* KeyLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_JoystickKeys"));
	if (!Root || !Plate || !Canvas_Joystick || !Track || !Panel_JoystickKnob
		|| !Button_JoystickDrop || !MoveLabel || !DropLabel || !KeyLabel)
	{
		return false;
	}

	Root->SetWidthOverride(NativeJoystickWidth);
	Root->SetHeightOverride(NativeJoystickHeight);
	Plate->SetBrush(MakeRoundedBrush(
		FLinearColor(0.92f, 0.56f, 0.10f, 0.98f),
		12.0f,
		FLinearColor(1.0f, 0.78f, 0.24f, 0.92f),
		2.0f));
	Plate->SetBrushColor(FLinearColor::White);
	Plate->SetPadding(FMargin(4.0f));
	Track->SetBrush(MakeRoundedBrush(
		FLinearColor(0.012f, 0.035f, 0.045f, 1.0f),
		40.0f,
		FLinearColor(0.03f, 0.18f, 0.20f, 1.0f),
		3.0f));
	Track->SetBrushColor(FLinearColor::White);
	Panel_JoystickKnob->SetBrush(MakeRoundedBrush(
		FLinearColor(0.96f, 0.15f, 0.12f, 1.0f),
		19.0f,
		FLinearColor(1.0f, 0.42f, 0.28f, 1.0f),
		2.0f));
	Panel_JoystickKnob->SetBrushColor(FLinearColor::White);
	Button_JoystickDrop->SetStyle(MakeDropButtonStyle());
	MoveLabel->SetText(LOCTEXT("MoveLabel", "MOVE"));
	DropLabel->SetText(LOCTEXT("DropLabel", "DROP"));
	KeyLabel->SetText(LOCTEXT("KeyLabel", "SPACE / A"));
	ConfigureText(*MoveLabel, 13, FLinearColor(0.02f, 0.08f, 0.09f, 1.0f));
	ConfigureText(*DropLabel, 14, FLinearColor::White);
	ConfigureText(*KeyLabel, 10, FLinearColor(0.02f, 0.08f, 0.09f, 1.0f));
	Button_JoystickDrop->SetContent(DropLabel);

	Canvas_Joystick->AddChildToCanvas(Track);
	Canvas_Joystick->AddChildToCanvas(Panel_JoystickKnob);
	Canvas_Joystick->AddChildToCanvas(Button_JoystickDrop);
	Canvas_Joystick->AddChildToCanvas(MoveLabel);
	Canvas_Joystick->AddChildToCanvas(KeyLabel);
	ConfigureCanvasSlot(*Track, FVector2D(20.0f, 36.0f), FVector2D(80.0f, 80.0f), 1);
	ConfigureCanvasSlot(*Panel_JoystickKnob, FVector2D(41.0f, 57.0f), FVector2D(38.0f, 38.0f), 3);
	ConfigureCanvasSlot(*Button_JoystickDrop, FVector2D(158.0f, 28.0f), FVector2D(70.0f, 70.0f), 3);
	ConfigureCanvasSlot(*MoveLabel, FVector2D(18.0f, 10.0f), FVector2D(84.0f, 18.0f), 4);
	ConfigureCanvasSlot(*KeyLabel, FVector2D(137.0f, 105.0f), FVector2D(112.0f, 18.0f), 4);
	Plate->SetContent(Canvas_Joystick);
	Root->SetContent(Plate);
	WidgetTree->RootWidget = Root;
	return true;
}

void UFF_ClawMachineJoystickWidget::ResolveBindings()
{
	if (!WidgetTree)
	{
		return;
	}
	Canvas_Joystick = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("Canvas_Joystick")));
	Panel_JoystickKnob = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Panel_JoystickKnob")));
	Button_JoystickDrop = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_JoystickDrop")));
}

FVector2D UFF_ClawMachineJoystickWidget::CalculatePointerAxis(
	const FGeometry& Geometry,
	const FPointerEvent& Event) const
{
	const FVector2D LocalPosition = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
	const FVector2D LocalSize = Geometry.GetLocalSize();
	const FVector2D TrackCenter(
		LocalSize.X * (NativeTrackCenter.X / NativeJoystickWidth),
		LocalSize.Y * (NativeTrackCenter.Y / NativeJoystickHeight));
	const float Radius = FMath::Max(
		1.0f,
		FMath::Min(LocalSize.X / NativeJoystickWidth, LocalSize.Y / NativeJoystickHeight)
			* KnobTravelPixels);
	return ((LocalPosition - TrackCenter) / Radius).GetClampedToMaxSize(1.0f);
}

void UFF_ClawMachineJoystickWidget::ApplyPointerAxis(
	const FGeometry& Geometry,
	const FPointerEvent& Event)
{
	TargetInputAxis = CalculatePointerAxis(Geometry, Event);
	OnInputAxisChanged.Broadcast(TargetInputAxis);
}

void UFF_ClawMachineJoystickWidget::ReleasePointer()
{
	if (!bPointerCaptured && TargetInputAxis.IsNearlyZero())
	{
		return;
	}
	bPointerCaptured = false;
	TargetInputAxis = FVector2D::ZeroVector;
	OnInputAxisChanged.Broadcast(TargetInputAxis);
}

void UFF_ClawMachineJoystickWidget::HandleDropPressed()
{
	OnDropRequested.Broadcast();
}

#undef LOCTEXT_NAMESPACE
