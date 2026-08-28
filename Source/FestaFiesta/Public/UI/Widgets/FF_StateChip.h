// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_StateChip.generated.h"

class UBorder;
class UImage;
class UTexture2D;
class UTextBlock;

/**
 * 상태 배지 하나 — 아이콘 + 문구를 색과 함께 쓴다.
 *
 * === AT-011 을 API 층에서 지킨다 ===
 * **색만 받는 함수를 두지 않는다.** 두 함수 모두 문구를 필수 인자로 받는다.
 * 색맹 사용자에게 색은 전달되지 않으므로 문구 없는 배지는 상태를 못 알린다.
 * 규칙을 주석이 아니라 시그니처로 강제하면 화면이 12개로 늘어도 자동으로 따라간다.
 *
 * === 구간 로직을 다시 쓰지 않는다 ===
 * 0~100 지표는 `UFF_MetricRowWidget::ApplyBand` 를 그대로 재사용한다. 구간 경계·라벨·아이콘은
 * `UFF_UIMetricBandSettings` 가 소유하며 여기서 다시 판정하지 않는다.
 * 색 판정이 두 곳에 있으면 조용히 갈라진다.
 *
 * === 왜 배경이 아니라 글자·아이콘에 색을 넣는가 ===
 * 칩 배경은 `InGame.PaperSunken` 으로 고정하고 **아이콘과 문구에 상태색**을 넣는다.
 * 배경을 상태색으로 칠하면 `ApplyBand` 가 아이콘을 같은 색으로 틴트하므로 아이콘이 사라진다.
 * 작은 크기에서는 중립 배경 위의 색 글자가 색 배경 위의 흰 글자보다 잘 읽힌다.
 *
 * === 두 가지 쓰임 ===
 *   1. `SetMetricState(62)`      — 만족도·안전 같은 0~100 지표. 구간이 붙는다
 *   2. `SetCustomState(...)`     — 가동/폐쇄처럼 구간이 없는 상태
 *
 * 시설 상태 배지를 `bAcceptingNew` 로 만들지 않는다 (C-027). 폐쇄만 상태로 쓰고
 * 혼잡은 대기시간으로 표현한다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF State Chip"))
class FESTAFIESTA_API UFF_StateChip : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 0~100 지표의 구간을 반영한다. 문구·아이콘·색이 `UFF_UIMetricBandSettings` 에서 함께 온다.
	 * 구간을 못 찾으면 **임의의 색으로 칠하지 않고 칩을 숨긴다** — 틀린 색이 빈 칸보다 나쁘다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetMetricState(int32 Value);

	/**
	 * 구간이 없는 상태를 직접 넣는다.
	 *
	 * @param InLabel  화면 문구. **비울 수 없다** — 비우면 색만 남아 AT-011 을 못 지킨다
	 * @param InColor  문구와 아이콘에 쓸 상태색. 팔레트에서 가져온다
	 * @param InIcon   흰색 단색 텍스처. InColor 로 틴트된다. null 이면 아이콘 칸을 숨긴다
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetCustomState(const FText& InLabel, const FLinearColor& InColor, UTexture2D* InIcon);

	/** 칩 전체를 숨긴다. 표시할 상태가 없을 때 쓴다 — 빈 칩을 남기지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetChipVisible(bool bVisible);

protected:
	virtual void NativePreConstruct() override;

	// --- 위젯 바인딩. WBP의 위젯 이름이 아래와 정확히 같아야 한다 ---

	/** 칩 배경. 상태와 무관하게 InGame.PaperSunken 으로 고정한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UBorder> Border_Chip;

	/**
	 * 상태 아이콘. **구간마다 실루엣이 달라야 한다** — 흑백으로 뽑았을 때 구분되지 않으면
	 * AT-011 을 못 지킨다. 브러시 `ImageSize` 는 생성기가 24x24 로 박아 둔다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_Icon;

	/** 상태 문구. 내부 이름을 쓰지 않는다 — `SimSatisfaction` 이 아니라 `좋음` 이다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Label;
};
