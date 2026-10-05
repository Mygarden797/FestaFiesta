// Fill out your copyright notice in the Description page of Project Settings.

#define FF_STAFF_LOG(Format, ...) \
UE_LOG(LogTemp, Warning, TEXT("[FF_StaffSubsystem] " Format), ##__VA_ARGS__)

#include "Staff/FF_StaffSubsystem.h"

#include "Engine/GameInstance.h"
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
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	Stage = GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	const UFF_IncidentSettings* Settings = UFF_IncidentSettings::Get();
	TravelSpeed = FMath::Max(1.f, Settings->StaffTravelSpeed);
	StaffActorClass = Settings->StaffActorClass.IsNull()
		? AFF_StaffActor::StaticClass() : Settings->StaffActorClass.LoadSynchronous();
	if (!StaffActorClass)
	{
		StaffActorClass = AFF_StaffActor::StaticClass();
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
	Stage = nullptr;
	StaffActorClass = nullptr;
	Super::Deinitialize();
}

void UFF_StaffSubsystem::Tick(float DeltaTime)
{
	const float SimDeltaTime = DeltaTime * GetEffectiveTimeScale();
	if (SimDeltaTime <= 0.f || Staff.IsEmpty())
	{
		return;
	}
	TArray<int32> StaffIds;
	Staff.GetKeys(StaffIds);
	for (const int32 StaffId : StaffIds)
	{
		if (FStaffRuntime* Runtime = Staff.Find(StaffId))
		{
			TickRuntime(StaffId, *Runtime, SimDeltaTime);
		}
	}
}

TStatId UFF_StaffSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(
	UFF_StaffSubsystem,
	STATGROUP_Tickables);
}

bool UFF_StaffSubsystem::SendStaff(const FFF_StaffDispatchOrigin& Origin, const FFF_IncidentAnchor& Target,
	const FVector& SpreadOffset)
{
	UWorld* World = GetWorld();
	if (!World || Staff.Contains(Origin.StaffId))
	{
		return false;
	}
	FStaffRuntime Runtime;
	Runtime.Target = Target;
	Runtime.SpreadOffset = FVector(SpreadOffset.X, SpreadOffset.Y, 0.f);
	Runtime.Position = Origin.Location + Runtime.SpreadOffset;
	Runtime.HomeLocation = Origin.Location;
	Runtime.Travelled.Add(Origin.Location);
	if (!Runtime.SpreadOffset.IsNearlyZero())
	{
		Runtime.Travelled.Add(Runtime.Position);
	}
	if (!ResolveTargetLocation(Runtime, Runtime.PathGoal))
	{
		return false;
	}
	const bool bHasPath = FindSpreadPath(Runtime, Runtime.Position, Runtime.PathGoal, Runtime.Path);
	ResetProgress(Runtime, Runtime.PathGoal);
	Runtime.FailCount = bHasPath ? 0 : 1;
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFF_StaffActor* Actor = World->SpawnActor<AFF_StaffActor>(
		StaffActorClass ? StaffActorClass.Get() : AFF_StaffActor::StaticClass(),
		Runtime.Position, FRotator::ZeroRotator, Parameters);
	if (!Actor)
	{
		return false;
	}
	Actor->SetActorEnableCollision(false);
	Actor->SetStaffId(Origin.StaffId);
	Runtime.Actor = Actor;
	SetState(Runtime, EFF_StaffState::Moving);
	Staff.Add(Origin.StaffId, MoveTemp(Runtime));
	return true;
}

