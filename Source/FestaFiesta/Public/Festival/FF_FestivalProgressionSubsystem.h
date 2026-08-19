// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Economy/Rules/FF_FestivalFunds.h"
#include "Economy/Rules/FF_DayLedger.h"
#include "Economy/Rules/FF_FixedCostArrears.h"
#include "Economy/Rules/FF_StaffPool.h"
#include "FF_FestivalProgressionSubsystem.generated.h"

class UFF_FestivalDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnEconomyValueChanged, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnFinaleGradeChanged, EFF_FinaleGrade, NewGrade);

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
	
	
	// --- 미납 -------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetTotalArrears() const { return Arrears.GetTotalArrears(); }

	// 미납 총액이 바뀌면 쏜다. 자금난 경고·엔딩 평가가 붙을 자리다 (E-24).
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnEconomyValueChanged OnTotalArrearsChanged;

private:
	// 값이 실제로 바뀔 때만 통지한다.
	void BroadcastGoldIfChanged(int32 PreviousGold);
	void BroadcastTotalRevenueIfChanged(int32 PreviousTotalRevenue);
	void BroadcastTodayRevenueIfChanged(int32 PreviousTodayRevenue);
	void BroadcastTotalArrearsIfChanged(int32 PreviousTotalArrears);
	void BroadcastTotalStaffCountIfChanged(int32 PreviousTotalStaffCount);
	
	// 하루 경제 정산을 수행한다.
	// 같은 날 이미 처리했다면 아무 작업도 하지 않고 false 를 반환한다.
	bool TryProcessNewDay(int32 NewDay);

	// 당일 임대료를 정산한다.
	// 잔액이 부족해도 거부하지 않으며, 낼 수 있는 만큼 차감하고 부족분은 미납으로 남긴다.
	void SettleVenueRent();

	// 현재 Gold 로 기존 미납을 상환하고 실제 상환액을 반환한다.
	// 미납이 남아 있는 동안 Gold 가 잔액으로 남지 않도록 가능한 만큼 전액 상환에 사용한다.
	int32 RepayArrearsFromGold();
	
	int32 GetRemainingHireCap() const;
	
	// 잔액 규칙의 소유자. UObject 가 아니라 값 타입이다.
	FFF_FestivalFunds Funds;
	
	// 당일 원인별/시설별 분해. 여기서만 건드린다.
	FFF_DayLedger DayLedger;
	
	// 임대료 미납. 하루 경계에서 리셋되지 않는다.
	FFF_FixedCostArrears Arrears;

	// 무페이 기본 팀 + 오늘 고용한 알바.
	FFF_StaffPool StaffPool;
	
	// 임대료를 마지막으로 정산한 날. 같은 날 중복 정산을 막는 수단이다.
	int32 LastProcessedDay = 0;
	
	// 구매한 피날레 등급. 축제당 1회이고 되돌릴 수 없다.
	EFF_FinaleGrade PurchasedFinaleGrade = EFF_FinaleGrade::None;
	
	// 현재 축제 정의. 시작 전이면 nullptr.
	UPROPERTY()
	TObjectPtr<const UFF_FestivalDefinition> Definition;
};
