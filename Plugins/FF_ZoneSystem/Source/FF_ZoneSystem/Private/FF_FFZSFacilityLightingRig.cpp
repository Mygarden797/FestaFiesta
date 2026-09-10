#include "FF_FFZSFacilityLightingRig.h"

#include "Components/LightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/SceneComponent.h"

AFFZSFacilityLightingRig::AFFZSFacilityLightingRig()
{
    PrimaryActorTick.bCanEverTick = false;
    LightingRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LightingRoot"));
    LightingRoot->SetMobility(EComponentMobility::Movable);
    SetRootComponent(LightingRoot);
    SetActorEnableCollision(false);
}

void AFFZSFacilityLightingRig::ApplyLightingState(const FFFZSFacilityLightingState& State, const bool bPresentationAllowed)
{
    if (!State.IsValid()) return;

    TInlineComponentArray<ULightComponent*> Lights(this);
    for (ULightComponent* Light : Lights)
    {
        if (!Light || AuthoredLights.ContainsByPredicate([Light](const FAuthoredLight& Entry)
            { return Entry.Component.Get() == Light; })) continue;
        FAuthoredLight& Entry = AuthoredLights.AddDefaulted_GetRef();
        Entry.Component = Light;
        Entry.Intensity = Light->Intensity;
        Entry.Color = Light->GetLightColor();
        Entry.bVisible = Light->IsVisible();
        if (const ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(Light))
        {
            Entry.AttenuationRadius = LocalLight->AttenuationRadius;
        }
        Light->SetMobility(EComponentMobility::Movable);
    }

    // 비균일 맞춤에서도 구형 감쇠 범위가 무대를 덮도록 가장 큰 축 배율을 사용한다.
    const float RangeScale = static_cast<float>(GetActorScale3D().GetAbsMax());
    for (const FAuthoredLight& Entry : AuthoredLights)
    {
        if (ULightComponent* Light = Entry.Component.Get())
        {
            const bool bOn = bPresentationAllowed && State.bEnabled && Entry.bVisible;
            Light->SetVisibility(bOn);
            Light->SetIntensity(bOn ? Entry.Intensity * State.BrightnessMultiplier : 0.0f);
            Light->SetLightColor(State.bOverrideColor ? State.Color : Entry.Color, true);
            if (ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(Light))
            {
                LocalLight->SetAttenuationRadius(Entry.AttenuationRadius * RangeScale);
            }
        }
    }
}
