// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Incident/FF_IncidentTypes.h"

struct FFF_UIPaletteInGame;

// 규격: Design/UXUI/IncidentMarker_LightTeal/README.md
enum class EFF_IncidentMarkerGlyph : uint8
{
	Quest,
	Info,
	Dispatched
};

enum class EFF_IncidentMarkerMotion : uint8
{
	None,
	Pulse,
	Blink
};

struct FFF_IncidentMarkerLook
{
	EFF_IncidentMarkerGlyph Glyph = EFF_IncidentMarkerGlyph::Quest;
	FLinearColor RingColor = FLinearColor::White;
	float Progress = 1.f;
	EFF_IncidentMarkerMotion Motion = EFF_IncidentMarkerMotion::None;
	FText Tooltip;
};

namespace FFIncidentMarkerRules
{
	FESTAFIESTA_API FFF_IncidentMarkerLook Resolve(const FFF_IncidentView& View, bool bHasNow, float NowMinutes, const FFF_UIPaletteInGame& Palette);

	FESTAFIESTA_API float GetLapseProgress(float OccurredAtMinutes, float MinutesUntilLapse, bool bHasNow, float NowMinutes);
	FESTAFIESTA_API float GetWorkProgress(EFF_IncidentResponseStage Stage, float RemainingWorkMinutes, float TotalWorkMinutes);
	FESTAFIESTA_API FLinearColor GetUrgencyColor(EFF_IncidentUrgency Urgency, const FFF_UIPaletteInGame& Palette);
	FESTAFIESTA_API EFF_IncidentMarkerMotion GetUrgencyMotion(EFF_IncidentUrgency Urgency);

	FESTAFIESTA_API bool IsInsideRect(const FVector2D& Point, const FVector2D& Min, const FVector2D& Max);
	FESTAFIESTA_API FVector2D DirectionFromCameraLocal(float Right, float Up);
	FESTAFIESTA_API FVector2D ClampToRectEdge(const FVector2D& Direction, const FVector2D& Min, const FVector2D& Max);
	FESTAFIESTA_API float DirectionToAngleDegrees(const FVector2D& Direction);
}
