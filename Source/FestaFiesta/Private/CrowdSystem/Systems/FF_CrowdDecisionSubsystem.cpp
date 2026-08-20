// Copyright Epic Games, Inc. All Rights Reserved.

#include "CrowdSystem/Systems/FF_CrowdDecisionSubsystem.h"

#include "FF_FFZSZoneActor.h"
#include "NavigationSystem.h"

#include "CoreGlobals.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "CrowdSystem/Types/FF_SimulationSettings.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "NPC/Destination/FF_PreferenceVectorBuilder.h"
#include "NPC/Destination/FF_UtilityCoefficientHelpers.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"
#include "NPC/Party/FF_VisitorDistributionBuilder.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCrowdDecision, Log, All);

void UFF_CrowdDecisionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	UFF_CrowdSimulationSubsystem* Crowd = InWorld.GetSubsystem<UFF_CrowdSimulationSubsystem>();
	if (!Crowd)
	{
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("CrowdSimulationSubsystem이 없다. 목적지 결정이 연결되지 않는다"));
		return;
	}

	if (Crowd->OnPartyNeedsDecision.IsBound())
	{
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("이미 다른 결정자가 물려 있다. 덮어쓴다 — 결정 권한이 둘이면 안 된다"));
	}

	Crowd->OnPartyNeedsDecision.BindUObject(this, &UFF_CrowdDecisionSubsystem::HandlePartyNeedsDecision);

	const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get();
	CachedCoefficientTable = Settings ? Settings->UtilityCoefficientTable.LoadSynchronous() : nullptr;

	if (!CachedCoefficientTable)
	{
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("UtilityCoefficientTable 미설정 — Choose가 코드 기본 계수로 동작한다. ")
			TEXT("동작은 하지만 계수 튜닝 결과가 반영되지 않는다"));
	}

	NoChoiceLeavePatienceRounds = FMath::RoundToInt(FFF_UtilityCoefficientHelpers::GetCoefficient(
		CachedCoefficientTable.Get(), TEXT("no_choice_leave_patience_rounds"), 5.f));
	CachedArchetypeTable = Settings ? Settings->ArchetypeTable.LoadSynchronous() : nullptr;
	CachedCompanionModifierTable = Settings ? Settings->CompanionModifierTable.LoadSynchronous() : nullptr;
	if (!CachedArchetypeTable)
	{
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("ArchetypeTable 미설정 — preference 항이 0으로 동작한다"));
	}
	else if (!CachedCompanionModifierTable)
	{
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("CompanionModifierTable 미설정 — 취향이 아키타입만으로 합성된다 (동반유형 보정 없음)"));
	}
	UE_LOG(LogCrowdDecision, Log, TEXT("목적지 결정 연결 완료 (후보 상한 %d)"), MaxCandidates);
}

void UFF_CrowdDecisionSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		if (UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>())
		{
			if (Crowd->OnPartyNeedsDecision.IsBoundToObject(this))
			{
				Crowd->OnPartyNeedsDecision.Unbind();
			}
		}
	}

	Super::Deinitialize();
}

void UFF_CrowdDecisionSubsystem::SetDecisionEnabled(bool bEnabled)
{
	const UWorld* World = GetWorld();
	UFF_CrowdSimulationSubsystem* Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
	if (!Crowd)
	{
		return;
	}

	if (bEnabled)
	{
		if (!Crowd->OnPartyNeedsDecision.IsBoundToObject(this))
		{
			Crowd->OnPartyNeedsDecision.BindUObject(this, &UFF_CrowdDecisionSubsystem::HandlePartyNeedsDecision);
		}
	}
	else if (Crowd->OnPartyNeedsDecision.IsBoundToObject(this))
	{
		Crowd->OnPartyNeedsDecision.Unbind();
	}

	UE_LOG(LogCrowdDecision, Display,
		TEXT("결정 연결 %s — 이미 이동 중인 파티는 다음 도착부터 반영된다"),
		bEnabled ? TEXT("켜짐") : TEXT("꺼짐 (배회만)"));
}

bool UFF_CrowdDecisionSubsystem::IsDecisionEnabled() const
{
	const UWorld* World = GetWorld();
	const UFF_CrowdSimulationSubsystem* Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;

	return Crowd && Crowd->OnPartyNeedsDecision.IsBoundToObject(this);
}

