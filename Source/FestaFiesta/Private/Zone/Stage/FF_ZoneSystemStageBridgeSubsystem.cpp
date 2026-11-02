// FF_ZoneSystemStageBridgeSubsystem.cpp

// Festival Stage를 FFZS ChangePhase와 시설 자동 개폐 명령으로 동기화한다.
// 흐름: Stage edge를 저장하고 Phase 동기화가 끝난 다음 프레임에 대상 시설을 배치 처리한다.
// 불변식: 최신 Stage가 권위이며 Closed 시설은 명시적인 수동 개장 없이는 다시 열지 않는다.
// 실패/성능: 늦은 Stage 생성을 재결합하고 짧은 재진입 충돌만 제한 횟수로 재시도한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Zone/FF_ZoneSystemStageBridgeSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "FFZoneSystemStageBridge"
DEFINE_LOG_CATEGORY_STATIC(LogFFZoneStageBridge, Log, All);

namespace FFZoneStageBridge
{
	// 재진입 중인 명령을 잠시 기다리는 안전장치다. 게임 운영 시간이 아닌 재시도 횟수다.
	constexpr int32 MaxAutomaticLifecycleRetries = 3;
}

void UFF_ZoneSystemStageBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Zone 의존성을 먼저 생성한 뒤 GameInstance 수명의 Stage를 조회해 서로 다른 수명 범위를 잇는다.
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!TryBindFestivalStage())
	{
		// 일부 격리 World는 WorldSubsystem 뒤에 GameInstanceSubsystem을 붙인다. Tick 또는 명시적
		// 동기화에서 다시 결합하므로 초기화 순서 차이가 영구적인 연동 실패가 되지 않는다.
		UE_LOG(LogFFZoneStageBridge, Verbose,
			TEXT("FestivalStageSubsystem이 아직 없어 다음 동기화에서 다시 결합합니다."));
	}

	if (ZoneSystem)
	{
		bPolicyRegistered = ZoneSystem->RegisterExternalPolicy(this);
		if (!bPolicyRegistered)
		{
			UE_LOG(LogFFZoneStageBridge, Warning,
				TEXT("FestivalStage의 SimulationTime Policy를 FF_ZoneSystem에 등록하지 못했습니다."));
		}
	}

	SynchronizeCurrentStage();
}

void UFF_ZoneSystemStageBridgeSubsystem::Deinitialize()
{
	// delegate와 외부 정책을 먼저 해제해 종료 중인 subsystem으로 재진입하는 콜백을 차단한다.
	if (FestivalStage)
	{
		FestivalStage->OnStageChanged.RemoveDynamic(
			this, &UFF_ZoneSystemStageBridgeSubsystem::HandleStageChanged);
	}
	if (bPolicyRegistered && ZoneSystem)
	{
		ZoneSystem->UnregisterExternalPolicy(this);
	}
	bStageSyncRetryScheduled = false;
	bAutomaticLifecycleScheduled = false;
	AutomaticLifecycleRetryCount = 0;
	PendingAutomaticLifecycleCommand.Reset();
	bPolicyRegistered = false;
	FestivalStage = nullptr;
	ZoneSystem = nullptr;
	Super::Deinitialize();
}

bool UFF_ZoneSystemStageBridgeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFF_ZoneSystemStageBridgeSubsystem::Tick(const float DeltaTime)
{
	(void)DeltaTime;

	// RestoreWorldSnapshot은 단계 변경 이벤트를 방송하지 않는다. Stage가 권위이므로 값 하나만
	// 비교해 조용한 복원이나 직접 대입 뒤에도 다음 프레임에 자동으로 다시 맞춘다.
	if (!ZoneSystem || !TryBindFestivalStage())
	{
		return;
	}

	if (ZoneSystem->GetChangePhase() != ToZoneChangePhase(FestivalStage->CurrentStage))
	{
		if (TrySynchronizeStage(false))
		{
			ScheduleAutomaticLifecycle();
		}
	}
	else if (PendingAutomaticLifecycleCommand.IsSet())
	{
		ScheduleAutomaticLifecycle();
	}
}

TStatId UFF_ZoneSystemStageBridgeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_ZoneSystemStageBridgeSubsystem, STATGROUP_Tickables);
}

