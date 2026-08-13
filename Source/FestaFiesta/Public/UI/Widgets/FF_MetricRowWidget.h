// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_MetricRowWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * 지표 한 줄 = 캡션 + 숫자 + 구간 라벨 + 구간 아이콘.
 *
 * === 왜 뽑았나 ===
 * `UFF_MetricsHudWidget`이 지표 3개에 위젯을 9개 들고 있었다. 지표가 늘 때마다 세 줄씩
 * 늘어나는 구조라 Report·Overview까지 가면 되돌리기 어려워진다. 그 TODO를 여기서 해소한다.
 *
 * === CommonUI를 쓰지 않는다 ===
 * 값을 보여주기만 하고 입력을 받지 않는다. 실시간 표시 위젯은 일반 `UUserWidget`이다.
 * 근거: docs/ui/ui-tech-decision.md 7절
 *
 * === 화면 라벨에 내부 이름을 쓰지 않는다 ===
 * 캡션은 호출하는 쪽이 넘긴다. `SimSatisfaction` 같은 코드 이름이 화면에 나오면 안 된다.
 * 예) `SimSatisfaction` → "관객 만족도"
 *
 * === AT-011 ===
 * 구간을 색으로만 알리지 않는다. 라벨(좋음/주의/나쁨)과 아이콘을 함께 놓는다.
 * 구간 경계는 `UFF_UIMetricBandSettings`(Project Settings > Game)에서 읽고 하드코딩하지 않는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Metric Row Widget"))
class FESTAFIESTA_API UFF_MetricRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 캡션과 값을 함께 설정한다.
	 *
	 * 캡션과 값을 따로 세팅하는 API를 두지 않는다 — 캡션만 바뀌고 숫자가 안 바뀌는 상태를
	 * 만들 수 있고, 그러면 화면에 "안전: 62"처럼 틀린 조합이 뜬다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetMetric(const FText& InCaption, int32 InValue);

	/** 값만 갱신한다. 캡션은 한 번 정해지면 바뀌지 않는 화면에서 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetValue(int32 InValue);

	/**
	 * 구간 표시를 끈다. 0~100 지표가 아닌 값(금액 등)을 이 줄에 표시할 때 쓴다.
	 * 매출에 "좋음/주의/나쁨"을 붙이면 안 된다 — 구간 기준이 0~100 지표용이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetBandVisible(bool bVisible);

	/**
	 * 0~100 값에 맞는 구간을 라벨·아이콘에 반영한다. 둘 중 없는 것은 건너뛴다.
	 * 구간을 못 찾으면 임의의 색으로 칠하지 않고 비운다 — 틀린 색이 빈 칸보다 나쁘다.
	 *
	 * `UFF_MetricsHudWidget`에서 옮겨 왔다. static이라 위젯 인스턴스 없이도 쓸 수 있다.
	 */
	static void ApplyBand(int32 Value, UTextBlock* BandLabel, UImage* BandIcon);

protected:
	// --- 위젯 바인딩. WBP의 위젯 이름이 아래와 정확히 같아야 한다 ---

	/** 지표 이름. 화면 문구이며 내부 이름을 쓰지 않는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Caption;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Value;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Band;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_Band;

private:
	/** 마지막으로 반영한 값. 구간 표시를 다시 켤 때 이 값으로 복원한다. */
	int32 CurrentValue = 0;

	bool bBandVisible = true;

	void RefreshBand();
};
