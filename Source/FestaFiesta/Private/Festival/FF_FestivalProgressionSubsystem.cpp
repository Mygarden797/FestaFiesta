// Copyright Epic Games, Inc. All Rights Reserved.

#include "Festival/FF_FestivalProgressionSubsystem.h"

#include "Economy/FF_EconomyLog.h"
#include "Economy/FF_EconomyDisplayTextLibrary.h"
#include "Economy/Rules/FF_FinaleRules.h"
#include "Economy/Rules/FF_ProgramScheduleRules.h"
#include "Building/FF_FacilityNamingLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalSettings.h"

namespace
{
	// enum 값 하나를 결산 표의 한 줄로 만든다.
	// DisplayName 을 밖에서 받는다. (GetDisplayNameTextByValue 로 뽑으면 안 되기 때문)
	// UMETA 조회가 통째로 `#if WITH_EDITOR` 안에 있어서 패키징 빌드에서는 짧은 enum 이름으로
	// 폴백되고, 그러면 DisplayName 이 ReasonId 와 똑같은 값이 된다.
	FFF_LedgerRow MakeReasonRow(const UEnum* EnumType, int64 Value, int32 Amount, const FText& DisplayName)
	{
		FFF_LedgerRow Row;
		Row.Amount = Amount;
		Row.DisplayName = DisplayName;

		if (EnumType != nullptr)
		{
			// 코드에 작성된 enum 이름 자체를 얻는 함수
			Row.ReasonId = FName(*EnumType->GetAuthoredNameStringByValue(Value));
		}

		return Row;
	}
}


void UFF_FestivalProgressionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 예약 슬롯의 소유자. 같은 GameInstance 스코프라 수명이 같아서 캐시해도 된다.
	// StartFestival 이 아래에서 ResetAll 을 부르므로 그 전에 물려야 한다.
	SetProgramReservations(
		Collection.InitializeDependency<UFF_ProgramReservationSubsystem>());

	StartConfiguredFestival();
}

void UFF_FestivalProgressionSubsystem::StartConfiguredFestival()
{
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

void UFF_FestivalProgressionSubsystem::RestartFestival()
{
	if (Definition != nullptr)
	{
		StartFestival(Definition);
		return;
	}

	// 여기 오는 것은 `Initialize` 가 설정 누락으로 그냥 돌아갔던 경우다. 그 사이에 저작이
	// 채워졌을 수 있으니 한 번 더 본다. 여전히 비어 있으면 저쪽이 Warning 을 남긴다.
	StartConfiguredFestival();
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
	// 새 축제에 이전 판 스냅샷이 남지 않게 한다.
	// [주의] 지금은 아래 TryProcessNewDay(1) 이 어차피 덮으므로 이 두 줄이 없어도 동작이 같다
	//        (변이 검사로 확인: 지우면 죽는 테스트가 없다). 호출 순서가 바뀔 때를 위한 방어다.
	EndedDayLedger.Reset();
	EndedDay = 0;
	Arrears.ResetArrears();
	StaffPool.Initialize(InDefinition->InitialStaffCount);
	PurchasedFinaleGrade = EFF_FinaleGrade::None;
	ScheduledPrograms.Reset();
	if (ProgramReservations != nullptr)
	{
		// 새 축제에 이전 판의 슬롯이 남지 않게 한다.
		ProgramReservations->ResetAll();
	}
	
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

bool UFF_FestivalProgressionSubsystem::HasLedgerForDay(int32 Day) const
{
	return FindLedgerForDay(Day) != nullptr;
}

TArray<FFF_LedgerRow> UFF_FestivalProgressionSubsystem::GetEarnRowsForDay(int32 Day) const
{
	TArray<FFF_LedgerRow> Rows;
	
	const FFF_DayLedger* Ledger = FindLedgerForDay(Day);
	if (Ledger == nullptr)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] Day %d 의 원장이 없다(진행 중 %d · 닫힌 날 %d). 빈 표를 돌려준다."),
			Day, LastProcessedDay, EndedDay);
		return Rows;
	}
	
	// EFF_EarnReason enum의 리플렉션 정보를 가져옴.
	// 이후 enum 값의 내부 이름/표시 이름을 얻을 때 사용.
	const UEnum* ReasonEnum = StaticEnum<EFF_EarnReason>();
	
	for (int64 Value = 0; Value <= static_cast<int64>(EFF_EarnReason::MinigameReward); ++Value)
	{
		const int32 Amount = Ledger->GetEarnByReason(static_cast<EFF_EarnReason>(Value));
		if (Amount == 0) continue;
		Rows.Add(MakeReasonRow(ReasonEnum, Value, Amount,
			UFF_EconomyDisplayTextLibrary::GetEarnReasonText(static_cast<EFF_EarnReason>(Value))));
	}
	
	return Rows;
}

