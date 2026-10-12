// FF_ZoneSystemQueueBridgeSubsystem.cpp

// Zone 시설 상태와 동선을 프로젝트 Party Queue 등록으로 투영하고 두 시스템의 권위를 조정한다.
// 흐름: Zone 이벤트로 등록을 갱신하고 Tick에서 조용한 복원·drain 완료·이중 점유를 보정한다.
// 불변식: Zone GUID별 Queue ID는 안정적으로 유지하며 실제 Party 항목은 한 Queue 구현만 소유한다.
// 실패/성능: 무효 geometry와 권위 충돌은 해당 Queue만 fail-closed하고 이벤트는 Tick 끝에 모아 보낸다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "Zone/FF_ZoneSystemStageBridgeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/ScopeExit.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Queue/FF_QueueSubsystem.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "Time/FF_FestivalStageSubsystem.h"

#define LOCTEXT_NAMESPACE "FFZoneSystemQueueBridge"
DEFINE_LOG_CATEGORY_STATIC(LogFFZoneQueueBridge, Log, All);

void UFF_ZoneSystemQueueBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// 두 권위 subsystem을 먼저 준비한 뒤 이벤트·Actor 세대 감시·외부 정책을 한 수명에 묶는다.
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Collection.InitializeDependency<UFF_QueueSubsystem>();
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	QueueSystem = World ? World->GetSubsystem<UFF_QueueSubsystem>() : nullptr;

	if (!ZoneSystem || !QueueSystem)
	{
		UE_LOG(LogFFZoneQueueBridge, Warning,
			TEXT("ZoneSystem 또는 QueueSubsystem이 없어 시설-대기열 연동을 시작하지 못했습니다."));
		return;
	}

	ZoneSystem->OnWorldChanged.AddUniqueDynamic(
		this, &UFF_ZoneSystemQueueBridgeSubsystem::HandleZoneWorldChanged);
	ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(
			this, &UFF_ZoneSystemQueueBridgeSubsystem::HandleActorSpawned));
	ActorDestroyedHandle = World->AddOnActorDestroyedHandler(
		FOnActorDestroyed::FDelegate::CreateUObject(
			this, &UFF_ZoneSystemQueueBridgeSubsystem::HandleActorDestroyed));
	bPolicyRegistered = ZoneSystem->RegisterExternalPolicy(this);
	if (!bPolicyRegistered)
	{
		UE_LOG(LogFFZoneQueueBridge, Warning,
			TEXT("QueueSubsystem 점유 Policy를 FF_ZoneSystem에 등록하지 못했습니다."));
	}

	RefreshQueueRegistrations();
}

void UFF_ZoneSystemQueueBridgeSubsystem::Deinitialize()
{
	// 브리지가 등록한 handler와 Queue만 해제하고 다른 시스템이 소유한 Queue ID는 건드리지 않는다.
	if (UWorld* World = GetWorld())
	{
		if (ActorSpawnedHandle.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
		}
		if (ActorDestroyedHandle.IsValid())
		{
			World->RemoveOnActorDestroyedHandler(ActorDestroyedHandle);
		}
	}
	ActorSpawnedHandle.Reset();
	ActorDestroyedHandle.Reset();

	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.RemoveDynamic(
			this, &UFF_ZoneSystemQueueBridgeSubsystem::HandleZoneWorldChanged);
		if (bPolicyRegistered)
		{
			ZoneSystem->UnregisterExternalPolicy(this);
		}
	}
	if (QueueSystem)
	{
		for (const int32 QueueFacilityId : ActiveBridgeQueueFacilityIds)
		{
			if (QueueSystem->IsFacilityRegistered(QueueFacilityId))
			{
				QueueSystem->UnregisterFacility(QueueFacilityId);
			}
		}
	}

	bPolicyRegistered = false;
	PendingUpgradeQueues.Reset();
	ZoneToQueueFacilityIds.Reset();
	QueueToZoneFacilityIds.Reset();
	ActiveBridgeQueueFacilityIds.Reset();
	DrainingQueueFacilityIds.Reset();
	ProjectQueueAuthorityFacilityIds.Reset();
	ObservedFacilityActors.Reset();
	NextQueueFacilityId = -2;
	LastObservedWorldRevision = INDEX_NONE;
	bActorGenerationCheckPending = false;
	bQueueEventsNeedFlush = false;
	bMainProgramQueueBreak = false;
	QueueSystem = nullptr;
	ZoneSystem = nullptr;
	Super::Deinitialize();
}

bool UFF_ZoneSystemQueueBridgeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// UFF_QueueSubsystem과 수명 범위를 정확히 맞춘다.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFF_ZoneSystemQueueBridgeSubsystem::Tick(const float DeltaTime)
{
	SynchronizeMainProgramQueueBreak();
	if (bActorGenerationCheckPending)
	{
		bActorGenerationCheckPending = false;
		if (HasObservedWorldChanged())
		{
			// 성공한 Snapshot 복원은 이벤트 없이 Actor 세대를 교체한다. Legacy Queue에는 Snapshot이
			// 없으므로 취소 로그를 남긴 뒤 현재 Zone 투영을 fail-closed 방식으로 다시 만든다.
			RefreshQueueRegistrations();
		}
	}

	(void)DeltaTime;
	EnforceProjectQueueAuthority();
	if (!DrainingQueueFacilityIds.IsEmpty() && !CanProjectQueuesAdvanceNaturally())
	{
		// Legacy Queue의 시간은 전역이라 AdvanceFixedSteps로 한 시설만 진행할 수 없다.
		// 비-Live/일시정지 경계에서는 해당 시설만 닫아 다른 시설의 시간을 움직이지 않는다.
		CancelDrainsThatCannotAdvance();
	}
	FinishDrainedQueues();
	if (bQueueEventsNeedFlush && QueueSystem)
	{
		bQueueEventsNeedFlush = false;
		QueueSystem->FlushPendingEvents();
	}
}

TStatId UFF_ZoneSystemQueueBridgeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_ZoneSystemQueueBridgeSubsystem, STATGROUP_Tickables);
}

int32 UFF_ZoneSystemQueueBridgeSubsystem::FindQueueFacilityId(const FGuid ZoneFacilityId) const
{
	const int32* Found = ZoneToQueueFacilityIds.Find(ZoneFacilityId);
	return Found ? *Found : INDEX_NONE;
}

FGuid UFF_ZoneSystemQueueBridgeSubsystem::FindZoneFacilityId(const int32 QueueFacilityId) const
{
	const FGuid* Found = QueueToZoneFacilityIds.Find(QueueFacilityId);
	return Found ? *Found : FGuid();
}

int32 UFF_ZoneSystemQueueBridgeSubsystem::FindOrAllocateQueueFacilityId(const FGuid ZoneFacilityId)
{
	if (!ZoneFacilityId.IsValid())
	{
		return INDEX_NONE;
	}
	if (const int32* Existing = ZoneToQueueFacilityIds.Find(ZoneFacilityId))
	{
		const int32 ExistingId = *Existing;
		const bool bOwnedAndActive = ActiveBridgeQueueFacilityIds.Contains(ExistingId);
		const bool bTakenByAnotherQueue = QueueSystem
			&& QueueSystem->IsFacilityRegistered(ExistingId)
			&& !bOwnedAndActive;
		if (!bTakenByAnotherQueue)
		{
			return ExistingId;
		}

		// 제거 뒤 남겨 둔 tombstone ID를 다른 시스템이 사용 중이면 절대 재구성하지 않는다.
		UE_LOG(LogFFZoneQueueBridge, Warning,
			TEXT("Zone Facility %s의 이전 Queue ID %d가 다른 Queue에 사용 중이어서 새 ID를 배정합니다."),
			*ZoneFacilityId.ToString(), ExistingId);
		ZoneToQueueFacilityIds.Remove(ZoneFacilityId);
		QueueToZoneFacilityIds.Remove(ExistingId);
		ProjectQueueAuthorityFacilityIds.Remove(ExistingId);
		DrainingQueueFacilityIds.Remove(ExistingId);
	}

	// 프로젝트의 일반 양수 ID와 충돌 가능성을 줄이기 위해 -2부터 내려가는 브리지 전용 공간을 쓴다.
	// INDEX_NONE(-1)만 Legacy Queue에서 무효이므로 나머지 음수 ID는 정상 키다.
	for (int64 Candidate = NextQueueFacilityId; Candidate >= MIN_int32; --Candidate)
	{
		const int32 CandidateId = static_cast<int32>(Candidate);
		const bool bOwnedByBridge = QueueToZoneFacilityIds.Contains(CandidateId);
		const bool bOwnedByAnotherQueue = QueueSystem && QueueSystem->IsFacilityRegistered(CandidateId);
		if (bOwnedByBridge || bOwnedByAnotherQueue)
		{
			continue;
		}

		ZoneToQueueFacilityIds.Add(ZoneFacilityId, CandidateId);
		QueueToZoneFacilityIds.Add(CandidateId, ZoneFacilityId);
		NextQueueFacilityId = Candidate - 1;
		return CandidateId;
	}

	UE_LOG(LogFFZoneQueueBridge, Error,
		TEXT("Zone Facility %s에 배정할 int32 Queue FacilityId가 고갈되었습니다."),
		*ZoneFacilityId.ToString());
	return INDEX_NONE;
}

TArray<FFF_QueuePlacementObstacle> UFF_ZoneSystemQueueBridgeSubsystem::BuildPlacementObstacles(
	const FFFZSPolicyContext* ProposedChange) const
{
	TArray<FFF_QueuePlacementObstacle> Obstacles;
	if (ZoneSystem)
	{
		for (const FFFZSFacilityReadModel& Model : ZoneSystem->BuildReadModel().Facilities)
		{
			if (ProposedChange && Model.FacilityInstanceId == ProposedChange->Command.FacilityInstanceId) continue;
			const AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(Model.FacilityInstanceId);
			if (!Facility || Facility->GetFacilityState() == EFFZSFacilityState::Removed) continue;
			FFF_QueuePlacementObstacle Obstacle;
			if (TryBuildPlacementObstacle(Facility->GetFacilitySpec(), Facility->GetActorTransform(), Obstacle))
			{
				Obstacles.Add(Obstacle);
			}
		}
	}
	if (ProposedChange && ProposedChange->Command.Type != EFFZSCommandType::Remove)
	{
		FFF_QueuePlacementObstacle Obstacle;
		if (TryBuildPlacementObstacle(ProposedChange->Command.FacilitySpec,
			ProposedChange->Validation.PlacementTransform, Obstacle))
		{
			Obstacles.Add(Obstacle);
		}
	}
	return Obstacles;
}

