// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NPC/Party/FF_NeedState.h"
#include "NPC/Party/FF_ProgramReservation.h"
#include "NPC/Party/FF_VisitHistory.h"
#include "FF_FestivalCrowdTypes.generated.h"



enum class EUnitAnimationState : uint8
{
	Idle,
	Walk,
	Run
};

UENUM()
enum class EFF_CrowdRole : uint8
{
	Visitor,
	Staff,
	Undefined
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

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Data")
	FVector SteerTargetLocation = FVector::ZeroVector;

	bool bHasSteerTarget = false;

	FVector GetSteerTarget() const { return bHasSteerTarget ? SteerTargetLocation : TargetLocation; }

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

	// 사건 처리 실패 같은 외부 요인으로 이 방문객의 만족도에 붙은 감점(0~100점).
	// 시간이 지나면 SatisfactionPenaltyRecoverPerMinute 만큼씩 줄어든다.
	float SatisfactionPenalty = 0.f;
	FName CommandType = NAME_None;

	// 소속 파티. 1인 방문객도 멤버 1명짜리 파티로 처리해 코드 경로를 하나로 통일한다.
	// 스폰되지 않은 빈 슬롯만 INDEX_NONE이다
	int32 PartyID = INDEX_NONE;

	//Leader가 받는 목적지 Index
	int32 LeaderAgentIndex = INDEX_NONE;

	// 파티 내 자리 번호. 0이 리더. 대형(formation) 오프셋을 이 번호로 계산한다
	int32 PartySlotIndex = 0;
	
	FVector QueueSlotLocation = FVector::ZeroVector;
	bool bHasQueueSlot = false;
	// 실제로 배정된 대기열 자리·관람석은 넓은 접근 반경이 아니라 자리 중심까지 이동한다.
	bool bUsePreciseSlotPosition = false;
	float LaneYaw = 0.f;

	// 재현성을 위한 고정 ID. 슬롯 인덱스는 재사용되므로 그것과 별개로 둔다
	int32 StableAgentID = INDEX_NONE;
	
	//---회피 관련 변수들 ---//

	//이번 스텝의 가속도
	FVector Force = FVector::ZeroVector;

	/**
	 * 질량, kg. Helbing 힘(N)을 가속도로 되돌리는 데 쓴다.
	 *
	 * 전원 같은 값으로 시작한다. 개체 변주로 흩뿌리는 것은 나중 과제다 —
	 * 지금 넣으면 근거 없는 개인차가 밀도 수치에 섞인다.
	 */
	float MassKg = 70.f;

	//연속으로 머문 횟수. 일정 이상이면 탈출  속도를 발동시킨다.
	int32 StuckSteps = 0;

	int32 UnstuckHoldSteps = 0;

	int32 UnstuckCooldownSteps = 0;

	int32 UnstuckAttemptCount = 0;

	FVector FacingVelocity = FVector::ZeroVector;

	bool bFacingFollowsVelocity = false;

	float MaxSpeed = 150.f;

	float Radius = 34.f;

	//개체의 도착 판정
	float ArrivalRadiusUU = 150.f;

	// 추격 중이 아닐 때 내는 속도, UU/s. Speed가 MaxSpeed로 덮이므로 역산할 수 없다
	float CruiseSpeed = 150.f;

	// 스폰 시점의 MaxSpeed. 공연 집결처럼 일시적으로 속도를 올린 뒤 되돌릴 기준값이다.
	// Velocity 가 MaxSpeed 로 잘리므로 Speed 만 올리면 실제로는 안 빨라진다.
	float CruiseMaxSpeed = 150.f;

	// 멤버가 리더를 따라붙는 중인가. 켜는 임계와 푸는 임계가 다르다(히스테리시스)
	bool bCatchingUpToLeader = false;

	bool bFormationPathBlocked = false;
	int32 FormationClearStreak = 0;

	//Animation
	
	
	
	//이 개체가 들어온 게이트. ValidGates 인덱스이다.
	int32 EntryGateIndex = INDEX_NONE;
	
	EFF_CrowdRole CrowdRole = EFF_CrowdRole::Undefined;
	
	
	
	int32 MissingCount = 0;
	