void UFF_StaffSubsystem::MakeSpreadOffsets(int32 Count, TArray<FVector>& OutOffsets)
{
	OutOffsets.Init(FVector::ZeroVector, FMath::Max(0, Count));
	if (Count < 2)
	{
		return;
	}
	const float MinRadius = SpreadSpacingCm * 0.5f / FMath::Sin(PI / Count);
	const float BaseAngle = FMath::FRandRange(0.f, 2.f * PI);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = BaseAngle + 2.f * PI * Index / Count;
		const float Radius = MinRadius + FMath::FRandRange(0.f, SpreadJitterCm);
		OutOffsets[Index] = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
	}
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
	{
		if (!ResolveTargetLocation(Runtime, TargetNow))
		{
			return true;
		}
		const bool bTargetMoved = !TargetNow.Equals(Runtime.PathGoal, RepathTargetMoveCm);
		const bool bPathEnded = !Runtime.Path.IsValidIndex(Runtime.NextIndex);
		const bool bShortOfTarget = FVector::DistSquared(Runtime.Position, TargetNow) > FMath::Square(ArrivalRadiusCm);
		if ((bTargetMoved || (bPathEnded && bShortOfTarget)) && Runtime.RepathElapsed >= RepathIntervalSeconds)
		{
			Runtime.RepathElapsed = 0.f;
			Runtime.Travelled.Add(Runtime.Position);
			TArray<FVector> NewPath;
			if (FindSpreadPath(Runtime, Runtime.Position, TargetNow, NewPath))
			{
				Runtime.Path = MoveTemp(NewPath);
				Runtime.NextIndex = 0;
				Runtime.PathGoal = TargetNow;
				Runtime.BestDistance = GetRemainingDistance(Runtime, TargetNow);
			}
			else
			{
				++Runtime.FailCount;
			}
		}
		const bool bArrived = MoveStaff(Runtime, DeltaTime);
		if (bArrived && FVector::DistSquared(Runtime.Position, TargetNow) <= FMath::Square(ArrivalRadiusCm))
		{
			HoldAtTarget(StaffId, Runtime, TargetNow);
			return true;
		}
		if (CheckProgress(Runtime, TargetNow, DeltaTime))
		{
			WarpToTarget(StaffId, Runtime, TargetNow);
		}
		return true;
	}

	case EFF_StaffState::Working:
		if (ResolveTargetLocation(Runtime, TargetNow) && !TargetNow.Equals(Runtime.PathGoal, WorkLeaveRadiusCm))
		{
			ResetProgress(Runtime, TargetNow);
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
	if (AFF_StaffActor* Actor = Runtime.Actor.Get())
	{
		const FVector Movement = Runtime.Position - Previous;
		const FRotator Rotation = Movement.IsNearlyZero() ? Actor->GetActorRotation() : Movement.Rotation();
		Actor->PushPresentation(Runtime.Position, Rotation, Movement.Size() / DeltaTime);
	}
	return bArrived;
}

void UFF_StaffSubsystem::HoldAtTarget(int32 StaffId, FStaffRuntime& Runtime, const FVector& TargetNow)
{
	Runtime.PathGoal = TargetNow;
	SetState(Runtime, EFF_StaffState::Working);
	if (AFF_StaffActor* Actor = Runtime.Actor.Get())
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
	if (!FFF_IncidentRules::TryGetAnchorLocation(GetWorld(), Runtime.Target, OutLocation))
	{
		return false;
	}
	OutLocation += Runtime.SpreadOffset;
	return true;
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
	if (AFF_StaffActor* Actor = Runtime.Actor.Get())
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
	const FStaffRuntime* Runtime = Staff.Find(StaffId);
	return Runtime ? Runtime->Actor.Get() : nullptr;
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
		FF_STAFF_LOG("경로 실패[내비 없음] %s → %s", *From.ToCompactString(), *To.ToCompactString());
		return false;
	}
	FVector NavStart;
	FVector NavEnd;
	if (!ProjectEndpoint(*Navigation, From, TEXT("출발"), NavStart)
		|| !ProjectEndpoint(*Navigation, To, TEXT("목표"), NavEnd))
	{
		return false;
	}
	const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), NavStart, NavEnd);
	if (!Path || !Path->IsValid() || Path->PathPoints.IsEmpty())
	{
		FF_STAFF_LOG("경로 실패[%s] 내비 출발 %s → 내비 목표 %s",
			!Path ? TEXT("경로 객체 없음") : !Path->IsValid() ? TEXT("무효 경로") : TEXT("점 없음"),
			*NavStart.ToCompactString(), *NavEnd.ToCompactString());
		return false;
	}
	if (Path->IsPartial())
	{
		FF_STAFF_LOG("중간 끊김 경로 사용: 내비 출발 %s → 내비 목표 %s, 경로 끝 %s (목표까지 %.0f)",
			*NavStart.ToCompactString(), *NavEnd.ToCompactString(),
			*Path->PathPoints.Last().ToCompactString(), FVector::Dist(Path->PathPoints.Last(), NavEnd));
	}
	AssemblePath(From, To, Path->PathPoints, Path->IsPartial(), OutPath);
	return true;
}

bool UFF_StaffSubsystem::FindSpreadPath(const FStaffRuntime& Runtime, const FVector& From, const FVector& To,
	TArray<FVector>& OutPath) const
{
	if (!FindPath(From - Runtime.SpreadOffset, To - Runtime.SpreadOffset, OutPath))
	{
		return false;
	}
	ShiftPath(OutPath, Runtime.SpreadOffset);
	return true;
}

void UFF_StaffSubsystem::ShiftPath(TArray<FVector>& Path, const FVector& Offset)
{
	for (FVector& Point : Path)
	{
		Point += Offset;
	}
}

