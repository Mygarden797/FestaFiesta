#pragma once

#include "CoreMinimal.h"
#include "Incident/FF_IncidentTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_StaffDispatchSubsystem.generated.h"

class UFF_IncidentSubsystem;
class UFF_StaffFacilitySubsystem;
class UFF_StaffSubsystem;

/** Staff reservations belong to StaffFacility and staff movement belongs to StaffSubsystem; this subsystem owns only the incident response.
 *  It only says "send" / "recall" and reads each member's state: all Working = start work, any member gave up = cancel,
 *  Idle while returning = that member is home and its reservation is released. */
UCLASS()
class FESTAFIESTA_API UFF_StaffDispatchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	// All responders must have a home and a visual before dispatch succeeds. A missing navigation path does not block dispatch.
	// WorkAmount(사람-분)가 0보다 크면 전원 도착 뒤 현장에서 WorkAmount ÷ 인원 (축제 분) 동안 작업하고 사건을 닫는다.
	// OutResult 를 주면 Dispatched 또는 거부 이유를 채운다.
	bool TryDispatch(FFF_IncidentHandle Handle, int32 OptionIndex, int32 RequiredStaff,
		const FFF_IncidentAnchor& Anchor, float WorkAmount = 0.f, EFF_IncidentResolveResult* OutResult = nullptr);
	bool IsResponding(FFF_IncidentHandle Handle) const;

	// 현장 작업 중이면 남은 축제 분을 돌려준다. 이동 중이거나 출동이 없으면 false.
	bool GetRemainingWorkMinutes(FFF_IncidentHandle Handle, float& OutMinutes) const;

private:
	struct FDispatch
	{
		FFF_IncidentHandle Handle;
		int32 OptionIndex = INDEX_NONE;
		FFF_IncidentAnchor Anchor;
		TArray<int32> StaffIds;
		bool bReturning = false;
		bool bWorking = false;
		float RemainingWorkMinutes = 0.f;
	};

	bool TryGetFestivalMinutes(float& OutMinutes) const;

	bool TryGetLocation(const FFF_IncidentAnchor& Anchor, FVector& OutLocation) const;
	void BeginReturn(FDispatch& Dispatch);
	void ReleaseReturnedStaff(FDispatch& Dispatch);
	void ReleaseDispatch(FDispatch& Dispatch);

	UFUNCTION()
	void HandleIncidentClosed(FFF_IncidentHandle Handle);
	UFUNCTION()
	void HandleReservationsReset();

	UPROPERTY(Transient)
	TObjectPtr<UFF_IncidentSubsystem> Incident;
	UPROPERTY(Transient)
	TObjectPtr<UFF_StaffFacilitySubsystem> Staffing;
	UPROPERTY(Transient)
	TObjectPtr<UFF_StaffSubsystem> StaffBodies;

	TArray<FDispatch> Dispatches;
	float LastWorkClockMinutes = -1.f;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FFF_StaffDispatchJourneyTest;
	friend class FFF_StaffDispatchIncidentTest;
	friend class FFF_StaffDispatchWarpTest;
#endif
};
