// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FacilityPaidProgramTypes.generated.h"

/**
 * 시설별 유료 프로그램 카드의 불변 UI 스냅샷.
 *
 * WBP는 이 값만 표시한다. 경제/시설 서브시스템 조회와 구매 판정은 FacilityInfoSubsystem이
 * 담당하므로 디자이너 Blueprint가 게임플레이 상태를 직접 결합하지 않는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilityPaidProgramCardViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	FName ProgramId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	FText TimeText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	FText CostText;


	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	FText CapacityText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	FText TicketFeeText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	FText AvailabilityText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	int32 CostGold = 0;


	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	int32 CapacityPeople = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	int32 TicketFeeGold = 0;

	/** Capacity multiplied by ticket fee, not guaranteed revenue or net profit. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	int32 MaxRevenueGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	bool bCanSchedule = false;
};

/** One complete render pass for the facility-scoped picker. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilityPaidProgramPickerViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	FGuid FacilityInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	FText FacilityName;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	int32 AvailableGold = 0;


	/** Only facility-compatible cards are projected into this array. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	TArray<FFF_FacilityPaidProgramCardViewData> Cards;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	FText FeedbackText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Facility Info|Paid Program")
	bool bFeedbackIsError = false;
};

