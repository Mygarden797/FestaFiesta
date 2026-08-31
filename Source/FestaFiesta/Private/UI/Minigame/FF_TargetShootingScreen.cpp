// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Minigame/FF_TargetShootingScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Viewport.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Math/RotationMatrix.h"
#include "Minigame/FF_TargetShootingRuntime.h"
#include "Minigame/TargetShooting/FF_TargetShootingRules.h"
#include "Rendering/DrawElementTypes.h"
#include "UI/FF_UILog.h"
#include "UI/Minigame/FF_MinigameUIStyle.h"
#include "UI/Minigame/FF_TargetShootingArena3D.h"

#define LOCTEXT_NAMESPACE "FFTargetShootingScreen"

namespace
{
	const TCHAR* const TargetShootingBackdropPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/TargetShooting/T_MG_TargetShooting_Backdrop_Single_ImageGen_v4."
			 "T_MG_TargetShooting_Backdrop_Single_ImageGen_v4");
	const TCHAR* const TargetShootingWaterBlasterPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/UIUX_v2/T_MG_TargetShooting_WaterBlaster_UIUX_v2."
			 "T_MG_TargetShooting_WaterBlaster_UIUX_v2");
	const TCHAR* const TargetShootingTargetPath =
		TEXT("/Game/FestaFiesta/UI/Minigame/TargetShooting/T_MG_TargetShooting_Target_Simple_ImageGen_v1."
			 "T_MG_TargetShooting_Target_Simple_ImageGen_v1");
	constexpr float ArenaProjectileOriginDepthX = -810.0f;
	constexpr float ArenaProjectileImpactDepthX = 420.0f;

	FVector2D RotateScreenVector(const FVector2D& Vector, const float AngleDegrees)
	{
		const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
		const float CosAngle = FMath::Cos(AngleRadians);
		const float SinAngle = FMath::Sin(AngleRadians);
		return FVector2D(
			Vector.X * CosAngle - Vector.Y * SinAngle,
			Vector.X * SinAngle + Vector.Y * CosAngle);
	}

	UTexture2D* LoadPreferredTexture(
		const TSoftObjectPtr<UTexture2D>& PreferredTexture,
		const TCHAR* FallbackObjectPath)
	{
		// Designer-authored Class Defaults are authoritative. The native path is only
		// a resilient fallback for a blank or stale soft reference.
		if (!PreferredTexture.IsNull())
		{
			if (UTexture2D* Texture = PreferredTexture.LoadSynchronous())
			{
				return Texture;
			}
		}
		return LoadObject<UTexture2D>(nullptr, FallbackObjectPath);
	}

	void StyleNativeText(
		UTextBlock* TextBlock,
		const float FontSize,
		const FLinearColor& Color = FLinearColor::White,
		const FName Typeface = TEXT("Bold"))
	{
		if (!TextBlock)
		{
			return;
		}

		FSlateFontInfo Font = TextBlock->GetFont();
		if (UFont* UiFont = LoadObject<UFont>(
			nullptr,
			TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard")))
		{
			Font.FontObject = UiFont;
		}
		Font.Size = FMath::RoundToInt(FontSize);
		Font.TypefaceFontName = Typeface;
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetShadowOffset(FVector2D(1.0f, 2.0f));
		TextBlock->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.01f, 0.02f, 0.82f));
	}
}

void UFF_TargetShootingTargetVisual::Configure(
	const FLinearColor& InOuterColor,
	const FLinearColor& InPrecisionColor,
	const FLinearColor& InBullseyeColor,
	const float InPrecisionRatio,
	const float InBullseyeRatio,
	const float InRemainingLifeRatio,
	const FLinearColor& InCountdownColor,
	const float InLineThickness)
{
	if (OuterColor.Equals(InOuterColor)
		&& PrecisionColor.Equals(InPrecisionColor)
		&& BullseyeColor.Equals(InBullseyeColor)
		&& CountdownColor.Equals(InCountdownColor)
		&& FMath::IsNearlyEqual(PrecisionRatio, InPrecisionRatio)
		&& FMath::IsNearlyEqual(BullseyeRatio, InBullseyeRatio)
		&& FMath::IsNearlyEqual(RemainingLifeRatio, InRemainingLifeRatio)
		&& FMath::IsNearlyEqual(LineThickness, InLineThickness))
	{
		return;
	}
	OuterColor = InOuterColor;
	PrecisionColor = InPrecisionColor;
	BullseyeColor = InBullseyeColor;
	CountdownColor = InCountdownColor;
	PrecisionRatio = FMath::Clamp(InPrecisionRatio, 0.01f, 0.99f);
	BullseyeRatio = FMath::Clamp(InBullseyeRatio, 0.01f, PrecisionRatio);
	RemainingLifeRatio = FMath::Clamp(InRemainingLifeRatio, 0.0f, 1.0f);
	LineThickness = FMath::Clamp(InLineThickness, 1.0f, 12.0f);
	InvalidateLayoutAndVolatility();
}

int32 UFF_TargetShootingTargetVisual::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	if (LocalSize.X <= UE_SMALL_NUMBER || LocalSize.Y <= UE_SMALL_NUMBER)
	{
		return BaseLayer;
	}

	constexpr int32 SegmentCount = 64;
	auto PaintEllipse = [&](const float RadiusRatio,
		const FLinearColor& Color,
		const float Thickness,
		const int32 RingLayer)
	{
		TArray<FVector2f> Points;
		Points.Reserve(SegmentCount + 1);
		const FVector2f Center(
			static_cast<float>(LocalSize.X * 0.5),
			static_cast<float>(LocalSize.Y * 0.5));
		const FVector2f Radius(
			FMath::Max(0.0f, static_cast<float>(LocalSize.X * 0.5) - Thickness * 0.5f),
			FMath::Max(0.0f, static_cast<float>(LocalSize.Y * 0.5) - Thickness * 0.5f));
		for (int32 Index = 0; Index <= SegmentCount; ++Index)
		{
			const float Angle = 2.0f * UE_PI * static_cast<float>(Index) / SegmentCount;
			Points.Emplace(
				Center.X + Radius.X * RadiusRatio * FMath::Cos(Angle),
				Center.Y + Radius.Y * RadiusRatio * FMath::Sin(Angle));
		}
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			RingLayer,
			AllottedGeometry.ToPaintGeometry(),
			MoveTemp(Points),
			ESlateDrawEffect::None,
			Color * InWidgetStyle.GetColorAndOpacityTint(),
			true,
			Thickness);
	};
	auto PaintEllipseArc = [&](const float RadiusRatio,
		const float ArcRatio,
		const FLinearColor& Color,
		const float Thickness,
		const int32 RingLayer)
	{
		const float SafeArcRatio = FMath::Clamp(ArcRatio, 0.0f, 1.0f);
		if (SafeArcRatio <= UE_SMALL_NUMBER)
		{
			return;
		}

		const int32 ArcSegmentCount = FMath::Clamp(
			FMath::CeilToInt(static_cast<float>(SegmentCount) * SafeArcRatio),
			2,
			SegmentCount);
		TArray<FVector2f> Points;
		Points.Reserve(ArcSegmentCount + 1);
		const FVector2f Center(
			static_cast<float>(LocalSize.X * 0.5),
			static_cast<float>(LocalSize.Y * 0.5));
		const FVector2f Radius(
			FMath::Max(0.0f, static_cast<float>(LocalSize.X * 0.5) - Thickness * 0.5f),
			FMath::Max(0.0f, static_cast<float>(LocalSize.Y * 0.5) - Thickness * 0.5f));
		for (int32 Index = 0; Index <= ArcSegmentCount; ++Index)
		{
			const float ArcAlpha = static_cast<float>(Index) / ArcSegmentCount;
			const float Angle = -0.5f * UE_PI
				+ 2.0f * UE_PI * SafeArcRatio * ArcAlpha;
			Points.Emplace(
				Center.X + Radius.X * RadiusRatio * FMath::Cos(Angle),
				Center.Y + Radius.Y * RadiusRatio * FMath::Sin(Angle));
		}
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			RingLayer,
			AllottedGeometry.ToPaintGeometry(),
			MoveTemp(Points),
			ESlateDrawEffect::None,
			Color * InWidgetStyle.GetColorAndOpacityTint(),
			true,
			Thickness);
	};

	// Dark under-strokes isolate all three scoring zones from detailed art while
	// leaving the outer edge equal to the authoritative hit ellipse.
	const FLinearColor TargetOutline(0.002f, 0.008f, 0.018f, 0.96f);
	PaintEllipse(1.0f, TargetOutline, LineThickness * 3.2f, BaseLayer + 1);
	PaintEllipse(1.0f, OuterColor, LineThickness, BaseLayer + 2);
	PaintEllipse(PrecisionRatio, TargetOutline, LineThickness * 2.6f, BaseLayer + 3);
	PaintEllipse(PrecisionRatio, PrecisionColor, LineThickness * 0.86f, BaseLayer + 4);
	PaintEllipse(BullseyeRatio, TargetOutline, LineThickness * 2.8f, BaseLayer + 5);
	PaintEllipse(BullseyeRatio, BullseyeColor, LineThickness * 1.15f, BaseLayer + 6);
	// The bright section drains clockwise from twelve o'clock. It shares the exact
	// outer edge instead of adding a larger halo that could imply a false hit area.
	PaintEllipseArc(
		1.0f,
		RemainingLifeRatio,
		CountdownColor,
		FMath::Max(1.0f, LineThickness * 0.48f),
		BaseLayer + 7);

	// Crisp cardinal acquisition marks make the target readable against detailed art.
	// They remain inside the authoritative outer ellipse and never imply a larger hit area.
	const FVector2f Center(
		static_cast<float>(LocalSize.X * 0.5),
		static_cast<float>(LocalSize.Y * 0.5));
	const FVector2f Radius(
		FMath::Max(0.0f, static_cast<float>(LocalSize.X * 0.5) - LineThickness),
		FMath::Max(0.0f, static_cast<float>(LocalSize.Y * 0.5) - LineThickness));
	auto PaintTick = [&](const FVector2f& Start, const FVector2f& End, const int32 TickLayer)
	{
		TArray<FVector2f> Points { Start, End };
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			TickLayer,
			AllottedGeometry.ToPaintGeometry(),
			MoveTemp(Points),
			ESlateDrawEffect::None,
			BullseyeColor * InWidgetStyle.GetColorAndOpacityTint(),
			true,
			FMath::Max(1.0f, LineThickness * 0.72f));
	};
	constexpr float TickInnerRatio = 0.76f;
	PaintTick(
		FVector2f(Center.X - Radius.X, Center.Y),
		FVector2f(Center.X - Radius.X * TickInnerRatio, Center.Y),
		BaseLayer + 8);
	PaintTick(
		FVector2f(Center.X + Radius.X * TickInnerRatio, Center.Y),
		FVector2f(Center.X + Radius.X, Center.Y),
		BaseLayer + 8);
	PaintTick(
		FVector2f(Center.X, Center.Y - Radius.Y),
		FVector2f(Center.X, Center.Y - Radius.Y * TickInnerRatio),
		BaseLayer + 8);
	PaintTick(
		FVector2f(Center.X, Center.Y + Radius.Y * TickInnerRatio),
		FVector2f(Center.X, Center.Y + Radius.Y),
		BaseLayer + 8);
	const float CenterTick = FMath::Max(3.0f, FMath::Min(LocalSize.X, LocalSize.Y) * 0.055f);
	PaintTick(
		FVector2f(Center.X - CenterTick, Center.Y),
		FVector2f(Center.X + CenterTick, Center.Y),
		BaseLayer + 9);
	PaintTick(
		FVector2f(Center.X, Center.Y - CenterTick),
		FVector2f(Center.X, Center.Y + CenterTick),
		BaseLayer + 9);
	PaintTick(
		FVector2f(Center.X - LineThickness, Center.Y),
		FVector2f(Center.X + LineThickness, Center.Y),
		BaseLayer + 10);
	return BaseLayer + 10;
}

void UFF_TargetShootingProjectileVisual::Configure(
	const FVector2D& InDirection,
	const FLinearColor& InCoreColor,
	const FLinearColor& InGlowColor,
	const float InCoreThickness,
	const float InTrailLength)
{
	const FVector2D SafeDirection = InDirection.IsNearlyZero()
		|| !FMath::IsFinite(InDirection.X)
		|| !FMath::IsFinite(InDirection.Y)
		? FVector2D(0.0f, -1.0f)
		: InDirection.GetSafeNormal();
	if (Direction.Equals(SafeDirection)
		&& CoreColor.Equals(InCoreColor)
		&& GlowColor.Equals(InGlowColor)
		&& FMath::IsNearlyEqual(CoreThickness, InCoreThickness)
		&& FMath::IsNearlyEqual(TrailLength, InTrailLength))
	{
		return;
	}

	Direction = SafeDirection;
	CoreColor = InCoreColor;
	GlowColor = InGlowColor;
	CoreThickness = FMath::Clamp(InCoreThickness, 1.0f, 12.0f);
	TrailLength = FMath::Clamp(InTrailLength, 8.0f, 96.0f);
	InvalidateLayoutAndVolatility();
}

int32 UFF_TargetShootingProjectileVisual::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	const int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled);
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	if (LocalSize.X <= UE_SMALL_NUMBER || LocalSize.Y <= UE_SMALL_NUMBER)
	{
		return BaseLayer;
	}

	const FVector2f Center(
		static_cast<float>(LocalSize.X * 0.5),
		static_cast<float>(LocalSize.Y * 0.5));
	const FVector2f FlightDirection(
		static_cast<float>(Direction.X),
		static_cast<float>(Direction.Y));
	const FVector2f Perpendicular(-FlightDirection.Y, FlightDirection.X);
	const float SafeTrailLength = FMath::Min(
		TrailLength,
		static_cast<float>(FMath::Min(LocalSize.X, LocalSize.Y) * 0.46));
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();

	auto PaintLine = [&](const FVector2f& Start,
		const FVector2f& End,
		const FLinearColor& Color,
		const float Thickness,
		const int32 LineLayer)
	{
		TArray<FVector2f> Points { Start, End };
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LineLayer,
			AllottedGeometry.ToPaintGeometry(),
			MoveTemp(Points),
			ESlateDrawEffect::None,
			Color * Tint,
			true,
			Thickness);
	};

	const FVector2f TrailStart = Center - FlightDirection * SafeTrailLength;
	const FVector2f TrailEnd = Center - FlightDirection * (CoreThickness * 0.5f);
	FLinearColor SoftGlow = GlowColor;
	SoftGlow.A *= 0.24f;
	PaintLine(TrailStart, TrailEnd, SoftGlow, CoreThickness * 3.4f, BaseLayer + 1);
	PaintLine(TrailStart, Center, GlowColor, CoreThickness, BaseLayer + 2);

	const FVector2f CoreStart = Center - FlightDirection * (CoreThickness * 1.2f);
	const FVector2f CoreEnd = Center + FlightDirection * (CoreThickness * 1.8f);
	PaintLine(CoreStart, CoreEnd, CoreColor, CoreThickness * 1.8f, BaseLayer + 3);
	PaintLine(
		Center - Perpendicular * CoreThickness,
		Center + Perpendicular * CoreThickness,
		CoreColor,
		CoreThickness,
		BaseLayer + 4);

	// A bright circular water capsule reads as a projectile instead of a cursor
	// streak, even during short flights on a high-refresh display.
	constexpr int32 BubbleSegments = 24;
	TArray<FVector2f> Bubble;
	Bubble.Reserve(BubbleSegments + 1);
	const float BubbleRadius = FMath::Max(6.0f, CoreThickness * 1.65f);
	for (int32 Index = 0; Index <= BubbleSegments; ++Index)
	{
		const float Angle = 2.0f * UE_PI * static_cast<float>(Index) / BubbleSegments;
		Bubble.Emplace(
			Center.X + BubbleRadius * FMath::Cos(Angle),
			Center.Y + BubbleRadius * FMath::Sin(Angle));
	}
	FLinearColor BubbleGlow = GlowColor;
	BubbleGlow.A *= 0.92f;
	FSlateDrawElement::MakeLines(
		OutDrawElements,
		BaseLayer + 5,
		AllottedGeometry.ToPaintGeometry(),
		MoveTemp(Bubble),
		ESlateDrawEffect::None,
		BubbleGlow * Tint,
		true,
		FMath::Max(2.0f, CoreThickness * 0.70f));
	PaintLine(
		Center - Perpendicular * BubbleRadius * 0.45f
			- FlightDirection * BubbleRadius * 0.28f,
		Center + Perpendicular * BubbleRadius * 0.10f
			- FlightDirection * BubbleRadius * 0.55f,
		CoreColor,
		FMath::Max(1.5f, CoreThickness * 0.45f),
		BaseLayer + 6);
	return BaseLayer + 6;
}

