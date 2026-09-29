// Fill out your copyright notice in the Description page of Project Settings.

#define FF_STAFF_LOG(Format, ...) \
UE_LOG(LogTemp, Warning, TEXT("[FF_StaffSubsystem] " Format), ##__VA_ARGS__)

#include "Staff/FF_StaffSubsystem.h"

#include "CrowdSystem/Systems/FF_CrowdPathFollow.h"
#include "EngineUtils.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "GameplayEffect.h"
#include "NavigationSystem.h"
#include "Staff/FF_StaffActor.h"
#include "Staff/FF_StaffBase.h"
#include "Time/FF_FestivalStageSubsystem.h"


#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GFF_StaffSendCommand(
	TEXT("FF.Staff.Send"),
	TEXT("FF.Staff.Send <StaffId> <X> <Y> <Z>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
	}));

static FAutoConsoleCommandWithWorldAndArgs GFF_StaffRecallCommand(
	TEXT("FF.Staff.Recall"),
	TEXT("FF.Staff.Recall <StaffId>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
	}));
#endif


bool UFF_StaffSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return Super::DoesSupportWorldType(WorldType);
}

void UFF_StaffSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UFF_StaffSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UFF_StaffSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UFF_StaffSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

TStatId UFF_StaffSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(
	UFF_StaffSubsystem,
	STATGROUP_Tickables);
}

bool UFF_StaffSubsystem::SendStaff(int32 StaffId, const FFF_IncidentAnchor& Target)
{
	return false;
}

void UFF_StaffSubsystem::RecallStaff(int32 StaffId)
{
}

bool UFF_StaffSubsystem::IsIdle(int32 StaffId) const
{
	return false;
}

EFF_StaffState UFF_StaffSubsystem::GetStaffState(int32 StaffId) const
{
	return EFF_StaffState::Idle;
}

AFF_StaffActor* UFF_StaffSubsystem::FindStaffActor(int32 StaffId) const
{
	return nullptr;
}

bool UFF_StaffSubsystem::ApplyEffectToStaff(int32 StaffId, TSubclassOf<UGameplayEffect> Effect)
{
	return false;
}

void UFF_StaffSubsystem::SyncActorsWithRoster()
{
}

void UFF_StaffSubsystem::HandleStaffRosterChanged()
{
}

void UFF_StaffSubsystem::BeginMove(FStaffRuntime& Runtime, EFF_StaffState MoveState,
                                   const FFF_IncidentAnchor& Target, float NowMinutes)
{
}

bool UFF_StaffSubsystem::ResolveTargetLocation(const FFF_IncidentAnchor& Target, FVector& OutLocation) const
{
	return false;
}

bool UFF_StaffSubsystem::FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const
{
	return false;
}

bool UFF_StaffSubsystem::ShouldRepath(const FStaffRuntime& Runtime, const FVector& TargetNow,
                                      float NowMinutes) const
{
	return false;
}

bool UFF_StaffSubsystem::Repath(FStaffRuntime& Runtime, const FVector& TargetNow, float NowMinutes)
{
	return false;
}

bool UFF_StaffSubsystem::TickMove(FStaffRuntime& Runtime, const FVector& TargetNow, float SimDeltaSeconds)
{
	return false;
}

float UFF_StaffSubsystem::GetMoveSpeed(const FStaffRuntime& Runtime) const
{
	return 0.f;
}

bool UFF_StaffSubsystem::CheckProgress(FStaffRuntime& Runtime, const FVector& TargetNow, float NowMinutes)
{
	return false;
}

void UFF_StaffSubsystem::WarpToTarget(int32 StaffId, FStaffRuntime& Runtime, const FVector& TargetNow)
{
}

void UFF_StaffSubsystem::SetState(FStaffRuntime& Runtime, EFF_StaffState NewState)
{
}

void UFF_StaffSubsystem::HandleArrived(int32 StaffId)
{
}

void UFF_StaffSubsystem::HandleTargetLost(int32 StaffId)
{
}

FFF_IncidentAnchor UFF_StaffSubsystem::MakeHomeAnchor(const FStaffRuntime& Runtime) const
{
	return FFF_IncidentAnchor();
}

bool UFF_StaffSubsystem::TryGetNowMinutes(float& OutMinutes) const
{
	return false;
}

float UFF_StaffSubsystem::GetEffectiveTimeScale() const
{
	return 1.f;
}
