// Copyright Epic Games, Inc. All Rights Reserved.

#include "Festival/FF_FestivalProgressionSubsystem.h"

#include "Economy/FF_EconomyLog.h"
#include "Economy/FF_FinaleRules.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalSettings.h"

void UFF_FestivalProgressionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UFF_FestivalSettings* Settings = UFF_FestivalSettings::Get();
	if (Settings == nullptr || Settings->DefaultFestival.IsNull())
	{
		// 설정이 비어 있으면 경제를 시작하지 않는다. Gold 는 0 으로 남는다.
		// 임의의 기본값을 만들지 않는다 — "설정이 안 됐다"와 "돈이 없다"는 다른 상태이고,
		// 조용히 10,000 을 넣으면 저작 누락이 플레이 중에 드러난다.
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[축제] DefaultFestival 이 지정되지 않았다. Project Settings > Game > FestaFiesta Festival에서 축제 정의를 지정해야 축제 진행 상태가 초기화된다."));
		return;
	}

	// 작은 DataAsset 하나라 동기 로드로 충분하다. GameInstance 초기화 시점이라 프레임 예산과 무관하다.
	StartFestival(Settings->DefaultFestival.LoadSynchronous());
}

void UFF_FestivalProgressionSubsystem::StartFestival(const UFF_FestivalDefinition* InDefinition)
{
	if (InDefinition == nullptr)
	{
		UE_LOG(LogFestaFiestaEconomy, Error,
			TEXT("[경제] StartFestival 에 nullptr 이 들어왔다. 경제를 시작하지 않는다."));
		return;
	}

	// 저작 오류가 있으면 시작은 하되 시끄럽게 남긴다. 기획서 §13 AT-012 는 로드 전 거부를 요구하지만
	// 그 창구는 에디터 Validate Assets 다. 런타임에서 여기까지 왔다면 검증을 건너뛴 경로가 있다는 뜻이다.
	TArray<FText> Errors;
	if (!InDefinition->IsDefinitionValid(Errors))
	{
		for (const FText& Error : Errors)
		{
			UE_LOG(LogFestaFiestaEconomy, Error, TEXT("[경제] 축제 정의 오류: %s"), *Error.ToString());
		}
	}

	const int32 PreviousGold = Funds.GetGold();
	const int32 PreviousTotalRevenue = Funds.GetTotalRevenue();
	const int32 PreviousTodayRevenue = Funds.GetTodayRevenue();
	const int32 PreviousTotalArrears = Arrears.GetTotalArrears();
	const int32 PreviousTotalStaffCount = StaffPool.GetTotalStaffCount();

	Definition = InDefinition;
	Funds.Initialize(InDefinition->InitialGold);
	DayLedger.Reset();
	Arrears.ResetArrears();
	StaffPool.Initialize(InDefinition->InitialStaffCount);
	PurchasedFinaleGrade = EFF_FinaleGrade::None;
	
	// 새 축제
	LastProcessedDay = 0;

	UE_LOG(LogFestaFiestaEconomy, Log, TEXT("[경제] 축제 시작: %s, 초기 Gold %d, 기본 Staff %d명"),
		*InDefinition->FestivalId.ToString(), Funds.GetGold(), StaffPool.GetBaseStaffCount());
	
	// Day1 도 정산 대상 (Day1 은 Setup 으로 시작해 MorningCheck 을 통과하지 않으므로)
	TryProcessNewDay(1);

	BroadcastGoldIfChanged(PreviousGold);
	BroadcastTotalRevenueIfChanged(PreviousTotalRevenue);
	BroadcastTodayRevenueIfChanged(PreviousTodayRevenue);
	BroadcastTotalArrearsIfChanged(PreviousTotalArrears);
	BroadcastTotalStaffCountIfChanged(PreviousTotalStaffCount);
}

bool UFF_FestivalProgressionSubsystem::TrySpend(int32 Amount, EFF_SpendReason Reason)
{
	const int32 PreviousGold = Funds.GetGold();

	if (!Funds.TrySpend(Amount))
	{
		// 실패는 상태를 바꾸지 않으므로 델리게이트도 쏘지 않는다.
		return false;
	}
	
	DayLedger.RecordSpend(Reason, Amount);
	
	BroadcastGoldIfChanged(PreviousGold);
	return true;
}

