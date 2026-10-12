#include "Staff/FF_StaffDispatchSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Economy/Rules/FF_StaffEventRules.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Incident/Rules/FF_IncidentRules.h"
#include "Staff/FF_StaffFacilitySubsystem.h"
#include "Staff/FF_StaffSubsystem.h"
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
	StaffBodies = Collection.InitializeDependency<UFF_StaffSubsystem>();
	if (Staffing)
	{
		Staffing->OnStaffReservationsReset.AddUniqueDynamic(this, &ThisClass::HandleReservationsReset);
	}
	if (Incident)
	{
		Incident->OnIncidentClosed.AddUniqueDynamic(this, &ThisClass::HandleIncidentClosed);
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
	ResetDispatches(false);
	Incident = nullptr;
	Staffing = nullptr;
	StaffBodies = nullptr;
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

bool UFF_StaffDispatchSubsystem::GetTotalWorkMinutes(FFF_IncidentHandle Handle, float& OutMinutes) const
{
	const FDispatch* Found = Dispatches.FindByPredicate([Handle](const FDispatch& Dispatch)
	{
		return Dispatch.Handle == Handle && !Dispatch.bReturning;
	});
	if (!Found)
	{
		return false;
	}
	OutMinutes = Found->TotalWorkMinutes;
	return true;
}

bool UFF_StaffDispatchSubsystem::ShouldAnnounceWorkMinute(float PreviousMinutes, float CurrentMinutes)
{
	return CurrentMinutes > 0.f && FMath::CeilToInt(PreviousMinutes) != FMath::CeilToInt(CurrentMinutes);
}

EFF_IncidentResponseStage UFF_StaffDispatchSubsystem::GetResponseStage(FFF_IncidentHandle Handle, float& OutRemainingMinutes) const
{
	OutRemainingMinutes = 0.f;
	if (GetRemainingWorkMinutes(Handle, OutRemainingMinutes))
	{
		return EFF_IncidentResponseStage::Working;
	}
	return IsResponding(Handle) ? EFF_IncidentResponseStage::Travelling : EFF_IncidentResponseStage::None;
}

// 작업 시간은 배속과 맞아야 하므로 실시간 DeltaTime 이 아니라 축제 시계로 흘린다. Live 가 아니면 작업도 멈춘다.
bool UFF_StaffDispatchSubsystem::TryGetFestivalMinutes(float& OutMinutes) const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UFF_FestivalStageSubsystem* Stage = GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
	return Stage && Stage->TryGetLiveElapsedMinutes(OutMinutes);
}

bool UFF_StaffDispatchSubsystem::CalculateWork(const TArray<FFF_StaffDispatchOrigin>& Origins,
	float WorkAmount, int32 MinStaff, float& OutMinutes, float& OutAbility)
{
	OutMinutes = 0.f;
	OutAbility = 0.f;
	if (Origins.Num() < FMath::Max(1, MinStaff) || !FMath::IsFinite(WorkAmount)) return false;
	TArray<FFF_StaffMember> Members;
	Members.Reserve(Origins.Num());
	for (const FFF_StaffDispatchOrigin& Origin : Origins)
	{
		if (Origin.TrainingLevel < 1) return false;
		FFF_StaffMember& Member = Members.AddDefaulted_GetRef();
		Member.StaffId = Origin.StaffId;
		Member.TrainingLevel = Origin.TrainingLevel;
	}
	const float Ability = FFF_StaffEventRules::CalcAbilitySum(Members, FMath::Max(1, MinStaff));
	FFF_FestivalEventCard Card;
	Card.WorkAmount = FMath::Max(0.f, WorkAmount);
	// 원본 함수의 초는 축제 초다. 작업 시계는 축제 분이므로 여기서 한 번만 환산한다.
	const float Minutes = FFF_StaffEventRules::CalcResolveSeconds(Card, Ability) / 60.f;
	if (Ability <= 0.f || !FMath::IsFinite(Minutes)) return false;
	OutAbility = Ability;
	OutMinutes = Minutes;
	return true;
}

bool UFF_StaffDispatchSubsystem::PreviewWork(int32 RequiredStaff, const FFF_IncidentAnchor& Anchor,
	float WorkAmount, int32 MinStaff, float& OutMinutes, float& OutAbility) const
{
	OutMinutes = 0.f;
	OutAbility = 0.f;
	FVector Destination;
	TArray<FFF_StaffDispatchOrigin> Origins;
	return Staffing && TryGetLocation(Anchor, Destination)
		&& Staffing->PreviewStaffReservation(RequiredStaff, Destination, Origins)
		&& CalculateWork(Origins, WorkAmount, MinStaff, OutMinutes, OutAbility);
}

