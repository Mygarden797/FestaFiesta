// Fill out your copyright notice in the Description page of Project Settings.


#include "Economy/FF_CrowdEconomyBridgeSubsystem.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Economy/FF_EconomyLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"

void UFF_CrowdEconomyBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Crowd 가 이 브리지보다 먼저 초기화되는 것을 보장한다.
	// Crowd 는 World 스코프라 같은 컬렉션에 있고, 그래서 이 방식이 성립한다.
	Crowd = Collection.InitializeDependency<UFF_CrowdSimulationSubsystem>();
	Super::Initialize(Collection);

	if (Crowd == nullptr)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] Crowd 서브시스템이 없다. 입장료·예상 방문객 반영이 안 된다."));
		return;
	}

	Crowd->OnVisitorsAdmitted.AddDynamic(this, &UFF_CrowdEconomyBridgeSubsystem::HandleVisitorsAdmitted);
	BindProgression();
}

void UFF_CrowdEconomyBridgeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// GameInstance 가 Initialize 시점엔 아직 안 붙어 있었을 수 있다. 플레이 시작 전에 다시 시도한다.
	BindProgression();
}

void UFF_CrowdEconomyBridgeSubsystem::Deinitialize()
{
	// 월드가 죽을 때 Crowd 도 같이 죽으므로 구독도 같은 시점에 사라져야 한다.
	if (Crowd != nullptr)
	{
		Crowd->OnVisitorsAdmitted.RemoveDynamic(this, &UFF_CrowdEconomyBridgeSubsystem::HandleVisitorsAdmitted);
		Crowd = nullptr;
	}

	// 경제는 이 월드보다 오래 살 수 있다(GameInstance 스코프) — 구독을 안 지우면
	// 죽은 이 브리지를 계속 가리키는 델리게이트가 남는다.
	if (Progression != nullptr)
	{
		Progression->OnFestivalDefinitionChanged.RemoveDynamic(
			this, &UFF_CrowdEconomyBridgeSubsystem::HandleFestivalDefinitionChanged);
		Progression->OnExpectedVisitorsChanged.RemoveDynamic(
			this, &UFF_CrowdEconomyBridgeSubsystem::HandleExpectedVisitorsChanged);
		Progression = nullptr;
	}

	Super::Deinitialize();
}

void UFF_CrowdEconomyBridgeSubsystem::BindProgression()
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFF_FestivalProgressionSubsystem* Current =
		GameInstance ? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	if (Progression == Current) { return; }

	if (Progression != nullptr)
	{
		Progression->OnFestivalDefinitionChanged.RemoveDynamic(
			this, &UFF_CrowdEconomyBridgeSubsystem::HandleFestivalDefinitionChanged);
		Progression->OnExpectedVisitorsChanged.RemoveDynamic(
			this, &UFF_CrowdEconomyBridgeSubsystem::HandleExpectedVisitorsChanged);
	}

	Progression = Current;

	if (Progression != nullptr)
	{
		Progression->OnFestivalDefinitionChanged.AddUniqueDynamic(
			this, &UFF_CrowdEconomyBridgeSubsystem::HandleFestivalDefinitionChanged);
		Progression->OnExpectedVisitorsChanged.AddUniqueDynamic(
			this, &UFF_CrowdEconomyBridgeSubsystem::HandleExpectedVisitorsChanged);

		// 이미 시작된 축제에 이 브리지가 나중에 붙는 경우(월드 재진입 등)를 위해
		// 지금 값으로 한 번 채워 둔다. 방금 붙었으니 Crowd 쪽 값은 낡았을 수 있다.
		if (Progression->IsFestivalStarted() && Crowd != nullptr)
		{
			Crowd->SetExpectedVisitorCountOverride(Progression->GetExpectedVisitors());
		}
	}
}

void UFF_CrowdEconomyBridgeSubsystem::HandleVisitorsAdmitted(int32 AdmittedCount)
{
	if (AdmittedCount <= 0) return;
	if (Progression == nullptr || !Progression->IsFestivalStarted()) return;

	const UFF_FestivalDefinition* Definition = Progression->GetDefinition();
	if (Definition == nullptr) return;
	Progression->RecordAdmittedVisitorsForDailyGoal(AdmittedCount);

	// 시설 핸들이 없는 매출이다(입장은 특정 시설에 귀속되지 않는다) — AddFacilityRevenue 가 아니라
	// 기존 AddRevenue 를 그대로 쓴다. 일일 목표 보너스·미니게임 보상과 같은 패턴이다.
	const int32 Amount = AdmittedCount * Definition->AdmissionFeeGold;
	if (Amount <= 0) return;

	Progression->AddRevenue(Amount, EFF_EarnReason::AdmissionFee);

	UE_LOG(LogFestaFiestaEconomy, Log,
		TEXT("[경제] 입장료: %d명 x %d Gold = %d, 총 매출 %d"),
		AdmittedCount, Definition->AdmissionFeeGold, Amount, Progression->GetTotalRevenue());
}

void UFF_CrowdEconomyBridgeSubsystem::HandleFestivalDefinitionChanged()
{
	// 홍보를 안 사도 BaseDailyVisitors 가 반영되도록, 축제가 (재)시작되는 시점의 값으로 채운다.
	if (Crowd == nullptr || Progression == nullptr || !Progression->IsFestivalStarted()) return;
	Crowd->SetExpectedVisitorCountOverride(Progression->GetExpectedVisitors());
}

void UFF_CrowdEconomyBridgeSubsystem::HandleExpectedVisitorsChanged(int32 NewExpectedVisitors)
{
	if (Crowd == nullptr) return;
	Crowd->SetExpectedVisitorCountOverride(NewExpectedVisitors);
}
