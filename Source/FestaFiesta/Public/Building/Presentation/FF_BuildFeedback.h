#pragma once

class APlayerController;

namespace FFBuildFeedback
{
    // A short, optional controller pulse. Pointer-only devices are unaffected.
    FESTAFIESTA_API void Pulse(APlayerController* Controller, bool bSuccess = true, bool bCommit = false);
}
