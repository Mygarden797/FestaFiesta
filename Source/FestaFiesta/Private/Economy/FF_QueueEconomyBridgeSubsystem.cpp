// Fill out your copyright notice in the Description page of Project Settings.


#include "Economy/FF_QueueEconomyBridgeSubsystem.h"
#include "Building/FF_FacilityNamingLibrary.h"
#include "Building/Synergy/FF_FacilitySynergySubsystem.h"
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

bool UFF_QueueEconomyBridgeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

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

	Queue->OnPartyServiceCompleted.AddDynamic(this, &UFF_QueueEconomyBridgeSubsystem::HandlePartyServiceCompleted);
}

void UFF_QueueEconomyBridgeSubsystem::Deinitialize()
{
	// 월드가 죽을 때 Queue 도 같이 죽으므로 구독도 같은 시점에 사라져야 한다.
	if (Queue != nullptr)
	{
		Queue->OnPartyServiceCompleted.RemoveDynamic(this, &UFF_QueueEconomyBridgeSubsystem::HandlePartyServiceCompleted);
		Queue = nullptr;
	}

	Super::Deinitialize();
}

void UFF_QueueEconomyBridgeSubsystem::HandlePartyServiceCompleted(
	int32 FacilityId,
	int32 PartyId,
	int32 ServedMemberCount,
	float WaitedSeconds)
{
	// 취소·포기는 완료 신호를 만들지 않는다. 정산은 Queue가 확정한 이용 인원만 받는다.
	if (ServedMemberCount <= 0) return;

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
	
	const UDataTable* RevenueTable = Definition->GetFacilityRevenueTable();
	//   [3] 시설별 표와 개별 설정이 없는 기존 시설만 소분류 이름을 확인한다.
	FFF_FacilityNameParts Parts;
	FString FailureReason;
	if (!FFF_BoothRevenueRules::HasFacilityRevenueRow(Spec, RevenueTable)
		&& !Spec.bOverrideRevenuePerUseGold && !UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
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
	
	//   [4] 시설별 표 → Data Asset의 개별 설정 → 소분류 기본값. 명시한 0은 무료다.
	const int32 RevenuePerUse = FFF_BoothRevenueRules::ResolveRevenuePerUse(
		Spec, Definition->RevenuePerUseByMinorToken, RevenueTable);
	if (RevenuePerUse <= 0) return;

	// [5] 가입 때 확정한 팀 인원 × 1인 요금 × 수수료율 × 시설 효과 → 원장.
	// 완료 시점에 Crowd를 다시 조회하지 않으므로 팀 정리·변경 순서가 금액을 바꾸지 않는다.
	const UFF_FacilitySynergySubsystem* Synergies = World->GetSubsystem<UFF_FacilitySynergySubsystem>();
	const float SynergyMultiplier = Synergies ? Synergies->GetRevenueMultiplier(ZoneFacilityId) : 1.f;
	const float CommissionRate = FFF_BoothRevenueRules::ResolveCommissionRate(
		Spec, Definition->BoothCommissionRate, RevenueTable);
	const float RevenueMultiplier = FFF_BoothRevenueRules::ResolveRevenueMultiplier(
		Spec, Facility->GetUpgradeLevel(), Facility->GetUpgradeRevenueMultiplier(), RevenueTable);
	const int32 Commission = FFF_BoothRevenueRules::ResolveCommission(
		RevenuePerUse, ServedMemberCount, CommissionRate, static_cast<double>(RevenueMultiplier) * SynergyMultiplier);
	if (Commission <= 0) return;

	// 종류(FacilityDefinitionId)는 결산 표로, 인스턴스(ZoneFacilityId)는 시설 정보 패널의 시설별 장부로 간다.
	Progression->AddFacilityRevenue(
		Commission, EFF_EarnReason::FacilityCommission, Spec.FacilityDefinitionId, ZoneFacilityId);
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
