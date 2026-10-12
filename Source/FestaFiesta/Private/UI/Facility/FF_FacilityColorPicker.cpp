// Facility-only hue/saturation disk. The WBP owns its exact size and placement.
#include "UI/Facility/FF_FacilityColorPicker.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Colors/SColorWheel.h"

class SFFFacilityColorPicker : public SColorWheel
{
public:
    void Construct(const SColorWheel::FArguments& InArgs)
    {
        SColorWheel::Construct(InArgs);
        ShowSelector(false);
    }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
    {
        LayerId = SColorWheel::OnPaint(Args, AllottedGeometry, MyCullingRect,
            OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

        // Enlarge only the lighting selector; native color coordinates and drag input stay intact.
        const FSlateBrush* Selector = FCoreStyle::Get().GetBrush("ColorWheel.Selector");
        const FVector2f SelectorSize = Selector->ImageSize * 5.f;
        const FVector2f CircleCenter = AllottedGeometry.GetLocalSize() * 0.5f;
        const FVector2f SelectorLocation = CircleCenter +
            (CalcRelativePositionFromCenter() * CircleCenter) - (0.5f * SelectorSize);
        // Vector outlines stay smooth when enlarged, with contrast on every wheel color.
        const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
        const FSlateRoundedBoxBrush OuterRing(FLinearColor::Transparent, FLinearColor::Black * Tint, 4.f);
        const FSlateRoundedBoxBrush InnerRing(FLinearColor::Transparent, FLinearColor::White * Tint, 2.f);
        const ESlateDrawEffect Effects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
        FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(SelectorSize, FSlateLayoutTransform(SelectorLocation)),
            &OuterRing, Effects, FLinearColor::Transparent);
        FSlateDrawElement::MakeBox(OutDrawElements, ++LayerId,
            AllottedGeometry.ToPaintGeometry(SelectorSize - FVector2f(2.f),
                FSlateLayoutTransform(SelectorLocation + FVector2f(1.f))),
            &InnerRing, Effects, FLinearColor::Transparent);
        return LayerId;
    }
};

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
    return SNew(SFFFacilityColorPicker)
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