int32 UFF_CrowdDecisionSubsystem::Debug_OpenAllValidatedFacilities(int32& OutFailedCount)
{
	OutFailedCount = 0;

	UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!ZoneSystem)
	{
		return 0;
	}

	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();

	int32 Opened = 0;

	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		if (Facility.State == EFFZSFacilityState::Active)
		{
			continue;
		}

		FFFZSCommand Command;
		Command.Type = EFFZSCommandType::Open;
		Command.FacilityInstanceId = Facility.FacilityInstanceId;

		Command.CommandId = FGuid::NewGuid();

		Command.TransactionId = FGuid::NewGuid();

		Command.ExpectedWorldRevision = -1;

		const FFFZSCommandResult Result = ZoneSystem->ExecuteCommand(Command);
		if (Result.bSucceeded)
		{
			++Opened;
			continue;
		}

		++OutFailedCount;

		const UEnum* StateEnum = StaticEnum<EFFZSFacilityState>();
		const UEnum* FailureEnum = StaticEnum<EFFZSPlacementFailure>();

		UE_LOG(LogCrowdDecision, Warning,
			TEXT("개장 실패: %s | 상태 %s | 사유 %s | %s"),
			*Facility.FacilityDefinitionId.ToString(),
			StateEnum ? *StateEnum->GetNameStringByValue(static_cast<int64>(Facility.State)) : TEXT("?"),
			FailureEnum ? *FailureEnum->GetNameStringByValue(static_cast<int64>(Result.FailureReason)) : TEXT("?"),
			*Result.Message.ToString());
	}

	return Opened;
}

