// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Time/FF_FestivalStage.h"
#include "FF_FestivalViewModelSubsystem.generated.h"

class UFF_FestivalStageSubsystem;
class UFF_FestivalProgressionSubsystem;
// !! class가 아니라 struct다 !! (Core/Public/HAL/IConsoleManager.h)
// class로 쓰면 C4099가 나고, 이 프로젝트는 경고를 에러로 취급한다.
struct IConsoleCommand;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnIntMetricChanged, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFF_OnStageViewChanged, EFF_FestivalStage, NewStage, int32, NewDay);

/**
 * UI용 Read Model.
 *
 * === 이 클래스의 존재 이유 ===
 * UI 위젯은 게임 시스템을 직접 참조하지 않고 **이것만** 본다.
 * 각 시스템이 바뀌어도 위젯을 고치지 않기 위해서다.
 *
 * === 현재 상태: 일부만 진짜 값 (2026-08-14 기준) ===
 * 아직 더미인 지표가 남아 있다. 실제 소유자의 구현이 생기면 이 클래스 안쪽만 교체하고
 * 위젯은 그대로 둔다. 그게 이 계층을 두는 이유다.
 *
 * | 값                     | 실제 소유자      | 현재        |
 * |------------------------|------------------|-------------|
 * | Stage / Day            | 강민서           | **진짜 값** |
 * | 남은 시간              | 강민서           | **진짜 값** |
 * | Gold                   | 김시리           | **진짜 값** (`OnGoldChanged` 구독) |
 * | SimSatisfaction        | 김시리 · 이정원  | 더미        |
 * | ScoreSatisfaction      | 김시리           | 더미        |
 * | Safety                 | 이정원           | 더미 (소유자 API 없음) |
 * | TotalRevenue           | 김시리           | 더미 — **소유자 API는 완료, 구독 연결만 남음** |
 * | TodayRevenue           | 김시리           | 더미 — **소유자 API는 완료, 구독 연결만 남음** |
 *
 * `GetTotalRevenue()`/`GetTodayRevenue()`와 `OnTotalRevenueChanged`/`OnTodayRevenueChanged`가
 * `UFF_FestivalProgressionSubsystem`에 이미 있다. Gold와 같은 모양이므로 같은 Bind + Sync로 붙인다.
 * 원인별·부스별 분해는 `GetDayLedger()`로 읽을 수 있다 (2026-08-18 확인). `FFF_DayLedger`는
 * 원인별 지출·원인별 매출·부스별 매출(`GetRevenueByFacility()`)·철거 환급을 따로 쌓고 하루 경계에서
 * `Reset()`된다. Blueprint 노출은 없고 C++ const 참조라 Report 화면이 직접 읽는다.
 *
 * Staff도 소유자 API가 생겼다 — `GetBaseStaffCount()`/`GetTemporaryStaffCount()`/`GetTotalStaffCount()`.
 * 무페이 기본 팀 + 당일 알바 구성이고 임금은 알바에만 붙는다. 이 ViewModel에는 아직 Staff 필드가 없다.
 * 임대료 미납은 `FFF_FixedCostArrears`에 누적된다. 상세는 리포 루트 `UI_API.md`.
 *
 * === 값 규격 (2026-08-06 결정) ===
 * - Satisfaction / Safety : int32 0~100, 높을수록 좋음.
 * - Gold / Revenue : int32 금액. 0~100 상한을 적용하지 않는다.
 *
 * **Gold와 Revenue는 다른 값이다.** 헷갈리면 화면에 틀린 숫자가 뜬다.
 * - `Gold`         : 지금 쓸 수 있는 **잔액**. 지출로 줄어든다. 초기 10,000, 축제 종료 시 초기화.
 * - `TotalRevenue` : 축제 전체 **누적 매출**. 줄어들지 않는다. 상단 바 표시용.
 * - `TodayRevenue` : 오늘 하루 매출. Report에서 "오늘 얼마 벌었나"에 쓴다.
 *
 * 대략 `Gold ≒ 초기자금 + TotalRevenue - 누적지출`이지만, **UI가 이 식을 계산하지 않는다.**
 * 셋 다 소유자에게서 그대로 받는다. 근거: docs/design/game-overview.md 5절
 *
 * Buzz는 여기 없다. 기획서에서 Utility AI의 점수 항이며 dev-schedule 기준 9월 투입 대상이다.
 * 좋고 나쁨 구간은 아직 미정이라 UI에서 색을 칠하지 않는다. 상세는 리포 루트 UI_API.md 참고.
 *
 * === 갱신 주기 (2026-08-06 결정) ===
 * 지표는 **일정 틱마다** 바뀐다. 소스가 매 틱 흔들려도 위젯을 매 틱 갱신하면 안 되므로
 * 여기서 `PollInterval`마다 샘플링하고 **정수값이 실제로 바뀔 때만** 델리게이트를 쏜다.
 * 지표를 int32로 둔 것이 여기서 이득이 된다. 위젯은 이 처리를 몰라도 된다.
 *
 * === 시간을 자체 계산하지 않는다 ===
 * 시계 배율이 출처마다 달라 아직 미확정이다(30초 / 60초 / 코드 180초×4).
 * UI는 남은 시간을 스스로 계산하지 않고 Stage 시스템이 주는 값을 그대로 표시만 한다.
 * 근거: docs/memory/time-contract-draft-2026-08-06.md
 */