void UFF_FestivalProgressionSubsystem::AddRevenue(int32 Amount, EFF_EarnReason Reason, FName FacilityId)
{
	const int32 PreviousGold = Funds.GetGold();
	const int32 PreviousTotalRevenue = Funds.GetTotalRevenue();
	const int32 PreviousTodayRevenue = Funds.GetTodayRevenue();
	const int32 PreviousTotalArrears = Arrears.GetTotalArrears();

	Funds.AddRevenue(Amount);
	DayLedger.RecordEarn(Reason, Amount, FacilityId);
	
	// 미납이 있으면 방금 들어온 돈이 잔액에 남지 않고 먼저 임대료를 갚는다.
	RepayArrearsFromGold();

	BroadcastGoldIfChanged(PreviousGold);
	BroadcastTotalRevenueIfChanged(PreviousTotalRevenue);
	BroadcastTodayRevenueIfChanged(PreviousTodayRevenue);
	BroadcastTotalArrearsIfChanged(PreviousTotalArrears);
}

void UFF_FestivalProgressionSubsystem::Refund(int32 Amount)
{
	const int32 PreviousGold = Funds.GetGold();
	const int32 PreviousTotalArrears = Arrears.GetTotalArrears();
	
	Funds.Refund(Amount);
	DayLedger.RecordRefund(Amount);
	
	// 미납이 있으면 방금 들어온 돈이 잔액에 남지 않고 먼저 임대료를 갚는다.
	RepayArrearsFromGold();
	
	BroadcastGoldIfChanged(PreviousGold);
	BroadcastTotalArrearsIfChanged(PreviousTotalArrears);
}

void UFF_FestivalProgressionSubsystem::BeginNewDay(int32 NewDay)
{
	const int32 PreviousGold = Funds.GetGold();
	const int32 PreviousTodayRevenue = Funds.GetTodayRevenue();
	const int32 PreviousTotalArrears = Arrears.GetTotalArrears();
	const int32 PreviousTotalStaffCount = StaffPool.GetTotalStaffCount();
	
	// 실제로 정산했으면 true, 같은 날 중복 호출이라 건너뛰었으면 false.
	if (!TryProcessNewDay(NewDay)) return;
	
	BroadcastGoldIfChanged(PreviousGold);
	BroadcastTodayRevenueIfChanged(PreviousTodayRevenue);
	BroadcastTotalArrearsIfChanged(PreviousTotalArrears);
	BroadcastTotalStaffCountIfChanged(PreviousTotalStaffCount);
}

bool UFF_FestivalProgressionSubsystem::TryHireTemporaryStaff(int32 RequestedCount)
{
	if (Definition == nullptr || RequestedCount <= 0) return false;

	const int32 AlreadyHired = StaffPool.GetTemporaryStaffCount();
	const int32 RemainingCap = GetRemainingHireCap();
	
	if (RequestedCount > RemainingCap)
	{
		UE_LOG(LogFestaFiestaEconomy, Log,
			TEXT("[경제] 알바 고용 거부: 상한 초과 (오늘 %d명 + 요청 %d명 > 상한 %d명)"),
			AlreadyHired, RequestedCount, Definition->MaxHiredStaffPerDay);
		return false;
	}

	const int32 Wage = FFF_StaffPool::CalcTemporaryStaffWage(
		RequestedCount, Definition->DailyStaffWageGold);

	const int32 PreviousGold = Funds.GetGold();
	const int32 PreviousTotalStaffCount = StaffPool.GetTotalStaffCount();

	// 일일 알바 채용이기 때문에 보유 Gold 내에서만 임금을 낸다. 즉, 지출 거부 가능하다.
	if (!Funds.TrySpend(Wage))
	{
		UE_LOG(LogFestaFiestaEconomy, Log,
			TEXT("[경제] 알바 고용 거부: 잔액 부족 (임금 %d, 잔액 %d)"), Wage, Funds.GetGold());
		return false;
	}

	StaffPool.SetTemporaryStaffCount(AlreadyHired + RequestedCount);
	DayLedger.RecordSpend(EFF_SpendReason::StaffWages, Wage);

	BroadcastGoldIfChanged(PreviousGold);
	BroadcastTotalStaffCountIfChanged(PreviousTotalStaffCount);
	return true;
}

