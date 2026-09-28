// FF_BuildFeedback.h
// 역할: 건설 조작 결과를 게임패드의 짧은 진동으로 알려 준다.

#pragma once

class APlayerController;

namespace FFBuildFeedback
{
    // A short, optional controller pulse. Pointer-only devices are unaffected.
    FESTAFIESTA_API void Pulse(APlayerController* Controller, bool bSuccess = true, bool bCommit = false);
}