UCLASS()
class FESTAFIESTA_API UFF_FestivalViewModelSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- 지표 ---
	// 규칙: 값마다 "현재값 Getter + 변경 델리게이트"를 쌍으로 제공한다.
	// 델리게이트만 있으면 위젯이 늦게 생성될 때 초기값을 놓친다.

	/** 0~100. 대기·밀도·취향 매칭에서 나오는 순수 시뮬레이션 값. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetSimSatisfaction() const { return SimSatisfaction; }

	/** 0~100. SimSatisfaction + FinaleBonus + 연출 보정. 등급 산출용 게임 점수. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetScoreSatisfaction() const { return ScoreSatisfaction; }

	/** 0~100. 높을수록 안전. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetSafety() const { return Safety; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnSimSatisfactionChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnScoreSatisfactionChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnSafetyChanged;

	// --- 경제 ---
	// Gold(잔액)와 Revenue(매출)는 다른 값이다. 클래스 주석의 구분표를 먼저 읽는다.

	/** 지금 쓸 수 있는 잔액. 지출로 줄어든다. 상단 바·Finale 구매 가능 판정에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetGold() const { return Gold; }

	/** 축제 전체 누적 매출. 줄어들지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetTotalRevenue() const { return TotalRevenue; }

	/** 오늘 하루 매출. Report의 "오늘 얼마 벌었나"에 쓴다. UI가 누적값에서 빼서 만들지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetTodayRevenue() const { return TodayRevenue; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Economy")
	FFF_OnIntMetricChanged OnGoldChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Economy")
	FFF_OnIntMetricChanged OnTotalRevenueChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Economy")
	FFF_OnIntMetricChanged OnTodayRevenueChanged;

	// --- 고정비 미납 (2026-08-26 추가 — U4 준비 단계 HUD) ---
	//
	// !! 잔액과 함께 보여야 하는 값이다 !!
	// 미납이 남아 있으면 매출이 들어와도 잔액이 안 늘어난다(`RepayArrearsFromGold`).
	// 이 칸이 없으면 "돈을 벌었는데 잔액이 그대로"인 화면을 설명할 방법이 없다.
	// 0 이면 화면에서 칸을 숨긴다 — 0 을 띄우면 없는 문제를 있는 것처럼 만든다.

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetArrears() const { return Arrears; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Economy")
	FFF_OnIntMetricChanged OnArrearsChanged;

	// --- 스태프 (2026-08-26 추가 — U4 준비 단계 HUD) ---
	//
	// 무페이 기본 팀 + 당일 알바다. 임금은 알바에만 붙고 매일 재계약한다.
	// **축제 시작 전에는 `INDEX_NONE`(모름)이다.** 0 을 띄우면 "스태프가 없다"는 틀린 사실이 된다.

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetBaseStaffCount() const { return BaseStaffCount; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetTemporaryStaffCount() const { return TemporaryStaffCount; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetTotalStaffCount() const { return TotalStaffCount; }

	/** 전체 인원이 바뀔 때. 기본·알바 내역도 이때 함께 갱신된다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Staff")
	FFF_OnIntMetricChanged OnTotalStaffCountChanged;

	// --- 알바 고용 (2026-08-26 추가 — U5 인력 패널) ---
	//
	// 판정의 소유자는 경제 시스템이다. **UI 가 `Gold / 임금` 을 직접 계산하지 않는다** —
	// 상한 규칙이 바뀌면 UI 의 계산식이 조용히 틀리게 된다.
	// 값이 없으면 전부 `INDEX_NONE`(모름)이고 화면은 그 칸을 숨긴다.

	/**
	 * 지금 추가로 고용할 수 있는 최대 인원. **잔액과 하루 상한을 둘 다 반영한 값**이다.
	 * 스텝퍼 상한과 버튼 비활성 판정에 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetAffordableTemporaryStaffCount() const;

	/**
	 * 오늘 상한까지 남은 자리. 잔액은 보지 않는다.
	 *
	 * `GetAffordableTemporaryStaffCount()` 와 **둘 다 필요한 이유**는 막힌 이유를 구분하기
	 * 위해서다 — `상한을 다 썼다` 와 `돈이 없다` 는 플레이어가 할 행동이 다르다 (AT-011).
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetRemainingTemporaryStaffCap() const;

	/** 알바 1명·1일 임금. 고정비 표와 견적에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetDailyStaffWageGold() const;

	/** 하루 알바 상한. 화면에 `오늘 최대 N명` 을 쓰기 위한 값이다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetMaxTemporaryStaffPerDay() const;

	/** 장소 임대료(Gold/일). 고정비 표에 임금과 함께 보여준다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetDailyVenueRentGold() const;

	// --- Command ---

	/**
	 * 알바를 `Count` 명 고용해 달라고 **요청한다.**
	 *
	 * !! 부분 고용이 없다 !! 실패하면 아무것도 바뀌지 않는다. 그래서 낙관적 갱신을 하지 않고
	 * 화면은 `OnGoldChanged`·`OnTotalStaffCountChanged` 결과 이벤트로만 갱신된다.
	 *
	 * 돌려주는 값은 "요청이 받아들여졌는지"일 뿐이다. **실패 이유는 이 값에 없다** —
	 * 이유는 `GetRemainingTemporaryStaffCap()` 과 `GetAffordableTemporaryStaffCount()` 로
	 * 화면이 구분한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Command")
	bool RequestHireTemporaryStaff(int32 Count);

	/**
	 * 이 금액을 지금 지출할 수 있는지. 구매 버튼 비활성 판정에 쓴다.
	 *
	 * UI가 `GetGold() >= Amount`로 직접 비교하지 않는다 — 잔액 규칙의 소유자는 경제 시스템이고
	 * 나중에 예약금·할인 같은 조건이 붙으면 UI의 비교식이 조용히 틀리게 된다.
	 * 소유자가 없으면 false를 돌려준다. "살 수 있다"고 잘못 알리는 쪽이 더 나쁘다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	bool CanAfford(int32 Amount) const;

	// --- Stage (강민서 시스템 중계) ---

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	EFF_FestivalStage GetCurrentStage() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	int32 GetCurrentDay() const;

	// !! Slot은 의도적으로 노출하지 않는다 !!
	//
	// 2026-08-06 12시 스크럼에서 하루 4 Slot 구조를 없애고 날짜마다 Live를 한 번만 돌리기로 했다.
	// 시간 담당의 제거 작업이 아직 원격에 안 올라와서 UFF_FestivalStageSubsystem::CurrentSlot은
	// 여전히 존재하지만, UI가 그걸 화면에 띄우면 곧 지워질 개념을 플레이어에게 노출하게 된다.
	// 제거가 실제로 들어와도 이 클래스는 안 깨진다.
	// 근거: docs/design/stage-and-time.md 5절

	/** 남은 시간(초). Stage 시스템이 주는 값을 그대로 돌려준다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	float GetRemainingSeconds() const;

	/**
	 * Stage 표시 문구.
	 * 모르는 Stage가 와도 빈 화면 대신 원본 이름을 돌려준다.
	 * enum 순서나 개수에 의존하지 않는다 — 기획 7단계와 코드 8단계가 아직 다르다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	FText GetCurrentStageDisplayText() const;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Stage")
	FFF_OnStageViewChanged OnStageViewChanged;

	/**
	 * 남은 시간이 1초 단위로 바뀔 때만 쏜다. 매 틱 쏘면 위젯이 초당 수십 번 갱신된다.
	 *
	 * 2026-08-11부터 Stage 시스템의 `OnRemainingSecondsChanged`를 **구독해서 중계**한다.
	 * 폴링하지 않는다. 위젯 입장에서는 바뀐 게 없다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Stage")
	FFF_OnIntMetricChanged OnRemainingSecondsChanged;

	// --- Command (Stage 진행 요청) ---
	//
	// !! 위젯은 Stage 서브시스템을 직접 부르지 않는다 !!
	// UI 규칙 2 — "ViewModel로 읽고 Command로 요청한다". 여기가 그 Command 계층이다.
	// 소유자 시스템이 바뀌어도 위젯을 고치지 않기 위해 한 겹 둔다.
	//
	// 낙관적 갱신을 하지 않는다. 요청만 보내고 화면은 결과 이벤트(OnStageViewChanged)로 갱신된다.

	/** Report에서 `다음 날`을 눌렀을 때. Report가 아니거나 마지막 날이면 아무 일도 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Command")
	void RequestAdvanceToNextDay();

	/** 스킵 가능한 단계를 건너뛴다. 불가능한 단계면 Stage 시스템이 무시한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Command")
	void RequestSkipCurrentStage();

	/** `다음 날` 버튼을 켤지. Report 단계이고 마지막 날이 아닐 때만 true. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Command")
	bool CanAdvanceToNextDay() const;

	/**
	 * 마지막 날인지. **UI가 총 일수를 알지 못하게 하려고** Stage 시스템의 판단을 그대로 중계한다.
	 * `CanAdvanceToNextDay()`가 false인 이유를 화면이 구분해서 설명하려면 이게 필요하다
	 * (마지막 날이라서 vs 아직 Report가 아니라서).
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	bool IsFinalDay() const;

	/** 지금 Report 단계인지. Stage enum을 위젯이 직접 비교하지 않게 한다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	bool IsReportStage() const;

	/**
	 * `건너뛰기` 버튼을 켤지.
	 *
	 * DaySequence 표의 `bSkippable`을 읽는 `UFF_FestivalStageSubsystem::IsCurrentStageSkippable()`을
	 * 그대로 중계한다. 2026-08-13에 강민서가 public + BlueprintPure로 열어 줘서
	 * Stage 종류로 짐작하던 근사값을 걷어냈다.
	 *
	 * !! 이 값으로 요청을 막지 않는다 !! 버튼 활성/비활성 표시에만 쓴다.
	 * 스킵 가능 여부의 최종 판정은 Stage 시스템이 소유한다 (UI 규칙 2).
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Command")
	bool CanSkipCurrentStage() const;

	// --- 배속 (Live HUD 재생 컨트롤) ---

	/**
	 * 배속을 바꿔 달라고 요청한다.
	 *
	 * !! UI가 시간을 소유하지 않는다 !! 메인 공연 잠금 구간에서는 Stage 시스템이 요청을 무시하고
	 * 1배속을 유지한다. 여기서 미리 막지 않고 요청만 보낸다 — 잠금 판정의 소유자는 저쪽이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Command")
	void RequestSetTimeScale(float NewScale);

	/** 현재 배속. 버튼 선택 상태 표시에 쓴다. 소유자가 없으면 1을 돌려준다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	float GetTimeScale() const;

	/**
	 * 지금 배속을 바꿀 수 없는 구간인지 (메인 공연 잠금).
	 * 버튼을 비활성할 때 **이유를 문구로 함께** 보여준다. 흐려짐만으로는 왜 못 누르는지 알 수 없다 (AT-011).
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	bool IsTimeScaleLocked() const;

	/**
	 * Live 중 게임 속 시각 문구 (예: `19:42`).
	 *
	 * Stage 시스템이 `FString`으로 주는 것을 `FText`로 감싸 돌려준다.
	 * Live가 아니면 빈 값이다 — 호출하는 쪽이 비어 있으면 시계를 숨긴다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	FText GetLiveClockText() const;

	// --- 스텁 전용. 실제 소유자 구현이 붙으면 삭제한다 ---

	/**
	 * 0~100 지표를 강제로 설정한다. 값이 실제로 바뀔 때만 델리게이트를 쏘고 범위를 벗어나면 클램프한다.
	 * 경제 값은 단위가 달라 섞지 않는다. Debug_SetEconomy를 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Debug")
	void Debug_SetMetrics(int32 InSimSatisfaction, int32 InScoreSatisfaction, int32 InSafety);

	/** 경제 값을 강제로 설정한다. 금액이라 클램프하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Debug")
	void Debug_SetEconomy(int32 InGold, int32 InTotalRevenue, int32 InTodayRevenue);

	/** 현재 값 전부를 로그에 찍는다. 위젯이 없거나 화면을 못 볼 때 확인용. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Debug")
	void Debug_DumpToLog() const;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_FestivalViewModelSubsystem, STATGROUP_Tickables);
	}

protected:
	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	/**
	 * Stage 시스템이 초 단위로 쏘는 값을 받아 위젯에 중계한다.
	 *
	 * 여기서 한 번 더 중복을 걸러낸다. Stage 쪽 `LastBroadcastSeconds`가 **Stage 전환 시
	 * 초기화되지 않아서**, 새 단계의 첫 초가 이전 값과 같으면 방송이 아예 안 오고,
	 * 반대로 Sync와 겹쳐 같은 값이 두 번 올 수도 있다. 위젯이 그걸 몰라도 되게 한다.
	 */
	UFUNCTION()
	void HandleRemainingSecondsChanged(int32 NewSeconds);

	/** 경제 시스템이 쏘는 Gold를 받아 위젯에 중계한다. */
	UFUNCTION()
	void HandleOwnerGoldChanged(int32 NewValue);

	/** 미납 총액을 받아 중계한다. */
	UFUNCTION()
	void HandleOwnerArrearsChanged(int32 NewValue);

	/**
	 * 전체 인원을 받아 중계한다. 기본·알바 내역은 방송에 실려 오지 않으므로 여기서 다시 읽는다.
	 * 인자 하나만 믿고 내역을 갱신하지 않으면 "총 6명인데 기본 4 + 알바 0" 같은 화면이 된다.
	 */
	UFUNCTION()
	void HandleOwnerStaffCountChanged(int32 NewValue);