EFFZSChangePhase UFF_ZoneSystemStageBridgeSubsystem::ToZoneChangePhase(
	const EFF_FestivalStage FestivalStageValue)
{
	// 프로젝트의 세부 운영 단계는 FFZS가 허용하는 네 가지 배치 권한 단계로 축약한다.
	switch (FestivalStageValue)
	{
	case EFF_FestivalStage::Setup:
		return EFFZSChangePhase::InitialSetup;
	case EFF_FestivalStage::MorningCheck:
		return EFFZSChangePhase::MorningCheck;
	case EFF_FestivalStage::PromotionPreview:
	case EFF_FestivalStage::Live:
		return EFFZSChangePhase::LiveOperation;
	case EFF_FestivalStage::Closing:
	case EFF_FestivalStage::Report:
	case EFF_FestivalStage::Finale:
	default:
		// 새 Stage가 추가되어 매핑을 빼먹어도 배치를 열지 않는 fail-closed 기본값이다.
		return EFFZSChangePhase::EveningSettlement;
	}
}

bool UFF_ZoneSystemStageBridgeSubsystem::SynchronizeCurrentStage()
{
	if (!TryBindFestivalStage())
	{
		return false;
	}
	return TrySynchronizeStage(true);
}

bool UFF_ZoneSystemStageBridgeSubsystem::TryBindFestivalStage()
{
	if (FestivalStage)
	{
		return true;
	}

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	FestivalStage = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
		: nullptr;
	if (!FestivalStage)
	{
		return false;
	}

	// AddUnique는 초기화와 지연 재결합이 같은 인스턴스에서 만나도 중복 방송을 막는다.
	FestivalStage->OnStageChanged.AddUniqueDynamic(
		this, &UFF_ZoneSystemStageBridgeSubsystem::HandleStageChanged);
	return true;
}

bool UFF_ZoneSystemStageBridgeSubsystem::TrySynchronizeStage(const bool bScheduleOneRetry)
{
	if (!ZoneSystem || !FestivalStage)
	{
		return false;
	}

	const EFFZSChangePhase TargetPhase = ToZoneChangePhase(FestivalStage->CurrentStage);
	if (ZoneSystem->GetChangePhase() == TargetPhase)
	{
		return true;
	}
	if (ZoneSystem->SetChangePhase(TargetPhase))
	{
		return true;
	}

	UE_LOG(LogFFZoneStageBridge, Warning,
		TEXT("FestivalStage(%d)를 FFZS ChangePhase(%d)로 동기화하지 못했습니다. 권한 명령 종료 후 재시도합니다."),
		static_cast<int32>(FestivalStage->CurrentStage), static_cast<int32>(TargetPhase));

	UWorld* World = GetWorld();
	if (bScheduleOneRetry && !bStageSyncRetryScheduled && World)
	{
		// 현재 FFZS 명령의 재진입 guard가 풀린 다음 Tick에 한 번만 다시 시도한다.
		bStageSyncRetryScheduled = true;
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UFF_ZoneSystemStageBridgeSubsystem::RetryStageSynchronization));
	}
	return false;
}

void UFF_ZoneSystemStageBridgeSubsystem::RetryStageSynchronization()
{
	bStageSyncRetryScheduled = false;
	if (TrySynchronizeStage(false))
	{
		ScheduleAutomaticLifecycle();
	}
}

void UFF_ZoneSystemStageBridgeSubsystem::HandleStageChanged(
	const EFF_FestivalStage PreviousStage,
	const EFF_FestivalStage NewStage)
{
	EFFZSCommandType LifecycleCommand = EFFZSCommandType::Open;
	if (TryGetAutomaticLifecycleCommand(PreviousStage, NewStage, LifecycleCommand))
	{
		// 최신 edge가 이전에 예약된 작업을 대체합니다. Live->Live는 아무 작업도 만들지 않습니다.
		PendingAutomaticLifecycleCommand = LifecycleCommand;
	}
	SynchronizeCurrentStage();
	ScheduleAutomaticLifecycle();
}

bool UFF_ZoneSystemStageBridgeSubsystem::TryGetAutomaticLifecycleCommand(
	const EFF_FestivalStage PreviousStage,
	const EFF_FestivalStage NewStage,
	EFFZSCommandType& OutCommandType)
{
	if (PreviousStage != EFF_FestivalStage::Live && NewStage == EFF_FestivalStage::Live)
	{
		OutCommandType = EFFZSCommandType::Open;
		return true;
	}
	if (PreviousStage == EFF_FestivalStage::Live && NewStage != EFF_FestivalStage::Live)
	{
		OutCommandType = EFFZSCommandType::Close;
		return true;
	}
	return false;
}

void UFF_ZoneSystemStageBridgeSubsystem::ScheduleAutomaticLifecycle()
{
	UWorld* World = GetWorld();
	if (!World || !PendingAutomaticLifecycleCommand.IsSet() || bAutomaticLifecycleScheduled)
	{
		return;
	}

	// Stage delegate 안에서 시설 명령을 재진입시키지 않도록 다음 Tick으로 직렬화한다.
	bAutomaticLifecycleScheduled = true;
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(
			this, &UFF_ZoneSystemStageBridgeSubsystem::RunScheduledAutomaticLifecycle));
}

