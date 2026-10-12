// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/LiveHUD/Panels/FF_LivePanelFacilities.h"
#include "FF_LiveFacilityRowWidget.generated.h"

class UBorder;
class UTextBlock;

/** 시설 상태 표의 한 행. 크기·정렬·글꼴은 파생 WBP에서, 동적 문구와 배지 배경은 Class Defaults에서 편집한다. */
UCLASS(Abstract, meta = (DisplayName = "FF Live Facility Row"))
class FESTAFIESTA_API UFF_LiveFacilityRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live|Facilities")
	void SetRowData(const FFF_LiveFacilityRow& InRow);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text")
	FText MissingValueText = NSLOCTEXT("FestaFiestaLiveFacilityRow", "MissingValue", "—");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text", meta = (ToolTip = "{Count}: 팀 수"))
	FText TeamCountTextFormat = NSLOCTEXT("FestaFiestaLiveFacilityRow", "TeamCountFormat", "{Count}팀");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text", meta = (ToolTip = "{Seconds}: 지금 줄을 서면 예상되는 대기 시간"))
	FText ExpectedWaitTextFormat = NSLOCTEXT("FestaFiestaLiveFacilityRow", "ExpectedWaitFormat", "{Seconds}초");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text", meta = (ToolTip = "{Seconds}: 완료 고객의 실제 평균 대기 시간"))
	FText AverageWaitTooltipFormat = NSLOCTEXT("FestaFiestaLiveFacilityRow", "AverageWaitTooltipFormat", "완료 고객의 실제 평균 대기: {Seconds}초");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text")
	FText AverageWaitUnknownText = NSLOCTEXT("FestaFiestaLiveFacilityRow", "AverageWaitUnknown", "완료 고객의 실제 평균 대기: 집계 준비 중");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text")
	FText OkText = NSLOCTEXT("FestaFiestaLiveFacilityRow", "Ok", "정상 운영");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text")
	FText WatchText = NSLOCTEXT("FestaFiestaLiveFacilityRow", "Watch", "주의");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text")
	FText BlockedText = NSLOCTEXT("FestaFiestaLiveFacilityRow", "Blocked", "막힘");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text")
	FText UnknownText = NSLOCTEXT("FestaFiestaLiveFacilityRow", "Unknown", "미확인");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text")
	FText NeedsRelocationText = NSLOCTEXT("FestaFiestaLiveFacilityRow", "NeedsRelocation", "자리를 다시 잡아야 합니다");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text")
	FText QueueBlockedText = NSLOCTEXT("FestaFiestaLiveFacilityRow", "QueueBlocked", "줄이 막혀 손님을 못 받습니다");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text", meta = (ToolTip = "{State}: 현재 운영 상태"))
	FText NotActiveTextFormat = NSLOCTEXT("FestaFiestaLiveFacilityRow", "NotActiveFormat", "운영 중이 아닙니다 ({State})");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text")
	FText QueueFullText = NSLOCTEXT("FestaFiestaLiveFacilityRow", "QueueFull", "정원이 찼습니다");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text", meta = (ToolTip = "{Count}: 줄 서 있는 팀 수"))
	FText LongQueueTextFormat = NSLOCTEXT("FestaFiestaLiveFacilityRow", "LongQueueFormat", "{Count}팀이 줄 서 있습니다");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Text")
	FText UnknownReasonText = NSLOCTEXT("FestaFiestaLiveFacilityRow", "UnknownReason", "상태를 읽지 못했습니다");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Status")
	FLinearColor OkBackgroundColor = FLinearColor::FromSRGBColor(FColor(229, 245, 233));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Status")
	FLinearColor WatchBackgroundColor = FLinearColor::FromSRGBColor(FColor(255, 245, 220));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Status")
	FLinearColor BlockedBackgroundColor = FLinearColor::FromSRGBColor(FColor(255, 233, 225));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Facilities|Status")
	FLinearColor UnknownBackgroundColor = FLinearColor::FromSRGBColor(FColor(238, 242, 243));

protected:
	virtual void SynchronizeProperties() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Facilities")
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Facilities")
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Facilities")
	TObjectPtr<UTextBlock> Text_Reason;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Facilities")
	TObjectPtr<UTextBlock> Text_Waiting;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Facilities")
	TObjectPtr<UTextBlock> Text_InService;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Facilities")
	TObjectPtr<UTextBlock> Text_ExpectedWait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Facilities")
	TObjectPtr<UBorder> Border_Status;

private:
	void RefreshRow();
	FText GetReasonText() const;

	FFF_LiveFacilityRow RowData;
	bool bHasRowData = false;
};