void UFF_ZoneSystemQueueBridgeSubsystem::RefreshAllPlacementObstacles()
{
	if (!QueueSystem) return;
	const TArray<FFF_QueuePlacementObstacle> Obstacles = BuildPlacementObstacles();
	for (const int32 QueueId : ActiveBridgeQueueFacilityIds)
	{
		if (FFF_FacilityQueue* Queue = QueueSystem->FindQueue(QueueId))
		{
			if (!Queue->RefreshPlacementObstacles(Obstacles))
			{
				UE_LOG(LogFFZoneQueueBridge, Warning,
					TEXT("Facility %d의 점유 대기열과 새 시설 경계가 충돌해 기존 자리 배정을 유지합니다."), QueueId);
			}
		}
	}
}

bool UFF_ZoneSystemQueueBridgeSubsystem::RegisterOrRefreshFacility(AFFZSFacilityActor& Facility)
{
	// 조명·길·타일은 Party 서비스 시설이 아니므로 Queue ID나 이동 슬롯을 만들지 않는다.
	if (!QueueSystem || !Facility.GetFacilitySpec().UsesServiceQueue())
	{
		return false;
	}

	const FGuid ZoneFacilityId = Facility.GetFacilityInstanceId();
	const int32 QueueFacilityId = FindOrAllocateQueueFacilityId(ZoneFacilityId);
	const UFFZSFacilityQueueComponent* ZoneQueue = Facility.GetQueueComponent();
	if (QueueFacilityId == INDEX_NONE || !ZoneQueue)
	{
		return false;
	}
	if (ActiveBridgeQueueFacilityIds.Contains(QueueFacilityId)
		&& QueueSystem->IsFacilityRegistered(QueueFacilityId)
		&& QueueSystem->GetQueueState(QueueFacilityId).OccupiedMemberCount > 0)
	{
		// RegisterFacility 재설정은 Party 배열을 지우므로 점유 상태에서는 호출자가 먼저 drain해야 한다.
		UE_LOG(LogFFZoneQueueBridge, Error,
			TEXT("점유 중인 Zone Facility %s의 Queue 재구성을 거부했습니다. Party를 먼저 비워야 합니다."),
			*ZoneFacilityId.ToString());
		return false;
	}

	const FFFZSFacilitySpec& Spec = Facility.GetFacilitySpec();
	const FFFZSQueueRules Rules = Facility.GetEffectiveQueueRules();

	const FFF_QueueConfig Config = ToProjectQueueConfig(Rules, ZoneQueue->GetExitWorldLocation(0));

	FFF_QueueLaneGeometry Lane;
	if (!BuildProjectQueueLaneGeometry(Spec, Facility.GetActorTransform(), Lane))
	{
		UE_LOG(LogFFZoneQueueBridge, Error,
			TEXT("Zone Facility %s has invalid Party Queue geometry; registration failed closed."),
			*ZoneFacilityId.ToString());
		return false;
	}

	Lane.PlacementObstacles = BuildPlacementObstacles();
	if (!QueueSystem->RegisterFacility(QueueFacilityId, Config, Lane) ||
		!QueueSystem->IsFacilityRegistered(QueueFacilityId))
	{
		return false;
	}
	ActiveBridgeQueueFacilityIds.Add(QueueFacilityId);
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	QueueSystem->SetPerformanceBreak(QueueFacilityId,
		Stage && Stage->IsMainProgramQueueBreakActive() && PausesForMainProgram(Facility.GetFacilitySpec()));

	if (Facility.GetFacilityState() == EFFZSFacilityState::Active)
	{
		QueueSystem->ReopenFacility(QueueFacilityId);
	}
	else
	{
		QueueSystem->CloseFacility(QueueFacilityId, EFF_QueueCloseReason::Reconfigure);
	}
	return true;
}

void UFF_ZoneSystemQueueBridgeSubsystem::UnregisterFacility(const FGuid ZoneFacilityId)
{
	if (!QueueSystem)
	{
		return;
	}
	if (const int32* QueueFacilityId = ZoneToQueueFacilityIds.Find(ZoneFacilityId))
	{
		if (ActiveBridgeQueueFacilityIds.Contains(*QueueFacilityId))
		{
			QueueSystem->UnregisterFacility(*QueueFacilityId);
			ActiveBridgeQueueFacilityIds.Remove(*QueueFacilityId);
			DrainingQueueFacilityIds.Remove(*QueueFacilityId);
			ProjectQueueAuthorityFacilityIds.Remove(*QueueFacilityId);
			bQueueEventsNeedFlush = true;
		}
		// 양방향 매핑은 tombstone으로 남긴다. 제거 뒤 다른 시설이 같은 int32 ID를 재사용하지 않는다.
	}
}

