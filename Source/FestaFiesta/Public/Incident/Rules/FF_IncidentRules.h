// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Incident/FF_IncidentTypes.h"

class UFF_IncidentDefinition;
class UWorld;

/**
 * 사건 판정 용 순수 구조체
 *
 * 중복 억제 키 · 수명 판정 · 상태 전이 · 앵커 위치를 갖는다. Zone 앵커 위치만 World 가 필요하고 나머지는 World 없이 테스트된다.
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

	// 자연 해소는 열람했거나 출동 중이던 사건만 알린다. 세션 정리는 조용히, 처리 완료·시간 만료는 알린다.
	// 열람은 UI 상태이며 출동을 뜻하는 Acknowledged 상태와 구분한다. 원인이 없는 이전 계약은 기존 정책을 따른다.
	static bool ShouldToastClosed(const FFF_IncidentClosedInfo& Info, bool bWasViewed = false);

	// 같은 키가 다시 올라왔을 때 남길 Intensity. 평균이 아니라 최대다.
	static float MergeIntensity(float ExistingPeak, float IncomingIntensity);

	// --- 앵커 위치 --------------------------------------------------------

	// 앵커가 지금 가리키는 월드 위치. 위치가 없거나(Screenspace) 대상이 사라졌으면 false.
	// DensityCell 은 셀 번호를 변환하지 않고 신고자가 함께 채운 WorldPoint(셀 중심)를 쓴다.
	// World 는 Zone 조회에만 쓰며, 나머지 종류는 nullptr 이어도 된다.
	static bool TryGetAnchorLocation(const UWorld* World, const FFF_IncidentAnchor& Anchor, FVector& OutLocation);

	// --- 선택지 View -------------------------------------------------------

	// 정의의 Options 를 UI 용 값으로 복사한다. null 선택지도 자리를 채우고, MinStaff 는 1 이상으로 정리한다.
	static void BuildOptionViews(const UFF_IncidentDefinition& Definition, TArray<FFF_IncidentOptionView>& OutViews);
};
