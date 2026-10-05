// Fill out your copyright notice in the Description page of Project Settings.

#define FF_STAFF_LOG(Format, ...) \
UE_LOG(LogTemp, Warning, TEXT("[FF_StaffSubsystem] " Format), ##__VA_ARGS__)

#include "Staff/FF_StaffSubsystem.h"

#include "CrowdSystem/FF_BaseCrowd.h"
#include "EngineUtils.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "GameplayEffect.h"
#include "Incident/FF_IncidentSettings.h"
#include "Incident/Rules/FF_IncidentRules.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Staff/FF_StaffActor.h"
#include "Staff/FF_StaffBase.h"
#include "Staff/FF_StaffFacilitySubsystem.h"
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
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}

void UFF_StaffSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Staffing = Collection.InitializeDependency<UFF_StaffFacilitySubsystem>();
	const UFF_IncidentSettings* Settings = UFF_IncidentSettings::Get();
	TravelSpeed = FMath::Max(1.f, Settings->StaffTravelSpeed);
	ResponderClass = Settings->StaffResponderClass.IsNull()
		? AFF_BaseCrowd::StaticClass() : Settings->StaffResponderClass.LoadSynchronous();
	if (!ResponderClass)
	{
		ResponderClass = AFF_BaseCrowd::StaticClass();
	}
}

void UFF_StaffSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UFF_StaffSubsystem::Deinitialize()
{
	TArray<int32> StaffIds;
	Staff.GetKeys(StaffIds);
	for (const int32 StaffId : StaffIds)
	{
		DespawnStaff(StaffId);
	}
	Staffing = nullptr;
	ResponderClass = nullptr;
	Super::Deinitialize();
}

void UFF_StaffSubsystem::Tick(float DeltaTime)
{
	if (DeltaTime <= 0.f || Staff.IsEmpty())
	{
		return;
	}
	TArray<int32> StaffIds;
	Staff.GetKeys(StaffIds);
	for (const int32 StaffId : StaffIds)
	{
		if (FStaffRuntime* Runtime = Staff.Find(StaffId))
		{
			TickRuntime(StaffId, *Runtime, DeltaTime);
		}
	}
}

TStatId UFF_StaffSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(
	UFF_StaffSubsystem,
	STATGROUP_Tickables);
}

