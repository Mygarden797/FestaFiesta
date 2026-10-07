// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Incident/FF_IncidentMarkerButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/LiveHUD/Incident/FF_IncidentUiStyle.h"

namespace FFIncidentMarkerButton
{
	static const FName ProgressParam(TEXT("Progress"));
	static const FName FillColorParam(TEXT("FillColor"));
	static const FName OuterRadiusParam(TEXT("OuterRadius"));
	constexpr float DiscOpacity = 0.88f;
	constexpr float HoverScale = 1.08f;
	constexpr float PulseHz = 0.8f;
	constexpr float PulseAmount = 0.06f;
	constexpr float BlinkHz = 2.5f;
	constexpr float BlinkMinOpacity = 0.55f;
}

void UFF_IncidentMarkerButton::EnsureTree()
{
	if (Disc || !WidgetTree)
	{
		return;
	}

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_Marker"));
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Root;

	auto AddLayer = [this, Root](const TCHAR* Name) -> UImage*
	{
		UImage* Layer = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		Layer->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* LayerSlot = Root->AddChildToOverlay(Layer))
		{
			LayerSlot->SetHorizontalAlignment(HAlign_Fill);
			LayerSlot->SetVerticalAlignment(VAlign_Fill);
		}
		return Layer;
	};

	Disc = AddLayer(TEXT("Image_Disc"));
	Ring = AddLayer(TEXT("Image_Ring"));
	Glyph = AddLayer(TEXT("Image_Glyph"));

	FLinearColor DiscColor = FLinearColor::Black;
	if (const UFF_UIPalette* Palette = UFF_UIPalette::Get())
	{
		DiscColor = Palette->InGame.Ink;
	}
	DiscColor.A = FFIncidentMarkerButton::DiscOpacity;
	Disc->SetBrush(FFIncidentUiStyle::MakePill(DiscColor));
	Ring->SetVisibility(ESlateVisibility::Collapsed);
	Glyph->SetVisibility(ESlateVisibility::Collapsed);
}

TSharedRef<SWidget> UFF_IncidentMarkerButton::RebuildWidget()
{
	EnsureTree();
	return Super::RebuildWidget();
}

void UFF_IncidentMarkerButton::SetMarkerSize(const FVector2D& Size)
{
	EnsureTree();
	if (UOverlaySlot* DiscSlot = Disc ? Cast<UOverlaySlot>(Disc->Slot) : nullptr)
	{
		const float Inset = (0.5f - DiscRadiusUV) * FMath::Min(Size.X, Size.Y);
		DiscSlot->SetPadding(FMargin(Inset));
	}
}

void UFF_IncidentMarkerButton::SetRingMaterial(UMaterialInterface* Material)
{
	EnsureTree();
	if (!Ring || !Material)
	{
		return;
	}
	RingMID = UMaterialInstanceDynamic::Create(Material, this);
	Ring->SetBrushFromMaterial(RingMID);
	Ring->SetVisibility(ESlateVisibility::HitTestInvisible);
	ApplyRingParameters();
}

void UFF_IncidentMarkerButton::ApplyLook(const FFF_IncidentMarkerLook& InLook, UTexture2D* GlyphTexture)
{
	EnsureTree();
	if (Look.Motion != InLook.Motion)
	{
		MotionTime = 0.f;
	}
	Look = InLook;

	if (Glyph && GlyphTexture != CurrentGlyph)
	{
		CurrentGlyph = GlyphTexture;
		if (GlyphTexture)
		{
			Glyph->SetBrushFromTexture(GlyphTexture, false);
			Glyph->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Glyph->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	ApplyRingParameters();
	SetToolTipText(Look.Tooltip);
	ApplyTransform();
}

float UFF_IncidentMarkerButton::GetRingOuterRadius() const
{
	return Look.Motion == EFF_IncidentMarkerMotion::Blink ? RingOuterRadiusUV + BlinkRingBoostUV : RingOuterRadiusUV;
}

void UFF_IncidentMarkerButton::ApplyRingParameters()
{
	if (!RingMID)
	{
		return;
	}
	RingMID->SetScalarParameterValue(FFIncidentMarkerButton::ProgressParam, Look.Progress);
	RingMID->SetVectorParameterValue(FFIncidentMarkerButton::FillColorParam, Look.RingColor);
	RingMID->SetScalarParameterValue(FFIncidentMarkerButton::OuterRadiusParam, GetRingOuterRadius());
}

void UFF_IncidentMarkerButton::ApplyTransform()
{
	float Scale = bHovered ? FFIncidentMarkerButton::HoverScale : 1.f;
	float Opacity = 1.f;
	const float Phase = 2.f * PI * MotionTime;
	switch (Look.Motion)
	{
	case EFF_IncidentMarkerMotion::Pulse:
		Scale *= 1.f + FFIncidentMarkerButton::PulseAmount * (0.5f + 0.5f * FMath::Sin(Phase * FFIncidentMarkerButton::PulseHz));
		break;
	case EFF_IncidentMarkerMotion::Blink:
		Opacity = FMath::Lerp(FFIncidentMarkerButton::BlinkMinOpacity, 1.f,
			0.5f + 0.5f * FMath::Cos(Phase * FFIncidentMarkerButton::BlinkHz));
		break;
	default:
		break;
	}
	SetRenderScale(FVector2D(Scale));
	SetRenderOpacity(Opacity);
}

void UFF_IncidentMarkerButton::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Look.Motion == EFF_IncidentMarkerMotion::None)
	{
		return;
	}
	MotionTime += InDeltaTime;
	ApplyTransform();
}

bool UFF_IncidentMarkerButton::IsInsideDisc(const FGeometry& InGeometry, const FVector2D& ScreenPosition) const
{
	const FVector2D Size = InGeometry.GetLocalSize();
	const FVector2D Local = InGeometry.AbsoluteToLocal(ScreenPosition);
	const float Radius = DiscRadiusUV * FMath::Min(Size.X, Size.Y);
	return FVector2D::DistSquared(Local, Size * 0.5f) <= FMath::Square(Radius);
}

FReply UFF_IncidentMarkerButton::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton
		|| !IsInsideDisc(InGeometry, InMouseEvent.GetScreenSpacePosition()))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	OnMarkerClicked.ExecuteIfBound(Handle);
	return FReply::Handled();
}

FReply UFF_IncidentMarkerButton::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bInside = IsInsideDisc(InGeometry, InMouseEvent.GetScreenSpacePosition());
	if (bInside != bHovered)
	{
		bHovered = bInside;
		ApplyTransform();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UFF_IncidentMarkerButton::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	ApplyTransform();
}
