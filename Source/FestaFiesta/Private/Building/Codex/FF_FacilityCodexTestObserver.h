#pragma once

#include "CoreMinimal.h"
#include "FF_FacilityCodexTestObserver.generated.h"

/** Observes public codex notifications in lifecycle regression tests. */
UCLASS()
class UFF_FacilityCodexTestObserver final : public UObject
{
	GENERATED_BODY()
public:
	int32 NotificationCount = 0;

	UFUNCTION()
	void HandleCodexChanged() { ++NotificationCount; }
};
