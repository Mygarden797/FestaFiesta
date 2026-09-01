// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace FFSurfingInputFocusPolicy
{
	/** Pure policy kept separate so focus-repair guardrails remain regression tested. */
	inline bool ShouldRepairFocus(
		const bool bScreenActive,
		const bool bSurfingRunPlaying,
		const bool bApplicationActive,
		const bool bGamePaused,
		const bool bScreenHasFocus)
	{
		return bScreenActive
			&& bSurfingRunPlaying
			&& bApplicationActive
			&& !bGamePaused
			&& !bScreenHasFocus;
	}
}

