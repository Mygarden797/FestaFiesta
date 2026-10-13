// Fill out your copyright notice in the Description page of Project Settings.

#include "Performance/FF_CrowdPerformanceBridgeSubsystem.h"

#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSFacilityActor.h"

void UFF_CrowdPerformanceBridgeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Crowd = InWorld.GetSubsystem<UFF_CrowdSimulationSubsystem>();
}

void UFF_CrowdPerformanceBridgeSubsystem::Deinitialize()
{
	Crowd = nullptr;
	DancingAgents.Reset();
	ScratchSeated.Reset();
	Super::Deinitialize();
}

TStatId UFF_CrowdPerformanceBridgeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_CrowdPerformanceBridgeSubsystem, STATGROUP_Tickables);
}

void UFF_CrowdPerformanceBridgeSubsystem::Tick(float DeltaTime)
{
	SecondsSinceSync += DeltaTime;
	if (!Crowd || SecondsSinceSync < SyncIntervalSeconds)
	{
		return;
	}
	SecondsSinceSync = 0.f;

	Crowd->GetSeatedAudienceAgents(ScratchSeated);

	TSet<int32> NowDancing;
	for (const FFF_CrowdSeatedAgent& Seated : ScratchSeated)
	{
		if (ShouldDance(Seated))
		{
			NowDancing.Add(Seated.AgentIndex);
			Crowd->SetCrowdStateForAgent(Seated.AgentIndex, EFF_CrowdRunState::Dancing);
		}
	}

	for (const int32 AgentIndex : DancingAgents)
	{
		if (!NowDancing.Contains(AgentIndex))
		{
			Crowd->SetCrowdStateForAgent(AgentIndex, EFF_CrowdRunState::Running);
		}
	}
	DancingAgents = MoveTemp(NowDancing);
}

bool UFF_CrowdPerformanceBridgeSubsystem::ShouldDance(const FFF_CrowdSeatedAgent& Seated) const
{
	if (Seated.Phase != EFF_PartyAudiencePhase::Spectating)
	{
		return false;
	}

	const UFFZSWorldSubsystem* Zones = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>();
	const AFFZSFacilityActor* Facility = Zones ? Zones->FindFacility(Seated.FacilityId) : nullptr;
	return Facility && FFF_ZoneSystemBridge::IsMainStageFacility(
		Facility->GetFacilitySpec().Category, Facility->GetFacilitySpec().FacilityDefinitionId);
}
