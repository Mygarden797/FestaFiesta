// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Systems/FF_CrowdDensityGrid.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "FF_LivePanelDensity.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

/**
 * 범례 한 줄. 등급 하나에 대응한다.
 *
 * 🔴 **색만으로 등급을 말하지 않는다** (AT-011). 색·문자·이름·수치 넷을 같이 준다.
 * 색을 못 보는 사람도 `D · 0.72~1.08명/㎡ · 혼잡` 을 읽을 수 있어야 한다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_DensityLegendRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	ECrowdLOS Grade = ECrowdLOS::A;

	/** `A`~`F`. 색과 무관하게 등급을 가르는 값이다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	FText GradeLetter;

	/** `여유`·`위험` 같은 말. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	FText GradeName;

	/** 이 등급이 시작되는 밀도(명/㎡). `A` 는 0 이다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	float MinDensity = 0.f;

	/**
	 * 이 등급이 끝나는 밀도(명/㎡). **가장 높은 등급(F)은 위가 없어 음수**다.
	 * 0 을 쓰면 `0 명/㎡ 까지` 로 읽혀서 정반대 뜻이 된다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	float MaxDensity = -1.f;

	bool HasUpperBound() const { return MaxDensity >= 0.f; }
};

/**
 * 밀집 화면에 쓰는 순수 함수.
 *
 * 위젯 없이 테스트할 수 있게 분리한다 — 경계값을 어떻게 얻는지가 이 파일의 핵심이라
 * 그 부분만 따로 못으로 박아야 한다.
 */
struct FESTAFIESTA_API FFF_DensityRules
{
	/**
	 * 범례 여섯 줄을 만든다.
	 *
	 * === 🔴 경계값을 **베끼지 않고 물어본다** ===
	 * Fruin 경계(`0.31 / 0.43 / 0.72 / 1.08 / 2.17`)는 `FF_CrowdDensityGrid.cpp` 의 익명
	 * 네임스페이스 안에 있어 UI 가 직접 못 읽는다. 그렇다고 UI 에 숫자를 베껴 두면
	 * **이정원 님이 값을 바꿨을 때 범례만 조용히 틀려진다** — 화면은 멀쩡해 보이는데
	 * 실제 색과 적힌 수치가 어긋나는, 제일 알아채기 어려운 종류의 거짓말이다.
	 *
	 * 그래서 공개돼 있는 `FCrowdDensityGrid::DensityToLOS()` 를 촘촘히 훑어 **등급이
	 * 바뀌는 지점을 찾아낸다.** 저쪽이 값을 바꾸면 범례도 저절로 따라간다.
	 * 패널을 열 때 한 번만 도는 계산이라 비용은 문제가 되지 않는다.
	 *
	 * @param MaxScanDensity  훑을 상한(명/㎡). 마지막 등급 시작점보다 충분히 커야 한다.
	 * @param StepDensity     훑는 간격. 작을수록 경계가 정확하다.
	 */
	static TArray<FFF_DensityLegendRow> BuildLegend(
		float MaxScanDensity = 4.f,
		float StepDensity = 0.001f);

	/** `A`~`F`. 색을 못 봐도 등급을 가를 수 있어야 한다. */
	static FText GetGradeLetter(ECrowdLOS Grade);

	/** `여유`·`위험` 같은 말. */
	static FText GetGradeName(ECrowdLOS Grade);
};

/**
 * 라이브 `밀집` 패널 — 히트맵 토글 · 범례 · 지금 상태.
 *
 * === 🔴 지금은 에디터에서만 켜진다 ===
 * 바닥에 깔리는 히트맵은 `FCrowdDensityGrid::DrawDebug()` 이고 그 안은 `DrawDebugSolidBox`
 * 다. **`DrawDebug*` 는 셰이핑 빌드에서 컴파일 아웃되므로 패키징에는 안 나온다.**
 * 제품용 렌더링과 토글 API 를 `UI_API.md` 11번으로 요청해 뒀다 (이정원 님).
 *
 * 그때까지 이 패널은 콘솔 변수(`FF.Crowd.Debug`)를 대신 켠다. API 가 오면 **토글 한 줄만**
 * 갈아끼우면 되고 범례·요약은 그대로 쓴다.
 *
 * === UI 가 밀도를 계산하지 않는다 ===
 * 최댓값·혼잡 셀 수는 Crowd 가 이미 세어 둔 값을 그대로 읽는다 (C-009 규칙).
 * 셀 면적이나 등급 경계를 UI 에서 다시 만들지 않는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - Density"))
class FESTAFIESTA_API UFF_LivePanelDensity : public UFF_LivePanelBase
{
	GENERATED_BODY()

protected:
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleToggleClicked();

protected:
	/** 히트맵 켜기/끄기. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UButton> Button_Toggle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_ToggleLabel;

	/** 범례 여섯 줄이 들어간다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UVerticalBox> Box_Legend;

	/** `가장 붐비는 곳 1.4명/㎡ · E (매우 혼잡)`. 못 읽으면 접힌다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_Peak;

	/** `위험 구간 3칸`. 0 이면 없다고 적는다 — 접지 않는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_Congested;

	/** 히트맵이 에디터 전용이라는 사실. **지운다고 문제가 사라지지 않는다.** */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_MissingNote;

	/** 요약을 몇 초마다 다시 읽을지. 밀도는 매 프레임 바뀌지만 눈으로 좇을 값이 아니다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Live|Density", meta = (ClampMin = "0.1"))
	float RefreshIntervalSeconds = 0.5f;

private:
	void RebuildLegend();
	void RefreshSummary();
	void RefreshToggleLabel();

	/** 마지막 갱신 이후 흐른 시간. */
	float TimeSinceRefresh = 0.f;
};
