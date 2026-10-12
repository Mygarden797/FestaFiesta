#pragma once

#include "CoreMinimal.h"

namespace FFUIForegroundRules
{
    /** HUD bar controls remain available; open windows cover the background facility UI. */
    FESTAFIESTA_API bool HasForegroundWindow(const UObject* ContextObject, bool bIncludeBuildWindows = true);
}
