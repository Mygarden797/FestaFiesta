// Fill out your copyright notice in the Description page of Project Settings.


#include "FestaFiesta/Public/Systems/FF_CrowdSimulationSubsystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "FestaFiesta/Public/Types/FF_FestivalCrowdTypes.h"

void UFF_CrowdSimulationSubsystem::Tick(float DeltaTime)
{
	//성능 확인을 위한 코드
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Tick);
	if (!ISMComponent) return;
	
	
	
	for (FUnitData& Unit: Units)
	{
		const FVector ToTarget = Unit.TargetLocation - Unit.Position;
		if (ToTarget.SizeSquared() > 100.f)
		{
			//벡터의 방향만 남기고 정규화(GetSafeNormal) 이후 Speed를 통해서 속력 계산
			Unit.Velocity = ToTarget.GetSafeNormal() * Unit.Speed;
			//갈 거리 계산
			Unit.Position += Unit.Velocity * DeltaTime;
			//ISMC의 인스턴스를 업데이트 한다.
			ISMComponent->UpdateInstanceTransform(Unit.InstanceIndex,FTransform(Unit.Position), true, false, false);
		}
	}
	//컴포넌트의 렌더링 정보가 바뀌었으므로 GPU에게 다시 업데이트 하는 것이다.
	ISMComponent->MarkRenderStateDirty();
	
}

TStatId UFF_CrowdSimulationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_CrowdSimulationSubsystem, STATGROUP_Tickables);
}

void UFF_CrowdSimulationSubsystem::SpawnUnits(int32 Count, UInstancedStaticMeshComponent* InISMComponent)
{
	ISMComponent = InISMComponent;
	Units.Reserve(Count);
	
	for (int32 i=0; i< Count; ++i)
	{
		FUnitData NewUnit;
		NewUnit.Position = FVector(FMath::RandRange(-1000.f, 1000.f), FMath::RandRange(-1000.f,1000.f),0.f);
		NewUnit.TargetLocation = FVector(FMath::RandRange(-1000.f, 1000.f), FMath::RandRange(-1000.f, 1000.f), 0.f);
		NewUnit.InstanceIndex = ISMComponent->AddInstance(FTransform(NewUnit.Position));
		Units.Add(NewUnit);
		
	}
	
}