void UFF_CrowdDecisionSubsystem::EnsureWalkDistanceMatrix(
	const UFFZSWorldSubsystem& ZoneSystem, const FFFZSReadModel& ReadModel)
{
	if (!bEnableWalkDistances)
	{
		if (WalkDistances.GetZoneCount() > 0)
		{
			WalkDistances.Reset();
			WalkDistanceBuiltRevision = -1;
			WalkDistanceMismatchCount = 0;
			WalkDistanceUnauthoredAnchors = 0;
			WalkDistanceProjectionFailures = 0;
			WalkDistanceMaxProjectionDropCm = 0.f;
		}
		return;
	}

	const int64 Revision = ZoneSystem.GetWorldRevision();
	if (Revision == WalkDistanceBuiltRevision)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FFF_WalkDistanceMatrix::FZoneAnchor> Anchors;
	Anchors.Reserve(ReadModel.Zones.Num());

	int32 MissingActors = 0;
	int32 UnauthoredAnchors = 0;
	int32 ProjectionFailures = 0;
	double MaxProjectionDropCm = 0.0;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	const FVector ProjectionExtent(50.0, 50.0, 500.0);

	for (const FFFZSZoneReadModel& Zone : ReadModel.Zones)
	{
		const AFFZSGameplayZone* ZoneActor = ZoneSystem.FindZone(Zone.ZoneId);
		if (!ZoneActor)
		{
			++MissingActors;
			continue;
		}

		FFF_WalkDistanceMatrix::FZoneAnchor Anchor;
		Anchor.ZoneId = Zone.ZoneId;
		Anchor.NeighborZoneIds = Zone.NeighborZoneIds;

		const bool bHasAuthoredAnchor = !Zone.EntryExitAnchors.IsEmpty();
		if (!bHasAuthoredAnchor)
		{
			++UnauthoredAnchors;
		}

		FVector AnchorCm = bHasAuthoredAnchor
			? ZoneActor->GetActorTransform().TransformPosition(Zone.EntryExitAnchors[0].GetLocation())
			: ZoneActor->GetActorLocation();

		if (NavSys)
		{
			FNavLocation Projected;
			if (NavSys->ProjectPointToNavigation(AnchorCm, Projected, ProjectionExtent))
			{
				MaxProjectionDropCm = FMath::Max(MaxProjectionDropCm,
					FMath::Abs(AnchorCm.Z - Projected.Location.Z));
				AnchorCm = Projected.Location;
			}
			else
			{
				++ProjectionFailures;
				UE_LOG(LogCrowdDecision, Warning,
					TEXT("보행거리 행렬: 존 %s의 앵커가 NavMesh에 투영되지 않았다 (%.0f, %.0f, %.0f) — ")
					TEXT("이 존이 걸린 쌍은 직선거리로 폴백한다"),
					*Zone.ZoneId.ToString(), AnchorCm.X, AnchorCm.Y, AnchorCm.Z);
			}
		}

		Anchor.WorldLocationCm = AnchorCm;

		Anchors.Add(MoveTemp(Anchor));
	}

	auto PathLength = [World](const FVector& FromCm, const FVector& ToCm, float& OutLengthCm) -> bool
	{
		double LengthCm = 0.0;
		const ENavigationQueryResult::Type Result =
			UNavigationSystemV1::GetPathLength(World, FromCm, ToCm, LengthCm);

		if (Result != ENavigationQueryResult::Success)
		{
			return false;
		}

		OutLengthCm = static_cast<float>(LengthCm);
		return true;
	};

	FFF_WalkDistanceMatrix::FBuildStats BuildStats;
	WalkDistances.Build(Anchors, PathLength, &BuildStats);

	WalkDistanceBuiltRevision = Revision;
	WalkDistanceMismatchCount = BuildStats.Mismatches.Num();
	WalkDistanceUnauthoredAnchors = UnauthoredAnchors;
	WalkDistanceProjectionFailures = ProjectionFailures;
	WalkDistanceMaxProjectionDropCm = static_cast<float>(MaxProjectionDropCm);

	float MaxWalkCm = 0.f;
	for (const FFF_WalkDistanceMatrix::FZoneAnchor& From : Anchors)
	{
		for (const FFF_WalkDistanceMatrix::FZoneAnchor& To : Anchors)
		{
			float LengthCm = 0.f;
			if (WalkDistances.TryGetWalkDistanceCm(From.ZoneId, To.ZoneId, LengthCm))
			{
				MaxWalkCm = FMath::Max(MaxWalkCm, LengthCm);
			}
		}
	}

	if (BuildStats.FailedPairs > 0 && BuildStats.FailedPairs == BuildStats.QueryCount)
	{
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("보행거리 행렬: 전 쌍(%d)이 실패했다. NavSys %s / 앵커 투영 실패 %d개 / ")
			TEXT("앵커 미저작 %d개 — 결정은 직선거리로 돌아간다"),
			BuildStats.QueryCount,
			NavSys ? TEXT("있음") : TEXT("없음"),
			ProjectionFailures, UnauthoredAnchors);
	}
	else
	{
		UE_LOG(LogCrowdDecision, Log,
			TEXT("보행거리 행렬 재구축(Revision %lld): 존 %d개, 질의 %d회, 실패 %d, 액터 없음 %d, ")
			TEXT("최대 보행거리 %.1fm — distance_ref_meters 재보정 시 이 값을 볼 것"),
			Revision, BuildStats.ZoneCount, BuildStats.QueryCount, BuildStats.FailedPairs, MissingActors,
			MaxWalkCm * 0.01f);
	}

	if (UnauthoredAnchors > 0)
	{
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("보행거리 행렬: 존 %d/%d개가 EntryExitAnchors 미저작 — 존 중심을 대표점으로 썼다. ")
			TEXT("출입구가 아니므로 거리가 실제 동선과 다르다(저작 필요)"),
			UnauthoredAnchors, BuildStats.ZoneCount);
	}

	if (MaxProjectionDropCm > 0.0)
	{
		UE_LOG(LogCrowdDecision, Log,
			TEXT("보행거리 행렬: 앵커 투영 최대 낙차 %.0fcm — 이 값이 크면 존 액터가 지면에서 떠 있다"),
			MaxProjectionDropCm);
	}

	for (const FString& Mismatch : BuildStats.Mismatches)
	{
		UE_LOG(LogCrowdDecision, Warning, TEXT("보행거리 행렬 불일치: %s"), *Mismatch);
	}
}

UFFZSWorldSubsystem* UFF_CrowdDecisionSubsystem::GetZoneSystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
}

const FFFZSReadModel& UFF_CrowdDecisionSubsystem::GetReadModelForThisFrame()
{
	if (CachedReadModelFrame == GFrameCounter)
	{
		return CachedReadModel;
	}

	CachedReadModelFrame = GFrameCounter;

	CachedReadModel = FFFZSReadModel();

	if (const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem())
	{
		CachedReadModel = ZoneSystem->BuildReadModel();
	}

	return CachedReadModel;
}

