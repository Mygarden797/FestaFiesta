// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Incident/FF_IncidentMarkerRules.h"

#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UIDisplayTextLibrary.h"

#define LOCTEXT_NAMESPACE "FestaFiestaIncidentMarker"

namespace FFIncidentMarkerRules
{
	float GetLapseProgress(float OccurredAtMinutes, float MinutesUntilLapse, bool bHasNow, float NowMinutes)
	{
		if (MinutesUntilLapse <= 0.f || !bHasNow)
		{
			return 1.f;
		}
		return FMath::Clamp(1.f - (NowMinutes - OccurredAtMinutes) / MinutesUntilLapse, 0.f, 1.f);
	}

	float GetWorkProgress(EFF_IncidentResponseStage Stage, float RemainingWorkMinutes, float TotalWorkMinutes)
	{
		if (Stage != EFF_IncidentResponseStage::Working || TotalWorkMinutes <= 0.f)
		{
			return 0.f;
		}
		return FMath::Clamp(1.f - RemainingWorkMinutes / TotalWorkMinutes, 0.f, 1.f);
	}

	FLinearColor GetUrgencyColor(EFF_IncidentUrgency Urgency, const FFF_UIPaletteInGame& Palette)
	{
		switch (Urgency)
		{
		case EFF_IncidentUrgency::Low:       return Palette.IncidentUrgencyLow;
		case EFF_IncidentUrgency::High:      return Palette.Amber;
		case EFF_IncidentUrgency::Immediate: return Palette.Danger;
		case EFF_IncidentUrgency::Medium:
		default:                             return Palette.IncidentUrgencyMedium;
		}
	}

	EFF_IncidentMarkerMotion GetUrgencyMotion(EFF_IncidentUrgency Urgency)
	{
		switch (Urgency)
		{
		case EFF_IncidentUrgency::High:      return EFF_IncidentMarkerMotion::Pulse;
		case EFF_IncidentUrgency::Immediate: return EFF_IncidentMarkerMotion::Blink;
		default:                             return EFF_IncidentMarkerMotion::None;
		}
	}

	bool IsInsideRect(const FVector2D& Point, const FVector2D& Min, const FVector2D& Max)
	{
		return Point.X >= Min.X && Point.Y >= Min.Y && Point.X <= Max.X && Point.Y <= Max.Y;
	}

	FVector2D DirectionFromCameraLocal(float Right, float Up)
	{
		const FVector2D Direction(Right, -Up);
		return Direction.IsNearlyZero() ? FVector2D(0.f, 1.f) : Direction.GetSafeNormal();
	}

	FVector2D ClampToRectEdge(const FVector2D& Direction, const FVector2D& Min, const FVector2D& Max)
	{
		const FVector2D Center = (Min + Max) * 0.5f;
		const FVector2D Half = (Max - Min) * 0.5f;
		const FVector2D Dir = Direction.IsNearlyZero() ? FVector2D(0.f, 1.f) : Direction.GetSafeNormal();

		const float ScaleX = FMath::Abs(Dir.X) > KINDA_SMALL_NUMBER ? Half.X / FMath::Abs(Dir.X) : TNumericLimits<float>::Max();
		const float ScaleY = FMath::Abs(Dir.Y) > KINDA_SMALL_NUMBER ? Half.Y / FMath::Abs(Dir.Y) : TNumericLimits<float>::Max();
		return Center + Dir * FMath::Min(ScaleX, ScaleY);
	}

	float DirectionToAngleDegrees(const FVector2D& Direction)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
	}

	FFF_IncidentMarkerLook Resolve(const FFF_IncidentView& View, bool bHasNow, float NowMinutes, const FFF_UIPaletteInGame& Palette)
	{
		FFF_IncidentMarkerLook Look;
		if (View.ResponseStage != EFF_IncidentResponseStage::None)
		{
			Look.Glyph = EFF_IncidentMarkerGlyph::Dispatched;
			Look.RingColor = Palette.IncidentDispatch;
			Look.Progress = GetWorkProgress(View.ResponseStage, View.RemainingWorkMinutes, View.TotalWorkMinutes);
			Look.Tooltip = FText::Format(LOCTEXT("RespondingTip", "스태프 출동 중 · {0}"),
				UFF_UIDisplayTextLibrary::GetIncidentResponseStageText(View.ResponseStage));
			return Look;
		}

		Look.Glyph = View.Tone == EFF_IncidentTone::Notice ? EFF_IncidentMarkerGlyph::Info : EFF_IncidentMarkerGlyph::Quest;
		Look.RingColor = GetUrgencyColor(View.Urgency, Palette);
		Look.Motion = GetUrgencyMotion(View.Urgency);
		Look.Progress = GetLapseProgress(View.OccurredAtMinutes, View.MinuteUntileLapse, bHasNow, NowMinutes);
		if (View.MinuteUntileLapse > 0.f && bHasNow)
		{
			const float Remaining = FMath::Max(0.f, View.MinuteUntileLapse - (NowMinutes - View.OccurredAtMinutes));
			Look.Tooltip = FText::Format(LOCTEXT("LapseTip", "남은 시간 약 {0}분"), FText::AsNumber(FMath::Max(1, FMath::CeilToInt(Remaining))));
		}
		return Look;
	}
}

#undef LOCTEXT_NAMESPACE
