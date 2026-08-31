// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Styling/SlateTypes.h"
#include "UI/Data/FF_UIPalette.h"

/**
 * Shared LightTeal presentation helpers for every minigame.
 *
 * Jongtae's in-game UI owns its colours in UFF_UIPalette. Minigames used to
 * carry three unrelated dark/arcade palettes, which also made result buttons
 * difficult to read. Keep spacing and game-specific art local, but source all
 * common surfaces, text, outlines and button states from the same palette.
 */
namespace FFMinigameUIStyle
{
	struct FTheme
	{
		FLinearColor White;
		FLinearColor Header;
		FLinearColor HeaderStrong;
		FLinearColor AccentSoft;
		FLinearColor Surface;
		FLinearColor SurfaceSunken;
		FLinearColor StatusSurface;
		FLinearColor Hover;
		FLinearColor Line;
		FLinearColor LineStrong;
		FLinearColor Text;
		FLinearColor TextMuted;
		FLinearColor TextFaint;
		FLinearColor Scrim;
		FLinearColor Warning;
		FLinearColor Danger;
		FLinearColor Success;
	};

	inline FLinearColor WithOpacity(FLinearColor Color, const float Opacity)
	{
		Color.A = FMath::Clamp(Opacity, 0.0f, 1.0f);
		return Color;
	}

	inline FTheme GetTheme()
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		check(Palette);
		const FFF_UIPaletteInGame& InGame = Palette->InGame;
		return {
			InGame.White,
			InGame.Teal,
			InGame.TealDeep,
			InGame.TealSoft,
			InGame.Paper,
			InGame.PaperSunken,
			InGame.StatusBar,
			InGame.Hover,
			InGame.Line,
			InGame.LineStrong,
			InGame.Ink,
			InGame.InkMuted,
			InGame.InkFaint,
			InGame.Scrim,
			InGame.Amber,
			InGame.Danger,
			InGame.BandGood,
		};
	}

	inline FSlateBrush MakeRoundedBrush(
		const FLinearColor& Fill,
		const float Radius,
		const FLinearColor& Outline = FLinearColor::Transparent,
		const float OutlineWidth = 0.0f)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			Radius,
			FSlateColor(Outline),
			OutlineWidth);
		return Brush;
	}

	inline void ApplyCard(
		UBorder& Border,
		const FLinearColor& Fill,
		const FMargin& Padding,
		const FLinearColor& Outline,
		const float Radius = 8.0f,
		const float OutlineWidth = 1.0f)
	{
		Border.SetBrush(MakeRoundedBrush(Fill, Radius, Outline, OutlineWidth));
		Border.SetBrushColor(FLinearColor::White);
		Border.SetPadding(Padding);
	}

	inline FButtonStyle MakeButtonStyle(const FTheme& Theme, const bool bPrimary)
	{
		FButtonStyle Style;
		Style.Normal = MakeRoundedBrush(
			bPrimary ? Theme.Header : Theme.SurfaceSunken,
			6.0f,
			bPrimary ? Theme.HeaderStrong : Theme.Line,
			1.0f);
		Style.Hovered = MakeRoundedBrush(
			bPrimary ? Theme.HeaderStrong : Theme.Hover,
			6.0f,
			Theme.HeaderStrong,
			2.0f);
		Style.Pressed = MakeRoundedBrush(
			bPrimary ? Theme.LineStrong : Theme.StatusSurface,
			6.0f,
			Theme.HeaderStrong,
			2.0f);
		Style.Disabled = MakeRoundedBrush(
			Theme.StatusSurface,
			6.0f,
			Theme.Line,
			1.0f);
		Style.NormalPadding = FMargin(18.0f, 12.0f);
		Style.PressedPadding = FMargin(18.0f, 14.0f, 18.0f, 10.0f);
		return Style;
	}

	inline void ApplyButton(UButton& Button, const FTheme& Theme, const bool bPrimary)
	{
		Button.SetStyle(MakeButtonStyle(Theme, bPrimary));
		Button.SetBackgroundColor(FLinearColor::White);
		Button.SetColorAndOpacity(FLinearColor::White);
	}
}
