#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Economy/Rules/FF_StaffPool.h"
#include "FF_FFZSTypes.h"
#include "FF_StaffFacilitySubsystem.generated.h"

class UFFZSWorldSubsystem;
class UFF_FestivalProgressionSubsystem;
class UFF_SaveSubsystem;

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_StaffDispatchOrigin
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Staff") int32 StaffId = INDEX_NONE;
	// 선발한 순간의 원본 숙련도. 출동 후 승급은 다음 출동부터 반영한다.
	UPROPERTY(BlueprintReadOnly, Category="Staff") int32 TrainingLevel = 1;
	UPROPERTY(BlueprintReadOnly, Category="Staff") FGuid FacilityInstanceId;
	UPROPERTY(BlueprintReadOnly, Category="Staff") FVector Location = FVector::ZeroVector;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnStaffingChanged);

/** 시설 정원/소속/출동 예약의 단일 경계. 명단과 소속 GUID 저장은 Progression이 담당한다.
 * 변경 이벤트와 레지스트리 버전으로 캐시를 갱신하며 매 프레임 시설을 검색하지 않는다. */
UCLASS()
class FESTAFIESTA_API UFF_StaffFacilitySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	UFUNCTION(BlueprintPure, Category="FestaFiesta|Staff") int32 GetTotalFacilityCapacity() const;
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Staff") int32 GetFacilityCapacity(FGuid FacilityId) const;
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Staff") int32 GetAssignedStaffCount(FGuid FacilityId) const;
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Staff") int32 GetUnassignedStaffCount() const;
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Staff") TArray<FFF_StaffMember> GetFacilityStaff(FGuid FacilityId) const;
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Staff") int32 GetBusyStaffCount() const { return BusyStaff.Num(); }
	// TryReserveStaff 가 지금 한 번에 예약할 수 있는 최대 인원. 출동 중이 아니고 배정 시설에 출발 지점이 있는 사람만 센다.
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Staff") int32 GetDispatchableStaffCount() const;
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Staff") bool IsStaffBusy(int32 StaffId) const { return BusyStaff.Contains(StaffId); }
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Staff") bool CanAssignAvailableStaff(FGuid FacilityId) const;
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Staff") bool CanUnassignAvailableStaff(FGuid FacilityId) const;
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Staff") bool TryAssignStaff(int32 StaffId, FGuid FacilityId);
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Staff") bool TryUnassignStaff(int32 StaffId);
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Staff") bool TryAssignAvailableStaff(FGuid FacilityId);
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Staff") bool TryUnassignAvailableStaff(FGuid FacilityId);
	UPROPERTY(BlueprintAssignable, Category="FestaFiesta|Staff") FFF_OnStaffingChanged OnStaffingChanged;
	// Dispatch cancels old journeys before a new session can reserve the same staff IDs.
	FFF_OnStaffingChanged OnStaffReservationsReset;

	// All-or-nothing reservation: no partial departure if any required member is unavailable.
	bool TryReserveStaff(int32 Count, const FVector& Target, TArray<FFF_StaffDispatchOrigin>& OutOrigins);
	// 예약을 바꾸지 않고 실제 출동과 같은 근거리/StaffId 순서로 후보를 선발한다.
	bool PreviewStaffReservation(int32 Count, const FVector& Target, TArray<FFF_StaffDispatchOrigin>& OutOrigins) const;
	void ReleaseStaff(const TArray<int32>& StaffIds);
	bool GetStaffDispatchOrigin(int32 StaffId, FFF_StaffDispatchOrigin& OutOrigin) const;
	void RefreshStaffing();

private:
	UPROPERTY(Transient) TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;
	UPROPERTY(Transient) TObjectPtr<UFF_FestivalProgressionSubsystem> Progression;
	UPROPERTY(Transient) TObjectPtr<UFF_SaveSubsystem> Save;
	TMap<FGuid, int32> FacilityCapacities;
	TSet<int32> BusyStaff;
	uint64 CachedRegistryRevision = MAX_uint64;
	int32 TotalCapacity = 0;
	bool bRefreshing = false;
	bool bResettingReservations = false;
	void BindProgression();
	void EnsureFresh() const;
	void PublishAssignmentChange();
	bool BuildDispatchOrigin(const FFF_StaffMember& Member, FFF_StaffDispatchOrigin& OutOrigin) const;
	UFUNCTION() void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);
	UFUNCTION() void HandleSnapshotRestored();
	UFUNCTION() void HandleRosterChanged();
	UFUNCTION() void HandleSaveLoaded(FName SlotId);
	UFUNCTION() void HandleFestivalReset();
};
