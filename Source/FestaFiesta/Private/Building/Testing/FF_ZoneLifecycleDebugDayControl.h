// FF_ZoneLifecycleDebugDayControl.h
// 역할: 개발용 시설 점검 패널에서 이전 날짜와 다음 날짜로 이동하는 내부 함수를 선언한다.

#pragma once

class UFF_FestivalStageSubsystem;

/** Internal day navigation used only by the Zone Lifecycle debug panel and its tests. */
namespace FFZoneLifecycleDebugDayControl
{
	bool TryMoveToPreviousDay(UFF_FestivalStageSubsystem& Stage);
	bool TryMoveToNextDay(UFF_FestivalStageSubsystem& Stage);
}
