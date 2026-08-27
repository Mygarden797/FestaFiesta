// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Economy/Rules/FF_FestivalFunds.h"
#include "Economy/Rules/FF_DayLedger.h"
#include "Economy/Rules/FF_FixedCostArrears.h"
#include "Economy/Rules/FF_StaffPool.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "NPC/Party/FF_ProgramReservation.h"
#include "NPC/Party/FF_VisitorDistributionBuilder.h"
#include "FF_FestivalProgressionSubsystem.generated.h"

class UFF_ProgramReservationSubsystem;

class UFF_FestivalDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnEconomyValueChanged, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnFinaleGradeChanged, EFF_FinaleGrade, NewGrade);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnProgramScheduleChanged);

/**
 * 잔액 규칙은 FFF_FestivalFunds(순수 구조체)가 갖고, 여기는 "상태 보관과 통지"만 한다.
 * 축제 세션의 진행 상태를 소유한다. [참고] data/metadata/spec/economy_spec.md §4.4
 *
 * [참고] Getter + 변경 델리게이트를 쌍으로 준다
 * [참고] 전부 Gold(int32)이고 상한이 없다.
 */
UCLASS()
class FESTAFIESTA_API UFF_FestivalProgressionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// --- 축제 시작 -------------------------------------------------------

	// 축제를 시작하고 자금을 초기 자금으로 맞춘다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	void StartFestival(const UFF_FestivalDefinition* InDefinition);

	// 축제 시작됐는지
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	bool IsFestivalStarted() const { return Definition != nullptr; }

	// 현재 축제 정의. 시작 전이면 nullptr.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	const UFF_FestivalDefinition* GetDefinition() const { return Definition; }

	// --- Gold ------------------------------------------------------------

	// 지금 쓸 수 있는 잔액. 상단 바 표시와 구매 가능 판정에 쓴다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetGold() const { return Funds.GetGold(); }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnEconomyValueChanged OnGoldChanged;

	// 이 금액을 지출할 수 있는지. 상태를 바꾸지 않는다. UI 구매 버튼 비활성화 판정용.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	bool CanAfford(int32 Amount) const { return Funds.CanAfford(Amount); }

	// 지출한다.
	// 성공하면 true 이고 잔액이 그만큼 줄어든다. DayLedger에 원인별로 기록된다.
	// 실패하면 상태를 전혀 바꾸지 않고 델리게이트도 쏘지 않는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	bool TrySpend(int32 Amount, EFF_SpendReason Reason);
	
	// 누적 매출, 상단바 노출용
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetTotalRevenue() const { return Funds.GetTotalRevenue(); }
	
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnEconomyValueChanged OnTotalRevenueChanged;
	
	// 오늘 매출. Report 화면용 - 하루 경계마다 리셋된다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetTodayRevenue() const { return Funds.GetTodayRevenue(); }
	
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnEconomyValueChanged OnTodayRevenueChanged;
	
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	void AddRevenue(int32 Amount, EFF_EarnReason Reason, FName FacilityId = NAME_None);

	// 잔액을 늘린다. 시설 철거 환급이 유일한 호출자다.
	// TotalRevenue는 안 건드린다 — 환급은 새 매출이 아니라 이미 쓴 돈이 되돌아온 것이다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	void Refund(int32 Amount);

	// Report·테스트가 원인별·부스별 분해와 환급 총액을 읽는 창구.
	const FFF_DayLedger& GetDayLedger() const { return DayLedger; }
	
	// 방금 끝난 하루의 원장 — "하루 경계를 넘은 뒤"에만 유효하다.
	// [주의] Report 안에서는 아직 경계를 안 넘었으므로 여기엔 '어제' 값이 들어 있다.
	//		Day1 Report 에는 스냅샷이 아예 없다 (EndedDay == 0)
	//		Day3 뒤에는 경계가 없어서 최종 결산까지 스냅샷이 Day2 에 멈춘다.
	// [규칙] 그리려는 날짜 == GetEndedDay() 일 때만 이것을 쓰고, 아니면 GetDayLedger() 를 쓴다.
	const FFF_DayLedger& GetEndedDayLedger() const { return EndedDayLedger; }

	// GetEndedDayLedger() 가 어느 날짜의 것인지. 0 이면 아직 끝난 하루가 없다.
	// GetLastProcessedDay() 와 하루 어긋난다 — 이쪽은 '닫힌 날', 저쪽은 '열린 날'이다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetEndedDay() const { return EndedDay; }
	
	
	// --- 결산 표 (UI 읽기 모델) ---------------------------------------------
	//
	// 날짜를 인자로 받는다. 원장이 두 개(라이브·스냅샷)라 "어느 쪽인가"를 호출부가
	// 판단하면 Day1·Day3 에서 조용히 틀린 표가 나온다. 그리려는 날짜만 넘기면 안쪽에서 고른다.
	// 들고 있는 것은 두 칸뿐이다 — 진행 중인 하루와 방금 닫힌 하루.

	// 그 날짜의 원장을 갖고 있는가.
	// [주의] 아래 4종이 **빈 결과인 것과 "그 날 값이 0" 인 것을 구분하는 유일한 수단**이다.
	//        표를 그리기 전에 이걸 먼저 물어야 한다 (NeedMetrics 의 GetSampleCount()==0 과 같은 원칙).
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	bool HasLedgerForDay(int32 Day) const;

	// 매출 5종을 enum 선언 순서로. 값 0 인 사유는 줄을 만들지 않는다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	TArray<FFF_LedgerRow> GetEarnRowsForDay(int32 Day) const;

	// 지출 9종을 enum 선언 순서로. 값 0 인 사유는 줄을 만들지 않는다.
	// [주의] 장소 임대료는 하루치(2,400)를 넘을 수 있다 — 어제 못 낸 몫을 오늘 갚으면 합산된다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	TArray<FFF_LedgerRow> GetSpendRowsForDay(int32 Day) const;

	// 부스별 매출을 FacilityId 오름차순으로. UI 가 다시 정렬하지 않는다.
	// [주의] 표시 이름은 주지 않는다 — 경제는 시설의 한글 이름을 모른다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	TArray<FFF_FacilityRevenueRow> GetRevenueByFacilityRowsForDay(int32 Day) const;

	// 시설 철거 환급 총액. **매출이 아니다** — GetEarnRowsForDay 에 섞이지 않는다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetTotalRefundForDay(int32 Day) const;
	
	
	
	// --- 하루 경계 ---------------------------------------------------------
	
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	void BeginNewDay(int32 NewDay);
	
	// 임대료를 마지막으로 정산한 날. 축제 시작 전이면 0.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetLastProcessedDay() const { return LastProcessedDay; }
	
	
	// --- Staff ------------------------------------------------------------
	
	// 무페이 기본 Staff.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetBaseStaffCount() const
	{
		return StaffPool.GetBaseStaffCount();
	}

	// 오늘 고용한 알바. 임금을 낸 만큼이다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetTemporaryStaffCount() const
	{
		return StaffPool.GetTemporaryStaffCount();
	}

	// 오늘 실제로 일하는 전체 인원.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetTotalStaffCount() const
	{
		return StaffPool.GetTotalStaffCount();
	}
	
	// 오늘 알바를 RequestedCount 명 고용하고 임금 지불
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	bool TryHireTemporaryStaff(int32 RequestedCount);
	
	// 지금 잔액으로 추가 고용할 수 있는 최대 인원. 상태를 바꾸지 않는다.
	// UI 슬라이더 상한과 버튼 비활성화 판정용.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetAffordableTemporaryStaffCount() const;

	// 오늘 하루 상한까지 남은 고용 가능 인원. 잔액은 보지 않는다.
	// `GetAffordableTemporaryStaffCount()`과 다르면 막은 것이 상한인지 돈인지 화면에서 구분할 수 있다.
	// [주의] "왜 더 못 뽑는가"를 화면에 쓰기 위한 것이다. 고용 가능 판정에 쓰지 말 것. 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetRemainingHireCap() const;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnEconomyValueChanged OnTotalStaffCountChanged;
	
	
	// --- 피날레 -----------------------------------------------------------

	// 피날레를 구매한다. 실패 이유를 결과로 돌려주고, 실패 시 상태를 전혀 바꾸지 않는다.
	// Day3 MorningCheck 판정은 UFF_StageEconomyBridgeSubsystem 에서 진행
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	EFF_FinalePurchaseResult TryPurchaseFinale(EFF_FinaleGrade Grade);

	// 구매한 등급. 안 샀으면 None.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	EFF_FinaleGrade GetPurchasedFinaleGrade() const { return PurchasedFinaleGrade; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnFinaleGradeChanged OnFinaleGradeChanged;

	// 구매한 등급의 만족도 가산값 (0~100 척도). 안 샀으면 0.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetFinaleSatisfactionBonus() const;


	// --- 유료 프로그램 -----------------------------------------------------

	// 편성할 수 있는 카드 전체. 축제가 시작되지 않았으면 빈 배열.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	TArray<FFF_ProgramCard> GetProgramCatalog() const;

	// 오늘 편성한 카드와 그 예약 핸들. 하루 경계에서 비워진다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	TArray<FFF_ScheduledProgram> GetScheduledPrograms() const { return ScheduledPrograms; }

	// 오늘 프로그램에 묶여 있는 인원.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetStaffAssignedToPrograms() const;

	// vent 대응에 쓸 수 있는 인원 = 전체 − 프로그램에 묶인 인원.
	// 즉, 프로그램을 편성하면 줄어든다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetAvailableStaffCount() const;

	// 편성 가능한지만 본다. 상태를 바꾸지 않으므로 버튼 활성화·툴팁에 그대로 쓴다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	EFF_ProgramPurchaseResult CanScheduleProgram(FName ProgramId) const;

	/**
	 * 프로그램을 오늘 편성한다. 실패 시 상태를 바꾸지 않는다.
	 * 허용 단계(Setup·MorningCheck) 판정은 호출부가 한다.
	 *
	 * @param FacilityInstanceId 프로그램을 여는 시설. ZoneSystem 의 인스턴스 핸들이다
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	EFF_ProgramPurchaseResult TryScheduleProgram(FName ProgramId, FGuid FacilityInstanceId);

	/**
	 * 시작 시각이 지난 프로그램의 체험비를 원장에 넣는다. **정상 경로다.**
	 *
	 * 참가자는 스폰될 때 이미 확정된다 (`UFF_ProgramReservationSubsystem::ConfirmReservedParty`).
	 * 프로그램을 사면 정원만큼의 손님이 추가로 방문하므로, 프로그램이 열리는 순간 체험비는
	 * 확정값이다 — 마감까지 기다릴 이유가 없다. 부스 수수료도 이미 Live 중 실시간이다.
	 *
	 * 한 번 걷은 프로그램은 다시 걷지 않는다. Live 동안 매 초 불려도 안전하다.
	 *
	 * @param NowMinutes Live 경과 시각(분). `FFF_FestivalClock::TryGetLiveElapsedMinutes` 로 얻는다
	 * @return 이번 호출에서 원장에 들어간 체험비 총액
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	int32 SettleStartedPrograms(float NowMinutes);

	/**
	 * 아직 안 걷은 프로그램의 체험비를 전부 걷는다. **안전망이다.**
	 *
	 * Closing 진입에서 한 번 부른다. 여기서 걷히는 것이 있으면 시작 시각 정산이 그 카드를
	 * 놓쳤다는 뜻이라 Warning 을 남긴다 — 카드 시각이 그날 Live 길이를 넘었거나
	 * Live 를 스킵한 경우다.
	 *
	 * @return 이번 호출에서 원장에 들어간 체험비 총액. 정상이면 0 이다
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	int32 SettleScheduledPrograms();

	/**
	 * 오늘 편성한 프로그램들이 데려올 손님의 주문서. (스폰 쪽에 넘길 목록)
	 *
	 * 아침에 편성이 끝난 뒤 그날 유입을 시작할 때 한 번 읽는다.
	 * 합계 인원은 편성한 카드들의 정원 합과 같다.
	 */
	TArray<FFF_ReservedPartyRequest> GetReservedPartyRequests() const;

	// 예약 슬롯의 소유자를 물린다.
	void SetProgramReservations(UFF_ProgramReservationSubsystem* InReservations);

	// 오늘 편성 목록이 바뀌면 쏜다. 인원 표시도 같이 갱신해야 한다.
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnProgramScheduleChanged OnScheduledProgramsChanged;
	
	
	// --- 미납 -------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetTotalArrears() const { return Arrears.GetTotalArrears(); }

	// 미납 총액이 바뀌면 쏜다. 자금난 경고·엔딩 평가가 붙을 자리다 (E-24).
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnEconomyValueChanged OnTotalArrearsChanged;

