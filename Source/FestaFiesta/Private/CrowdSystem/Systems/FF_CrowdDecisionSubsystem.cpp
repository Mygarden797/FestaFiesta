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
#include "FF_FFZSFacilityActor.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "NPC/Destination/FF_PreferenceVectorBuilder.h"
#include "NPC/Destination/FF_UtilityCoefficientHelpers.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"
#include "NPC/Metrics/FF_SatisfactionMetricsSubsystem.h"
#include "NPC/Party/FF_ProgramReservationReturnTrip.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Engine/GameInstance.h"
#include "NPC/Party/FF_VisitorDistributionBuilder.h"
#include "Queue/FF_QueueWaitAdapter.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCrowdDecision, Log, All);

// 화장실 진단에서 "급하다"로 보는 Restroom 값. 만족도의 못 채움 기준(UFF_NpcNeedMetricsSubsystem 기본 0.7)과 같게 둔다
static constexpr float UrgentRestroomThreshold = 0.7f;

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

	// 공연 시간표 게이팅. 시각의 유일한 출처가 Stage 이므로 여기서 시계를 들지 않는다.
	// Stage 가 없는 월드(크라우드 단독 테스트 맵)에서는 게이팅이 꺼진 채로 돌아간다 —
	// bMainStageGatingActive 가 false 로 남아 IsMainStageOpen() 이 항상 true 다.
	if (UGameInstance* GameInstance = InWorld.GetGameInstance())
	{
		if (UFF_FestivalStageSubsystem* Stage = GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
		{
			Stage->OnStageChanged.AddDynamic(this, &UFF_CrowdDecisionSubsystem::HandleStageChanged);
			Stage->OnMainProgramStarted.AddDynamic(this, &UFF_CrowdDecisionSubsystem::HandleMainProgramStarted);
			Stage->OnMainProgramEnded.AddDynamic(this, &UFF_CrowdDecisionSubsystem::HandleMainProgramEnded);
		}
		else
		{
			UE_LOG(LogCrowdDecision, Warning,
				TEXT("FestivalStageSubsystem이 없다 — 공연 시간표 게이팅이 꺼진다(메인 무대가 상시 후보). ")
				TEXT("Live 후반 무대 쏠림(AT-016)이 재현되지 않는다"));
		}
	}

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

		// Stage 는 GameInstance 스코프라 이 월드보다 오래 산다 — 안 끊으면 다음 월드까지
		// 죽은 this 가 물려 있다.
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UFF_FestivalStageSubsystem* Stage = GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
			{
				Stage->OnStageChanged.RemoveAll(this);
				Stage->OnMainProgramStarted.RemoveAll(this);
				Stage->OnMainProgramEnded.RemoveAll(this);
			}
		}
	}

	Super::Deinitialize();
}

void UFF_CrowdDecisionSubsystem::HandleStageChanged(
	EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage)
{
	if (NewStage == EFF_FestivalStage::Live)
	{
		// 관객은 공연 전부터 와서 기다릴 수 있다. 전역 발견은 공연 시작 이벤트가 별도로 연다.
		bMainStageGatingActive = true;
		bMainStageOpen = true;
		bMainProgramActive = false;
		bSawMainProgramThisLive = false;
		return;
	}

	if (PreviousStage == EFF_FestivalStage::Live)
	{
		if (!bSawMainProgramThisLive)
		{
			// 표에 bIsMainProgramLockStart 행이 없으면 IsMainProgramLocked() 이 영원히 false 라
			// 시작 신호가 안 온다. 그 경우 무대는 Live 내내 후보에서 빠져 있었고, 조용히 두면
			// "왜 아무도 무대에 안 갔는가"의 원인이 로그에 안 남는다.
			UE_LOG(LogCrowdDecision, Warning,
				TEXT("Live 동안 메인 공연 시작 신호가 한 번도 오지 않았다 — 메인 무대가 하루 종일 ")
				TEXT("후보에서 빠져 있었다. DT_ProgramSchedule 에 bIsMainProgramLockStart 행이 있는지 확인할 것"));
		}

		bMainStageGatingActive = false;
		bMainStageOpen = false;
		bMainProgramActive = false;
		bSawMainProgramThisLive = false;
	}
}

