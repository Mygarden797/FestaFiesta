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
