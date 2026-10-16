// FF_FFZSBuildPresentationSettings.h
// 역할: 격자·예약 범위·배치 고스트의 색, 선 두께, 재질을 Project Settings에서 조정하게 한다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_FFZSBuildPresentationSettings.generated.h"

class UMaterialInterface;

// Presentation only. These values never enter a command, placement mask, queue or save snapshot.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FF Build Presentation"))
class FF_ZONESYSTEM_API UFFZSBuildPresentationSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UFFZSBuildPresentationSettings();

    // 생성자의 초기 재질을 이 설정에서 교체할 수 있다. 기존 기본 외형은 그대로 유지한다.
    // 개별 PreviewActor가 재질을 지정한 경우에는 액터 값이 우선한다.
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement Feedback")
    TSoftObjectPtr<UMaterialInterface> DefaultPlacementFeedbackMaterial;

    // Generated translucent copies preserve each source material's textures and parameters.
    // Soft references also keep these variants reachable when the project is cooked.
    UPROPERTY(Config, EditAnywhere, Category = "Placement Feedback")
    TMap<FSoftObjectPath, TSoftObjectPtr<UMaterialInterface>> GeneratedPreviewMaterials;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement Feedback")
    FLinearColor ValidPlacementColor = FLinearColor(0.015f, 0.65f, 1.0f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement Feedback")
    FLinearColor InvalidPlacementColor = FLinearColor(0.85f, 0.035f, 0.025f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement Feedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ValidPlacementOpacity = 0.68f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement Feedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float InvalidPlacementOpacity = 0.72f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placement Feedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FailureSurfaceOpacity = 0.18f;

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
    FLinearColor FootprintColor = FLinearColor(0.18f, 0.50f, 0.96f, 0.92f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges")
    FLinearColor QueueColor = FLinearColor(0.03f, 0.74f, 0.64f, 0.95f);

    // Retained only to read existing config; safety perimeter outlines are no longer drawn.
    UPROPERTY(Config)
    FLinearColor ClearanceColor = FLinearColor(0.96f, 0.66f, 0.12f, 0.92f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges")
    FLinearColor AudienceColor = FLinearColor(0.68f, 0.35f, 0.96f, 0.95f);

    // Exact construction boundary, independent of the optional placement grid.
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges")
    FLinearColor BuildBoundaryColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.98f);

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges")
    FLinearColor FailureColor = FLinearColor(0.86f, 0.12f, 0.08f, 0.95f);

    // Screen pixels, rendered by UFFZSBuildRangeComponent independently of camera zoom.
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges", meta = (ClampMin = "0.5", ClampMax = "6.0"))
    float RangeLineThickness = 1.5f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges", meta = (DisplayName = "Audience Dash Length", ClampMin = "1.0", ClampMax = "200.0", Units = "cm"))
    float ClearanceDashLengthCm = 35.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges", meta = (DisplayName = "Audience Dash Gap", ClampMin = "1.0", ClampMax = "200.0", Units = "cm"))
    float ClearanceDashGapCm = 25.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges", meta = (ClampMin = "0.5", ClampMax = "20.0", Units = "cm"))
    float RangeSurfaceLiftCm = 4.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges|Queue")
    bool bShowQueueSlots = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ranges|Queue", meta = (ClampMin = "2.0", ClampMax = "50.0", Units = "cm"))
    float QueueSlotRadiusCm = 12.0f;
};
