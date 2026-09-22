// FF_FacilityInfoSelection.h
// 역할: 시설 선택 표시의 위치·범위와 선택 유지 규칙을 계산한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

namespace FFFacilityInfoSelection
{
	enum class ETraceDecision : uint8
	{
		Reject,
		UseSelectionProxy,
		UseFootprintFallback
	};

	/**
	 * Keeps an occluded selection proxy from falling through to the ground-footprint resolver.
	 * The fallback exists only for facilities that do not expose a proxy on the selection channel.
	 */
	constexpr ETraceDecision ResolveTraceDecision(
		const bool bHasSelectionProxyHit,
		const bool bProxyIsRegisteredFacility,
		const bool bOccludedByExternalBlocker)
	{
		if (!bHasSelectionProxyHit)
		{
			return ETraceDecision::UseFootprintFallback;
		}
		if (!bProxyIsRegisteredFacility || bOccludedByExternalBlocker)
		{
			return ETraceDecision::Reject;
		}
		return ETraceDecision::UseSelectionProxy;
	}

	/** Hover 표시도 실제 클릭 입력과 같은 Live·입력·포인터 gate를 통과해야 한다. */
	constexpr bool ShouldResolveLiveSelectionHover(
		const bool bIsLiveStage,
		const bool bPickingAllowed,
		const bool bCursorVisible,
		const bool bPointerOverPanel)
	{
		return bIsLiveStage && bPickingAllowed && bCursorVisible && !bPointerOverPanel;
	}
}
