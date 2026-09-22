// FF_BuildSynergyWidget.h
// 역할: 시설 조합의 예상 효과와 거리, 연결선을 표시한다.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Building/Synergy/FF_FacilitySynergyTypes.h"
#include "FF_BuildSynergyWidget.generated.h"

class UBorder;
class UTextBlock;

// Plain, bounded rendering data. The widget never queries actors or evaluates rules while painting.
struct FFF_BuildSynergyMarker
{
    FVector WorldLocation = FVector::ZeroVector;
    bool bActive = false;
    int32 RowNumber = 0;
};

/** WBP owns layout/typography. C++ projects rule data and bounded, shipping-safe world connections. */
UCLASS(Blueprintable, meta = (DisableNativeTick))
class FESTAFIESTA_API UFF_BuildSynergyWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFF_BuildSynergyWidget(const FObjectInitializer& ObjectInitializer);
    void PresentPreview(const FFF_FacilitySynergyPreview& Preview, const FVector& CandidateLocation);
    void ClearPreview();

    UPROPERTY(BlueprintReadOnly, Category = "Building|Synergy")
    FFF_FacilitySynergyPreview PreviewData;
    // Off by default: text colors authored in the WBP stay untouched. A designer can opt
    // into these state colors in Class Defaults, or implement BP_OnPreviewChanged.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Text", meta = (DisplayName = "상태별 제목 색 사용", ToolTip = "끄면 Designer에서 정한 글자 색을 유지합니다. 켜면 아래 적용/미적용 색을 사용합니다.")) bool bApplyStateTextColors = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Text", meta = (DisplayName = "효과 적용 제목 색")) FSlateColor ActiveTextColor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Text", meta = (DisplayName = "미적용 제목 색")) FSlateColor InactiveTextColor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Connections", meta = (DisplayName = "효과 적용 연결선 색", ToolTip = "조합 조건을 만족하는 시설을 실선으로 연결합니다.")) FLinearColor ActiveLineColor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Connections", meta = (DisplayName = "미적용 연결선 색", ToolTip = "거리 초과 등 아직 효과가 없는 시설을 점선으로 연결합니다.")) FLinearColor InactiveLineColor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Connections", meta = (DisplayName = "거리 원 불투명도", ClampMin = "0.0", ClampMax = "1.0")) float RangeOpacity = 0.6f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Connections", meta = (DisplayName = "거리 원 선 두께", ClampMin = "0.5", ClampMax = "12.0")) float RangeLineThickness = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Connections", meta = (DisplayName = "적용 연결선 두께", ClampMin = "0.5", ClampMax = "12.0")) float ActiveLineThickness = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Connections", meta = (DisplayName = "미적용 연결선 두께", ClampMin = "0.5", ClampMax = "12.0")) float InactiveLineThickness = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Markers", meta = (DisplayName = "시설 표시 크기", ToolTip = "연결된 시설을 둘러싼 마름모 표시의 반지름입니다.", ClampMin = "4.0", ClampMax = "48.0")) float MarkerRadius = 15.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Markers", meta = (DisplayName = "시설 표시 선 두께", ClampMin = "0.5", ClampMax = "12.0")) float MarkerThickness = 3.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Style|Markers", meta = (DisplayName = "시설 번호 글자 크기", ClampMin = "8", ClampMax = "32")) int32 MarkerFontSize = 12;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

    UFUNCTION(BlueprintImplementableEvent, Category = "Building|Synergy")
    void BP_OnPreviewChanged(const FFF_FacilitySynergyPreview& Data);
    UFUNCTION(BlueprintImplementableEvent, Category = "Building|Synergy")
    void BP_OnPreviewCleared();

    UPROPERTY(meta = (BindWidget)) TObjectPtr<UBorder> Border_Synergy;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SynergyLegend;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SynergyFooter;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SynergyTitle_1;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SynergyTitle_2;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SynergyTitle_3;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SynergyDetail_1;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SynergyDetail_2;
    UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Text_SynergyDetail_3;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FFFBuildSynergyPresentationTest;
#endif
    static constexpr int32 MaxVisibleRows = 3;
    static constexpr int32 CircleSegments = 48;

    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> RowTitles;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> RowDetails;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Footer;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Legend;
    UPROPERTY(Transient) TObjectPtr<UBorder> CardPanel;
    TArray<FFF_BuildSynergyMarker> Markers;
    TArray<FVector> RangePoints;
    FVector CandidateWorldLocation = FVector::ZeroVector;
    bool bFirstRangeActive = false;

    void BuildNativeTree();
    void ResolveBindings();
    bool HasRequiredBindings() const;
    FText ResolveFacilityName(FName DefinitionId) const;
};