bool UFF_StaffDispatchSubsystem::TryDispatch(FFF_IncidentHandle Handle, int32 OptionIndex,
	int32 RequiredStaff, const FFF_IncidentAnchor& Anchor, float WorkAmount, EFF_IncidentResolveResult* OutResult,
	int32 MinStaff)
{
	EFF_IncidentResolveResult LocalResult = EFF_IncidentResolveResult::DispatchFailed;
	EFF_IncidentResolveResult& Result = OutResult ? *OutResult : LocalResult;
	Result = EFF_IncidentResolveResult::DispatchFailed;
	if (bDispatchInFlight || !Staffing || !StaffBodies || !Incident || RequiredStaff < FMath::Max(1, MinStaff)
		|| !FMath::IsFinite(WorkAmount))
	{
		return false;
	}
	// 예약 변경 방송 중 같은 출동 경로가 재진입해 중복 팀을 보내지 않게 한다.
	TGuardValue<bool> DispatchGuard(bDispatchInFlight, true);
	if (!Incident->IsHandleValid(Handle))
	{
		Result = EFF_IncidentResolveResult::InvalidIncident;
		return false;
	}
	if (IsResponding(Handle))
	{
		Result = EFF_IncidentResolveResult::AlreadyResponding;
		return false;
	}
	FDispatch Pending;
	Pending.Handle = Handle;
	Pending.OptionIndex = OptionIndex;
	Pending.Anchor = Anchor;
	FVector Destination;
	if (!TryGetLocation(Anchor, Destination))
	{
		Result = EFF_IncidentResolveResult::NoSite;
		return false;
	}
	TArray<FFF_StaffDispatchOrigin> Origins;
	if (!Staffing->TryReserveStaff(RequiredStaff, Destination, Origins))
	{
		Result = EFF_IncidentResolveResult::NotEnoughStaff;
		return false;
	}
	// Register all reservations in Pending before any fallible work, so rollback is always complete.
	for (const FFF_StaffDispatchOrigin& Origin : Origins)
	{
		Pending.StaffIds.Add(Origin.StaffId);
	}
	// 예약 방송에서 사건이 닫혔을 수도 있으므로 몸체 생성 전에 다시 확인한다.
	if (!Incident->IsHandleValid(Handle))
	{
		Staffing->ReleaseStaff(Pending.StaffIds);
		Result = EFF_IncidentResolveResult::InvalidIncident;
		return false;
	}
	float Ability = 0.f;
	if (!CalculateWork(Origins, WorkAmount, MinStaff, Pending.TotalWorkMinutes, Ability))
	{
		ReleaseDispatch(Pending);
		return false;
	}
	Pending.RemainingWorkMinutes = Pending.TotalWorkMinutes;
	TArray<FVector> SpreadOffsets;
	UFF_StaffSubsystem::MakeSpreadOffsets(Origins.Num(), SpreadOffsets);
	for (int32 Index = 0; Index < Origins.Num(); ++Index)
	{
		if (!StaffBodies->SendStaff(Origins[Index], Anchor, SpreadOffsets[Index]))
		{
			ReleaseDispatch(Pending);
			return false;
		}
	}
	if (Dispatches.IsEmpty())
	{
		LastWorkClockMinutes = -1.f;
	}
	Dispatches.Add(MoveTemp(Pending));
	Incident->SetResponding(Handle,true);
	Incident->OnIncidentUpdated.Broadcast(Handle);
	Result = EFF_IncidentResolveResult::Dispatched;
	return true;
}

