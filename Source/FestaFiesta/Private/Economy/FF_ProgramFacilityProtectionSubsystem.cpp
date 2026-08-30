// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_ProgramFacilityProtectionSubsystem.h"

#include "Economy/FF_EconomyLog.h"
#include "Economy/FF_ProgramSchedulingSubsystem.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Subsystems/SubsystemCollection.h"

#define LOCTEXT_NAMESPACE "FFProgramFacilityProtection"

namespace
{
	FFFZSPolicyDecision AllowedDecision()
	{
		return FFFZSPolicyDecision();
	}

	FFFZSPolicyDecision RejectedDecision(const FText& Message)
	{
		FFFZSPolicyDecision Decision;
		Decision.bAllowed = false;
		Decision.FailureReason = EFFZSPlacementFailure::ExternalPolicyRejected;
		Decision.Message = Message;
		return Decision;
	}
}

void UFF_ProgramFacilityProtectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Collection.InitializeDependency<UFF_ProgramSchedulingSubsystem>();
	Super::Initialize(Collection);

	ZoneSystem = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	bPolicyRegistered = ZoneSystem && ZoneSystem->RegisterExternalPolicy(this);
	if (!bPolicyRegistered)
	{
		UE_LOG(LogFestaFiestaEconomy, Error,
			TEXT("[유료 프로그램] 편성 시설 보호 정책을 Zone System에 등록하지 못했습니다."));
	}
}

void UFF_ProgramFacilityProtectionSubsystem::Deinitialize()
{
	if (bPolicyRegistered && ZoneSystem)
	{
		ZoneSystem->UnregisterExternalPolicy(this);
	}
	bPolicyRegistered = false;
	ZoneSystem = nullptr;
	Super::Deinitialize();
}

bool UFF_ProgramFacilityProtectionSubsystem::DoesSupportWorldType(
	const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game
		|| WorldType == EWorldType::PIE
		|| WorldType == EWorldType::GamePreview;
}

FName UFF_ProgramFacilityProtectionSubsystem::GetFFZoneSystemPolicyName_Implementation() const
{
	return TEXT("FestaFiesta.PaidProgramFacilityProtection");
}

EFFZSPolicyDomain
UFF_ProgramFacilityProtectionSubsystem::GetFFZoneSystemPolicyDomain_Implementation() const
{
	// Custom은 여러 독립 정책을 허용한다. 기존 Economy/Queue 정책의 단일 도메인을 침범하지 않는다.
	return EFFZSPolicyDomain::Custom;
}

FFFZSPolicyDecision
UFF_ProgramFacilityProtectionSubsystem::PreValidateFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context) const
{
	if (Context.Command.Type != EFFZSCommandType::Move
		&& Context.Command.Type != EFFZSCommandType::Remove)
	{
		return AllowedDecision();
	}

	const UFF_ProgramSchedulingSubsystem* Scheduling = GetWorld()
		? GetWorld()->GetSubsystem<UFF_ProgramSchedulingSubsystem>()
		: nullptr;
	if (!Scheduling)
	{
		return RejectedDecision(LOCTEXT(
			"SchedulingUnavailable",
			"프로그램 편성 상태를 확인할 수 없어 시설을 변경할 수 없습니다."));
	}
	if (!Scheduling->HasScheduledProgramForFacility(Context.Command.FacilityInstanceId))
	{
		return AllowedDecision();
	}

	return RejectedDecision(Context.Command.Type == EFFZSCommandType::Move
		? LOCTEXT("ScheduledFacilityMove", "오늘 프로그램이 편성된 시설은 이동할 수 없습니다.")
		: LOCTEXT("ScheduledFacilityRemove", "오늘 프로그램이 편성된 시설은 철거할 수 없습니다."));
}

FFFZSPolicyDecision
UFF_ProgramFacilityProtectionSubsystem::ApplyTentativeFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	(void)Context;
	return AllowedDecision();
}

void UFF_ProgramFacilityProtectionSubsystem::CommitFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	(void)Context;
}

void UFF_ProgramFacilityProtectionSubsystem::RollbackFFZoneSystemChange_Implementation(
	const FFFZSPolicyContext& Context)
{
	(void)Context;
}

#undef LOCTEXT_NAMESPACE
