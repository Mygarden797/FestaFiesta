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
	ResetCongestionTracking(false);
	MissingKeyByAgent.Reset();
	Crowd = nullptr;
	Incident = nullptr;

	Super::Deinitialize();
}

void UFF_CrowdIncidentBridgeSubsystem::HandleCongestionSampled(const FFF_CrowdCongestionSnapshot& Snapshot)
{
	// 시작 직후의 미계측 스냅샷과 잘못된 입력은 해소 근거로 쓰지 않는다.
	if (Incident == nullptr || Snapshot.SnapshotIndex < 0
		|| !FMath::IsFinite(Snapshot.SimTimeMinutes)
		|| !FMath::IsFinite(Snapshot.CellSizeMeters) || Snapshot.CellSizeMeters <= 0.f)
	{
		return;
	}

	if (LastCongestionSnapshotIndex != INDEX_NONE)
	{
		const bool bSessionReset = Snapshot.SnapshotIndex < LastCongestionSnapshotIndex
			|| Snapshot.SimTimeMinutes < LastCongestionSimMinutes - KINDA_SMALL_NUMBER
			|| !FMath::IsNearlyEqual(Snapshot.CellSizeMeters, LastCongestionCellSizeMeters);
		if (bSessionReset)
		{
			ResetCongestionTracking(true);
		}
		else if (Snapshot.SnapshotIndex == LastCongestionSnapshotIndex)
		{
			return;
		}
		else if (static_cast<int64>(Snapshot.SnapshotIndex) > static_cast<int64>(LastCongestionSnapshotIndex) + 1)
		{
			// 빠진 계측 구간은 혼잡 또는 해소가 이어졌다고 가정하지 않는다.
			CongestionStreakByCell.Reset();
			CongestionClearStreakByCell.Reset();
		}
	}

	LastCongestionSnapshotIndex = Snapshot.SnapshotIndex;
	LastCongestionSimMinutes = Snapshot.SimTimeMinutes;
	LastCongestionCellSizeMeters = Snapshot.CellSizeMeters;
	const int32 RequiredClearSnapshots = FMath::Max(1, UFF_IncidentSettings::Get()->CongestionClearSnapshots);

	TSet<FIntPoint> CurrentCells;
	for (const FFF_CrowdCongestedCell& Cell : Snapshot.Cells)
	{
		if (CurrentCells.Contains(Cell.Cell))
		{
			continue;
		}
		CurrentCells.Add(Cell.Cell);
		CongestionClearStreakByCell.Remove(Cell.Cell);
		if (OpenCongestionHandles.Contains(Cell.Cell))
		{
			// 현장 처리로 닫혔어도 같은 혼잡이 이어지는 동안 다시 신고하지 않는다.
			continue;
		}

		int32& Streak = CongestionStreakByCell.FindOrAdd(Cell.Cell);
		Streak = FMath::Min(Streak + 1, CongestionSustainSnapShots);
		if (Streak < CongestionSustainSnapShots)
		{
			continue;
		}

		FFF_IncidentReport Report;
		Report.IncidentTag = FFIncidentTags::Crowd::Congestion;
		Report.Anchor.Kind = EFF_IncidentAnchorKind::DensityCell;
		Report.Anchor.DensityCell = Cell.Cell;
		Report.Anchor.WorldPoint = Cell.CenterWorldUU;
		Report.Intensity = 1.f;
		const FFF_IncidentHandle Handle = Incident->ReportIncident(Report);
		if (Handle.IsSet())
		{
			OpenCongestionHandles.Add(Cell.Cell, Handle);
			FF_CI_BRIDGE_LOG("CONGESTION reported cell (%d,%d) density %.2f occupancy %d streak %d",
				Cell.Cell.X, Cell.Cell.Y, Cell.DensityPerSqm, Cell.Occupancy, Streak);
		}
	}

	for (auto It = CongestionStreakByCell.CreateIterator(); It; ++It)
	{
		if (!CurrentCells.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	TArray<TPair<FIntPoint, FFF_IncidentHandle>> ClearedCells;
	for (auto It = OpenCongestionHandles.CreateIterator(); It; ++It)
	{
		if (CurrentCells.Contains(It.Key()))
		{
			continue;
		}
		if (++CongestionClearStreakByCell.FindOrAdd(It.Key()) < RequiredClearSnapshots)
		{
			continue;
		}
		ClearedCells.Emplace(It.Key(), It.Value());
		CongestionClearStreakByCell.Remove(It.Key());
		It.RemoveCurrent();
	}

	// 종료 델리게이트가 다른 사건을 만들더라도 순회와 오래된 핸들 철회가 섞이지 않게 한다.
	for (const TPair<FIntPoint, FFF_IncidentHandle>& Cleared : ClearedCells)
	{
		if (!Incident->IsHandleValid(Cleared.Value))
		{
			continue;
		}
		FFF_IncidentAnchor Anchor;
		Anchor.Kind = EFF_IncidentAnchorKind::DensityCell;
		Anchor.DensityCell = Cleared.Key;
		Incident->WithdrawIncident(FFF_IncidentRules::MakeDedupeKey(FFIncidentTags::Crowd::Congestion, Anchor));
		FF_CI_BRIDGE_LOG("CONGESTION withdrawn cell (%d,%d) after %d clear snapshots",
			Cleared.Key.X, Cleared.Key.Y, RequiredClearSnapshots);
	}
}

void UFF_CrowdIncidentBridgeSubsystem::ResetCongestionTracking(bool bWithdrawOpenIncidents)
{
	const TMap<FIntPoint, FFF_IncidentHandle> PreviousHandles = MoveTemp(OpenCongestionHandles);
	OpenCongestionHandles.Reset();
	CongestionStreakByCell.Reset();
	CongestionClearStreakByCell.Reset();
	LastCongestionSnapshotIndex = INDEX_NONE;
	LastCongestionSimMinutes = 0.f;
	LastCongestionCellSizeMeters = 0.f;

	if (!bWithdrawOpenIncidents || !Incident)
	{
		return;
	}
	for (const TPair<FIntPoint, FFF_IncidentHandle>& Previous : PreviousHandles)
	{
		if (!Incident->IsHandleValid(Previous.Value))
		{
			continue;
		}
		FFF_IncidentAnchor Anchor;
		Anchor.Kind = EFF_IncidentAnchorKind::DensityCell;
		Anchor.DensityCell = Previous.Key;
		Incident->WithdrawIncident(FFF_IncidentRules::MakeDedupeKey(FFIncidentTags::Crowd::Congestion, Anchor),
			EFF_IncidentCloseReason::SessionReset);
	}
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
