#include "Incident/FF_StaffDispatchSubsystem.h"

#include "CrowdSystem/FF_BaseCrowd.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Incident/FF_IncidentSettings.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Incident/Rules/FF_IncidentRules.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Staff/FF_StaffFacilitySubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"

bool UFF_StaffDispatchSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}

void UFF_StaffDispatchSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Incident = Collection.InitializeDependency<UFF_IncidentSubsystem>();
	Staffing = Collection.InitializeDependency<UFF_StaffFacilitySubsystem>();
	if (Staffing)
	{
		Staffing->OnStaffReservationsReset.AddUniqueDynamic(this, &ThisClass::HandleReservationsReset);
	}
	if (Incident)
	{
		Incident->OnIncidentClosed.AddUniqueDynamic(this, &ThisClass::HandleIncidentClosed);
	}
	const UFF_IncidentSettings* Settings = UFF_IncidentSettings::Get();
	TravelSpeed = FMath::Max(1.f, Settings->StaffTravelSpeed);
	ResponderClass = Settings->StaffResponderClass.IsNull()
		? AFF_BaseCrowd::StaticClass() : Settings->StaffResponderClass.LoadSynchronous();
	if (!ResponderClass)
	{
		ResponderClass = AFF_BaseCrowd::StaticClass();
	}
}

void UFF_StaffDispatchSubsystem::Deinitialize()
{
	if (Incident)
	{
		Incident->OnIncidentClosed.RemoveDynamic(this, &ThisClass::HandleIncidentClosed);
	}
	if (Staffing)
	{
		Staffing->OnStaffReservationsReset.RemoveDynamic(this, &ThisClass::HandleReservationsReset);
	}
	HandleReservationsReset();
	Incident = nullptr;
	Staffing = nullptr;
	ResponderClass = nullptr;
	Super::Deinitialize();
}

bool UFF_StaffDispatchSubsystem::IsTickable() const
{
	return Super::IsTickable() && !Dispatches.IsEmpty();
}

TStatId UFF_StaffDispatchSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_StaffDispatchSubsystem, STATGROUP_Tickables);
}

bool UFF_StaffDispatchSubsystem::TryGetLocation(const FFF_IncidentAnchor& Anchor, FVector& OutLocation) const
{
	//IncidentRules에 옮기게 되었습니다
	return FFF_IncidentRules::TryGetAnchorLocation(GetWorld(), Anchor, OutLocation);
}

bool UFF_StaffDispatchSubsystem::BuildPath(const FVector& Start, const FVector& End, TArray<FVector>& OutPath) const
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
	if (!Navigation->ProjectPointToNavigation(Start, NavStart, ProjectionExtent)
		|| !Navigation->ProjectPointToNavigation(End, NavEnd, ProjectionExtent))
	{
		return false;
	}
	// A successful projection alone is insufficient: do not resolve from a distant floor or island.
	if (FVector::Dist2D(Start, NavStart.Location) > 100.f || FMath::Abs(Start.Z - NavStart.Location.Z) > 100.f
		|| FVector::Dist2D(End, NavEnd.Location) > 100.f || FMath::Abs(End.Z - NavEnd.Location.Z) > 100.f)
	{
		return false;
	}
	const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
		GetWorld(), NavStart.Location, NavEnd.Location);
	if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.IsEmpty())
	{
		return false;
	}
	// Keep the facility's actual departure point: projecting to navigation must not teleport the visual.
	OutPath.Add(Start);
	OutPath.Append(Path->PathPoints);
	return true;
}

bool UFF_StaffDispatchSubsystem::IsResponding(FFF_IncidentHandle Handle) const
{
	return Dispatches.ContainsByPredicate([Handle](const FDispatch& Dispatch)
	{
		return Dispatch.Handle == Handle && !Dispatch.bReturning;
	});
}

bool UFF_StaffDispatchSubsystem::GetRemainingWorkMinutes(FFF_IncidentHandle Handle, float& OutMinutes) const
{
	const FDispatch* Found = Dispatches.FindByPredicate([Handle](const FDispatch& Dispatch)
	{
		return Dispatch.Handle == Handle && Dispatch.bWorking && !Dispatch.bReturning;
	});
	if (!Found)
	{
		return false;
	}
	OutMinutes = Found->RemainingWorkMinutes;
	return true;
}

