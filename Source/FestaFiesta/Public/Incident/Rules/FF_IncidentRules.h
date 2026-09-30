// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Incident/FF_IncidentTypes.h"

class UFF_IncidentDefinition;

/**
 * 사건 판정 용 순수 구조체
 *
 * 중복 억제 키 · 수명 판정 · 상태 전이를 갖는다. 전부 World 없이 테스트된다.
 * UFF_IncidentSubsystem 은 보관 · 수명주기 · 조회만 하고 판정은 여기로 넘긴다.
 */
struct FESTAFIESTA_API FFF_IncidentRules
{
	// --- 중복 억제 --------------------------------------------------------

	// WorldPoint 앵커 양자화 격자. 밀집 격자(2m)와 같은 스케일.
	static constexpr float WorldPointDedupeGridUU = 200.f;

	// 앵커 하나를 문자열 조각으로 줄인다. WorldPoint 는 반드시 격자로 양자화한다.
	static FString MakeAnchorId(const FFF_IncidentAnchor& Anchor);

	// 같은 사건인지 가르는 키. Tag_AnchorId 형태다. 손으로 짓는 경로를 만들지 않는다.
	static FName MakeDedupeKey(const FGameplayTag& IncidentTag, const FFF_IncidentAnchor& Anchor);

	// --- 수명 · 상태 전이 -------------------------------------------------

	// 시각은 전부 축제분(Live 경과분)이다. MinutesUntilLapse <= 0 이면 만료되지 않는다.
	static bool HasLapsed(float NowMinutes, float OccurredAtMinutes, float MinutesUntilLapse);

	// 시간 초과로 닫힐 때의 최종 상태. Problem → Ignored, Opportunity → Missed.
	static EFF_IncidentState LapseStateFor(EFF_IncidentTone Tone);

	// 같은 키가 다시 올라왔을 때 남길 Intensity. 평균이 아니라 최대다.
	static float MergeIntensity(float ExistingPeak, float IncomingIntensity);

	// --- 선택지 View -------------------------------------------------------

	// 정의의 Options 를 UI 용 값으로 복사한다. null 선택지도 자리를 채우고, MinStaff 는 1 이상으로 정리한다.
	static void BuildOptionViews(const UFF_IncidentDefinition& Definition, TArray<FFF_IncidentOptionView>& OutViews);
};
