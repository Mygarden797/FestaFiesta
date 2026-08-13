// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Economy/FF_FestivalFunds.h"
#include "Economy/FF_DayLedger.h"
#include "FF_FestivalProgressionSubsystem.generated.h"

class UFF_FestivalDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnEconomyValueChanged, int32, NewValue);

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

private:
	// 값이 실제로 바뀔 때만 통지한다.
	void BroadcastGoldIfChanged(int32 PreviousGold);
	void BroadcastTotalRevenueIfChanged(int32 PreviousTotalRevenue);
	void BroadcastTodayRevenueIfChanged(int32 PreviousTodayRevenue);

	// 잔액 규칙의 소유자. UObject 가 아니라 값 타입이다.
	FFF_FestivalFunds Funds;
	
	// 당일 원인별/시설별 분해. 여기서만 건드린다.
	FFF_DayLedger DayLedger;

	// 현재 축제 정의. 시작 전이면 nullptr.
	UPROPERTY()
	TObjectPtr<const UFF_FestivalDefinition> Definition;
	
};