UFF_TargetShootingScreen::UFF_TargetShootingScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VisualStyle.BackdropTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TargetShootingBackdropPath));
	VisualStyle.WaterBlasterTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TargetShootingWaterBlasterPath));
	VisualStyle.TargetTexture = TSoftObjectPtr<UTexture2D>(
		FSoftObjectPath(TargetShootingTargetPath));
	VisualStyle.Arena3DActorClass = AFF_TargetShootingArena3D::StaticClass();
}

bool UFF_TargetShootingScreen::SetRuntime(UFF_TargetShootingRuntime* InRuntime)
{
	if (!IsValid(InRuntime))
	{
		Runtime = nullptr;
		SetSessionId(FGuid());
		ResetTransientPresentationState();
		RefreshPresentation();
		return false;
	}

	const FFF_TargetShootingSnapshot Snapshot = InRuntime->GetTargetSnapshot();
	const FGuid RuntimeSessionId = Snapshot.Session.Context.SessionId;
	if (!RuntimeSessionId.IsValid())
	{
		return false;
	}

	if (GetSessionId().IsValid() && GetSessionId() != RuntimeSessionId)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] 다른 세션의 TargetShooting runtime 연결을 거부했다. Screen=%s Runtime=%s"),
			*GetName(), *GetSessionId().ToString(), *RuntimeSessionId.ToString());
		return false;
	}

	if (!GetSessionId().IsValid())
	{
		// Direct CreateWidget 사용과 자동화에서도 안전하게 동작한다. 일반 게임 흐름에서는
		// subsystem이 BindRuntime 전에 동일한 SessionId를 먼저 지정한다.
		SetSessionId(RuntimeSessionId);
	}

	if (Runtime != InRuntime)
	{
		ResetTransientPresentationState();
	}
	Runtime = InRuntime;
	ApplySnapshot(Snapshot);
	return true;
}

bool UFF_TargetShootingScreen::BindRuntime(UFF_MinigameRuntime* InRuntime)
{
	return SetRuntime(Cast<UFF_TargetShootingRuntime>(InRuntime));
}

void UFF_TargetShootingScreen::ShowResult(const FFF_MinigameResult& Result)
{
	if (GetSessionId().IsValid() && Result.SessionId != GetSessionId())
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 다른 세션의 결과 표시를 무시했다. Screen=%s Result=%s"),
			*GetName(), *GetSessionId().ToString(), *Result.SessionId.ToString());
		return;
	}

	DisplayedResult = Result;
	bHasResult = true;
	RefreshPresentation();
}

void UFF_TargetShootingScreen::ApplySnapshot(const FFF_TargetShootingSnapshot& Snapshot)
{
	const FGuid SnapshotSessionId = Snapshot.Session.Context.SessionId;
	if (GetSessionId().IsValid() && SnapshotSessionId != GetSessionId())
	{
		return;
	}

	if (Snapshot.TargetSerial > 0 && Snapshot.TargetSerial != PresentedTargetSerial)
	{
		PresentedTargetSerial = Snapshot.TargetSerial;
		PlayMinigameSound(EFF_MinigameAudioEvent::TargetSpawned);
		TargetSpawnPulseRemainingSeconds = FMath::Max(
			0.0f,
			VisualStyle.TargetSpawnPulseSeconds);
	}
	if (Snapshot.ProjectileSerial > 0
		&& Snapshot.ProjectileSerial != PresentedProjectileSerial)
	{
		PresentedProjectileSerial = Snapshot.ProjectileSerial;
		PlayMinigameSound(EFF_MinigameAudioEvent::ShotFired);
		ReticleShotPulseRemainingSeconds = FMath::Max(
			0.0f,
			VisualStyle.ReticleShotPulseSeconds);
		MuzzleFlashRemainingSeconds = FMath::Max(
			0.0f,
			VisualStyle.MuzzleFlashSeconds);
	}
	if (Snapshot.LatestFeedbackSerial > 0
		&& Snapshot.LatestFeedbackSerial != PresentedFeedbackSerial)
	{
		PresentedFeedbackSerial = Snapshot.LatestFeedbackSerial;
		EFF_MinigameAudioEvent AudioEvent = EFF_MinigameAudioEvent::TargetHit;
		if (Snapshot.bLatestFeedbackWasBonusTarget
			&& Snapshot.LatestFeedbackType != EFF_TargetShootingFeedbackType::Miss
			&& Snapshot.LatestFeedbackType != EFF_TargetShootingFeedbackType::Escaped)
		{
			AudioEvent = EFF_MinigameAudioEvent::BonusTargetHit;
		}
		else
		{
			switch (Snapshot.LatestFeedbackType)
			{
			case EFF_TargetShootingFeedbackType::Precision:
				AudioEvent = EFF_MinigameAudioEvent::TargetPrecision;
				break;
			case EFF_TargetShootingFeedbackType::Bullseye:
				AudioEvent = EFF_MinigameAudioEvent::TargetBullseye;
				break;
			case EFF_TargetShootingFeedbackType::Miss:
				AudioEvent = EFF_MinigameAudioEvent::TargetMissed;
				break;
			case EFF_TargetShootingFeedbackType::Escaped:
				AudioEvent = EFF_MinigameAudioEvent::TargetEscaped;
				break;
			default:
				break;
			}
		}
		PlayMinigameSound(AudioEvent);
		FeedbackRemainingSeconds = FMath::Max(
			0.0f,
			VisualStyle.FeedbackDurationSeconds);
		if (Snapshot.LatestFeedbackType != EFF_TargetShootingFeedbackType::Escaped)
		{
			ReticleShotPulseRemainingSeconds = FMath::Max(
				0.0f,
				VisualStyle.ReticleShotPulseSeconds);
		}
	}

	DisplayedSnapshot = Snapshot;
	bHasSnapshot = true;
	RefreshPresentation();
}

TSharedRef<SWidget> UFF_TargetShootingScreen::RebuildWidget()
{
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		if (!BuildNativeWidgetTree())
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[%s] TargetShooting native fallback WidgetTree 생성에 실패했다."), *GetName());
		}
	}

	ResolveBindingsFromWidgetTree();
	bUsingDesignerWidgetTree = !bBuiltNativeWidgetTree && HasUsableDesignerBindings();
	InitializeBindings();
	RefreshPresentation();
	return Super::RebuildWidget();
}

void UFF_TargetShootingScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ResolveBindingsFromWidgetTree();
	InitializeBindings();
	RefreshPresentation();
}

void UFF_TargetShootingScreen::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveBindingsFromWidgetTree();
	InitializeBindings();
	TryInitializeThreeDimensionalArena();
	RefreshPresentation();
}

void UFF_TargetShootingScreen::NativeDestruct()
{
	if (Button_Close)
	{
		Button_Close->OnClicked.RemoveAll(this);
	}
	if (Button_ResultClose)
	{
		Button_ResultClose->OnClicked.RemoveAll(this);
	}

	InitialFocusTarget = nullptr;
	Runtime = nullptr;
	Arena3DActor = nullptr;
	bThreeDimensionalArenaReady = false;
	if (HasMouseCapture() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}
	SetSessionId(FGuid());
	ResetTransientPresentationState();
	if (Border_PlayArea)
	{
		Border_PlayArea->SetCursor(EMouseCursor::Default);
		if (bHasBasePlayAreaBrushColor)
		{
			Border_PlayArea->SetBrushColor(BasePlayAreaBrushColor);
		}
	}
	bHasBasePlayAreaBrushColor = false;
	bBindingsInitialized = false;
	bResultFocusApplied = false;
	Super::NativeDestruct();
}

void UFF_TargetShootingScreen::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const bool bPointerAimActive = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing;
	if (bPointerAimActive && FSlateApplication::IsInitialized())
	{
		// Polling closes the initial-frame and layout-change gaps where no move event
		// is emitted even though the absolute pointer and canvas relationship changed.
		UpdateAimFromScreenPosition(FSlateApplication::Get().GetCursorPos());
	}
	if (!VisualStyle.bUseSingleBackdrop && !bThreeDimensionalArenaReady)
	{
		TryInitializeThreeDimensionalArena();
	}

	if (IsValid(Runtime))
	{
		ApplySnapshot(Runtime->GetTargetSnapshot());
	}
	else
	{
		RefreshTargetLayout();
	}

	const float SafeDeltaSeconds = FMath::Max(0.0f, InDeltaTime);
	TargetSpawnPulseRemainingSeconds = FMath::Max(
		0.0f,
		TargetSpawnPulseRemainingSeconds - SafeDeltaSeconds);
	FeedbackRemainingSeconds = FMath::Max(
		0.0f,
		FeedbackRemainingSeconds - SafeDeltaSeconds);
	ReticleShotPulseRemainingSeconds = FMath::Max(
		0.0f,
		ReticleShotPulseRemainingSeconds - SafeDeltaSeconds);
	MuzzleFlashRemainingSeconds = FMath::Max(
		0.0f,
		MuzzleFlashRemainingSeconds - SafeDeltaSeconds);
	RefreshTargetLayout();
	RefreshProjectilePresentation();
	RefreshWeaponPresentation();
	RefreshFeedbackPresentation();
	RefreshReticlePresentation();
}

FReply UFF_TargetShootingScreen::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	// The modal screen consumes left-clicks even while a result is visible or the
	// runtime is being detached. This prevents the same click from selecting a
	// facility in the world underneath the CommonUI stack.
	if (!bHasSnapshot
		|| bHasResult
		|| DisplayedSnapshot.Session.State != EFF_MinigameSessionState::Playing
		|| !IsValid(Runtime))
	{
		return FReply::Handled();
	}
	UpdateAimFromScreenPosition(InMouseEvent.GetScreenSpacePosition());

	// The button is only a trigger and the target widget is never click-tested. The
	// absolute pointer coordinate is converted to the shared reticle aim, then the
	// projectile follows the rotated weapon ray toward that same coordinate.
	Runtime->FireProjectileFromMuzzle(
		GetSessionId(),
		CalculateWeaponMuzzlePointNormalized(),
		CameraAimPointNormalized,
		DisplayedSnapshot.TargetSerial);
	ApplySnapshot(Runtime->GetTargetSnapshot());

	return FReply::Handled();
}

FReply UFF_TargetShootingScreen::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	const bool bPlaying = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing;
	if (!bPlaying || !Canvas_PlayArea)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	if (UpdateAimFromScreenPosition(InMouseEvent.GetScreenSpacePosition()))
	{
		RefreshCameraAimPresentation();
		RefreshTargetLayout();
		RefreshWeaponPresentation();
		RefreshReticlePresentation();
	}
	return FReply::Handled();
}

void UFF_TargetShootingScreen::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	// Absolute pointer aiming intentionally stops at the play-area boundary. The
	// reticle keeps the last valid clamped coordinate until the pointer returns.
	Super::NativeOnMouseLeave(InMouseEvent);
}

UWidget* UFF_TargetShootingScreen::NativeGetDesiredFocusTarget() const
{
	const bool bSessionFinished = bHasSnapshot
		&& (DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Completed
			|| DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Cancelled);
	if (bHasResult || bSessionFinished)
	{
		return Button_ResultClose ? Button_ResultClose.Get() : Button_Close.Get();
	}

	// The screen itself is focusable (configured by UFF_MinigameScreenBase) and has
	// no activation action. Giving gameplay focus to the close button would let an
	// incidental Enter/Space cancel the live session.
	return const_cast<UFF_TargetShootingScreen*>(this);
}