int32 UFF_FestivalProgressionSubsystem::GetRemainingHireCap() const
{
	if (Definition == nullptr) return 0;

	return FMath::Max(0,
		Definition->MaxHiredStaffPerDay - StaffPool.GetTemporaryStaffCount());
}

int32 UFF_FestivalProgressionSubsystem::GetAffordableTemporaryStaffCount() const
{
	if (Definition == nullptr) return 0;

	return StaffPool.CalcAffordableTemporaryStaffCount(
		Definition->DailyStaffWageGold, Funds.GetGold(), GetRemainingHireCap());
}

EFF_FinalePurchaseResult UFF_FestivalProgressionSubsystem::TryPurchaseFinale(EFF_FinaleGrade Grade)
{
	if (Definition == nullptr) return EFF_FinalePurchaseResult::FestivalNotStarted;
	
	FFF_FinaleTier Tier;
	const EFF_FinalePurchaseResult Result = FFF_FinaleRules::EvaluatePurchase(
		Definition->FinaleTiers, Grade, PurchasedFinaleGrade, Funds.GetGold(), Tier);
	
	if (Result != EFF_FinalePurchaseResult::Success)
	{
		UE_LOG(LogFestaFiestaEconomy, Log, TEXT("[경제] 피날레 구매 거부: %s (요청 등급 %s, 잔액 %d)"),
			*UEnum::GetValueAsString(Result), *UEnum::GetValueAsString(Grade), Funds.GetGold());
		return Result;
	}

	// 판정이 통과했으므로 차감이 실패할 수 없다. (이상 상황)
	if (!ensureMsgf(TrySpend(Tier.CostGold, EFF_SpendReason::FinaleFireworks),
		TEXT("피날레 비용 %d 차감이 판정 통과 후 실패했다. 잔액 %d"), Tier.CostGold, Funds.GetGold()))
	{
		return EFF_FinalePurchaseResult::InsufficientGold;
	}

	PurchasedFinaleGrade = Grade;

	UE_LOG(LogFestaFiestaEconomy, Log, TEXT("[경제] 피날레 %s 구매. 비용 %d, 남은 Gold %d, 만족도 보너스 +%d"),
		*UEnum::GetValueAsString(Grade), Tier.CostGold, Funds.GetGold(), Tier.SatisfactionBonus);

	OnFinaleGradeChanged.Broadcast(PurchasedFinaleGrade);
	return EFF_FinalePurchaseResult::Success;
}

int32 UFF_FestivalProgressionSubsystem::GetFinaleSatisfactionBonus() const
{
	if (Definition == nullptr) return 0;

	const FFF_FinaleTier* Tier =
		FFF_FinaleRules::FindTier(Definition->FinaleTiers, PurchasedFinaleGrade);

	return Tier != nullptr ? Tier->SatisfactionBonus : 0;
}

void UFF_FestivalProgressionSubsystem::BroadcastGoldIfChanged(int32 PreviousGold)
{
	const int32 CurrentGold = Funds.GetGold();
	if (CurrentGold == PreviousGold) return;

	OnGoldChanged.Broadcast(CurrentGold);
}

void UFF_FestivalProgressionSubsystem::BroadcastTotalRevenueIfChanged(int32 PreviousTotalRevenue)
{
	const int32 CurrentTotalRevenue = Funds.GetTotalRevenue();
	if (CurrentTotalRevenue == PreviousTotalRevenue) return;
	
	OnTotalRevenueChanged.Broadcast(CurrentTotalRevenue);
}

void UFF_FestivalProgressionSubsystem::BroadcastTodayRevenueIfChanged(int32 PreviousTodayRevenue)
{
	const int32 CurrentTodayRevenue = Funds.GetTodayRevenue();
	if (CurrentTodayRevenue == PreviousTodayRevenue) return;

	OnTodayRevenueChanged.Broadcast(CurrentTodayRevenue);
}