void UFF_ZoneSystemStageBridgeSubsystem::RunScheduledAutomaticLifecycle()
{
	bAutomaticLifecycleScheduled = false;
	if (!ZoneSystem || !FestivalStage || !PendingAutomaticLifecycleCommand.IsSet())
	{
		return;
	}

	const EFFZSChangePhase ExpectedPhase = ToZoneChangePhase(FestivalStage->CurrentStage);
	if (ZoneSystem->GetChangePhase() != ExpectedPhase && !TrySynchronizeStage(false))
	{
		// 명령/복원 직후의 짧은 재진입 차단만 흡수하고 무한 Tick 재시도는 만들지 않습니다.
		if (++AutomaticLifecycleRetryCount <= FFZoneStageBridge::MaxAutomaticLifecycleRetries)
		{
			ScheduleAutomaticLifecycle();
		}
		else
		{
			UE_LOG(LogFFZoneStageBridge, Error,
				TEXT("FestivalStage 기반 시설 개폐를 %d회 재시도했지만 FFZS Phase 동기화에 실패했습니다."),
				FFZoneStageBridge::MaxAutomaticLifecycleRetries);
			AutomaticLifecycleRetryCount = 0;
			PendingAutomaticLifecycleCommand.Reset();
		}
		return;
	}

	AutomaticLifecycleRetryCount = 0;
	const EFFZSCommandType CommandType = PendingAutomaticLifecycleCommand.GetValue();
	PendingAutomaticLifecycleCommand.Reset();
	const bool bCommandMatchesStage =
		(CommandType == EFFZSCommandType::Open
			&& FestivalStage->CurrentStage == EFF_FestivalStage::Live)
		|| (CommandType == EFFZSCommandType::Close
			&& FestivalStage->CurrentStage != EFF_FestivalStage::Live);
	if (!bCommandMatchesStage)
	{
		// 빠른 Stage 전환으로 이미 낡은 edge는 현재 권위 상태를 되돌리지 않게 폐기한다.
		return;
	}

	if (CommandType == EFFZSCommandType::Open)
	{
		ExecuteLifecycleBatch(
			EFFZSCommandType::Open,
			EFF_ZoneFacilityLifecycleAction::AutomaticOpen,
			false);
		return;
	}

	ExecuteLifecycleBatch(
		EFFZSCommandType::Close,
		EFF_ZoneFacilityLifecycleAction::AutomaticClose,
		false);
}

FFF_ZoneFacilityLifecycleBatchResult
UFF_ZoneSystemStageBridgeSubsystem::OpenEligibleFacilities(const bool bIncludeClosedFacilities)
{
	return ExecuteLifecycleBatch(
		EFFZSCommandType::Open,
		EFF_ZoneFacilityLifecycleAction::ManualOpen,
		bIncludeClosedFacilities);
}

FFF_ZoneFacilityLifecycleBatchResult
UFF_ZoneSystemStageBridgeSubsystem::CloseActiveFacilities(const bool bEmergency)
{
	return ExecuteLifecycleBatch(
		bEmergency ? EFFZSCommandType::EmergencyClose : EFFZSCommandType::Close,
		bEmergency
			? EFF_ZoneFacilityLifecycleAction::ManualEmergencyClose
			: EFF_ZoneFacilityLifecycleAction::ManualClose,
		false);
}

