// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Incident/FF_IncidentTypes.h"
#include "Staff/FF_StaffTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_StaffSubsystem.generated.h"

class AFF_StaffActor;
class AFF_StaffBase;
class UFF_FestivalProgressionSubsystem;
class UFF_FestivalStageSubsystem;
class UGameplayEffect;

// 스태프 몸(액터)의 스폰·이동·도착을 맡는다. 사건을 모르고, 도착은 델리게이트로만 알린다
UCLASS()
class FESTAFIESTA_API UFF_StaffSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	bool SendStaff(int32 StaffId, const FFF_IncidentAnchor& Target);
	void RecallStaff(int32 StaffId);

	bool IsIdle(int32 StaffId) const;
	EFF_StaffState GetStaffState(int32 StaffId) const;
	AFF_StaffActor* FindStaffActor(int32 StaffId) const;

	bool ApplyEffectToStaff(int32 StaffId, TSubclassOf<UGameplayEffect> Effect);

	FFF_OnStaffArrived OnStaffArrived;

private:
	struct FStaffRuntime
	{
		TWeakObjectPtr<AFF_StaffActor> Actor;
		EFF_StaffState State = EFF_StaffState::Idle;
		int32 SlotIndex = INDEX_NONE;
		FFF_IncidentAnchor Target;
		TArray<FVector> Path;
		int32 NextIndex = 0;
		FVector PathGoal = FVector::ZeroVector;
		float LastRepathMinutes = 0.f;
		float BestDistance = 0.f;
		float LastProgressMinutes = 0.f;
		int32 FailCount = 0;
	};

	// --- 명단 ---
	void SyncActorsWithRoster();

	UFUNCTION()
	void HandleStaffRosterChanged();

	// --- 이동 ---
	void BeginMove(FStaffRuntime& Runtime, EFF_StaffState MoveState, const FFF_IncidentAnchor& Target, float NowMinutes);
	bool ResolveTargetLocation(const FFF_IncidentAnchor& Target, FVector& OutLocation) const;
	bool FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const;
	bool ShouldRepath(const FStaffRuntime& Runtime, const FVector& TargetNow, float NowMinutes) const;
	bool Repath(FStaffRuntime& Runtime, const FVector& TargetNow, float NowMinutes);
	bool TickMove(FStaffRuntime& Runtime, const FVector& TargetNow, float SimDeltaSeconds);
	float GetMoveSpeed(const FStaffRuntime& Runtime) const;

	// --- 도착 보험 ---
	bool CheckProgress(FStaffRuntime& Runtime, const FVector& TargetNow, float NowMinutes);
	void WarpToTarget(int32 StaffId, FStaffRuntime& Runtime, const FVector& TargetNow);

	// --- 상태 ---
	void SetState(FStaffRuntime& Runtime, EFF_StaffState NewState);
	void HandleArrived(int32 StaffId);
	void HandleTargetLost(int32 StaffId);
	FFF_IncidentAnchor MakeHomeAnchor(const FStaffRuntime& Runtime) const;

	// --- 시간 ---
	bool TryGetNowMinutes(float& OutMinutes) const;
	float GetEffectiveTimeScale() const;

	UPROPERTY()
	TObjectPtr<UFF_FestivalStageSubsystem> Stage;

	UPROPERTY()
	TObjectPtr<UFF_FestivalProgressionSubsystem> Progression;

	TWeakObjectPtr<AFF_StaffBase> Base;

	TMap<int32, FStaffRuntime> Staff;

	static constexpr float ArrivalRadiusCm = 150.f;
	static constexpr float RepathIntervalMinutes = 1.f;
	static constexpr float RepathTargetMoveCm = 200.f;
	static constexpr float StallTimeoutMinutes = 5.f;
	static constexpr float MinProgressCm = 50.f;
	static constexpr int32 MaxFailCount = 2;
};