private:
	UFF_FestivalStageSubsystem* GetStageSubsystem() const;
	UFF_FestivalProgressionSubsystem* GetProgressionSubsystem() const;

	/** Stage 시스템 구독을 시도한다. 아직 없으면 재시도를 예약한다. */
	void TryBindStageSubsystem();

	/** 경제 시스템 구독을 시도한다. Stage와 같은 재시도 규칙을 쓴다. */
	void TryBindProgressionSubsystem();

	/**
	 * 소유자 시스템에서 지표를 읽어 온다. PollInterval마다 Tick에서 호출된다.
	 * 지금은 소유자가 없어 아무것도 하지 않는다 — 구현이 붙으면 **여기 한 곳만** 채운다.
	 * 변화 감지와 통지는 ApplyIntMetric이 이미 처리하므로 위젯은 손대지 않는다.
	 */
	void PollMetricsFromSources();

	/** 값이 실제로 바뀔 때만 통지한다. 같은 값을 계속 쏘면 위젯이 헛돈다. */
	static void ApplyIntMetric(int32& Current, int32 NewValue, FFF_OnIntMetricChanged& ChangedDelegate);

	// --- 지표 더미값 ---
	// !! 하드코딩 !! 실제 값이 생기면 이 초기화를 지우고 소유자 시스템 구독으로 바꾼다.

	UPROPERTY()
	int32 SimSatisfaction = 72;

	UPROPERTY()
	int32 ScoreSatisfaction = 72;

	UPROPERTY()
	int32 Safety = 88;

	/** 초기 Gold 10,000은 game-overview.md 밸런스 시드 v0의 `assumption`이다. 확정값이 아니다. */
	UPROPERTY()
	int32 Gold = 10000;

	UPROPERTY()
	int32 TotalRevenue = 0;

	UPROPERTY()
	int32 TodayRevenue = 0;

	/** 임대료 미납 누적. 하루 경계에서 리셋되지 않는다. */
	UPROPERTY()
	int32 Arrears = 0;

	// 스태프는 더미를 두지 않는다. 소유자에게서 오기 전에는 `모름`이고 화면은 칸을 숨긴다.

	UPROPERTY()
	int32 BaseStaffCount = INDEX_NONE;

	UPROPERTY()
	int32 TemporaryStaffCount = INDEX_NONE;

	UPROPERTY()
	int32 TotalStaffCount = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<UFF_FestivalStageSubsystem> CachedStageSubsystem;

	UPROPERTY()
	TObjectPtr<UFF_FestivalProgressionSubsystem> CachedProgressionSubsystem;

	/** 경제 시스템을 못 찾았을 때 재시도한 횟수. */
	int32 ProgressionBindRetryCount = 0;

	/** 남은 시간 중복 방송 차단용. 구독 중계와 Sync가 같은 값을 두 번 쏘지 않게 한다. */
	int32 LastBroadcastRemainingSeconds = INDEX_NONE;

	/** 구독 직후 현재 남은 시간을 한 번 반영한다(Bind + Sync). 중복이면 아무것도 하지 않는다. */
	void BroadcastRemainingSecondsIfChanged(int32 NewSeconds);

	float PollAccumulator = 0.f;

	/**
	 * 폴링 주기(초). 0.1초면 남은 시간의 초 단위 변화를 놓치지 않으면서 부하가 없다.
	 * 지표도 같은 주기로 샘플링하므로 위젯 갱신은 지표당 최대 초당 10회로 묶인다.
	 * 금액처럼 자주 흔들리는 값도 여기서 걸린다.
	 */
	static constexpr float PollInterval = 0.1f;

	/** Stage 시스템을 못 찾았을 때 재시도한 횟수. 상한을 두고 로그를 남긴다. */
	int32 StageBindRetryCount = 0;
	static constexpr int32 MaxStageBindRetry = 100;