// 작업 시간은 배속과 맞아야 하므로 실시간 DeltaTime 이 아니라 축제 시계로 흘린다. Live 가 아니면 작업도 멈춘다.
bool UFF_StaffDispatchSubsystem::TryGetFestivalMinutes(float& OutMinutes) const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	return Stage && Stage->TryGetLiveElapsedMinutes(OutMinutes);
}

bool UFF_StaffDispatchSubsystem::TryDispatch(FFF_IncidentHandle Handle, int32 OptionIndex,
	int32 RequiredStaff, const FFF_IncidentAnchor& Anchor, float WorkAmount)
{
	if (!Staffing || !Incident || !Incident->IsHandleValid(Handle) || RequiredStaff <= 0 || IsResponding(Handle))
	{
		return false;
	}
	FDispatch Pending;
	Pending.Handle = Handle;
	Pending.OptionIndex = OptionIndex;
	Pending.Anchor = Anchor;
	if (!TryGetLocation(Anchor, Pending.Destination))
	{
		return false;
	}
	TArray<FFF_StaffDispatchOrigin> Origins;
	if (!Staffing->TryReserveStaff(RequiredStaff, Pending.Destination, Origins))
	{
		return false;
	}
	// Register all reservations in Pending before any fallible work, so rollback is always complete.
	for (const FFF_StaffDispatchOrigin& Origin : Origins)
	{
		FJourney& Journey = Pending.Journeys.AddDefaulted_GetRef();
		Journey.StaffId = Origin.StaffId;
		Journey.Position = Origin.Location;
		Journey.HomeLocation = Origin.Location;
		Journey.Travelled.Add(Origin.Location);
	}
	for (FJourney& Journey : Pending.Journeys)
	{
		if (!BuildPath(Journey.Position, Pending.Destination, Journey.Path))
		{
			ReleaseDispatch(Pending);
			return false;
		}
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AFF_BaseCrowd* Actor = GetWorld()->SpawnActor<AFF_BaseCrowd>(ResponderClass,
			Journey.Position, FRotator::ZeroRotator, Parameters);
		if (!Actor)
		{
			ReleaseDispatch(Pending);
			return false;
		}
		Actor->SetActorEnableCollision(false);
		Actor->Tags.Add(TEXT("StaffResponder"));
		Journey.Actor = Actor;
	}
	Pending.RemainingWorkMinutes = WorkAmount > 0.f ? WorkAmount / static_cast<float>(Pending.Journeys.Num()) : 0.f;
	if (Dispatches.IsEmpty())
	{
		LastWorkClockMinutes = -1.f;
	}
	Dispatches.Add(MoveTemp(Pending));
	Incident->SetResponding(Handle,true);
	Incident->OnIncidentUpdated.Broadcast(Handle);
	return true;
}

bool UFF_StaffDispatchSubsystem::AdvanceJourney(FJourney& Journey, float Distance)
{
	while (Journey.Path.IsValidIndex(Journey.NextPoint))
	{
		const FVector Target = Journey.Path[Journey.NextPoint];
		const float Remaining = FVector::Distance(Journey.Position, Target);
		if (Remaining > Distance && Remaining > UE_SMALL_NUMBER)
		{
			Journey.Position += (Target - Journey.Position) * (Distance / Remaining);
			return false;
		}
		Journey.Position = Target;
		Distance = FMath::Max(0.f, Distance - Remaining);
		if (Journey.Travelled.IsEmpty() || !Journey.Travelled.Last().Equals(Target))
		{
			Journey.Travelled.Add(Target);
		}
		++Journey.NextPoint;
	}
	return true;
}

void UFF_StaffDispatchSubsystem::BeginReturn(FDispatch& Dispatch)
{
	if (Dispatch.bReturning)
	{
		return;
	}
	Dispatch.bReturning = true;
	Dispatch.RepathElapsed = 0.5f;
	for (FJourney& Journey : Dispatch.Journeys)
	{
		// Reverse only the route already travelled. Cancellation mid-segment starts at the actual position.
		Journey.Path.Reset(Journey.Travelled.Num());
		for (int32 Index = Journey.Travelled.Num() - 1; Index >= 0; --Index)
		{
			Journey.Path.Add(Journey.Travelled[Index]);
		}
		Journey.NextPoint = 0;
	}
}