bool UFF_CrowdDecisionSubsystem::IsMainStageOpen() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	if (Stage && Stage->IsLiveStage()) return Stage->IsMainProgramQueueBreakActive();
	return !bMainStageGatingActive || bMainStageOpen;
}

bool UFF_CrowdDecisionSubsystem::IsGlobalMainStageDiscoveryActive() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	return Stage && Stage->IsLiveStage() && Stage->IsMainProgramQueueBreakActive();
}

void UFF_CrowdDecisionSubsystem::HandleMainProgramStarted(FString EventLabel)
{
	bSawMainProgramThisLive = true;
	bMainStageOpen = true;
	bMainProgramActive = true;
	// 이 알림은 배속 잠금이다. 시설 이용을 끝내는 공연 3분 전까지 재집결하지 않는다.
	if (!IsGlobalMainStageDiscoveryActive()) return;

	// 무대를 여는 것만으로는 아무도 모른다. 밀어 넣지 않으면 회전율만큼 서서히 차기만 하고
	// "공연 시간에 몰린다"가 안 나온다.
	//
	// 개인 주기 재평가(EFF_DecisionReason::Reevaluated)가 붙은 뒤에도 이 밀어넣기는 필요하다 —
	// 그쪽 주기는 reevaluate_interval_seconds(기본 120초) 단위라, 무대가 열린 순간에 맞춰
	// 반응하지 않는다. 개장은 시각이 정해진 사건이므로 그 시각에 알려야 한다.
	const UWorld* World = GetWorld();
	UFF_CrowdSimulationSubsystem* Crowd =
		World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;

	const int32 PendingBefore = Crowd ? Crowd->GetPendingDecisionCount() : 0;
	const int32 AttemptedCount = Crowd
		? Crowd->RequestRedecisionForIdleParties(MaxPartiesRedecidedOnStageOpen)
		: 0;
	const int32 NewlyQueuedCount = Crowd ? Crowd->GetPendingDecisionCount() - PendingBefore : 0;

	UE_LOG(LogCrowdDecision, Log,
		TEXT("메인 공연 시작('%s') — 전역 메인 무대 후보를 열고 재결정 %d개 시도, %d개 새로 대기. ")
		TEXT("요청 상한 %d(0=전체), 프레임당 처리 %d. 대기열·예약·퇴장 보호는 유지한다"),
		*EventLabel, AttemptedCount, NewlyQueuedCount, MaxPartiesRedecidedOnStageOpen,
		Crowd ? Crowd->MaxDecisionsPerFrame : 0);
}

void UFF_CrowdDecisionSubsystem::HandleMainProgramEnded(FString EventLabel)
{
	bMainProgramActive = false;

	UE_LOG(LogCrowdDecision, Log,
		TEXT("메인 공연 종료('%s') — 전역 발견만 닫고 무대는 Live 동안 계속 열어 둔다"), *EventLabel);
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
			WalkDistanceNavBuildDeferrals = 0;
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

	// 시설 nav 구멍은 dynamic modifier라 리비전이 오른 **몇 프레임 뒤에** 구워진다.
	// 여기서 기다리지 않으면 구멍이 없는 NavMesh로 거리를 재고, WalkDistanceBuiltRevision을
	// 찍어 버리므로 다음 리비전까지 그 틀린 값이 고정된다 — 강 건너편이 가깝게 나오는 종류의
	// 조용한 오류다. 리비전을 기록하지 않고 빠지면 다음 결정 요청에서 다시 시도한다.
	//
	// 그동안 후보 거리는 직전 리비전의 행렬(없으면 직선거리)로 계산된다. 한쪽은 몇 프레임 낡고
	// 다른 한쪽은 영구히 틀리므로, 낡은 쪽을 고른다.
	if (NavSys && NavSys->GetNumRemainingBuildTasks() > 0)
	{
		++WalkDistanceNavBuildDeferrals;
		return;
	}

	const FVector ProjectionExtent = FFF_CrowdNav::GetAnchorProjectionExtentCm();

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
	// 배수는 1 로 굳혀 둔다 — 개체 변주는 GetScaledPreferenceVector 가 얹는다.
	// 여기에 개체 배수를 넣으면 캐시 키가 개체 수만큼 늘어난다.
	return PreferenceVectorCache.Add(CacheKey, FFF_PreferenceVectorBuilder::Build(
		ArchetypeId,
		CompanionType,
		1.f,
		CachedArchetypeTable.Get(),
		CachedCompanionModifierTable.Get()));
}

