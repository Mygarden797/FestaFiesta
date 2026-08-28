// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/FF_SpawnActor.h"

#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"


AFF_SpawnActor::AFF_SpawnActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AFF_SpawnActor::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	UFF_CrowdSimulationSubsystem* CrowdSim = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	if (!IsValid(CrowdSim))
	{
		UE_LOG(LogTemp, Warning, TEXT("[FF_SpawnActor] CrowdSimulationSubsystem not found"));
		return;
	}

	// 서브시스템의 OnWorldBeginPlay는 이 BeginPlay보다 먼저 끝났지만(UWorld::BeginPlay 순서)
	// 거기서는 풀을 만들지 않는다. 어떤 Actor 클래스를 쓸지 아는 건 이쪽이므로 초기화도 여기서 건다.
	 FVector Origin = GetActorLocation();
	CrowdSim->InitializeSimulation(Origin, SimRadiusUU, ActorPoolSize, AgentPoolSize, CrowdActorClass);

	CrowdSim->bDrawDebugAgents = bDrawDebugAgents;
	CrowdSim->bDrawDebugDensity = bDrawDebugDensity;

	// 파티 설정은 SpawnAgents보다 먼저 넘겨야 한다. 파티 구성은 스폰 시점에 확정되기 때문이다.
	CrowdSim->bEnablePartyGrouping = bEnablePartyGrouping;
	CrowdSim->SetTemp_ExitWorldLocation(Origin + Temp_ExitOffsetFromOrigin);

	if (IsValid(SpawnAchor))
	{
		Origin = SpawnAchor->GetActorLocation();
	}
	
	
	if (CrowdSim->IsArrivalCurveActive())
	{
		CrowdSim ->BeginArrivalSchedule(UnitCount,Origin,SpawnRadiusUU);
	}
	else
	{
		CrowdSim->SpawnAgents(UnitCount, Origin, SpawnRadiusUU);	
	}
	
}
