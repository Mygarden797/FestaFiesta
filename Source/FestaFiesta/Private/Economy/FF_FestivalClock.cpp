// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_FestivalClock.h"

#include "Time/FF_FestivalStageSubsystem.h"

bool FFF_FestivalClock::TryGetLiveElapsedMinutes(
	const UFF_FestivalStageSubsystem* Stage, float& OutMinutes)
{
	if (Stage == nullptr || !Stage->IsLiveStage())
	{
		return false;
	}

	// public 멤버를 그대로 읽는다. T2 Getter 가 생기면 이 한 줄만 바뀐다.
	OutMinutes = Stage->ElapsedLiveSeconds * MinutesPerStageSecond;
	return true;
}

bool FFF_FestivalClock::TryGetLiveDurationMinutes(
	UFF_FestivalStageSubsystem* Stage, float& OutMinutes)
{
	if (Stage == nullptr)
	{
		return false;
	}

	const float DurationSeconds = Stage->GetLiveDurationForCurrentDay();
	if (DurationSeconds <= 0.f)
	{
		// 표에 오늘 이벤트가 없다. 0 을 길이로 쓰면 모든 카드가 하루를 넘는 것으로 보인다.
		return false;
	}

	OutMinutes = DurationSeconds * MinutesPerStageSecond;
	return true;
}
