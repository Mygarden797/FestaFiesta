// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "FF_PrepPanelCheck.generated.h"

class UButton;
class UTextBlock;
class UWidget;
class UFF_FestivalViewModelSubsystem;
class UFF_PrepChoiceButton;

/**
 * 예상 방문객이 수용량에 비해 어느 정도인지.
 *
 * !! 색만으로 구분하지 않는다 !! (AT-011)
 * 각 단계마다 아이콘·문구가 함께 붙는다. 그래서 `bool bIsDangerous` 가 아니라 단계 enum 이다.
 */
UENUM(BlueprintType)
enum class EFF_PrepCapacityRisk : uint8
{
	/** 예상 방문객이나 수용량을 아직 읽지 못했다. **위험하지 않다는 뜻이 아니다.** */
	Unknown		UMETA(DisplayName = "판정 불가"),

	Comfortable	UMETA(DisplayName = "여유"),
	Caution		UMETA(DisplayName = "주의"),
	Danger		UMETA(DisplayName = "위험")
};

/**
 * 오늘 하루 견적. 화면은 이 값을 그대로 그리고 계산을 하지 않는다.
 *
 * === 모르는 값은 `INDEX_NONE` 이다 ===
 * `0` 을 넣으면 "비용이 없다"·"수입이 없다"는 **틀린 사실**이 된다. 개장 직전 확인 화면에서
 * 그건 특히 나쁘다 — 플레이어가 그 숫자를 보고 개장을 결정한다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PrepDayEstimate
{
	GENERATED_BODY()

	/** 오늘 이미 지불한 고용비 총액.*/
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 StaffHireCost = INDEX_NONE;

	/** 예상 방문객 × 입장료. 예상 방문객을 모르면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 ExpectedAdmissionRevenue = INDEX_NONE;

	/**
	 * 일일 목표 달성 보너스. **예상 방문객이 목표에 못 미치면 0 이다** —
	 * 이 0 은 "모른다"가 아니라 "못 받는다"라는 확정 정보다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 ExpectedGoalBonus = INDEX_NONE;
};

/**
 * 점검 패널의 판단 규칙. **순수 함수만 둔다.**
 *
 * === 왜 위젯 밖으로 뺐나 ===
 * 수용 위험 구간은 **그 방문객 수가 실제로 오는 날까지 기다려야** 눈으로 확인된다.
 * 경계값(80% · 100%)이 `assumption` 이라 앞으로 바뀔 값이고, 바뀔 때 화면 코드가 아니라
 * 여기 하나만 고치면 되게 한다. U4·U5 의 다른 규칙과 같은 이유다.
 */
struct FESTAFIESTA_API FFF_PrepCheckRules
{
	/**
	 * 예상 방문객을 수용량과 비교해 구간을 고른다.
	 *
	 * @param ExpectedVisitors 모르면 `INDEX_NONE`
	 * @param SoftCapacity     Zone 정원 합. 0 이하거나 모르면 판정하지 않는다
	 * @param CautionRatio     이 비율 이상이면 `주의`. `assumption` 0.8
	 * @param DangerRatio      이 비율 이상이면 `위험`. `assumption` 1.0
	 *
	 * ★ 둘 중 하나라도 모르면 `Unknown` 이다. **`여유` 로 떨어지지 않는다** —
	 *   모르는 것을 안전하다고 알리면 플레이어가 그걸 믿고 개장한다.
	 */
	static EFF_PrepCapacityRisk ClassifyCapacityRisk(
		int32 ExpectedVisitors,
		int32 SoftCapacity,
		float CautionRatio,
		float DangerRatio);

	/**
	 * 하루 견적을 만든다. 모르는 값은 전파되고 0 으로 바뀌지 않는다.
	 *
	 * @param ExpectedVisitors 모르면 `INDEX_NONE` — 수입과 보너스가 함께 비워진다
	 * @param DailyVisitorGoal 목표. 모르면 보너스를 비운다
	 */
	static FFF_PrepDayEstimate BuildDayEstimate(
		int32 HiredStaffCount,
		int32 HireCostPerStaff,
		int32 ExpectedVisitors,
		int32 AdmissionFee,
		int32 DailyVisitorGoal,
		int32 DailyGoalBonus);
};

