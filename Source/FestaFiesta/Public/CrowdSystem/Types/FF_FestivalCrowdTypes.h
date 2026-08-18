// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NPC/Party/FF_NeedState.h"
#include "NPC/Party/FF_VisitHistory.h"
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

	bool bIsDependentChild = false;
	FFF_NeedState Needs;
	FName CommandType = NAME_None;

	// 소속 파티. 1인 방문객도 멤버 1명짜리 파티로 처리해 코드 경로를 하나로 통일한다.
	// 스폰되지 않은 빈 슬롯만 INDEX_NONE이다
	int32 PartyID = INDEX_NONE;

	//Leader가 받는 목적지 Index
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

/**
 * 파티가 새 목적지를 필요로 하게 된 이유.
 *
 * 재평가 진입점이 하나(도착)가 아니라 다섯이라는 사실을 타입으로 남기는 것이 목적이다.
 * 이유를 구분하지 못하면 나중에 "방금 거절당한 시설을 그대로 다시 고르는" 문제를 막을 수 없다.
 */
UENUM()
enum class EFF_DecisionReason : uint8
{
	Spawned,        // 스폰 직후 첫 목적지
	Arrived,        // 목적지에 도착했다
	JoinRejected,   // 도착했으나 시설 대기열이 만석·폐쇄로 거절했다   [미사용 — Queue 연동 후]
	MoveFailed,     // 경로 없음 / 막힘                              [미사용 — 경로탐색 도입 후]
	Interrupted,    // 시설 이동·폐쇄로 진행 중인 명령이 취소됐다      [미사용 — Zone 연동 후]
	StayExpired     // 체류 시간이 끝났다                            [미사용 — 체류 도입 후]
};

UENUM()
enum class EFF_PartyFallbackAction : uint8
{
	Wander,
	Relocate,
	Leave
};
USTRUCT()
struct FFF_PartyDecisionResult
{
	GENERATED_BODY()
	FVector Destination = FVector::ZeroVector;
	FName DestinationId = NAME_None;
	EFF_PartyFallbackAction FallbackAction = EFF_PartyFallbackAction::Wander;
	bool bCountAsNoChoice = false;
};
/**
 * 결정 계층에 넘기는 파티 스냅샷.
 *
 * 구조체로 묶는 이유는 조정자가 Crowd를 역참조하지 않게 하려는 것이다.
 * PartyId만 넘기면 조정자가 위치·개체ID·회차를 얻으려고 Crowd에 getter 3개를 요구하고,
 * 그러면 조정자가 Crowd 포인터를 들고 있어야 해서 의존 방향이 양쪽으로 열린다.
 *
 * AgentIndex를 넣지 않는다 — 슬롯은 재사용되므로 조정자가 프레임을 넘겨 들고 있으면
 * 그 인덱스가 다른 사람을 가리킨다.
 */
USTRUCT()
struct FFF_PartyDecisionContext
{
	GENERATED_BODY()

	// 재사용되지 않는 파티 식별자. 조정자가 상태를 기억할 때의 키다
	int32 PartyId = INDEX_NONE;

	// 리더의 개체 정체성. Choose의 난수 시드에 들어간다
	int32 StableAgentId = INDEX_NONE;

	// 결정 회차. 같은 파티가 매번 같은 곳을 고르지 않게 하는 값이다
	int32 ChoiceRoundIndex = 0;

	// 리더의 현재 월드 위치, cm. Zone 판정과 후보 거리 계산의 기준점이다
	FVector PartyWorldLocation = FVector::ZeroVector;

	// 왜 결정이 필요해졌는지
	EFF_DecisionReason Reason = EFF_DecisionReason::Spawned;
	FName ArchetypeId = NAME_None;
	int32 PartySize = 1;
	FFF_NeedState Needs;
	FFF_VisitHistory VisitHistory;
	int32 ConsecutiveNoChoiceCount = 0;
	float NowLiveMinutes = 0.f;
	float RemainingStayMinutes = 0.f;
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

USTRUCT()
struct FPartyDecisionState
{
	GENERATED_BODY();
	//추가 목적지의 결정이 필요한지를 판단한다.
	bool bNeedsDecision = false;
	//DecisionQueue에 이미 들어가 있는지
	bool bQueued = false;
	//마지막 결정 시간을 저장한다.
	float LastDecisionSimTime = 0.f;
	// 결정 회차 MakeDecisionSeed의 3번째 인자이다.
	int32 ChoiceRoundIndex = 0;
	
	EFF_DecisionReason PendingReason = EFF_DecisionReason::Spawned;
	FName CurrentDestinationId = NAME_None;
	int32 ConsecutiveNoChoiceCount = 0;
	bool bIsLeaving = false;
	float LeaveStartedSimTime = 0.f;
};