bool UFF_StaffSubsystem::SendStaff(const FFF_StaffDispatchOrigin& Origin, const FFF_IncidentAnchor& Target)
{
	UWorld* World = GetWorld();
	if (!World || Staff.Contains(Origin.StaffId))
	{
		return false;
	}
	FStaffRuntime Runtime;
	Runtime.Target = Target;
	Runtime.Position = Origin.Location;
	Runtime.HomeLocation = Origin.Location;
	Runtime.Travelled.Add(Origin.Location);
	if (!ResolveTargetLocation(Runtime, Runtime.PathGoal)
		|| !FindPath(Runtime.Position, Runtime.PathGoal, Runtime.Path))
	{
		return false;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFF_BaseCrowd* Actor = World->SpawnActor<AFF_BaseCrowd>(ResponderClass,
		Runtime.Position, FRotator::ZeroRotator, Parameters);
	if (!Actor)
	{
		return false;
	}
	Actor->SetActorEnableCollision(false);
	Actor->Tags.Add(TEXT("StaffResponder"));
	Runtime.Responder = Actor;
	SetState(Runtime, EFF_StaffState::Moving);
	Staff.Add(Origin.StaffId, MoveTemp(Runtime));
	return true;
}

void UFF_StaffSubsystem::RecallStaff(int32 StaffId)
{
	FStaffRuntime* Runtime = Staff.Find(StaffId);
	if (!Runtime || Runtime->State == EFF_StaffState::Returning)
	{
		return;
	}
	GiveUp(*Runtime);
}

bool UFF_StaffSubsystem::TickRuntime(int32 StaffId, FStaffRuntime& Runtime, float DeltaTime)
{
	Runtime.RepathElapsed += DeltaTime;
	FVector TargetNow;
	switch (Runtime.State)
	{
	case EFF_StaffState::Moving:
		if (Runtime.RepathElapsed >= RepathIntervalSeconds && ResolveTargetLocation(Runtime, TargetNow)
			&& !TargetNow.Equals(Runtime.PathGoal, RepathTargetMoveCm))
		{
			Runtime.RepathElapsed = 0.f;
			Runtime.Travelled.Add(Runtime.Position);
			if (!FindPath(Runtime.Position, TargetNow, Runtime.Path))
			{
				GiveUp(Runtime);
				return true;
			}
			Runtime.PathGoal = TargetNow;
			Runtime.NextIndex = 0;
		}
		if (!MoveStaff(Runtime, DeltaTime))
		{
			return true;
		}
		if (!ResolveTargetLocation(Runtime, TargetNow) || !TargetNow.Equals(Runtime.PathGoal, RepathTargetMoveCm))
		{
			return true;
		}
		if (FVector::DistSquared(Runtime.Position, TargetNow) > FMath::Square(ArrivalRadiusCm))
		{
			GiveUp(Runtime);
			return true;
		}
		HoldAtTarget(StaffId, Runtime);
		return true;

	case EFF_StaffState::Working:
		if (ResolveTargetLocation(Runtime, TargetNow) && !TargetNow.Equals(Runtime.PathGoal, WorkLeaveRadiusCm))
		{
			SetState(Runtime, EFF_StaffState::Moving);
		}
		return true;

	case EFF_StaffState::Returning:
		if (Runtime.RepathElapsed >= RepathIntervalSeconds)
		{
			Runtime.RepathElapsed = 0.f;
			if (!RefreshReturnHome(StaffId, Runtime))
			{
				DespawnStaff(StaffId);
				return false;
			}
		}
		if (MoveStaff(Runtime, DeltaTime))
		{
			DespawnStaff(StaffId);
			return false;
		}
		return true;

	default:
		return true;
	}
}

bool UFF_StaffSubsystem::MoveStaff(FStaffRuntime& Runtime, float DeltaTime)
{
	const FVector Previous = Runtime.Position;
	const bool bArrived = AdvanceAlongPath(Runtime, TravelSpeed * DeltaTime);
	if (AFF_BaseCrowd* Actor = Runtime.Responder.Get())
	{
		const FVector Movement = Runtime.Position - Previous;
		const FRotator Rotation = Movement.IsNearlyZero() ? Actor->GetActorRotation() : Movement.Rotation();
		Actor->PushPresentation(Runtime.Position, Rotation, Movement.Size() / DeltaTime);
	}
	return bArrived;
}

void UFF_StaffSubsystem::HoldAtTarget(int32 StaffId, FStaffRuntime& Runtime)
{
	SetState(Runtime, EFF_StaffState::Working);
	if (AFF_BaseCrowd* Actor = Runtime.Responder.Get())
	{
		Actor->PushPresentation(Runtime.Position, Actor->GetActorRotation(), 0.f);
	}
	OnStaffArrived.Broadcast(StaffId);
}

void UFF_StaffSubsystem::GiveUp(FStaffRuntime& Runtime)
{
	ReverseTravelledPath(Runtime);
	SetState(Runtime, EFF_StaffState::Returning);
	Runtime.RepathElapsed = RepathIntervalSeconds;
}

bool UFF_StaffSubsystem::ResolveTargetLocation(const FStaffRuntime& Runtime, FVector& OutLocation) const
{
	return FFF_IncidentRules::TryGetAnchorLocation(GetWorld(), Runtime.Target, OutLocation);
}

bool UFF_StaffSubsystem::RefreshReturnHome(int32 StaffId, FStaffRuntime& Runtime)
{
	if (!Staffing)
	{
		return true;
	}
	FFF_StaffDispatchOrigin CurrentHome;
	const bool bHasHome = Staffing->GetStaffDispatchOrigin(StaffId, CurrentHome);
	if (bHasHome && Runtime.HomeLocation.Equals(CurrentHome.Location, 1.f))
	{
		return true;
	}
	if (bHasHome && FindPath(Runtime.Position, CurrentHome.Location, Runtime.Path))
	{
		Runtime.HomeLocation = CurrentHome.Location;
		Runtime.NextIndex = 0;
		return true;
	}
	return false;
}

void UFF_StaffSubsystem::DespawnStaff(int32 StaffId)
{
	FStaffRuntime Runtime;
	if (!Staff.RemoveAndCopyValue(StaffId, Runtime))
	{
		return;
	}
	if (AFF_BaseCrowd* Actor = Runtime.Responder.Get())
	{
		Actor->Destroy();
	}
}

bool UFF_StaffSubsystem::IsIdle(int32 StaffId) const
{
	return GetStaffState(StaffId) == EFF_StaffState::Idle;
}

EFF_StaffState UFF_StaffSubsystem::GetStaffState(int32 StaffId) const
{
	const FStaffRuntime* Runtime = Staff.Find(StaffId);
	return Runtime ? Runtime->State : EFF_StaffState::Idle;
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

bool UFF_StaffSubsystem::FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const
{
	OutPath.Reset();
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation)
	{
		return false;
	}
	FNavLocation NavStart;
	FNavLocation NavEnd;
	const FVector ProjectionExtent(200.f, 200.f, 400.f);
	if (!Navigation->ProjectPointToNavigation(From, NavStart, ProjectionExtent)
		|| !Navigation->ProjectPointToNavigation(To, NavEnd, ProjectionExtent))
	{
		return false;
	}
	if (FVector::Dist2D(From, NavStart.Location) > 100.f || FMath::Abs(From.Z - NavStart.Location.Z) > 100.f
		|| FVector::Dist2D(To, NavEnd.Location) > 100.f || FMath::Abs(To.Z - NavEnd.Location.Z) > 100.f)
	{
		return false;
	}
	const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
		GetWorld(), NavStart.Location, NavEnd.Location);
	if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.IsEmpty())
	{
		return false;
	}
	OutPath.Add(From);
	OutPath.Append(Path->PathPoints);
	return true;
}

bool UFF_StaffSubsystem::AdvanceAlongPath(FStaffRuntime& Runtime, float Distance)
{
	while (Runtime.Path.IsValidIndex(Runtime.NextIndex))
	{
		const FVector Target = Runtime.Path[Runtime.NextIndex];
		const float Remaining = FVector::Distance(Runtime.Position, Target);
		if (Remaining > Distance && Remaining > UE_SMALL_NUMBER)
		{
			Runtime.Position += (Target - Runtime.Position) * (Distance / Remaining);
			return false;
		}
		Runtime.Position = Target;
		Distance = FMath::Max(0.f, Distance - Remaining);
		if (Runtime.Travelled.IsEmpty() || !Runtime.Travelled.Last().Equals(Target))
		{
			Runtime.Travelled.Add(Target);
		}
		++Runtime.NextIndex;
	}
	return true;
}

void UFF_StaffSubsystem::ReverseTravelledPath(FStaffRuntime& Runtime)
{
	Runtime.Path.Reset(Runtime.Travelled.Num());
	for (int32 Index = Runtime.Travelled.Num() - 1; Index >= 0; --Index)
	{
		Runtime.Path.Add(Runtime.Travelled[Index]);
	}
	Runtime.NextIndex = 0;
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
	Runtime.State = NewState;
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
