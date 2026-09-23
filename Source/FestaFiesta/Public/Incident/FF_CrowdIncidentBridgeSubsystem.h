// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_CrowdIncidentBridgeSubsystem.generated.h"

class UFF_CrowdSimulationSubsystem;
class UFF_IncidentSubsystem;
struct FFF_CrowdCongestionSnapshot;
struct FFF_CrowdMemberMissingEvent;

// Crowd 알림을 Incident Report/Withdraw 로 옮긴다. Crowd 발 사건은 전부 여기를 거친다
UCLASS()
class FESTAFIESTA_API UFF_CrowdIncidentBridgeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	// --- 밀집 ---
	void HandleCongestionSampled(const FFF_CrowdCongestionSnapshot& Snapshot);

	// --- 미아 ---
	void HandleMemberMissingChanged(const FFF_CrowdMemberMissingEvent& Event);
	void ReportMissing(const FFF_CrowdMemberMissingEvent& Event);
	void WithdrawMissing(int32 StableAgentID);

	UPROPERTY()
	TObjectPtr<UFF_CrowdSimulationSubsystem> Crowd;

	UPROPERTY()
	TObjectPtr<UFF_IncidentSubsystem> Incident;

	FDelegateHandle CongestionHandle;
	FDelegateHandle MissingHandle;

	TMap<FIntPoint, int32> CongestionStreakByCell;
	TSet<FIntPoint> OpenCongestionCells;

	TMap<int32, FName> MissingKeyByAgent;
};
