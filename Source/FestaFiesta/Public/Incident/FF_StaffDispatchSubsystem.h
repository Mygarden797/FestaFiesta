#pragma once

#include "CoreMinimal.h"
#include "Incident/FF_IncidentTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_StaffDispatchSubsystem.generated.h"

class AFF_BaseCrowd;
class UFF_IncidentSubsystem;
class UFF_StaffFacilitySubsystem;

/** Staff reservations belong to StaffFacility; this subsystem owns only journeys and presentation. */
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

	// All responders must have a home, a complete navigation path and a visual before dispatch succeeds.
	// WorkAmount(사람-분)가 0보다 크면 전원 도착 뒤 현장에서 WorkAmount ÷ 인원 (축제 분) 동안 작업하고 사건을 닫는다.
	bool TryDispatch(FFF_IncidentHandle Handle, int32 OptionIndex, int32 RequiredStaff,
		const FFF_IncidentAnchor& Anchor, float WorkAmount = 0.f);
	bool IsResponding(FFF_IncidentHandle Handle) const;

	// 현장 작업 중이면 남은 축제 분을 돌려준다. 이동 중이거나 출동이 없으면 false.
	bool GetRemainingWorkMinutes(FFF_IncidentHandle Handle, float& OutMinutes) const;

private:
	struct FJourney
	{
		int32 StaffId = INDEX_NONE;
		FVector Position = FVector::ZeroVector;
		FVector HomeLocation = FVector::ZeroVector;
		TWeakObjectPtr<AFF_BaseCrowd> Actor;
		TArray<FVector> Path;
		TArray<FVector> Travelled;
		int32 NextPoint = 0;
	};

	struct FDispatch
	{
		FFF_IncidentHandle Handle;
		int32 OptionIndex = INDEX_NONE;
		FFF_IncidentAnchor Anchor;
		FVector Destination = FVector::ZeroVector;
		TArray<FJourney> Journeys;
		bool bReturning = false;
		float RepathElapsed = 0.f;
		bool bWorking = false;
		float RemainingWorkMinutes = 0.f;
	};

	bool TryGetFestivalMinutes(float& OutMinutes) const;

	bool TryGetLocation(const FFF_IncidentAnchor& Anchor, FVector& OutLocation) const;
	bool BuildPath(const FVector& Start, const FVector& End, TArray<FVector>& OutPath) const;
	static bool AdvanceJourney(FJourney& Journey, float Distance);
	static void BeginReturn(FDispatch& Dispatch);
	void RefreshReturnHomes(FDispatch& Dispatch);
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
	TSubclassOf<AFF_BaseCrowd> ResponderClass;

	TArray<FDispatch> Dispatches;
	float TravelSpeed = 300.f;
	float LastWorkClockMinutes = -1.f;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FFF_StaffDispatchJourneyTest;
	friend class FFF_StaffDispatchIncidentTest;
#endif
};
