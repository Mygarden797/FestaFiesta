#include "FF_FFZSFacilityLightingRig.h"

#include "Components/LightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AFFZSFacilityLightingRig::AFFZSFacilityLightingRig()
{
    PrimaryActorTick.bCanEverTick = false;
    LightingRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LightingRoot"));
    LightingRoot->SetMobility(EComponentMobility::Movable);
    SetRootComponent(LightingRoot);
    SetActorEnableCollision(false);
}

void AFFZSFacilityLightingRig::ApplyLightingState(const FFFZSFacilityLightingState& State,
    const bool bPresentationAllowed, UMeshComponent* FacilityMesh)
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
        Entry.bUseTemperature = Light->bUseTemperature;
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
            Light->SetUseTemperature(State.bOverrideColor ? false : Entry.bUseTemperature);
            Light->SetLightColor(State.bOverrideColor ? State.Color : Entry.Color, true);
            if (ULocalLightComponent* LocalLight = Cast<ULocalLightComponent>(Light))
            {
                LocalLight->SetAttenuationRadius(Entry.AttenuationRadius * RangeScale);
            }
        }
    }

    ApplyEmissiveState(State, bPresentationAllowed, FacilityMesh);
}

void AFFZSFacilityLightingRig::ApplyEmissiveState(const FFFZSFacilityLightingState& State,
    const bool bPresentationAllowed, UMeshComponent* FacilityMesh)
{
    if (!FacilityMesh) return;
    static const FName BrightnessParameter(TEXT("FF_LightingBrightness"));
    static const FName ColorParameter(TEXT("FF_LightingColor"));
    static const FName OverrideParameter(TEXT("FF_LightingUseColorOverride"));
    EmissiveMaterials.RemoveAll([](const TWeakObjectPtr<UMaterialInstanceDynamic>& Material)
        { return !Material.IsValid(); });

    for (int32 Index = 0; Index < FacilityMesh->GetNumMaterials(); ++Index)
    {
        UMaterialInterface* Source = FacilityMesh->GetMaterial(Index);
        float Brightness = 0.0f;
        if (!Source || !Source->GetScalarParameterValue(FMaterialParameterInfo(BrightnessParameter), Brightness)) continue;
        UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Source);
        if (!Material || !EmissiveMaterials.Contains(Material))
        {
            // MIDs cannot parent other MIDs. Preserve external overrides on a new sibling instance
            // so a shared supplied material still cannot leak one facility's controls into another.
            UMaterialInstanceDynamic* SourceInstance = Material;
            Material = UMaterialInstanceDynamic::Create(SourceInstance ? SourceInstance->Parent.Get() : Source, FacilityMesh);
            if (!Material) continue;
            if (SourceInstance) Material->CopyParameterOverrides(SourceInstance);
            FacilityMesh->SetMaterial(Index, Material);
            EmissiveMaterials.Add(Material);
        }
        // This parameter is a multiplier, not the material's authored emissive strength.
        // Write the absolute state so toggling, repeated edits and restoring never compound it.
        Material->SetScalarParameterValue(BrightnessParameter,
            bPresentationAllowed && State.bEnabled ? State.BrightnessMultiplier : 0.0f);
        Material->SetVectorParameterValue(ColorParameter, State.Color);
        Material->SetScalarParameterValue(OverrideParameter, State.bOverrideColor ? 1.0f : 0.0f);
    }
}
