// Facility-only hue/saturation disk. The WBP owns its exact size and placement.
#include "UI/Facility/FF_FacilityColorPicker.h"
#include "Widgets/Colors/SColorWheel.h"

UFF_FacilityColorPicker::UFF_FacilityColorPicker(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UFF_FacilityColorPicker::SetColor(FLinearColor Color)
{
    CurrentColor = Color;
    if (const TSharedPtr<SWidget> Cached = GetCachedWidget())
        Cached->Invalidate(EInvalidateWidgetReason::Paint);
}

TSharedRef<SWidget> UFF_FacilityColorPicker::RebuildWidget()
{
    const TWeakObjectPtr<UFF_FacilityColorPicker> WeakThis(this);
    return SNew(SColorWheel)
        .SelectedColor_Lambda([WeakThis]()
        {
            const FColor RGB = WeakThis.IsValid() ? WeakThis->CurrentColor.ToFColorSRGB() : FColor::White;
            FLinearColor HSV = FLinearColor(RGB.R / 255.f, RGB.G / 255.f, RGB.B / 255.f).LinearRGBToHSV();
            // The disk selects hue/saturation at full value. Precision fields retain V control.
            HSV.B = 1.f;
            return HSV;
        })
        .OnMouseCaptureBegin(FSimpleDelegate::CreateWeakLambda(this, [this]() { OnInteractionStarted.Broadcast(); }))
        .OnValueChanged_Lambda([WeakThis](FLinearColor HSV)
        {
            if (!WeakThis.IsValid()) return;
            const FLinearColor RGB = HSV.HSVToLinearRGB();
            const FColor SRGB(FMath::RoundToInt(RGB.R * 255.f), FMath::RoundToInt(RGB.G * 255.f), FMath::RoundToInt(RGB.B * 255.f));
            WeakThis->CurrentColor = FLinearColor::FromSRGBColor(SRGB);
            WeakThis->OnColorPicked.Broadcast(WeakThis->CurrentColor);
        });
}
