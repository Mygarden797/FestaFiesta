#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/LiveHUD/Panels/FF_LivePanelDensity.h"
#include "FF_LiveDensityLegendRowWidget.generated.h"

class UBorder;
class UTextBlock;

/** 밀도 범례 한 행. 배치·글꼴은 Designer, 강조색·문구는 Class Defaults에서 편집한다. */
UCLASS(Abstract, meta = (DisplayName = "FF Live Density Legend Row"))
class FESTAFIESTA_API UFF_LiveDensityLegendRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live|Density")
	void SetRowData(const FFF_DensityLegendRow& InRow);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live|Density")
	void SetCurrentPeak(bool bInCurrentPeak);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Style")
	FLinearColor RowBackgroundColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Style")
	FLinearColor PeakBackgroundColor = FLinearColor::FromSRGBColor(FColor(229, 244, 251));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text", meta = (ToolTip = "{Grade}: A~F, {Name}: 등급 이름"))
	FText GradeTextFormat = NSLOCTEXT("FestaFiestaLiveDensityLegend", "GradeFormat", "{Grade} · {Name}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text", meta = (ToolTip = "{Min}: 하한, {Max}: 상한. 상한은 포함하지 않습니다."))
	FText BoundedRangeTextFormat = NSLOCTEXT("FestaFiestaLiveDensityLegend", "BoundedFormat", "{Min} 이상 · {Max} 미만 명/㎡");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text", meta = (ToolTip = "{Min}: 최상위 등급의 하한"))
	FText OpenRangeTextFormat = NSLOCTEXT("FestaFiestaLiveDensityLegend", "OpenFormat", "{Min}명/㎡ 이상");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText CurrentPeakText = NSLOCTEXT("FestaFiestaLiveDensityLegend", "CurrentPeak", "현재 최대");

protected:
	virtual void SynchronizeProperties() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UBorder> Border_Row;

	/** 이 색만은 바닥 히트맵과 같아야 하므로 Crowd 원본 LOSToColor를 그대로 쓴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UBorder> Border_GradeColor;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UBorder> Border_CurrentPeak;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_Grade;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_Range;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_CurrentPeak;

private:
	void RefreshRow();
	void RefreshPeak();
	FFF_DensityLegendRow RowData;
	bool bHasRowData = false;
	bool bCurrentPeak = false;
};
