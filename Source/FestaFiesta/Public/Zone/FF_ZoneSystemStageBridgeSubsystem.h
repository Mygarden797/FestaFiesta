// FF_ZoneSystemStageBridgeSubsystem.h

// Festival Stage와 FF_ZoneSystem의 단계·시설 생명주기 계약을 선언한다.
// 흐름: Stage 변경을 ChangePhase와 시설 개폐 배치로 변환하고 결과를 Blueprint에 전달한다.
// 불변식: Stage가 배치 권한의 기준이며, 자동 명령은 실제 Live 진입·이탈 edge에서만 생성한다.
// 실패/수명: 늦게 준비된 Stage를 재결합하며 종료 시 정책·delegate·예약 명령을 모두 해제한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSExternalPolicy.h"
#include "Subsystems/WorldSubsystem.h"
#include "Time/FF_FestivalStage.h"
#include "FF_ZoneSystemStageBridgeSubsystem.generated.h"

class UFF_FestivalStageSubsystem;
class UFFZSWorldSubsystem;

/** 한 번의 시설 개장/폐쇄 배치가 수행된 이유입니다. */
UENUM(BlueprintType)
enum class EFF_ZoneFacilityLifecycleAction : uint8
{
	None,
	AutomaticOpen,
	AutomaticClose,
	ManualOpen,
	ManualClose,
	ManualEmergencyClose
};

/** 자동 운영과 테스트 패널이 공유하는 시설 개폐 배치 결과입니다. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_ZoneFacilityLifecycleBatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Integration|Lifecycle")
	EFF_ZoneFacilityLifecycleAction Action = EFF_ZoneFacilityLifecycleAction::None;

	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Integration|Lifecycle")
	int32 AttemptedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Integration|Lifecycle")
	int32 SucceededCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Integration|Lifecycle")
	int32 FailedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Integration|Lifecycle")
	int32 SkippedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Integration|Lifecycle")
	int64 StartingWorldRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Integration|Lifecycle")
	int64 FinalWorldRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Integration|Lifecycle")
	TArray<FGuid> FailedFacilityIds;

	UPROPERTY(BlueprintReadOnly, Category = "FF Zone System|Integration|Lifecycle")
	FText Summary;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFF_OnZoneFacilityLifecycleBatchCompleted,
	const FFF_ZoneFacilityLifecycleBatchResult&,
	Result);

/**
 * 프로젝트의 축제 진행 단계와 FF_ZoneSystem의 시설 변경 단계를 연결한다.
 *
 * FestivalStage는 GameInstance 수명이고 ZoneSystem은 World 수명이므로, 월드마다 이 브리지가
 * 현재 단계를 한 번 맞춘 뒤 OnStageChanged를 구독한다. Stage 팀과 플러그인 어느 쪽도 서로의
 * 헤더를 직접 참조하지 않게 해 모듈 경계를 유지한다.
 */
UCLASS()
class FESTAFIESTA_API UFF_ZoneSystemStageBridgeSubsystem final
	: public UTickableWorldSubsystem
	, public IFFZSExternalPolicy
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** 현재 FestivalStage를 이 월드의 FFZS ChangePhase에 즉시 반영한다. */
	UFUNCTION(BlueprintCallable, Category = "FF Zone System|Integration|Stage")
	bool SynchronizeCurrentStage();

	/** 8개 프로젝트 단계를 FFZS의 4개 운영 단계로 바꾸는 단일 매핑 지점이다. */
	UFUNCTION(BlueprintPure, Category = "FF Zone System|Integration|Stage")
	static EFFZSChangePhase ToZoneChangePhase(EFF_FestivalStage FestivalStage);

	/** 실제 Live 진입/이탈에서만 자동 개폐 명령을 만드는 순수 edge 판정입니다. */
	static bool TryGetAutomaticLifecycleCommand(
		EFF_FestivalStage PreviousStage,
		EFF_FestivalStage NewStage,
		EFFZSCommandType& OutCommandType);

	/** Live에서 Validated/MovePending 시설을 열고, 선택적으로 수동 Closed 시설도 다시 엽니다. */
	UFUNCTION(BlueprintCallable, Category = "FF Zone System|Integration|Lifecycle")
	FFF_ZoneFacilityLifecycleBatchResult OpenEligibleFacilities(
		bool bIncludeClosedFacilities = true);

	/** 현재 Active 시설을 정상 폐쇄하거나 테스트 목적의 긴급 폐쇄로 전환합니다. */
	UFUNCTION(BlueprintCallable, Category = "FF Zone System|Integration|Lifecycle")
	FFF_ZoneFacilityLifecycleBatchResult CloseActiveFacilities(bool bEmergency = false);

	UFUNCTION(BlueprintPure, Category = "FF Zone System|Integration|Lifecycle")
	FFF_ZoneFacilityLifecycleBatchResult GetLastLifecycleBatchResult() const
	{
		return LastLifecycleBatchResult;
	}

	UPROPERTY(BlueprintAssignable, Category = "FF Zone System|Integration|Lifecycle")
	FFF_OnZoneFacilityLifecycleBatchCompleted OnFacilityLifecycleBatchCompleted;

	// IFFZSExternalPolicy: Production 통합 준비도에서 SimulationTime 제공자로 참여한다.
	virtual FName GetFFZoneSystemPolicyName_Implementation() const override;
	virtual EFFZSPolicyDomain GetFFZoneSystemPolicyDomain_Implementation() const override;
	virtual FFFZSPolicyDecision PreValidateFFZoneSystemChange_Implementation(
		const FFFZSPolicyContext& Context) const override;
	virtual FFFZSPolicyDecision ApplyTentativeFFZoneSystemChange_Implementation(
		const FFFZSPolicyContext& Context) override;
	virtual void CommitFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;
	virtual void RollbackFFZoneSystemChange_Implementation(const FFFZSPolicyContext& Context) override;

private:
	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	bool TryBindFestivalStage();
	bool TrySynchronizeStage(bool bScheduleOneRetry);
	void RetryStageSynchronization();
	void ScheduleAutomaticLifecycle();
	void RunScheduledAutomaticLifecycle();
	FFF_ZoneFacilityLifecycleBatchResult ExecuteLifecycleBatch(
		EFFZSCommandType CommandType,
		EFF_ZoneFacilityLifecycleAction Action,
		bool bIncludeClosedFacilities);

	UPROPERTY(Transient)
	TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalStageSubsystem> FestivalStage;

	bool bPolicyRegistered = false;
	bool bStageSyncRetryScheduled = false;
	bool bAutomaticLifecycleScheduled = false;
	int32 AutomaticLifecycleRetryCount = 0;
	TOptional<EFFZSCommandType> PendingAutomaticLifecycleCommand;

	UPROPERTY(Transient)
	FFF_ZoneFacilityLifecycleBatchResult LastLifecycleBatchResult;
};
