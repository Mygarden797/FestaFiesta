#pragma once

class UFF_FestivalStageSubsystem;

/** Internal day navigation used only by the Zone Lifecycle debug panel and its tests. */
namespace FFZoneLifecycleDebugDayControl
{
	bool TryMoveToPreviousDay(UFF_FestivalStageSubsystem& Stage);
	bool TryMoveToNextDay(UFF_FestivalStageSubsystem& Stage);
}