#if !UE_BUILD_SHIPPING
	/**
	 * 디버그 콘솔 명령을 등록한다.
	 *
	 * === 왜 콘솔인가 ===
	 * 화면이 `ECommonInputMode::Menu`면 **게임 입력이 차단**되어, 레벨 블루프린트의 키 이벤트로는
	 * `Debug_Set*`을 부를 수 없다 (2026-08-07에 실제로 막혔다). 콘솔은 입력 모드와 무관하게 동작한다.
	 *
	 * === UFUNCTION(Exec)을 쓰지 않은 이유 ===
	 * 엔진의 `UGameInstance`에 `ProcessConsoleExec`이 없어서 **콘솔이 서브시스템의 Exec 함수를
	 * 찾아가지 않는다.** 그래서 `IConsoleManager`에 직접 등록한다.
	 *
	 * === 해제를 반드시 하는 이유 ===
	 * PIE는 한 에디터 세션에서 여러 번 돌아간다. 해제하지 않으면 두 번째 PIE에서 같은 이름 등록이
	 * 실패하고, 파괴된 this를 가리키는 델리게이트가 콘솔에 남는다.
	 * 람다는 `CreateWeakLambda`로 묶어 두 번째 방어선을 만든다.
	 */
	void RegisterDebugConsoleCommands();
	void UnregisterDebugConsoleCommands();

	IConsoleCommand* SetMetricsConsoleCommand = nullptr;
	IConsoleCommand* SetEconomyConsoleCommand = nullptr;
	IConsoleCommand* DumpMetricsConsoleCommand = nullptr;
#endif

public:
	/** 0~100 지표의 상한. 위젯이 게이지 비율을 계산할 때 이 값을 쓴다. 하드코딩하지 않게 하기 위함. */
	static constexpr int32 MetricMax = 100;
};