TArray<FFF_LedgerRow> UFF_FestivalProgressionSubsystem::GetSpendRowsForDay(int32 Day) const
{
	TArray<FFF_LedgerRow> Rows;

	const FFF_DayLedger* Ledger = FindLedgerForDay(Day);
	if (Ledger == nullptr)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] Day %d 의 원장이 없다(진행 중 %d · 닫힌 날 %d). 빈 표를 돌려준다."),
			Day, LastProcessedDay, EndedDay);
		return Rows;
	}

	const UEnum* ReasonEnum = StaticEnum<EFF_SpendReason>();

	for (int64 Value = 0; Value <= static_cast<int64>(EFF_SpendReason::FinaleFireworks); ++Value)
	{
		const int32 Amount = Ledger->GetSpendByReason(static_cast<EFF_SpendReason>(Value));
		if (Amount == 0)
		{
			continue;
		}

		Rows.Add(MakeReasonRow(ReasonEnum, Value, Amount,
			UFF_EconomyDisplayTextLibrary::GetSpendReasonText(static_cast<EFF_SpendReason>(Value))));
	}

	return Rows;
}

TArray<FFF_FacilityRevenueRow> UFF_FestivalProgressionSubsystem::GetRevenueByFacilityRowsForDay(int32 Day) const
{
	TArray<FFF_FacilityRevenueRow> Rows;

	const FFF_DayLedger* Ledger = FindLedgerForDay(Day);
	if (Ledger == nullptr)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] Day %d 의 원장이 없다(진행 중 %d · 닫힌 날 %d). 빈 표를 돌려준다."),
			Day, LastProcessedDay, EndedDay);
		return Rows;
	}

	for (const TPair<FName, int32>& Pair : Ledger->GetRevenueByFacility())
	{
		FFF_FacilityRevenueRow Row;
		Row.FacilityId = Pair.Key;
		Row.Amount = Pair.Value;
		Rows.Add(Row);
	}

	// TMap은 순회 순서를 의미 있는 출력 순서로 보장하지 않음.
	// 따라서 실행 환경이나 내부 상태에 따라 UI 표의 순서가 달라질 수 있음.
	// 이를 방지하기 위해 FacilityId의 "문자열 내용"을 기준으로 명시적으로 정렬함.
	// LexicalLess()는 FName 내부 인덱스가 아니라 이름 문자열의 사전식 순서를 비교함.
	Rows.Sort([](const FFF_FacilityRevenueRow& A, const FFF_FacilityRevenueRow& B)
	{
		return A.FacilityId.LexicalLess(B.FacilityId);
	});
	
	return Rows;
}

int32 UFF_FestivalProgressionSubsystem::GetTotalRefundForDay(int32 Day) const
{
	const FFF_DayLedger* Ledger = FindLedgerForDay(Day);
	if (Ledger == nullptr)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] Day %d 의 원장이 없다(진행 중 %d · 닫힌 날 %d). 0 을 돌려준다."),
			Day, LastProcessedDay, EndedDay);
		return 0;
	}

	return Ledger->GetTotalRefund();
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

TArray<FFF_ProgramCard> UFF_FestivalProgressionSubsystem::GetProgramCatalog() const
{
	return Definition != nullptr ? Definition->ProgramCatalog : TArray<FFF_ProgramCard>();
}

void UFF_FestivalProgressionSubsystem::SetProgramReservations(
	UFF_ProgramReservationSubsystem* InReservations)
{
	ProgramReservations = InReservations;
}

TArray<FName> UFF_FestivalProgressionSubsystem::GetScheduledProgramIds() const
{
	TArray<FName> Ids;
	Ids.Reserve(ScheduledPrograms.Num());
	for (const FFF_ScheduledProgram& Scheduled : ScheduledPrograms)
	{
		Ids.Add(Scheduled.ProgramId);
	}
	return Ids;
}

int32 UFF_FestivalProgressionSubsystem::GetStaffAssignedToPrograms() const
{
	if (Definition == nullptr) return 0;

	return FFF_ProgramScheduleRules::CalcStaffAssignedToPrograms(
		Definition->ProgramCatalog, GetScheduledProgramIds());
}

int32 UFF_FestivalProgressionSubsystem::GetAvailableStaffCount() const
{
	// 음수로 내려가지 않게 한다. 카탈로그가 바뀌어 요구 인원이 늘어난 저장 슬롯을
	// 불러오면 묶인 인원이 전체를 넘을 수 있다.
	return FMath::Max(0, StaffPool.GetTotalStaffCount() - GetStaffAssignedToPrograms());
}

