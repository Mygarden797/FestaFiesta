// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_CrowdPerformanceBridgeSubsystem.generated.h"

class UFF_CrowdSimulationSubsystem;

// 공연 관객의 연출을 정한다. Crowd 는 "누가 어느 시설 좌석에 앉았나"만 알려 주고,
// 그 시설이 메인 스테이지인지·지금 춤출 때인지는 여기서만 판단한다. 판단 결과는
// SetCrowdStateForAgent 로만 내려 이동·좌석 상태를 건드리지 않는다
UCLASS()
class FESTAFIESTA_API UFF_CrowdPerformanceBridgeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	float SyncIntervalSeconds = 0.25f;

private:
	bool ShouldDance(const FFF_CrowdSeatedAgent& Seated) const;

	UPROPERTY()
	TObjectPtr<UFF_CrowdSimulationSubsystem> Crowd;

	TArray<FFF_CrowdSeatedAgent> ScratchSeated;
	TSet<int32> DancingAgents;
	float SecondsSinceSync = 0.f;
};
