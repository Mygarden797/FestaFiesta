// 패널 안 스크롤바의 공용 모양. 셋업·라이브 패널과 음악·결산 화면이 같은 막대를 쓴다.
#pragma once

#include "CoreMinimal.h"
#include "FF_PanelScrollStyle.generated.h"

class UWidgetTree;
struct FSlateBrush;

/**
 * 엔진 기본 스크롤바(어두운 회색 막대)를 옅은 트랙 위의 차분한 하늘색 막대로 바꾼다.
 * 각 패널의 Class Defaults 에서 고치고, `Apply` 를 끄면 ScrollBox 별 Designer 스타일을 그대로 쓴다.
 * 패널이 처음 만들어질 때 한 번만 적용한다. 매 갱신마다 다시 덮어쓰지 않는다.
 *
 * 2026-10-07 — 막대 끝이 뾰족하게 늘어나던 원인: 둥글기를 `HalfHeightRadius`(높이의 절반)로 줬다.
 * 세로 막대는 높이가 길어 반지름이 폭보다 훨씬 커지고, 모양이 렌즈처럼 양 끝이 뾰족해진다.
 * 이제 고정 반지름(`Corner Radius`)을 쓴다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PanelScrollStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scroll")
	bool bApply = true;

	/** 보이는 막대 폭. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scroll", meta = (ClampMin = "2.0", UIMax = "16.0"))
	float Thickness = 8.f;

	/**
	 * 마우스로 잡히는 폭. 보이는 막대보다 넓으면 양옆이 투명한 손잡이가 된다.
	 * 막대는 이 폭의 가운데에 그려지고, 모자라면 `Thickness` 를 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scroll", meta = (ClampMin = "2.0", UIMax = "24.0"))
	float GrabWidth = 14.f;

	/** 막대·트랙 양 끝의 둥글기(반지름). 폭의 절반이면 끝이 반원이고, 그보다 크게 줘도 반원에서 멈춘다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scroll", meta = (ClampMin = "0.0", UIMax = "8.0"))
	float CornerRadius = 3.f;

	/** 잡는 영역 바깥 여백. 왼쪽 값이 목록과 막대 사이 간격이다(보이는 간격은 여기에 잡는 폭의 투명한 몫이 더해진다). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scroll")
	FMargin Padding = FMargin(4.f, 2.f, 0.f, 2.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scroll")
	FLinearColor TrackColor = FLinearColor::FromSRGBColor(FColor(0xEA, 0xF1, 0xF2));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scroll")
	FLinearColor ThumbColor = FLinearColor::FromSRGBColor(FColor(0x88, 0xB9, 0xC8));

	/** 마우스를 올렸을 때. 색만 바뀌고 폭·위치는 그대로다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scroll")
	FLinearColor ThumbHoveredColor = FLinearColor::FromSRGBColor(FColor(0x6E, 0xA6, 0xB8));

	/** 끄는 중. 색만 바뀌고 폭·위치는 그대로다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scroll")
	FLinearColor ThumbDraggedColor = FLinearColor::FromSRGBColor(FColor(0x5A, 0x95, 0xA8));

	/** 트리 안 모든 ScrollBox 에 적용한다. 스크롤바를 보일지는 각 ScrollBox 설정을 따른다. */
	void ApplyTo(const UWidgetTree* Tree) const;

	/** 잡는 폭 안에 보이는 폭만큼만 칠하는 둥근 막대 브러시. 테스트에서도 쓴다. */
	FSlateBrush MakeBarBrush(const FLinearColor& Color) const;
};