EFF_ProgramPurchaseResult UFF_FestivalProgressionSubsystem::CanScheduleProgram(
	const FName ProgramId) const
{
	if (Definition == nullptr) return EFF_ProgramPurchaseResult::FestivalNotStarted;

	FFF_ProgramCard Unused;
	return FFF_ProgramScheduleRules::EvaluatePurchase(
		Definition->ProgramCatalog, ProgramId, GetScheduledProgramIds(),
		Funds.GetGold(), GetAvailableStaffCount(), Unused);
}

EFF_ProgramPurchaseResult UFF_FestivalProgressionSubsystem::TryScheduleProgram(
	const FName ProgramId, const FGuid FacilityInstanceId)
{
	if (Definition == nullptr) return EFF_ProgramPurchaseResult::FestivalNotStarted;

	FFF_ProgramCard Card;
	const EFF_ProgramPurchaseResult Result = FFF_ProgramScheduleRules::EvaluatePurchase(
		Definition->ProgramCatalog, ProgramId, GetScheduledProgramIds(),
		Funds.GetGold(), GetAvailableStaffCount(), Card);

	if (Result != EFF_ProgramPurchaseResult::Success)
	{
		UE_LOG(LogFestaFiestaEconomy, Log,
			TEXT("[경제] 프로그램 편성 거부: %s (요청 %s, 잔액 %d, 남은 인원 %d)"),
			*UEnum::GetValueAsString(Result), *ProgramId.ToString(),
			Funds.GetGold(), GetAvailableStaffCount());
		return Result;
	}

	if (!IsFacilityTypeAllowedForCard(Card, FacilityInstanceId))
	{
		UE_LOG(LogFestaFiestaEconomy, Log,
			TEXT("[경제] 프로그램 '%s' 편성 거부: 이 시설에서는 열 수 없다."),
			*ProgramId.ToString());
		return EFF_ProgramPurchaseResult::FacilityTypeMismatch;
	}

	// 슬롯을 못 만들면 차감도 하지 않는다. 순서를 뒤집으면 Gold 만 빠지고
	// 아무도 배정되지 않는 상태가 남는다.
	if (ProgramReservations == nullptr)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] 예약 서브시스템이 없어 프로그램 '%s' 을 편성할 수 없다."),
			*ProgramId.ToString());
		return EFF_ProgramPurchaseResult::UnknownProgram;
	}

	const FGuid ReservationId = ProgramReservations->RegisterProgram(
		FacilityInstanceId, Card.StartMinutes, Card.EndMinutes, Card.Capacity);

	if (!ReservationId.IsValid())
	{
		// 시설 핸들이 무효하거나 시각이 뒤집힌 경우다. 카드는 IsValid 로 걸렀으므로
		// 남는 원인은 시설 쪽이다.
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] 프로그램 '%s' 의 예약 슬롯을 만들지 못했다(시설 %s). 차감하지 않는다."),
			*ProgramId.ToString(), *FacilityInstanceId.ToString());
		return EFF_ProgramPurchaseResult::UnknownProgram;
	}

	const int32 PreviousGold = Funds.GetGold();

	// 판정이 통과했으므로 차감이 실패할 수 없다. (이상 상황)
	if (!ensureMsgf(TrySpend(Card.CostGold, EFF_SpendReason::ProgramPurchase),
		TEXT("프로그램 비용 %d 차감이 판정 통과 후 실패했다. 잔액 %d"),
		Card.CostGold, Funds.GetGold()))
	{
		// 슬롯을 되돌린다 — 돈을 안 받았는데 프로그램이 열려 있으면 안 된다.
		ProgramReservations->RemoveReservation(ReservationId);
		return EFF_ProgramPurchaseResult::InsufficientGold;
	}

	ScheduledPrograms.Add(FFF_ScheduledProgram{ ProgramId, ReservationId });

	UE_LOG(LogFestaFiestaEconomy, Log,
		TEXT("[경제] 프로그램 '%s' 편성. 비용 %d, 남은 Gold %d, 인원 %d명 묶임(남은 인원 %d), "
			 "정원 %d명·체험비 %d, %.0f~%.0f분"),
		*ProgramId.ToString(), Card.CostGold, Funds.GetGold(),
		Card.RequiredStaff, GetAvailableStaffCount(), Card.Capacity, Card.FeePerPerson,
		Card.StartMinutes, Card.EndMinutes);

	// TrySpend 가 Gold 를 이미 쏘지만, 편성 목록·인원은 여기서만 바뀐다.
	BroadcastGoldIfChanged(PreviousGold);
	OnScheduledProgramsChanged.Broadcast();

	return EFF_ProgramPurchaseResult::Success;
}