const TMap<FName, float>& UFF_CrowdDecisionSubsystem::GetPreferenceVector(FName ArchetypeId, FName CompanionType)
{
	const FName CacheKey(*FString::Printf(TEXT("%s|%s"), *ArchetypeId.ToString(), *CompanionType.ToString()));
	if (const TMap<FName, float>* Cached = PreferenceVectorCache.Find(CacheKey))
	{
		return *Cached;
	}
	return PreferenceVectorCache.Add(CacheKey, FFF_PreferenceVectorBuilder::Build(
		ArchetypeId,
		CompanionType,
		1.f,
		CachedArchetypeTable.Get(),
		CachedCompanionModifierTable.Get()));
}
EFF_PartyFallbackAction UFF_CrowdDecisionSubsystem::ResolveFallbackAction(const FFF_PartyDecisionContext& Context)
{
	const float CurrentZoneDensityRatio = 0.f;
	const float PersonalDensityThresholdRatio = 1.f;
	const EFF_NoChoiceAction Action = FFF_UtilityDestinationSelector::ResolveNoChoiceAction(
		Context.RemainingStayMinutes,
		Context.ConsecutiveNoChoiceCount,
		NoChoiceLeavePatienceRounds,
		CurrentZoneDensityRatio,
		PersonalDensityThresholdRatio);
	switch (Action)
	{
	case EFF_NoChoiceAction::Leave:
		if (Context.RemainingStayMinutes <= 0.f) { ++LeaveByStayExpiredCount; }
		else { ++LeaveByNoChoicePatienceCount; }
		return EFF_PartyFallbackAction::Leave;
	case EFF_NoChoiceAction::Relocate:
		++RelocateRequestedCount;
		return EFF_PartyFallbackAction::Relocate;
	default:
		return EFF_PartyFallbackAction::Wander;
	}
}
bool UFF_CrowdDecisionSubsystem::HandlePartyNeedsDecision(
	const FFF_PartyDecisionContext& Context, FFF_PartyDecisionResult& OutResult)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdDecision_Decide);

	++Stats.Requests;

	OutResult.FallbackAction = ResolveFallbackAction(Context);
	if (DecisionMode == EFF_DecisionMode::Random)
	{
		++Stats.NoChoice;
		return false;
	}
	const FFFZSReadModel& ReadModel = GetReadModelForThisFrame();

	if (ReadModel.Facilities.IsEmpty())
	{
		++Stats.NoFacility;
		return false;
	}

	const bool bAnyOpenFacility = ReadModel.Facilities.ContainsByPredicate(
		[](const FFFZSFacilityReadModel& Facility)
		{
			return Facility.State == EFFZSFacilityState::Active && !Facility.bNeedsRelocation;
		});

	if (!bAnyOpenFacility)
	{
		++Stats.NoOpenFacility;
		return false;
	}

	const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem();
	if (!ZoneSystem)
	{
		++Stats.NoZoneSystem;
		return false;
	}

	const FName CurrentZoneId = ZoneSystem->ResolveZoneAtWorldLocation(Context.PartyWorldLocation);
	if (CurrentZoneId.IsNone())
	{
		++Stats.OutsideZone;
		return false;
	}

	EnsureWalkDistanceMatrix(*ZoneSystem, ReadModel);

	const FFF_WalkDistanceMatrix* WalkDistancesOrNull =
		(bEnableWalkDistances && WalkDistances.IsValid()) ? &WalkDistances : nullptr;

	TArray<FFF_DestinationCandidate> Candidates = FFF_ZoneSystemBridge::BuildCandidates(
		ReadModel, CurrentZoneId, Context.PartyWorldLocation, MaxCandidates, WalkDistancesOrNull);

	if (Candidates.IsEmpty())
	{
		++Stats.NoCandidate;
		return false;
	}

	static const TMap<FName, float> EmptyPreferenceVector;

	const bool bFullMode = (DecisionMode == EFF_DecisionMode::Full);
	if (bFullMode && Context.ArchetypeId.IsNone() && !bWarnedNoPreferenceVector)
	{
		bWarnedNoPreferenceVector = true;
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("파티 %d의 ArchetypeId가 NAME_None이다 — preference 항이 0으로 동작한다. ")
			TEXT("ArchetypeTable 설정을 확인할 것"), Context.PartyId);
	}
	if (!bFullMode)
	{
		for (FFF_DestinationCandidate& Candidate : Candidates)
		{
			Candidate.CurrentOccupancy = 0.f;
			Candidate.EstimatedWaitMinutes = 0.f;
		}
	}
	else
	{
		Context.VisitHistory.ApplyToCandidates(Candidates, Context.NowLiveMinutes);
	}
	const TMap<FName, float>& PreferenceVector = bFullMode
		? GetPreferenceVector(
			Context.ArchetypeId,
			FFF_VisitorDistributionBuilder::CompanionSizeTagForGroupSize(Context.PartySize))
		: EmptyPreferenceVector;

	const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get();
	const int32 GlobalSeed = Settings ? Settings->GlobalSimulationSeed : 0;

	const FFF_DestinationDecision Decision = FFF_UtilityDestinationSelector::Choose(
		Candidates,
		PreferenceVector,
		CachedCoefficientTable.Get(),
		GlobalSeed,
		Context.StableAgentId,
		Context.ChoiceRoundIndex,
		bFullMode ? Context.Needs : FFF_NeedState(),
		bFullMode ? Context.ConsecutiveNoChoiceCount : 0);

	if (Decision.bIsNoChoice || Decision.ChosenDestinationId.IsNone())
	{
		++Stats.NoChoice;
		OutResult.bCountAsNoChoice = true;
		FFF_PartyDecisionContext Bumped = Context;
		Bumped.ConsecutiveNoChoiceCount = Context.ConsecutiveNoChoiceCount + 1;
		OutResult.FallbackAction = ResolveFallbackAction(Bumped);
		return false;
	}

	FGuid FacilityInstanceId;
	if (!FFF_ZoneSystemBridge::TryParseDestinationId(Decision.ChosenDestinationId, FacilityInstanceId))
	{
		++Stats.ParseFailed;
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("DestinationId '%s'를 FGuid로 파싱하지 못했다 (파티 %d)"),
			*Decision.ChosenDestinationId.ToString(), Context.PartyId);
		return false;
	}

	const FFFZSFacilityReadModel* Facility = ReadModel.Facilities.FindByPredicate(
		[&FacilityInstanceId](const FFFZSFacilityReadModel& Candidate)
		{
			return Candidate.FacilityInstanceId == FacilityInstanceId;
		});

	if (!Facility)
	{
		++Stats.FacilityMissing;
		return false;
	}

	if (!Facility->bHasApproachWorldTransform)
	{
		++Stats.InvalidApproach;
		UE_LOG(LogCrowdDecision, Warning,
			TEXT("시설 %s의 Entry[0] 접근점이 파생되지 않았다 — FacilityDefinition 확인 필요"),
			*Facility->FacilityDefinitionId.ToString());
		return false;
	}

	OutResult.Destination = Facility->ApproachWorldTransform.GetLocation();
	OutResult.DestinationId = Decision.ChosenDestinationId;

	++Stats.Chosen;

	UE_LOG(LogCrowdDecision, Verbose,
		TEXT("파티 %d (개체 %d, 회차 %d, 사유 %d) → %s, 후보 %d개"),
		Context.PartyId, Context.StableAgentId, Context.ChoiceRoundIndex,
		static_cast<int32>(Context.Reason),
		*Facility->FacilityDefinitionId.ToString(), Candidates.Num());

	return true;
}

