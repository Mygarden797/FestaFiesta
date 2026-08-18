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
}
