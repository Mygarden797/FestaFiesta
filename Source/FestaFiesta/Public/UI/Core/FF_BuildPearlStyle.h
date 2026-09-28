// 게임 화면과 에디터 제작 도구가 함께 사용하는 공통 규칙이다.
#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "Styling/SlateBrush.h"

// Shared authored construction surfaces. No ticking material instances or per-card asset loads.
namespace FFBuildPearl
{
inline FLinearColor Color(uint8 R, uint8 G, uint8 B, float A = 1.f)
{
	FLinearColor Result = FLinearColor::FromSRGBColor(FColor(R, G, B));
	Result.A = A;
	return Result;
}
inline const FLinearColor Ink = Color(61, 60, 79);
inline const FLinearColor Muted = Color(123, 125, 146);
inline const FLinearColor Charcoal = Color(60, 59, 79, .94f);
inline const FLinearColor Pearl = Color(249, 249, 255, .86f);
inline const FLinearColor Cyan = Color(145, 223, 255);
inline const FLinearColor Line = Color(219, 222, 237, .75f);

inline FSlateBrush Rounded(FLinearColor Fill, float Radius = 22.f,
	FLinearColor Outline = FLinearColor::Transparent, float Width = 0.f)
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

inline FSlateBrush MaterialBrush(const TCHAR* Role)
{
	FSlateBrush Brush = Rounded(Pearl);
	const FString Name = FString(TEXT("MI_UI_BuildPearl_")) + Role;
	const FString Path = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Controls/Pearl/") + Name + TEXT(".") + Name;
	if (auto* Material = LoadObject<UMaterialInterface>(nullptr, *Path))
	{
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.TintColor = FLinearColor::White;
		Brush.SetResourceObject(Material);
	}
	return Brush;
}
}