int32 UFF_FestivalProgressionSubsystem::SettleOneProgram(
	FFF_ScheduledProgram& Scheduled, const bool bLateSettlement)
{
	if (Scheduled.bFeeSettled) return 0;

	const FFF_ProgramCard* Card = FFF_ProgramScheduleRules::FindCard(
		Definition->ProgramCatalog, Scheduled.ProgramId);
	if (Card == nullptr) return 0;

	// 예약 그룹은 스폰될 때 이미 확정돼 있기 때문에 여기서 배정하지 않는다.
	// (`ConfirmReservedParty`). 정산에서 다시 배정하면 그 확정을 덮어쓴다.
	FFF_ProgramReservation Reservation;
	if (!ProgramReservations->FindReservation(Scheduled.ReservationId, Reservation))
	{
		return 0;
	}

	const int32 Fee = FFF_ProgramScheduleRules::CalcProgramRevenue(
		*Card, Reservation.AssignedHeadcount);

	// 여기까지 왔으면 이 카드는 오늘 처리가 끝났다. 금액이 0 이어도 다시 걷지 않는다 —
	// 안 그러면 아무도 안 온 프로그램을 Closing 안전망이 또 집어 Warning 이 두 번 난다.
	Scheduled.bFeeSettled = true;

	if (bLateSettlement)
	{
		// 시작 시각 정산이 놓친 카드다. 카드 시각이 그날 Live 길이를 넘었거나 Live 를 건너뛴
		// 경우라 저작·단계 전환 쪽 결함이다.
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] 프로그램 '%s' 이 시작 시각(%.0f분)에 정산되지 않아 마감에 걷었다. "
				 "체험비 %d — 카드 시각이 오늘 Live 길이를 넘었는지 확인해야 한다"),
			*Scheduled.ProgramId.ToString(), Card->StartMinutes, Fee);
	}

	if (Reservation.UnfilledSeats > 0)
	{
		// 산 정원만큼 스폰되므로 여기 걸리면 스폰 배선이 깨진 것이다.
		// 정상 상황이 아니라 결함이라 Warning 으로 올린다.
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] 프로그램 '%s' 정원 미달: %d/%d명 (빈자리 %d). 체험비 %d — "
				 "산 정원만큼 스폰됐어야 한다"),
			*Scheduled.ProgramId.ToString(), Reservation.AssignedHeadcount,
			Reservation.Capacity, Reservation.UnfilledSeats, Fee);
	}

	if (Fee > 0)
	{
		AddRevenue(Fee, EFF_EarnReason::ProgramFee, Scheduled.ProgramId);
	}

	return Fee;
}

int32 UFF_FestivalProgressionSubsystem::SettleStartedPrograms(const float NowMinutes)
{
	if (Definition == nullptr || ProgramReservations == nullptr) return 0;
	if (ScheduledPrograms.Num() == 0) return 0;

	const int32 PreviousGold = Funds.GetGold();
	int32 TotalFee = 0;
	int32 SettledCount = 0;

	for (FFF_ScheduledProgram& Scheduled : ScheduledPrograms)
	{
		if (Scheduled.bFeeSettled) continue;

		const FFF_ProgramCard* Card = FFF_ProgramScheduleRules::FindCard(
			Definition->ProgramCatalog, Scheduled.ProgramId);
		if (Card == nullptr || NowMinutes < Card->StartMinutes) continue;

		TotalFee += SettleOneProgram(Scheduled, /*bLateSettlement=*/false);
		++SettledCount;
	}

	if (SettledCount > 0)
	{
		UE_LOG(LogFestaFiestaEconomy, Log,
			TEXT("[경제] %.0f분에 프로그램 %d개가 열렸다. 체험비 합계 %d, 잔액 %d"),
			NowMinutes, SettledCount, TotalFee, Funds.GetGold());
	}

	BroadcastGoldIfChanged(PreviousGold);
	return TotalFee;
}

int32 UFF_FestivalProgressionSubsystem::SettleScheduledPrograms()
{
	if (Definition == nullptr || ProgramReservations == nullptr) return 0;
	if (ScheduledPrograms.Num() == 0) return 0;

	const int32 PreviousGold = Funds.GetGold();
	int32 TotalFee = 0;
	int32 SettledCount = 0;

	for (FFF_ScheduledProgram& Scheduled : ScheduledPrograms)
	{
		if (Scheduled.bFeeSettled) continue;

		TotalFee += SettleOneProgram(Scheduled, /*bLateSettlement=*/true);
		++SettledCount;
	}

	if (SettledCount > 0)
	{
		UE_LOG(LogFestaFiestaEconomy, Log,
			TEXT("[경제] 마감에 프로그램 %d개를 뒤늦게 정산했다. 체험비 합계 %d, 잔액 %d"),
			SettledCount, TotalFee, Funds.GetGold());
	}

	BroadcastGoldIfChanged(PreviousGold);
	return TotalFee;
}

