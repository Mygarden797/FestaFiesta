// FF_FFZSLightingTestRig.h
// 역할: 조명 테스트에서 제어 가능한 임시 라이트 리그를 제공한다. 배포용 시설 콘텐츠는 아니다.
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
