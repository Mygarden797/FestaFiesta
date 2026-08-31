// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "UI/Data/FF_UIPalette.h"

/**
 * ESC/F5 Pause · Options surfaces derived from Jongtae's LightTeal palette.
 *
 * RGB values must stay in UFF_UIPalette.  This small role map only owns the
 * translucency required when LightTeal paper is placed over a blurred world.
 * Keeping it here prevents Pause, Settings and the keyboard guide from
 * drifting into separate visual systems again.
 */
namespace FFPauseOptionsLightTeal
{
	inline constexpr const TCHAR* PanelHeaderPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Panels/T_FF_LH_Panel_Header_1024x128_9Slice.T_FF_LH_Panel_Header_1024x128_9Slice");
	inline constexpr const TCHAR* PanelBodyPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Panels/T_FF_LH_Panel_Body_512_9Slice.T_FF_LH_Panel_Body_512_9Slice");
	inline constexpr const TCHAR* PanelInsetPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Panels/T_FF_LH_Panel_Inset_512_9Slice.T_FF_LH_Panel_Inset_512_9Slice");
	inline constexpr const TCHAR* PanelFooterPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Panels/T_FF_LH_Panel_Footer_1024x96_9Slice.T_FF_LH_Panel_Footer_1024x96_9Slice");
	inline constexpr const TCHAR* MenuRailPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Panels/T_FF_LH_HUD_MenuRail_2048x128_9Slice.T_FF_LH_HUD_MenuRail_2048x128_9Slice");
	inline constexpr const TCHAR* ListRowPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Cards/T_FF_LH_Card_ListRow_1024x144_9Slice.T_FF_LH_Card_ListRow_1024x144_9Slice");
	inline constexpr const TCHAR* RectNavDefaultPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Buttons/T_FF_LH_Button_Rect_Nav_Default_512x128_9Slice.T_FF_LH_Button_Rect_Nav_Default_512x128_9Slice");
	inline constexpr const TCHAR* RectNavHoverPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Buttons/T_FF_LH_Button_Rect_Nav_Hover_512x128_9Slice.T_FF_LH_Button_Rect_Nav_Hover_512x128_9Slice");
	inline constexpr const TCHAR* RectNavPressedPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Buttons/T_FF_LH_Button_Rect_Nav_Pressed_512x128_9Slice.T_FF_LH_Button_Rect_Nav_Pressed_512x128_9Slice");
	inline constexpr const TCHAR* RectNavSelectedPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Buttons/T_FF_LH_Button_Rect_Nav_Selected_512x128_9Slice.T_FF_LH_Button_Rect_Nav_Selected_512x128_9Slice");
	inline constexpr const TCHAR* RectNavDisabledPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Buttons/T_FF_LH_Button_Rect_Nav_Disabled_512x128_9Slice.T_FF_LH_Button_Rect_Nav_Disabled_512x128_9Slice");
	inline constexpr const TCHAR* RectPrimaryPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Buttons/T_FF_LH_Button_Rect_Primary_Default_512x128_9Slice.T_FF_LH_Button_Rect_Primary_Default_512x128_9Slice");
	inline constexpr const TCHAR* RectPrimaryHoverPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Buttons/T_FF_LH_Button_Rect_Primary_Hover_512x128_9Slice.T_FF_LH_Button_Rect_Primary_Hover_512x128_9Slice");
	inline constexpr const TCHAR* RectSecondaryPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Buttons/T_FF_LH_Button_Rect_Secondary_Default_512x128_9Slice.T_FF_LH_Button_Rect_Secondary_Default_512x128_9Slice");
	inline constexpr const TCHAR* RectDangerPath =
		TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Buttons/T_FF_LH_Button_Rect_Danger_Default_512x128_9Slice.T_FF_LH_Button_Rect_Danger_Default_512x128_9Slice");

	inline FLinearColor WithAlpha(FLinearColor Color, const float Alpha)
	{
		Color.A = Alpha;
		return Color;
	}

	struct FTokens
	{
		FLinearColor Scrim;
		FLinearColor Paper;
		FLinearColor PaperStrong;
		FLinearColor PaperSunken;
		FLinearColor StatusBar;
		FLinearColor Hover;
		FLinearColor Disabled;
		FLinearColor Line;
		FLinearColor LineStrong;
		FLinearColor Ink;
		FLinearColor InkMuted;
		FLinearColor InkFaint;
		FLinearColor Teal;
		FLinearColor TealDeep;
		FLinearColor TealSoft;
		FLinearColor White;
		FLinearColor Danger;
	};