FFF_ZoneFacilityLifecycleBatchResult
UFF_ZoneSystemStageBridgeSubsystem::ExecuteLifecycleBatch(
	const EFFZSCommandType CommandType,
	const EFF_ZoneFacilityLifecycleAction Action,
	const bool bIncludeClosedFacilities)
{
	FFF_ZoneFacilityLifecycleBatchResult Batch;
	Batch.Action = Action;
	if (!ZoneSystem)
	{
		Batch.FailedCount = 1;
		Batch.Summary = LOCTEXT("LifecycleMissingZoneSystem", "FF_ZoneSystem을 찾을 수 없습니다.");
		LastLifecycleBatchResult = Batch;
		OnFacilityLifecycleBatchCompleted.Broadcast(Batch);
		return Batch;
	}

	Batch.StartingWorldRevision = ZoneSystem->GetWorldRevision();
	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
	const FGuid TransactionId = FGuid::NewGuid();
	TArray<FGuid> EligibleFacilityIds;
	EligibleFacilityIds.Reserve(ReadModel.Facilities.Num());

	// 먼저 고정 ReadModel에서 대상을 고르면 앞선 명령의 상태 변경이 뒤 시설의 포함 여부를 바꾸지 않는다.
	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		bool bEligible = false;
		if (CommandType == EFFZSCommandType::Open)
		{
			bEligible = !Facility.bNeedsRelocation
				&& (Facility.State == EFFZSFacilityState::Validated
					|| Facility.State == EFFZSFacilityState::MovePending
					|| (bIncludeClosedFacilities
						&& Facility.State == EFFZSFacilityState::Closed));
		}
		else
		{
			bEligible = Facility.State == EFFZSFacilityState::Active;
		}

		if (bEligible)
		{
			EligibleFacilityIds.Add(Facility.FacilityInstanceId);
		}
	}

	Batch.SkippedCount = ReadModel.Facilities.Num() - EligibleFacilityIds.Num();
	// 한 배치의 명령은 TransactionId를 공유하되 낙관적 동시성 revision은 성공마다 최신 값으로 갱신한다.
	for (const FGuid FacilityId : EligibleFacilityIds)
	{
		FFFZSCommand Command;
		Command.CommandId = FGuid::NewGuid();
		Command.TransactionId = TransactionId;
		Command.ExpectedWorldRevision = ZoneSystem->GetWorldRevision();
		Command.Type = CommandType;
		Command.FacilityInstanceId = FacilityId;

		++Batch.AttemptedCount;
		const FFFZSCommandResult Result = ZoneSystem->ExecuteCommand(Command);
		if (Result.bSucceeded)
		{
			++Batch.SucceededCount;
		}
		else
		{
			++Batch.FailedCount;
			Batch.FailedFacilityIds.Add(FacilityId);
			UE_LOG(LogFFZoneStageBridge, Warning,
				TEXT("시설 %s Lifecycle(%d) 명령 실패: %s"),
				*FacilityId.ToString(),
				static_cast<int32>(CommandType),
				*Result.Message.ToString());
		}
	}

	Batch.FinalWorldRevision = ZoneSystem->GetWorldRevision();
	Batch.Summary = FText::Format(
		LOCTEXT(
			"LifecycleBatchSummary",
			"처리 {0}개 / 성공 {1}개 / 실패 {2}개 / 제외 {3}개 (Revision {4} → {5})"),
		FText::AsNumber(Batch.AttemptedCount),
		FText::AsNumber(Batch.SucceededCount),
		FText::AsNumber(Batch.FailedCount),
		FText::AsNumber(Batch.SkippedCount),
		FText::AsNumber(Batch.StartingWorldRevision),
		FText::AsNumber(Batch.FinalWorldRevision));

	LastLifecycleBatchResult = Batch;
	OnFacilityLifecycleBatchCompleted.Broadcast(Batch);
	UE_LOG(LogFFZoneStageBridge, Display, TEXT("%s"), *Batch.Summary.ToString());
	return Batch;
}

FName UFF_ZoneSystemStageBridgeSubsystem::GetFFZoneSystemPolicyName_Implementation() const
{
	return TEXT("FestaFiesta.FestivalStage");
}

EFFZSPolicyDomain UFF_ZoneSystemStageBridgeSubsystem::GetFFZoneSystemPolicyDomain_Implementation() const
{
	return EFFZSPolicyDomain::SimulationTime;
}

FFFZSPolicyDecision UFF_ZoneSystemStageBridgeSubsystem::PreValidateFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context) const
{
	FFFZSPolicyDecision Decision;
	const bool bChangesPlacement = Context.Command.Type == EFFZSCommandType::Install
		|| Context.Command.Type == EFFZSCommandType::Move
		|| Context.Command.Type == EFFZSCommandType::Remove;

	// Stage가 없는 독립 테스트 월드는 플러그인의 자체 ChangePhase 규칙만 사용한다.
	if (bChangesPlacement && FestivalStage && !FestivalStage->IsReconfigureAllowed())
	{
		Decision.bAllowed = false;
		Decision.FailureReason = EFFZSPlacementFailure::WrongStage;
		Decision.Message = LOCTEXT(
			"FestivalStageDisallowsPlacement",
			"현재 축제 단계에서는 시설을 설치·이동·제거할 수 없습니다.");
	}
	return Decision;
}

FFFZSPolicyDecision UFF_ZoneSystemStageBridgeSubsystem::ApplyTentativeFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	// 시간 시스템에는 예약할 외부 자원이 없으므로 같은 읽기 검증만 반복한다.
	return PreValidateFFZoneSystemChange_Implementation(Context);
}

void UFF_ZoneSystemStageBridgeSubsystem::CommitFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	(void)Context;
}

void UFF_ZoneSystemStageBridgeSubsystem::RollbackFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	(void)Context;
}

#undef LOCTEXT_NAMESPACE