bool UFF_FestivalProgressionSubsystem::IsFacilityTypeAllowedForCard(
	const FFF_ProgramCard& Card, const FGuid& FacilityInstanceId) const
{
	if (Card.AllowedFacilityTypes.Num() == 0)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] 프로그램 '%s' 에 허용 시설이 저작되지 않았다. 아무 시설에서나 열린다."),
			*Card.ProgramId.ToString());
		return true;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	UFFZSWorldSubsystem* Zone = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;

	// ZoneSystem 자체가 없는 것은 시설 검증을 할 수 없는 환경이라는 뜻이다(월드 없는 테스트).
	// 시설 핸들이 유효한데 못 찾는 것과는 다른 상황이라 구분해서 다룬다.
	if (Zone == nullptr)
	{
		return true;
	}

	const AFFZSFacilityActor* Facility = Zone->FindFacility(FacilityInstanceId);
	if (Facility == nullptr)
	{
		return false;
	}

	const FFFZSFacilityReadModel ReadModel = Facility->GetReadModel();

	FFF_FacilityNameParts Parts;
	FString FailureReason;
	if (!UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
		ReadModel.Category, ReadModel.FacilityDefinitionId, Parts, FailureReason))
	{
		// 이름 규칙을 어긴 시설이다. 종류를 알 수 없으므로 열어주지 않는다.
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] 시설 '%s' 의 이름을 해석하지 못했다: %s"),
			*ReadModel.FacilityDefinitionId.ToString(), *FailureReason);
		return false;
	}

	return FFF_ProgramScheduleRules::IsFacilityTypeAllowed(
		Card, Parts.MajorToken, Parts.MinorToken);
}

TArray<FFF_ReservedPartyRequest>
UFF_FestivalProgressionSubsystem::GetReservedPartyRequests() const
{
	TArray<FFF_ReservedPartyRequest> Requests;
	if (Definition == nullptr) return Requests;

	for (const FFF_ScheduledProgram& Scheduled : ScheduledPrograms)
	{
		const FFF_ProgramCard* Card = FFF_ProgramScheduleRules::FindCard(
			Definition->ProgramCatalog, Scheduled.ProgramId);
		if (Card == nullptr) continue;

		Requests.Append(FFF_ProgramScheduleRules::BuildReservedPartyRequests(
			*Card, Scheduled.ReservationId));
	}

	return Requests;
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

	// 덮기 전에 읽는다 — 바로 아래에서 LastProcessedDay 가 NewDay 로 바뀐다.
	const int32 JustEndedDay = LastProcessedDay;

	LastProcessedDay = NewDay;

	// TodayRevenue & DayLedger 리셋
	Funds.ResetTodayRevenue();

	// 리셋 직전에 직전 하루를 보관한다.
	EndedDayLedger = DayLedger;
	EndedDay = JustEndedDay;

	DayLedger.Reset();
	
	// 알바는 일당 계약이라 매일 0 으로 돌아간다. 플레이어가 오늘 다시 정한다.
	StaffPool.SetTemporaryStaffCount(0);

	// 프로그램 카드도 하루짜리다. 비우지 않으면 어제 편성이 오늘 인원을 계속 묶는다.
	// 슬롯까지 같이 지운다 — 어제 예약이 남아 있으면 오늘 스폰된 파티가 어제 시각에
	// 잠기거나 퇴장을 막힌다.
	const bool bHadPrograms = ScheduledPrograms.Num() > 0;
	if (ProgramReservations != nullptr)
	{
		for (const FFF_ScheduledProgram& Scheduled : ScheduledPrograms)
		{
			ProgramReservations->RemoveReservation(Scheduled.ReservationId);
		}
	}
	ScheduledPrograms.Reset();

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

	if (bHadPrograms)
	{
		OnScheduledProgramsChanged.Broadcast();
	}

	return true;
}

const FFF_DayLedger* UFF_FestivalProgressionSubsystem::FindLedgerForDay(int32 Day) const
{
	if (Day <= 0) return nullptr;
	
	if (Day == LastProcessedDay) return &DayLedger;
	
	if (Day == EndedDay) return &EndedDayLedger;
	
	return nullptr;
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