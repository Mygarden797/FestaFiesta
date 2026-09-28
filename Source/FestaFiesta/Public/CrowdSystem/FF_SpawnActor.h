// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "FF_SpawnActor.generated.h"

class AFF_BaseCrowd;


/**
 * 레벨에 놓아 군중 시뮬레이션을 시작시키는 액터.
 *
 * 시뮬레이션 자체는 UFF_CrowdSimulationSubsystem이 전부 담당한다. 이 액터가 하는 일은
 * "어디를, 얼마나 넓게, 몇 명으로" 를 자기 트랜스폼 기준으로 알려주는 것뿐이다.
 * 레벨에 하나만 놓는다.
 */
UCLASS()
class FESTAFIESTA_API AFF_SpawnActor : public AActor
{
	GENERATED_BODY()

public:

	AFF_SpawnActor();

protected:

	virtual void BeginPlay() override;

	//실체화할 Actor Class
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	TSubclassOf<AFF_BaseCrowd> CrowdActorClass;

	// 스폰할 인원 수. 경제가 예상 방문객을 넘겨주면 그 값이 우선한다(이 값은 폴백)
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	int32 UnitCount = 1000;

	// 화면에 실체화할 Actor 수. AgentPoolSize 와 같게 둔다 — 작으면 보이지 않는 Agent 가 생긴다
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	int32 ActorPoolSize = 1100;

	// 동시에 떠 있을 수 있는 Agent 상한. 하루 누적이 아니라 동시 인원이다. 퇴장하면 슬롯을 다시 쓴다
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	int32 AgentPoolSize = 1100;

	// 이 액터 위치 기준 스폰 반경, UU
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	float SpawnRadiusUU = 10.f;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd|Temp")
	FVector Temp_ExitOffsetFromOrigin = FVector(-4000.f, 0.f, 0.f);
	//같이 온 사람들을 전부 묶어서 이동시킬 것일까?
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd|Party")
	bool bEnablePartyGrouping = true;

	// 에디터에서 Agent 위치/목적지 선을 그린다
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Debug")
	bool bDrawDebugAgents = false;

	// 에디터에서 밀도 그리드를 LOS 색상 박스로 그린다
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Debug")
	bool bDrawDebugDensity = true;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	TObjectPtr<AActor> SpawnAchor;

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	TObjectPtr<AActor> DespawnAchor;

	// 입장·퇴장 게이트. 비워두면 단일 앵커로 돌고, 순서가 인덱스라 중간에 끼우지 말고 뒤에 추가한다.
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd|Gate")
	TArray<FFF_CrowdGate> CrowdGates;
};