void UFF_FestivalProgressionSubsystem::BroadcastTotalArrearsIfChanged(int32 PreviousTotalArrears)
{
	const int32 CurrentTotalArrears = Arrears.GetTotalArrears();
	if (CurrentTotalArrears == PreviousTotalArrears) return;

	OnTotalArrearsChanged.Broadcast(CurrentTotalArrears);
}

void UFF_FestivalProgressionSubsystem::BroadcastTotalStaffCountIfChanged(int32 PreviousTotalStaffCount)
{
	const int32 CurrentTotalStaffCount = StaffPool.GetTotalStaffCount();
	if (CurrentTotalStaffCount == PreviousTotalStaffCount) return;

	OnTotalStaffCountChanged.Broadcast(CurrentTotalStaffCount);
}

bool UFF_FestivalProgressionSubsystem::TryProcessNewDay(int32 NewDay)
{
	if (Definition == nullptr)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] 축제가 시작되지 않았는데 BeginNewDay(%d) 가 호출됐다. 무시한다."), NewDay);
		return false;
	}

	if (NewDay <= LastProcessedDay)
	{
		// 스테이지 재진입으로 인한 중복 호출 방지
		UE_LOG(LogFestaFiestaEconomy, Verbose,
			TEXT("[경제] Day %d 는 이미 정산했다(마지막 정산일 %d). BeginNewDay 를 건너뛴다."),
			NewDay, LastProcessedDay);
		return false;
	}

	LastProcessedDay = NewDay;

	// TodayRevenue & DayLedger 리셋
	Funds.ResetTodayRevenue();
	DayLedger.Reset();
	
	// 알바는 일당 계약이라 매일 0 으로 돌아간다. 플레이어가 오늘 다시 정한다.
	StaffPool.SetTemporaryStaffCount(0);

	SettleVenueRent();       // 거절할 수 없다 — 못 내면 미납
	
	ensureMsgf(Arrears.HasNoArrears() || Funds.GetGold() == 0,
		TEXT("미납 %d 이 남았는데 잔액이 %d 다. 상환 훅을 안 부르는 유입 경로가 있다."),
		Arrears.GetTotalArrears(), Funds.GetGold());
	
	RepayArrearsFromGold();  // 남은 잔액으로 밀린 것부터

	UE_LOG(LogFestaFiestaEconomy, Log,
		TEXT("[경제] Day %d 시작. 정산 후 Gold %d, 미납 %d, Staff %d명(기본 %d + 알바 %d)"),
		NewDay, Funds.GetGold(), Arrears.GetTotalArrears(),
		StaffPool.GetTotalStaffCount(), StaffPool.GetBaseStaffCount(),
		StaffPool.GetTemporaryStaffCount());

	return true;
}

void UFF_FestivalProgressionSubsystem::SettleVenueRent()
{
	const int32 Rent = Definition->DailyVenueRentGold;
	if (Rent <= 0) return;
	
	const int32 Paid = Funds.PayMandatoryCost(Rent);
	if (Paid > 0)
	{
		// 실제로 빠져나간 금액을 기록
		DayLedger.RecordSpend(EFF_SpendReason::VenueRent, Paid);
	}
	
	// 못 낸 부분은 미납이 된다.
	Arrears.AddArrears(Rent - Paid);
}


int32 UFF_FestivalProgressionSubsystem::RepayArrearsFromGold()
{
	// 정상적으로 미납금을 내고 있었다면 얼리 리턴
	if (Arrears.HasNoArrears() || Funds.GetGold() <= 0) return 0;

	const int32 Repaid = Arrears.RepayArrears(Funds.GetGold());
	if (Repaid <= 0) return 0;

	// 상환액은 잔액을 넘지 않게 잘라 왔으므로 전액이 빠져야 한다.
	const int32 Deducted = Funds.PayMandatoryCost(Repaid);
	
	ensureMsgf(Deducted == Repaid,
		TEXT("미납 상환액이 잔액을 넘었다. Repaid=%d Deducted=%d"), Repaid, Deducted);

	// 임대료 지출로 기록
	DayLedger.RecordSpend(EFF_SpendReason::VenueRent, Deducted);

	UE_LOG(LogFestaFiestaEconomy, Log, TEXT("[경제] 밀린 임대료 %d 상환. 남은 미납 %d"),
		Deducted, Arrears.GetTotalArrears());

	return Deducted;
}