bool UFF_StaffSubsystem::ProjectEndpoint(const UNavigationSystemV1& Navigation, const FVector& Point,
	const TCHAR* Label, FVector& OutNavLocation)
{
	const FVector NearExtent(200.f, 200.f, 400.f);
	FNavLocation Near;
	const bool bNearProjected = Navigation.ProjectPointToNavigation(Point, Near, NearExtent);
	if (bNearProjected && IsProjectionAcceptable(Point, Near.Location, NearProjectionGapCm))
	{
		OutNavLocation = Near.Location;
		return true;
	}
	const FVector WideExtent(WideProjectionHorizontalCm, WideProjectionHorizontalCm, ProjectionMaxGapZCm);
	FNavLocation Wide;
	if (Navigation.ProjectPointToNavigation(Point, Wide, WideExtent)
		&& IsProjectionAcceptable(Point, Wide.Location, WideExtent.Size2D()))
	{
		FF_STAFF_LOG("넓은 투영 사용[%s] %s → 내비 %s (수평 %.0f, 수직 %.0f). 좁은 투영: %s",
			Label, *Point.ToCompactString(), *Wide.Location.ToCompactString(),
			FVector::Dist2D(Point, Wide.Location), FMath::Abs(Point.Z - Wide.Location.Z),
			bNearProjected ? *FString::Printf(TEXT("%s (수평 %.0f, 수직 %.0f)"), *Near.Location.ToCompactString(),
				FVector::Dist2D(Point, Near.Location), FMath::Abs(Point.Z - Near.Location.Z)) : TEXT("없음"));
		OutNavLocation = Wide.Location;
		return true;
	}
	FF_STAFF_LOG("경로 실패[투영 불가 %s] %s. 좁은 투영: %s, 넓은 투영(수평 %.0f, 수직 %.0f): 없음",
		Label, *Point.ToCompactString(),
		bNearProjected ? *FString::Printf(TEXT("%s (수평 %.0f, 수직 %.0f)"), *Near.Location.ToCompactString(),
			FVector::Dist2D(Point, Near.Location), FMath::Abs(Point.Z - Near.Location.Z)) : TEXT("없음"),
		WideProjectionHorizontalCm, ProjectionMaxGapZCm);
	return false;
}

bool UFF_StaffSubsystem::IsProjectionAcceptable(const FVector& Point, const FVector& Projected, float MaxGap2DCm)
{
	return FVector::Dist2D(Point, Projected) <= MaxGap2DCm && FMath::Abs(Point.Z - Projected.Z) <= ProjectionMaxGapZCm;
}

void UFF_StaffSubsystem::AssemblePath(const FVector& From, const FVector& To, const TArray<FVector>& NavPoints,
	bool bPartial, TArray<FVector>& OutPath)
{
	OutPath.Reset(NavPoints.Num() + 2);
	OutPath.Add(From);
	for (const FVector& Point : NavPoints)
	{
		if (!OutPath.Last().Equals(Point, 1.f))
		{
			OutPath.Add(Point);
		}
	}
	if (!bPartial && !OutPath.Last().Equals(To, 1.f))
	{
		OutPath.Add(To);
	}
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

float UFF_StaffSubsystem::GetRemainingDistance(const FStaffRuntime& Runtime, const FVector& TargetNow)
{
	float Remaining = 0.f;
	FVector From = Runtime.Position;
	for (int32 Index = FMath::Max(0, Runtime.NextIndex); Index < Runtime.Path.Num(); ++Index)
	{
		Remaining += FVector::Distance(From, Runtime.Path[Index]);
		From = Runtime.Path[Index];
	}
	return Remaining + FVector::Distance(From, TargetNow);
}

void UFF_StaffSubsystem::ResetProgress(FStaffRuntime& Runtime, const FVector& TargetNow)
{
	Runtime.BestDistance = GetRemainingDistance(Runtime, TargetNow);
	Runtime.StallElapsed = 0.f;
	Runtime.FailCount = 0;
}

bool UFF_StaffSubsystem::CheckProgress(FStaffRuntime& Runtime, const FVector& TargetNow, float DeltaTime)
{
	const float Remaining = GetRemainingDistance(Runtime, TargetNow);
	if (Remaining <= Runtime.BestDistance - MinProgressCm)
	{
		Runtime.BestDistance = Remaining;
		Runtime.StallElapsed = 0.f;
		Runtime.FailCount = 0;
		return false;
	}
	Runtime.StallElapsed += DeltaTime;
	if (Runtime.StallElapsed >= StallTimeoutSeconds)
	{
		Runtime.StallElapsed = 0.f;
		++Runtime.FailCount;
	}
	return Runtime.FailCount >= MaxFailCount;
}

void UFF_StaffSubsystem::WarpToTarget(int32 StaffId, FStaffRuntime& Runtime, const FVector& TargetNow)
{
	FF_STAFF_LOG("스태프 %d 순간이동: 실패 %d회, %s → %s", StaffId, Runtime.FailCount,
		*Runtime.Position.ToCompactString(), *TargetNow.ToCompactString());
	Runtime.Position = TargetNow;
	Runtime.Path.Reset();
	Runtime.Path.Add(TargetNow);
	Runtime.NextIndex = Runtime.Path.Num();
	Runtime.Travelled.Reset();
	Runtime.Travelled.Add(TargetNow);
	ResetProgress(Runtime, TargetNow);
	HoldAtTarget(StaffId, Runtime, TargetNow);
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
	return Stage ? FMath::Max(0.f, Stage->GetEffectiveTimeScale()) : 1.f;
}
