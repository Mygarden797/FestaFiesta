// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_FestivalCrowdTypes.generated.h"



enum class EUnitAnimationState : uint8
{
	Idle,
	Walk,
	Run
};



USTRUCT(BlueprintType)
struct FUnitData
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	FVector Position = FVector::ZeroVector;

	// 직전 고정 스텝이 시작될 때의 위치.
	// 시뮬레이션은 30Hz 고정 스텝인데 화면은 60fps 이상으로 그려지므로,
	// 렌더링할 때 PrevPosition <-> Position 사이를 보간하지 않으면 끊겨 보인다.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	FVector PrevPosition = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	FVector TargetLocation = FVector::ZeroVector;

	// 아무도 없을 때 내고 싶은 속도(희망 속도), cm/s. 150cm/s = 1.5m/s로 보통 성인 보행 속도.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	float Speed = 150.f;

	// 풀에 미리 만들어 둔 슬롯은 비활성이 기본이다.
	// 여기가 true면 SpawnAgent를 거치지 않은 빈 슬롯까지 시뮬레이션 루프가 돌게 된다.
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	bool bIsActive = false;
	
	//----Group---//
	FName ArchetypeID = NAME_None;

	FName CommandType = NAME_None;

	// 소속 파티. 1인 방문객도 멤버 1명짜리 파티로 처리해 코드 경로를 하나로 통일한다.
	// 스폰되지 않은 빈 슬롯만 INDEX_NONE이다
	int32 PartyID = INDEX_NONE;

	/**
	 * 이 Agent가 목적지를 물려받는 리더의 슬롯 인덱스.
	 *
	 * 자기 자신을 가리키면 리더다(1인 파티는 항상 자기 자신). 별도 bIsLeader를 두지 않는 이유는
	 * 멤버가 매 스텝 리더를 찾아야 해서 어차피 인덱스가 필요하고, 두 필드가 어긋날 여지를 없애기 위해서다.
	 */
	int32 LeaderAgentIndex = INDEX_NONE;

	// 파티 내 자리 번호. 0이 리더. 대형(formation) 오프셋을 이 번호로 계산한다
	int32 PartySlotIndex = 0;

	// 재현성을 위한 고정 ID. 슬롯 인덱스는 재사용되므로 그것과 별개로 둔다
	int32 StableAgentID = INDEX_NONE;
	
	//---회피 관련 변수들 ---//
	FVector Force = FVector::ZeroVector;
	
	float MaxSpeed = 150.f;
	
	float Radius = 34.f;
	
	//Animation
	
	
};

USTRUCT(BlueprintType)
struct FCrowdGroupTypes
{
	GENERATED_BODY()
	
	// UPROPERTY는 반드시 초기화해야 한다. 엔진이 시작 시 검사해서
	// "IntProperty ... is not initialized properly" 에러를 띄운다.
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Crowd|Group")
	int32 GroupID = INDEX_NONE;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Crowd|Group")
	FName GroupName = NAME_None;
};