void UFF_StaffDispatchSubsystem::HandleIncidentClosed(FFF_IncidentHandle Handle)
{
	for (FDispatch& Dispatch : Dispatches)
	{
		if (Dispatch.Handle == Handle)
		{
			BeginReturn(Dispatch);
		}
	}
}

void UFF_StaffDispatchSubsystem::HandleReservationsReset()
{
	// Detach the old batch before publishing releases; save/restart observers may start fresh work.
	TArray<FDispatch> Previous = MoveTemp(Dispatches);
	Dispatches.Reset();
	for (FDispatch& Dispatch : Previous)
	{
		if (!Dispatch.bReturning && Incident && Incident->IsHandleValid(Dispatch.Handle))
		{
			Incident->SetResponding(Dispatch.Handle, false);
		}
		ReleaseDispatch(Dispatch);
	}
}

void UFF_StaffDispatchSubsystem::RefreshReturnHomes(FDispatch& Dispatch)
{
	if (!Staffing)
	{
		return;
	}
	for (int32 Index = Dispatch.Journeys.Num() - 1; Index >= 0; --Index)
	{
		FJourney& Journey = Dispatch.Journeys[Index];
		FFF_StaffDispatchOrigin CurrentHome;
		const bool bHasHome = Staffing->GetStaffDispatchOrigin(Journey.StaffId, CurrentHome);
		if (bHasHome && Journey.HomeLocation.Equals(CurrentHome.Location, 1.f))
		{
			continue;
		}
		if (bHasHome && BuildPath(Journey.Position, CurrentHome.Location, Journey.Path))
		{
			Journey.HomeLocation = CurrentHome.Location;
			Journey.NextPoint = 0;
			continue;
		}
		// A removed/unreachable home must not leave an employee permanently reserved or teleport them.
		if (AFF_BaseCrowd* Actor = Journey.Actor.Get())
		{
			Actor->Destroy();
		}
		const int32 StaffId = Journey.StaffId;
		Dispatch.Journeys.RemoveAtSwap(Index);
		Staffing->ReleaseStaff({StaffId});
	}
}

void UFF_StaffDispatchSubsystem::ReleaseDispatch(FDispatch& Dispatch)
{
	TArray<int32> StaffIds;
	StaffIds.Reserve(Dispatch.Journeys.Num());
	for (FJourney& Journey : Dispatch.Journeys)
	{
		StaffIds.Add(Journey.StaffId);
		if (AFF_BaseCrowd* Actor = Journey.Actor.Get())
		{
			Actor->Destroy();
		}
	}
	if (Staffing)
	{
		Staffing->ReleaseStaff(StaffIds);
	}
}

