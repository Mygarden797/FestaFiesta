// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_UIMetricBandSettings.generated.h"

class UTexture2D;

/**
 * 0~100 지표 하나의 구간.
 *
 * AT-011: **색만으로 상태를 구분하지 않는다.** 색 + 라벨 + 아이콘을 함께 쓴다.
 * Label을 비워 두면 색맹 사용자에게 구간이 전달되지 않으므로 비우지 않는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_UIMetricBand
{
	GENERATED_BODY()

	/** 이 구간에 들어가는 최솟값(이상). 구간 배열은 이 값의 내림차순으로 둔다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Band", meta = (ClampMin = 0, ClampMax = 100))
	int32 MinValue = 0;

	/** 화면에 그대로 나가는 문구. 내부 이름을 쓰지 않는다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Band")
	FText Label;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Band")
	FLinearColor Color = FLinearColor::White;

	/**
	 * 흰색 단색 텍스처를 Color로 틴트해 쓴다.
	 * 구간마다 **실루엣이 달라야 한다.** 흑백으로 뽑았을 때 구분되지 않으면 AT-011을 못 지킨다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Band")
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * 0~100 지표의 좋고 나쁨 구간.
 *
 * === 왜 설정으로 빼는가 ===
 * 기획 수치는 대부분 `assumption`이라 확정 사실처럼 코드에 박으면 안 된다 (AGENTS.md 작업 관례).
 * Project Settings > Game > FestaFiesta UI Metric Bands 에서 조정한다.
 *
 * === 구간을 공유하는 지표 ===
 * `SimSatisfaction` · `ScoreSatisfaction` · `Safety` 셋은 전부 0~100이고 **높을수록 좋다.**
 * 방향이 같아서 구간 하나를 공유한다. 방향이 다른 지표가 생기면 그때 세트를 나눈다.
 *
 * **`Gold` · `TotalRevenue` · `TodayRevenue`는 여기 해당하지 않는다.** 상한이 없어 절대 구간이
 * 무의미하다. Gold는 "이걸 살 수 있나"(가격 대비), 매출은 전날 대비 증감으로 표현한다.
 *
 * === 기본값 70 / 40 (2026-08-06 결정) ===
 * - 화면 전체가 3단계 어휘(좋음/주의/나쁨)를 공유하게 하려는 것이 원래 근거였다.
 *   단 **Heat Map 쪽 3구간 전제는 2026-08-14 기준 확정이 아니다** — Crowd 구현은 Fruin 보행로 LOS
 *   6등급이다. 지표 구간(이 파일)과 Heat Map 구간은 별개로 결정될 수 있다. C-029 참조.
 * - 초기 더미값 72(만족도) · 88(안전)이 "좋음"에 들어간다. 시작 상태가 정상으로 보여야 한다.
 * - 구간 폭 30 = Finale S등급 FinaleBonus +30. S를 사면 구간을 정확히 하나 건넌다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta UI Metric Bands"))
class FESTAFIESTA_API UFF_UIMetricBandSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UFF_UIMetricBandSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static const UFF_UIMetricBandSettings* Get();

	/**
	 * 값이 들어가는 구간을 돌려준다. 어디에도 안 걸리면 nullptr.
	 * 호출부는 nullptr을 색 없음·라벨 없음으로 처리한다. 임의의 색으로 대체하지 않는다.
	 */
	const FFF_UIMetricBand* FindBand(int32 Value) const;

	/** MinValue 내림차순으로 정렬된 구간 목록. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Metric Bands")
	TArray<FFF_UIMetricBand> Bands;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** 내림차순 정렬을 보장한다. 순서가 틀리면 FindBand가 엉뚱한 구간을 돌려준다. */
	void SortBandsDescending();
};