private:
	// 이 시설에서 이 프로그램을 열 수 있는가.
	// 시설 종류를 ZoneSystem 에서 읽어 카드의 허용 목록과 대조한다.
	bool IsFacilityTypeAllowedForCard(const FFF_ProgramCard& Card, const FGuid& FacilityInstanceId) const;

	/**
	 * 프로그램 하나의 체험비를 원장에 넣고 `bFeeSettled` 를 켠다.
	 * 시작 시각 정산과 Closing 안전망이 같은 계산을 쓰게 하는 유일한 구현이다.
	 *
	 * @param bLateSettlement 안전망 경로인가. Warning 문구가 갈린다
	 * @return 원장에 들어간 금액. 이미 걷었거나 카드·예약을 못 찾으면 0
	 */
	int32 SettleOneProgram(FFF_ScheduledProgram& Scheduled, bool bLateSettlement);

	// 값이 실제로 바뀔 때만 통지한다.
	void BroadcastGoldIfChanged(int32 PreviousGold);
	void BroadcastTotalRevenueIfChanged(int32 PreviousTotalRevenue);
	void BroadcastTodayRevenueIfChanged(int32 PreviousTodayRevenue);
	void BroadcastTotalArrearsIfChanged(int32 PreviousTotalArrears);
	void BroadcastTotalStaffCountIfChanged(int32 PreviousTotalStaffCount);
	
	// 하루 경제 정산을 수행한다.
	// 같은 날 이미 처리했다면 아무 작업도 하지 않고 false 를 반환한다.
	bool TryProcessNewDay(int32 NewDay);
	
	// Day 에 해당하는 원장을 찾는다. 안 들고 있으면 nullptr.
	const FFF_DayLedger* FindLedgerForDay(int32 Day) const;

	// 당일 임대료를 정산한다.
	// 잔액이 부족해도 거부하지 않으며, 낼 수 있는 만큼 차감하고 부족분은 미납으로 남긴다.
	void SettleVenueRent();

	// 현재 Gold 로 기존 미납을 상환하고 실제 상환액을 반환한다.
	// 미납이 남아 있는 동안 Gold 가 잔액으로 남지 않도록 가능한 만큼 전액 상환에 사용한다.
	int32 RepayArrearsFromGold();

	// 잔액 규칙의 소유자. UObject 가 아니라 값 타입이다.
	FFF_FestivalFunds Funds;
	
	// 당일 원인별/시설별 분해. 여기서만 건드린다.
	FFF_DayLedger DayLedger;
	
	// 직전 하루의 원장 스냅샷. TryProcessNewDay 의 Reset 직전에서만 갱신한다
	FFF_DayLedger EndedDayLedger;

	// EndedDayLedger 가 담고 있는 날짜. 0 = 아직 끝난 하루 없음.
	// LastProcessedDay 와 하루 어긋난다 (닫힌 날 vs 열린 날). 둘을 섞어 쓰지 않는다.
	int32 EndedDay = 0;
	
	// 임대료 미납. 하루 경계에서 리셋되지 않는다.
	FFF_FixedCostArrears Arrears;

	// 무페이 기본 팀 + 오늘 고용한 알바.
	FFF_StaffPool StaffPool;
	
	// 임대료를 마지막으로 정산한 날. 같은 날 중복 정산을 막는 수단이다.
	int32 LastProcessedDay = 0;
	
	// 구매한 피날레 등급. 축제당 1회이고 되돌릴 수 없다.
	EFF_FinaleGrade PurchasedFinaleGrade = EFF_FinaleGrade::None;

	// 오늘 편성한 프로그램. 카드가 하루짜리라 하루 경계에서 비운다.
	TArray<FFF_ScheduledProgram> ScheduledPrograms;

	// 예약 슬롯의 소유자. GameInstance 스코프로 수명이 같아서 캐시해도 된다.
	UPROPERTY()
	TObjectPtr<UFF_ProgramReservationSubsystem> ProgramReservations;

	// 편성된 카드의 ProgramId 만 뽑는다. 순수 판정 함수가 그 형태를 받는다.
	TArray<FName> GetScheduledProgramIds() const;
	
	// 현재 축제 정의. 시작 전이면 nullptr.
	UPROPERTY()
	TObjectPtr<const UFF_FestivalDefinition> Definition;
};
