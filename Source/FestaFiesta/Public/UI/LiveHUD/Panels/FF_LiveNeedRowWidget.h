#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/LiveHUD/Panels/FF_LivePanelGuests.h"
#include "FF_LiveNeedRowWidget.generated.h"

class UBorder;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;

/** 욕구 미해소율 한 행. 배치·글꼴은 Designer, 동적 상태색·문구는 Class Defaults에서 편집한다. */
UCLASS(Abstract, meta = (DisplayName = "FF Live Need Row"))
class FESTAFIESTA_API UFF_LiveNeedRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live|Guests")
	void SetRowData(const FFF_LiveNeedRow& InRow);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Icons")
	TMap<EFF_NeedType, TObjectPtr<UTexture2D>> NeedIcons;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText MissingValueText = NSLOCTEXT("FestaFiestaLiveNeedRow", "MissingValue", "—");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text", meta = (ToolTip = "{Percent}: 미해소율 0~100"))
	FText PercentTextFormat = NSLOCTEXT("FestaFiestaLiveNeedRow", "PercentFormat", "{Percent}%");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText OkText = NSLOCTEXT("FestaFiestaLiveNeedRow", "Ok", "괜찮음");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText WatchText = NSLOCTEXT("FestaFiestaLiveNeedRow", "Watch", "주의");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText BadText = NSLOCTEXT("FestaFiestaLiveNeedRow", "Bad", "나쁨");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText UnknownText = NSLOCTEXT("FestaFiestaLiveNeedRow", "Unknown", "집계 준비 중");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Status")
	FLinearColor OkBackgroundColor = FLinearColor::FromSRGBColor(FColor(229, 245, 233));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Status")
	FLinearColor WatchBackgroundColor = FLinearColor::FromSRGBColor(FColor(255, 245, 220));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Status")
	FLinearColor BadBackgroundColor = FLinearColor::FromSRGBColor(FColor(255, 233, 225));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Status")
	FLinearColor UnknownBackgroundColor = FLinearColor::FromSRGBColor(FColor(238, 242, 243));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Status")
	FLinearColor OkBarColor = FLinearColor::FromSRGBColor(FColor(93, 174, 143));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Status")
	FLinearColor WatchBarColor = FLinearColor::FromSRGBColor(FColor(229, 169, 62));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Status")
	FLinearColor BadBarColor = FLinearColor::FromSRGBColor(FColor(216, 112, 91));

protected:
	virtual void SynchronizeProperties() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UTextBlock> Text_Percent;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UTextBlock> Text_Remedy;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UProgressBar> Progress_Unmet;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UBorder> Border_Status;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UImage> Image_NeedIcon;

private:
	void RefreshRow();
	FFF_LiveNeedRow RowData;
	bool bHasRowData = false;
};
