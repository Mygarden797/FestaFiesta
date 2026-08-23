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

	/**
	 * 이번 스텝의 가속도, UU/s^2. (이름은 Force지만 이미 질량으로 나눈 값이다)
	 *
	 * 사회력 계산은 SI(N, m)로 하고, 질량으로 나눠 가속도로 만든 뒤 UU/s^2로 되돌려 여기 담는다.
	 * 왜 힘(N)을 그대로 담지 않는가: 그러면 StepAgent이 질량을 알아야 하고,
	 * 단위 변환 지점이 두 곳으로 늘어난다. 변환이 두 곳이 되는 순간 100배 오차가 숨을 자리가 생긴다.
	 *
	 * ★ 안에 무엇이 들어 있는지는 bAvoidanceDrivesVelocity에 달려 있다.
	 *   켜짐: 구동력 + 회피. StepAgent은 Lerp 없이 이 값만 적분한다
	 *   꺼짐: 회피분만. 목표 추종은 StepAgent의 지수 평활이 맡는다
	 *   판단은 CrowdSteering::FStepParams::bDrivingForceInForce로 전달된다.
	 *
	 * bEnableAvoidance가 false면 항상 0이고(그때는 Lerp 경로가 강제된다),
	 * StepAgent에서 0을 더하는 것은 결과를 바꾸지 않는다 —
	 * 그래서 회피를 껐을 때 기존 수치와 완전히 같다(회귀 판정의 근거).
	 */
	FVector Force = FVector::ZeroVector;

	/**
	 * 질량, kg. Helbing 힘(N)을 가속도로 되돌리는 데 쓴다.
	 *
	 * 전원 같은 값으로 시작한다. 개체 변주로 흩뿌리는 것은 나중 과제다 —
	 * 지금 넣으면 근거 없는 개인차가 밀도 수치에 섞인다.
	 */
	float MassKg = 70.f;

	/**
	 * 연속으로 제자리에 머문 고정 스텝 수. 임계를 넘으면 탈출 가속을 받는다.
	 *
	 * 회피를 붙이는 것과 같은 커밋에 넣는다. 나중에 붙이면
	 * "가끔 안 움직이는 NPC"를 재현하느라 하루를 쓴다.
	 */
	int32 StuckSteps = 0;

	float MaxSpeed = 150.f;

	float Radius = 34.f;

	/**
	 * 이 개체의 도착 판정 반경, UU. ★ 개체마다 다르다.
	 *
	 * ── 왜 전역 값이 아닌가 (실측 근거) ──
	 *
	 * 부스의 대기열 슬롯 간격은 90cm다(FFFZSQueueRules::SlotSpacingCm).
	 * 전역 150UU를 쓰면 도착 반경이 슬롯 간격보다 커서, 슬롯 5번으로 걸어가던 파티가
	 * 슬롯 3~4번 자리에서 이미 '도착'으로 판정되어 그 자리에 선다.
	 * 인접 슬롯 두세 개가 서로의 반경 안에 들어가므로 9.9m로 펴져야 할 줄이
	 * 1.5m 뭉치로 압축된다 — 부스 앞 덩어리의 직접 원인이다.
	 * 큐 자신은 25cm(SlotArrivalToleranceCm)로 판정하므로 두 시스템이 6배 다른 기준을 쓰고 있었다.
	 *
	 * 그래서 목적지 종류에 따라 값이 달라야 한다.
	 *   큐 슬롯·서비스 지점 : QueueSlotArrivalRadiusUU (슬롯 간격의 절반 미만)
	 *   그 밖(접근점·배회)  : ArrivalRadiusUU
	 *
	 * 갱신은 UFF_CrowdSimulationSubsystem::StepSimulation이 스텝마다 한 번 한다 —
	 * 큐 단계를 아는 것은 그쪽뿐이다. 여기 값을 직접 쓰지 말 것.
	 */
	float ArrivalRadiusUU = 150.f;
	
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
};