const TMap<FName, float>& UFF_CrowdDecisionSubsystem::GetScaledPreferenceVector(
	FName ArchetypeId, FName CompanionType, float PreferenceStrengthMultiplier)
{
	const TMap<FName, float>& BaseVector = GetPreferenceVector(ArchetypeId, CompanionType);

	// 음수 배수는 "싫어하는 것을 좋아한다"가 되어 의도와 반대다. 호출부 버그로 보고 0 으로 막는다
	// (FFF_UtilityDestinationSelector 의 novelty 배수와 같은 방침).
	const float SafeMultiplier = FMath::Max(PreferenceStrengthMultiplier, 0.f);

	if (FMath::IsNearlyEqual(SafeMultiplier, 1.f))
	{
		return BaseVector;
	}

	ScaledPreferenceVector.Reset();
	ScaledPreferenceVector.Reserve(BaseVector.Num());
	for (const TPair<FName, float>& Entry : BaseVector)
	{
		ScaledPreferenceVector.Add(Entry.Key, Entry.Value * SafeMultiplier);
	}
	return ScaledPreferenceVector;
}
EFF_PartyFallbackAction UFF_CrowdDecisionSubsystem::ResolveFallbackAction(const FFF_PartyDecisionContext& Context)
{
	// 예약 중 후보 부족·체류 예산 소진은 퇴장 사유가 아니다. 복귀 마감은 Simulation이 감시한다.
	if (Context.bHasActiveReservation)
	{
		return EFF_PartyFallbackAction::Wander;
	}
	// 지금 서 있는 Zone 의 밀집도. Relocate 판정의 유일한 입력이라 이 값이 0 이면
	// no_choice 는 영원히 Wander 로만 떨어지고 crowd_tolerance_multiplier 도 쓰이지 않는다.
	//
	// ReadModel 은 프레임 단위로 캐시되므로(GetReadModelForThisFrame) 결정 하나마다
	// ZoneSystem 을 새로 긁지 않는다. Zone 을 못 찾으면 0 — 축제장 밖이라는 뜻이고,
	// 그 경우 밀집을 근거로 파티를 옮길 수 없다.
	float CurrentZoneDensityRatio = 0.f;
	if (const UFFZSWorldSubsystem* ZoneSystem = GetZoneSystem())
	{
		const FName CurrentZoneId = ZoneSystem->ResolveZoneAtWorldLocation(Context.PartyWorldLocation);
		if (!CurrentZoneId.IsNone())
		{
			const FFFZSReadModel& ReadModel = GetReadModelForThisFrame();
			if (const FFFZSZoneReadModel* CurrentZone = ReadModel.Zones.FindByPredicate(
				[CurrentZoneId](const FFFZSZoneReadModel& Zone) { return Zone.ZoneId == CurrentZoneId; }))
			{
				CurrentZoneDensityRatio = CurrentZone->DensityRatio;
			}
		}
	}

	// Crowd 가 스폰 때 density_threshold_ratio × crowd_tolerance_multiplier 를 곱해 둔 값이다.
	// 여기서 다시 곱하지 않는다 — 변주가 두 번 걸린다.
	const EFF_NoChoiceAction Action = FFF_UtilityDestinationSelector::ResolveNoChoiceAction(
		Context.RemainingStayMinutes,
		Context.ConsecutiveNoChoiceCount,
		NoChoiceLeavePatienceRounds,
		CurrentZoneDensityRatio,
		Context.PersonalDensityThresholdRatio);
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
FName UFF_CrowdDecisionSubsystem::FindNearestZoneId(
	const UFFZSWorldSubsystem& ZoneSystem, const FFFZSReadModel& ReadModel, const FVector& WorldLocation)
{
	FName Nearest = NAME_None;
	double BestDistSq = FMath::Square(static_cast<double>(MaxOutsideZoneFallbackMeters) * 100.0);
	for (const FFFZSZoneReadModel& Zone : ReadModel.Zones)
	{
		const AFFZSGameplayZone* ZoneActor = ZoneSystem.FindZone(Zone.ZoneId);
		if (!ZoneActor)
		{
			continue;
		}
		const double DistSq = FVector::DistSquared2D(ZoneActor->GetActorLocation(), WorldLocation);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Nearest = Zone.ZoneId;
		}
	}
	return Nearest;
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

	FName CurrentZoneId = ZoneSystem->ResolveZoneAtWorldLocation(Context.PartyWorldLocation);
	if (CurrentZoneId.IsNone())
	{
		// 입구(3번 게이트 등)나 존 사이 길은 어느 존에도 속하지 않는다. 여기서 결정을 포기하면
		// 그 파티는 배회만 하다가 존 안으로 우연히 들어설 때까지 시설을 못 본다 — 3일 시나리오에서
		// 결정 요청의 45% 가 이 경우였다. 가까운 존을 현재 존으로 보고 후보를 찾는다.
		CurrentZoneId = FindNearestZoneId(*ZoneSystem, ReadModel, Context.PartyWorldLocation);
		if (CurrentZoneId.IsNone())
		{
			++Stats.OutsideZone;
			return false;
		}
		++Stats.OutsideZoneUsedNearest;
	}

	EnsureWalkDistanceMatrix(*ZoneSystem, ReadModel);

	const FFF_WalkDistanceMatrix* WalkDistancesOrNull =
		(bEnableWalkDistances && WalkDistances.IsValid()) ? &WalkDistances : nullptr;

	TArray<FFF_DestinationCandidate> Candidates = FFF_ZoneSystemBridge::BuildCandidates(
		ReadModel, CurrentZoneId, Context.PartyWorldLocation, MaxCandidates, WalkDistancesOrNull,
		IsMainStageOpen(), IsGlobalMainStageDiscoveryActive());
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFF_ProgramReservationSubsystem* Reservations = GameInstance
		? GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	const bool bMainProgramQueueBreak = Stage && Stage->IsMainProgramQueueBreakActive();
	if (bMainProgramQueueBreak)
	{
		const UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge =
			GetWorld()->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>();
		Candidates.RemoveAll([ZoneSystem, QueueBridge](const FFF_DestinationCandidate& Candidate)
		{
			FGuid FacilityId;
			if (!QueueBridge || !FFF_ZoneSystemBridge::TryParseDestinationId(Candidate.DestinationId, FacilityId)
				|| QueueBridge->FindQueueFacilityId(FacilityId) == INDEX_NONE) return false;
			const AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(FacilityId);
			// 공연 관람은 일반 FIFO 대기열을 사용하지 않는다. 화장실은 공연 중에도 연다
			return !Facility || (!Facility->GetFacilitySpec().Audience.bEnabled
				&& UFF_ZoneSystemQueueBridgeSubsystem::PausesForMainProgram(Facility->GetFacilitySpec()));
		});
	}
	if (Reservations)
	{
		Candidates.RemoveAll([Reservations](const FFF_DestinationCandidate& Candidate)
		{
			FGuid FacilityId;
			return FFF_ZoneSystemBridge::TryParseDestinationId(Candidate.DestinationId, FacilityId)
				&& Reservations->IsProgramFacility(FacilityId); // 당일 프로그램이 편성된 시설이면 일반 목적지 후보에서 제거합니다.
		});
	}

	if (Candidates.IsEmpty() && !Context.bHasActiveReservation)
	{
		++Stats.NoCandidate;
		return false;
	}

	// 실패한 사유로 온 요청만 직전 목적지를 벌한다 — Arrived(이용 완료)는 정상적인 재방문이고
	// revisit 항이 맡는다. bFullMode 분기 밖에 두는 이유: 혼잡·대기는 모드에 따라 지우는
	// 추정값이지만 "방금 여기 못 갔다"는 사실이다.
	//
	// QueuePatienceExpired 가 여기 있는 이유: 인내심이 짧은 개체는 줄을 떠난 직후 다시 고르는데,
	// 그 시설의 대기시간 추정은 방금 떠난 그 줄 그대로다. 감점하지 않으면 같은 줄에 들어가
	// 또 나오는 왕복이 인내심이 짧을수록 심해진다 — 변주를 넣은 결과가 버그처럼 보이게 된다.
	// Reevaluated 는 넣지 않는다. 그쪽은 실패가 아니라 "다시 보자"이고, 감점하면 재평가할수록
	// 원래 가려던 곳이 싫어져 주기가 짧은 개체가 목적지를 계속 갈아치운다.
	if ((Context.Reason == EFF_DecisionReason::MoveFailed
			|| Context.Reason == EFF_DecisionReason::JoinRejected
			|| Context.Reason == EFF_DecisionReason::QueuePatienceExpired)
		&& !Context.PreviousDestinationId.IsNone())
	{
		for (FFF_DestinationCandidate& Candidate : Candidates)
		{
			if (Candidate.DestinationId == Context.PreviousDestinationId)
			{
				Candidate.bFailedRecently = true;
				++Stats.FailedDestinationPenalized;
			}
		}
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
			Candidate.InTransitMemberCount = 0.f;
		}
	}
	else
	{
		Context.VisitHistory.ApplyToCandidates(Candidates, Context.NowLiveMinutes);

		// 대기시간을 채운다.
		// 이 호출이 없으면 EstimatedWaitMinutes 는 전부 0 이고, 0 은 Choose 에서 "줄이 없다"로 
		// 계산되어 wait_cost 항이 통째로 죽는다.
		//
		// 멤버 수는 가입할 일행이 유효한지 알리는 입력이다. 양수면 한 팀으로 계산한다.
		// 예상 대기는 앞선 팀 수·동시 처리 팀 수·진행 중인 이용의 남은 시간으로 정하며,
		// 큰 팀이라고 기다리는 시간을 다시 인원수 배로 늘리지 않는다.
		Stats.QueueWaitApplied += FFF_QueueWaitAdapter::ApplyFromZoneBridge(
			GetWorld(), Candidates, Context.PartySize);

		// 아직 줄에 반영되지 않은 수요. 널이면 이 항은 0 으로 남는다(결선 전과 동일 동작).
		// Crowd 가 결정 배치 안에서 누적해 주므로 같은 프레임에 결정하는 파티들이 서로를 본다.
		if (Context.InTransitMemberCounts)
		{
			for (FFF_DestinationCandidate& Candidate : Candidates)
			{
				Candidate.InTransitMemberCount =
					static_cast<float>(Context.InTransitMemberCounts->FindRef(Candidate.DestinationId));

				if (Candidate.InTransitMemberCount > 0.f)
				{
					++Stats.InTransitApplied;
				}
			}
		}
	}

	// 예약 파티는 지금 들른 시설을 이용한 뒤에도 시작 전에 돌아올 수 있어야 한다.
	// 일반 Queue의 실제 대기시간이 반영된 뒤 판정한다. 전부 탈락해도 마감 전에는 배회한다.
	// 복귀 전환은 후보 유무와 무관하게 Simulation의 고정 스텝이 처리한다.
	/*
	 * [주의]
	 * 예약 시설까지의 복귀 거리는 직선거리입니다.
	 */
	if (Context.bHasActiveReservation)
	{
		const FFFZSFacilityReadModel* ProgramFacility = ReadModel.Facilities.FindByPredicate(
			[&Context](const FFFZSFacilityReadModel& Facility)
			{
				return Facility.FacilityInstanceId == Context.ActiveReservation.FacilityInstanceId;
			});

		if (ProgramFacility && ProgramFacility->bHasApproachWorldTransform)
		{
			const FVector ProgramLocation = ProgramFacility->ApproachWorldTransform.GetLocation();
			Candidates.RemoveAll([&](const FFF_DestinationCandidate& Candidate)
			{
				if (!Candidate.bIsOpen)
				{
					return true;
				}
				FGuid CandidateFacilityId;
				if (!FFF_ZoneSystemBridge::TryParseDestinationId(
					Candidate.DestinationId, CandidateFacilityId))
				{
					return false;
				}
				const FFFZSFacilityReadModel* CandidateFacility = ReadModel.Facilities.FindByPredicate(
					[&CandidateFacilityId](const FFFZSFacilityReadModel& Facility)
					{
						return Facility.FacilityInstanceId == CandidateFacilityId;
					});
				if (!CandidateFacility || !CandidateFacility->bHasApproachWorldTransform)
				{
					return false;
				}

				FFF_ReturnTripInput Input;
				Input.WalkSpeedMetersPerSecond = Context.WalkSpeedMetersPerSecond;
				Input.ReturnBufferMinutes = Context.ProgramReturnBufferMinutes;
				Input.MinutesPerTravelSecond = UFF_FestivalStageSubsystem::StageSecondsToFestivalMinutes(1.f);
				Input.DistanceToCandidateMeters = Candidate.DistanceMeters;
				// Queue 어댑터의 실제 분을 프로그램의 축제 분으로 환산한다.
				Input.CandidateWaitMinutes = FFF_QueueWaitAdapter::ToQueueSeconds(Candidate.EstimatedWaitMinutes)
					* UFF_FestivalStageSubsystem::StageSecondsToFestivalMinutes(1.f);
				// 예상 대기는 이용 시작까지다. 예약 복귀 판단에는 팀의 이용 시간도 한 번 더한다.
				Input.CandidateUseMinutes = FMath::Max(0.f, CandidateFacility->EffectiveQueueRules.ServiceDurationSeconds)
					* UFF_FestivalStageSubsystem::StageSecondsToFestivalMinutes(1.f);
				Input.CandidateToProgramMeters = FVector::Dist2D(
					CandidateFacility->ApproachWorldTransform.GetLocation(), ProgramLocation) / 100.f;
				const FFF_ReturnTripVerdict Verdict = FFF_ProgramReservationReturnTrip::Evaluate(
					Context.ActiveReservation, Context.NowLiveMinutes, Input);
				return Verdict.bWasEvaluated && !Verdict.bCanReturnInTime;
			});

		}
	}
	const TMap<FName, float>& PreferenceVector = bFullMode
		? GetScaledPreferenceVector(
			Context.ArchetypeId,
			FFF_VisitorDistributionBuilder::CompanionSizeTagForGroupSize(Context.PartySize),
			Context.PreferenceStrengthMultiplier)
		: EmptyPreferenceVector;

	const UFF_SimulationSettings* Settings = UFF_SimulationSettings::Get();
	const int32 GlobalSeed = Settings ? Settings->GlobalSimulationSeed : 0;

	// Choose 의 항별 덤프 바로 앞에 개체 정체성을 찍는다. 같은 카테고리의 Preference 값이
	// 개체마다 다를 때 그것이 아키타입 차이인지 취향강도 변주인지 이 줄 없이는 못 가른다.
	//
	// Choose 에 아키타입을 넘겨 그쪽에서 찍지 않는 이유: 선택기는 "이 벡터로 이 점수"라는
	// 순수 대응만 알아야 하고, 로그 한 줄 때문에 순수 함수 시그니처를 넓히지 않는다.
	UE_LOG(LogFestaFiestaNpc, Verbose,
		TEXT("[개체] Agent=%d 파티=%d(%d명) 아키타입=%s 취향강도=%.3f 신규성=%.3f 밀집임계=%.3f"),
		Context.StableAgentId, Context.PartyId, Context.PartySize,
		Context.ArchetypeId.IsNone() ? TEXT("(없음/아동)") : *Context.ArchetypeId.ToString(),
		Context.PreferenceStrengthMultiplier, Context.NoveltySeekingMultiplier,
		Context.PersonalDensityThresholdRatio);

	const FFF_DestinationDecision Decision = FFF_UtilityDestinationSelector::Choose(
		Candidates,
		PreferenceVector,
		CachedCoefficientTable.Get(),
		GlobalSeed,
		Context.StableAgentId,
		Context.ChoiceRoundIndex,
		bFullMode ? Context.Needs : FFF_NeedState(),
		bFullMode ? Context.ConsecutiveNoChoiceCount : 0,
		// Random 모드에서는 1(변주 없음)로 둔다 — 그 모드는 취향·혼잡 추정을 전부 지우고
		// 비교 기준선을 만드는 용도이므로, 개체차가 섞이면 기준선이 아니게 된다.
		bFullMode ? Context.NoveltySeekingMultiplier : 1.f);

	if (bFullMode && Context.Needs.Restroom >= UrgentRestroomThreshold)
	{
		++Stats.UrgentRestroom;
		float NearestToiletMeters = TNumericLimits<float>::Max();
		bool bChoseToilet = false;
		for (const FFF_DestinationCandidate& Candidate : Candidates)
		{
			if (Candidate.NeedRelief.bRelievesRestroom)
			{
				NearestToiletMeters = FMath::Min(NearestToiletMeters, Candidate.DistanceMeters);
				bChoseToilet |= Candidate.DestinationId == Decision.ChosenDestinationId;
			}
		}
		const bool bHadToilet = NearestToiletMeters < TNumericLimits<float>::Max();
		if (bHadToilet)
		{
			++Stats.UrgentRestroomToiletCandidate;
			Stats.UrgentRestroomNearestToiletMetersSum += NearestToiletMeters;
		}
		else
		{
			++Stats.UrgentRestroomNoToiletByZone.FindOrAdd(CurrentZoneId);
		}
		Stats.UrgentRestroomDuringBreak += bMainProgramQueueBreak ? 1 : 0;
		Stats.UrgentRestroomNoToiletDuringBreak += bMainProgramQueueBreak && !bHadToilet ? 1 : 0;
		Stats.UrgentRestroomChoseToilet += bChoseToilet ? 1 : 0;
	}

	if (Decision.bIsNoChoice || Decision.ChosenDestinationId.IsNone())
	{
		++Stats.NoChoice;
		OutResult.bCountAsNoChoice = !Context.bHasActiveReservation;
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

	// 고른 후보의 해소 플래그를 결과에 실어 보낸다. 여기서 GetNeedReliefFlags 를 다시 부르지 않는다 —
	// BuildCandidates 가 이미 채운 값이고, 같은 판정을 두 번 하면 갈릴 자리가 생긴다.
	if (const FFF_DestinationCandidate* Chosen = Candidates.FindByPredicate(
			[&Decision](const FFF_DestinationCandidate& Candidate)
			{
				return Candidate.DestinationId == Decision.ChosenDestinationId;
			}))
	{
		OutResult.NeedRelief = Chosen->NeedRelief;
	}

	++Stats.Chosen;

	// 만족도 취향매칭 입력. no_choice(위 이른 return)나 파싱·시설 조회 실패는 확정된
	// 목적지가 아니므로 표본에 안 들어간다 — 실제로 간 곳만 "취향에 맞았는가"를 잰다.
	// GameInstance는 위(예약 필터 직전)에서 이미 구했다 — 여기서 다시 선언하지 않는다.
	if (GameInstance)
	{
		if (UFF_SatisfactionMetricsSubsystem* Satisfaction = GameInstance->GetSubsystem<UFF_SatisfactionMetricsSubsystem>())
		{
			if (const FFF_DestinationUtilityBreakdown* ChosenScore = Decision.AllCandidateScores.FindByPredicate(
					[&Decision](const FFF_DestinationUtilityBreakdown& Score)
					{
						return Score.DestinationId == Decision.ChosenDestinationId;
					}))
			{
				const float PreferenceRef = FFF_UtilityCoefficientHelpers::GetCoefficient(
					CachedCoefficientTable.Get(), TEXT("satisfaction_preference_term_ref"), 0.6f);
				const float PreferenceNorm = PreferenceRef > 0.f
					? FMath::Clamp(ChosenScore->PreferenceTerm / PreferenceRef, -1.f, 1.f)
					: 0.f;
				Satisfaction->AddPreferenceSample(PreferenceNorm);
			}
		}
	}

	UE_LOG(LogCrowdDecision, Verbose,
		TEXT("파티 %d (개체 %d, 회차 %d, 사유 %d) → %s, 후보 %d개"),
		Context.PartyId, Context.StableAgentId, Context.ChoiceRoundIndex,
		static_cast<int32>(Context.Reason),
		*Facility->FacilityDefinitionId.ToString(), Candidates.Num());

	return true;
}

