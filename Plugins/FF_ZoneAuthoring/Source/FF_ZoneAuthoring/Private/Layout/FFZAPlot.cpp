// FFZAPlot.cpp
// 역할: 구획의 잠금 상태 조회와 서브시스템 등록.
#include "FFZAPlot.h"

#include "Engine/World.h"
#include "FFZAZoneAuthoringSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FFZAPlot)

AFFZAPlot::AFFZAPlot()
{
    AreaColor = FLinearColor(0.25f, 0.25f, 0.28f, 1.0f);
}

bool AFFZAPlot::IsUnlocked() const
{
    const UWorld* World = GetWorld();
    const UFFZAZoneAuthoringSubsystem* Subsystem = World ? World->GetSubsystem<UFFZAZoneAuthoringSubsystem>() : nullptr;
    return Subsystem ? Subsystem->IsPlotActorUnlocked(*this) : bStartsUnlocked;
}

FLinearColor AFFZAPlot::GetOverlayColor() const
{
    return LockedColor;
}

bool AFFZAPlot::ShouldDrawOverlay() const
{
    // 해제된 구획은 일반 땅과 같으므로 오버레이를 그리지 않는다.
    return !IsUnlocked();
}

void AFFZAPlot::RegisterWithSubsystem(UFFZAZoneAuthoringSubsystem& Subsystem)
{
    Subsystem.RegisterPlot(this);
}

void AFFZAPlot::UnregisterFromSubsystem(UFFZAZoneAuthoringSubsystem& Subsystem)
{
    Subsystem.UnregisterPlot(this);
}
