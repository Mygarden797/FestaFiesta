#include "Staff/FF_StaffFacilitySubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Save/FF_SaveSubsystem.h"

void UFF_StaffFacilitySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ZoneSystem = Collection.InitializeDependency<UFFZSWorldSubsystem>();
	ZoneSystem->OnWorldChanged.AddDynamic(this, &ThisClass::HandleWorldChanged);
	ZoneSystem->OnWorldSnapshotRestored.AddDynamic(this, &ThisClass::HandleSnapshotRestored);
	BindProgression();
}

void UFF_StaffFacilitySubsystem::BindProgression()
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (!GI || Progression) return;
	Progression = GI->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (Progression)
	{
		Progression->OnStaffRosterChanged.AddUniqueDynamic(this, &ThisClass::HandleRosterChanged);
		Progression->OnFestivalDefinitionChanged.AddUniqueDynamic(this, &ThisClass::HandleFestivalReset);
	}
	Save = GI->GetSubsystem<UFF_SaveSubsystem>();
	if (Save) Save->OnSaveLoaded.AddUniqueDynamic(this, &ThisClass::HandleSaveLoaded);
}

void UFF_StaffFacilitySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BindProgression();
	RefreshStaffing();
}

void UFF_StaffFacilitySubsystem::Deinitialize()
{
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.RemoveDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.RemoveDynamic(this, &ThisClass::HandleSnapshotRestored);
	}
	if (Progression)
	{
		Progression->OnStaffRosterChanged.RemoveDynamic(this, &ThisClass::HandleRosterChanged);
		Progression->OnFestivalDefinitionChanged.RemoveDynamic(this, &ThisClass::HandleFestivalReset);
	}
	if (Save) Save->OnSaveLoaded.RemoveDynamic(this, &ThisClass::HandleSaveLoaded);
	BusyStaff.Reset();
	FacilityCapacities.Reset();
	Progression = nullptr;
	ZoneSystem = nullptr;
	Save = nullptr;
	Super::Deinitialize();
}

bool UFF_StaffFacilitySubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}

void UFF_StaffFacilitySubsystem::EnsureFresh() const
{
	if (ZoneSystem && CachedRegistryRevision != ZoneSystem->GetFacilityRegistryRevision())
		const_cast<ThisClass*>(this)->RefreshStaffing();
}

void UFF_StaffFacilitySubsystem::RefreshStaffing()
{
	if (bRefreshing || !ZoneSystem) return;
	TGuardValue<bool> Guard(bRefreshing, true);
	BindProgression();
	const int32 OldCapacity = TotalCapacity;
	const uint64 RegistryRevision = ZoneSystem->GetFacilityRegistryRevision();
	if (CachedRegistryRevision != RegistryRevision)
	{
		FacilityCapacities.Reset();
		int64 Sum = 0;
		for (const FFFZSFacilityReadModel& Row : ZoneSystem->BuildReadModel().Facilities)
		{
			const AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(Row.FacilityInstanceId);
			if (!Facility || Row.State == EFFZSFacilityState::Planned || Row.State == EFFZSFacilityState::Removed) continue;
			const int32 Capacity = Facility->GetFacilitySpec().GetStaffCapacity();
			if (Capacity <= 0) continue;
			FacilityCapacities.Add(Row.FacilityInstanceId, Capacity);
			Sum += Capacity;
		}
		TotalCapacity = static_cast<int32>(FMath::Min<int64>(Sum, MAX_int32));
		CachedRegistryRevision = RegistryRevision;
	}
	// Save parts restore in phases. Never erase a saved home before facilities finish restoring.
	if (Save && Save->IsRestoring()) return;
	bool bChanged = false;
	if (Progression)
	{
		TMap<FGuid, int32> Used;
		TArray<FFF_StaffMember> Roster = Progression->GetStaffRoster();
		Roster.Sort([](const FFF_StaffMember& A, const FFF_StaffMember& B) { return A.StaffId < B.StaffId; });
		TSet<int32> ExistingIds;
		for (const FFF_StaffMember& Member : Roster)
		{
			ExistingIds.Add(Member.StaffId);
			if (!Member.AssignedFacilityId.IsValid()) continue;
			const int32 Capacity = FacilityCapacities.FindRef(Member.AssignedFacilityId);
			int32& UsedCount = Used.FindOrAdd(Member.AssignedFacilityId);
			if (UsedCount < Capacity) ++UsedCount;
			else bChanged |= Progression->SetStaffFacilityAssignment(Member.StaffId, FGuid());
		}
		for (auto It = BusyStaff.CreateIterator(); It; ++It)
			if (!ExistingIds.Contains(*It)) It.RemoveCurrent();
		if (bChanged) Progression->OnStaffRosterChanged.Broadcast();
		if (OldCapacity != TotalCapacity) Progression->OnStaffCapacityChanged.Broadcast(Progression->GetMaxTotalStaffCount());
	}
	OnStaffingChanged.Broadcast();
}