	bool bMissing = false;
	
	
	
	
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
	JoinRejected,   // 도착했으나 시설 대기열이 만석·폐쇄로 거절했다   (FF_QueueCrowdLink)
	/**
	 * 목적지 방향으로 DecisionStuckSteps 동안 못 나아갔다 = 이 목적지를 포기한다.
	 *
	 * 유일하게 **도착 전에** 발화하는 사유다. 나머지는 전부 도착이 선행 조건이라, 이 사유가
	 * 없으면 걷는 도중 생긴 병목에 대해 파티가 할 수 있는 일이 "계속 밀어붙이기" 하나뿐이다.
	 *
	 * ★ 조정자에게: 이 사유로 온 요청은 직전 목적지를 감점하거나 후보에서 빼야 한다.
	 * 안 하면 못 갔던 곳을 그대로 다시 골라 같은 잼으로 걸어 들어간다 — 이 enum 을 사유별로
	 * 나눈 이유가 바로 그것이다(위 주석). 경로탐색이 붙으면 "경로 없음"도 이 사유를 쓴다.
	 */
	MoveFailed,
	Interrupted,    // 시설 이동·폐쇄로 진행 중인 명령이 취소됐다      (FF_QueueCrowdLink)
	StayExpired,    // 체류 시간이 끝났다                            [미사용 — 체류 도입 후]
	/**
	 * 닫혀 있던 목적지가 열렸다 = 지금 고를 수 있는 것이 늘었다.
	 *
	 * 나머지 사유와 달리 월드 쪽에서 생긴 일이라, 도착을 기다리지 않고
	 * 밖에서 밀어 넣는다(`RequestRedecisionForIdleParties`). 지금 유일한 발화점은
	 * 메인 공연 시각의 무대 개장이다(UFF_CrowdDecisionSubsystem).
	 *
	 * 이 사유가 없으면 무대가 열려도 아무도 통지받지 못해, 각자 하던 일을 끝낼 때까지
	 * 회전율만큼 서서히 차기만 하고 "공연 시간에 몰린다"가 안 나온다.
	 */
	DestinationOpened,

	// 개인 재평가 주기(reevaluate_interval_seconds × reevaluate_interval_multiplier)가 찼다.
	// 도착 전에 발화하는 두 번째 사유다. MoveFailed 와 달리 실패가 아니다.
	// 그래서 직전 목적지를 감점하지 않는다.
	Reevaluated,

	// 줄에서 개인 인내심 한계(wait_threshold_minutes × patience_multiplier)를 넘겨 이탈했다.
	// 직전 목적지를 감점해야 하는 사유다.
	// Warning을 남기는 QueueHoldTimeoutSeconds 강제 해제와 달리, 정상 행동이다.
	QueuePatienceExpired
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

	//시설이 해소하는 Need 성분
	FFF_NeedReliefFlags NeedRelief;
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

	/**
	 * 리더의 개체 변주 중 결정에 쓰이는 값들. Crowd 가 스폰 때 확정해 둔 것을 실어 보낸다
	 * (FFF_IndividualVariationResolver::ResolveWithGroupSharing).
	 * 기본값은 전부 "변주 없음"이라, 안 채워 보내면 예전과 동일하게 동작한다.
	 */
	float NoveltySeekingMultiplier = 1.f;
	float PreferenceStrengthMultiplier = 1.f;

	// density_threshold_ratio 에 이미 crowd_tolerance_multiplier 가 곱해진 값이다.
	// 조정자는 이것을 그대로 임계로 쓴다 — 다시 곱하면 변주가 두 번 걸린다.
	float PersonalDensityThresholdRatio = 1.f;

	// 왜 결정이 필요해졌는지
	EFF_DecisionReason Reason = EFF_DecisionReason::Spawned;