void UFF_StaffDispatchSubsystem::BeginReturn(FDispatch& Dispatch)
{
	if (Dispatch.bReturning)
	{
		return;
	}
	Dispatch.bReturning = true;
	Dispatch.bWorking = false;
	if (StaffBodies)
	{
		for (const int32 StaffId : Dispatch.StaffIds)
		{
			StaffBodies->RecallStaff(StaffId);
		}
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
	ResetDispatches(true);
}

void UFF_StaffDispatchSubsystem::ResetDispatches(bool bAnnounce)
{
	// Detach the old batch before publishing releases; save/restart observers may start fresh work.
	TArray<FDispatch> Previous = MoveTemp(Dispatches);
	Dispatches.Reset();
	TArray<FFF_IncidentHandle> Cancelled;
	for (FDispatch& Dispatch : Previous)
	{
		if (!Dispatch.bReturning && Incident && Incident->IsHandleValid(Dispatch.Handle))
		{
			Incident->SetResponding(Dispatch.Handle, false);
			Cancelled.Add(Dispatch.Handle);
		}
		ReleaseDispatch(Dispatch);
	}
	if (!bAnnounce || !Incident)
	{
		return;
	}
	for (const FFF_IncidentHandle Handle : Cancelled)
	{
		if (Incident->IsHandleValid(Handle))
		{
			Incident->OnIncidentUpdated.Broadcast(Handle);
		}
	}
}

void UFF_StaffDispatchSubsystem::ReleaseReturnedStaff(FDispatch& Dispatch)
{
	TArray<int32> Returned;
	for (int32 Index = Dispatch.StaffIds.Num() - 1; Index >= 0; --Index)
	{
		const int32 StaffId = Dispatch.StaffIds[Index];
		if (StaffBodies->IsIdle(StaffId))
		{
			Returned.Add(StaffId);
			Dispatch.StaffIds.RemoveAtSwap(Index);
		}
	}
	if (Staffing && !Returned.IsEmpty())
	{
		Staffing->ReleaseStaff(Returned);
	}
}

void UFF_StaffDispatchSubsystem::ReleaseDispatch(FDispatch& Dispatch)
{
	if (StaffBodies)
	{
		for (const int32 StaffId : Dispatch.StaffIds)
		{
			StaffBodies->DespawnStaff(StaffId);
		}
	}
	if (Staffing)
	{
		Staffing->ReleaseStaff(Dispatch.StaffIds);
	}
}

void UFF_StaffDispatchSubsystem::Tick(float DeltaTime)
{
	if (DeltaTime <= 0.f || !StaffBodies)
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
	TArray<FFF_IncidentHandle> ViewChanged;
	for (int32 Index = Dispatches.Num() - 1; Index >= 0; --Index)
	{
		FDispatch& Dispatch = Dispatches[Index];
		if (Dispatch.bReturning)
		{
			ReleaseReturnedStaff(Dispatch);
			if (Dispatch.StaffIds.IsEmpty())
			{
				Dispatches.RemoveAtSwap(Index);
			}
			continue;
		}
		FVector Site;
		if (!Incident || !Incident->IsHandleValid(Dispatch.Handle) || !TryGetLocation(Dispatch.Anchor, Site))
		{
			BeginReturn(Dispatch);
			Cancellations.Add(Dispatch.Handle);
			continue;
		}
		bool bAllWorking = true;
		bool bAnyGaveUp = false;
		for (const int32 StaffId : Dispatch.StaffIds)
		{
			const EFF_StaffState State = StaffBodies->GetStaffState(StaffId);
			bAllWorking &= State == EFF_StaffState::Working;
			bAnyGaveUp |= State == EFF_StaffState::Returning || State == EFF_StaffState::Idle;
		}
		if (bAnyGaveUp)
		{
			BeginReturn(Dispatch);
			Cancellations.Add(Dispatch.Handle);
			continue;
		}
		if (Dispatch.bWorking)
		{
			if (!bAllWorking)
			{
				Dispatch.bWorking = false;
				ViewChanged.Add(Dispatch.Handle);
				continue;
			}
			const float PreviousMinutes = Dispatch.RemainingWorkMinutes;
			Dispatch.RemainingWorkMinutes -= WorkDeltaMinutes;
			if (ShouldAnnounceWorkMinute(PreviousMinutes, Dispatch.RemainingWorkMinutes))
			{
				ViewChanged.Add(Dispatch.Handle);
			}
		}
		else if (bAllWorking)
		{
			if (Dispatch.RemainingWorkMinutes > 0.f)
			{
				Dispatch.bWorking = true;
				ViewChanged.Add(Dispatch.Handle);
				continue;
			}
		}
		else
		{
			continue;
		}
		if (Dispatch.RemainingWorkMinutes <= 0.f)
		{
			Dispatch.RemainingWorkMinutes = 0.f;
			Arrivals.Emplace(Dispatch.Handle, Dispatch.OptionIndex);
			BeginReturn(Dispatch);
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
	for (FFF_IncidentHandle Handle : ViewChanged)
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
