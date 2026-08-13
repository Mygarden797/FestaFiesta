// Copyright Epic Games, Inc. All Rights Reserved.

#include "Festival/FF_FestivalProgressionSubsystem.h"

#include "Economy/FF_EconomyLog.h"
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

	Definition = InDefinition;
	Funds.Initialize(InDefinition->InitialGold);
	DayLedger.Reset();

	UE_LOG(LogFestaFiestaEconomy, Log, TEXT("[경제] 축제 시작: %s, 초기 Gold %d"),
		*InDefinition->FestivalId.ToString(), Funds.GetGold());

	BroadcastGoldIfChanged(PreviousGold);
	BroadcastTotalRevenueIfChanged(PreviousTotalRevenue);
	BroadcastTodayRevenueIfChanged(PreviousTodayRevenue);
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

	Funds.AddRevenue(Amount);
	DayLedger.RecordEarn(Reason, Amount, FacilityId);

	BroadcastGoldIfChanged(PreviousGold);
	BroadcastTotalRevenueIfChanged(PreviousTotalRevenue);
	BroadcastTodayRevenueIfChanged(PreviousTodayRevenue);
}

void UFF_FestivalProgressionSubsystem::Refund(int32 Amount)
{
	const int32 PreviousGold = Funds.GetGold();
	
	Funds.Refund(Amount);
	DayLedger.RecordRefund(Amount);
	
	BroadcastGoldIfChanged(PreviousGold);
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