int32 UFF_ZoneSystemQueueBridgeSubsystem::RefreshQueueRegistrations()
{
	if (!ZoneSystem || !QueueSystem)
	{
		return 0;
	}

	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
	TSet<FGuid> CurrentFacilityIds;
	CurrentFacilityIds.Reserve(ReadModel.Facilities.Num());
	for (const FFFZSFacilityReadModel& FacilityModel : ReadModel.Facilities)
	{
		CurrentFacilityIds.Add(FacilityModel.FacilityInstanceId);
	}

	// Snapshot에서 사라진 시설의 Legacy Queue를 먼저 제거한다. 매핑 자체는 안정 ID를 위해 보존한다.
	for (const TPair<FGuid, int32>& Mapping : ZoneToQueueFacilityIds)
	{
		if (!CurrentFacilityIds.Contains(Mapping.Key)
			&& ActiveBridgeQueueFacilityIds.Contains(Mapping.Value))
		{
			UnregisterFacility(Mapping.Key);
		}
	}

	DrainingQueueFacilityIds.Reset();
	int32 RegisteredCount = 0;
	// BuildReadModel은 GUID 순으로 정렬되므로 최초 음수 ID 배정도 등록 순서와 무관하게 결정적이다.
	for (const FFFZSFacilityReadModel& FacilityModel : ReadModel.Facilities)
	{
		if (AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(FacilityModel.FacilityInstanceId))
		{
			const int32 ExistingId = FindQueueFacilityId(FacilityModel.FacilityInstanceId);
			if (ExistingId != INDEX_NONE
				&& ActiveBridgeQueueFacilityIds.Contains(ExistingId)
				&& QueueSystem->IsFacilityRegistered(ExistingId))
			{
				if (QueueSystem->GetQueueState(ExistingId).OccupiedMemberCount > 0)
				{
					UE_LOG(LogFFZoneQueueBridge, Warning,
						TEXT("Zone Snapshot이 동기 wrapper 밖에서 복원되어 Facility %s의 Party Queue를 안전 취소 후 재구성합니다."),
						*FacilityModel.FacilityInstanceId.ToString());
				}
				// RegisterFacility::Configure는 Party 배열을 초기화한다. 먼저 정상 Close 경로를 거쳐
				// Cancel 이벤트와 Queue 로그를 보존한 뒤 새 투영을 적용한다.
				QueueSystem->CloseFacility(ExistingId, EFF_QueueCloseReason::Reconfigure);
				bQueueEventsNeedFlush = true;
			}
			RegisteredCount += RegisterOrRefreshFacility(*Facility) ? 1 : 0;
		}
	}
	RefreshAllPlacementObstacles();
	CacheObservedWorld();
	return RegisteredCount;
}

bool UFF_ZoneSystemQueueBridgeSubsystem::RestoreZoneWorldSnapshotAndSynchronize(
	const FFFZSWorldSnapshot& Snapshot,
	TArray<FText>& OutErrors)
{
	// Snapshot에는 프로젝트 Party 데이터가 없으므로 점유 상태를 조용히 버리는 복원을 금지한다.
	if (QueueSystem)
	{
		for (const int32 QueueFacilityId : ActiveBridgeQueueFacilityIds)
		{
			if (QueueSystem->IsFacilityRegistered(QueueFacilityId)
				&& QueueSystem->GetQueueState(QueueFacilityId).OccupiedMemberCount > 0)
			{
				const FGuid ZoneFacilityId = FindZoneFacilityId(QueueFacilityId);
				OutErrors.Add(FText::Format(
					LOCTEXT("OccupiedProjectQueueBlocksRestore",
						"Zone Snapshot에는 프로젝트 Party Queue가 포함되지 않습니다. 시설 {0}의 대기열을 먼저 비우십시오."),
					FText::FromString(ZoneFacilityId.ToString())));
				return false;
			}
		}
	}

	if (!ZoneSystem)
	{
		return false;
	}

	// 저장된 배치는 저장 시점에 이미 배치 검증을 통과했으므로 복원은 권위 상태를 신뢰한다.
	// 이 가드가 없으면 Crowd 레인이 구운 GeneralRoute 보호 구역 때문에 레인 도입 이전 세이브가
	// 복원에 실패한다. 게다가 ProtectedArea 등록은 BeginPlay라 복원 시점에 등록이 끝났는지가
	// 보장되지 않아(비었으면 fail-open) 결과가 액터 순서에 따라 갈린다 — 여기서 결정적으로 만든다.
	TArray<AFFZSProtectedArea*> SuspendedProtectedAreas;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AFFZSProtectedArea> It(World); It; ++It)
		{
			if (ZoneSystem->UnregisterProtectedArea(*It))
			{
				SuspendedProtectedAreas.Add(*It);
			}
		}
	}

	ON_SCOPE_EXIT
	{
		for (AFFZSProtectedArea* Area : SuspendedProtectedAreas)
		{
			ZoneSystem->RegisterProtectedArea(Area);
		}
	};

	if (!ZoneSystem->RestoreWorldSnapshot(Snapshot, OutErrors))
	{
		return false;
	}

	// Zone Actor 세대, Party Queue 투영, Stage phase를 같은 wrapper 호출 안에서 다시 일치시킨다.
	RefreshQueueRegistrations();
	bActorGenerationCheckPending = false;
	if (UWorld* World = GetWorld())
	{
		if (UFF_ZoneSystemStageBridgeSubsystem* StageBridge =
			World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>())
		{
			StageBridge->SynchronizeCurrentStage();
		}
	}
	return true;
}

