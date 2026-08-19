// Fill out your copyright notice in the Description page of Project Settings.


#include "Economy/FF_StageEconomyBridgeSubsystem.h"

#include "Economy/FF_EconomyLog.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"

void UFF_StageEconomyBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// InitializeDependency : 해당 서브시스템의 초기화를 보장한 뒤 포인터를 반환
	Stage = Collection.InitializeDependency<UFF_FestivalStageSubsystem>();
	SetProgression(Collection.InitializeDependency<UFF_FestivalProgressionSubsystem>());
	
	if (Stage == nullptr)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning, TEXT("[경제] Stage 서브시스템이 없다. Day2·3 하루 경계 정산이 걸리지 않는다."));
		return;
	}
	
	Stage->OnStageChanged.AddDynamic(this, &UFF_StageEconomyBridgeSubsystem::HandleStageChanged);
}

void UFF_StageEconomyBridgeSubsystem::Deinitialize()
{
	if (Stage != nullptr)
	{
		Stage->OnStageChanged.RemoveDynamic(this, &UFF_StageEconomyBridgeSubsystem::HandleStageChanged);
		Stage = nullptr;
	}
	
	Progression = nullptr;
	
	Super::Deinitialize();
}

void UFF_StageEconomyBridgeSubsystem::ProcessStageEntry(EFF_FestivalStage NewStage, int32 Day)
{
	if (!IsDayBoundaryStage(NewStage)) return;
	
	if (Progression == nullptr) return;
	
	// 중복 정산은 Progression의 LastProcessedDay에서 단일하게 방어
	Progression->BeginNewDay(Day);
}

void UFF_StageEconomyBridgeSubsystem::SetProgression(UFF_FestivalProgressionSubsystem* InProgression)
{
	Progression = InProgression;
	
	if (InProgression == nullptr)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] 진행 서브시스템이 없다. 하루 경계에서 정산할 대상이 없다."));
	}
}

bool UFF_StageEconomyBridgeSubsystem::CanPurchaseFinaleNow() const
{
	if (Stage == nullptr || Progression == nullptr) return false;
	
	if (Progression->GetPurchasedFinaleGrade() != EFF_FinaleGrade::None) return false;
	
	return Stage->IsFinalDay() && Stage->CurrentStage == EFF_FestivalStage::MorningCheck;
}

void UFF_StageEconomyBridgeSubsystem::HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage)
{
	ProcessStageEntry(NewStage, Stage != nullptr ? Stage->CurrentDay : 0);
}
