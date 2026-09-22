// Copyright Epic Games, Inc. All Rights Reserved.

#include "Festival/FF_FestivalProgressionSubsystem.h"

#include "Save/FF_SaveArchive.h"
#include "Save/FF_SaveSubsystem.h"

#include "Economy/FF_EconomyLog.h"
#include "Economy/FF_EconomyDisplayTextLibrary.h"
#include "Economy/Rules/FF_BoothRevenueRules.h"
#include "Economy/Rules/FF_FinaleRules.h"
#include "Economy/Rules/FF_ProgramScheduleRules.h"
#include "Economy/Rules/FF_PromotionRules.h"
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

	/* 저장에 참여한다.

	   여기서 등록하는 이유는 저장 쪽이 목록을 들고 있기 때문이다.
	   언제 저장되는지, 파일이 어디 쓰이는지는 이 클래스가 몰라도 된다.
	*/
	if (UFF_SaveSubsystem* Save = GetGameInstance()->GetSubsystem<UFF_SaveSubsystem>())
	{
		Save->RegisterSaveable(this);
	}
}

void UFF_FestivalProgressionSubsystem::WriteSaveData(TArray<uint8>& OutBytes) const
{
	FFF_ProgressionSave State;
	State.Gold = Funds.GetGold();
	State.TotalRevenue = Funds.GetTotalRevenue();
	State.LastProcessedDay = LastProcessedDay;
	State.EndedDay = EndedDay;

	/* 장부와 편성은 담지 않는다.

	   저장은 정산이 끝난 뒤에만 하므로 그때 장부는 비어 있고 편성은 해제돼 있다.
	   비어 있는 것을 담으면 파일만 커지고 복원할 때 맞출 것이 늘어난다.

	   알바도 마찬가지다. 매일 다시 뽑는 것이라 하루가 시작되는 시점에는 0 이다.
	*/

	UFF_SaveArchive::WriteStruct(OutBytes, State);
}

