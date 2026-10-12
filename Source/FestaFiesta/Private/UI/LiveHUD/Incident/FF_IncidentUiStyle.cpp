// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Incident/FF_IncidentUiStyle.h"

#include "Engine/Font.h"
#include "Styling/CoreStyle.h"

namespace FFIncidentUiStyle
{
	FSlateBrush MakeRounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float Width)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = Width;
		return Brush;
	}

	FSlateBrush MakePill(const FLinearColor& Fill)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		return Brush;
	}

	FSlateBrush MakeRing(const FLinearColor& Outline, float Width)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor::Transparent);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		Brush.OutlineSettings.Color = FSlateColor(Outline);
		Brush.OutlineSettings.Width = Width;
		return Brush;
	}

	FSlateFontInfo MakeFont(UFont* UiFont, const TCHAR* Typeface, int32 Size)
	{
		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Regular", Size);
		if (UiFont)
		{
			Font.FontObject = UiFont;
			Font.TypefaceFontName = Typeface;
		}
		Font.Size = Size;
		return Font;
	}

	UFont* LoadUiFont()
	{
		return LoadObject<UFont>(nullptr, UiFontPath);
	}

	FButtonStyle MakeButtonStyle(const FFPauseOptionsLightTeal::FTokens& Tokens, bool bPrimary)
	{
		FButtonStyle Style;
		if (bPrimary)
		{
			Style.Normal = MakeRounded(Tokens.Teal, CardRadius, Tokens.Teal, 1.f);
			Style.Hovered = MakeRounded(Tokens.TealDeep, CardRadius, Tokens.TealDeep, 1.f);
			Style.Pressed = MakeRounded(Tokens.TealDeep, CardRadius, Tokens.TealDeep, 1.f);
		}
		else
		{
			Style.Normal = MakeRounded(Tokens.PaperStrong, CardRadius, Tokens.Line, 1.f);
			Style.Hovered = MakeRounded(Tokens.Hover, CardRadius, Tokens.TealDeep, 1.f);
			Style.Pressed = MakeRounded(Tokens.TealSoft, CardRadius, Tokens.TealDeep, 1.f);
		}
		Style.Disabled = MakeRounded(Tokens.Disabled, CardRadius, Tokens.Line, 1.f);
		Style.NormalPadding = FMargin(14.f, 6.f);
		Style.PressedPadding = Style.NormalPadding;
		return Style;
	}

	FLinearColor GetButtonLabelColor(const FFPauseOptionsLightTeal::FTokens& Tokens, bool bPrimary, bool bEnabled)
	{
		if (!bEnabled)
		{
			return Tokens.InkFaint;
		}
		return bPrimary ? Tokens.White : Tokens.TealDeep;
	}
}
