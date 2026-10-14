#pragma once

#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "FF_MapOverviewMarkerWidget.generated.h"

class UImage;

UENUM(BlueprintType)
enum class EFFMapOverviewMarkerKind : uint8
{
    Facility,
    Attraction,
    Service,
    Camera
};

// The marker blueprint owns its hexagon, glyph, layout and authored brushes.
// Native code only projects data and chooses the designer-authored category.
UCLASS(Blueprintable)
class FF_MAPOVERVIEW_API UFFMapOverviewMarkerWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Map Overview|Marker")
    void SetMarkerData(EFFMapOverviewMarkerKind InKind, const FText& InDisplayName);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Marker") FVector2D MarkerSize = FVector2D(16, 16);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Marker") EFFMapOverviewMarkerKind MarkerKind = EFFMapOverviewMarkerKind::Facility;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Marker|Colors") FLinearColor FacilityColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Marker|Colors") FLinearColor AttractionColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Marker|Colors") FLinearColor ServiceColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Marker|Colors") FLinearColor CameraColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Marker|Colors") FLinearColor IconColor = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Marker|Glyphs") FSlateBrush FacilityBrush;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Marker|Glyphs") FSlateBrush AttractionBrush;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Marker|Glyphs") FSlateBrush ServiceBrush;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Map Overview|Marker|Glyphs") FSlateBrush CameraBrush;
protected:
    virtual void NativePreConstruct() override;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Background;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UImage> Icon;
private:
    void ApplyDesignerAppearance();
};
