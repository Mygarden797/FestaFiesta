// Fill out your copyright notice in the Description page of Project Settings.

#define FF_CI_BRIDGE_LOG(Format, ...) \
UE_LOG(LogTemp, Warning, TEXT("[FF_CrowdIncidentBridgeSubsystem] " Format), ##__VA_ARGS__)

#include "Incident/FF_CrowdIncidentBridgeSubsystem.h"

#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Incident/FF_IncidentSettings.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Incident/FF_IncidentTags.h"
#include "Incident/Rules/FF_IncidentRules.h"


void UFF_CrowdIncidentBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Crowd = Collection.InitializeDependency<UFF_CrowdSimulationSubsystem>();
	Incident = Collection.InitializeDependency<UFF_IncidentSubsystem>();
	Super::Initialize(Collection);

	if (Crowd == nullptr || Incident == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdIncidentBridge] Crowd 또는 Incident 서브시스템이 없다. Crowd 사건이 올라가지 않는다."));
		return;
	}

	CongestionHandle = Crowd->OnCongestionSampled.AddUObject(
		this, &UFF_CrowdIncidentBridgeSubsystem::HandleCongestionSampled);
	MissingHandle = Crowd->OnMemberMissingChanged.AddUObject(
		this, &UFF_CrowdIncidentBridgeSubsystem::HandleMemberMissingChanged);
}

void UFF_CrowdIncidentBridgeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (Crowd != nullptr)
	{
		HandleCongestionSampled(Crowd->GetCongestionSnapshot());
	}
}

void UFF_CrowdIncidentBridgeSubsystem::Deinitialize()
{
	if (Crowd != nullptr)
	{
		Crowd->OnCongestionSampled.Remove(CongestionHandle);
		Crowd->OnMemberMissingChanged.Remove(MissingHandle);
	}

	CongestionHandle.Reset();
	MissingHandle.Reset();
	CongestionStreakByCell.Reset();
	OpenCongestionCells.Reset();
	MissingKeyByAgent.Reset();
	Crowd = nullptr;
	Incident = nullptr;

	Super::Deinitialize();
}

void UFF_CrowdIncidentBridgeSubsystem::HandleCongestionSampled(const FFF_CrowdCongestionSnapshot& Snapshot)
{
}

void UFF_CrowdIncidentBridgeSubsystem::HandleMemberMissingChanged(const FFF_CrowdMemberMissingEvent& Event)
{
	
	if (Incident == nullptr)
	{
		FF_CI_BRIDGE_LOG("Incident is nullptr");
		return;
	}
	
	
	switch (Event.Change)
	{
	case EFF_CrowdMissingChange::Started:
		ReportMissing(Event);
		break;

	case EFF_CrowdMissingChange::Returned:
		WithdrawMissing(Event.StableAgentID);
		break;
	case EFF_CrowdMissingChange::EnteredQueue:
		WithdrawMissing(Event.StableAgentID);
		break;
	case EFF_CrowdMissingChange::Despawned:
		WithdrawMissing(Event.StableAgentID);
		break;
	}
}

void UFF_CrowdIncidentBridgeSubsystem::ReportMissing(const FFF_CrowdMemberMissingEvent& Event)
{
	if (Incident == nullptr)
	{
		FF_CI_BRIDGE_LOG("Incident is nullptr");
		return;
	}

	if (!Event.Actor.IsValid())
	{
		FF_CI_BRIDGE_LOG("Agent %d has no actor. Missing incident is not reported", Event.StableAgentID);
		return;
	}

	FFF_IncidentReport EventReport;
	FFF_IncidentAnchor EventAnchor;
	EventAnchor.Actor = Event.Actor;
	EventAnchor.Kind = EFF_IncidentAnchorKind::Actor;

	EventReport.Anchor = EventAnchor;
	EventReport.IncidentTag = FFIncidentTags::Crowd::MemberMissing;
	EventReport.Intensity = 1.f;

	const FName IncidentDedupKey = FFF_IncidentRules::MakeDedupeKey(EventReport.IncidentTag, EventReport.Anchor);

	if (const int32* StaleAgentID = MissingKeyByAgent.FindKey(IncidentDedupKey))
	{
		if (*StaleAgentID != Event.StableAgentID)
		{
			FF_CI_BRIDGE_LOG("Key %s is still held by agent %d. Withdraw was missed",
				*IncidentDedupKey.ToString(), *StaleAgentID);
			WithdrawMissing(*StaleAgentID);
		}
	}

	const FFF_IncidentHandle IncidentHandle = Incident->ReportIncident(EventReport);
	if (!IncidentHandle.IsSet())
	{
		FF_CI_BRIDGE_LOG("ReportIncident rejected agent %d", Event.StableAgentID);
		return;
	}

	MissingKeyByAgent.Add(Event.StableAgentID, IncidentDedupKey);
}

void UFF_CrowdIncidentBridgeSubsystem::WithdrawMissing(int32 StableAgentID)
{
	 FName* IncidentDedupKey =  MissingKeyByAgent.Find(StableAgentID);
	
	if (!IncidentDedupKey)
	{
		FF_CI_BRIDGE_LOG("IncidentDedupKey is nullptr");
		return;
	}
	
	
	Incident->WithdrawIncident(*IncidentDedupKey);
	MissingKeyByAgent.Remove(StableAgentID);
}
