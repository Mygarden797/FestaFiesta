#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "FF_FacilityColorPicker.generated.h"

class SFFFacilityColorPicker;
DECLARE_MULTICAST_DELEGATE_OneParam(FFF_OnLightingColorPicked, FLinearColor);

/** A designer-placeable hue ring and saturation/value square. Values use sRGB HSV. */
UCLASS(meta=(DisplayName="FF Facility Color Picker"))
class FESTAFIESTA_API UFF_FacilityColorPicker : public UWidget
{

    GENERATED_BODY()
public:
    UFF_FacilityColorPicker(const FObjectInitializer& ObjectInitializer);
    FFF_OnLightingColorPicked OnColorPicked;
    FSimpleMulticastDelegate OnInteractionStarted;
    void SetColor(FLinearColor LinearColor);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UPROPERTY(Transient)
    FLinearColor CurrentColor = FLinearColor::White;
};
