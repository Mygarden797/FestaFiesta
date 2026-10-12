// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Incident/FF_IncidentMarkerButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
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

	static UTexture2D* MakeArrowTexture()
	{
		constexpr int32 Size = 64;
		constexpr int32 Sub = 4;
		UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
		if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
		{
			return nullptr;
		}

		const FVector2D A(0.15, 0.1);
		const FVector2D B(0.15, 0.9);
		const FVector2D C(0.95, 0.5);
		auto Cross = [](const FVector2D& P, const FVector2D& Q, const FVector2D& R)
		{
			return (Q.X - P.X) * (R.Y - P.Y) - (Q.Y - P.Y) * (R.X - P.X);
		};

		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		uint8* Pixels = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				int32 Covered = 0;
				for (int32 SY = 0; SY < Sub; ++SY)
				{
					for (int32 SX = 0; SX < Sub; ++SX)
					{
						const FVector2D P((X + (SX + 0.5) / Sub) / Size, (Y + (SY + 0.5) / Sub) / Size);
						const double D1 = Cross(A, B, P);
						const double D2 = Cross(B, C, P);
						const double D3 = Cross(C, A, P);
						if ((D1 >= 0. && D2 >= 0. && D3 >= 0.) || (D1 <= 0. && D2 <= 0. && D3 <= 0.))
						{
							++Covered;
						}
					}
				}
				uint8* Pixel = Pixels + (Y * Size + X) * 4;
				Pixel[0] = 255;
				Pixel[1] = 255;
				Pixel[2] = 255;
				Pixel[3] = static_cast<uint8>(255 * Covered / (Sub * Sub));
			}
		}
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		return Texture;
	}
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

	Arrow = AddLayer(TEXT("Image_Arrow"));
	if (UOverlaySlot* ArrowSlot = Cast<UOverlaySlot>(Arrow->Slot))
	{
		ArrowSlot->SetHorizontalAlignment(HAlign_Center);
		ArrowSlot->SetVerticalAlignment(VAlign_Center);
	}
	ArrowTexture = FFIncidentMarkerButton::MakeArrowTexture();
	if (ArrowTexture)
	{
		Arrow->SetBrushFromTexture(ArrowTexture, false);
	}
	Arrow->SetDesiredSizeOverride(FVector2D(ArrowSizePx));
	Arrow->SetVisibility(ESlateVisibility::Collapsed);
}

TSharedRef<SWidget> UFF_IncidentMarkerButton::RebuildWidget()
{
	EnsureTree();
	return Super::RebuildWidget();
}

void UFF_IncidentMarkerButton::SetMarkerSize(const FVector2D& Size)
{
	EnsureTree();
	MarkerSize = Size;
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

	if (Arrow)
	{
		Arrow->SetColorAndOpacity(Look.RingColor);
	}

	ApplyRingParameters();
	SetToolTipText(Look.Tooltip);
	ApplyTransform();
}

void UFF_IncidentMarkerButton::SetEdgeIndicator(bool bOffscreen, const FVector2D& Direction)
{
	EnsureTree();
	if (!Arrow || !ArrowTexture)
	{
		return;
	}
	if (!bOffscreen)
	{
		Arrow->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const float Distance = DiscRadiusUV * FMath::Min(MarkerSize.X, MarkerSize.Y) + ArrowGapPx + ArrowSizePx * 0.5f;
	FWidgetTransform Transform;
	Transform.Translation = Direction * Distance;
	Transform.Angle = FFIncidentMarkerRules::DirectionToAngleDegrees(Direction);
	Arrow->SetRenderTransform(Transform);
	Arrow->SetVisibility(ESlateVisibility::HitTestInvisible);
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