void UFF_ZoneSystemQueueBridgeSubsystem::HandleZoneWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	if (!ZoneSystem || !QueueSystem)
	{
		return;
	}

	// 성공한 Zone 명령만 이 이벤트에 도달하므로 외부 Queue 변경은 이 단일 commit 지점에서 수행한다.
	switch (Event.Type)
	{
	case EFFZSCommandType::Install:
		if (AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(Event.FacilityInstanceId))
		{
			RegisterOrRefreshFacility(*Facility);
		}
		DrainingQueueFacilityIds.Remove(FindQueueFacilityId(Event.FacilityInstanceId));
		break;

	case EFFZSCommandType::Move:
		if (AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(Event.FacilityInstanceId))
		{
			if (Facility->GetFacilitySpec().UsesServiceQueue() &&
				!RegisterOrRefreshFacility(*Facility))
			{
				// Move는 Zone 권위 배치가 커밋된 뒤 이 이벤트를 방송한다. 새 Queue geometry를 만들지
				// 못한 경우 이전 위치의 Queue를 남기면 다음 Open에서 잘못된 동선을 재활성화할 수 있다.
				// 기존 등록을 제거해 fail-closed하고, 이후 Open 경로가 동일 stable ID로 재등록을 시도하게 한다.
				UnregisterFacility(Event.FacilityInstanceId);
				UE_LOG(LogFFZoneQueueBridge, Error,
					TEXT("이동된 Zone Facility %s의 Party Queue 갱신에 실패해 이전 Queue를 안전하게 해제했습니다."),
					*Event.FacilityInstanceId.ToString());
			}
		}
		DrainingQueueFacilityIds.Remove(FindQueueFacilityId(Event.FacilityInstanceId));
		break;

	case EFFZSCommandType::Open:
		if (AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(Event.FacilityInstanceId))
		{
			int32 QueueFacilityId = FindQueueFacilityId(Event.FacilityInstanceId);
			if (QueueFacilityId == INDEX_NONE
				|| !ActiveBridgeQueueFacilityIds.Contains(QueueFacilityId)
				|| !QueueSystem->IsFacilityRegistered(QueueFacilityId))
			{
				RegisterOrRefreshFacility(*Facility);
				QueueFacilityId = FindQueueFacilityId(Event.FacilityInstanceId);
			}
			if (QueueFacilityId != INDEX_NONE && QueueSystem->IsFacilityRegistered(QueueFacilityId))
			{
				DrainingQueueFacilityIds.Remove(QueueFacilityId);
				QueueSystem->ReopenFacility(QueueFacilityId);
			}
		}
		break;

	case EFFZSCommandType::Close:
	case EFFZSCommandType::EmergencyClose:
		if (AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(Event.FacilityInstanceId))
		{
			int32 QueueFacilityId = FindQueueFacilityId(Event.FacilityInstanceId);
			if (QueueFacilityId == INDEX_NONE
				|| !ActiveBridgeQueueFacilityIds.Contains(QueueFacilityId)
				|| !QueueSystem->IsFacilityRegistered(QueueFacilityId))
			{
				RegisterOrRefreshFacility(*Facility);
				QueueFacilityId = FindQueueFacilityId(Event.FacilityInstanceId);
			}
			if (QueueFacilityId != INDEX_NONE && QueueSystem->IsFacilityRegistered(QueueFacilityId))
			{
				if (Event.Type == EFFZSCommandType::EmergencyClose)
				{
					DrainingQueueFacilityIds.Remove(QueueFacilityId);
					QueueSystem->CloseFacility(QueueFacilityId, EFF_QueueCloseReason::Emergency);
					ProjectQueueAuthorityFacilityIds.Remove(QueueFacilityId);
					bQueueEventsNeedFlush = true;
				}
				else if (QueueSystem->GetQueueState(QueueFacilityId).OccupiedMemberCount > 0
					&& CanProjectQueuesAdvanceNaturally())
				{
					// Legacy Queue에는 BeginDrain이 없다. Queue는 계속 처리하게 두되 façade의 신규
					// 가입을 막고, 마지막 Party가 끝난 다음 비어 있는 Queue를 닫는다.
					DrainingQueueFacilityIds.Add(QueueFacilityId);
				}
				else
				{
					if (QueueSystem->GetQueueState(QueueFacilityId).OccupiedMemberCount > 0)
					{
						UE_LOG(LogFFZoneQueueBridge, Warning,
							TEXT("Zone Facility %s의 non-Live Close를 해당 Project Queue만 안전 취소로 종료합니다."),
							*Event.FacilityInstanceId.ToString());
					}
					QueueSystem->CloseFacility(QueueFacilityId, EFF_QueueCloseReason::OperatingHoursEnded);
					DrainingQueueFacilityIds.Remove(QueueFacilityId);
					ProjectQueueAuthorityFacilityIds.Remove(QueueFacilityId);
					bQueueEventsNeedFlush = true;
				}
			}
		}
		break;

	case EFFZSCommandType::Remove:
		UnregisterFacility(Event.FacilityInstanceId);
		break;

	case EFFZSCommandType::Upgrade:
		// ApplyTentative에서 같은 ID에 새 처리율을 적용했고 Commit에서 확정했다.
		// 여기서 재등록하면 이미 확정한 큐를 다시 초기화하게 된다.
		break;

	default:
		break;
	}

	if (Event.Type == EFFZSCommandType::Install || Event.Type == EFFZSCommandType::Move
		|| Event.Type == EFFZSCommandType::Remove || Event.Type == EFFZSCommandType::Upgrade)
	{
		RefreshAllPlacementObstacles();
	}
	CacheObservedWorld();
	bActorGenerationCheckPending = false;
}

