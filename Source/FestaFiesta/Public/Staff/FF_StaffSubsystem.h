// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Incident/FF_IncidentTypes.h"
#include "Staff/FF_StaffTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_StaffSubsystem.generated.h"

class AFF_BaseCrowd;
class AFF_StaffActor;
class AFF_StaffBase;
class UFF_FestivalProgressionSubsystem;
class UFF_FestivalStageSubsystem;
class UFF_StaffFacilitySubsystem;
class UGameplayEffect;
struct FFF_StaffDispatchOrigin;

// 스태프 몸(액터)의 스폰·이동·도착을 맡는다. 사건을 모르고, 도착은 델리게이트로만 알린다
// 걸음에 관한 판단은 전부 여기서 한다. 바깥은 SendStaff(보내라) / RecallStaff(돌아와라)만 말하고 GetStaffState 로 결과를 읽는다.
//  Moving    : 실시간 DeltaTime × TravelSpeed 로 걷는다. 0.5초마다 목표가 50cm 넘게 움직였으면 길을 다시 찾는다.
//              경로 끝에서 목표 200cm 안이면 Working, 밖이거나 재경로가 실패하면 스스로 Returning(포기)으로 바뀐다.
//  Working   : 제자리. 목표가 200cm 넘게 떠나면 Moving 으로 돌아가 다시 따라간다.
//  Returning : 지나온 길을 거꾸로 되짚고, 0.5초마다 집(소속 시설 출구)이 옮겨졌는지 확인한다.
//              집에 닿거나 집이 사라지면 몸을 지우고 Idle 이 된다.
// 완전한 내비 경로가 없으면 출발 자체가 실패한다.
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

	bool SendStaff(const FFF_StaffDispatchOrigin& Origin, const FFF_IncidentAnchor& Target);
	void RecallStaff(int32 StaffId);
	void DespawnStaff(int32 StaffId);

	bool IsIdle(int32 StaffId) const;
	EFF_StaffState GetStaffState(int32 StaffId) const;
	AFF_StaffActor* FindStaffActor(int32 StaffId) const;

	bool ApplyEffectToStaff(int32 StaffId, TSubclassOf<UGameplayEffect> Effect);

	FFF_OnStaffArrived OnStaffArrived;

private:
	struct FStaffRuntime
	{
		TWeakObjectPtr<AFF_StaffActor> Actor;
		TWeakObjectPtr<AFF_BaseCrowd> Responder;
		EFF_StaffState State = EFF_StaffState::Idle;
		int32 SlotIndex = INDEX_NONE;
		FFF_IncidentAnchor Target;
		FVector Position = FVector::ZeroVector;
		FVector HomeLocation = FVector::ZeroVector;
		TArray<FVector> Path;
		TArray<FVector> Travelled;
		int32 NextIndex = 0;
		FVector PathGoal = FVector::ZeroVector;
		float RepathElapsed = 0.f;
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
	bool TickRuntime(int32 StaffId, FStaffRuntime& Runtime, float DeltaTime);
	bool MoveStaff(FStaffRuntime& Runtime, float DeltaTime);
	void HoldAtTarget(int32 StaffId, FStaffRuntime& Runtime);
	void GiveUp(FStaffRuntime& Runtime);
	bool ResolveTargetLocation(const FStaffRuntime& Runtime, FVector& OutLocation) const;
	bool RefreshReturnHome(int32 StaffId, FStaffRuntime& Runtime);
	bool FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const;
	static bool AdvanceAlongPath(FStaffRuntime& Runtime, float Distance);
	static void ReverseTravelledPath(FStaffRuntime& Runtime);

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

	UPROPERTY(Transient)
	TObjectPtr<UFF_StaffFacilitySubsystem> Staffing;

	UPROPERTY(Transient)
	TSubclassOf<AFF_BaseCrowd> ResponderClass;

	TWeakObjectPtr<AFF_StaffBase> Base;

	TMap<int32, FStaffRuntime> Staff;
	float TravelSpeed = 300.f;

	static constexpr float ArrivalRadiusCm = 200.f;
	static constexpr float WorkLeaveRadiusCm = 200.f;
	static constexpr float RepathIntervalSeconds = 0.5f;
	static constexpr float RepathIntervalMinutes = 1.f;
	static constexpr float RepathTargetMoveCm = 50.f;
	static constexpr float StallTimeoutMinutes = 5.f;
	static constexpr float MinProgressCm = 50.f;
	static constexpr int32 MaxFailCount = 2;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FFF_StaffDispatchJourneyTest;
	friend class FFF_StaffDispatchIncidentTest;
	friend class FFF_StaffDispatchGiveUpTest;
#endif
};
