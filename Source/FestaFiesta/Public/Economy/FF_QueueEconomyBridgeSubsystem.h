// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Queue/FF_QueueTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_QueueEconomyBridgeSubsystem.generated.h"

class UFF_QueueSubsystem;
class UFF_FestivalProgressionSubsystem;

/**
* 시설 이용 완료를 부스 수수료 매출로 옮기는 브리지
 * 경제가 Queue 를 구독하는 단방향 방식이다.
 * 
 * Queue·Crowd 는 World 스코프이고 경제는 GameInstance 스코프라,
 * 이 브리지는 World 에 살면서 경제를 호출 시점에 찾는다.
 */
UCLASS()
class FESTAFIESTA_API UFF_QueueEconomyBridgeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	// 이 퇴장 유형이 매출을 만드는가.
	// Served 만 이용 완료다 — Abandoned·Cancelled 는 줄을 섰다 나간 것이라 결제가 없다.
	static bool IsPaidExit(EFF_QueueExit ExitType)
	{
		return ExitType == EFF_QueueExit::Served;
	}
	
private:
	// 시설 이용을 마친 팀의 확정 인원으로 수수료를 계산해 원장에 넣는다.
	UFUNCTION()
	void HandlePartyServiceCompleted(
		int32 FacilityId,
		int32 PartyId,
		int32 ServedMemberCount,
		float WaitedSeconds);
	
	// 경제는 GameInstance 스코프라 호출 시점에 찾는다. 초기화 순서에 기대지 않는다.
	UFF_FestivalProgressionSubsystem* ResolveFestivalProgression() const;

	UPROPERTY()
	TObjectPtr<UFF_QueueSubsystem> Queue;
	
	// 네이밍 컨벤션 위반을 시설 정의당 한 번만 알린다.
	UPROPERTY()
	TSet<FName> WarnedNamingViolations;
};