bool UFF_TargetShootingScreen::BuildNativeWidgetTree()
{
	if (!WidgetTree)
	{
		return false;
	}

	bBuiltNativeWidgetTree = true;
	bUsingDesignerWidgetTree = false;
	const FFMinigameUIStyle::FTheme Theme = FFMinigameUIStyle::GetTheme();

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("Root_NativeTargetShooting"));
	Image_Backdrop = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("Image_Backdrop"));
	UBorder* ScreenWash = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ScreenWash"));
	UCanvasPanel* LayoutCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("Canvas_WaterBlasterHud"));
	UBorder* ArenaFrame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ArenaFrame"));
	Border_PlayArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_PlayArea"));
	Canvas_PlayArea = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("Canvas_PlayArea"));
	Border_Target = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_Target"));
	UOverlay* TargetOverlay = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("Overlay_TargetVisual"));
	Image_Target = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("Image_Target"));

	if (!Root || !Image_Backdrop || !ScreenWash || !LayoutCanvas || !ArenaFrame
		|| !Border_PlayArea || !Canvas_PlayArea || !Border_Target
		|| !TargetOverlay || !Image_Target)
	{
		return false;
	}

	WidgetTree->RootWidget = Root;
	auto StretchOnRoot = [Root](UWidget* Widget) -> bool
	{
		if (UOverlaySlot* Slot = Root->AddChildToOverlay(Widget))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
			return true;
		}
		return false;
	};
	auto PlaceAnchored = [LayoutCanvas](
		UWidget* Widget,
		const FAnchors& Anchors,
		const int32 ZOrder) -> bool
	{
		if (UCanvasPanelSlot* Slot = LayoutCanvas->AddChildToCanvas(Widget))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetOffsets(FMargin(0.0f));
			Slot->SetAlignment(FVector2D::ZeroVector);
			Slot->SetZOrder(ZOrder);
			return true;
		}
		return false;
	};
	auto ConfigureCard = [&Theme](UBorder* Card, const FLinearColor& Color, const FMargin& InPadding)
	{
		FFMinigameUIStyle::ApplyCard(*Card, Color, InPadding, Theme.Line);
		Card->SetHorizontalAlignment(HAlign_Fill);
		Card->SetVerticalAlignment(VAlign_Center);
		Card->SetVisibility(ESlateVisibility::HitTestInvisible);
	};

	Image_Backdrop->SetColorAndOpacity(VisualStyle.BackdropTint);
	Image_Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
	ScreenWash->SetBrushColor(VisualStyle.ScreenWashColor);
	ScreenWash->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!StretchOnRoot(Image_Backdrop)
		|| !StretchOnRoot(ScreenWash)
		|| !StretchOnRoot(LayoutCanvas))
	{
		return false;
	}

	ArenaFrame->SetBrushColor(VisualStyle.bUseSingleBackdrop
		? FLinearColor::Transparent
		: VisualStyle.ArenaFrameColor);
	ArenaFrame->SetPadding(VisualStyle.bUseSingleBackdrop
		? FMargin(0.0f)
		: FMargin(2.0f));
	ArenaFrame->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Border_PlayArea->SetBrushColor(VisualStyle.bUseSingleBackdrop
		? FLinearColor::Transparent
		: VisualStyle.ArenaSurfaceColor);
	Border_PlayArea->SetPadding(FMargin(0.0f));
	Border_PlayArea->SetContent(Canvas_PlayArea);
	ArenaFrame->SetContent(Border_PlayArea);
	if (!PlaceAnchored(ArenaFrame, FAnchors(0.08f, 0.16f, 0.92f, 0.93f), -100))
	{
		return false;
	}

	Border_Target->SetPadding(FMargin(0.0f));
	Border_Target->SetVisibility(ESlateVisibility::HitTestInvisible);
	Image_Target->SetColorAndOpacity(VisualStyle.TargetImageTint);
	Image_Target->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* ImageSlot = TargetOverlay->AddChildToOverlay(Image_Target))
	{
		ImageSlot->SetHorizontalAlignment(HAlign_Fill);
		ImageSlot->SetVerticalAlignment(VAlign_Fill);
	}
	else
	{
		return false;
	}
	Border_Target->SetContent(TargetOverlay);

	UCanvasPanelSlot* TargetSlot = Canvas_PlayArea->AddChildToCanvas(Border_Target);
	if (!TargetSlot)
	{
		return false;
	}
	TargetSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	TargetSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	TargetSlot->SetPosition(FVector2D::ZeroVector);
	TargetSlot->SetSize(FVector2D(112.0f, 112.0f));
	TargetSlot->SetZOrder(1);

	Text_TargetCue = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_TargetCue"));
	if (!Text_TargetCue)
	{
		return false;
	}
	Text_TargetCue->SetText(LOCTEXT("NativeTargetCue", "여기를 쏘세요"));
	Text_TargetCue->SetJustification(ETextJustify::Center);
	Text_TargetCue->SetVisibility(ESlateVisibility::HitTestInvisible);
	StyleNativeText(
		Text_TargetCue,
		VisualStyle.TargetCueFontSize,
		VisualStyle.TargetCueColor);
	if (UCanvasPanelSlot* CueSlot = Canvas_PlayArea->AddChildToCanvas(Text_TargetCue))
	{
		CueSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CueSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		CueSlot->SetPosition(FVector2D(0.0f, -72.0f));
		CueSlot->SetSize(FVector2D(240.0f, 38.0f));
		CueSlot->SetZOrder(7);
	}
	else
	{
		return false;
	}

	Text_ShotFeedback = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_ShotFeedback"));
	Text_Reticle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_Reticle"));
	UTextBlock* RangeLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_RangeLabel"));
	if (!Text_ShotFeedback || !Text_Reticle || !RangeLabel)
	{
		return false;
	}
	Text_ShotFeedback->SetText(LOCTEXT("NativeShotFeedbackPlaceholder", "BULLSEYE  +200"));
	Text_ShotFeedback->SetJustification(ETextJustify::Center);
	Text_ShotFeedback->SetVisibility(ESlateVisibility::Collapsed);
	StyleNativeText(Text_ShotFeedback, 28.0f, VisualStyle.HudAmberColor);
	if (UCanvasPanelSlot* FeedbackSlot = Canvas_PlayArea->AddChildToCanvas(Text_ShotFeedback))
	{
		FeedbackSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		FeedbackSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FeedbackSlot->SetPosition(FVector2D::ZeroVector);
		FeedbackSlot->SetSize(FVector2D(360.0f, 96.0f));
		FeedbackSlot->SetZOrder(10);
	}
	else
	{
		return false;
	}
	Text_Reticle->SetText(FText::FromString(VisualStyle.ReticleGlyph));
	Text_Reticle->SetJustification(ETextJustify::Center);
	Text_Reticle->SetVisibility(ESlateVisibility::Collapsed);
	StyleNativeText(Text_Reticle, VisualStyle.ReticleFontSize, VisualStyle.ReticleColor);
	if (UCanvasPanelSlot* ReticleSlot = Canvas_PlayArea->AddChildToCanvas(Text_Reticle))
	{
		ReticleSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		ReticleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ReticleSlot->SetPosition(FVector2D::ZeroVector);
		ReticleSlot->SetAutoSize(true);
		ReticleSlot->SetZOrder(20);
	}
	else
	{
		return false;
	}
	RangeLabel->SetText(LOCTEXT("RangeLabel", "WATER SHOT RANGE  //  과녁을 조준하세요"));
	StyleNativeText(RangeLabel, 11.0f, VisualStyle.SecondaryTextColor, TEXT("SemiBold"));
	RangeLabel->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* RangeSlot = Canvas_PlayArea->AddChildToCanvas(RangeLabel))
	{
		RangeSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		RangeSlot->SetAlignment(FVector2D::ZeroVector);
		RangeSlot->SetPosition(FVector2D(18.0f, 14.0f));
		RangeSlot->SetAutoSize(true);
		RangeSlot->SetZOrder(30);
	}
	else
	{
		return false;
	}

	UBorder* TitleCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_TitleCard"));
	UVerticalBox* TitleColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("Column_Title"));
	UTextBlock* TitleEyebrow = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_TitleEyebrow"));
	UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_Title"));
	if (!TitleCard || !TitleColumn || !TitleEyebrow || !TitleText)
	{
		return false;
	}
	ConfigureCard(TitleCard, Theme.Header, FMargin(20.0f, 12.0f));
	TitleEyebrow->SetText(LOCTEXT("TitleEyebrow", "BUSAN WATERFRONT FESTIVAL"));
	StyleNativeText(TitleEyebrow, 10.0f, Theme.AccentSoft, TEXT("SemiBold"));
	TitleText->SetText(LOCTEXT("Title", "WATER BLAST"));
	StyleNativeText(TitleText, 27.0f, Theme.White);
	TitleColumn->AddChildToVerticalBox(TitleEyebrow);
	TitleColumn->AddChildToVerticalBox(TitleText);
	TitleCard->SetContent(TitleColumn);
	if (!PlaceAnchored(TitleCard, FAnchors(0.025f, 0.035f, 0.285f, 0.145f), 30))
	{
		return false;
	}

	UBorder* TimerCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_TimerCard"));
	UVerticalBox* TimerColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("Column_Timer"));
	USizeBox* TimerGaugeSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("Size_TimeGauge"));
	Progress_Time = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("Progress_Time"));
	UBorder* ScoreCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ScoreCard"));
	UBorder* HitCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_HitCard"));
	UBorder* PerformanceCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_PerformanceCard"));
	UBorder* StreakCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_StreakCard"));
	Text_RemainingTime = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_RemainingTime"));
	Text_Score = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_Score"));
	Text_HitCount = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_HitCount"));
	Text_MissCount = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_MissCount"));
	Text_Streak = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_Streak"));

	if (!TimerCard || !TimerColumn || !TimerGaugeSize || !Progress_Time
		|| !ScoreCard || !HitCard || !PerformanceCard || !StreakCard
		|| !Text_RemainingTime || !Text_Score
		|| !Text_HitCount || !Text_MissCount || !Text_Streak)
	{
		return false;
	}

	ConfigureCard(TimerCard, FFMinigameUIStyle::WithOpacity(Theme.Surface, 0.96f), FMargin(18.0f, 6.0f));
	ConfigureCard(ScoreCard, FFMinigameUIStyle::WithOpacity(Theme.Surface, 0.96f), FMargin(18.0f, 10.0f));
	ConfigureCard(HitCard, FFMinigameUIStyle::WithOpacity(Theme.SurfaceSunken, 0.96f), FMargin(16.0f, 12.0f));
	ConfigureCard(PerformanceCard, FFMinigameUIStyle::WithOpacity(Theme.SurfaceSunken, 0.96f), FMargin(16.0f, 12.0f));
	ConfigureCard(StreakCard, FFMinigameUIStyle::WithOpacity(Theme.Surface, 0.96f), FMargin(18.0f, 8.0f));
	Text_RemainingTime->SetText(LOCTEXT("NativeTimePlaceholder", "00:30"));
	Text_RemainingTime->SetJustification(ETextJustify::Center);
	StyleNativeText(Text_RemainingTime, 48.0f, Theme.Text);
	Progress_Time->SetPercent(1.0f);
	Progress_Time->SetFillColorAndOpacity(Theme.Header);
	Progress_Time->SetVisibility(ESlateVisibility::HitTestInvisible);
	TimerGaugeSize->SetHeightOverride(5.0f);
	TimerGaugeSize->SetContent(Progress_Time);
	Text_Score->SetText(LOCTEXT("NativeScorePlaceholder", "점수  000000"));
	Text_Score->SetJustification(ETextJustify::Right);
	StyleNativeText(Text_Score, 27.0f, Theme.Warning);
	Text_HitCount->SetText(LOCTEXT("NativeHitsPlaceholder", "명중  0\n정확도  0%"));
	Text_HitCount->SetAutoWrapText(true);
	StyleNativeText(Text_HitCount, 16.0f, Theme.Text, TEXT("SemiBold"));
	Text_MissCount->SetText(LOCTEXT("NativePerformancePlaceholder", "초당  0.0발\n반응  0ms\n빗나감  0"));
	Text_MissCount->SetAutoWrapText(true);
	StyleNativeText(Text_MissCount, 15.0f, Theme.TextMuted, TEXT("SemiBold"));
	Text_Streak->SetText(LOCTEXT("NativeStreakPlaceholder", "연속 x0     최고 0"));
	Text_Streak->SetJustification(ETextJustify::Center);
	StyleNativeText(Text_Streak, 22.0f, Theme.HeaderStrong);
	if (UVerticalBoxSlot* TimerTextSlot = TimerColumn->AddChildToVerticalBox(Text_RemainingTime))
	{
		TimerTextSlot->SetHorizontalAlignment(HAlign_Fill);
		TimerTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	if (UVerticalBoxSlot* GaugeSlot = TimerColumn->AddChildToVerticalBox(TimerGaugeSize))
	{
		GaugeSlot->SetHorizontalAlignment(HAlign_Fill);
		GaugeSlot->SetPadding(FMargin(12.0f, 0.0f, 12.0f, 4.0f));
	}
	TimerCard->SetContent(TimerColumn);
	ScoreCard->SetContent(Text_Score);
	HitCard->SetContent(Text_HitCount);
	PerformanceCard->SetContent(Text_MissCount);
	StreakCard->SetContent(Text_Streak);
	if (!PlaceAnchored(TimerCard, FAnchors(0.405f, 0.025f, 0.605f, 0.150f), 31)
		|| !PlaceAnchored(ScoreCard, FAnchors(0.715f, 0.035f, 0.910f, 0.145f), 30)
		|| !PlaceAnchored(HitCard, FAnchors(0.080f, 0.835f, 0.255f, 0.925f), 30)
		|| !PlaceAnchored(PerformanceCard, FAnchors(0.745f, 0.835f, 0.920f, 0.925f), 30)
		|| !PlaceAnchored(StreakCard, FAnchors(0.665f, 0.175f, 0.900f, 0.235f), 30))
	{
		return false;
	}

	UBorder* HintCard = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_InputHint"));
	UTextBlock* HintText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_InputHint"));
	if (!HintCard || !HintText)
	{
		return false;
	}
	ConfigureCard(HintCard, FFMinigameUIStyle::WithOpacity(Theme.SurfaceSunken, 0.96f), FMargin(16.0f, 8.0f));
	HintText->SetText(LOCTEXT("InputHint", "마우스로 총기 회전  ·  좌클릭 발사  ·  ESC 나가기"));
	HintText->SetJustification(ETextJustify::Center);
	StyleNativeText(HintText, 13.0f, Theme.TextMuted, TEXT("SemiBold"));
	HintCard->SetContent(HintText);
	if (!PlaceAnchored(HintCard, FAnchors(0.350f, 0.940f, 0.650f, 0.985f), 30))
	{
		return false;
	}
	HintCard->SetVisibility(ESlateVisibility::Collapsed);

	Panel_Result = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Panel_Result"));
	UOverlay* ResultStage = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(), TEXT("Overlay_ResultStage"));
	USizeBox* ResultSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("Size_ResultCard"));
	UBorder* ResultFrame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ResultFrame"));
	UBorder* ResultSurface = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Border_ResultSurface"));
	UVerticalBox* ResultColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("Column_Result"));
	UTextBlock* ResultEyebrow = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_ResultEyebrow"));
	Text_Result = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_Result"));
	USizeBox* ResultCtaSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("Size_ResultCta"));
	Button_ResultClose = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("Button_ResultClose"));
	NativeResultCloseLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_ResultCloseLabel"));
	if (!Panel_Result || !ResultStage || !ResultSize || !ResultFrame || !ResultSurface
		|| !ResultColumn || !ResultEyebrow || !Text_Result || !ResultCtaSize
		|| !Button_ResultClose || !NativeResultCloseLabel)
	{
		return false;
	}

	Panel_Result->SetBrushColor(Theme.Scrim);
	Panel_Result->SetPadding(FMargin(0.0f));
	Panel_Result->SetContent(ResultStage);
	ResultSize->SetWidthOverride(820.0f);
	ResultSize->SetHeightOverride(500.0f);
	ResultSize->SetContent(ResultFrame);
	ResultFrame->SetBrushColor(Theme.LineStrong);
	ResultFrame->SetPadding(FMargin(2.0f));
	ResultFrame->SetContent(ResultSurface);
	FFMinigameUIStyle::ApplyCard(
		*ResultSurface, Theme.Surface, FMargin(48.0f, 40.0f), Theme.LineStrong, 10.0f);
	ResultSurface->SetContent(ResultColumn);
	ResultEyebrow->SetText(LOCTEXT("ResultEyebrow", "WATER BLAST  //  플레이 결과"));
	ResultEyebrow->SetJustification(ETextJustify::Center);
	StyleNativeText(ResultEyebrow, 13.0f, Theme.HeaderStrong, TEXT("SemiBold"));
	Text_Result->SetJustification(ETextJustify::Center);
	Text_Result->SetAutoWrapText(true);
	StyleNativeText(Text_Result, 26.0f, Theme.Text);
	if (UVerticalBoxSlot* EyebrowSlot = ResultColumn->AddChildToVerticalBox(ResultEyebrow))
	{
		EyebrowSlot->SetHorizontalAlignment(HAlign_Center);
		EyebrowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	}
	if (UVerticalBoxSlot* ResultTextSlot = ResultColumn->AddChildToVerticalBox(Text_Result))
	{
		ResultTextSlot->SetHorizontalAlignment(HAlign_Fill);
		ResultTextSlot->SetVerticalAlignment(VAlign_Center);
		ResultTextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	ResultCtaSize->SetWidthOverride(260.0f);
	ResultCtaSize->SetHeightOverride(54.0f);
	NativeResultCloseLabel->SetText(LOCTEXT("NativeResultCloseButton", "확인  [ESC]"));
	NativeResultCloseLabel->SetJustification(ETextJustify::Center);
	StyleNativeText(NativeResultCloseLabel, 15.0f, Theme.White, TEXT("SemiBold"));
	FFMinigameUIStyle::ApplyButton(*Button_ResultClose, Theme, true);
	Button_ResultClose->SetContent(NativeResultCloseLabel);
	ResultCtaSize->SetContent(Button_ResultClose);
	if (UVerticalBoxSlot* ResultCtaSlot = ResultColumn->AddChildToVerticalBox(ResultCtaSize))
	{
		ResultCtaSlot->SetHorizontalAlignment(HAlign_Center);
		ResultCtaSlot->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));
	}
	else
	{
		return false;
	}
	if (UOverlaySlot* ResultCardSlot = ResultStage->AddChildToOverlay(ResultSize))
	{
		ResultCardSlot->SetHorizontalAlignment(HAlign_Center);
		ResultCardSlot->SetVerticalAlignment(VAlign_Center);
	}
	else
	{
		return false;
	}
	Panel_Result->SetVisibility(ESlateVisibility::Collapsed);
	if (!PlaceAnchored(Panel_Result, FAnchors(0.0f, 0.0f, 1.0f, 1.0f), 100))
	{
		return false;
	}

	Button_Close = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("Button_Close"));
	NativeCloseLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("Text_CloseLabel"));
	if (!Button_Close || !NativeCloseLabel)
	{
		return false;
	}

	NativeCloseLabel->SetText(LOCTEXT("NativeClose", "나가기  [ESC]"));
	NativeCloseLabel->SetJustification(ETextJustify::Center);
	StyleNativeText(NativeCloseLabel, 13.0f, Theme.Text, TEXT("SemiBold"));
	Button_Close->SetContent(NativeCloseLabel);
	FFMinigameUIStyle::ApplyButton(*Button_Close, Theme, false);
	// UButton is focusable by default; keeping that default gives keyboard and
	// gamepad users a deterministic close target without version-specific setters.
	if (!PlaceAnchored(Button_Close, FAnchors(0.925f, 0.050f, 0.982f, 0.110f), 110))
	{
		return false;
	}

	return HasUsableDesignerBindings();
}

