// Fill out your copyright notice in the Description page of Project Settings.


#include "Economy/FF_QueueEconomyBridgeSubsystem.h"
#include "Building/FF_FacilityNamingLibrary.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Economy/Rules/FF_BoothRevenueRules.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
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

	UWorld* World = GetWorld();
	if (World == nullptr) return;

	UFF_FestivalProgressionSubsystem* Progression = ResolveFestivalProgression();
	if (Progression == nullptr || !Progression->IsFestivalStarted()) return;

	const UFF_FestivalDefinition* Definition = Progression->GetDefinition();
	if (Definition == nullptr) return;

	//   [1] Queue 의 FacilityId(int32) → Zone 의 FGuid
	const UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge =
			World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>();
	if (QueueBridge == nullptr) return;

	const FGuid ZoneFacilityId = QueueBridge->FindZoneFacilityId(FacilityId);
	if (!ZoneFacilityId.IsValid()) return;

	//   [2] FGuid → 시설 사양
	const UFFZSWorldSubsystem* ZoneSystem = World->GetSubsystem<UFFZSWorldSubsystem>();
	const AFFZSFacilityActor* Facility = ZoneSystem ? ZoneSystem->FindFacility(ZoneFacilityId) : nullptr;
	if (Facility == nullptr) return;

	const FFFZSFacilitySpec& Spec = Facility->GetFacilitySpec();
	
	//   [3] FacilityDefinitionId → 소분류 토큰
	FFF_FacilityNameParts Parts;
	FString FailureReason;
	if (!UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			Spec.Category, Spec.FacilityDefinitionId, Parts, FailureReason))
	{
		// 이 부스는 앞으로도 계속 매출이 0 이다. 조용히 두면 "인기 없는 부스" 로 보인다.
		if (!WarnedNamingViolations.Contains(Spec.FacilityDefinitionId))
		{
			WarnedNamingViolations.Add(Spec.FacilityDefinitionId);
			UE_LOG(LogFestaFiestaEconomy, Warning,
				TEXT("[경제] 시설 '%s' 의 이름을 읽지 못해 수수료를 못 받는다: %s"),
				*Spec.FacilityDefinitionId.ToString(), *FailureReason);
		}
		return;
	}
	
	//   [4] 소분류 → 객단가. 0 은 무료 시설이라 정상 경로다(휴식·포토존·공연)
	const int32 RevenuePerUse = FFF_BoothRevenueRules::FindRevenuePerUse(
		Definition->RevenuePerUseByMinorToken, Parts.MinorToken);
	if (RevenuePerUse <= 0) return;

	//   [5] 인원
	// Served 는 퇴장이 아니라 이용 종료라 이 시점에 파티가 살아 있어야 한다.
	const UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	if (Crowd == nullptr) return;

	const int32 MemberCount = Crowd->GetPartyMembers(PartyId).Num();
	if (MemberCount <= 0)
	{
		// 결제 한 건이 통째로 사라진다. 여기는 매번 알린다 — 반복되면 그 자체가 신호다.
		UE_LOG(LogFestaFiestaEconomy, Warning,
			TEXT("[경제] 파티 %d 가 시설 '%s' 이용을 마쳤는데 인원을 찾을 수 없다. 수수료를 못 받는다."),
			PartyId, *Spec.FacilityDefinitionId.ToString());
		return;
	}
	
	// [6] 수수료 → 원장.
	const int32 Commission = FFF_BoothRevenueRules::ResolveCommission(
		RevenuePerUse, MemberCount, Definition->BoothCommissionRate);
	if (Commission <= 0) return;

	Progression->AddRevenue(Commission, EFF_EarnReason::FacilityCommission, Spec.FacilityDefinitionId);
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
