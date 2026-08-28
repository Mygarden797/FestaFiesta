// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UFF_FestivalStageSubsystem;

/**
 * Live 경과 시각을 읽는 유일한 창구.
 *
 * ### 왜 창구가 필요한가
 * 프로그램 카드·예약 슬롯은 전부 **분**을 축으로 쓰는데 Stage 는 **초**를 들고 있다.
 * 그 변환과 "1 Stage초 = 1 경제분" 계약이 여러 곳에 흩어지면 배속이 붙거나 단위가 바뀔 때
 * 어디를 고쳐야 하는지 알 수 없게 된다. 여기 한 곳에만 둔다.
 *
 * 시각의 출처는 Stage(강민서) 하나다. 자체 시계를 들지 않는다.
 */
struct FESTAFIESTA_API FFF_FestivalClock
{
	// 1 Stage초 = 1 경제분. 밤 조명 트리거 90초와 선셋 요가 카드 90분이 같은 값인 것처럼,
	// 세 시스템이 이미 이 환산을 가정하고 있다. 다만 계약으로 적힌 곳이 여기뿐이다.
	static constexpr float MinutesPerStageSecond = 1.f;

	/**
	 * 지금 Live 단계면 그 경과 시각을 분으로 준다.
	 * @return Live 가 아니거나 Stage 가 없으면 false — 이때 OutMinutes 는 건드리지 않는다
	 */
	static bool TryGetLiveElapsedMinutes(const UFF_FestivalStageSubsystem* Stage, float& OutMinutes);

	/**
	 * 오늘 Live 의 총 길이를 분으로 준다. 카드 시각이 하루 안에 들어오는지 볼 때 쓴다.
	 * 날짜별로 다르다 — 실측 Day1 240 / Day2·3 300 이다. 단계와 무관하게 오늘 값을 준다.
	 */
	static bool TryGetLiveDurationMinutes(UFF_FestivalStageSubsystem* Stage, float& OutMinutes);
};
