// Festival pause and options only. Keep the shared gameplay palette and other screens independent.
#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

namespace FFFestivalOptionsStyle
{
	inline FLinearColor Hex(const TCHAR* Value)
	{
		return FLinearColor::FromSRGBColor(FColor::FromHex(Value));
	}

	struct FTokens
	{
		FLinearColor Paper = Hex(TEXT("FAF9F2"));
		FLinearColor Background = Hex(TEXT("EFF5ED"));
		FLinearColor Ink = Hex(TEXT("123F32"));
		FLinearColor Muted = Hex(TEXT("526D60"));
		FLinearColor Line = Hex(TEXT("CEE5D5"));
		FLinearColor LineStrong = Hex(TEXT("9EC8AC"));
		FLinearColor Green = Hex(TEXT("197B51"));
		FLinearColor GreenDeep = Hex(TEXT("105639"));
		FLinearColor Selected = Hex(TEXT("DDF0DC"));
		FLinearColor Hover = Hex(TEXT("EBF6E8"));
		FLinearColor Disabled = Hex(TEXT("E7EDE5"));
		FLinearColor White = FLinearColor::White;
	};

	inline FTokens Get() { return FTokens(); }

	inline FSlateBrush Rounded(const FLinearColor& Fill, const FLinearColor& Outline,
		const float Radius = 12.f, const float Width = 1.f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = Fill;
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Color = Outline;
		Brush.OutlineSettings.Width = Width;
		return Brush;
	}
}