#if !UE_BUILD_SHIPPING

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
	Ar.Logf(TEXT("[폴백] Zone 밖       : %d   (가까운 Zone 도 없다)"), S.OutsideZone);
	Ar.Logf(TEXT("[보정] Zone 밖→근처 : %d   (입구·길에서 가까운 Zone 으로 결정)"), S.OutsideZoneUsedNearest);
	Ar.Logf(TEXT("[폴백] 후보 0개      : %d   (인접 Zone에 열린 시설이 없다)"), S.NoCandidate);
	Ar.Logf(TEXT("[폴백] no_choice     : %d   (Choose의 정상 판단)"), S.NoChoice);
	Ar.Logf(TEXT("[문제] ZoneSystem 없음: %d"), S.NoZoneSystem);
	Ar.Logf(TEXT("[문제] GUID 파싱 실패 : %d"), S.ParseFailed);
	Ar.Logf(TEXT("[문제] 시설 소실      : %d"), S.FacilityMissing);
	Ar.Logf(TEXT("[문제] 접근점 무효    : %d   (FacilityDefinition의 Entry[0] 확인)"), S.InvalidApproach);
	Ar.Logf(TEXT("실패 목적지 감점 : %d   <- FF.Crowd.Flow의 '막혀서 포기'와 짝이 맞아야 한다"),
		S.FailedDestinationPenalized);
	Ar.Logf(TEXT("이동 중 인원 반영: %d   <- 0이면 Crowd 원장이 안 넘어오거나 Full 모드가 아니다"),
		S.InTransitApplied);
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

		Ar.Logf(TEXT("  nav빌드 대기  : %d회 미룸 (배치 변경 직후 몇 번 오르는 것이 정상)"),
			Decision->GetWalkDistanceNavBuildDeferrals());

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

#endif // !UE_BUILD_SHIPPING