/**
 * 준비 단계 `점검` 패널 — 개장 직전 견적서.
 *
 * === 이 화면의 목적 ===
 * `개장하기` 를 누르기 전에 **오늘 무엇이 준비됐고 무엇이 부족한지**를 한 화면에서 보여준다.
 * 그래서 값이 없을 때 빈칸으로 두지 않고 `아직 알 수 없습니다` 를 적는다 — 개장 판단의
 * 근거가 되는 화면에서 빈칸은 "문제 없음"으로 읽힌다.
 *
 * 구성 —
 *   카드 4장   날씨(선택 가능) · 일일 목표 · 예상 방문객 · 보유 금화
 *   수용 위험  예상 방문객 vs Zone 정원 합. 색 + 아이콘 + 문구
 *   요약       인력 · 프로그램 · (마지막 날) 피날레
 *   견적표     고용비 · 예상 입장료 수입 · 목표 보너스
 *   버튼       `배치 계속하기` · `개장하기`
 *
 * === ★ 날씨 선택이 여기 있는 이유 ===
 * `SetSelectedWeather()` 는 셋업·홍보·개장 준비에서만 허용된다 — 즉 **준비 단계의 플레이어
 * 결정**이다. 이전에는 테스트 액터(`BP_StageTest`)의 버튼이 유일한 창구였고, 그 액터를
 * 지우면서 정식 자리를 여기로 옮겼다 (2026-08-26).
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Panel - Check"))
class FESTAFIESTA_API UFF_PrepPanelCheck : public UFF_PrepPanelBase
{
	GENERATED_BODY()

public:
	UFF_PrepPanelCheck(const FObjectInitializer& ObjectInitializer);

	/**
	 * 예상 방문객을 외부에서 넣는다. **소유자 API 가 없다** (2026-08-26, `UI_API.md` 요청 중).
	 * `INDEX_NONE` 이면 관련 칸이 모두 `아직 알 수 없습니다` 가 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Debug")
	void Debug_SetExpectedVisitors(int32 InMin, int32 InMax);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;

	// --- 버튼 ---

	UFUNCTION()
	void HandleContinueBuildClicked();

	UFUNCTION()
	void HandleOpenGateClicked();

	/** 날씨 선택지 클릭. 베이스가 `int32` 로 주므로 여기서 되돌려 해석한다. */
	void HandleWeatherChoiceClicked(int32 ChoiceId);

	// --- 결과 이벤트 ---

	UFUNCTION()
	void HandleWeatherChanged(EFF_WeatherType NewWeather);

	UFUNCTION()
	void HandleGoldChanged(int32 NewValue);

	UFUNCTION()
	void HandleStaffCountChanged(int32 NewValue);

	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

	/** 피날레를 사면 요약의 마지막 날 줄이 `미구매` 에서 등급·보너스로 바뀐다. */
	UFUNCTION()
	void HandlePurchasedFinaleGradeChanged(EFF_FinaleGrade NewGrade);

	/** 시설을 놓거나 철거하면 개장 판정이 바뀐다. 바가 알려 준다. */
	UFUNCTION()
	void HandleCtaStateChanged();

