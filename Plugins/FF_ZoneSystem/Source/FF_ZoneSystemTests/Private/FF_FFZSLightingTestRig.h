#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSFacilityLightingRig.h"
#include "FF_FFZSLightingTestRig.generated.h"

class USpotLightComponent;

UCLASS()
class AFFZSLightingTestRig final : public AFFZSFacilityLightingRig
{
    GENERATED_BODY()
public:
    AFFZSLightingTestRig();
    UPROPERTY()
    TObjectPtr<USpotLightComponent> TestLight;
};
