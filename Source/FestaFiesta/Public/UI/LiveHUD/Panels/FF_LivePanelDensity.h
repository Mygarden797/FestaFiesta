// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Systems/FF_CrowdDensityGrid.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "FF_LivePanelDensity.generated.h"

class UButton;
class UBorder;
class UTextBlock;
class UVerticalBox;
class UFF_LiveDensityLegendRowWidget;

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
 * === 현재 히트맵은 디버그 표시다 ===
 * 바닥에 깔리는 히트맵은 `FCrowdDensityGrid::DrawDebug()` 이고 그 안은 `DrawDebugSolidBox`
 * 다. `ENABLE_DRAW_DEBUG`가 켜진 에디터·개발 빌드에서만 지원하며 출시 빌드는 미지원이다.
 * 제품용 렌더링과 토글 API 를 `UI_API.md` 11번으로 요청해 뒀다 (이정원 님).
 *
 * 그때까지 `bDrawDebugDensity` 표시 플래그만 켠다. API가 오면 토글 경로를 교체한다.
 *
 * === UI 가 밀도를 계산하지 않는다 ===
 * 최댓값·혼잡 셀 수는 Crowd 가 이미 세어 둔 값을 그대로 읽는다 (C-009 규칙).
 * 셀 크기·면적과 등급 색도 같은 Grid에서 읽는다. 격자가 준비되지 않으면 0으로 표시하지 않는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - Density"))
class FESTAFIESTA_API UFF_LivePanelDensity : public UFF_LivePanelBase
{
	GENERATED_BODY()

public:
	/** 범례 행의 크기·열 폭·글꼴을 Designer에서 편집할 수 있는 클래스. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Rows")
	TSubclassOf<UFF_LiveDensityLegendRowWidget> LegendRowWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Rows")
	FMargin LegendRowPadding = FMargin(0.f, 0.f, 0.f, 6.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText MissingValueText = NSLOCTEXT("FestaFiestaLiveDensity", "MissingValue", "—");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText PendingGradeText = NSLOCTEXT("FestaFiestaLiveDensity", "PendingGrade", "집계 준비 중");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText InvalidGradeText = NSLOCTEXT("FestaFiestaLiveDensity", "InvalidGrade", "밀도 값 확인 필요");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text", meta = (ToolTip = "{Density}: 원본 최대 밀도"))
	FText PeakValueTextFormat = NSLOCTEXT("FestaFiestaLiveDensity", "PeakValueFormat", "{Density}명/㎡");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text", meta = (ToolTip = "{Grade}: A~F, {Name}: 등급 이름"))
	FText PeakGradeTextFormat = NSLOCTEXT("FestaFiestaLiveDensity", "PeakGradeFormat", "{Grade} · {Name}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text", meta = (ToolTip = "{Count}: Crowd 원본 혼잡 칸 수. D등급 칸 수가 아닙니다."))
	FText CongestedTextFormat = NSLOCTEXT("FestaFiestaLiveDensity", "CongestedFormat", "{Count}칸");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text", meta = (ToolTip = "{Size}: 격자 한 변의 실제 길이(m)"))
	FText CellSizeTextFormat = NSLOCTEXT("FestaFiestaLiveDensity", "CellSizeFormat", "{Size} × {Size}m");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text", meta = (ToolTip = "{Area}: 실제 격자 면적(㎡)"))
	FText CellAreaTextFormat = NSLOCTEXT("FestaFiestaLiveDensity", "CellAreaFormat", "한 칸 면적 {Area}㎡");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText ToggleOnText = NSLOCTEXT("FestaFiestaLiveDensity", "LiveDensity_ToggleOn", "히트맵 끄기");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText ToggleOffText = NSLOCTEXT("FestaFiestaLiveDensity", "LiveDensity_ToggleOff", "히트맵 켜기");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText ToggleUnavailableText = NSLOCTEXT("FestaFiestaLiveDensity", "LiveDensity_HeatmapUnavailable", "히트맵 미지원");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText HeatmapOnText = NSLOCTEXT("FestaFiestaLiveDensity", "HeatmapOn", "바닥 히트맵 · 켜짐");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText HeatmapOffText = NSLOCTEXT("FestaFiestaLiveDensity", "HeatmapOff", "바닥 히트맵 · 꺼짐");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText HeatmapPendingText = NSLOCTEXT("FestaFiestaLiveDensity", "HeatmapPending", "바닥 히트맵 · 준비 중");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText HeatmapUnavailableText = NSLOCTEXT("FestaFiestaLiveDensity", "HeatmapUnavailable", "바닥 히트맵 · 미지원");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText DebugBuildNoteText = NSLOCTEXT("FestaFiestaLiveDensity", "LiveDensity_DebugBuildNote", "바닥 히트맵은 개발용 표시입니다. 출시 빌드에서는 지원하지 않습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText GridPendingNoteText = NSLOCTEXT("FestaFiestaLiveDensity", "LiveDensity_GridPendingNote", "밀도 격자가 준비되면 히트맵을 켤 수 있습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Density|Text")
	FText UnsupportedBuildNoteText = NSLOCTEXT("FestaFiestaLiveDensity", "LiveDensity_NoDebugDrawNote", "이 빌드는 바닥 히트맵을 지원하지 않습니다. 밀도 수치와 등급은 계속 확인할 수 있습니다.");

protected:
	virtual void NativeDestruct() override;
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

	/** 최대 밀도. Text_PeakGrade가 없는 구 WBP에는 등급까지 한 줄로 표시한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_Peak;

	/** `E · 매우 혼잡`. 새 요약 카드에서는 밀도 숫자와 등급을 구분한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_PeakGrade;

	/** 혼잡 칸 수는 Crowd 집계값 그대로 쓴다. 0이어도 숨기지 않는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_Congested;

	/** 실제 격자의 한 변(m)과 면적(㎡). 2m 같은 가정값을 복사하지 않는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_CellSize;

	/** 별도 면적 줄. 없는 구 WBP에는 Text_CellSize에 길이와 면적을 함께 표시한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_CellArea;

	/** 토글 버튼의 동작명과 구분한 현재 켜짐/꺼짐/준비 중/미지원 상태. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_HeatmapStatus;

	/** 현재 빌드의 히트맵 지원 여부. 숫자 조회는 바닥 표시와 독립적이다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Density")
	TObjectPtr<UTextBlock> Text_MissingNote;

	/** 요약을 몇 초마다 다시 읽을지. 밀도는 매 프레임 바뀌지만 눈으로 좇을 값이 아니다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Live|Density", meta = (ClampMin = "0.1"))
	float RefreshIntervalSeconds = 0.5f;

private:
	void RebuildLegend();
	void RefreshSummary();
	void RefreshToggleLabel();
	void RefreshLegendHighlight(TOptional<ECrowdLOS> PeakGrade);

	/** 패널 재오픈과 수치 갱신에서 같은 행 인스턴스를 재사용한다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_LiveDensityLegendRowWidget>> LegendRowWidgets;

	UPROPERTY(Transient)
	TSubclassOf<UFF_LiveDensityLegendRowWidget> CachedLegendRowWidgetClass;

	/** 현재 최대 등급이 바뀔 때 위젯을 재생성하지 않고 행 강조만 갱신한다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> LegendRowBorders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> LegendPeakLabels;

	TArray<ECrowdLOS> LegendGrades;

	/** 마지막 갱신 이후 흐른 시간. */
	float TimeSinceRefresh = 0.f;
};
