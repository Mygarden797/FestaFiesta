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

void UFF_ZoneSystemStageBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	FestivalStage = GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;

	if (FestivalStage)
	{
		FestivalStage->OnStageChanged.AddUniqueDynamic(
			this, &UFF_ZoneSystemStageBridgeSubsystem::HandleStageChanged);
	}
	else
	{
		UE_LOG(LogFFZoneStageBridge, Warning,
			TEXT("FestivalStageSubsystem이 없어 FF_ZoneSystem 단계 연동을 시작하지 못했습니다."));
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
	if (ZoneSystem && FestivalStage
		&& ZoneSystem->GetChangePhase() != ToZoneChangePhase(FestivalStage->CurrentStage))
	{
		TrySynchronizeStage(false);
	}
}

TStatId UFF_ZoneSystemStageBridgeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_ZoneSystemStageBridgeSubsystem, STATGROUP_Tickables);
}

EFFZSChangePhase UFF_ZoneSystemStageBridgeSubsystem::ToZoneChangePhase(
	const EFF_FestivalStage FestivalStageValue)
{
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
	return TrySynchronizeStage(true);
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
		bStageSyncRetryScheduled = true;
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UFF_ZoneSystemStageBridgeSubsystem::RetryStageSynchronization));
	}
	return false;
}

void UFF_ZoneSystemStageBridgeSubsystem::RetryStageSynchronization()
{
	bStageSyncRetryScheduled = false;
	TrySynchronizeStage(false);
}

void UFF_ZoneSystemStageBridgeSubsystem::HandleStageChanged(
	const EFF_FestivalStage PreviousStage,
	const EFF_FestivalStage NewStage)
{
	(void)PreviousStage;
	(void)NewStage;
	SynchronizeCurrentStage();
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
