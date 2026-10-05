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
class UFF_StaffFacilitySubsystem;
class UGameplayEffect;
class UNavigationSystemV1;
struct FFF_StaffDispatchOrigin;

// 스태프 몸(AFF_StaffActor, 클래스는 IncidentSettings::StaffActorClass)의 스폰·이동·도착을 맡는다. 사건을 모르고, 도착은 델리게이트로만 알린다
// 걸음에 관한 판단은 전부 여기서 한다. 바깥은 SendStaff(보내라) / RecallStaff(돌아와라)만 말하고 GetStaffState 로 결과를 읽는다.
// 모든 시간(걸음·재경로 간격·막힘 판정)은 실시간 DeltaTime × 축제 실효 배속으로 흐른다. 일시정지(배속 0)면 그 자리에 선다.
//  Moving    : 배속이 적용된 DeltaTime × TravelSpeed 로 걷는다. 목표가 50cm 넘게 움직였거나 경로 끝에 닿았는데 목표 200cm 밖이면
//              0.5초마다 길을 다시 찾는다. 경로 끝에서 목표 200cm 안이면 Working.
//  도착 보험 : 재경로 실패, 또는 5초 동안 목표 거리가 50cm 도 줄지 않으면 실패 1회. 거리가 줄면 실패 횟수는 0 으로 돌아간다.
//              실패가 2회 쌓이면 목표 위치로 순간이동해 Working 이 된다(경고 로그). 지나온 길은 그 자리에서 다시 시작한다.
//  Working   : 제자리. 목표가 200cm 넘게 떠나면 Moving 으로 돌아가 다시 따라간다.
//  Returning : 지나온 길을 거꾸로 되짚고, 0.5초마다 집(소속 시설 출구)이 옮겨졌는지 확인한다.
//              집에 닿거나 집이 사라지면 몸을 지우고 Idle 이 된다. 순간이동한 스태프는 되짚을 길이 없어 그 자리에서 사라진다.
// 목표 위치를 알 수 없을 때만 출발이 실패한다. 내비 경로가 없어도 몸을 만들고 보험에 맡긴다.
// 경로 찾기(FindPath): 출발·목표를 먼저 좁게(수평 200·수직 400 탐색, 수평·수직 100cm 이내) NavMesh 에 올린다.
//  안 되면 넓게(수평 500·수직 100 탐색) 다시 올린다. 시설 출구처럼 NavMesh 구멍 안에 있는 점이 밑층이 아니라 같은 높이 바닥에 붙게 하려는 것이다.
//  수직 100cm 제한은 두 경우 모두 지킨다. 경로는 출발점 → NavMesh 경로 → 목표점 순서이고, 앞뒤는 직선으로 잇는다(몸 충돌이 꺼져 있다).
//  중간에 끊긴 경로(Partial)도 받아서 끊긴 곳까지 걷는다. 그 뒤는 재경로와 도착 보험이 처리한다.
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
		float BestDistance = 0.f;
		float StallElapsed = 0.f;
		int32 FailCount = 0;
	};

	// --- 명단 ---
	void SyncActorsWithRoster();

	UFUNCTION()
	void HandleStaffRosterChanged();

	// --- 이동 ---
	bool TickRuntime(int32 StaffId, FStaffRuntime& Runtime, float DeltaTime);
	bool MoveStaff(FStaffRuntime& Runtime, float DeltaTime);
	void HoldAtTarget(int32 StaffId, FStaffRuntime& Runtime, const FVector& TargetNow);
	void GiveUp(FStaffRuntime& Runtime);
	bool ResolveTargetLocation(const FStaffRuntime& Runtime, FVector& OutLocation) const;
	bool RefreshReturnHome(int32 StaffId, FStaffRuntime& Runtime);
	bool FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPath) const;
	static bool ProjectEndpoint(const UNavigationSystemV1& Navigation, const FVector& Point, const TCHAR* Label, FVector& OutNavLocation);
	static bool IsProjectionAcceptable(const FVector& Point, const FVector& Projected, float MaxGap2DCm);
	static void AssemblePath(const FVector& From, const FVector& To, const TArray<FVector>& NavPoints, bool bPartial, TArray<FVector>& OutPath);
	static bool AdvanceAlongPath(FStaffRuntime& Runtime, float Distance);
	static void ReverseTravelledPath(FStaffRuntime& Runtime);

	// --- 도착 보험 ---
	static float GetRemainingDistance(const FStaffRuntime& Runtime, const FVector& TargetNow);
	static void ResetProgress(FStaffRuntime& Runtime, const FVector& TargetNow);
	bool CheckProgress(FStaffRuntime& Runtime, const FVector& TargetNow, float DeltaTime);
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
	TSubclassOf<AFF_StaffActor> StaffActorClass;

	TWeakObjectPtr<AFF_StaffBase> Base;

	TMap<int32, FStaffRuntime> Staff;
	float TravelSpeed = 300.f;

	static constexpr float ArrivalRadiusCm = 200.f;
	static constexpr float WorkLeaveRadiusCm = 200.f;
	static constexpr float RepathIntervalSeconds = 0.5f;
	static constexpr float RepathTargetMoveCm = 50.f;
	static constexpr float StallTimeoutSeconds = 5.f;
	static constexpr float MinProgressCm = 50.f;
	static constexpr int32 MaxFailCount = 2;
	static constexpr float NearProjectionGapCm = 100.f;
	static constexpr float WideProjectionHorizontalCm = 500.f;
	static constexpr float ProjectionMaxGapZCm = 100.f;

#if WITH_DEV_AUTOMATION_TESTS
	friend class FFF_StaffDispatchJourneyTest;
	friend class FFF_StaffDispatchIncidentTest;
	friend class FFF_StaffDispatchWarpTest;
	friend class FFF_StaffDispatchStallTest;
	friend class FFF_StaffTimeScaleTest;
	friend class FFF_StaffPathProjectionTest;
#endif
};
