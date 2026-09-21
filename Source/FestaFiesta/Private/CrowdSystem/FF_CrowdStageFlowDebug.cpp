// Copyright Epic Games, Inc. All Rights Reserved.

#include "CrowdSystem/Systems/FF_CrowdDecisionSubsystem.h"

#if !UE_BUILD_SHIPPING

#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Queue/FF_QueueWaitAdapter.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

namespace FFStageFlowDebug
{
	bool Contains(const TArray<FFF_DestinationCandidate>& Candidates, FName Id)
	{
		return Candidates.ContainsByPredicate([Id](const FFF_DestinationCandidate& Candidate)
		{
			return Candidate.DestinationId == Id;
		});
	}

	void ProbePath(UWorld* World, const FVector& From, const FVector& To, const TCHAR* Label,
		FOutputDevice& Ar)
	{
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
		const ANavigationData* Data = Nav ? Nav->GetDefaultNavDataInstance() : nullptr;
		if (!Data)
		{
			Ar.Logf(TEXT("  Nav[%s]: NO_NAV_DATA"), Label);
			return;
		}
		FNavLocation Start, End;
		const FVector Extent = FFF_CrowdNav::GetAnchorProjectionExtentCm();
		const bool bStart = Nav->ProjectPointToNavigation(From, Start, Extent);
		const bool bEnd = Nav->ProjectPointToNavigation(To, End, Extent);
		// Same raw endpoints and flags as BuildPathForAgent. Projection is diagnostic only.
		FPathFindingQuery Query(nullptr, *Data, From, To);
		Query.SetAllowPartialPaths(true);
		Query.SetRequireNavigableEndLocation(false);
		const FPathFindingResult Result = Nav->FindPathSync(Query);
		const bool bSuccess = Result.IsSuccessful() && Result.Path.IsValid();
		double EndGapCm = -1.0;
		if (bSuccess && !Result.Path->GetPathPoints().IsEmpty())
		{
			EndGapCm = FVector::Dist(Result.Path->GetPathPoints().Last().Location, To);
		}
		Ar.Logf(TEXT("  Nav[%s]: startProjected=%d endProjected=%d endProjectionOffsetCm=%.1f path=%s endGapCm=%.1f points=%d"),
			Label, bStart, bEnd, bEnd ? FVector::Dist(End.Location, To) : -1.0,
			!bSuccess ? TEXT("FAILED") : (Result.IsPartial() ? TEXT("PARTIAL") : TEXT("COMPLETE")),
			EndGapCm, bSuccess ? Result.Path->GetPathPoints().Num() : 0);
	}
}

