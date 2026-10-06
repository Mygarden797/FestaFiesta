// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/Core/FF_PauseOptionsLightTealStyle.h"

class UFont;

/**
 * 사건 UI(알림 · 상세 패널 · 마커)가 함께 쓰는 LightTeal 모양.
 *
 * 색은 전부 UFF_UIPalette → FFPauseOptionsLightTeal 토큰에서 받는다. 여기서는 그 색을 둥근 브러시·버튼 스타일·글꼴로 묶기만 한다.
 * 크기 기준은 Design/UXUI/PrepBuild_LightTeal/UMG_PRODUCTION_GUIDE.md 4절과 FacilityInfo_LightTeal/README.md 를 따른다:
 * 패널 모서리 8px · 카드·버튼 6px · 탭·칩 4px, 패널 제목 20px SemiBold, 누를 수 있는 높이 44px 이상(DesignSystem_v2 README).
 * 테두리는 1px, 글꼴은 프로젝트 UI 글꼴 Pretendard 다.
 */
namespace FFIncidentUiStyle
{
	inline constexpr const TCHAR* UiFontPath = TEXT("/Game/FestaFiesta/UI/Fonts/F_UI_Pretendard.F_UI_Pretendard");
	inline constexpr float PanelRadius = 8.f;
	inline constexpr float CardRadius = 6.f;
	inline constexpr float ChipRadius = 4.f;
	inline constexpr float MinInteractiveSize = 44.f;
	inline constexpr int32 PanelTitleFontSize = 20;

	FESTAFIESTA_API FSlateBrush MakeRounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float Width);
	FESTAFIESTA_API FSlateBrush MakePill(const FLinearColor& Fill);
	FESTAFIESTA_API FSlateBrush MakeRing(const FLinearColor& Outline, float Width);

	/** UiFont 가 없으면 엔진 기본 글꼴로 대신한다. */
	FESTAFIESTA_API FSlateFontInfo MakeFont(UFont* UiFont, const TCHAR* Typeface, int32 Size);
	FESTAFIESTA_API UFont* LoadUiFont();

	/** 주 버튼은 Teal 바탕, 보조 버튼은 PaperStrong 바탕에 Line 테두리. 모서리는 CardRadius(6px). */
	FESTAFIESTA_API FButtonStyle MakeButtonStyle(const FFPauseOptionsLightTeal::FTokens& Tokens, bool bPrimary);
	FESTAFIESTA_API FLinearColor GetButtonLabelColor(const FFPauseOptionsLightTeal::FTokens& Tokens, bool bPrimary, bool bEnabled);
}
