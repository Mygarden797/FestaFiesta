// Fill out your copyright notice in the Description page of Project Settings.



#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/FF_BaseNPC.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"


void UFF_CrowdSimulationSubsystem::Tick(float DeltaTime)
{
	//성능 확인을 위한 코드
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Tick);
}

void UFF_CrowdSimulationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	InitializeCrowdActorPool(InWorld,FVector::ZeroVector);
	InitializeCrowdAgentPool(InWorld, FVector::ZeroVector);
	
	
}

TStatId UFF_CrowdSimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_CrowdSimulationSubsystem, STATGROUP_Tickables);
}

void UFF_CrowdSimulationSubsystem::BindAgentToPool(int32 AgentIndex)
{

	if (AgentIndex == INDEX_NONE || !UnitDatas.IsValidIndex(AgentIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Invalid AgentIndex"));
		return;
	}
	
	
	if (AgentToActorSlot[AgentIndex] != INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Agent %d is already bound"), AgentIndex);
		return;
	}
	
	
	if (FreeActorSlotIndex.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] No free Actor slot available"));
		return;
	}
	
	int32 FreeActorIndex = FreeActorSlotIndex.Pop();
	CrowdPool[FreeActorIndex]->BindToAgent(AgentIndex, UnitDatas[AgentIndex].Position,UnitDatas[AgentIndex].Rotation);
	CrowdPool[FreeActorIndex]->SetActorHiddenInGame(false);
	CrowdPool[FreeActorIndex]->SetActorEnableCollision(true);
	
	AgentToActorSlot[AgentIndex] = FreeActorIndex;
	
	
	
	
	
	
	
	
}

void UFF_CrowdSimulationSubsystem::ReleaseAgentToPool(int32 AgentIndex)
{
	if (AgentIndex == INDEX_NONE || !UnitDatas.IsValidIndex(AgentIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Invalid AgentIndex"));
		return;
	}
	
	if (AgentToActorSlot[AgentIndex] == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] Agent %d is already released"), AgentIndex);
		return;
	}

	int32 ReleaseActorIndex = AgentToActorSlot[AgentIndex];
	
	CrowdPool[ReleaseActorIndex]->ReleaseFromAgent();
	CrowdPool[ReleaseActorIndex]->SetActorHiddenInGame(true);
	CrowdPool[ReleaseActorIndex]->SetActorEnableCollision(false);
	
	FreeActorSlotIndex.Add(ReleaseActorIndex);
	AgentToActorSlot[AgentIndex] = INDEX_NONE;
	
	
	
	
	
}


int32 UFF_CrowdSimulationSubsystem::SpawnAgent(UWorld& InWorld, FVector SpawnLocation, FRotator SpawnRotation)
{
	if (FreeAgentSlotIndex.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CrowdSimulationSubsystem] No free agent slot available"));
		return INDEX_NONE;
	}
	
	int32 index = FreeAgentSlotIndex.Pop();
	UnitDatas[index].Position = SpawnLocation;
	UnitDatas[index].Rotation = SpawnRotation;
	
	return index;
}

void UFF_CrowdSimulationSubsystem::InitializeCrowdActorPool(UWorld& InWorld, FVector PoolLocation)
{
	if (!IsValid(&InWorld))
	{
		UE_LOG(LogTemp,Warning, TEXT("[CrowdSimulationSubsystem] World is inValid"))
		return;
	}
	
	FreeActorSlotIndex.Reserve(ActorPoolSize);
	for (int32 i=0;i<ActorPoolSize;i++)
	{
		AFF_BaseCrowd* NewCrowd = InWorld.SpawnActor<AFF_BaseCrowd>(AFF_BaseCrowd::StaticClass(),PoolLocation,FRotator::ZeroRotator);
		NewCrowd->SetActorHiddenInGame(true);
		NewCrowd->SetActorEnableCollision(false);
		CrowdPool.Add(NewCrowd);
		FreeActorSlotIndex.Add(i);
		
		 
		
	}
}

void UFF_CrowdSimulationSubsystem::InitializeCrowdAgentPool(UWorld& InWorld, FVector AgentPoolLocation)
{
	if (!IsValid(&InWorld))
	{
		UE_LOG(LogTemp,Warning, TEXT("[CrowdSimulationSubsystem] World is inValid"))
		return;
	}
	
	FreeAgentSlotIndex.Reserve(AgentPoolSize);
	
	
	for (int32 i=0;i<AgentPoolSize;i++)
	{
		FUnitData NewUnit;
		NewUnit.Position = AgentPoolLocation;
		
		UnitDatas.Add(NewUnit);
		FreeAgentSlotIndex.Add(i);
		AgentToActorSlot.Add(INDEX_NONE);
		
		//todo...Group 결정과 Archtype에 의한 스폰 비율 결정을 추가할 것.
	}
	
}