	/**
	 * 직전 라운드에 이 파티가 들고 있던 목적지. 배회 중이었으면 NAME_None 이다.
	 *
	 * ### 이 값만으로 판단하지 말 것 — Reason 과 함께 읽는다
	 * "직전 목적지"는 그 자체로 나쁜 후보가 아니다. 이용을 마치고 나온 시설도 여기 담기는데,
	 * 그 경우 다시 고르는 것은 정상이고 억제는 revisit 항이 이미 맡고 있다.
	 *
	 * 빼거나 감점해야 하는 것은 **실패한 사유로 온 요청**뿐이다:
	 *   MoveFailed   — 그 방향이 막혀서 포기했다. 다시 고르면 같은 잼으로 걸어 들어간다
	 *   JoinRejected — 만석·폐쇄로 거절당했다. 다시 고르면 같은 거절을 받는다
	 *
	 * 이 구분을 안 하면 사유별로 enum 을 나눈 의미가 없어진다(EFF_DecisionReason 주석).
	 */
	FName PreviousDestinationId = NAME_None;
	FName ArchetypeId = NAME_None;
	int32 PartySize = 1;
	FFF_NeedState Needs;
	FFF_VisitHistory VisitHistory;
	int32 ConsecutiveNoChoiceCount = 0;
	float NowLiveMinutes = 0.f;

	float WalkSpeedMetersPerSecond = 1.5f;
	// Simulation의 복귀 여유(축제분). 후보 방문 후 귀환 가능 판정에 전달한다.
	float ProgramReturnBufferMinutes = 0.f;
	float RemainingStayMinutes = 0.f;
	bool bHasActiveReservation = false;
	FFF_ProgramReservation ActiveReservation;

	/**
	 * 목적지별 "지금 그곳으로 걸어가고 있는 인원". 널일 수 있다 — 조정자는 널이면 이 항을 끈다.
	 *
	 * ### 왜 이 값이 필요한가 (시설 몰림의 직접 원인)
	 * 후보의 혼잡·대기 값은 전부 **결정 시점의 스냅샷**이다. 같은 프레임대에 결정하는 파티들은
	 * 모두 같은 "비어 있는" 인기 시설을 보고 동시에 출발하고, 도착해서야 만석을 확인한다
	 * (가입 거절 → 재평가 → 또 같은 곳). 이미 그곳으로 향하고 있는 수요가 후보 점수에
	 * 들어가지 않으면 이 몰림은 계수를 어떻게 만져도 사라지지 않는다.
	 *
	 * ### 왜 포인터로 넘기는가
	 * 목적지를 들고 있는 파티를 아는 것은 Crowd 뿐이다. 그렇다고 조정자가 Crowd 를 역참조하게
	 * 두면 이 구조체를 만든 이유(FFF_PartyDecisionContext 주석)가 무너진다. 그래서 Crowd 가
	 * 결정 배치 앞에서 한 번 짓고 그 스냅샷을 읽기 전용으로 빌려준다.
	 *
	 * 수명은 이 호출 한 번이다 — 조정자가 프레임을 넘겨 들고 있으면 안 된다.
	 * 줄에 들어간 파티는 여기서 빠진다(대기시간이 이미 세고 있어 이중 계산이 된다).
	 */
	const TMap<FName, int32>* InTransitMemberCounts = nullptr;
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

/**
 * 파티가 대기열의 어느 단계에 있는가.
 *
 * 이 단계가 필요한 이유는 하나다 — 줄에 서 있는 파티는 도착 판정이 매 스텝 참인데,
 * 그때마다 재평가를 걸면 줄을 서자마자 다른 곳으로 걸어가 버린다.
 * Queue가 파티를 소유하는 구간을 Crowd가 알아야 재평가를 멈출 수 있다.
 */
UENUM()
enum class EFF_PartyQueuePhase : uint8
{
	// 큐와 무관하다. 도착하면 평소대로 재평가한다
	None,
	// 가입이 수락됐고 배정받은 슬롯으로 걸어가는 중
	Joined,
	// 슬롯에 도착해 서비스를 기다리는 중. 줄이 줄면 새 슬롯 좌표를 받아 앞으로 옮겨간다
	AtSlot,
};



// 일반 시설 Queue와 독립적인 예약 프로그램 이동 상태.
UENUM()
enum class EFF_PartyProgramPhase : uint8
{
	None,
	Returning,		// 복귀 중
	Waiting,		// 대기 중 
	Participating   // 참여 중
};

// 일반 Queue 및 유료 프로그램과 독립적인 공연 관람 상태.
// Audience 자리는 사람 단위지만 행동 잠금은 파티 리더가 소유한다.
UENUM()
enum class EFF_PartyAudiencePhase : uint8
{
	None,
	GoingToEntry,
	GoingToSlot,
	Waiting,
	Spectating,
	Exiting
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