namespace FFDecisionConsole
{
	UFF_CrowdDecisionSubsystem* GetDecision(UWorld* World, FOutputDevice& Ar)
	{
		UFF_CrowdDecisionSubsystem* Decision = World ? World->GetSubsystem<UFF_CrowdDecisionSubsystem>() : nullptr;
		if (!Decision)
		{
			Ar.Logf(TEXT("CrowdDecisionSubsystem이 없다 (PIE가 실행 중인지 확인)"));
		}
		return Decision;
	}
}

static void DecisionStatsCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdDecisionSubsystem* Decision = FFDecisionConsole::GetDecision(World, Ar);
	if (!Decision)
	{
		return;
	}

	if (Args.Num() > 0 && Args[0].Equals(TEXT("reset"), ESearchCase::IgnoreCase))
	{
		Decision->ResetStats();
		Ar.Logf(TEXT("통계를 초기화했다"));
		return;
	}

	const FFF_CrowdDecisionStats& S = Decision->GetStats();

	Ar.Logf(TEXT("--- 목적지 결정 ---"));
	Ar.Logf(TEXT("연결             : %s"),
		Decision->IsDecisionEnabled() ? TEXT("켜짐") : TEXT("꺼짐 — 배회만 한다"));
	Ar.Logf(TEXT("요청             : %d"), S.Requests);
	Ar.Logf(TEXT("시설 좌표 지급   : %d   <- 0이면 Utility가 한 번도 안 먹었다"), S.Chosen);
	Ar.Logf(TEXT("[폴백] 시설 없음     : %d   (인게임에 짓기 전에는 정상)"), S.NoFacility);
	Ar.Logf(TEXT("[폴백] 개장 안 됨    : %d   <- FF.Decision.OpenFacilities"), S.NoOpenFacility);
	Ar.Logf(TEXT("[폴백] Zone 밖       : %d   (Zone 경계 밖에 스폰됐다)"), S.OutsideZone);
	Ar.Logf(TEXT("[폴백] 후보 0개      : %d   (인접 Zone에 열린 시설이 없다)"), S.NoCandidate);
	Ar.Logf(TEXT("[폴백] no_choice     : %d   (Choose의 정상 판단)"), S.NoChoice);
	Ar.Logf(TEXT("[문제] ZoneSystem 없음: %d"), S.NoZoneSystem);
	Ar.Logf(TEXT("[문제] GUID 파싱 실패 : %d"), S.ParseFailed);
	Ar.Logf(TEXT("[문제] 시설 소실      : %d"), S.FacilityMissing);
	Ar.Logf(TEXT("[문제] 접근점 무효    : %d   (FacilityDefinition의 Entry[0] 확인)"), S.InvalidApproach);
	Ar.Logf(TEXT("--- 퇴장 판정 ---"));
	Ar.Logf(TEXT("퇴장: 체류 소진    : %d   (주 원인이어야 한다)"), Decision->GetLeaveByStayExpiredCount());
	Ar.Logf(TEXT("퇴장: 인내 한계    : %d   (연속 no_choice %d회. 체류 소진보다 많으면 인원 보존 위험)"),
		Decision->GetLeaveByNoChoicePatienceCount(), Decision->GetNoChoiceLeavePatienceRounds());
	Ar.Logf(TEXT("Relocate 요청      : %d   (v0에서는 배회로 처리된다)"), Decision->GetRelocateRequestedCount());

	Ar.Logf(TEXT("--- 거리 척도 ---"));
	if (!Decision->bEnableWalkDistances)
	{
		Ar.Logf(TEXT("직선거리 (보행거리 꺼짐)"));
		Ar.Logf(TEXT("  켜기 전에 distance_ref_meters 재보정이 선행 조건이다 —"));
		Ar.Logf(TEXT("  보행거리는 직선보다 크므로 정규화가 포화되어 먼 시설의 거리 항이 전부 같아진다"));
	}
	else
	{
		const int32 ZoneCount = Decision->GetWalkDistanceZoneCount();
		Ar.Logf(TEXT("존 앵커 보행거리 (존 %d개)%s"), ZoneCount,
			(ZoneCount < 2) ? TEXT("  <- 2개 미만이라 직선거리로 돌아간다") : TEXT(""));

		const int32 Unauthored = Decision->GetUnauthoredAnchorCount();
		const int32 ProjFailures = Decision->GetAnchorProjectionFailureCount();

		if (Unauthored > 0)
		{
			Ar.Logf(ELogVerbosity::Warning,
				TEXT("  앵커 미저작   : %d개 — 존 중심을 대표점으로 썼다(출입구가 아니다). ")
				TEXT("이 상태의 최대 보행거리로 재보정하면 값이 어긋난다"), Unauthored);
		}
		if (ProjFailures > 0)
		{
			Ar.Logf(ELogVerbosity::Warning,
				TEXT("  투영 실패     : %d개 — 그 존이 걸린 쌍은 직선거리로 폴백한다"), ProjFailures);
		}
		Ar.Logf(TEXT("  앵커 투영 낙차: 최대 %.0fcm (0이면 앵커가 이미 NavMesh 위)"),
			Decision->GetMaxAnchorProjectionDropCm());

		const int32 Mismatches = Decision->GetWalkDistanceMismatchCount();
		if (Mismatches > 0)
		{
			Ar.Logf(ELogVerbosity::Warning,
				TEXT("  불일치 %d건 — 존 그래프와 NavMesh 연결성이 어긋났다. 상세는 LogCrowdDecision 로그"),
				Mismatches);
		}
		else
		{
			Ar.Logf(TEXT("  존 그래프 ↔ NavMesh 연결성 일치"));
		}
	}
}

