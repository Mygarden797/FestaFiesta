// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_CrowdSimulationSubsystem.generated.h"

class AFF_BaseCrowd;
struct FUnitData;
/**
 * 군중 시뮬레이션을 관리하는 TickableWorldSubsystem
 * Actor 풀과 Agent 풀을 분리 관리하여 필요한 Agent에만 Actor를 바인딩하는 방식으로 동작
 */
UCLASS()
class FESTAFIESTA_API UFF_CrowdSimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
    
public:
	virtual void Tick(float DeltaTime) override;

	// 월드 시작 시 Actor/Agent 풀을 초기화
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    
	// 통계 시스템(Stat Group)에 등록할 StatId 반환
	virtual TStatId GetStatId() const override;
    
	// 지정한 AgentIndex에 비어있는 Actor 슬롯을 바인딩 
	void BindAgentToPool(int32 AgentIndex);

	// AgentIndex에 바인딩된 Actor를 다시 풀로 반환 
	void ReleaseAgentToPool(int32 AgentIndex);

	// Agent 풀에서 빈 슬롯을 찾아 새 Agent를 스폰하고, 스폰된 Agent의 인덱스를 반환
	int32 SpawnAgent(UWorld& InWorld, FVector SpawnLocation, FRotator SpawnRotation);

	// ActorPoolSize만큼 AFF_BaseCrowd Actor를 미리 생성하여 CrowdPool을 채움
	void InitializeCrowdActorPool(UWorld& InWorld, FVector ActorPoolLocation);

	// AgentPoolSize만큼 Agent 데이터(UnitDatas)를 미리 생성하여 Agent 풀을 채움
	void InitializeCrowdAgentPool(UWorld& InWorld, FVector AgentPoolLocation);

    
public:
	// 실제 스폰되어 있는 Crowd Actor들의 풀
	UPROPERTY()
	TArray<AFF_BaseCrowd*> CrowdPool;
    
	// CrowdPool 중 현재 비어있는(미할당) Actor 슬롯 인덱스 목록
	UPROPERTY()
	TArray<int32> FreeActorSlotIndex;
    
	// Agent 풀 중 현재 비어있는(미할당) 슬롯 인덱스 목록
	UPROPERTY()
	TArray<int32> FreeAgentSlotIndex;
    
	// AgentIndex -> 바인딩된 ActorSlot 인덱스 매핑 (미바인딩 시 -1 등으로 표시)
	UPROPERTY()
	TArray<int32> AgentToActorSlot;
    
	// Actor 풀의 최대 크기 (동시에 실체화 가능한 Crowd Actor 수)
	UPROPERTY()
	int32 ActorPoolSize = 500;
    
	// Agent 풀의 최대 크기 (Actor 바인딩 여부와 무관한 논리적 개체 수)
	UPROPERTY()
	int32 AgentPoolSize = 500;
    
    
private:
	// 각 Agent의 실제 시뮬레이션 데이터(위치, 상태 등)를 저장하는 배열
	TArray<FUnitData> UnitDatas;
};
