#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSFacilityLightingRig.generated.h"

class ULightComponent;
class UMaterialInstanceDynamic;
class UMeshComponent;
class USceneComponent;

// Blueprint에 Spot/Point/Rect Light를 추가해 개별 위치·색·밝기를 저작한다.
// 시설은 이 액터를 VisualMesh의 자식으로 생성하고 원본 값에 절대 배수를 적용한다.
UCLASS(Blueprintable)
class FF_ZONESYSTEM_API AFFZSFacilityLightingRig : public AActor
{
    GENERATED_BODY()

public:
    AFFZSFacilityLightingRig();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facility Lighting")
    TObjectPtr<USceneComponent> LightingRoot;

    UFUNCTION(BlueprintCallable, Category = "Facility Lighting")
    void ApplyLightingState(const FFFZSFacilityLightingState& State, bool bPresentationAllowed = true,
        UMeshComponent* FacilityMesh = nullptr);

    UFUNCTION(BlueprintPure, Category = "Facility Lighting")
    int32 GetAuthoredLightCount() const { return AuthoredLights.Num(); }

private:
    struct FAuthoredLight
    {
        TWeakObjectPtr<ULightComponent> Component;
        float Intensity = 0.0f;
        FLinearColor Color = FLinearColor::White;
        float AttenuationRadius = 0.0f;
        bool bVisible = true;
        bool bUseTemperature = false;
    };
    TArray<FAuthoredLight> AuthoredLights;

    // Only materials exposing FF_LightingBrightness opt in. The source asset remains unchanged,
    // and every facility gets its own dynamic instance for bulb glow as well as real illumination.
    TArray<TWeakObjectPtr<UMaterialInstanceDynamic>> EmissiveMaterials;
    void ApplyEmissiveState(const FFFZSFacilityLightingState& State, bool bPresentationAllowed,
        UMeshComponent* FacilityMesh);
};