void UFF_ZoneSystemQueueBridgeSubsystem::SynchronizeMainProgramQueueBreak()
{
	if (!QueueSystem) return;
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	const bool bShouldBreak = Stage && Stage->IsMainProgramQueueBreakActive();
	if (bMainProgramQueueBreak == bShouldBreak) return;

	// 모든 큐를 먼저 막은 뒤 이벤트를 보낸다. 퇴장 핸들러의 재선택이 다른 큐로 새지 않는다.
	bMainProgramQueueBreak = bShouldBreak;
	TArray<int32> FacilityIds = ActiveBridgeQueueFacilityIds.Array();
	FacilityIds.Sort();
	for (const int32 FacilityId : FacilityIds)
	{
		QueueSystem->SetPerformanceBreak(FacilityId, bShouldBreak && PausesForMainProgram(FacilityId));
	}
	QueueSystem->FlushPendingEvents();
}

bool UFF_ZoneSystemQueueBridgeSubsystem::PausesForMainProgram(const int32 QueueFacilityId) const
{
	const FGuid* ZoneFacilityId = QueueToZoneFacilityIds.Find(QueueFacilityId);
	const AFFZSFacilityActor* Facility = ZoneFacilityId && ZoneSystem ? ZoneSystem->FindFacility(*ZoneFacilityId) : nullptr;
	return !Facility || PausesForMainProgram(Facility->GetFacilitySpec());
}

FFF_QueueJoinResponse UFF_ZoneSystemQueueBridgeSubsystem::TryJoinZoneFacility(
	const FGuid ZoneFacilityId,
	FFF_JoinRequest Request)
{
	FFF_QueueJoinResponse Response;
	if (!ZoneSystem || !QueueSystem || !ZoneFacilityId.IsValid())
	{
		return Response;
	}
	SynchronizeMainProgramQueueBreak();
	AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(ZoneFacilityId);
	if (bMainProgramQueueBreak && (!Facility || PausesForMainProgram(Facility->GetFacilitySpec())))
	{
		Response.Result = EFF_QueueJoinResult::RejectedClosed;
		return Response;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFF_ProgramReservationSubsystem* Reservations = GameInstance
		? GameInstance->GetSubsystem<UFF_ProgramReservationSubsystem>() : nullptr;
	if (Reservations && Reservations->IsProgramFacility(ZoneFacilityId))
	{
		Response.Result = EFF_QueueJoinResult::RejectedClosed;
		return Response; // 예약자는 슬롯 좌표만 사용하고 일반 서비스 Queue에는 가입하지 않는다.
	}
	if (!Facility || Facility->GetFacilityState() != EFFZSFacilityState::Active ||
		!Facility->GetFacilitySpec().UsesServiceQueue())
	{
		Response.Result = EFF_QueueJoinResult::RejectedClosed;
		return Response;
	}

	int32 QueueFacilityId = FindQueueFacilityId(ZoneFacilityId);
	if (QueueFacilityId == INDEX_NONE
		|| !ActiveBridgeQueueFacilityIds.Contains(QueueFacilityId)
		|| !QueueSystem->IsFacilityRegistered(QueueFacilityId))
	{
		if (!RegisterOrRefreshFacility(*Facility))
		{
			return Response;
		}
		QueueFacilityId = FindQueueFacilityId(ZoneFacilityId);
	}

	if (QueueFacilityId == INDEX_NONE || DrainingQueueFacilityIds.Contains(QueueFacilityId))
	{
		Response.Result = EFF_QueueJoinResult::RejectedClosed;
		return Response;
	}

	// Navigation/Safety의 Blocked는 실제 운영 신호이므로 authority mutex로 재사용하지 않는다.
	// Production 호출자는 이 façade만 사용하고, 계약 밖의 FFZS Agent 가입은 Tick에서 감지해 fail-closed 한다.
	const UFFZSFacilityQueueComponent* ZoneQueue = Facility->GetQueueComponent();
	if (!ZoneQueue || ZoneQueue->IsOccupied())
	{
		UE_LOG(LogFFZoneQueueBridge, Error,
			TEXT("Zone Facility %s는 FFZS Agent Queue가 사용 중이므로 Project Party Queue 가입을 거부했습니다."),
			*ZoneFacilityId.ToString());
		Response.Result = EFF_QueueJoinResult::RejectedClosed;
		return Response;
	}
	if (ZoneQueue->GetQueueState() != EFFZSQueueState::Open)
	{
		// Navigation/Safety의 Blocked와 lifecycle의 Draining/Closed를 항상 우선한다.
		Response.Result = EFF_QueueJoinResult::RejectedClosed;
		return Response;
	}
	const FFF_QueueState ProjectState = QueueSystem->GetQueueState(QueueFacilityId);
	if (!ProjectState.bIsOpen && ProjectState.OccupiedMemberCount == 0)
	{
		// Safety fail-close가 해제된 활성 시설은 다음 façade 가입에서만 다시 연다.
		QueueSystem->ReopenFacility(QueueFacilityId);
	}

	Request.FacilityId = QueueFacilityId;
	const float ZoneAbandonSeconds = Facility->GetFacilitySpec().QueueRules.AbandonAfterSeconds;
	Request.MaxWaitSeconds = ToProjectMaxWaitSeconds(Request.MaxWaitSeconds, ZoneAbandonSeconds);
	Response = QueueSystem->TryJoin(Request);
	bQueueEventsNeedFlush = true;
	if (Response.bAccepted)
	{
		ProjectQueueAuthorityFacilityIds.Add(QueueFacilityId);
	}
	return Response;
}

bool UFF_ZoneSystemQueueBridgeSubsystem::LeaveZoneFacilityQueue(
	const FGuid ZoneFacilityId,
	const int32 PartyId)
{
	const int32 QueueFacilityId = FindQueueFacilityId(ZoneFacilityId);
	const bool bLeft = QueueSystem && QueueFacilityId != INDEX_NONE
		&& ActiveBridgeQueueFacilityIds.Contains(QueueFacilityId)
		&& QueueSystem->LeaveQueue(QueueFacilityId, PartyId);
	bQueueEventsNeedFlush |= bLeft;
	return bLeft;
}

bool UFF_ZoneSystemQueueBridgeSubsystem::TryGetProgramWaitingTransform(
	const FGuid ZoneFacilityId,
	const int32 WaitingIndex,
	FTransform& OutTransform) const
{
	OutTransform = FTransform::Identity;
	const int32 QueueFacilityId = FindQueueFacilityId(ZoneFacilityId);
	const FFF_FacilityQueue* Queue = QueueSystem && QueueFacilityId != INDEX_NONE
		? QueueSystem->FindQueue(QueueFacilityId)
		: nullptr;
	if (!Queue || WaitingIndex < 0)
	{
		return false;
	}

	const FFF_QueueLaneGeometry& Lane = Queue->GetLaneGeometry();
	if (Lane.ExplicitSlotTransforms.IsValidIndex(WaitingIndex))
	{
		OutTransform = Lane.ExplicitSlotTransforms[WaitingIndex];
		return !OutTransform.ContainsNaN();
	}
	// 저작 범위 밖으로 줄을 늘리면 장애물이나 다른 시설에 들어갈 수 있다.
	return false;
}

void UFF_ZoneSystemQueueBridgeSubsystem::NotifyZoneFacilityPartyArrived(
	const FGuid ZoneFacilityId,
	const int32 PartyId)
{
	const int32 QueueFacilityId = FindQueueFacilityId(ZoneFacilityId);
	if (QueueSystem && QueueFacilityId != INDEX_NONE
		&& ActiveBridgeQueueFacilityIds.Contains(QueueFacilityId))
	{
		QueueSystem->NotifyPartyArrived(QueueFacilityId, PartyId);
	}
}

FFF_QueueState UFF_ZoneSystemQueueBridgeSubsystem::GetZoneFacilityQueueState(
	const FGuid ZoneFacilityId) const
{
	const int32 QueueFacilityId = FindQueueFacilityId(ZoneFacilityId);
	FFF_QueueState State = QueueSystem && QueueFacilityId != INDEX_NONE
		&& ActiveBridgeQueueFacilityIds.Contains(QueueFacilityId)
		? QueueSystem->GetQueueState(QueueFacilityId)
		: FFF_QueueState();
	if (DrainingQueueFacilityIds.Contains(QueueFacilityId))
	{
		State.bAcceptingNew = false;
		State.bIsOpen = false;
	}
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	if (Stage && Stage->IsMainProgramQueueBreakActive()) State.bAcceptingNew = false;
	return State;
}

void UFF_ZoneSystemQueueBridgeSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (Actor && Actor->IsA<AFFZSFacilityActor>())
	{
		bActorGenerationCheckPending = true;
	}
}

