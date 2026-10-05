// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Incident/FF_IncidentMarkerButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UIDisplayTextLibrary.h"
#include "UI/LiveHUD/Incident/FF_IncidentUiStyle.h"

#define LOCTEXT_NAMESPACE "FestaFiestaIncidentMarker"

void UFF_IncidentMarkerButton::EnsureTree()
{
	if (Image || !WidgetTree)
	{
		return;
	}

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Overlay_Marker"));
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Root;

	Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_Marker"));
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* ImageSlot = Root->AddChildToOverlay(Image))
	{
		ImageSlot->SetHorizontalAlignment(HAlign_Fill);
		ImageSlot->SetVerticalAlignment(VAlign_Fill);
		ImageSlot->SetPadding(FMargin(3.f));
	}

	Ring = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_ResponseRing"));
	if (const UFF_UIPalette* Palette = UFF_UIPalette::Get())
	{
		Ring->SetBrush(FFIncidentUiStyle::MakeRing(Palette->InGame.Teal, 3.f));
	}
	Ring->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* RingSlot = Root->AddChildToOverlay(Ring))
	{
		RingSlot->SetHorizontalAlignment(HAlign_Fill);
		RingSlot->SetVerticalAlignment(VAlign_Fill);
	}
}

void UFF_IncidentMarkerButton::SetResponseStage(EFF_IncidentResponseStage InStage)
{
	EnsureTree();
	ResponseStage = InStage;
	const bool bResponding = InStage != EFF_IncidentResponseStage::None;
	if (Ring)
	{
		Ring->SetVisibility(bResponding ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	SetToolTipText(bResponding
		? FText::Format(LOCTEXT("RespondingTip", "스태프 출동 중 · {0}"), UFF_UIDisplayTextLibrary::GetIncidentResponseStageText(InStage))
		: FText::GetEmpty());
}

bool UFF_IncidentMarkerButton::IsResponseRingVisible() const
{
	return Ring && Ring->GetVisibility() != ESlateVisibility::Collapsed;
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

#undef LOCTEXT_NAMESPACE

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