	// 포기·완료 직전에 들고 있던 목적지. RequestDecision 이 CurrentDestinationId 를 비울 때
	// 그 값을 여기로 옮긴다 — 안 옮기면 조정자가 "무엇을 못 갔는지"를 알 방법이 없다.
	FName PreviousDestinationId = NAME_None;

	// CurrentDestinationId 와 한 쌍이다. 목적지가 NAME_None 이면 이 값도 의미가 없다.
	// 둘을 항상 같이 세우고 같이 지울 것 — 갈리면 "A 시설에 갔는데 B 의 Need 가 풀리는" 상태가 된다.
	FFF_NeedReliefFlags CurrentNeedRelief;

	int32 ConsecutiveNoChoiceCount = 0;
	bool bIsLeaving = false;
	float LeaveStartedSimTime = 0.f;

	// --- 대기열 -------------------------------------------------------------------------
	EFF_PartyQueuePhase QueuePhase = EFF_PartyQueuePhase::None;

	// 가입한 시설의 Zone GUID. QueuePhase가 None이면 무효다.
	// int32 QueueFacilityId가 아니라 GUID를 들고 있는 이유: 결정 계층이 만들어내는 것이
	// GUID이고, int32 대응은 브리지가 실행 중에 바꿀 수 있는 내부 배정이기 때문이다.
	FGuid QueuedFacilityId;

	// 줄에 들어간 시각(SimTimeSeconds). 탈출 타이머의 기준이다.
	//
	// 타이머가 필요한 이유: 줄을 벗어나는 정상 경로는 전부 Queue의 이벤트(거절·포기·서비스
	// 완료·폐쇄 취소)인데, 그 이벤트가 하나라도 유실되면 파티가 영원히 재평가되지 않는다.
	// 그러면 화면에는 "이유 없이 굳은 NPC 무리"로만 보이고 로그에는 아무것도 안 남는다.
	float QueueEnteredSimTime = 0.f;

	// --- 예약 프로그램 -----------------------------------------------------------------
	EFF_PartyProgramPhase ProgramPhase = EFF_PartyProgramPhase::None;
	FGuid ProgramFacilityId;
	float ProgramStartMinutes = 0.f;
	float ProgramEndMinutes = 0.f;
	FVector ProgramWaitingLocation = FVector::ZeroVector;

	// --- 공연 관람 -----------------------------------------------------------------------
	EFF_PartyAudiencePhase AudiencePhase = EFF_PartyAudiencePhase::None;
	FGuid AudienceFacilityId;
	int32 AudienceFirstSlotIndex = INDEX_NONE;
	int32 AudienceSlotCount = 0;
	FVector AudienceEntryLocation = FVector::ZeroVector;
	FVector AudienceExitLocation = FVector::ZeroVector;
	float AudienceArrivalTolerance = 0.f;
};

// 입장, 퇴장 게이트 하나. 저작 데이터만 담고, 배열 순서가 곧 게이트 인덱스라 저작 순서가 결정성의 계약이다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_CrowdGate
{
	GENERATED_BODY()

	// 게이트 별 집계 정산 용도 ID. 인덱스로 대신하면 게이트를 하나 지울 때 과거 지표가 전부 밀린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Gate")
	FName GateId;

	// 레벨에 놓은 앵커. 없을 경우 GateLocation을 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Gate")
	TObjectPtr<AActor> GateAnchor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Gate")
	FVector GateLocation = FVector::ZeroVector;

	// 문의 크기. 스폰 산개 반경이자 퇴장 판정 반경이며, 여유를 얹으면 문 앞 병목이 지표에서 지워진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Gate", meta = (ClampMin = "0.0"))
	float GateRadiusUU = 400.f;

	// Spawn과 Exit를 허용하는지의 여부이다. 양방향이 기본이고 전용 게이트가 예외다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Gate")
	bool bAllowSpawn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Gate")
	bool bAllowExit = true;

	// 각 Gate의 Spawn 비율을 정산하기 위해 사용하는 장치. 0 이하면 검증에서 제외된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crowd|Gate", meta = (ClampMin = "0.0"))
	float GateWeight = 1.f;
	

};



