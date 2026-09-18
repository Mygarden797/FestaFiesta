// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityProgramMarkerWidget.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Font.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/Data/FF_UIPalette.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "FFFacilityProgramMarker"

UFF_FacilityProgramMarkerWidget::UFF_FacilityProgramMarkerWidget(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> UFF_FacilityProgramMarkerWidget::RebuildWidget()
{
	LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	FontAsset = LoadObject<UFont>(nullptr,
		TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard"));
	if (FontAsset)
	{
		LabelFont = FSlateFontInfo(FontAsset, 11, FName(TEXT("SemiBold")));
	}
	return SNew(SBox).WidthOverride(156.f).HeightOverride(34.f)
		.Visibility(EVisibility::HitTestInvisible);
}

void UFF_FacilityProgramMarkerWidget::SetMarkerState(const EFF_FacilityProgramMarkerState InState)
{
	MarkerState = InState;
	Label = MarkerState == EFF_FacilityProgramMarkerState::Scheduled
		? LOCTEXT("Scheduled", "프로그램 편성됨")
		: LOCTEXT("Available", "프로그램 가능");
	SetVisibility(MarkerState == EFF_FacilityProgramMarkerState::Hidden
		? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	InvalidateLayoutAndVolatility();
}

int32 UFF_FacilityProgramMarkerWidget::NativePaint(const FPaintArgs& Args,
	const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (MarkerState == EFF_FacilityProgramMarkerState::Hidden)
	{
		return BaseLayer;
	}

	const FFF_UIPaletteInGame Palette;
	const FLinearColor Color = MarkerState == EFF_FacilityProgramMarkerState::Scheduled
		? Palette.Info : Palette.Neutral;
	static const FSlateRoundedBoxBrush BadgeBrush(FLinearColor::White, 17.f);
	FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 1,
		AllottedGeometry.ToPaintGeometry(), &BadgeBrush, ESlateDrawEffect::None, Color);

	// Draw a calendar directly so the icon survives missing emoji fonts and cooked assets.
	auto DrawLine = [&](TArray<FVector2f> Points)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 2,
			AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
			FLinearColor::White, true, 1.7f);
	};
	DrawLine({{11.f, 11.f}, {28.f, 11.f}, {28.f, 25.f}, {11.f, 25.f}, {11.f, 11.f}});
	DrawLine({{11.f, 15.f}, {28.f, 15.f}});
	DrawLine({{15.f, 8.f}, {15.f, 13.f}});
	DrawLine({{24.f, 8.f}, {24.f, 13.f}});
	if (MarkerState == EFF_FacilityProgramMarkerState::Scheduled)
	{
		DrawLine({{15.f, 20.f}, {18.f, 23.f}, {24.f, 18.f}});
	}
	else
	{
		DrawLine({{16.f, 20.f}, {23.f, 20.f}});
		DrawLine({{19.5f, 17.f}, {19.5f, 23.f}});
	}
	FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 2,
		AllottedGeometry.ToPaintGeometry(FVector2f(112.f, 22.f),
			FSlateLayoutTransform(FVector2f(35.f, 8.f))),
		Label, LabelFont, ESlateDrawEffect::None, FLinearColor::White);
	return BaseLayer + 2;
}

#undef LOCTEXT_NAMESPACE
