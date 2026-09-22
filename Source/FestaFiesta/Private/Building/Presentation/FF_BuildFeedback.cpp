#include "Building/Presentation/FF_BuildFeedback.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace
{
    TAutoConsoleVariable<int32> BuildHaptics(TEXT("FF.UI.BuildHaptics"), 1,
        TEXT("Optional construction controller feedback. 0 disables vibration; visual feedback remains."));
}

void FFBuildFeedback::Pulse(APlayerController* Controller, bool bSuccess, bool bCommit)
{
    if (!Controller || !Controller->IsLocalController() || !Controller->bForceFeedbackEnabled ||
        BuildHaptics.GetValueOnGameThread() == 0 || !FSlateApplication::IsInitialized() ||
        !FSlateApplication::Get().IsGamepadAttached() || !FSlateApplication::Get().IsActive()) return;
    // Throttle continuous road placement as well as rapid repeated menu presses.
    static TWeakObjectPtr<APlayerController> LastController;
    static double LastPulse = -1.0;
    const double Now = FPlatformTime::Seconds();
    if (LastController == Controller && Now - LastPulse < 0.12) return;
    LastController = Controller;
    LastPulse = Now;
    Controller->PlayDynamicForceFeedback(bSuccess ? (bCommit ? 0.22f : 0.10f) : 0.16f,
        bCommit ? 0.065f : 0.035f, false, true, false, true);
}