void UFF_TargetShootingScreen::ResolveBindingsFromWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	Border_PlayArea = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_PlayArea")));
	Image_Backdrop = Cast<UImage>(WidgetTree->FindWidget(TEXT("Image_Backdrop")));
	Canvas_PlayArea = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("Canvas_PlayArea")));
	Viewport_Arena3D = Cast<UViewport>(WidgetTree->FindWidget(TEXT("Viewport_Arena3D")));
	Border_Target = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_Target")));
	Image_Target = Cast<UImage>(WidgetTree->FindWidget(TEXT("Image_Target")));
	TargetVisual = Cast<UFF_TargetShootingTargetVisual>(
		WidgetTree->FindWidget(TEXT("TargetVisual_AuthoritativeRings")));
	ProjectileVisual = Cast<UFF_TargetShootingProjectileVisual>(
		WidgetTree->FindWidget(TEXT("ProjectileVisual_Tracer")));
	Image_WaterBlaster = Cast<UImage>(
		WidgetTree->FindWidget(TEXT("Image_WaterBlaster")));
	Text_MuzzleFlash = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("Text_MuzzleFlash")));
	Text_WeaponStatus = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("Text_WeaponStatus")));
	Text_TargetCue = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("Text_TargetCue")));
	Text_ShotFeedback = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_ShotFeedback")));
	Text_Reticle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Reticle")));
	Text_RemainingTime = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_RemainingTime")));
	Progress_Time = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("Progress_Time")));
	Text_Score = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Score")));
	Text_HitCount = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_HitCount")));
	Text_MissCount = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_MissCount")));
	Text_Streak = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Streak")));
	Panel_Result = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Panel_Result")));
	Text_Result = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_Result")));
	Button_Close = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_Close")));
	Button_ResultClose = Cast<UButton>(WidgetTree->FindWidget(TEXT("Button_ResultClose")));
	NativeCloseLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_CloseLabel")));
	NativeResultCloseLabel = Cast<UTextBlock>(
		WidgetTree->FindWidget(TEXT("Text_ResultCloseLabel")));
}

bool UFF_TargetShootingScreen::HasUsableDesignerBindings() const
{
	if (!Border_PlayArea || !Canvas_PlayArea || !Border_Target
		|| !Text_RemainingTime || !Text_Score || !Text_HitCount
		|| !Text_MissCount || !Text_Streak || !Panel_Result
		|| !Text_Result || !Button_Close)
	{
		return false;
	}

	return Border_PlayArea->GetContent() == Canvas_PlayArea
		&& Border_Target->GetParent() == Canvas_PlayArea
		&& Cast<UCanvasPanelSlot>(Border_Target->Slot) != nullptr;
}

void UFF_TargetShootingScreen::InitializeBindings()
{
	if (bBindingsInitialized)
	{
		return;
	}

	if (!HasUsableDesignerBindings())
	{
		if (WidgetTree && WidgetTree->RootWidget)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("[%s] TargetShooting WBP의 BindWidget 이름, 타입 또는 play-area 계층이 올바르지 않다."),
				*GetName());
		}
		return;
	}

	Border_Target->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Image_Backdrop)
	{
		Image_Backdrop->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (!bUsingDesignerWidgetTree)
		{
			Image_Backdrop->SetColorAndOpacity(VisualStyle.BackdropTint);
			if (UTexture2D* BackdropTexture = LoadPreferredTexture(
				VisualStyle.BackdropTexture,
				TargetShootingBackdropPath))
			{
				Image_Backdrop->SetBrushFromTexture(BackdropTexture, true);
			}
		}
	}
	if (Image_WaterBlaster)
	{
		Image_WaterBlaster->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (!bUsingDesignerWidgetTree)
		{
			Image_WaterBlaster->SetColorAndOpacity(VisualStyle.WaterBlasterTint);
			if (UTexture2D* WaterBlasterTexture = LoadPreferredTexture(
				VisualStyle.WaterBlasterTexture,
				TargetShootingWaterBlasterPath))
			{
				Image_WaterBlaster->SetBrushFromTexture(WaterBlasterTexture, true);
			}
		}
	}
	if (!Image_Target)
	{
		Image_Target = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), TEXT("Image_Target"));
	}
	if (!TargetVisual)
	{
		TargetVisual = WidgetTree->ConstructWidget<UFF_TargetShootingTargetVisual>(
			UFF_TargetShootingTargetVisual::StaticClass(),
			TEXT("TargetVisual_AuthoritativeRings"));
	}
	if (Image_Target && TargetVisual)
	{
		UOverlay* TargetOverlay = Cast<UOverlay>(Border_Target->GetContent());
		if (!TargetOverlay)
		{
			TargetOverlay = WidgetTree->ConstructWidget<UOverlay>(
				UOverlay::StaticClass(), TEXT("Overlay_TargetVisual"));
			Border_Target->SetContent(TargetOverlay);
		}
		if (Image_Target->GetParent() != TargetOverlay)
		{
			Image_Target->RemoveFromParent();
			if (UOverlaySlot* ImageSlot = TargetOverlay->AddChildToOverlay(Image_Target))
			{
				ImageSlot->SetHorizontalAlignment(HAlign_Fill);
				ImageSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
		if (TargetVisual->GetParent() != TargetOverlay)
		{
			TargetVisual->RemoveFromParent();
			if (UOverlaySlot* RingSlot = TargetOverlay->AddChildToOverlay(TargetVisual))
			{
				RingSlot->SetHorizontalAlignment(HAlign_Fill);
				RingSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
		Image_Target->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (!bUsingDesignerWidgetTree)
		{
			Image_Target->SetColorAndOpacity(VisualStyle.TargetImageTint);
			if (UTexture2D* TargetTexture = LoadPreferredTexture(
				VisualStyle.TargetTexture,
				TargetShootingTargetPath))
			{
				Image_Target->SetBrushFromTexture(TargetTexture, true);
			}
		}
		// A production WBP owns the complete target appearance through Image_Target.
		// The paint-only rings remain available only for the asset-free native fallback.
		TargetVisual->SetVisibility(bUsingDesignerWidgetTree
			? ESlateVisibility::Collapsed
			: ESlateVisibility::HitTestInvisible);
		if (!bUsingDesignerWidgetTree)
		{
			Border_Target->SetPadding(FMargin(0.0f));
			FSlateBrush NoBackground;
			NoBackground.DrawAs = ESlateBrushDrawType::NoDrawType;
			Border_Target->SetBrush(NoBackground);
		}
	}
	EnsureTransientPresentationWidgets();
	if (UWidget* ArenaFrame = WidgetTree->FindWidget(TEXT("Border_ArenaFrame")))
	{
		if (!bUsingDesignerWidgetTree)
		{
			if (UBorder* ArenaFrameBorder = Cast<UBorder>(ArenaFrame))
			{
				ArenaFrameBorder->SetBrushColor(VisualStyle.bUseSingleBackdrop
					? FLinearColor::Transparent
					: VisualStyle.ArenaFrameColor);
				ArenaFrameBorder->SetPadding(VisualStyle.bUseSingleBackdrop
					? FMargin(0.0f)
					: FMargin(2.0f));
			}
		}
		if (UCanvasPanelSlot* ArenaFrameSlot = Cast<UCanvasPanelSlot>(ArenaFrame->Slot))
		{
			// UViewport renders a separate scene family. Keeping its containing frame
			// far below every HUD card prevents a late viewport draw from covering UI.
			ArenaFrameSlot->SetZOrder(-100);
		}
	}
	if (!bUsingDesignerWidgetTree)
	{
		Border_PlayArea->SetBrushColor(VisualStyle.bUseSingleBackdrop
			? FLinearColor::Transparent
			: VisualStyle.ArenaSurfaceColor);
	}
	TryInitializeThreeDimensionalArena();
	if (!bHasBasePlayAreaBrushColor)
	{
		BasePlayAreaBrushColor = Border_PlayArea->GetBrushColor();
		bHasBasePlayAreaBrushColor = true;
	}
	Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	if (Button_ResultClose)
	{
		Button_ResultClose->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
	InitialFocusTarget = this;
	bBindingsInitialized = true;
}

void UFF_TargetShootingScreen::EnsureTransientPresentationWidgets()
{
	if (!WidgetTree || !Canvas_PlayArea)
	{
		return;
	}

	auto EnsureDirectCanvasChild = [this](
		UWidget* Widget,
		const FVector2D& DefaultSize,
		const int32 ZOrder,
		const bool bAutoSize) -> UCanvasPanelSlot*
	{
		if (!Widget)
		{
			return nullptr;
		}

		UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
		if (Widget->GetParent() != Canvas_PlayArea || !CanvasSlot)
		{
			// Optional designer widgets are runtime-positioned in normalized play-area
			// coordinates. Reparenting them here keeps WBP layout edits from silently
			// separating the visible reticle/feedback from the authoritative hit canvas.
			Widget->RemoveFromParent();
			CanvasSlot = Canvas_PlayArea->AddChildToCanvas(Widget);
			if (CanvasSlot)
			{
				CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
				CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				CanvasSlot->SetPosition(FVector2D::ZeroVector);
				CanvasSlot->SetSize(DefaultSize);
			}
		}

		if (CanvasSlot)
		{
			CanvasSlot->SetZOrder(ZOrder);
			CanvasSlot->SetAutoSize(bAutoSize);
		}
		return CanvasSlot;
	};

	if (!Viewport_Arena3D)
	{
		Viewport_Arena3D = WidgetTree->ConstructWidget<UViewport>(
			UViewport::StaticClass(),
			TEXT("Viewport_Arena3D"));
	}
	if (UCanvasPanelSlot* ArenaSlot = EnsureDirectCanvasChild(
		Viewport_Arena3D,
		FVector2D::ZeroVector,
		-10,
		false))
	{
		ArenaSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		ArenaSlot->SetOffsets(FMargin(0.0f));
		ArenaSlot->SetAlignment(FVector2D::ZeroVector);
	}
	if (Viewport_Arena3D)
	{
		Viewport_Arena3D->SetBackgroundColor(VisualStyle.bUseSingleBackdrop
			? FLinearColor::Transparent
			: VisualStyle.Arena3DBackgroundColor);
		Viewport_Arena3D->SetVisibility(ESlateVisibility::Collapsed);
		Viewport_Arena3D->SetIsEnabled(true);
	}

	if (!Text_TargetCue)
	{
		Text_TargetCue = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("Text_TargetCue"));
	}
	EnsureDirectCanvasChild(Text_TargetCue, FVector2D(240.0f, 38.0f), 7, false);
	if (Text_TargetCue)
	{
		Text_TargetCue->SetText(LOCTEXT("TargetCue", "여기를 쏘세요"));
		if (!bUsingDesignerWidgetTree)
		{
			Text_TargetCue->SetJustification(ETextJustify::Center);
			Text_TargetCue->SetShadowOffset(FVector2D(2.0f, 2.0f));
			Text_TargetCue->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.92f));
			StyleNativeText(
				Text_TargetCue,
				FMath::Clamp(VisualStyle.TargetCueFontSize, 10.0f, 32.0f),
				VisualStyle.TargetCueColor);
		}
		Text_TargetCue->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!Text_ShotFeedback)
	{
		Text_ShotFeedback = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("Text_ShotFeedback"));
	}
	EnsureDirectCanvasChild(Text_ShotFeedback, FVector2D(320.0f, 90.0f), 10, false);
	if (Text_ShotFeedback)
	{
		if (!bUsingDesignerWidgetTree)
		{
			Text_ShotFeedback->SetJustification(ETextJustify::Center);
			Text_ShotFeedback->SetShadowOffset(FVector2D(2.0f, 2.0f));
			Text_ShotFeedback->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
			StyleNativeText(Text_ShotFeedback, FMath::Max(8.0f, VisualStyle.FeedbackFontSize));
		}
		Text_ShotFeedback->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!ProjectileVisual)
	{
		ProjectileVisual = WidgetTree->ConstructWidget<UFF_TargetShootingProjectileVisual>(
			UFF_TargetShootingProjectileVisual::StaticClass(),
			TEXT("ProjectileVisual_Tracer"));
	}
	const float ProjectileSize = FMath::Clamp(
		VisualStyle.ProjectileVisualSize,
		64.0f,
		220.0f);
	EnsureDirectCanvasChild(
		ProjectileVisual,
		FVector2D(ProjectileSize, ProjectileSize),
		18,
		false);
	if (ProjectileVisual)
	{
		ProjectileVisual->SetVisibility(ESlateVisibility::Collapsed);
		ProjectileVisual->Configure(
			FVector2D(0.0f, -1.0f),
			VisualStyle.ProjectileCoreColor,
			VisualStyle.ProjectileGlowColor,
			VisualStyle.ProjectileCoreThickness,
			VisualStyle.ProjectileTrailLength);
	}

	if (!Image_WaterBlaster)
	{
		Image_WaterBlaster = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			TEXT("Image_WaterBlaster"));
	}
	const float WaterBlasterSize = FMath::Clamp(
		VisualStyle.WaterBlasterVisualSize,
		180.0f,
		520.0f);
	if (UCanvasPanelSlot* WaterBlasterSlot = EnsureDirectCanvasChild(
		Image_WaterBlaster,
		FVector2D(WaterBlasterSize, WaterBlasterSize),
		15,
		false))
	{
		WaterBlasterSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		WaterBlasterSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		WaterBlasterSlot->SetPosition(FVector2D(
			0.0f,
			FMath::Clamp(VisualStyle.WaterBlasterBottomOffsetPixels, 0.0f, 80.0f)));
		WaterBlasterSlot->SetSize(FVector2D(WaterBlasterSize, WaterBlasterSize));
	}
	if (Image_WaterBlaster)
	{
		Image_WaterBlaster->SetVisibility(ESlateVisibility::Collapsed);
		if (!bUsingDesignerWidgetTree)
		{
			Image_WaterBlaster->SetColorAndOpacity(VisualStyle.WaterBlasterTint);
			if (UTexture2D* WaterBlasterTexture = LoadPreferredTexture(
				VisualStyle.WaterBlasterTexture,
				TargetShootingWaterBlasterPath))
			{
				Image_WaterBlaster->SetBrushFromTexture(WaterBlasterTexture, true);
			}
		}
	}

	if (!Text_MuzzleFlash)
	{
		Text_MuzzleFlash = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("Text_MuzzleFlash"));
	}
	EnsureDirectCanvasChild(Text_MuzzleFlash, FVector2D(96.0f, 96.0f), 17, false);
	if (Text_MuzzleFlash)
	{
		Text_MuzzleFlash->SetText(FText::FromString(TEXT("+")));
		if (!bUsingDesignerWidgetTree)
		{
			Text_MuzzleFlash->SetJustification(ETextJustify::Center);
			Text_MuzzleFlash->SetColorAndOpacity(FSlateColor(VisualStyle.MuzzleFlashColor));
			Text_MuzzleFlash->SetShadowOffset(FVector2D(0.0f, 0.0f));
			Text_MuzzleFlash->SetShadowColorAndOpacity(FLinearColor::Transparent);
			StyleNativeText(Text_MuzzleFlash, 54.0f, VisualStyle.MuzzleFlashColor);
		}
		Text_MuzzleFlash->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!Text_WeaponStatus)
	{
		Text_WeaponStatus = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("Text_WeaponStatus"));
	}
	if (UCanvasPanelSlot* WeaponStatusSlot = EnsureDirectCanvasChild(
		Text_WeaponStatus,
		FVector2D(440.0f, 42.0f),
		19,
		false))
	{
		WeaponStatusSlot->SetAnchors(FAnchors(0.5f, 0.075f));
		WeaponStatusSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		WeaponStatusSlot->SetPosition(FVector2D::ZeroVector);
		WeaponStatusSlot->SetSize(FVector2D(440.0f, 42.0f));
	}
	if (Text_WeaponStatus)
	{
		if (!bUsingDesignerWidgetTree)
		{
			Text_WeaponStatus->SetJustification(ETextJustify::Center);
			StyleNativeText(Text_WeaponStatus, 16.0f, VisualStyle.HudAmberColor, TEXT("SemiBold"));
		}
		Text_WeaponStatus->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!Text_Reticle)
	{
		Text_Reticle = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("Text_Reticle"));
	}
	EnsureDirectCanvasChild(Text_Reticle, FVector2D(64.0f, 64.0f), 20, true);
	if (Text_Reticle)
	{
		Text_Reticle->SetText(FText::FromString(VisualStyle.ReticleGlyph));
		if (!bUsingDesignerWidgetTree)
		{
			Text_Reticle->SetJustification(ETextJustify::Center);
			Text_Reticle->SetColorAndOpacity(FSlateColor(VisualStyle.ReticleColor));
			Text_Reticle->SetShadowOffset(FVector2D(1.0f, 1.0f));
			Text_Reticle->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.70f));
			StyleNativeText(Text_Reticle, FMath::Max(8.0f, VisualStyle.ReticleFontSize));
		}
		Text_Reticle->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UFF_TargetShootingScreen::TryInitializeThreeDimensionalArena()
{
	if (VisualStyle.bUseSingleBackdrop)
	{
		if (Viewport_Arena3D)
		{
			Viewport_Arena3D->SetVisibility(ESlateVisibility::Collapsed);
		}
		bThreeDimensionalArenaReady = false;
		return false;
	}
	if (bThreeDimensionalArenaReady && IsValid(Arena3DActor))
	{
		return true;
	}
	if (!Viewport_Arena3D || !Viewport_Arena3D->GetViewportWorld())
	{
		return false;
	}

	Viewport_Arena3D->SetBackgroundColor(VisualStyle.bUseSingleBackdrop
		? FLinearColor::Transparent
		: VisualStyle.Arena3DBackgroundColor);
	Viewport_Arena3D->SetEnableAdvancedFeatures(true);
	Viewport_Arena3D->SetLightIntensity(FMath::Max(0.1f, VisualStyle.Arena3DKeyLightIntensity));
	Viewport_Arena3D->SetSkyIntensity(FMath::Max(0.0f, VisualStyle.Arena3DSkyIntensity));
	Viewport_Arena3D->SetViewLocation(VisualStyle.Arena3DCameraLocation);
	Viewport_Arena3D->SetShowFlag(TEXT("Grid"), false);
	Viewport_Arena3D->SetShowFlag(TEXT("SelectionOutline"), false);
	Viewport_Arena3D->SetShowFlag(TEXT("MotionBlur"), false);
	Viewport_Arena3D->SetShowFlag(TEXT("OnScreenDebug"), false);

	UClass* ArenaClass = VisualStyle.Arena3DActorClass
		? VisualStyle.Arena3DActorClass.Get()
		: AFF_TargetShootingArena3D::StaticClass();
	Arena3DActor = Cast<AFF_TargetShootingArena3D>(Viewport_Arena3D->Spawn(ArenaClass));
	bThreeDimensionalArenaReady = IsValid(Arena3DActor);
	if (bThreeDimensionalArenaReady)
	{
		Arena3DActor->SetEnvironmentVisible(!VisualStyle.bUseSingleBackdrop);
	}
	RefreshCameraAimPresentation();
	if (!VisualStyle.bUseSingleBackdrop && !bThreeDimensionalArenaReady)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 3D 사격장 생성에 실패해 2D 과녁 폴백을 사용한다."),
			*GetName());
	}
	return bThreeDimensionalArenaReady;
}

