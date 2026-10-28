#include "FF_MapOverviewMarkerWidget.h"
#include "Components/Image.h"

void UFFMapOverviewMarkerWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyDesignerAppearance();
}

void UFFMapOverviewMarkerWidget::SetMarkerData(const EFFMapOverviewMarkerKind InKind, const FText& InDisplayName)
{
    MarkerKind = InKind;
    SetToolTipText(InDisplayName);
    ApplyDesignerAppearance();
}

void UFFMapOverviewMarkerWidget::ApplyDesignerAppearance()
{
    const FSlateBrush* Brush = &FacilityBrush;
    FLinearColor Color = FacilityColor;
    switch (MarkerKind)
    {
    case EFFMapOverviewMarkerKind::Attraction: Brush = &AttractionBrush; Color = AttractionColor; break;
    case EFFMapOverviewMarkerKind::Service: Brush = &ServiceBrush; Color = ServiceColor; break;
    case EFFMapOverviewMarkerKind::Camera: Brush = &CameraBrush; Color = CameraColor; break;
    default: break;
    }
    if (Background) Background->SetColorAndOpacity(Color);
    if (Icon)
    {
        if (Brush->GetResourceObject() || !Brush->GetResourceName().IsNone()) Icon->SetBrush(*Brush);
        Icon->SetColorAndOpacity(IconColor);
    }
}
