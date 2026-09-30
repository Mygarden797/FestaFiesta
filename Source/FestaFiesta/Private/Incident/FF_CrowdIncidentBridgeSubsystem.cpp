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
	ItemLostHandle = Crowd->OnCrowdItemLostEventChanged.AddUObject(
		this, &UFF_CrowdIncidentBridgeSubsystem::HandleItemLost);
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
		Crowd->OnCrowdItemLostEventChanged.Remove(ItemLostHandle);
	}

	CongestionHandle.Reset();
	MissingHandle.Reset();
	ItemLostHandle.Reset();
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

void UFF_CrowdIncidentBridgeSubsystem::HandleMemberMissingChanged(const TArray<FFF_CrowdMemberMissingEvent>& Events)
{

	if (Incident == nullptr)
	{
		FF_CI_BRIDGE_LOG("Incident is nullptr");
		return;
	}

	for (const FFF_CrowdMemberMissingEvent& Event : Events)
	{
		if (Event.Change != EFF_CrowdMissingChange::Started)
		{
			const UEnum* ChangeEnum = StaticEnum<EFF_CrowdMissingChange>();
			FF_CI_BRIDGE_LOG("MISSING %s agent %d actor %s hasKey %s",
				ChangeEnum ? *ChangeEnum->GetNameStringByValue(static_cast<int64>(Event.Change)) : TEXT("?"),
				Event.StableAgentID,
				Event.Actor.IsValid() ? *Event.Actor->GetName() : TEXT("<null>"),
				MissingKeyByAgent.Contains(Event.StableAgentID) ? TEXT("Y") : TEXT("N"));
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

	FF_CI_BRIDGE_LOG("MISSING Started agent %d party %d actor %s actorLoc %s eventLoc %s gap %.0f",
		Event.StableAgentID, Event.PartyID, *Event.Actor->GetName(),
		*Event.Actor->GetActorLocation().ToCompactString(), *Event.LocationUU.ToCompactString(),
		FVector::Dist(Event.Actor->GetActorLocation(), Event.LocationUU));

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

/**
 * 분실물은 떨어뜨린 순간 한 번뿐이고 Crowd가 거둘 일이 없다. 그래서 Report만 하고 에이전트→키 맵을 두지 않는다.
 * 끝은 허브가 낸다(플레이어 해소 또는 만료). 주인이 퇴장해도 물건은 남는다.
 * WorldPoint 앵커는 WorldPointDedupeGridUU 칸 단위로 합쳐지므로 같은 칸의 두 번째 분실은 기존 사건 갱신이 된다.
 */
void UFF_CrowdIncidentBridgeSubsystem::HandleItemLost(const TArray<FFF_CrowdItemLostEvent>& Events)
{
	if (Incident == nullptr)
	{
		FF_CI_BRIDGE_LOG("Incident is nullptr");
		return;
	}

	for (const FFF_CrowdItemLostEvent& Event : Events)
	{
		FFF_IncidentReport EventReport;
		EventReport.IncidentTag = FFIncidentTags::Crowd::ItemLost;
		EventReport.Anchor.Kind = EFF_IncidentAnchorKind::WorldPoint;
		EventReport.Anchor.WorldPoint = Event.LocationUU;
		EventReport.Intensity = 1.f;

		const FFF_IncidentHandle IncidentHandle = Incident->ReportIncident(EventReport);
		if (!IncidentHandle.IsSet())
		{
			FF_CI_BRIDGE_LOG("ReportIncident rejected item lost of agent %d", Event.StableAgentID);
		}
	}
}
