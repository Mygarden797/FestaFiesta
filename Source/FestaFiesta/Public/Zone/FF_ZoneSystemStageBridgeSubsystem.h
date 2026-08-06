// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSExternalPolicy.h"
#include "Subsystems/WorldSubsystem.h"
#include "Time/FF_FestivalStage.h"
#include "FF_ZoneSystemStageBridgeSubsystem.generated.h"

class UFF_FestivalStageSubsystem;
class UFFZSWorldSubsystem;

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

	bool TrySynchronizeStage(bool bScheduleOneRetry);
	void RetryStageSynchronization();

	UPROPERTY(Transient)
	TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalStageSubsystem> FestivalStage;

	bool bPolicyRegistered = false;
	bool bStageSyncRetryScheduled = false;
};
