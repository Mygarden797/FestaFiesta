// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_StatCard.generated.h"

class UBorder;
class UTextBlock;

/**
 * 라벨 + 큰 수치 + 보조문구 카드 하나.
 *
 * 결산·최종 결산·점검 견적서·홍보 예상치가 공유한다.
 *
 * === UFF_MetricRowWidget 과 무엇이 다른가 ===
 * `MetricRow` 는 **가로 한 줄**이고 0~100 지표 전용이라 `int32` 를 받고 구간(좋음/주의/나쁨)을
 * 붙인다. 이쪽은 **큰 수치 카드**이고 금액·등급·비율처럼 상한이 없거나 단위가 다른 값을 받는다.
 * 그래서 `FText` 를 받고 구간을 붙이지 않는다 — 매출에 "좋음"을 붙이면 안 된다.
 * 구간 배지가 필요하면 `UFF_StateChip` 을 옆에 둔다.
 *
 * === 값을 FText 로 받는 이유 ===
 * 서식은 부르는 쪽이 정한다. **Gold·Revenue 는 정수다** — 소수를 그대로 넘기면 화면에
 * `10,742.968` 이 뜬다. 천 단위 구분은 `FText::AsNumber` 를 쓴다.
 *
 * === 라벨과 값을 따로 세팅하는 API 를 기본으로 두지 않는다 ===
 * `MetricRow` 와 같은 이유다. 라벨만 바뀌고 숫자가 안 바뀌는 상태를 만들 수 있고, 그러면
 * 화면에 "오늘 매출: 82" 처럼 틀린 조합이 뜬다. 라벨이 고정된 화면에서만 `SetValue` 를 쓴다.
 *
 * === 값이 없으면 0 을 쓰지 않는다 ===
 * `SetUnavailable` 로 이유를 띄운다. Need 미해소율은 표본 0건일 때 `0%` 가 아니라
 * `데이터 없음` 이다 (module-ownership.md 3절). 날씨·뒷정리 효과처럼 Read Model 이 아직
 * 없는 값도 이 경로를 쓴다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Stat Card"))
class FESTAFIESTA_API UFF_StatCard : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 라벨과 값을 함께 설정한다. 카드를 처음 채울 때는 항상 이걸 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetStat(const FText& InLabel, const FText& InValue);

	/** 값만 갱신한다. 라벨이 한 번 정해지면 바뀌지 않는 화면에서만 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetValue(const FText& InValue);

	/** 보조문구. 비우면 칸을 숨긴다 — `-` 를 넣지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetCaption(const FText& InCaption);

	/**
	 * 값 자리에 이유를 띄운다. `데이터 없음` · `근거 문서 없음` · `효과 계수 미확정` 같은 문구다.
	 * 글자색을 InkFaint 로 낮춰 수치와 혼동되지 않게 한다. **폰트 크기는 건드리지 않는다** —
	 * 크기는 디자이너가 소유하고, 코드가 저장·복원하면 손으로 조절한 값과 갈라진다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetUnavailable(const FText& InReason);

	/**
	 * 근거가 없어 만들어 넣은 값에 붙이는 표시. `잠정` 배지를 켠다.
	 * 최종 점수 가중치·뒷정리 효과 계수처럼 합의 전인 값에 쓴다. 합의되면 끈다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetProvisional(bool bProvisional);

protected:
	virtual void NativePreConstruct() override;

	// --- 위젯 바인딩. WBP의 위젯 이름이 아래와 정확히 같아야 한다 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Label;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Value;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Caption;

	/** `잠정` 배지. 근거 문서가 없는 값에만 켠다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Provisional;

	/** 카드 배경. 팔레트의 InGame.Paper 로 칠한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UBorder> Border_Card;

private:
	/** 값 글자색을 정상(Ink) / 값 없음(InkFaint) 사이에서 되돌리기 위해 쓴다. */
	void ApplyValueColor(bool bAvailable);
};
