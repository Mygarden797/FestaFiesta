// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

	/**
	 * 화면에 실체화할 Actor 클래스. SkeletalMesh와 UFF_CrowdAnimInstance 기반 AnimBP가
	 * 설정된 AFF_BaseCrowd 상속 BP를 지정한다.
	 *
	 * 비워 두면 메시가 없는 AFF_BaseCrowd가 스폰되어 아무것도 보이지 않는다.
	 * 시뮬레이션과 밀도 계측은 그래도 정상 동작한다.
	 */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	TSubclassOf<AFF_BaseCrowd> CrowdActorClass;

	// 스폰할 인원 수. AgentPoolSize를 넘으면 넘는 만큼은 스폰되지 않는다
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	int32 UnitCount = 500;

	/**
	 * 동시에 화면에 실체화할 수 있는 Actor 수. UnitCount보다 작아도 된다.
	 *
	 * 작게 잡으면 일부 Agent는 시뮬레이션만 되고 화면에 안 나온다.
	 * 밀도/혼잡도 수치에는 전원이 반영되므로 분석 결과는 이 값에 영향받지 않는다.
	 */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	int32 ActorPoolSize = 500;

	// 논리적 Agent 수의 상한. UnitCount 이상이어야 한다
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	int32 AgentPoolSize = 500;

	// 이 액터 위치 기준 스폰 반경, UU
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	float SpawnRadiusUU = 3000.f;

	// 시뮬레이션/밀도 그리드가 덮는 반경, UU. 목적지도 이 안에서 뽑힌다.
	// 스폰 반경보다 넉넉해야 한다 — 좁으면 목적지가 몰려 인위적인 혼잡이 생긴다
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd")
	float SimRadiusUU = 10000.f;

	/**
	 * 같이 온 사람들을 파티로 묶어 함께 이동시킨다.
	 *
	 * 끄면 전원이 1인으로 개별 이동한다. 그룹 이동이 혼잡도에 주는 영향을 비교할 때 A/B로 쓴다.
	 * 파티 구성은 스폰 시점에 결정되므로 이 값은 BeginPlay 전에 정해져 있어야 한다.
	 */
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd|Party")
	bool bEnablePartyGrouping = true;

	// 파티 크기 1~6인의 추첨 가중치. 비워 두면 서브시스템 기본값을 쓴다.
	// 실측 분포(FFF_GroupSizeDistributionRow) 연동 전까지의 임시 손잡이다
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Crowd|Party")
	TArray<float> PartySizeWeights;

	// 에디터에서 Agent 위치/목적지 선을 그린다
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Debug")
	bool bDrawDebugAgents = false;

	// 에디터에서 밀도 그리드를 LOS 색상 박스로 그린다
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "FF|Debug")
	bool bDrawDebugDensity = true;
};