int32 UFF_StaffFacilitySubsystem::GetTotalFacilityCapacity() const { EnsureFresh(); return TotalCapacity; }
int32 UFF_StaffFacilitySubsystem::GetFacilityCapacity(FGuid FacilityId) const { EnsureFresh(); return FacilityCapacities.FindRef(FacilityId); }

TArray<FFF_StaffMember> UFF_StaffFacilitySubsystem::GetFacilityStaff(FGuid FacilityId) const
{
	EnsureFresh();
	TArray<FFF_StaffMember> Result;
	if (Progression && FacilityId.IsValid())
		for (const FFF_StaffMember& Member : Progression->GetStaffRoster())
			if (Member.AssignedFacilityId == FacilityId) Result.Add(Member);
	return Result;
}

int32 UFF_StaffFacilitySubsystem::GetAssignedStaffCount(FGuid FacilityId) const { return GetFacilityStaff(FacilityId).Num(); }

int32 UFF_StaffFacilitySubsystem::GetDispatchableStaffCount() const
{
	EnsureFresh();
	if (!Progression || bResettingReservations || (Save && Save->IsRestoring())) return 0;
	int32 Count = 0;
	FFF_StaffDispatchOrigin Origin;
	for (const FFF_StaffMember& Member : Progression->GetStaffRoster())
		if (!IsStaffBusy(Member.StaffId) && BuildDispatchOrigin(Member, Origin)) ++Count;
	return Count;
}

int32 UFF_StaffFacilitySubsystem::GetUnassignedStaffCount() const
{
	EnsureFresh();
	int32 Count = 0;
	if (Progression)
		for (const FFF_StaffMember& Member : Progression->GetStaffRoster())
			if (!Member.AssignedFacilityId.IsValid()) ++Count;
	return Count;
}

bool UFF_StaffFacilitySubsystem::CanAssignAvailableStaff(FGuid FacilityId) const
{
	if (GetFacilityCapacity(FacilityId) <= GetAssignedStaffCount(FacilityId) || !Progression) return false;
	for (const FFF_StaffMember& Member : Progression->GetStaffRoster())
		if (!Member.AssignedFacilityId.IsValid() && !IsStaffBusy(Member.StaffId)) return true;
	return false;
}

bool UFF_StaffFacilitySubsystem::CanUnassignAvailableStaff(FGuid FacilityId) const
{
	for (const FFF_StaffMember& Member : GetFacilityStaff(FacilityId))
		if (!IsStaffBusy(Member.StaffId)) return true;
	return false;
}

void UFF_StaffFacilitySubsystem::PublishAssignmentChange()
{
	TGuardValue<bool> Guard(bRefreshing, true);
	if (Progression) Progression->OnStaffRosterChanged.Broadcast();
	OnStaffingChanged.Broadcast();
}

bool UFF_StaffFacilitySubsystem::TryAssignStaff(int32 StaffId, FGuid FacilityId)
{
	EnsureFresh();
	if (!Progression || IsStaffBusy(StaffId) || GetFacilityCapacity(FacilityId) <= GetAssignedStaffCount(FacilityId)) return false;
	if (!Progression->SetStaffFacilityAssignment(StaffId, FacilityId)) return false;
	PublishAssignmentChange();
	return true;
}

bool UFF_StaffFacilitySubsystem::TryUnassignStaff(int32 StaffId)
{
	EnsureFresh();
	if (!Progression || IsStaffBusy(StaffId) || !Progression->SetStaffFacilityAssignment(StaffId, FGuid())) return false;
	PublishAssignmentChange();
	return true;
}

bool UFF_StaffFacilitySubsystem::TryAssignAvailableStaff(FGuid FacilityId)
{
	if (!CanAssignAvailableStaff(FacilityId)) return false;
	for (const FFF_StaffMember& Member : Progression->GetStaffRoster())
		if (!Member.AssignedFacilityId.IsValid() && !IsStaffBusy(Member.StaffId)) return TryAssignStaff(Member.StaffId, FacilityId);
	return false;
}