static void DecisionWalkDistanceCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdDecisionSubsystem* Decision = World ? World->GetSubsystem<UFF_CrowdDecisionSubsystem>() : nullptr;
	if (!Decision)
	{
		Ar.Logf(ELogVerbosity::Error, TEXT("CrowdDecisionSubsystem을 찾을 수 없습니다"));
		return;
	}

	if (Args.Num() > 0)
	{
		Decision->bEnableWalkDistances = (FCString::Atoi(*Args[0]) != 0);
	}

	Ar.Logf(TEXT("거리 척도: %s"), Decision->bEnableWalkDistances
		? TEXT("존 앵커 보행거리") : TEXT("직선거리"));
	Ar.Logf(TEXT("행렬은 다음 결정에서 지어진다(WorldRevision이 바뀔 때만 재구축). 상세는 FF.Decision.Stats"));

	Ar.Logf(TEXT("이미 이동 중인 파티는 그대로다. 다음 재평가부터 적용된다"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GDecisionWalkDistanceCommand(
	TEXT("FF.Decision.WalkDistance"),
	TEXT("후보 거리를 직선/보행거리로 전환한다. FF.Decision.WalkDistance [0|1]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DecisionWalkDistanceCommand));

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GDecisionStatsCommand(
	TEXT("FF.Decision.Stats"),
	TEXT("목적지 결정이 어디서 끝났는지 센다. FF.Decision.Stats [reset]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DecisionStatsCommand));

static void DecisionBenchCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdDecisionSubsystem* Decision = FFDecisionConsole::GetDecision(World, Ar);
	if (!Decision || !World)
	{
		return;
	}
	const int32 Mode = (Args.Num() > 0) ? FMath::Clamp(FCString::Atoi(*Args[0]), 0, 2) : 2;
	const float WaitSeconds = (Args.Num() > 1) ? FMath::Max(FCString::Atof(*Args[1]), 1.f) : 60.f;
	Decision->DecisionMode = static_cast<EFF_DecisionMode>(Mode);
	Decision->ResetStats();
	Ar.Logf(TEXT("[벤치] 모드 %d로 %.0f초 측정 시작"), Mode, WaitSeconds);
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Decision,
		[World, Decision, Mode, WaitSeconds]()
		{
			const UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
			const FFF_CrowdDecisionStats& S = Decision->GetStats();
			UE_LOG(LogCrowdDecision, Log, TEXT("[벤치결과] 모드=%d 대기=%.0fs"), Mode, WaitSeconds);
			if (Crowd)
			{
				UE_LOG(LogCrowdDecision, Log,
					TEXT("[벤치결과] 활성=%d 최대밀도=%.3f 혼잡셀=%d"),
					Crowd->GetActiveAgentCount(), Crowd->GetMaxDensity(), Crowd->GetCongestedCellCount());
				float HungerSum = 0.f;
				float RestroomSum = 0.f;
				int32 Counted = 0;
				for (int32 i = 0; i < Crowd->GetAgentPoolSize(); ++i)
				{
					if (!Crowd->IsAgentActive(i)) { continue; }
					const FFF_NeedState N = Crowd->GetAgentNeeds(i);
					HungerSum += N.Hunger;
					RestroomSum += N.Restroom;
					++Counted;
				}
				if (Counted > 0)
				{
					UE_LOG(LogCrowdDecision, Log,
						TEXT("[벤치결과] 경과분=%.1f Hunger평균=%.4f Restroom평균=%.4f"),
						Crowd->GetLiveMinutes(), HungerSum / Counted, RestroomSum / Counted);
				}
			}
			UE_LOG(LogCrowdDecision, Log,
				TEXT("[벤치결과] 요청=%d 지급=%d | no_choice=%d 후보0=%d 시설없음=%d 개장안됨=%d ")
				TEXT("Zone밖=%d ZoneSys없음=%d 파싱실패=%d 시설소실=%d 접근점없음=%d"),
				S.Requests, S.Chosen, S.NoChoice, S.NoCandidate, S.NoFacility, S.NoOpenFacility,
				S.OutsideZone, S.NoZoneSystem, S.ParseFailed, S.FacilityMissing, S.InvalidApproach);
			UE_LOG(LogCrowdDecision, Log,
				TEXT("[벤치결과] 퇴장판정: 체류소진=%d 인내한계=%d(한계 %d회) | Relocate요청=%d"),
				Decision->GetLeaveByStayExpiredCount(), Decision->GetLeaveByNoChoicePatienceCount(),
				Decision->GetNoChoiceLeavePatienceRounds(), Decision->GetRelocateRequestedCount());
			if (Crowd)
			{
				UE_LOG(LogCrowdDecision, Log,
					TEXT("[벤치결과] 출구도착=%d 퇴장중=%d 강제퇴장=%d"),
					Crowd->GetLeftPartyCount(), Crowd->GetLeavingPartyCount(), Crowd->GetForcedLeaveCount());
			}
			if (GIsEditor)
			{
				UE_LOG(LogCrowdDecision, Log,
					TEXT("[벤치결과] 에디터라 종료하지 않는다 — 다음 조건은 FF.Decision.Bench로 이어서 부를 것"));
			}
			else
			{
				FPlatformMisc::RequestExit(false);
			}
		}), WaitSeconds, false);
}
static FAutoConsoleCommandWithWorldArgsAndOutputDevice GDecisionBenchCommand(
	TEXT("FF.Decision.Bench"),
	TEXT("모드를 정해 일정 시간 돌린 뒤 밀도·계수기를 찍고 종료한다. FF.Decision.Bench [모드=2] [초=60]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DecisionBenchCommand));
static void DecisionModeCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdDecisionSubsystem* Decision = FFDecisionConsole::GetDecision(World, Ar);
	if (!Decision)
	{
		return;
	}
	static const TCHAR* ModeNames[] = {
		TEXT("0 Random — 전원 배회"),
		TEXT("1 Greedy — attraction + distance만"),
		TEXT("2 Full — 6항 전부")
	};
	if (Args.Num() > 0)
	{
		const int32 Requested = FMath::Clamp(FCString::Atoi(*Args[0]), 0, 2);
		Decision->DecisionMode = static_cast<EFF_DecisionMode>(Requested);
		Decision->ResetStats();
		Ar.Logf(TEXT("결정 모드: %s (계수기 초기화됨)"), ModeNames[Requested]);
		Ar.Logf(TEXT("이미 이동 중인 파티는 다음 도착부터 반영된다 — 밀도를 재기 전에 기다릴 것"));
		return;
	}
	Ar.Logf(TEXT("현재 모드: %s"), ModeNames[static_cast<int32>(Decision->DecisionMode)]);
	Ar.Logf(TEXT("FF.Decision.Mode [0|1|2]"));
}
static FAutoConsoleCommandWithWorldArgsAndOutputDevice GDecisionModeCommand(
	TEXT("FF.Decision.Mode"),
	TEXT("결정에 넣을 입력을 고른다(3종 밀도 지도 비교). FF.Decision.Mode [0=랜덤|1=그리디|2=풀]"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DecisionModeCommand));
static void DecisionEnableCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	UFF_CrowdDecisionSubsystem* Decision = FFDecisionConsole::GetDecision(World, Ar);
	if (!Decision)
	{
		return;
	}

	const bool bEnable = (Args.Num() > 0)
		? (Args[0] != TEXT("0"))
		: !Decision->IsDecisionEnabled();

	Decision->SetDecisionEnabled(bEnable);

	Ar.Logf(TEXT("결정 연결 %s — 이미 이동 중인 파티는 다음 도착부터 반영된다"),
		bEnable ? TEXT("켜짐") : TEXT("꺼짐 (배회만)"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GDecisionEnableCommand(
	TEXT("FF.Decision.Enable"),
	TEXT("목적지 결정 연결을 켜고 끈다(대조군 만들기). FF.Decision.Enable [0|1], 인자 없으면 토글"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&DecisionEnableCommand));

namespace
{
	void OpenAllFacilities(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
	{
		UFF_CrowdDecisionSubsystem* Decision = FFDecisionConsole::GetDecision(World, Ar);
		if (!Decision)
		{
			return;
		}

		int32 Failed = 0;
		const int32 Opened = Decision->Debug_OpenAllValidatedFacilities(Failed);

		if (Opened == 0 && Failed == 0)
		{
			Ar.Logf(TEXT("개장할 시설이 없다 - 등록된 시설이 없거나 이미 전부 Active다"));
			return;
		}

		Ar.Logf(TEXT("시설 개장: 성공 %d / 실패 %d  (실패 사유는 위 Warning 참조)"), Opened, Failed);
	}
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice GOpenAllFacilitiesCommand(
	TEXT("FF.Decision.OpenFacilities"),
	TEXT("Validated 상태 시설을 전부 Active로 개장한다(빌드 모드가 Open을 발행하지 않으므로 필요한 임시 도구)"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&OpenAllFacilities));