	inline FTokens Get(const UFF_UIPalette& Palette)
	{
		const FFF_UIPaletteInGame& InGame = Palette.InGame;
		return {
			WithAlpha(InGame.Scrim, 0.34f),
			WithAlpha(InGame.Paper, 0.94f),
			WithAlpha(InGame.Paper, 0.985f),
			WithAlpha(InGame.PaperSunken, 0.94f),
			WithAlpha(InGame.StatusBar, 0.97f),
			WithAlpha(InGame.Hover, 0.98f),
			WithAlpha(InGame.Beige, 0.94f),
			WithAlpha(InGame.Line, 0.94f),
			InGame.LineStrong,
			InGame.Ink,
			InGame.InkMuted,
			InGame.InkFaint,
			InGame.Teal,
			InGame.TealDeep,
			InGame.TealSoft,
			InGame.White,
			InGame.Danger
		};
	}

	/** Creates a hard-referenced 9-slice brush, with a palette fallback for editor recovery. */
	inline FSlateBrush MakeThemeBox(
		const TCHAR* ObjectPath,
		const FVector2D SourceSize,
		const FMargin NormalizedMargin,
		const FLinearColor FallbackFill,
		const FLinearColor FallbackOutline,
		const float FallbackRadius,
		const float Opacity = 1.f)
	{
		FSlateBrush Brush;
		if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, ObjectPath))
		{
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.SetResourceObject(Texture);
			Brush.ImageSize = SourceSize;
			Brush.Margin = NormalizedMargin;
			Brush.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, Opacity));
			return Brush;
		}

		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FallbackFill);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(
			FallbackRadius, FallbackRadius, FallbackRadius, FallbackRadius);
		Brush.OutlineSettings.Color = FSlateColor(FallbackOutline);
		Brush.OutlineSettings.Width = 1.f;
		return Brush;
	}

	inline FSlateBrush MakeHeaderBrush(const FTokens& Tokens)
	{
		return MakeThemeBox(PanelHeaderPath, FVector2D(1024.f, 128.f),
			FMargin(0.0234375f, 0.1875f), Tokens.TealDeep, Tokens.TealDeep, 8.f);
	}

	inline FSlateBrush MakeBodyBrush(const FTokens& Tokens)
	{
		return MakeThemeBox(PanelBodyPath, FVector2D(512.f, 512.f),
			FMargin(0.09375f), Tokens.PaperStrong, Tokens.Line, 8.f, 0.98f);
	}

	inline FSlateBrush MakeInsetBrush(const FTokens& Tokens)
	{
		return MakeThemeBox(PanelInsetPath, FVector2D(512.f, 512.f),
			FMargin(0.0546875f), Tokens.PaperSunken, Tokens.Line, 6.f, 0.98f);
	}

	inline FSlateBrush MakeFooterBrush(const FTokens& Tokens)
	{
		return MakeThemeBox(PanelFooterPath, FVector2D(1024.f, 96.f),
			FMargin(0.01953125f, 0.208333f), Tokens.StatusBar, Tokens.Line, 8.f, 0.98f);
	}

	inline FSlateBrush MakeMenuRailBrush(const FTokens& Tokens)
	{
		return MakeThemeBox(MenuRailPath, FVector2D(2048.f, 128.f),
			FMargin(0.0234375f, 0.1875f), Tokens.PaperStrong, Tokens.Line, 8.f, 0.98f);
	}

	inline FSlateBrush MakeListRowBrush(const FTokens& Tokens)
	{
		return MakeThemeBox(ListRowPath, FVector2D(1024.f, 144.f),
			FMargin(0.0234375f, 0.166667f), Tokens.PaperSunken, Tokens.Line, 6.f, 0.98f);
	}

	inline FSlateBrush MakeRectButtonBrush(
		const TCHAR* Path, const FLinearColor FallbackFill, const FLinearColor FallbackOutline)
	{
		return MakeThemeBox(Path, FVector2D(512.f, 128.f),
			FMargin(0.0234375f, 0.09375f), FallbackFill, FallbackOutline, 4.f);
	}
}