protected:
	// --- 카드 4장 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_Weather;

	/** 날씨 선택지. 이름이 `EFF_WeatherType` 항목 이름과 짝이다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UFF_PrepChoiceButton> Choice_Clear;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UFF_PrepChoiceButton> Choice_Rain;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UFF_PrepChoiceButton> Choice_Windy;

	/** 날씨를 못 바꾸는 이유. 흐려짐만으로는 알 수 없다 (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_WeatherLockReason;

	/** `500명 · 달성 시 +300`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_DailyGoal;

	/** `320~480명` 또는 `아직 알 수 없습니다`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_ExpectedVisitors;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_Gold;

	// --- 수용 위험 ---

	/** `여유` / `주의` / `위험` / `판정 불가`. 아이콘 문자를 앞에 붙인다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_CapacityRisk;

	/** `정원 900명 대비 예상 480명`. 판정의 근거를 함께 보여준다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_CapacityDetail;

	// --- 요약 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_StaffSummary;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_ProgramSummary;

	/** 마지막 날이 아니면 줄째 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_FinaleSummary;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UWidget> Box_FinaleSummaryRow;

	// --- 견적표 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_StaffWageCost;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_ExpectedAdmission;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_ExpectedGoalBonus;

	/** 부스 수수료처럼 운영 결과에 달린 값은 예측하지 않는다는 안내. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_EstimateNotice;

	// --- 버튼 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UButton> Button_ContinueBuild;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UButton> Button_OpenGate;

	/**
	 * 개장 버튼 문구. **단계마다 다르다** — 셋업은 `확정하고 Day 1 시작`, 개장 준비는 `개장하기`.
	 * 바가 단계별 문구를 소유하므로 여기서 박지 않고 받아 온다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Check")
	TObjectPtr<UTextBlock> Text_OpenGateLabel;

	// --- 설정 ---

	/**
	 * 수용 위험 경계 비율. **둘 다 `assumption` 이다** — 산정 근거가 아직 없다.
	 * 값이 정해지면 여기만 고친다. 화면 문구와 코드가 갈라지지 않게 하려고 값으로 뺀다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Check", meta = (ClampMin = "0.0"))
	float CapacityCautionRatio = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Check", meta = (ClampMin = "0.0"))
	float CapacityDangerRatio = 1.0f;

	/** 견적에 넣지 않는 값이 있다는 안내. 비우지 않는다 — 표가 전부인 것처럼 읽힌다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Check")
	FText EstimateNoticeText;

	/**
	 * 🔴 날씨 표시 문구. **`EFF_WeatherType` 의 `DisplayName` 을 읽지 않는다.**
	 *
	 * 그 enum 에는 `UMETA(DisplayName)` 이 아예 없다 — 한글이 **주석에만** 있어서
	 * `GetDisplayNameTextByIndex()` 가 `Clear`·`Rain`·`Windy` 를 돌려준다. 2026-08-26 에
	 * 화면에 영문이 그대로 떴다. 그 enum 은 강민서님 파일이라 메타데이터를 넣지 않고
	 * (AGENTS.md 규칙 3) **표시 문구를 UI 가 소유한다.**
	 *
	 * 기본값은 생성자에서 넣는다. 라이브 HUD 도 날씨를 쓰게 되면 이 표를 공용 라이브러리로 뺀다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Check")
	TMap<EFF_WeatherType, FText> WeatherLabels;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();

	void CollectWeatherChoices();

	/** 전체를 다시 그린다. 부분 갱신하지 않아 이벤트 순서에 의존하지 않는다. */
	void RefreshAll();

	void RefreshWeather();
	void RefreshCapacity();
	void RefreshSummary();
	void RefreshEstimate();

	/**
	 * 개장 버튼의 문구와 활성 상태를 바의 판정으로 맞춘다.
	 *
	 * !! 여기서 다시 판정하지 않는다 !! 시설 수와 단계 표를 아는 것은 바뿐이라,
	 * 패널이 따로 세면 두 곳의 규칙이 갈라진다 (UI 규칙 2).
	 */
	void RefreshOpenGate();

	/** 설치된 Zone 의 정원 합. 못 읽으면 `INDEX_NONE`. */
	int32 GetTotalSoftCapacity() const;

	/** 예약된 유료 프로그램 수. 못 읽으면 `INDEX_NONE`. */
	int32 GetProgramCount() const;

	/** 표에서 찾은 문구. 없으면 enum 이름을 그대로 돌려주고 경고를 남긴다. */
	FText GetWeatherLabel(EFF_WeatherType Weather) const;

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);
	static void SetGroupTextOrHide(UWidget* Group, UTextBlock* TextBlock, const FText& Value);

	/** 금액 칸 하나. `INDEX_NONE` 이면 `아직 알 수 없습니다`. */
	static FText FormatGoldOrUnknown(int32 Amount);

	UPROPERTY()
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	UPROPERTY()
	TArray<TObjectPtr<UFF_PrepChoiceButton>> WeatherChoices;

	// 소유자 API 가 없는 값. `INDEX_NONE` 이면 관련 칸이 전부 `모름` 이 된다.
	/**
	 * 🔴 경제는 **하루 단일 예상치**를 준다 (`GetExpectedVisitors()`). 범위 API 는 아직 없다.
	 * 그래서 지금은 Min·Max 에 같은 값이 들어가고 화면도 `500명` 하나로 뜬다.
	 * 범위가 오면 여기만 갈라 넣으면 되고 표시 코드는 그대로다 (2026-09-02).
	 */
	int32 ExpectedVisitorsMin = INDEX_NONE;
	int32 ExpectedVisitorsMax = INDEX_NONE;
};