void UFF_FestivalProgressionSubsystem::ReadSaveData(const TArray<uint8>& InBytes)
{
	FFF_ProgressionSave State;
	if (!UFF_SaveArchive::ReadStruct(InBytes, State))
	{
		// 저장에 경제가 없다. 첫 축제이거나 옛 저장이다. 기본 상태로 둔다.
		return;
	}

	const int32 PreviousGold = Funds.GetGold();
	const int32 PreviousTotalRevenue = Funds.GetTotalRevenue();

	/* 값을 직접 되돌린다.

	   AddRevenue 로 맞추면 장부에 없던 수입이 기록돼 결산이 틀어진다.
	   복원은 "벌었다" 가 아니라 "그때 그랬다" 이므로 기록을 남기면 안 된다.
	*/
	Funds.RestoreFromSave(State.Gold, State.TotalRevenue);

	LastProcessedDay = State.LastProcessedDay;
	EndedDay = State.EndedDay;

	// 화면이 따라오게 알린다. 값만 바꾸면 UI 가 옛 숫자를 계속 보여준다.
	BroadcastGoldIfChanged(PreviousGold);
	BroadcastTotalRevenueIfChanged(PreviousTotalRevenue);

	UE_LOG(LogFestaFiestaEconomy, Log,
		TEXT("[경제] 저장에서 되돌렸다. 금화 %d / 누적 %d / %d일차"),
		State.Gold, State.TotalRevenue, State.LastProcessedDay);
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
	// A policy callback cannot restart the economy in the middle of an atomic facility purchase.
	if (PendingFacilityUpgradeCommand.IsValid())
	{
		return;
	}
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
	const int32 PreviousTotalStaffCount = StaffPool.GetTotalStaffCount();
	// 재시작 시 등급이 지워지는데 이 셋을 안 알리면 UI 캐시가 "이미 구매함"으로 남는다
	const int32 PreviousExpectedVisitors = GetExpectedVisitors();
	const EFF_PromotionGrade PreviousPromotionGrade = PurchasedPromotionGrade;
	const EFF_FinaleGrade PreviousFinaleGrade = PurchasedFinaleGrade;

	Definition = InDefinition;
	Funds.Initialize(InDefinition->InitialGold);
	DayLedger.Reset();
	// 시설별 장부는 축제 단위 누적이라 하루 경계가 아니라 여기서만 비운다.
	FacilityRevenueBook.Reset();
	// 새 축제에 이전 판 스냅샷이 남지 않게 한다.
	// [주의] 지금은 아래 TryProcessNewDay(1) 이 어차피 덮으므로 이 두 줄이 없어도 동작이 같다
	//        (변이 검사로 확인: 지우면 죽는 테스트가 없다). 호출 순서가 바뀔 때를 위한 방어다.
	EndedDayLedger.Reset();
	EndedDay = 0;
	StaffPool.Initialize(InDefinition->InitialStaffCount);
	PurchasedFinaleGrade = EFF_FinaleGrade::None;
	PurchasedPromotionGrade = EFF_PromotionGrade::None;
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
	BroadcastTotalStaffCountIfChanged(PreviousTotalStaffCount);
	if (PurchasedPromotionGrade != PreviousPromotionGrade)
	{
		OnPromotionGradeChanged.Broadcast(PurchasedPromotionGrade);
	}
	if (PurchasedFinaleGrade != PreviousFinaleGrade)
	{
		OnFinaleGradeChanged.Broadcast(PurchasedFinaleGrade);
	}
	BroadcastExpectedVisitorsIfChanged(PreviousExpectedVisitors);
	OnFestivalDefinitionChanged.Broadcast();
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

bool UFF_FestivalProgressionSubsystem::ReserveFacilityUpgradeSpend(const FGuid CommandId, const int32 Amount)
{
	if (!IsInGameThread() || !IsFestivalStarted() || !CommandId.IsValid()
		|| PendingFacilityUpgradeCommand.IsValid() || Amount < 0
		|| DayLedger.GetSpendByReason(EFF_SpendReason::FacilityUpgrade) > MAX_int32 - Amount)
	{
		return false;
	}
	const int32 PreviousGold = Funds.GetGold();
	if (!Funds.TrySpend(Amount))
	{
		return false;
	}
	PendingFacilityUpgradeCommand = CommandId;
	PendingFacilityUpgradeCost = Amount;
	GoldBeforeFacilityUpgrade = PreviousGold;
	bFacilityUpgradeSpendCommitted = false;
	return true;
}

void UFF_FestivalProgressionSubsystem::CommitFacilityUpgradeSpend(const FGuid CommandId)
{
	if (!ensure(PendingFacilityUpgradeCommand == CommandId) || bFacilityUpgradeSpendCommitted)
	{
		return;
	}
	// Free authored tiers still reserve a command, but do not create a fictitious expense.
	if (PendingFacilityUpgradeCost > 0)
	{
		DayLedger.RecordSpend(EFF_SpendReason::FacilityUpgrade, PendingFacilityUpgradeCost);
	}
	bFacilityUpgradeSpendCommitted = true;
}

void UFF_FestivalProgressionSubsystem::RollbackFacilityUpgradeSpend(const FGuid CommandId)
{
	if (PendingFacilityUpgradeCommand != CommandId || bFacilityUpgradeSpendCommitted)
	{
		return;
	}
	// Release a reservation, not a player refund: do not alter refund/revenue ledgers or broadcast.
	Funds.Refund(PendingFacilityUpgradeCost);
	PendingFacilityUpgradeCommand.Invalidate();
	PendingFacilityUpgradeCost = 0;
	GoldBeforeFacilityUpgrade = 0;
}

void UFF_FestivalProgressionSubsystem::PublishFacilityUpgradeSpend(const FGuid CommandId)
{
	if (PendingFacilityUpgradeCommand != CommandId || !bFacilityUpgradeSpendCommitted)
	{
		return;
	}
	const int32 PreviousGold = GoldBeforeFacilityUpgrade;
	PendingFacilityUpgradeCommand.Invalidate();
	PendingFacilityUpgradeCost = 0;
	GoldBeforeFacilityUpgrade = 0;
	bFacilityUpgradeSpendCommitted = false;
	BroadcastGoldIfChanged(PreviousGold);
}

void UFF_FestivalProgressionSubsystem::AddRevenue(int32 Amount, EFF_EarnReason Reason, FName FacilityId)
{
	RecordRevenue(Amount, Reason, FacilityId, FGuid());
}

void UFF_FestivalProgressionSubsystem::AddFacilityRevenue(
	int32 Amount, EFF_EarnReason Reason, FName FacilityId, FGuid FacilityInstanceId)
{
	RecordRevenue(Amount, Reason, FacilityId, FacilityInstanceId);
}

bool UFF_FestivalProgressionSubsystem::GetFacilityInstanceRevenue(
	FGuid FacilityInstanceId, FFF_FacilityInstanceRevenue& OutRevenue) const
{
	return FacilityRevenueBook.Find(FacilityInstanceId, OutRevenue);
}

void UFF_FestivalProgressionSubsystem::RecordRevenue(
	int32 Amount, EFF_EarnReason Reason, FName FacilityId, const FGuid& FacilityInstanceId)
{
	if (Amount > 0)
	{
		// 잔액과 누적 매출 중 먼저 상한에 닿는 값에 맞춰 동일한 수용액을 원장에도 기록한다.
		const int32 Capacity = MAX_int32 - FMath::Max3(Funds.GetGold(), Funds.GetTotalRevenue(), Funds.GetTodayRevenue());
		Amount = FMath::Min(Amount, Capacity);
		if (Amount == 0) return;
	}
	const int32 PreviousGold = Funds.GetGold();
	const int32 PreviousTotalRevenue = Funds.GetTotalRevenue();
	const int32 PreviousTodayRevenue = Funds.GetTodayRevenue();

	Funds.AddRevenue(Amount);
	DayLedger.RecordEarn(Reason, Amount, FacilityId);
	// 시설 핸들이 없는 매출(입장료·보너스)은 시설별 장부에 들어가지 않는다. 같은 금액을 같은 호출에서
	// 세 장부에 넣으므로 시설 정보 패널의 값과 결산 표의 합이 어긋날 수 없다.
	const bool bFacilityRevenueChanged = FacilityRevenueBook.Record(FacilityInstanceId, Amount);

	BroadcastGoldIfChanged(PreviousGold);
	BroadcastTotalRevenueIfChanged(PreviousTotalRevenue);
	BroadcastTodayRevenueIfChanged(PreviousTodayRevenue);
	if (bFacilityRevenueChanged)
	{
		OnFacilityRevenueChanged.Broadcast(FacilityInstanceId);
	}
}

void UFF_FestivalProgressionSubsystem::GrantDiscoveryGold(int32 Amount)
{
	if (Amount <= 0 || !IsFestivalStarted()) { return; }
	const int32 PreviousGold = Funds.GetGold();
	Funds.Refund(Amount);
	BroadcastGoldIfChanged(PreviousGold);
}

void UFF_FestivalProgressionSubsystem::Refund(int32 Amount)
{
	const int32 PreviousGold = Funds.GetGold();
	
	Funds.Refund(Amount);
	DayLedger.RecordRefund(Amount);

	BroadcastGoldIfChanged(PreviousGold);
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
	
	for (int64 Value = 0; Value <= static_cast<int64>(EFF_SpendReason::FacilityUpgrade); ++Value)
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
	const int32 PreviousTotalStaffCount = StaffPool.GetTotalStaffCount();
	
	// 실제로 정산했으면 true, 같은 날 중복 호출이라 건너뛰었으면 false.
	if (!TryProcessNewDay(NewDay)) return;
	
	BroadcastGoldIfChanged(PreviousGold);
	BroadcastTodayRevenueIfChanged(PreviousTodayRevenue);
	BroadcastTotalStaffCountIfChanged(PreviousTotalStaffCount);
}

bool UFF_FestivalProgressionSubsystem::TryHireStaff(int32 RequestedCount)
{
	if (Definition == nullptr || RequestedCount <= 0) return false;

	const int32 AlreadyHired = StaffPool.GetHiredStaffCount();
	const int32 RemainingCap = GetRemainingHireCap();

	if (RequestedCount > RemainingCap)
	{
		UE_LOG(LogFestaFiestaEconomy, Log,
			TEXT("[경제] 고용 거부: 총원 상한 초과 (고용 %d명 + 요청 %d명 > 상한 %d명)"),
			AlreadyHired, RequestedCount, Definition->MaxHiredStaffCount);
		return false;
	}

	const int32 Cost = FFF_StaffPool::CalcHireCost(
		RequestedCount, Definition->StaffHireCostGold);

	const int32 PreviousGold = Funds.GetGold();
	const int32 PreviousTotalStaffCount = StaffPool.GetTotalStaffCount();

	// 고용은 플레이어가 고르는 지출이라 잔액 안에서만 낸다. 모자라면 거부하고 상태를 안 바꾼다.
	if (!Funds.TrySpend(Cost))
	{
		UE_LOG(LogFestaFiestaEconomy, Log,
			TEXT("[경제] 고용 거부: 잔액 부족 (고용비 %d, 잔액 %d)"), Cost, Funds.GetGold());
		return false;
	}

	StaffPool.SetHiredStaffCount(AlreadyHired + RequestedCount);
	DayLedger.RecordSpend(EFF_SpendReason::StaffHire, Cost);

	UE_LOG(LogFestaFiestaEconomy, Log,
		TEXT("[경제] Staff %d명 고용. 비용 %d, 총원 %d명(기본 %d + 고용 %d). 축제 끝까지 남는다"),
		RequestedCount, Cost, StaffPool.GetTotalStaffCount(),
		StaffPool.GetBaseStaffCount(), StaffPool.GetHiredStaffCount());

	BroadcastGoldIfChanged(PreviousGold);
	BroadcastTotalStaffCountIfChanged(PreviousTotalStaffCount);
	return true;
}

int32 UFF_FestivalProgressionSubsystem::GetRemainingHireCap() const
{
	if (Definition == nullptr) return 0;

	// 축제 전체 상한이다. 하루 경계에서 다시 열리지 않는다.
	return FMath::Max(0,
		Definition->MaxHiredStaffCount - StaffPool.GetHiredStaffCount());
}

int32 UFF_FestivalProgressionSubsystem::GetAffordableHireCount() const
{
	if (Definition == nullptr) return 0;

	return StaffPool.CalcAffordableHireCount(
		Definition->StaffHireCostGold, Funds.GetGold(), GetRemainingHireCap());
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

EFF_PromotionPurchaseResult UFF_FestivalProgressionSubsystem::TryPurchasePromotion(
	const EFF_PromotionGrade Grade)
{
	if (Definition == nullptr) return EFF_PromotionPurchaseResult::FestivalNotStarted;

	FFF_PromotionTier Tier;
	const EFF_PromotionPurchaseResult Result = FFF_PromotionRules::EvaluatePurchase(
		Grade, PurchasedPromotionGrade, Funds.GetGold(),
		Definition->PromotionBasic, Definition->PromotionFocused, Tier);

	if (Result != EFF_PromotionPurchaseResult::Success)
	{
		UE_LOG(LogFestaFiestaEconomy, Log, TEXT("[경제] 홍보 구매 거부: %s (요청 등급 %s, 잔액 %d)"),
			*UEnum::GetValueAsString(Result), *UEnum::GetValueAsString(Grade), Funds.GetGold());
		return Result;
	}

	const int32 PreviousExpectedVisitors = GetExpectedVisitors();

	// 판정이 통과했으므로 차감이 실패할 수 없다. (이상 상황)
	if (!ensureMsgf(TrySpend(Tier.CostGold, EFF_SpendReason::Promotion),
		TEXT("홍보 비용 %d 차감이 판정 통과 후 실패했다. 잔액 %d"), Tier.CostGold, Funds.GetGold()))
	{
		return EFF_PromotionPurchaseResult::InsufficientGold;
	}

	PurchasedPromotionGrade = Grade;

	UE_LOG(LogFestaFiestaEconomy, Log,
		TEXT("[경제] 홍보 %s 구매. 비용 %d, 남은 Gold %d, 예상 방문객 %d -> %d"),
		*UEnum::GetValueAsString(Grade), Tier.CostGold, Funds.GetGold(),
		PreviousExpectedVisitors, GetExpectedVisitors());

	OnPromotionGradeChanged.Broadcast(PurchasedPromotionGrade);
	BroadcastExpectedVisitorsIfChanged(PreviousExpectedVisitors);
	return EFF_PromotionPurchaseResult::Success;
}

int32 UFF_FestivalProgressionSubsystem::GetExpectedVisitors() const
{
	// 축제 전에는 0 이 아니라 "모름"이다. 0 을 돌려주면 아무도 안 온다.
	if (Definition == nullptr)
	{
		return INDEX_NONE;
	}

	const FFF_PromotionTier* Tier = FFF_PromotionRules::FindTier(
		PurchasedPromotionGrade, Definition->PromotionBasic, Definition->PromotionFocused);

	// 미구매면 배수 1.0 이다 — 티어가 없는 것이 곧 "홍보 안 함"이다.
	const float Multiplier = Tier != nullptr ? Tier->VisitorMultiplier : 1.f;

	return FFF_PromotionRules::CalcExpectedVisitors(Definition->BaseDailyVisitors, Multiplier);
}

void UFF_FestivalProgressionSubsystem::BroadcastExpectedVisitorsIfChanged(
	const int32 PreviousExpectedVisitors)
{
	const int32 Current = GetExpectedVisitors();
	if (Current != PreviousExpectedVisitors)
	{
		OnExpectedVisitorsChanged.Broadcast(Current);
	}
}

TArray<FFF_ProgramCard> UFF_FestivalProgressionSubsystem::GetProgramCatalog() const
{
	return Definition != nullptr ? Definition->ProgramCatalog : TArray<FFF_ProgramCard>();
}

FName UFF_FestivalProgressionSubsystem::ResolveFacilityDefinitionId(const FGuid FacilityInstanceId) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const UFFZSWorldSubsystem* Zone = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	const AFFZSFacilityActor* Facility = Zone ? Zone->FindFacility(FacilityInstanceId) : nullptr;
	return Facility ? Facility->GetFacilitySpec().FacilityDefinitionId : NAME_None;
}

TArray<FFF_ProgramCard> UFF_FestivalProgressionSubsystem::GetProgramCatalogForFacility(
	const FGuid FacilityInstanceId) const
{
	TArray<FFF_ProgramCard> Catalog = GetProgramCatalog();
	if (!Definition) return Catalog;
	const FName FacilityDefinitionId = ResolveFacilityDefinitionId(FacilityInstanceId);
	const UDataTable* RevenueTable = Definition->GetFacilityRevenueTable();
	for (FFF_ProgramCard& Card : Catalog)
	{
		Card.FeePerPerson = FFF_BoothRevenueRules::ResolveProgramFeePerPerson(
			FacilityDefinitionId, Card.ProgramId, Card.FeePerPerson, RevenueTable);
	}
	return Catalog;
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

EFF_ProgramPurchaseResult UFF_FestivalProgressionSubsystem::CanScheduleProgram(
	const FName ProgramId) const
{
	if (Definition == nullptr) return EFF_ProgramPurchaseResult::FestivalNotStarted;

	FFF_ProgramCard Unused;
	return FFF_ProgramScheduleRules::EvaluatePurchase(
		Definition->ProgramCatalog, ProgramId, GetScheduledProgramIds(),
		Funds.GetGold(), Unused);
}

EFF_ProgramPurchaseResult UFF_FestivalProgressionSubsystem::TryScheduleProgram(
	const FName ProgramId, const FGuid FacilityInstanceId)
{
	if (Definition == nullptr) return EFF_ProgramPurchaseResult::FestivalNotStarted;

	FFF_ProgramCard Card;
	const TArray<FFF_ProgramCard> Catalog = GetProgramCatalogForFacility(FacilityInstanceId);
	const EFF_ProgramPurchaseResult Result = FFF_ProgramScheduleRules::EvaluatePurchase(

		Definition->ProgramCatalog, ProgramId, GetScheduledProgramIds(),
		Funds.GetGold(), Card);


	if (Result != EFF_ProgramPurchaseResult::Success)
	{
		UE_LOG(LogFestaFiestaEconomy, Log,
			TEXT("[경제] 프로그램 편성 거부: %s (요청 %s, 잔액 %d)"),
			*UEnum::GetValueAsString(Result), *ProgramId.ToString(),
			Funds.GetGold());
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

	// 이후 비용 차감 알림에서 월드가 바뀌더라도 편성 장소의 수익 행은 유지한다.
	const FName FacilityDefinitionId = ResolveFacilityDefinitionId(FacilityInstanceId);
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

	FFF_ScheduledProgram Scheduled{ ProgramId, ReservationId };
	Scheduled.FacilityDefinitionId = FacilityDefinitionId;
	ScheduledPrograms.Add(Scheduled);

	UE_LOG(LogFestaFiestaEconomy, Log,
		TEXT("[경제] 프로그램 '%s' 편성. 비용 %d, 남은 Gold %d, "
			 "정원 %d명·체험비 %d, %.0f~%.0f분"),
		*ProgramId.ToString(), Card.CostGold, Funds.GetGold(),
		Card.Capacity, Card.FeePerPerson,
		Card.StartMinutes, Card.EndMinutes);

	// TrySpend 가 Gold 를 이미 쏘지만, 편성 목록은 여기서만 바뀐다.
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

	if (Scheduled.FacilityDefinitionId.IsNone())
	{
		Scheduled.FacilityDefinitionId = ResolveFacilityDefinitionId(Reservation.FacilityInstanceId);
	}
	FFF_ProgramCard RevenueCard = *Card;
	RevenueCard.FeePerPerson = FFF_BoothRevenueRules::ResolveProgramFeePerPerson(
		Scheduled.FacilityDefinitionId, Scheduled.ProgramId, Card->FeePerPerson,
		Definition->GetFacilityRevenueTable());
	const int32 Fee = FFF_ProgramScheduleRules::CalcProgramRevenue(
		RevenueCard, Reservation.AssignedHeadcount);

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
		// 체험비는 프로그램을 연 설치 시설에도 귀속된다 — 시설 정보 패널이 "이 시설이 번 돈"으로 보여 준다.
		const int32 PreviousRevenue = Funds.GetTotalRevenue();
		AddFacilityRevenue(Fee, EFF_EarnReason::ProgramFee, Scheduled.ProgramId, Reservation.FacilityInstanceId);
		// 잔액 상한이나 미납 상환이 있어도 원장에 수용된 매출만 반환한다.
		return Funds.GetTotalRevenue() - PreviousRevenue;
	}

	return 0;
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

		TotalFee = static_cast<int32>(FMath::Min<int64>(MAX_int32,
			static_cast<int64>(TotalFee) + SettleOneProgram(Scheduled, /*bLateSettlement=*/false)));
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

		TotalFee = static_cast<int32>(FMath::Min<int64>(MAX_int32,
			static_cast<int64>(TotalFee) + SettleOneProgram(Scheduled, /*bLateSettlement=*/true)));
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
	// 시설별 장부는 오늘 값만 0 으로 돌아가고 누적은 남는다 (시설 정보 패널의 "누적 매출").
	FacilityRevenueBook.BeginNewDay();
	
	// 프로그램 카드도 하루짜리다. 비우지 않으면 어제 편성이 오늘 "이미 편성함" 으로 막는다.
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

	UE_LOG(LogFestaFiestaEconomy, Log,
		TEXT("[경제] Day %d 시작. Gold %d, Staff %d명(기본 %d + 고용 %d)"),
		NewDay, Funds.GetGold(),
		StaffPool.GetTotalStaffCount(), StaffPool.GetBaseStaffCount(),
		StaffPool.GetHiredStaffCount());

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

