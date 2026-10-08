#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"

/** A font-independent bulb also works when the native catalog is the browser fallback. */
namespace FFBuildLightingBadge
{
	inline int32 Paint(const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer)
	{
		static const FSlateRoundedBoxBrush Background(FLinearColor::FromSRGBColor(FColor(255, 245, 211)), 7.f);
		const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(139, 95, 16));
		const FPaintGeometry Badge = Geometry.ToPaintGeometry(FVector2D(30, 30), FSlateLayoutTransform(FVector2D(12, 12)));
		FSlateDrawElement::MakeBox(Elements, ++Layer, Badge, &Background, ESlateDrawEffect::None);
		TArray<FVector2D> Bulb;
		for (int32 Index = 0; Index <= 20; ++Index)
		{
			const float Angle = PI * 2.f * Index / 20.f;
			Bulb.Add(FVector2D(15 + 6 * FMath::Cos(Angle), 12 + 6 * FMath::Sin(Angle)));
		}
		FSlateDrawElement::MakeLines(Elements, ++Layer, Badge, Bulb, ESlateDrawEffect::None, Ink, true, 1.8f);
		const auto Line = [&](std::initializer_list<FVector2D> Points)
		{
			TArray<FVector2D> Path;
			for (const FVector2D Point : Points) Path.Add(Point);
			FSlateDrawElement::MakeLines(Elements, Layer, Badge, Path, ESlateDrawEffect::None, Ink, true, 1.8f);
		};
		Line({{11, 17}, {12, 21}, {18, 21}, {19, 17}});
		Line({{13, 24}, {17, 24}});
		Line({{15, 1}, {15, 3}});
		Line({{4, 11}, {6, 11}});
		Line({{24, 11}, {26, 11}});
		return Layer;
	}
}
