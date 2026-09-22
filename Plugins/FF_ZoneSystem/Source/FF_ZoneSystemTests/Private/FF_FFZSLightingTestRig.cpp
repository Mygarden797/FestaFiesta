// FF_FFZSLightingTestRig.cpp
// 역할: 조명 테스트에서 제어 가능한 임시 라이트 리그를 제공한다. 배포용 시설 콘텐츠는 아니다.
#include "FF_FFZSLightingTestRig.h"
#include "Components/SpotLightComponent.h"

AFFZSLightingTestRig::AFFZSLightingTestRig()
{
    TestLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("TestLight"));
    TestLight->SetupAttachment(LightingRoot);
    TestLight->SetMobility(EComponentMobility::Movable);
    TestLight->SetRelativeLocation(FVector(17.0, -31.0, 93.0));
    TestLight->SetIntensity(1000.0f);
    TestLight->SetLightColor(FLinearColor(0.15f, 0.5f, 0.8f));
    TestLight->SetUseTemperature(true);
    TestLight->SetTemperature(2700.0f);
    TestLight->SetAttenuationRadius(400.0f);
}
