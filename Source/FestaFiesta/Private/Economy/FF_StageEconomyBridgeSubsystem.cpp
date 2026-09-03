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
	Stage->OnFestivalStarted.AddDynamic(this, &UFF_StageEconomyBridgeSubsystem::HandleFestivalStarted);
}

void UFF_StageEconomyBridgeSubsystem::Deinitialize()
{
	if (Stage != nullptr)
	{
		Stage->OnStageChanged.RemoveDynamic(this, &UFF_StageEconomyBridgeSubsystem::HandleStageChanged);
		Stage->OnFestivalStarted.RemoveDynamic(this, &UFF_StageEconomyBridgeSubsystem::HandleFestivalStarted);
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

void UFF_StageEconomyBridgeSubsystem::ProcessFestivalStarted()
{
	if (Progression == nullptr) return;

	// `StartFestival` 은 완전한 리셋 함수라 두 번 돌아도 같은 상태로 끝나므로 
	// `Initialize` 호출과 겹치는 것을 따로 막지 않는다.
	Progression->RestartFestival();
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

bool UFF_StageEconomyBridgeSubsystem::CanScheduleProgramNow() const
{
	if (Stage == nullptr) return false;

	// 남은 카드가 있는지는 보지 않는다. 그것은 CanScheduleProgram 이 카드별로 답하는 질문이고,
	// 여기는 "지금이 그 화면을 띄울 단계인가"만 답한다.
	return IsProgramSchedulingStage(Stage->CurrentStage);
}

void UFF_StageEconomyBridgeSubsystem::HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage)
{
	ProcessStageEntry(NewStage, Stage != nullptr ? Stage->CurrentDay : 0);
}

void UFF_StageEconomyBridgeSubsystem::HandleFestivalStarted()
{
	ProcessFestivalStarted();
}
