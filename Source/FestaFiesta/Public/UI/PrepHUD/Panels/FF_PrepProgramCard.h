// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "UI/PrepHUD/Panels/FF_PrepDetailCard.h"
#include "FF_PrepProgramCard.generated.h"

class UTextBlock;

/**
 * 준비 단계 `프로그램` 패널의 카드 한 장.
 *
 * 상태·배지·색 재선언은 `UFF_PrepDetailCard` 가 갖는다. 여기는 **수치 네 줄만** 얹는다.
 *
 * === 카드가 스스로 판정하지 않는다 ===
 * 잠금 여부와 배지 문구는 패널이 `FFF_PrepProgramRules` 로 정해서 넣어 준다.
 * 카드가 잔액을 읽기 시작하면 같은 판정이 두 곳에 생긴다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Program Card"))
class FESTAFIESTA_API UFF_PrepProgramCard : public UFF_PrepDetailCard
{
	GENERATED_BODY()

public:
	/**
	 * 카드 내용을 채운다. **선택 id 는 카탈로그 안의 순번**이다 —
	 * `FName` 을 `int32` 로 넣을 수 없어서 패널이 순번으로 되돌려 해석한다.
	 *
	 * @param InIndex        카탈로그 순번. 클릭 알림에 그대로 실려 온다
	 * @param InCard         표시할 카드
	 * @param InTimeRangeText `19:30 ~ 20:00` 같은 시간대 문구. **패널이 만들어서 넣는다** —
	 *                        Live 경과분을 시계 문자열로 바꾸는 규칙은 시간 시스템이 소유한다
	 */
	void ConfigureCard(int32 InIndex, const FFF_ProgramCard& InCard, const FText& InTimeRangeText);

	/** 시설을 고르기 전에는 정원만, 고른 후에는 그 시설의 참가비로 계산한 매출을 표시한다. */
	void SetRevenuePreview(const FFF_ProgramCard* FacilityCard);

protected:
	virtual void CollectStateTexts(TArray<UTextBlock*>& OutTexts) const override;

	// --- 위젯 바인딩. 전부 Optional 이다 ---
	// 목업의 일부만 만든 WBP 도 통과시킨다. `Button_Root` 만 베이스가 강제한다.

	/** `200 Gold`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UTextBlock> Text_Cost;


	/** `정원 12명 · 확정 360`. 정원이 보장되므로 `예상` 이 아니다. */

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UTextBlock> Text_Capacity;

	/** 시간대. 패널이 넣어 준다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UTextBlock> Text_TimeRange;

private:
	FFF_ProgramCard Card;
};