bool UFF_StaffFacilitySubsystem::TryUnassignAvailableStaff(FGuid FacilityId)
{
	for (const FFF_StaffMember& Member : GetFacilityStaff(FacilityId))
		if (!IsStaffBusy(Member.StaffId)) return TryUnassignStaff(Member.StaffId);
	return false;
}

bool UFF_StaffFacilitySubsystem::GetStaffDispatchOrigin(int32 StaffId, FFF_StaffDispatchOrigin& OutOrigin) const
{
	EnsureFresh();
	OutOrigin = FFF_StaffDispatchOrigin();
	if (!Progression || !ZoneSystem) return false;
	for (const FFF_StaffMember& Member : Progression->GetStaffRoster())
	{
		if (Member.StaffId == StaffId) return BuildDispatchOrigin(Member, OutOrigin);
	}
	return false;
}

bool UFF_StaffFacilitySubsystem::BuildDispatchOrigin(const FFF_StaffMember& Member, FFF_StaffDispatchOrigin& OutOrigin) const
{
	if (!ZoneSystem || FacilityCapacities.FindRef(Member.AssignedFacilityId) <= 0) return false;
	const AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(Member.AssignedFacilityId);
	if (!Facility || Facility->GetFacilityState() == EFFZSFacilityState::MovePending) return false;
	FTransform Origin;
	if (!Facility->GetAnchorWorldTransform(EFFZSAnchorType::Exit, 0, Origin)
		&& !Facility->GetAnchorWorldTransform(EFFZSAnchorType::Entry, 0, Origin)) return false;
	OutOrigin.StaffId = Member.StaffId;
	OutOrigin.FacilityInstanceId = Member.AssignedFacilityId;
	OutOrigin.Location = Origin.GetLocation();
	return !OutOrigin.Location.ContainsNaN();
}

bool UFF_StaffFacilitySubsystem::TryReserveStaff(int32 Count, const FVector& Target, TArray<FFF_StaffDispatchOrigin>& OutOrigins)
{
	EnsureFresh();
	OutOrigins.Reset();
	if (!Progression || bResettingReservations || (Save && Save->IsRestoring()) || Count <= 0 || Target.ContainsNaN()) return false;
	TArray<FFF_StaffDispatchOrigin> Candidates;
	for (const FFF_StaffMember& Member : Progression->GetStaffRoster())
	{
		FFF_StaffDispatchOrigin Origin;
		if (!IsStaffBusy(Member.StaffId) && BuildDispatchOrigin(Member, Origin)) Candidates.Add(Origin);
	}
	if (Candidates.Num() < Count) return false;
	Candidates.Sort([&Target](const FFF_StaffDispatchOrigin& A, const FFF_StaffDispatchOrigin& B)
	{
		const double DA = FVector::DistSquared(A.Location, Target), DB = FVector::DistSquared(B.Location, Target);
		return DA == DB ? A.StaffId < B.StaffId : DA < DB;
	});
	Candidates.SetNum(Count);
	for (const FFF_StaffDispatchOrigin& Origin : Candidates) BusyStaff.Add(Origin.StaffId);
	OutOrigins = MoveTemp(Candidates);
	OnStaffingChanged.Broadcast();
	return true;
}

void UFF_StaffFacilitySubsystem::ReleaseStaff(const TArray<int32>& StaffIds)
{
	bool bChanged = false;
	for (int32 StaffId : StaffIds) bChanged |= BusyStaff.Remove(StaffId) > 0;
	if (bChanged) OnStaffingChanged.Broadcast();
}

void UFF_StaffFacilitySubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event) { RefreshStaffing(); }
void UFF_StaffFacilitySubsystem::HandleSnapshotRestored() { RefreshStaffing(); }
void UFF_StaffFacilitySubsystem::HandleRosterChanged() { RefreshStaffing(); }
void UFF_StaffFacilitySubsystem::HandleSaveLoaded(FName SlotId)
{
	HandleFestivalReset();
	// The cache can be queried between restore phases; publish the final limit even if
	// that query already observed it while notifications were intentionally suspended.
	if (Progression) Progression->OnStaffCapacityChanged.Broadcast(Progression->GetMaxTotalStaffCount());
}
void UFF_StaffFacilitySubsystem::HandleFestivalReset()
{
	{
		TGuardValue<bool> ResetGuard(bResettingReservations, true);
		OnStaffReservationsReset.Broadcast();
		BusyStaff.Reset();
	}
	RefreshStaffing();
}
