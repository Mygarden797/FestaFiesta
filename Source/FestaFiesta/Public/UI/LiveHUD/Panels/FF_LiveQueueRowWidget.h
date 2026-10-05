// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "FF_LiveQueueRowWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UTexture2D;

/** 대기열 표의 한 행. 크기·글꼴·아이콘 색은 Designer, 상태별 문구·배경은 Class Defaults에서 편집한다. */
UCLASS(Abstract, meta = (DisplayName = "FF Live Queue Row"))
class FESTAFIESTA_API UFF_LiveQueueRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFF_LiveQueueRowWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live|Queues")
	void SetRowData(const FFF_LiveFacilityView& InRow);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Status")
	TMap<EFF_LiveFacilityQueueState, TObjectPtr<UTexture2D>> StateIcons;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Status")
	TMap<EFF_LiveFacilityQueueState, FText> StateLabels;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Status")
	TMap<EFF_LiveFacilityQueueState, FLinearColor> StateBackgroundColors;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text")
	FText MissingValueText = NSLOCTEXT("FestaFiestaLiveQueueRow", "MissingValue", "—");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text", meta = (ToolTip = "{Count}: 팀 수"))
	FText TeamCountTextFormat = NSLOCTEXT("FestaFiestaLiveQueueRow", "TeamCount", "{Count}팀");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text", meta = (ToolTip = "{Seconds}: 지금 줄을 서면 예상되는 대기 시간"))
	FText ExpectedWaitTextFormat = NSLOCTEXT("FestaFiestaLiveQueueRow", "ExpectedWait", "{Seconds}초");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text", meta = (ToolTip = "{Seconds}: 완료 고객의 실제 평균 대기 시간"))
	FText AverageWaitTooltipFormat = NSLOCTEXT("FestaFiestaLiveQueueRow", "AverageWaitTooltip", "완료 고객의 실제 평균 대기: {Seconds}초");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text")
	FText AverageWaitUnknownText = NSLOCTEXT("FestaFiestaLiveQueueRow", "AverageWaitUnknown", "완료 고객의 실제 평균 대기: 집계 준비 중");

protected:
	virtual void SynchronizeProperties() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_Name;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_Waiting;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_InService;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_ExpectedWait;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UBorder> Border_Status;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UImage> Image_Status;

private:
	void RefreshRow();
	FFF_LiveFacilityView RowData;
	bool bHasRowData = false;
};