void UFF_TargetShootingScreen::ResetTransientPresentationState()
{
	DisplayedSnapshot = FFF_TargetShootingSnapshot();
	DisplayedResult = FFF_MinigameResult();
	bHasSnapshot = false;
	bHasResult = false;
	LastPlayAreaSize = FVector2D::ZeroVector;
	PresentedTargetSerial = 0;
	PresentedProjectileSerial = 0;
	PresentedFeedbackSerial = 0;
	TargetSpawnPulseRemainingSeconds = 0.0f;
	FeedbackRemainingSeconds = 0.0f;
	ReticleShotPulseRemainingSeconds = 0.0f;
	MuzzleFlashRemainingSeconds = 0.0f;
	CameraAimPointNormalized = FVector2D(0.5f, 0.5f);
}

void UFF_TargetShootingScreen::RefreshPresentation()
{
	if (!HasUsableDesignerBindings())
	{
		return;
	}
	if (!VisualStyle.bUseSingleBackdrop && !bThreeDimensionalArenaReady)
	{
		TryInitializeThreeDimensionalArena();
	}

	const float RemainingSeconds = bHasSnapshot
		? FMath::Max(0.0f, DisplayedSnapshot.Session.RemainingSeconds)
		: 0.0f;
	const int32 RemainingWholeSeconds = FMath::CeilToInt(RemainingSeconds);
	const int32 RemainingMinutes = RemainingWholeSeconds / 60;
	const int32 RemainingSecondPart = RemainingWholeSeconds % 60;
	const int32 Score = bHasSnapshot ? DisplayedSnapshot.Session.Score : 0;
	const int32 HitCount = bHasSnapshot ? DisplayedSnapshot.HitCount : 0;
	const int32 MissCount = bHasSnapshot ? DisplayedSnapshot.MissCount : 0;
	const int32 EscapedTargetCount = bHasSnapshot ? DisplayedSnapshot.EscapedTargetCount : 0;
	const int32 CurrentCombo = bHasSnapshot ? DisplayedSnapshot.CurrentCombo : 0;
	const int32 BestCombo = bHasSnapshot ? DisplayedSnapshot.BestCombo : 0;
	const int32 AccuracyPercent = bHasSnapshot
		? FMath::Clamp(FMath::RoundToInt(DisplayedSnapshot.AccuracyPercent), 0, 100)
		: 0;
	const int32 AverageReactionMilliseconds = bHasSnapshot
		? FMath::Max(0, FMath::RoundToInt(DisplayedSnapshot.AverageHitReactionSeconds * 1000.0f))
		: 0;
	FNumberFormattingOptions OneDecimal;
	OneDecimal.SetMinimumFractionalDigits(1);
	OneDecimal.SetMaximumFractionalDigits(1);
	FNumberFormattingOptions TwoDigits;
	TwoDigits.SetMinimumIntegralDigits(2);
	TwoDigits.SetMaximumIntegralDigits(2);
	FNumberFormattingOptions SixDigits;
	SixDigits.SetMinimumIntegralDigits(6);

	Text_RemainingTime->SetText(FText::Format(
		LOCTEXT("RemainingTimeFormat", "{0}:{1}"),
		FText::AsNumber(RemainingMinutes, &TwoDigits),
		FText::AsNumber(RemainingSecondPart, &TwoDigits)));
	if (!bUsingDesignerWidgetTree)
	{
		Text_RemainingTime->SetColorAndOpacity(FSlateColor(
			bHasSnapshot && RemainingSeconds <= 5.0f
				? VisualStyle.HudAmberColor
				: VisualStyle.PrimaryTextColor));
	}
	if (Progress_Time)
	{
		const float TotalSessionSeconds = bHasSnapshot
			? DisplayedSnapshot.Session.ElapsedSeconds + RemainingSeconds
			: 0.0f;
		Progress_Time->SetPercent(TotalSessionSeconds > UE_SMALL_NUMBER
			? FMath::Clamp(RemainingSeconds / TotalSessionSeconds, 0.0f, 1.0f)
			: 0.0f);
		if (!bUsingDesignerWidgetTree)
		{
			Progress_Time->SetFillColorAndOpacity(
				bHasSnapshot && RemainingSeconds <= 5.0f
					? VisualStyle.HudAmberColor
					: VisualStyle.HudAccentColor);
		}
	}
	Text_Score->SetText(FText::Format(
		LOCTEXT("ScoreFormat", "점수  {0}"),
		FText::AsNumber(Score, &SixDigits)));
	Text_HitCount->SetText(FText::Format(
		LOCTEXT("HitCountFormat", "명중  {0}\n정확도  {1}%"),
		FText::AsNumber(HitCount),
		FText::AsNumber(AccuracyPercent)));
	Text_MissCount->SetText(FText::Format(
		LOCTEXT("PerformanceFormat", "초당  {0}발\n반응  {1}ms\n빗나감  {2}   놓침  {3}"),
		FText::AsNumber(bHasSnapshot ? DisplayedSnapshot.HitsPerSecond : 0.0f, &OneDecimal),
		FText::AsNumber(AverageReactionMilliseconds),
		FText::AsNumber(MissCount),
		FText::AsNumber(EscapedTargetCount)));
	Text_Streak->SetText(CurrentCombo >= 5
		? FText::Format(
			LOCTEXT("FeverStreakFormat", "피버!     연속 x{0}     최고 {1}"),
			FText::AsNumber(CurrentCombo),
			FText::AsNumber(BestCombo))
		: FText::Format(
			LOCTEXT("StreakFormat", "연속 x{0}     최고 {1}"),
			FText::AsNumber(CurrentCombo),
			FText::AsNumber(BestCombo)));
	if (!bUsingDesignerWidgetTree)
	{
		Text_Streak->SetColorAndOpacity(FSlateColor(
			CurrentCombo >= 5 ? VisualStyle.HudAmberColor : VisualStyle.HudAccentColor));
	}

	const bool bSessionFinished = bHasSnapshot
		&& (DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Completed
			|| DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Cancelled);
	const bool bShowResult = bHasResult || bSessionFinished;
	UButton* ResultActionButton = Button_ResultClose ? Button_ResultClose.Get() : Button_Close.Get();
	Panel_Result->SetVisibility(bShowResult
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
	InitialFocusTarget = bShowResult
		? static_cast<UWidget*>(ResultActionButton)
		: static_cast<UWidget*>(this);
	if (bShowResult && !bResultFocusApplied && GetOwningLocalPlayer() && ResultActionButton)
	{
		ResultActionButton->SetFocus();
		bResultFocusApplied = true;
	}
	else if (!bShowResult && bResultFocusApplied)
	{
		if (GetOwningLocalPlayer())
		{
			SetFocus();
		}
		bResultFocusApplied = false;
	}

	if (bHasResult)
	{
		const FText Grade = DisplayedResult.GradeText.IsEmpty()
			? LOCTEXT("Completed", "게임 종료")
			: DisplayedResult.GradeText;
		FFormatNamedArguments ResultArguments;
		ResultArguments.Add(TEXT("Grade"), Grade);
		ResultArguments.Add(TEXT("Score"), DisplayedResult.Score);
		ResultArguments.Add(TEXT("Reward"), DisplayedResult.RewardGold);
		ResultArguments.Add(TEXT("Accuracy"), AccuracyPercent);
		ResultArguments.Add(TEXT("Tps"), FText::AsNumber(
			bHasSnapshot ? DisplayedSnapshot.HitsPerSecond : 0.0f,
			&OneDecimal));
		ResultArguments.Add(TEXT("Reaction"), AverageReactionMilliseconds);
		ResultArguments.Add(TEXT("Combo"), BestCombo);
		ResultArguments.Add(TEXT("Escaped"), EscapedTargetCount);
		Text_Result->SetText(FText::Format(
			LOCTEXT(
				"ResultWithRewardFormat",
				"{Grade}\n"
				"최종 점수  {Score}  ·  축제 골드  +{Reward}\n"
				"정확도  {Accuracy}%  ·  초당  {Tps}발  ·  놓침  {Escaped}\n"
				"평균 반응  {Reaction}ms  ·  최고 연속  {Combo}"),
			ResultArguments));
	}
	else if (bSessionFinished)
	{
		const FText ResultTitle = DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Completed
			? LOCTEXT("TimeExpired", "30초 플레이 완료!")
			: LOCTEXT("SessionCancelled", "게임이 종료되었습니다");
		FFormatNamedArguments SnapshotArguments;
		SnapshotArguments.Add(TEXT("Title"), ResultTitle);
		SnapshotArguments.Add(TEXT("Score"), Score);
		SnapshotArguments.Add(TEXT("Accuracy"), AccuracyPercent);
		SnapshotArguments.Add(TEXT("Tps"), FText::AsNumber(
			bHasSnapshot ? DisplayedSnapshot.HitsPerSecond : 0.0f,
			&OneDecimal));
		SnapshotArguments.Add(TEXT("Reaction"), AverageReactionMilliseconds);
		SnapshotArguments.Add(TEXT("Combo"), BestCombo);
		SnapshotArguments.Add(TEXT("Escaped"), EscapedTargetCount);
		Text_Result->SetText(FText::Format(
			LOCTEXT(
				"SnapshotResultFormat",
				"{Title}\n"
				"최종 점수  {Score}\n"
				"정확도  {Accuracy}%  ·  초당  {Tps}발  ·  놓침  {Escaped}\n"
				"평균 반응  {Reaction}ms  ·  최고 연속  {Combo}"),
			SnapshotArguments));
	}

	if (NativeCloseLabel)
	{
		NativeCloseLabel->SetText(LOCTEXT("NativePlayingClose", "나가기  [ESC]"));
	}
	if (Button_Close)
	{
		Button_Close->SetVisibility(
			bShowResult && Button_ResultClose
				? ESlateVisibility::Collapsed
				: ESlateVisibility::Visible);
		if (UCanvasPanelSlot* CloseSlot = Cast<UCanvasPanelSlot>(Button_Close->Slot))
		{
			// Legacy designer WBPs without the dedicated result button keep a safe
			// fallback CTA. Current assets parent Button_ResultClose inside the card.
			CloseSlot->SetAnchors(bShowResult && !Button_ResultClose
				? FAnchors(0.435f, 0.665f, 0.565f, 0.720f)
				: FAnchors(0.925f, 0.050f, 0.982f, 0.110f));
			CloseSlot->SetOffsets(FMargin(0.0f));
			CloseSlot->SetAlignment(FVector2D::ZeroVector);
			CloseSlot->SetZOrder(110);
		}
	}

	const bool bPlaying = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing;
	if (!bPlaying && HasMouseCapture() && FSlateApplication::IsInitialized())
	{
		// Result buttons must immediately regain ordinary pointer input when the
		// FPS session ends; waiting for widget destruction would trap the cursor.
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}
	RefreshCameraAimPresentation();
	if (Viewport_Arena3D)
	{
		Viewport_Arena3D->SetVisibility(bPlaying
			&& !VisualStyle.bUseSingleBackdrop
			&& bThreeDimensionalArenaReady
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (Border_PlayArea)
	{
		const bool bReticleUsesPlayCanvas = Text_Reticle
			&& Text_Reticle->GetParent() == Canvas_PlayArea
			&& Cast<UCanvasPanelSlot>(Text_Reticle->Slot) != nullptr;
		Border_PlayArea->SetCursor(bPlaying
			&& bReticleUsesPlayCanvas
			&& !VisualStyle.ReticleGlyph.IsEmpty()
			? EMouseCursor::None
			: EMouseCursor::Default);
	}

	RefreshTargetLayout();
	RefreshProjectilePresentation();
	RefreshWeaponPresentation();
	RefreshFeedbackPresentation();
	RefreshReticlePresentation();
}

void UFF_TargetShootingScreen::RefreshTargetLayout()
{
	if (!Border_Target || !Canvas_PlayArea)
	{
		return;
	}

	const bool bShowTarget = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing
		&& DisplayedSnapshot.bTargetVisible;
	const bool bShowTargetIn3D = bShowTarget
		&& bThreeDimensionalArenaReady
		&& !VisualStyle.bUseSingleBackdrop
		&& VisualStyle.bRenderTargetIn3D;
	// The exact 2D ring stays on Canvas_PlayArea for pixel-perfect hit readability,
	// while the target plate itself now participates in the perspective 3D scene.
	if (IsValid(Arena3DActor))
	{
		Arena3DActor->SetTargetPresentation(
			bShowTargetIn3D,
			DisplayedSnapshot.TargetCenterNormalized,
			DisplayedSnapshot.TargetRadiusNormalized,
			DisplayedSnapshot.PrecisionRadiusRatio,
			DisplayedSnapshot.BullseyeRadiusRatio,
			DisplayedSnapshot.bBonusTarget
				? VisualStyle.BonusTargetColor
				: VisualStyle.NormalTargetColor,
			VisualStyle.PrecisionColor,
			VisualStyle.BullseyeColor,
			TargetSpawnPulseRemainingSeconds > 0.0f ? 0.75f : 0.0f);
	}
	Border_Target->SetVisibility(bShowTarget
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	if (Text_TargetCue)
	{
		Text_TargetCue->SetVisibility(bShowTarget
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (!bShowTarget)
	{
		Border_Target->SetRenderScale(FVector2D(1.0f, 1.0f));
		Border_Target->SetRenderOpacity(1.0f);
		if (Text_TargetCue)
		{
			Text_TargetCue->SetRenderOpacity(1.0f);
		}
		return;
	}

	UCanvasPanelSlot* TargetSlot = Cast<UCanvasPanelSlot>(Border_Target->Slot);
	if (!TargetSlot)
	{
		return;
	}

	const FVector2D WorldCenter(
		FMath::Clamp(DisplayedSnapshot.TargetCenterNormalized.X, 0.0, 1.0),
		FMath::Clamp(DisplayedSnapshot.TargetCenterNormalized.Y, 0.0, 1.0));
	const FVector2D Radius(
		FMath::Clamp(DisplayedSnapshot.TargetRadiusNormalized.X, 0.0, 0.5),
		FMath::Clamp(DisplayedSnapshot.TargetRadiusNormalized.Y, 0.0, 0.5));
	const float RemainingLifeRatio = DisplayedSnapshot.TargetLifetimeSeconds > UE_SMALL_NUMBER
		? FMath::Clamp(
			DisplayedSnapshot.TargetRemainingSeconds
				/ DisplayedSnapshot.TargetLifetimeSeconds,
			0.0f,
			1.0f)
		: 0.0f;
	const FLinearColor CountdownColor = RemainingLifeRatio <= 0.15f
		? VisualStyle.MissColor
		: (RemainingLifeRatio <= 0.35f
			? VisualStyle.HudAmberColor
			: VisualStyle.TargetCueColor);

	FVector2D Center = WorldCenter;
	ProjectArenaPointToCanvas(WorldCenter, Center);
	TargetSlot->SetAnchors(FAnchors(Center.X, Center.Y));
	TargetSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	TargetSlot->SetPosition(FVector2D::ZeroVector);

	const FVector2D PlayAreaSize = Canvas_PlayArea->GetCachedGeometry().GetLocalSize();
	if (PlayAreaSize.X > UE_SMALL_NUMBER && PlayAreaSize.Y > UE_SMALL_NUMBER)
	{
		FVector2D Left = Center;
		FVector2D Right = Center;
		FVector2D Top = Center;
		FVector2D Bottom = Center;
		ProjectArenaPointToCanvas(
			FVector2D(WorldCenter.X - Radius.X, WorldCenter.Y), Left);
		ProjectArenaPointToCanvas(
			FVector2D(WorldCenter.X + Radius.X, WorldCenter.Y), Right);
		ProjectArenaPointToCanvas(
			FVector2D(WorldCenter.X, WorldCenter.Y - Radius.Y), Top);
		ProjectArenaPointToCanvas(
			FVector2D(WorldCenter.X, WorldCenter.Y + Radius.Y), Bottom);
		const FVector2D ProjectedRadius(
			FMath::Max(FMath::Abs(Center.X - Left.X), FMath::Abs(Right.X - Center.X)),
			FMath::Max(FMath::Abs(Center.Y - Top.Y), FMath::Abs(Bottom.Y - Center.Y)));
		TargetSlot->SetSize(FVector2D(
			ProjectedRadius.X * 2.0f * PlayAreaSize.X,
			ProjectedRadius.Y * 2.0f * PlayAreaSize.Y));
		if (UCanvasPanelSlot* CueSlot = Text_TargetCue
			? Cast<UCanvasPanelSlot>(Text_TargetCue->Slot)
			: nullptr)
		{
			CueSlot->SetAnchors(FAnchors(Center.X, Center.Y));
			CueSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			CueSlot->SetPosition(FVector2D(
				0.0f,
				-ProjectedRadius.Y * PlayAreaSize.Y - 10.0f));
			CueSlot->SetSize(FVector2D(240.0f, 38.0f));
			CueSlot->SetZOrder(7);
		}
		LastPlayAreaSize = PlayAreaSize;
	}

	if (!bUsingDesignerWidgetTree)
	{
		Border_Target->SetBrushColor(DisplayedSnapshot.bBonusTarget
			? VisualStyle.BonusTargetColor
			: VisualStyle.NormalTargetColor);
	}
	if (TargetVisual)
	{
		TargetVisual->Configure(
			DisplayedSnapshot.bBonusTarget
				? VisualStyle.BonusTargetColor
				: VisualStyle.NormalTargetColor,
			VisualStyle.PrecisionColor,
			VisualStyle.BullseyeColor,
			DisplayedSnapshot.PrecisionRadiusRatio,
			DisplayedSnapshot.BullseyeRadiusRatio,
			RemainingLifeRatio,
			CountdownColor,
			VisualStyle.TargetRingThickness);
	}
	if (Text_TargetCue)
	{
		Text_TargetCue->SetText(DisplayedSnapshot.bBonusTarget
			? (RemainingLifeRatio <= 0.35f
				? LOCTEXT("UrgentBonusTargetCue", "보너스 · 서둘러!")
				: LOCTEXT("BonusTargetCue", "보너스 과녁!"))
			: (RemainingLifeRatio <= 0.35f
				? LOCTEXT("UrgentTargetCue", "서둘러!")
				: LOCTEXT("NormalTargetCue", "여기를 쏘세요")));
		if (!bUsingDesignerWidgetTree)
		{
			Text_TargetCue->SetColorAndOpacity(FSlateColor(DisplayedSnapshot.bBonusTarget
				? VisualStyle.BonusTargetColor
				: CountdownColor));
		}
		Text_TargetCue->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Text_TargetCue->SetRenderScale(FVector2D(1.0f, 1.0f));
	}
	// Never scale the target art: the runtime judges the exact ellipse represented
	// by this slot. The spawn fade and countdown arc stay inside the same exact
	// ellipse, preventing visible-but-unclickable edges.
	Border_Target->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Border_Target->SetRenderScale(FVector2D(1.0f, 1.0f));
	float TargetOpacity = 1.0f;
	float CueOpacity = 1.0f;
	if (TargetSpawnPulseRemainingSeconds > 0.0f
		&& VisualStyle.TargetSpawnPulseSeconds > UE_SMALL_NUMBER)
	{
		const float SpawnRemainingAlpha = FMath::Clamp(
			TargetSpawnPulseRemainingSeconds / VisualStyle.TargetSpawnPulseSeconds,
			0.0f,
			1.0f);
		const float SpawnProgress = 1.0f - SpawnRemainingAlpha;
		TargetOpacity *= FMath::Lerp(
			FMath::Clamp(VisualStyle.TargetSpawnMinOpacity, 0.0f, 1.0f),
			1.0f,
			SpawnProgress);
	}
	if (RemainingLifeRatio < 0.35f)
	{
		const float UrgencyAlpha = 1.0f - RemainingLifeRatio / 0.35f;
		const float PulseWave = 0.5f + 0.5f * FMath::Sin(
			DisplayedSnapshot.TargetAgeSeconds * 10.0f * UE_PI);
		// Keep the target fully readable and pulse only its helper label. The drain
		// arc already communicates urgency without hiding valid hit pixels.
		CueOpacity *= FMath::Lerp(
			1.0f,
			FMath::Clamp(VisualStyle.TargetUrgencyPulseMinOpacity, 0.0f, 1.0f),
			UrgencyAlpha * PulseWave);
	}
	const float ClampedTargetOpacity = FMath::Clamp(TargetOpacity, 0.0f, 1.0f);
	Border_Target->SetRenderOpacity(1.0f);
	if (Image_Target)
	{
		Image_Target->SetRenderOpacity(ClampedTargetOpacity);
	}
	if (TargetVisual)
	{
		TargetVisual->SetRenderOpacity(ClampedTargetOpacity * (bShowTargetIn3D
			? FMath::Clamp(VisualStyle.AuthoritativeRingOpacityIn3D, 0.0f, 1.0f)
			: 1.0f));
	}
	if (Text_TargetCue)
	{
		Text_TargetCue->SetRenderOpacity(FMath::Clamp(
			ClampedTargetOpacity * CueOpacity,
			0.0f,
			1.0f));
	}
}

void UFF_TargetShootingScreen::RefreshProjectilePresentation()
{
	const bool bShowProjectile = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing
		&& DisplayedSnapshot.bProjectileInFlight;
	const float TravelAlpha = DisplayedSnapshot.ProjectileTravelSeconds > UE_SMALL_NUMBER
		? 1.0f - FMath::Clamp(
			DisplayedSnapshot.ProjectileRemainingSeconds
				/ DisplayedSnapshot.ProjectileTravelSeconds,
			0.0f,
			1.0f)
		: 1.0f;
	if (IsValid(Arena3DActor))
	{
		Arena3DActor->SetProjectilePresentation(
			bShowProjectile
				&& bThreeDimensionalArenaReady
				&& VisualStyle.bRenderProjectileIn3D,
			DisplayedSnapshot.ProjectilePositionNormalized,
			DisplayedSnapshot.ProjectileAimPointNormalized,
			TravelAlpha,
			VisualStyle.ProjectileCoreColor,
			VisualStyle.ProjectileGlowColor);
	}

	UCanvasPanelSlot* ProjectileSlot = ProjectileVisual
		? Cast<UCanvasPanelSlot>(ProjectileVisual->Slot)
		: nullptr;
	if (!ProjectileVisual
		|| ProjectileVisual->GetParent() != Canvas_PlayArea
		|| !ProjectileSlot)
	{
		if (ProjectileVisual)
		{
			ProjectileVisual->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	ProjectileVisual->SetVisibility(bShowProjectile
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	if (!bShowProjectile)
	{
		ProjectileVisual->SetRenderOpacity(0.0f);
		return;
	}

	const FVector2D ProjectilePoint(
		FMath::Clamp(DisplayedSnapshot.ProjectilePositionNormalized.X, 0.0, 1.0),
		FMath::Clamp(DisplayedSnapshot.ProjectilePositionNormalized.Y, 0.0, 1.0));
	FVector2D ProjectedProjectilePoint = ProjectilePoint;
	ProjectArenaPointToCanvas(
		ProjectilePoint,
		ProjectedProjectilePoint,
		FMath::Lerp(
			ArenaProjectileOriginDepthX,
			ArenaProjectileImpactDepthX,
			TravelAlpha));
	ProjectileSlot->SetAnchors(FAnchors(
		ProjectedProjectilePoint.X,
		ProjectedProjectilePoint.Y));
	ProjectileSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ProjectileSlot->SetPosition(FVector2D::ZeroVector);
	const float ProjectileSize = FMath::Clamp(
		VisualStyle.ProjectileVisualSize,
		64.0f,
		220.0f);
	ProjectileSlot->SetSize(FVector2D(ProjectileSize, ProjectileSize));

	FVector2D PlayAreaSize = Canvas_PlayArea
		? Canvas_PlayArea->GetCachedGeometry().GetLocalSize()
		: FVector2D::ZeroVector;
	if (PlayAreaSize.X <= UE_SMALL_NUMBER || PlayAreaSize.Y <= UE_SMALL_NUMBER)
	{
		PlayAreaSize = LastPlayAreaSize;
	}
	FVector2D FlightDirection = DisplayedSnapshot.ProjectileAimPointNormalized
		- DisplayedSnapshot.ProjectileStartNormalized;
	if (PlayAreaSize.X > UE_SMALL_NUMBER && PlayAreaSize.Y > UE_SMALL_NUMBER)
	{
		FlightDirection *= PlayAreaSize;
	}
	ProjectileVisual->Configure(
		FlightDirection,
		VisualStyle.ProjectileCoreColor,
		VisualStyle.ProjectileGlowColor,
		VisualStyle.ProjectileCoreThickness,
		VisualStyle.ProjectileTrailLength);

	ProjectileVisual->SetRenderOpacity(FMath::Lerp(0.72f, 1.0f, TravelAlpha));
}

void UFF_TargetShootingScreen::RefreshWeaponPresentation()
{
	const bool bPlaying = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing;
	const float RecoilAlpha = VisualStyle.MuzzleFlashSeconds > UE_SMALL_NUMBER
		? FMath::Clamp(
			MuzzleFlashRemainingSeconds / VisualStyle.MuzzleFlashSeconds,
			0.0f,
			1.0f)
		: 0.0f;
	const float FlashAlpha = RecoilAlpha;
	if (IsValid(Arena3DActor))
	{
		Arena3DActor->SetWeaponPresentation(
			bPlaying
				&& bThreeDimensionalArenaReady
				&& VisualStyle.bRenderWeaponIn3D,
			RecoilAlpha,
			FlashAlpha);
	}

	UCanvasPanelSlot* WaterBlasterSlot = Image_WaterBlaster
		? Cast<UCanvasPanelSlot>(Image_WaterBlaster->Slot)
		: nullptr;
	if (Image_WaterBlaster)
	{
		const bool bValidBlaster = Image_WaterBlaster->GetParent() == Canvas_PlayArea
			&& WaterBlasterSlot != nullptr;
		Image_WaterBlaster->SetVisibility(bPlaying && bValidBlaster
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
		if (bPlaying && bValidBlaster)
		{
			const float BlasterSize = FMath::Clamp(
				VisualStyle.WaterBlasterVisualSize,
				180.0f,
				520.0f);
			WaterBlasterSlot->SetAnchors(FAnchors(0.5f, 1.0f));
			WaterBlasterSlot->SetAlignment(FVector2D(0.5f, 1.0f));
			WaterBlasterSlot->SetPosition(FVector2D(
				0.0f,
				FMath::Clamp(VisualStyle.WaterBlasterBottomOffsetPixels, 0.0f, 80.0f)));
			WaterBlasterSlot->SetSize(FVector2D(BlasterSize, BlasterSize));
			const FVector2D ViewFollowOffset = CalculateWeaponViewOffsetPixels();
			Image_WaterBlaster->SetRenderTransformPivot(FVector2D(
				FMath::Clamp(VisualStyle.WaterBlasterAimPivotNormalized.X, 0.0f, 1.0f),
				FMath::Clamp(VisualStyle.WaterBlasterAimPivotNormalized.Y, 0.0f, 1.0f)));
			Image_WaterBlaster->SetRenderTranslation(ViewFollowOffset + FVector2D(
				0.0f,
				FMath::Max(0.0f, VisualStyle.WeaponRecoilPixels) * RecoilAlpha));
			Image_WaterBlaster->SetRenderTransformAngle(CalculateWeaponAimAngleDegrees());
			Image_WaterBlaster->SetRenderOpacity(FMath::Lerp(1.0f, 0.90f, RecoilAlpha));
		}
		else
		{
			Image_WaterBlaster->SetRenderTranslation(FVector2D::ZeroVector);
			Image_WaterBlaster->SetRenderTransformAngle(0.0f);
			Image_WaterBlaster->SetRenderOpacity(1.0f);
		}
	}

	UCanvasPanelSlot* MuzzleSlot = Text_MuzzleFlash
		? Cast<UCanvasPanelSlot>(Text_MuzzleFlash->Slot)
		: nullptr;
	const bool bShowMuzzleFlash = bPlaying
		&& Text_MuzzleFlash
		&& Text_MuzzleFlash->GetParent() == Canvas_PlayArea
		&& MuzzleSlot
		&& MuzzleFlashRemainingSeconds > 0.0f;
	if (Text_MuzzleFlash)
	{
		Text_MuzzleFlash->SetVisibility(bShowMuzzleFlash
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (bShowMuzzleFlash)
	{
		const FVector2D MuzzlePoint(
			FMath::Clamp(DisplayedSnapshot.ProjectileStartNormalized.X, 0.0, 1.0),
			FMath::Clamp(DisplayedSnapshot.ProjectileStartNormalized.Y, 0.0, 1.0));
		MuzzleSlot->SetAnchors(FAnchors(MuzzlePoint.X, MuzzlePoint.Y));
		MuzzleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MuzzleSlot->SetPosition(FVector2D::ZeroVector);
		MuzzleSlot->SetSize(FVector2D(96.0f, 96.0f));
		Text_MuzzleFlash->SetRenderOpacity(FlashAlpha);
		Text_MuzzleFlash->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		const float FlashScale = FMath::Lerp(0.75f, 1.55f, FlashAlpha);
		Text_MuzzleFlash->SetRenderScale(FVector2D(FlashScale, FlashScale));
	}

	if (Text_WeaponStatus)
	{
		const bool bValidStatus = Text_WeaponStatus->GetParent() == Canvas_PlayArea
			&& Cast<UCanvasPanelSlot>(Text_WeaponStatus->Slot) != nullptr;
		Text_WeaponStatus->SetVisibility(bPlaying && bValidStatus
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
		if (bPlaying && bValidStatus)
		{
			const bool bProjectileInFlight = DisplayedSnapshot.bProjectileInFlight;
			Text_WeaponStatus->SetText(bProjectileInFlight
				? LOCTEXT("ProjectileInFlightStatus", "과녁 판정 예약  ·  물 탄환 비행 중")
				: LOCTEXT("WeaponReadyStatus", "총기를 회전해 과녁을 조준하고 발사!"));
			if (!bUsingDesignerWidgetTree)
			{
				Text_WeaponStatus->SetColorAndOpacity(FSlateColor(bProjectileInFlight
					? VisualStyle.HudAccentColor
					: VisualStyle.HudAmberColor));
			}
		}
	}
}

void UFF_TargetShootingScreen::RefreshFeedbackPresentation()
{
	if (!Text_ShotFeedback)
	{
		return;
	}

	const bool bShowFeedback = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing
		&& PresentedFeedbackSerial > 0
		&& FeedbackRemainingSeconds > 0.0f
		&& DisplayedSnapshot.LatestFeedbackType != EFF_TargetShootingFeedbackType::None;
	Text_ShotFeedback->SetVisibility(bShowFeedback
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	if (!bShowFeedback)
	{
		Text_ShotFeedback->SetRenderOpacity(0.0f);
		Text_ShotFeedback->SetRenderScale(FVector2D(1.0f, 1.0f));
		if (!bUsingDesignerWidgetTree && Border_PlayArea && bHasBasePlayAreaBrushColor)
		{
			Border_PlayArea->SetBrushColor(BasePlayAreaBrushColor);
		}
		return;
	}

	const float DurationSeconds = FMath::Max(
		VisualStyle.FeedbackDurationSeconds,
		UE_SMALL_NUMBER);
	const float FeedbackAlpha = FMath::Clamp(
		FeedbackRemainingSeconds / DurationSeconds,
		0.0f,
		1.0f);
	const FLinearColor FeedbackColor = GetFeedbackColor(DisplayedSnapshot.LatestFeedbackType);
	Text_ShotFeedback->SetText(MakeFeedbackText());
	if (!bUsingDesignerWidgetTree)
	{
		Text_ShotFeedback->SetColorAndOpacity(FSlateColor(FeedbackColor));
	}
	Text_ShotFeedback->SetRenderOpacity(FMath::Clamp(FeedbackAlpha * 1.55f, 0.0f, 1.0f));
	Text_ShotFeedback->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	const float FeedbackScale = FMath::Lerp(0.92f, 1.22f, FeedbackAlpha);
	Text_ShotFeedback->SetRenderScale(FVector2D(FeedbackScale, FeedbackScale));

	if (UCanvasPanelSlot* FeedbackSlot = Cast<UCanvasPanelSlot>(Text_ShotFeedback->Slot))
	{
		FVector2D FeedbackPoint(
			FMath::Clamp(DisplayedSnapshot.LatestFeedbackPointNormalized.X, 0.0, 1.0),
			FMath::Clamp(DisplayedSnapshot.LatestFeedbackPointNormalized.Y, 0.0, 1.0));
		ProjectArenaPointToCanvas(
			DisplayedSnapshot.LatestFeedbackPointNormalized,
			FeedbackPoint);
		// Keep the score callout clear of the authoritative target/reticle. Near the
		// top edge it travels downward; everywhere else it rises above the hit.
		const float FeedbackDirection = FeedbackPoint.Y < 0.24f ? 1.0f : -1.0f;
		const float FeedbackOffset = 110.0f
			+ FMath::Max(0.0f, VisualStyle.FeedbackRisePixels) * (1.0f - FeedbackAlpha);
		FeedbackSlot->SetAnchors(FAnchors(FeedbackPoint.X, FeedbackPoint.Y));
		FeedbackSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FeedbackSlot->SetPosition(FVector2D(0.0f, FeedbackDirection * FeedbackOffset));
	}

	if (!bUsingDesignerWidgetTree && Border_PlayArea && bHasBasePlayAreaBrushColor)
	{
		const float FlashAlpha = FMath::Clamp(
			VisualStyle.PlayAreaFlashStrength * FeedbackAlpha,
			0.0f,
			0.5f);
		FLinearColor FlashColor = FLinearColor::LerpUsingHSV(
			BasePlayAreaBrushColor,
			FeedbackColor,
			FlashAlpha);
		FlashColor.A = BasePlayAreaBrushColor.A;
		Border_PlayArea->SetBrushColor(FlashColor);
	}
}

void UFF_TargetShootingScreen::RefreshReticlePresentation()
{
	UCanvasPanelSlot* ReticleSlot = Text_Reticle
		? Cast<UCanvasPanelSlot>(Text_Reticle->Slot)
		: nullptr;
	if (!Text_Reticle || Text_Reticle->GetParent() != Canvas_PlayArea || !ReticleSlot)
	{
		if (Text_Reticle)
		{
			Text_Reticle->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	const bool bPlaying = bHasSnapshot
		&& !bHasResult
		&& DisplayedSnapshot.Session.State == EFF_MinigameSessionState::Playing;
	const bool bShowReticle = bPlaying && !VisualStyle.ReticleGlyph.IsEmpty();
	Text_Reticle->SetVisibility(bShowReticle
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
	if (!bShowReticle)
	{
		Text_Reticle->SetRenderScale(FVector2D(1.0f, 1.0f));
		return;
	}

	Text_Reticle->SetText(FText::FromString(VisualStyle.ReticleGlyph));
	const bool bMissFlash = FeedbackRemainingSeconds > 0.0f
		&& DisplayedSnapshot.LatestFeedbackType == EFF_TargetShootingFeedbackType::Miss;
	// Use the authoritative gameplay rule so the yellow ready state never promises
	// a hit outside the ellipse that will be evaluated at projectile impact.
	const bool bReticleOnTarget = DisplayedSnapshot.bTargetVisible
		&& FFTargetShootingRules::IsInsideTarget(
			CameraAimPointNormalized,
			DisplayedSnapshot.TargetCenterNormalized,
			DisplayedSnapshot.TargetRadiusNormalized);
	const bool bAimReady = bReticleOnTarget && !DisplayedSnapshot.bProjectileInFlight;
	if (!bUsingDesignerWidgetTree)
	{
		Text_Reticle->SetColorAndOpacity(FSlateColor(
			bMissFlash
				? VisualStyle.MissColor
				: (bAimReady ? VisualStyle.BullseyeColor : VisualStyle.ReticleColor)));
	}
	if (bAimReady && Text_TargetCue)
	{
		Text_TargetCue->SetText(LOCTEXT("AimReadyTargetCue", "발사!"));
		if (!bUsingDesignerWidgetTree)
		{
			Text_TargetCue->SetColorAndOpacity(FSlateColor(VisualStyle.BullseyeColor));
		}
		Text_TargetCue->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Text_TargetCue->SetRenderScale(FVector2D(1.12f, 1.12f));
	}
	const FVector2D ReticleAnchor = VisualStyle.bUseSingleBackdrop
		? CameraAimPointNormalized
		: FVector2D(0.5f, 0.5f);
	ReticleSlot->SetAnchors(FAnchors(ReticleAnchor.X, ReticleAnchor.Y));
	ReticleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ReticleSlot->SetPosition(FVector2D::ZeroVector);

	float ReticleScale = bAimReady ? 1.18f : 1.0f;
	if (ReticleShotPulseRemainingSeconds > 0.0f
		&& VisualStyle.ReticleShotPulseSeconds > UE_SMALL_NUMBER)
	{
		const float PulseAlpha = FMath::Clamp(
			ReticleShotPulseRemainingSeconds / VisualStyle.ReticleShotPulseSeconds,
			0.0f,
			1.0f);
		ReticleScale = FMath::Max(ReticleScale, FMath::Lerp(
			1.0f,
			FMath::Max(1.0f, VisualStyle.ReticleShotPulseScale),
			PulseAlpha));
	}
	Text_Reticle->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	Text_Reticle->SetRenderScale(FVector2D(ReticleScale, ReticleScale));
}

bool UFF_TargetShootingScreen::UpdateAimFromScreenPosition(
	const FVector2D& ScreenSpacePosition)
{
	if (!Canvas_PlayArea
		|| !FMath::IsFinite(ScreenSpacePosition.X)
		|| !FMath::IsFinite(ScreenSpacePosition.Y))
	{
		return false;
	}
	const FGeometry& PlayAreaGeometry = Canvas_PlayArea->GetCachedGeometry();
	const FVector2D PlayAreaSize = PlayAreaGeometry.GetLocalSize();
	if (PlayAreaSize.X <= UE_SMALL_NUMBER || PlayAreaSize.Y <= UE_SMALL_NUMBER)
	{
		return false;
	}
	const FVector2D LocalPointer = PlayAreaGeometry.AbsoluteToLocal(ScreenSpacePosition);
	if (!FMath::IsFinite(LocalPointer.X) || !FMath::IsFinite(LocalPointer.Y))
	{
		return false;
	}
	CameraAimPointNormalized = FVector2D(
		FMath::Clamp(LocalPointer.X / PlayAreaSize.X, 0.0f, 1.0f),
		FMath::Clamp(LocalPointer.Y / PlayAreaSize.Y, 0.0f, 1.0f));
	return true;
}

FVector2D UFF_TargetShootingScreen::ResolveWeaponPlayAreaSize() const
{
	FVector2D PlayAreaSize = Canvas_PlayArea
		? Canvas_PlayArea->GetCachedGeometry().GetLocalSize()
		: FVector2D::ZeroVector;
	if (PlayAreaSize.X <= UE_SMALL_NUMBER || PlayAreaSize.Y <= UE_SMALL_NUMBER)
	{
		PlayAreaSize = LastPlayAreaSize;
	}
	return PlayAreaSize;
}

FVector2D UFF_TargetShootingScreen::CalculateWeaponViewOffsetPixels() const
{
	const FVector2D PlayAreaSize = ResolveWeaponPlayAreaSize();
	const float FollowStrength = FMath::Clamp(
		VisualStyle.WeaponViewFollowStrength,
		0.0f,
		0.15f);
	return FVector2D(
		(CameraAimPointNormalized.X - 0.5f) * PlayAreaSize.X * FollowStrength,
		(CameraAimPointNormalized.Y - 0.5f) * PlayAreaSize.Y * FollowStrength);
}

FVector2D UFF_TargetShootingScreen::CalculateWeaponPivotPointPixels() const
{
	const FVector2D PlayAreaSize = ResolveWeaponPlayAreaSize();
	if (PlayAreaSize.X <= UE_SMALL_NUMBER || PlayAreaSize.Y <= UE_SMALL_NUMBER)
	{
		return FVector2D::ZeroVector;
	}
	const float BlasterSize = FMath::Clamp(
		VisualStyle.WaterBlasterVisualSize,
		180.0f,
		520.0f);
	const FVector2D PivotNormalized(
		FMath::Clamp(VisualStyle.WaterBlasterAimPivotNormalized.X, 0.0f, 1.0f),
		FMath::Clamp(VisualStyle.WaterBlasterAimPivotNormalized.Y, 0.0f, 1.0f));
	const FVector2D ImageTopLeft(
		PlayAreaSize.X * 0.5f - BlasterSize * 0.5f,
		PlayAreaSize.Y
			+ FMath::Clamp(VisualStyle.WaterBlasterBottomOffsetPixels, 0.0f, 80.0f)
			- BlasterSize);
	return ImageTopLeft
		+ PivotNormalized * BlasterSize
		+ CalculateWeaponViewOffsetPixels();
}

FVector2D UFF_TargetShootingScreen::CalculateWeaponBaseForwardDirection() const
{
	const FVector2D PivotNormalized(
		FMath::Clamp(VisualStyle.WaterBlasterAimPivotNormalized.X, 0.0f, 1.0f),
		FMath::Clamp(VisualStyle.WaterBlasterAimPivotNormalized.Y, 0.0f, 1.0f));
	const FVector2D MuzzleNormalized(
		FMath::Clamp(VisualStyle.WaterBlasterMuzzleNormalized.X, 0.0f, 1.0f),
		FMath::Clamp(VisualStyle.WaterBlasterMuzzleNormalized.Y, 0.0f, 1.0f));
	const FVector2D Forward = MuzzleNormalized - PivotNormalized;
	const float ForwardLength = Forward.Size();
	return ForwardLength > UE_SMALL_NUMBER
		? Forward / ForwardLength
		: FVector2D(0.0f, -1.0f);
}

float UFF_TargetShootingScreen::CalculateWeaponAimAngleDegrees() const
{
	const FVector2D PlayAreaSize = ResolveWeaponPlayAreaSize();
	if (PlayAreaSize.X <= UE_SMALL_NUMBER || PlayAreaSize.Y <= UE_SMALL_NUMBER)
	{
		return 0.0f;
	}
	const FVector2D DesiredDirection = CameraAimPointNormalized * PlayAreaSize
		- CalculateWeaponPivotPointPixels();
	if (DesiredDirection.SizeSquared() <= UE_SMALL_NUMBER)
	{
		return 0.0f;
	}
	const FVector2D BaseForward = CalculateWeaponBaseForwardDirection();
	const float BaseAngleDegrees = FMath::RadiansToDegrees(
		FMath::Atan2(BaseForward.Y, BaseForward.X));
	const float DesiredAngleDegrees = FMath::RadiansToDegrees(
		FMath::Atan2(DesiredDirection.Y, DesiredDirection.X));
	const float RequiredAngleDegrees = FMath::FindDeltaAngleDegrees(
		BaseAngleDegrees,
		DesiredAngleDegrees);
	return RequiredAngleDegrees;
}

FVector2D UFF_TargetShootingScreen::CalculateWeaponMuzzlePointNormalized() const
{
	const FVector2D PlayAreaSize = ResolveWeaponPlayAreaSize();
	if (PlayAreaSize.X <= UE_SMALL_NUMBER || PlayAreaSize.Y <= UE_SMALL_NUMBER)
	{
		return FVector2D(0.5f, 0.68f);
	}
	const float BlasterSize = FMath::Clamp(
		VisualStyle.WaterBlasterVisualSize,
		180.0f,
		520.0f);
	const FVector2D PivotNormalized(
		FMath::Clamp(VisualStyle.WaterBlasterAimPivotNormalized.X, 0.0f, 1.0f),
		FMath::Clamp(VisualStyle.WaterBlasterAimPivotNormalized.Y, 0.0f, 1.0f));
	const FVector2D MuzzleNormalized(
		FMath::Clamp(VisualStyle.WaterBlasterMuzzleNormalized.X, 0.0f, 1.0f),
		FMath::Clamp(VisualStyle.WaterBlasterMuzzleNormalized.Y, 0.0f, 1.0f));
	const FVector2D MuzzleFromPivot = (MuzzleNormalized - PivotNormalized) * BlasterSize;
	const FVector2D MuzzlePoint = CalculateWeaponPivotPointPixels()
		+ RotateScreenVector(MuzzleFromPivot, CalculateWeaponAimAngleDegrees());
	return FVector2D(
		FMath::Clamp(MuzzlePoint.X / PlayAreaSize.X, 0.0f, 1.0f),
		FMath::Clamp(MuzzlePoint.Y / PlayAreaSize.Y, 0.0f, 1.0f));
}

void UFF_TargetShootingScreen::RefreshCameraAimPresentation()
{
	CameraAimPointNormalized.X = FMath::Clamp(
		CameraAimPointNormalized.X,
		0.0f,
		1.0f);
	CameraAimPointNormalized.Y = FMath::Clamp(
		CameraAimPointNormalized.Y,
		0.0f,
		1.0f);

	const FVector AimWorld = AFF_TargetShootingArena3D::NormalizedToTargetPlane(
		CameraAimPointNormalized);
	const FVector AimDirection = AimWorld - VisualStyle.Arena3DCameraLocation;
	if (!VisualStyle.bUseSingleBackdrop
		&& !AimDirection.IsNearlyZero()
		&& Viewport_Arena3D)
	{
		const FRotator ViewRotation = AimDirection.Rotation()
			+ VisualStyle.Arena3DCameraRotation;
		Viewport_Arena3D->SetViewLocation(VisualStyle.Arena3DCameraLocation);
		Viewport_Arena3D->SetViewRotation(ViewRotation);
	}

	// The authored single backdrop is presentation-only and never follows aim.
	if (Image_Backdrop)
	{
		Image_Backdrop->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		Image_Backdrop->SetRenderScale(FVector2D(1.0f));
		Image_Backdrop->SetRenderTranslation(FVector2D::ZeroVector);
	}
}

bool UFF_TargetShootingScreen::ProjectArenaPointToCanvas(
	const FVector2D& ArenaPointNormalized,
	FVector2D& OutCanvasPointNormalized,
	const float DepthX) const
{
	if (!Canvas_PlayArea)
	{
		return false;
	}
	if (VisualStyle.bUseSingleBackdrop)
	{
		OutCanvasPointNormalized = FVector2D(
			FMath::Clamp(ArenaPointNormalized.X, 0.0, 1.0),
			FMath::Clamp(ArenaPointNormalized.Y, 0.0, 1.0));
		return true;
	}
	const FVector2D CanvasSize = Canvas_PlayArea->GetCachedGeometry().GetLocalSize();
	if (CanvasSize.X <= UE_SMALL_NUMBER || CanvasSize.Y <= UE_SMALL_NUMBER)
	{
		return false;
	}

	const FVector AimWorld = AFF_TargetShootingArena3D::NormalizedToTargetPlane(
		CameraAimPointNormalized);
	const FVector AimDirection = AimWorld - VisualStyle.Arena3DCameraLocation;
	if (AimDirection.IsNearlyZero())
	{
		return false;
	}

	const FRotator ViewRotation = AimDirection.Rotation()
		+ VisualStyle.Arena3DCameraRotation;
	const FRotationMatrix ViewAxes(ViewRotation);
	const FVector Delta = AFF_TargetShootingArena3D::NormalizedToTargetPlane(
		ArenaPointNormalized,
		DepthX) - VisualStyle.Arena3DCameraLocation;
	const float ForwardDepth = FVector::DotProduct(
		Delta,
		ViewAxes.GetUnitAxis(EAxis::X));
	if (!FMath::IsFinite(ForwardDepth) || ForwardDepth <= UE_SMALL_NUMBER)
	{
		return false;
	}

	// UViewport's preview client uses a 90-degree horizontal field of view. Project
	// the target plane with the same camera basis so the exact 2D hit ring follows
	// the 3D target when the player looks around.
	const float HorizontalTangent = FVector::DotProduct(
		Delta,
		ViewAxes.GetUnitAxis(EAxis::Y)) / ForwardDepth;
	const float VerticalTangent = FVector::DotProduct(
		Delta,
		ViewAxes.GetUnitAxis(EAxis::Z)) / ForwardDepth;
	const float AspectRatio = CanvasSize.X / CanvasSize.Y;
	OutCanvasPointNormalized = FVector2D(
		0.5f + HorizontalTangent * 0.5f,
		0.5f - VerticalTangent * AspectRatio * 0.5f);
	return FMath::IsFinite(OutCanvasPointNormalized.X)
		&& FMath::IsFinite(OutCanvasPointNormalized.Y);
}

FLinearColor UFF_TargetShootingScreen::GetFeedbackColor(
	const EFF_TargetShootingFeedbackType FeedbackType) const
{
	if (DisplayedSnapshot.bLatestFeedbackWasBonusTarget
		&& FeedbackType != EFF_TargetShootingFeedbackType::Miss
		&& FeedbackType != EFF_TargetShootingFeedbackType::Escaped)
	{
		return VisualStyle.BonusTargetColor;
	}

	switch (FeedbackType)
	{
	case EFF_TargetShootingFeedbackType::Bullseye:
		return VisualStyle.BullseyeColor;
	case EFF_TargetShootingFeedbackType::Precision:
		return VisualStyle.PrecisionColor;
	case EFF_TargetShootingFeedbackType::Hit:
		return VisualStyle.HitColor;
	case EFF_TargetShootingFeedbackType::Miss:
		return VisualStyle.MissColor;
	case EFF_TargetShootingFeedbackType::Escaped:
		return VisualStyle.EscapedColor;
	default:
		return FLinearColor::White;
	}
}

FText UFF_TargetShootingScreen::MakeFeedbackText() const
{
	if (DisplayedSnapshot.LatestFeedbackType == EFF_TargetShootingFeedbackType::Escaped)
	{
		return LOCTEXT("EscapedFeedback", "시간 초과\n연속 명중 종료");
	}
	if (DisplayedSnapshot.LatestFeedbackType == EFF_TargetShootingFeedbackType::Miss)
	{
		return DisplayedSnapshot.LatestScoreDelta < 0
			? FText::Format(
				LOCTEXT("MissPenaltyFeedback", "빗나감  {0}\n연속 명중 종료"),
				FText::AsNumber(DisplayedSnapshot.LatestScoreDelta))
			: LOCTEXT("MissFeedback", "빗나감\n연속 명중 종료");
	}

	FText QualityText;
	switch (DisplayedSnapshot.LatestFeedbackType)
	{
	case EFF_TargetShootingFeedbackType::Bullseye:
		QualityText = LOCTEXT("BullseyeFeedback", "정중앙!");
		break;
	case EFF_TargetShootingFeedbackType::Precision:
		QualityText = LOCTEXT("PrecisionFeedback", "정밀 명중!");
		break;
	default:
		QualityText = LOCTEXT("HitFeedback", "명중!");
		break;
	}
	if (DisplayedSnapshot.bLatestFeedbackWasBonusTarget)
	{
		QualityText = FText::Format(
			LOCTEXT("GoldenFeedback", "황금 {0}"),
			QualityText);
	}

	return DisplayedSnapshot.CurrentCombo >= 2
		? FText::Format(
			LOCTEXT("ScoredComboFeedback", "{0}  +{1}\n연속 x{2}"),
			QualityText,
			FText::AsNumber(FMath::Max(0, DisplayedSnapshot.LatestScoreDelta)),
			FText::AsNumber(DisplayedSnapshot.CurrentCombo))
		: FText::Format(
			LOCTEXT("ScoredFeedback", "{0}  +{1}"),
			QualityText,
			FText::AsNumber(FMath::Max(0, DisplayedSnapshot.LatestScoreDelta)));
}

void UFF_TargetShootingScreen::HandleCloseClicked()
{
	RequestClose();
}

#undef LOCTEXT_NAMESPACE
