#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_FFZSBuildPresentationSettings.generated.h"

// Presentation only. These values never enter a command, placement mask, queue or save snapshot.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FF Build Presentation"))
class FF_ZONESYSTEM_API UFFZSBuildPresentationSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UFFZSBuildPresentationSettings();

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid")
    FLinearColor MinorLineColor = FLinearColor(0.36f, 0.63f, 0.60f, 0.22f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid")
    FLinearColor MajorLineColor = FLinearColor(0.48f, 0.76f, 0.71f, 0.42f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid")
    FLinearColor BoundaryLineColor = FLinearColor(0.61f, 0.83f, 0.75f, 0.62f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "0.5", ClampMax = "4.0"))
    float MinorLineThickness = 0.75f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "0.5", ClampMax = "4.0"))
    float MajorLineThickness = 1.15f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "0.5", ClampMax = "4.0"))
    float BoundaryLineThickness = 1.5f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HaloOpacity = 0.12f;

    // Screen spacing controls only which existing minor lines are drawn, never the mask/snap interval.
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid|Zoom", meta = (ClampMin = "2.0", ClampMax = "32.0"))
    float MinimumMinorSpacingPixels = 9.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Grid|Zoom", meta = (ClampMin = "0.0", ClampMax = "2000.0", Units = "cm"))
    float SelectionDetailPaddingCm = 250.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges")
    FLinearColor FootprintColor = FLinearColor(0.025f, 0.68f, 0.74f, 0.92f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges")
    FLinearColor QueueColor = FLinearColor(0.94f, 0.64f, 0.12f, 0.9f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges")
    FLinearColor ClearanceColor = FLinearColor(0.36f, 0.68f, 0.29f, 0.78f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges")
    FLinearColor FailureColor = FLinearColor(0.86f, 0.12f, 0.08f, 0.95f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges", meta = (ClampMin = "0.5", ClampMax = "6.0"))
    float RangeLineThickness = 1.5f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges", meta = (ClampMin = "1.0", ClampMax = "200.0", Units = "cm"))
    float ClearanceDashLengthCm = 35.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges", meta = (ClampMin = "1.0", ClampMax = "200.0", Units = "cm"))
    float ClearanceDashGapCm = 25.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges", meta = (ClampMin = "0.5", ClampMax = "20.0", Units = "cm"))
    float RangeSurfaceLiftCm = 4.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges|Queue")
    bool bShowQueueSlots = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges|Queue", meta = (ClampMin = "2.0", ClampMax = "50.0", Units = "cm"))
    float QueueSlotRadiusCm = 12.0f;
};
