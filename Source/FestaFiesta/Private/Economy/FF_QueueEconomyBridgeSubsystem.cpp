// Fill out your copyright notice in the Description page of Project Settings.


#include "Economy/FF_QueueEconomyBridgeSubsystem.h"

#include "Economy/FF_EconomyLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Queue/FF_QueueSubsystem.h"

void UFF_QueueEconomyBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Queue 가 이 브리지보다 먼저 초기화되는 것을 보장한다.
	// Queue 는 World 스코프라 같은 컬렉션에 있고, 그래서 이 방식이 성립한다.
	Queue = Collection.InitializeDependency<UFF_QueueSubsystem>();
	Super::Initialize(Collection);

	if (Queue == nullptr)
	{
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] Queue 서브시스템이 없다. 부스 수수료가 들어오지 않는다."));
		return;
	}

	Queue->OnPartyExited.AddDynamic(this, &UFF_QueueEconomyBridgeSubsystem::HandlePartyExited);
}

void UFF_QueueEconomyBridgeSubsystem::Deinitialize()
{
	// 월드가 죽을 때 Queue 도 같이 죽으므로 구독도 같은 시점에 사라져야 한다.
	if (Queue != nullptr)
	{
		Queue->OnPartyExited.RemoveDynamic(this, &UFF_QueueEconomyBridgeSubsystem::HandlePartyExited);
		Queue = nullptr;
	}

	Super::Deinitialize();
}

void UFF_QueueEconomyBridgeSubsystem::HandlePartyExited(
	int32 FacilityId,
	int32 PartyId,
	EFF_QueueExit ExitType,
	float WaitedSeconds)
{
	// 결제가 없는 퇴장은 여기서 끝낸다.
	if (!IsPaidExit(ExitType)) return;

	// TODO(스텝 4): 여기부터가 실제 매출 경로
	//
	//   [1] Queue 의 FacilityId(int32) → Zone 의 FGuid
	//      UFF_ZoneSystemQueueBridgeSubsystem::FindZoneFacilityId() 가 역매핑을 갖고 있다.
	//
	//   [2] FGuid → FacilityDefinitionId → 소분류 토큰
	//      UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId() 가 파싱을 갖는다.
	//
	//   [3] 소분류 토큰 → 객단가
	//      FFF_BoothRevenueRules::FindRevenuePerUse(). 표에 없으면 0 이고,
	//      그때 시설당 한 번만 Warning 을 남긴다(네이밍 컨벤션 위반 진단).
	//
	//   [4] 인원
	//      OnPartyExited 에는 인원이 없다. 
	//      UFF_CrowdSimulationSubsystem::GetPartyMembers(PartyId)로 얻는다.
	//
	//   [5] 수수료 계산 → 원장
	//      FFF_BoothRevenueRules::ResolveCommission() 뒤 AddRevenue(..., FacilityCommission, FacilityId).
	//      0 이면 AddRevenue 를 부르지 않는다.
	//
	// WaitedSeconds 는 쓰지 않는다.
}

UFF_FestivalProgressionSubsystem*
UFF_QueueEconomyBridgeSubsystem::ResolveFestivalProgression() const
{
	// FestivalProgression은 GameInstanceSubsystem이라 InitializeDependency로 초기화 순서를 보장할 수 없다. 
	// Initialize 시점의 nullptr 캐시를 피하기 위해 필요할 때마다 조회한다. 
	// 결제당 1회 조회라 비용은 무시할 수준이다.
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
		: nullptr;
}