void UFF_ZoneSystemQueueBridgeSubsystem::HandleActorDestroyed(AActor* Actor)
{
	if (Actor && Actor->IsA<AFFZSFacilityActor>())
	{
		bActorGenerationCheckPending = true;
	}
}

bool UFF_ZoneSystemQueueBridgeSubsystem::HasObservedWorldChanged() const
{
	// Revision뿐 아니라 GUID가 가리키는 Actor 주소도 비교해 같은 Snapshot의 조용한 세대 교체를 찾는다.
	if (!ZoneSystem || LastObservedWorldRevision != ZoneSystem->GetWorldRevision())
	{
		return ZoneSystem != nullptr;
	}

	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
	if (ReadModel.Facilities.Num() != ObservedFacilityActors.Num())
	{
		return true;
	}
	for (const FFFZSFacilityReadModel& FacilityModel : ReadModel.Facilities)
	{
		const TWeakObjectPtr<AFFZSFacilityActor>* Observed =
			ObservedFacilityActors.Find(FacilityModel.FacilityInstanceId);
		if (!Observed || Observed->Get() != ZoneSystem->FindFacility(FacilityModel.FacilityInstanceId))
		{
			return true;
		}
	}
	return false;
}

void UFF_ZoneSystemQueueBridgeSubsystem::CacheObservedWorld()
{
	ObservedFacilityActors.Reset();
	if (!ZoneSystem)
	{
		LastObservedWorldRevision = INDEX_NONE;
		return;
	}

	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
	for (const FFFZSFacilityReadModel& FacilityModel : ReadModel.Facilities)
	{
		ObservedFacilityActors.Add(
			FacilityModel.FacilityInstanceId,
			ZoneSystem->FindFacility(FacilityModel.FacilityInstanceId));
	}
	LastObservedWorldRevision = ReadModel.WorldRevision;
}

