// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Incident/FF_IncidentTypes.h"
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
#if WITH_DEV_AUTOMATION_TESTS
	friend class FFF_CrowdIncidentBridgeTest;
#endif

	// --- 밀집 ---
	void HandleCongestionSampled(const FFF_CrowdCongestionSnapshot& Snapshot);
	void ResetCongestionTracking(bool bWithdrawOpenIncidents);

	// --- 미아 ---
	void HandleMemberMissingChanged(const TArray<FFF_CrowdMemberMissingEvent>& Events);
	void ReportMissing(const FFF_CrowdMemberMissingEvent& Event);
	void WithdrawMissing(int32 StableAgentID);
	
	//---분실물 ---
	void HandleItemLost(const TArray<FFF_CrowdItemLostEvent>& Events);

	UPROPERTY()
	TObjectPtr<UFF_CrowdSimulationSubsystem> Crowd;

	UPROPERTY()
	TObjectPtr<UFF_IncidentSubsystem> Incident;

	FDelegateHandle CongestionHandle;
	FDelegateHandle MissingHandle;
	FDelegateHandle ItemLostHandle;

	TMap<FIntPoint, int32> CongestionStreakByCell;
	TMap<FIntPoint, int32> CongestionClearStreakByCell;
	// 처리된 사건도 셀이 연속 해소될 때까지 남겨 같은 혼잡 중 재발생을 막는다.
	TMap<FIntPoint, FFF_IncidentHandle> OpenCongestionHandles;
	int32 LastCongestionSnapshotIndex = INDEX_NONE;
	float LastCongestionSimMinutes = 0.f;
	float LastCongestionCellSizeMeters = 0.f;

	TMap<int32, FName> MissingKeyByAgent;
	
	
	int32 CongestionSustainSnapShots = 10;
};