void UFF_StaffDispatchSubsystem::Tick(float DeltaTime)
{
	if (DeltaTime <= 0.f)
	{
		return;
	}
	float WorkDeltaMinutes = 0.f;
	float NowMinutes = 0.f;
	if (TryGetFestivalMinutes(NowMinutes))
	{
		if (LastWorkClockMinutes >= 0.f)
		{
			WorkDeltaMinutes = FMath::Max(0.f, NowMinutes - LastWorkClockMinutes);
		}
		LastWorkClockMinutes = NowMinutes;
	}

	TArray<TPair<FFF_IncidentHandle, int32>> Arrivals;
	TArray<FFF_IncidentHandle> Cancellations;
	TArray<FFF_IncidentHandle> WorkStarted;
	for (int32 Index = Dispatches.Num() - 1; Index >= 0; --Index)
	{
		FDispatch& Dispatch = Dispatches[Index];
		const bool bWasReturning = Dispatch.bReturning;
		if (Dispatch.bWorking && !Dispatch.bReturning)
		{
			FVector WorkSite;
			if (!Incident || !Incident->IsHandleValid(Dispatch.Handle) || !TryGetLocation(Dispatch.Anchor, WorkSite))
			{
				Dispatch.bWorking = false;
				BeginReturn(Dispatch);
			}
			else if (!WorkSite.Equals(Dispatch.Destination, 200.f))
			{
				// The incident moved away (e.g. a walking actor anchor): follow it again, keeping the progress made.
				Dispatch.bWorking = false;
			}
			else
			{
				Dispatch.RemainingWorkMinutes -= WorkDeltaMinutes;
				if (Dispatch.RemainingWorkMinutes <= 0.f)
				{
					Dispatch.RemainingWorkMinutes = 0.f;
					Dispatch.bWorking = false;
					Arrivals.Emplace(Dispatch.Handle, Dispatch.OptionIndex);
					BeginReturn(Dispatch);
				}
				continue;
			}
		}
		if (!Dispatch.bReturning)
		{
			FVector LatestDestination;
			if (!Incident || !Incident->IsHandleValid(Dispatch.Handle)
				|| !TryGetLocation(Dispatch.Anchor, LatestDestination))
			{
				BeginReturn(Dispatch);
			}
			else
			{
				Dispatch.RepathElapsed += DeltaTime;
				if (Dispatch.RepathElapsed >= 0.5f && !LatestDestination.Equals(Dispatch.Destination, 50.f))
				{
					Dispatch.RepathElapsed = 0.f;
					Dispatch.Destination = LatestDestination;
					for (FJourney& Journey : Dispatch.Journeys)
					{
						Journey.Travelled.Add(Journey.Position);
						if (!BuildPath(Journey.Position, LatestDestination, Journey.Path))
						{
							BeginReturn(Dispatch);
							break;
						}
						Journey.NextPoint = 0;
					}
				}
			}
		}
		if (Dispatch.bReturning)
		{
			if (!bWasReturning)
			{
				Cancellations.Add(Dispatch.Handle);
			}
			Dispatch.RepathElapsed += DeltaTime;
			if (Dispatch.RepathElapsed >= 0.5f)
			{
				Dispatch.RepathElapsed = 0.f;
				RefreshReturnHomes(Dispatch);
			}
		}
		bool bAllArrived = true;
		for (FJourney& Journey : Dispatch.Journeys)
		{
			const FVector Previous = Journey.Position;
			bAllArrived &= AdvanceJourney(Journey, TravelSpeed * DeltaTime);
			if (AFF_BaseCrowd* Actor = Journey.Actor.Get())
			{
				const FVector Movement = Journey.Position - Previous;
				const FRotator Rotation = Movement.IsNearlyZero() ? Actor->GetActorRotation() : Movement.Rotation();
				Actor->PushPresentation(Journey.Position, Rotation, Movement.Size() / DeltaTime);
			}
		}
		if (bAllArrived)
		{
			if (Dispatch.bReturning)
			{
				ReleaseDispatch(Dispatch);
				Dispatches.RemoveAtSwap(Index);
			}
			else
			{
				FVector LatestDestination;
				if (!TryGetLocation(Dispatch.Anchor, LatestDestination))
				{
					BeginReturn(Dispatch);
					Cancellations.Add(Dispatch.Handle);
					continue;
				}
				if (!LatestDestination.Equals(Dispatch.Destination, 50.f))
				{
					// A moving incident is still away from our last route endpoint. Repath on the next update.
					continue;
				}
				if (Dispatch.Journeys.ContainsByPredicate([&LatestDestination](const FJourney& Journey)
				{
					return FVector::DistSquared(Journey.Position, LatestDestination) > FMath::Square(200.f);
				}))
				{
					BeginReturn(Dispatch);
					Cancellations.Add(Dispatch.Handle);
					continue;
				}
				if (Dispatch.RemainingWorkMinutes > 0.f)
				{
					for (FJourney& Journey : Dispatch.Journeys)
					{
						if (AFF_BaseCrowd* Actor = Journey.Actor.Get())
						{
							Actor->PushPresentation(Journey.Position, Actor->GetActorRotation(), 0.f);
						}
					}
					Dispatch.bWorking = true;
					WorkStarted.Add(Dispatch.Handle);
					continue;
				}
				Arrivals.Emplace(Dispatch.Handle, Dispatch.OptionIndex);
				BeginReturn(Dispatch);
			}
		}
	}
	// Close callbacks may affect other responses: dispatch them only after iteration.
	for (FFF_IncidentHandle Handle : Cancellations)
	{
		if (Incident && Incident->IsHandleValid(Handle))
		{
			Incident->SetResponding(Handle,false);
			Incident->OnIncidentUpdated.Broadcast(Handle);
		}
	}
	for (FFF_IncidentHandle Handle : WorkStarted)
	{
		if (Incident && Incident->IsHandleValid(Handle))
		{
			Incident->OnIncidentUpdated.Broadcast(Handle);
		}
	}
	for (const TPair<FFF_IncidentHandle, int32>& Arrival : Arrivals)
	{
		if (Incident)
		{
			Incident->CompleteStaffResponse(Arrival.Key, Arrival.Value);
		}
	}
}
