// Fill out your copyright notice in the Description page of Project Settings.



#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "CrowdSystem/FF_BaseNPC.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"


void UFF_CrowdSimulationSubsystem::Tick(float DeltaTime)
{
	//성능 확인을 위한 코드
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Tick);
}

TStatId UFF_CrowdSimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_CrowdSimulationSubsystem, STATGROUP_Tickables);
}



void UFF_CrowdSimulationSubsystem::SpawnNPC(int32 Count, UWorld* SpawnWorld)
{
	if (!SpawnWorld)
	{
		UE_LOG(LogTemp,Warning, TEXT("[CrowdSimulationSubsystem] SpawnWorld is inValid"))	
	}
	for (int32 i=0;i<Count;i++)
	{
		FActorSpawnParameters SpawnParameters;
		FVector SpawnPosition = FVector(FMath::RandRange(-1000.f, 1000.f), FMath::RandRange(-1000.f,1000.f),0.f);
		SpawnWorld->SpawnActor<AFF_BaseNPC>(AFF_BaseNPC::StaticClass(),SpawnPosition,FRotator::ZeroRotator,SpawnParameters);
	}
}