void UFF_CrowdDecisionSubsystem::DebugDumpStageFlow(int32 LeaderAgentIndex, FOutputDevice& Ar) const
{
	UWorld* World = GetWorld();
	const UFF_CrowdSimulationSubsystem* Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
	const UFFZSWorldSubsystem* Zones = GetZoneSystem();
	if (!Crowd || !Zones)
	{
		Ar.Logf(TEXT("StageFlow: Crowd/Zone subsystem missing. Run in PIE/game."));
		return;
	}
	const FFFZSReadModel Model = Zones->BuildReadModel();
	TMap<FName, int32> LeadersByZone;
	int32 FirstLeader = INDEX_NONE;
	for (int32 Index = 0; Index < Crowd->GetAgentPoolSize(); ++Index)
	{
		if (Crowd->IsAgentActive(Index) && Crowd->IsPartyLeader(Index))
		{
			if (FirstLeader == INDEX_NONE) { FirstLeader = Index; }
			++LeadersByZone.FindOrAdd(Zones->ResolveZoneAtWorldLocation(Crowd->GetAgentPosition(Index)));
		}
	}
	Ar.Logf(TEXT("=== StageFlow world=%s time=%.2fs revision=%lld decisionEnabled=%d mode=%d mainGate=%d maxCandidates=%d stageRedecisionLimit=%d ==="),
		*World->GetName(), World->GetTimeSeconds(), Model.WorldRevision, IsDecisionEnabled(),
		static_cast<int32>(DecisionMode), IsMainStageOpen(), MaxCandidates, MaxPartiesRedecidedOnStageOpen);
	Ar.Logf(TEXT("GlobalMainStageDiscovery=%d (main stages bypass local candidate cap; reservation filter still applies)"),
		IsGlobalMainStageDiscoveryActive());
	const UGameInstance* GI = World->GetGameInstance();
	const UFF_FestivalStageSubsystem* Stage = GI ? GI->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	const UFF_ProgramReservationSubsystem* Reservations = GI ? GI->GetSubsystem<UFF_ProgramReservationSubsystem>() : nullptr;
	if (Stage)
	{
		Ar.Logf(TEXT("Stage=%d elapsedLiveSeconds=%.2f"), static_cast<int32>(Stage->CurrentStage), Stage->ElapsedLiveSeconds);
	}
	Ar.Logf(TEXT("ActiveParties=%d outsideZoneParties=%d queuedParties=%d pendingDecisions=%d"),
		Crowd->GetActivePartyCount(), LeadersByZone.FindRef(NAME_None), Crowd->GetQueuedPartyCount(), Crowd->GetPendingDecisionCount());
	const FBox& SimArea = Crowd->GetSimAreaBounds();
	Ar.Logf(TEXT("SimOrigin=%s simAreaMin=%s simAreaMax=%s avoidance=%d pathFollowing=%d pathFailures=%d partialPaths=%d pathBacklog=%d"),
		*Crowd->SimOrigin.ToCompactString(),
		SimArea.IsValid ? *SimArea.Min.ToCompactString() : TEXT("none"),
		SimArea.IsValid ? *SimArea.Max.ToCompactString() : TEXT("none"),
		Crowd->bEnableAvoidance,
		Crowd->bEnablePathFollowing, Crowd->GetPathQueryFailures(),
		Crowd->GetPartialPathCount(), Crowd->GetPathQueryBacklog());
	for (const FFFZSZoneReadModel& Zone : Model.Zones)
	{
		TArray<FString> Neighbors;
		for (FName Id : Zone.NeighborZoneIds) { Neighbors.Add(Id.ToString()); }
		Ar.Logf(TEXT("Zone=%s leaders=%d neighbors=[%s]"), *Zone.ZoneId.ToString(),
			LeadersByZone.FindRef(Zone.ZoneId), *FString::Join(Neighbors, TEXT(",")));
	}
	if (LeaderAgentIndex == INDEX_NONE) { LeaderAgentIndex = FirstLeader; }
	if (!Crowd->IsAgentActive(LeaderAgentIndex) || !Crowd->IsPartyLeader(LeaderAgentIndex))
	{
		Ar.Logf(TEXT("No valid active leader. Use FF.Crowd.StageFlow <leader agent index>, not PartyId."));
		return;
	}
	const FVector Position = Crowd->GetAgentPosition(LeaderAgentIndex);
	const FName CurrentZone = Zones->ResolveZoneAtWorldLocation(Position);
	const int32 PartySize = Crowd->GetPartyMembers(Crowd->GetAgentPartyID(LeaderAgentIndex)).Num();
	Ar.Logf(TEXT("Sample leader=%d party=%d members=%d zone=%s position=%s target=%s"), LeaderAgentIndex,
		Crowd->GetAgentPartyID(LeaderAgentIndex), PartySize, *CurrentZone.ToString(),
		*Position.ToCompactString(), *Crowd->GetAgentDestination(LeaderAgentIndex).ToCompactString());
	// Do not rebuild the distance cache: inspect exactly the cache available to decisions now.
	const FFF_WalkDistanceMatrix* Matrix = bEnableWalkDistances && WalkDistances.IsValid() ? &WalkDistances : nullptr;
	Ar.Logf(TEXT("CandidateDistance=%s cacheRevision=%lld (cache is not rebuilt by this command)"),
		Matrix ? TEXT("walk matrix") : TEXT("straight line"), WalkDistanceBuiltRevision);
	const bool bGlobalDiscovery = IsGlobalMainStageDiscoveryActive();
	const TArray<FFF_DestinationCandidate> Ungated = FFF_ZoneSystemBridge::BuildCandidates(Model, CurrentZone, Position, 0, Matrix, true, bGlobalDiscovery);
	const TArray<FFF_DestinationCandidate> Gated = FFF_ZoneSystemBridge::BuildCandidates(Model, CurrentZone, Position, 0, Matrix, IsMainStageOpen(), bGlobalDiscovery);
	TArray<FFF_DestinationCandidate> Capped = FFF_ZoneSystemBridge::BuildCandidates(Model, CurrentZone, Position, MaxCandidates, Matrix, IsMainStageOpen(), bGlobalDiscovery);
	if (DecisionMode == EFF_DecisionMode::Full) { FFF_QueueWaitAdapter::ApplyFromZoneBridge(World, Capped, PartySize); }
	const UFF_ZoneSystemQueueBridgeSubsystem* Bridge = World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>();
	const UFF_QueueSubsystem* Queue = World->GetSubsystem<UFF_QueueSubsystem>();
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	Ar.Logf(TEXT("NavPendingBuildTasks=%d QueueAdvancing=%d"), Nav ? Nav->GetNumRemainingBuildTasks() : -1, Crowd->IsQueueAdvancing());
	Ar.Logf(TEXT("Eligibility below is PRE-UTILITY. Need/preference/history/return deadline can still change choice. Nav probes are samples, not arrival guarantees."));
	int32 StageCount = 0;
	for (const FFFZSFacilityReadModel& Facility : Model.Facilities)
	{
		if (Facility.Category != EFFZSFacilityCategory::Performance) { continue; }
		if (++StageCount > 32) { Ar.Logf(TEXT("Stage output capped at 32 facilities.")); break; }
		const FName Id = FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(Facility.FacilityInstanceId);
		const bool bProgram = Reservations && Reservations->IsProgramFacility(Facility.FacilityInstanceId);
		const FFF_DestinationCandidate* Candidate = Capped.FindByPredicate([Id](const FFF_DestinationCandidate& C) { return C.DestinationId == Id; });
		const FFFZSZoneReadModel* OwnZone = Model.Zones.FindByPredicate([CurrentZone](const FFFZSZoneReadModel& Z) { return Z.ZoneId == CurrentZone; });
		const bool bGlobalStage = bGlobalDiscovery && FFF_ZoneSystemBridge::IsMainStageFacility(Facility.Category, Facility.FacilityDefinitionId)
			&& Model.Zones.ContainsByPredicate([&Facility](const FFFZSZoneReadModel& Z) { return Z.ZoneId == Facility.ZoneId; });
		const bool bInScope = OwnZone && (bGlobalStage || CurrentZone == Facility.ZoneId || OwnZone->NeighborZoneIds.Contains(Facility.ZoneId));
		const TCHAR* Eligibility = !bInScope ? TEXT("OUTSIDE_SEARCH_ZONES")
			: Facility.State != EFFZSFacilityState::Active || Facility.bNeedsRelocation ? TEXT("INACTIVE_OR_RELOCATION")
			: FFStageFlowDebug::Contains(Ungated, Id) && !FFStageFlowDebug::Contains(Gated, Id) ? TEXT("MAIN_STAGE_GATE_CLOSED")
			: !Candidate ? TEXT("CANDIDATE_CAP")
			: bProgram ? TEXT("RESERVATION_ONLY")
			: !Candidate->bIsOpen ? TEXT("QUEUE_CLOSED")
			: !Facility.bHasApproachWorldTransform ? TEXT("MISSING_APPROACH") : TEXT("ELIGIBLE");
		int32 InScopeParties = 0;
		for (const FFFZSZoneReadModel& Zone : Model.Zones)
		{
			if (bGlobalStage || Zone.ZoneId == Facility.ZoneId || Zone.NeighborZoneIds.Contains(Facility.ZoneId))
			{
				InScopeParties += LeadersByZone.FindRef(Zone.ZoneId);
			}
		}
		Ar.Logf(TEXT("--- %s id=%s zone=%s state=%s relocation=%d programOnly=%d sample=%s ---"),
			*Facility.FacilityDefinitionId.ToString(), *Id.ToString(), *Facility.ZoneId.ToString(),
			*StaticEnum<EFFZSFacilityState>()->GetNameStringByValue(static_cast<int64>(Facility.State)),
			Facility.bNeedsRelocation, bProgram, Eligibility);
		Ar.Logf(TEXT("  InSearchScopeParties=%d/%d straightDistanceM=%.1f candidateDistanceM=%.1f inTransitMembersLastDecisionBatch=%d approachValid=%d"),
			InScopeParties, Crowd->GetActivePartyCount(), FVector::Dist(Position, Facility.WorldLocation) * 0.01,
			Candidate ? Candidate->DistanceMeters : -1.f, Crowd->GetInTransitMemberCount(Id), Facility.bHasApproachWorldTransform);
		if (Reservations)
		{
			for (FGuid ReservationId : Reservations->GetReservationIds())
			{
				FFF_ProgramReservation R;
				if (Reservations->FindReservation(ReservationId, R) && R.FacilityInstanceId == Facility.FacilityInstanceId)
				{
					Ar.Logf(TEXT("  Reservation start=%.1f end=%.1f festivalMin assigned=%d capacity=%d groups=%d"),
						R.StartMinutes, R.EndMinutes, R.AssignedHeadcount, R.Capacity, R.AssignedGroupIds.Num());
				}
			}
		}
		const int32 QueueId = Bridge ? Bridge->FindQueueFacilityId(Facility.FacilityInstanceId) : INDEX_NONE;
		if (Queue && QueueId != INDEX_NONE && Queue->IsFacilityRegistered(QueueId))
		{
			const FFF_QueueState Q = Queue->GetQueueState(QueueId);
			Ar.Logf(TEXT("  Queue id=%d open=%d occupiedTeams=%d capacityTeams=%d samplePartyFitsNow=%d serviceTeamsPerSec=%.3f estimatedWaitMin=%.2f"),
				QueueId, Q.bIsOpen, Q.WaitingPartyCount, Q.MaxCapacityParties,
				Q.bIsOpen && Q.WaitingPartyCount < Q.MaxCapacityParties, Q.ServiceRate,
				FFF_QueueWaitAdapter::ToDecisionMinutes(FFF_QueueWaitAdapter::ComputeExpectedWaitSeconds(Q, PartySize)));
			FTransform Slot;
			if (Bridge->TryGetProgramWaitingTransform(Facility.FacilityInstanceId, 0, Slot))
			{
				FFStageFlowDebug::ProbePath(World, Position, Slot.GetLocation(), TEXT("first waiting slot"), Ar);
			}
		}
		else { Ar.Logf(TEXT("  Queue missing/unregistered (id=%d). Check queue bridge/geometry."), QueueId); }
		if (Facility.bHasApproachWorldTransform)
		{
			const FBox& Area = Crowd->GetSimAreaBounds();
			Ar.Logf(TEXT("  ApproachOutsideSimArea=%d (density/height grids do not cover it)"),
				Area.IsValid && !Area.IsInsideXY(Facility.ApproachWorldTransform.GetLocation()));
			Ar.Logf(TEXT("  Approach=%s"), *Facility.ApproachWorldTransform.GetLocation().ToCompactString());
			FFStageFlowDebug::ProbePath(World, Position, Facility.ApproachWorldTransform.GetLocation(), TEXT("approach"), Ar);
		}
	}
	if (StageCount == 0) { Ar.Logf(TEXT("No Performance facility registered.")); }
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GStageFlowCommand(
	TEXT("FF.Crowd.StageFlow"),
	TEXT("Read-only stage candidate, zone, reservation, queue and NavMesh audit. FF.Crowd.StageFlow [leader agent index]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			int32 Leader = INDEX_NONE;
			if (Args.Num() > 1 || (Args.Num() == 1 && (!LexTryParseString(Leader, *Args[0]) || Leader < 0)))
			{
				Ar.Logf(TEXT("Usage: FF.Crowd.StageFlow [nonnegative leader agent index]")); return;
			}
			if (const UFF_CrowdDecisionSubsystem* Decision = World ? World->GetSubsystem<UFF_CrowdDecisionSubsystem>() : nullptr)
			{
				Decision->DebugDumpStageFlow(Leader, Ar);
			}
		}));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GStageReevaluateCommand(
	TEXT("FF.Crowd.Reevaluate"),
	TEXT("Request decisions for non-queued/non-leaving parties. FF.Crowd.Reevaluate [max parties; 0=all, default=64]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
		{
			int32 Limit = 64;
			if (Args.Num() > 1 || (Args.Num() == 1 && (!LexTryParseString(Limit, *Args[0]) || Limit < 0)))
			{
				Ar.Logf(TEXT("Usage: FF.Crowd.Reevaluate [nonnegative limit; 0=all]")); return;
			}
			if (UFF_CrowdSimulationSubsystem* Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr)
			{
				const int32 Before = Crowd->GetPendingDecisionCount();
				const int32 Attempted = Crowd->RequestRedecisionForIdleParties(Limit);
				Ar.Logf(TEXT("Reevaluation attempted=%d newlyQueued=%d pending=%d. Reservation guards still apply; search scope is unchanged."),
					Attempted, Crowd->GetPendingDecisionCount() - Before, Crowd->GetPendingDecisionCount());
			}
		}));

#endif // !UE_BUILD_SHIPPING