void UFF_ZoneSystemQueueBridgeSubsystem::EnforceProjectQueueAuthority()
{
	if (!ZoneSystem || !QueueSystem || ProjectQueueAuthorityFacilityIds.IsEmpty())
	{
		return;
	}

	const TArray<int32> AuthorityIds = ProjectQueueAuthorityFacilityIds.Array();
	for (const int32 QueueFacilityId : AuthorityIds)
	{
		if (!ActiveBridgeQueueFacilityIds.Contains(QueueFacilityId)
			|| !QueueSystem->IsFacilityRegistered(QueueFacilityId))
		{
			ProjectQueueAuthorityFacilityIds.Remove(QueueFacilityId);
			continue;
		}
		if (QueueSystem->GetQueueState(QueueFacilityId).OccupiedMemberCount <= 0)
		{
			ProjectQueueAuthorityFacilityIds.Remove(QueueFacilityId);
			continue;
		}
		if (DrainingQueueFacilityIds.Contains(QueueFacilityId))
		{
			// Zone Close 뒤에는 Facility가 비활성이어도 Live 동안 기존 Project Party만 자연 배출한다.
			continue;
		}

		const FGuid ZoneFacilityId = FindZoneFacilityId(QueueFacilityId);
		AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(ZoneFacilityId);
		if (!Facility || Facility->GetFacilityState() != EFFZSFacilityState::Active)
		{
			QueueSystem->CloseFacility(QueueFacilityId, EFF_QueueCloseReason::Emergency);
			ProjectQueueAuthorityFacilityIds.Remove(QueueFacilityId);
			DrainingQueueFacilityIds.Remove(QueueFacilityId);
			bQueueEventsNeedFlush = true;
			continue;
		}

		const UFFZSFacilityQueueComponent* ZoneQueue = Facility->GetQueueComponent();
		if (!ZoneQueue)
		{
			QueueSystem->CloseFacility(QueueFacilityId, EFF_QueueCloseReason::Emergency);
			ProjectQueueAuthorityFacilityIds.Remove(QueueFacilityId);
			DrainingQueueFacilityIds.Remove(QueueFacilityId);
			bQueueEventsNeedFlush = true;
			continue;
		}
		const bool bAgentQueueOccupied = ZoneQueue->IsOccupied();
		if (bAgentQueueOccupied || ZoneQueue->GetQueueState() != EFFZSQueueState::Open)
		{
			// 계약 밖의 FFZS Join 또는 Navigation/Safety 차단이 생겼다. Project 쪽만 즉시
			// fail-closed해 한 프레임 안에 단일 권위와 안전 상태를 복구한다.
			if (bAgentQueueOccupied)
			{
				UE_LOG(LogFFZoneQueueBridge, Error,
					TEXT("Zone Facility %s의 Agent Queue 이중 점유를 감지해 Project Queue를 긴급 종료합니다."),
					*ZoneFacilityId.ToString());
			}
			else
			{
				UE_LOG(LogFFZoneQueueBridge, Warning,
					TEXT("Zone Facility %s의 Navigation/Safety 차단을 따라 Project Queue를 안전 종료합니다."),
					*ZoneFacilityId.ToString());
			}
			QueueSystem->CloseFacility(QueueFacilityId, EFF_QueueCloseReason::Emergency);
			ProjectQueueAuthorityFacilityIds.Remove(QueueFacilityId);
			DrainingQueueFacilityIds.Remove(QueueFacilityId);
			bQueueEventsNeedFlush = true;
			continue;
		}
	}
}

void UFF_ZoneSystemQueueBridgeSubsystem::CancelDrainsThatCannotAdvance()
{
	if (!QueueSystem)
	{
		return;
	}

	const TArray<int32> DrainingIds = DrainingQueueFacilityIds.Array();
	for (const int32 QueueFacilityId : DrainingIds)
	{
		if (ActiveBridgeQueueFacilityIds.Contains(QueueFacilityId)
			&& QueueSystem->IsFacilityRegistered(QueueFacilityId))
		{
			UE_LOG(LogFFZoneQueueBridge, Warning,
				TEXT("Project Queue %d의 자연 drain이 중지되어 해당 Queue만 안전 취소합니다."),
				QueueFacilityId);
			QueueSystem->CloseFacility(QueueFacilityId, EFF_QueueCloseReason::OperatingHoursEnded);
			bQueueEventsNeedFlush = true;
		}
		DrainingQueueFacilityIds.Remove(QueueFacilityId);
		ProjectQueueAuthorityFacilityIds.Remove(QueueFacilityId);
	}
}

void UFF_ZoneSystemQueueBridgeSubsystem::FinishDrainedQueues()
{
	if (!QueueSystem || DrainingQueueFacilityIds.IsEmpty())
	{
		return;
	}

	const TArray<int32> DrainingIds = DrainingQueueFacilityIds.Array();
	for (const int32 QueueFacilityId : DrainingIds)
	{
		if (!ActiveBridgeQueueFacilityIds.Contains(QueueFacilityId)
			|| !QueueSystem->IsFacilityRegistered(QueueFacilityId))
		{
			DrainingQueueFacilityIds.Remove(QueueFacilityId);
			continue;
		}
		if (QueueSystem->GetQueueState(QueueFacilityId).OccupiedMemberCount == 0)
		{
			QueueSystem->CloseFacility(QueueFacilityId, EFF_QueueCloseReason::OperatingHoursEnded);
			DrainingQueueFacilityIds.Remove(QueueFacilityId);
			ProjectQueueAuthorityFacilityIds.Remove(QueueFacilityId);
			bQueueEventsNeedFlush = true;
		}
	}
}

bool UFF_ZoneSystemQueueBridgeSubsystem::CanProjectQueuesAdvanceNaturally() const
{
	if (!QueueSystem || QueueSystem->IsPaused())
	{
		return false;
	}
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	// Stage가 없는 테스트 맵에서는 QueueSubsystem 자체 Tick이 계속 진행한다.
	return !Stage || Stage->CurrentStage == EFF_FestivalStage::Live;
}

#undef LOCTEXT_NAMESPACE
