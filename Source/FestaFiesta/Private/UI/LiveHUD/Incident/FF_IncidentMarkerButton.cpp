// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Incident/FF_IncidentMarkerButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"

void UFF_IncidentMarkerButton::EnsureTree()
{
	if (Image || !WidgetTree)
	{
		return;
	}

	Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Marker"));
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Image;
}

TSharedRef<SWidget> UFF_IncidentMarkerButton::RebuildWidget()
{
	EnsureTree();
	return Super::RebuildWidget();
}

void UFF_IncidentMarkerButton::SetIcon(UTexture2D* Icon, const FLinearColor& FallbackColor, const FVector2D& IconSize)
{
	EnsureTree();
	if (!Image)
	{
		return;
	}

	if (Icon)
	{
		Image->SetBrushFromTexture(Icon, false);
		BaseTint = FLinearColor::White;
	}
	else
	{
		FSlateBrush Dot;
		Dot.DrawAs = ESlateBrushDrawType::RoundedBox;
		Dot.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Image->SetBrush(Dot);
		BaseTint = FallbackColor;
	}
	Image->SetDesiredSizeOverride(IconSize);
	ApplyTint();
}

void UFF_IncidentMarkerButton::ApplyTint()
{
	if (Image)
	{
		const float Scale = bHovered ? 1.25f : 1.f;
		Image->SetColorAndOpacity(FLinearColor(BaseTint.R * Scale, BaseTint.G * Scale, BaseTint.B * Scale, BaseTint.A));
	}
}

FReply UFF_IncidentMarkerButton::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	OnMarkerClicked.ExecuteIfBound(Handle);
	return FReply::Handled();
}

void UFF_IncidentMarkerButton::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	ApplyTint();
}

void UFF_IncidentMarkerButton::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	ApplyTint();
}
