// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_QueueTypes.generated.h"

/**
 * 대기열 모듈 계약 타입 모음.
 *
 * 이 헤더는 Queue 가 소유하는 타입만 담는다. Crowd(FFF_CrowdReadModel)·
 * Decision(FFF_PartyDecisionContext) 쪽 타입은 여기에 두지 않는다.
 *
 * ID 는 PartyId / AgentId 모두 int32, CauseId 는 FName 이다.
 * 세 모듈이 같은 타입을 쓰므로 여기서 별도 래퍼를 정의하지 않는다.
 *
 * 대기시간은 이 모듈 안에서 전부 초로 다룬다. 결정 측은 분을 받으므로
 * 그대로 넘기면 60배 과대평가된다. 변환은 FFF_QueueWaitAdapter 를 거친다.
 */

/**
 * 단위 변환 상수는 여기 한 곳에만 둔다.
 *
 * Queue는 내부·로그·이벤트를 전부 "초"로 다루고, Decision 측
 * FFF_DestinationCandidate::EstimatedWaitMinutes 는 "분"이다. 초를 그대로 꽂으면
 * wait_ref_minutes=30 기준으로 60배 과대평가돼서 모든 시설이 no_choice로 밀린다.
 * 조용히 틀리는 종류의 버그라, 변환 지점을 흩뿌리지 않고 이 상수 하나로 모은다.
 */
namespace FFF_QueueUnits
{
	inline constexpr float SecondsPerDecisionMinute = 60.f;
}

namespace FFF_QueueTimings
{
	// 처리율이 0이거나 시설을 찾을 수 없을 때 돌려주는 "사실상 도달 불가" 대기시간(24시간).
	// TNumericLimits<float>::Max()를 쓰면 로그·CSV에 3.4e38이 찍히고 하류 정규화에서
	// inf/NaN이 새어나갈 여지가 생긴다. 유한한 큰 값이 훨씬 다루기 쉽다.
	inline constexpr float UnreachableWaitSeconds = 86400.f;
}

// 가입·서비스·이탈 판정 로그. 기본은 출력 안 함 — 콘솔에서
// `Log LogFestaFiestaQueue Verbose` 로 켠다 (Party 수가 많아 기본 노출 시 로그가 넘친다).
DECLARE_LOG_CATEGORY_EXTERN(LogFestaFiestaQueue, Log, All);

/**
 * TryJoin 결과. Party 단위 전부-아니면-전무이므로 부분 수락은 없다.
 * Accepted 이외의 값은 전부 Decision 측 재평가 트리거다.
 */
UENUM(BlueprintType)
enum class EFF_QueueJoinResult : uint8
{
	Accepted            UMETA(DisplayName = "수락"),
	RejectedClosed      UMETA(DisplayName = "거절-폐쇄"),
	RejectedFull        UMETA(DisplayName = "거절-만석"),
	RejectedAlreadyQueued UMETA(DisplayName = "거절-이미대기중"),
	RejectedUnknownFacility UMETA(DisplayName = "거절-미등록시설"),
	RejectedInvalidParty  UMETA(DisplayName = "거절-잘못된Party")
};

/** 대기열을 빠져나간 방식. */
UENUM(BlueprintType)
enum class EFF_QueueExit : uint8
{
	Served     UMETA(DisplayName = "서비스완료"),
	Abandoned  UMETA(DisplayName = "대기포기"),
	Cancelled  UMETA(DisplayName = "취소")
};

/** 시설이 닫힌 이유. 만족도 패널티 계산에 쓰인다. */
UENUM(BlueprintType)
enum class EFF_QueueCloseReason : uint8
{
	OperatingHoursEnded UMETA(DisplayName = "운영시간종료"),
	Emergency           UMETA(DisplayName = "긴급폐쇄"),
	Reconfigure         UMETA(DisplayName = "재구성")
};

/** Advance 중 쌓아뒀다가 상태 변경이 끝난 뒤에 한 번에 쏘는 내부 이벤트 레코드. */
UENUM()
enum class EFF_QueueEventType : uint8
{
	None,
	PartyJoined,
	PartyRejected,
	ServiceStarted,
	PartyExited
};

/**
 * 시설 한 곳의 대기열 설정. Queue가 소유하지 않는다 — 시설 데이터(한광훈)에서 넘어온다.
 * RegisterFacility 시점에 복사해서 보관한다.
 */
USTRUCT(BlueprintType)
struct FFF_QueueConfig
{
	GENERATED_BODY()

	// 이용 중인 팀을 포함한 총 팀 자리 수. 일행 인원수와 무관하게 한 팀이 한 칸을 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (ClampMin = "0", ClampMax = "512"))
	int32 MaxCapacityParties = 20;

	// 동시에 이용하는 팀 수. 각 팀은 자기 이용 시간을 독립적으로 진행한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (ClampMin = "1", ClampMax = "32"))
	int32 ConcurrentServiceSlots = 1;

	// 한 팀의 이용 시간(초). 1인 팀과 4인 팀에 같은 시간을 적용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	float ServiceSecondsPerParty = 20.f;

	// 같은 팀 자리 안에서 리더 양옆에 멤버를 배치하는 간격. 팀 자리 수에는 영향을 주지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (ClampMin = "0.0"))
	float PartyFormationSpacing = 90.f;

	// 서비스 종료 후 Crowd가 데려갈 퇴장 지점. 대기열 입구와 겹치면 데드락이 생긴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	FVector ExitLocation = FVector::ZeroVector;

	// true면 Crowd의 NotifyPartyArrived 통지를 받은 뒤에야 서비스 타이머가 돈다.
	// 기본값 false — Crowd 슬롯 도착 통지가 붙기 전까지는 순번만으로 서비스를 시작한다.
	// 통지가 연결되면 true 로 올려야 "걸어가는 중인데 서비스가 끝나 있는" 현상이 사라진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	bool bRequireArrivalConfirmation = false;
};

/** Solid world-space footprint used by the deterministic member-slot planner. */
USTRUCT(BlueprintType)
struct FFF_QueuePlacementObstacle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	FVector2D HalfExtentsCm = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	float YawDegrees = 0.f;
};

/**
 * 기본 대기열 형상은 직선 1열이다. 사행·다열은 나중에 확장한다.
 * 스플라인이 들어오면 이 구조체를 스플라인 평가로 교체한다 — 교체 지점은 ResolveSlot 하나뿐이다.
 */
USTRUCT(BlueprintType)
struct FFF_QueueLaneGeometry
{
	GENERATED_BODY()

	/** Production lanes reserve bounded, nonoverlapping member patterns before admitting parties. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	bool bConstrainMemberPlacement = false;

	/** Includes body radius and the final-position tolerance; two occupants need twice this clearance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (ClampMin = "1.0"))
	float MemberPlacementClearanceCm = 40.f;

	/** Fixed local envelope, never expanded to make an oversized or obstructed party fit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (ClampMin = "0.0", ClampMax = "270.0"))
	float MaxMemberLateralOffsetCm = 270.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	TArray<FFF_QueuePlacementObstacle> PlacementObstacles;

	/**
	 * Head-to-tail world-space linear polyline copied from the facility definition.
	 * Empty keeps the legacy HeadLocation/TailDirection lane. ExplicitSlotTransforms,
	 * when present for an index, remain authoritative over this derived geometry.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	TArray<FVector> PolylinePoints;

	/**
	 * Exact world-space waiting targets indexed by queue slot. This is the safe path
	 * for authored facility queues because bends and per-slot facing are preserved.
	 * Missing indices fall back to PolylinePoints and then the legacy straight lane.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	TArray<FTransform> ExplicitSlotTransforms;

	// 슬롯 0 = 첫 번째 팀 자리. 앞쪽 ConcurrentServiceSlots개가 이용 자리다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	FVector HeadLocation = FVector::ZeroVector;

	// 줄이 뒤로 늘어나는 방향. Configure에서 정규화한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	FVector TailDirection = FVector(1.f, 0.f, 0.f);

	// 연속한 팀 자리 사이의 간격.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	float SlotSpacingCm = 115.f;

	// 슬롯에서 바라볼 방향(창구 쪽). Crowd가 도착 후 회전에 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	float SlotYawDegrees = 0.f;
};

/**
 * Queue → Crowd. Queue가 하는 일은 이 배열을 갱신하고 알리는 것뿐이다.
 * Queue는 절대 트랜스폼을 직접 건드리지 않는다 — 순간이동도 Lerp도 아니고, Crowd가 걸어서 간다.
 * 그래야 앞사람 지연이 뒷사람에게 전파되고 실제 대기시간에 반영된다.
 *
 * Crowd의 SetPartyQueueColumn용으로 멤버마다 하나씩 나온다. 같은 팀의 모든 배정은
 * 하나의 SlotIndex를 공유하며, 저작된 팀 자리 안에서 멤버별 위치만 달라진다.
 */
USTRUCT(BlueprintType)
struct FFF_SlotAssignment
{
	GENERATED_BODY()

	// 이 멤버의 AgentId. PartyMemberIndex 0이 리더다.
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 AgentId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 PartyId = INDEX_NONE;

	// 팀 자리 번호. 같은 팀의 모든 멤버가 같은 번호를 사용한다.
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float TargetYaw = 0.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 PartyMemberIndex = 0;
};

/**
 * Decision → Queue. 가입 시점에 1회만 전달한다.
 * MaxWaitSeconds를 여기서 받으므로 Queue가 타이머를 들고 있다가 초과하면 이벤트를 쏜다 — 폴링 없음.
 */
USTRUCT(BlueprintType)
struct FFF_JoinRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Queue")
	int32 PartyId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Queue")
	int32 FacilityId = INDEX_NONE;

	// 슬롯 배정에 쓴다. MemberAgentIds가 비어 있으면 이 값만 보고 익명 슬롯을 만든다.
	UPROPERTY(BlueprintReadWrite, Category = "Queue", meta = (ClampMin = "1", ClampMax = "512"))
	int32 MemberCount = 1;

	// Crowd에 슬롯 목표를 내려보내려면 실제 AgentId가 필요하다. 비면 SlotAssignment의 AgentId가 INDEX_NONE이 된다.
	UPROPERTY(BlueprintReadWrite, Category = "Queue")
	TArray<int32> MemberAgentIds;

	// Need 압력에서 Decision이 계산한 개인별 인내심(초). 음수면 무제한 대기.
	// 화장실이 급한 Party는 오래 기다리고 심심해서 온 Party는 금방 포기한다.
	UPROPERTY(BlueprintReadWrite, Category = "Queue")
	float MaxWaitSeconds = 600.f;

	// DecisionLog 와 QueueLog 를 잇는 조인 키다.
	UPROPERTY(BlueprintReadWrite, Category = "Queue")
	FName CauseId;
};

/**
 * Queue → Decision(스냅샷 경유). FFF_QueueState.
 *
 * WaitingPartyCount는 이용 중인 팀을 포함한 전체 팀 수다. 용량도 팀 수로 판정한다.
 * 실제 인원은 통계와 정산용으로 별도 유지한다. 예상 대기는 현재 팀별 진행 시간을 반영한다.
 */
USTRUCT(BlueprintType)
struct FFF_QueueState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 FacilityId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 WaitingPartyCount = 0;

	// 이용 중인 팀도 포함한 총 인원. 밀도·통계 전용이며 팀 용량 판정에는 쓰지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 OccupiedMemberCount = 0;

	// 아직 이용을 완료하지 않은 인원. 통계 전용이다.
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 RemainingServiceMemberCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 MaxCapacityParties = 0;

	// 아직 이용을 완료하지 않은 팀 수(이용 중 포함).
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 RemainingServicePartyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 ActiveServicePartyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 CompletedPartyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float AverageCompletedWaitSeconds = 0.f;

	// 지금 한 팀이 추가되면 이용을 시작할 때까지의 시간. 현재 이용 팀의 경과 시간을 반영한다.
	// 도착까지 걸을 시간과 미래 포기는 예측하지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float NextServiceStartWaitSeconds = 0.f;

	// 초당 처리 팀 수 = Min(ConcurrentServiceSlots, MaxCapacityParties) / ServiceSecondsPerParty
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float ServiceRate = 0.f;

	// 운영 중이고 한 팀이 들어갈 자리가 남았는지. 만석·폐쇄면 false.
	//
	// ⚠ 목적지 후보를 거르는 게이트로 쓰지 말 것.
	//   이 값은 "지금 이 순간 TryJoin 이 성공하는가"라서 폐쇄와 '잠깐 만석'이 뭉쳐 있다.
	//   폐쇄는 몇 분~영원이지만 만석은 앞 Party 하나 빠지면 풀린다. 후보에서 빼버리면
	//   붐비는 부스가 결정 시점마다 사라졌다 나타났다 한다.
	//   후보 게이트는 bIsOpen 을, 혼잡도는 예상 대기시간을 쓸 것.
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	bool bAcceptingNew = true;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	bool bIsOpen = true;

	// ── 단위를 이름에 박은 접근자 ───────────────────────────────────────────
	// ServiceRate 라는 이름만 보고 단위를 짐작하면 안 된다. 프로젝트에는 분당 처리율을
	// 돌려주는 동명 함수가 따로 있어서(시설 정적 규칙 기반), 둘을 섞으면 60배가 조용히 들어간다.
	float GetServiceRatePerSecond() const { return ServiceRate; }
	float GetServiceRatePerMinute() const { return ServiceRate * FFF_QueueUnits::SecondsPerDecisionMinute; }
};

/**
 * 예상 대기와 실제 대기의 괴리가 Utility AI 튜닝의 직접 지표다 —
 * 예측이 계속 낮게 나오면 ExpectedWait 계산이나 가중치를 손봐야 한다는 뜻이고,
 * 이건 로그 없이는 절대 못 잡는다.
 */
USTRUCT(BlueprintType)
struct FFF_QueueLog
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 FacilityId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 PartyId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 MemberCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float JoinSimTime = 0.f;

	// 가입 시점에 Queue가 알려준 예측치
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float EstimatedWaitAtJoinSeconds = 0.f;

	// 실제로 기다린 시간 = 서비스 시작 - 가입. 서비스를 못 받고 나갔으면 퇴장 - 가입.
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float ActualWaitSeconds = 0.f;

	// 서비스에 실제로 쓴 시간. Served가 아니면 0.
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float ServiceSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float ExitSimTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	EFF_QueueExit ExitType = EFF_QueueExit::Served;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	FName CauseId;

	// ── 분 단위 뷰 ──────────────────────────────────────────────────────────
	// 시간 필드는 전부 초다(CSV 컬럼명도 _s 로 끝난다). 결정 측은 분을 쓰므로
	// (FFF_CrowdReadModel::SimTimeMinutes, FFF_DestinationCandidate::MinutesSinceLastVisit)
	// 두 로그를 시각으로 조인할 때 호출부가 매번 60으로 나누게 두면 언젠가 한 군데가 빠진다.
	// 저장은 초 하나만 한다 — 분을 필드로 중복 저장하면 둘이 어긋날 수 있다.
	float GetJoinSimTimeMinutes() const { return JoinSimTime / FFF_QueueUnits::SecondsPerDecisionMinute; }
	float GetExitSimTimeMinutes() const { return ExitSimTime / FFF_QueueUnits::SecondsPerDecisionMinute; }
	float GetEstimatedWaitAtJoinMinutes() const { return EstimatedWaitAtJoinSeconds / FFF_QueueUnits::SecondsPerDecisionMinute; }
	float GetActualWaitMinutes() const { return ActualWaitSeconds / FFF_QueueUnits::SecondsPerDecisionMinute; }
	float GetServiceMinutes() const { return ServiceSeconds / FFF_QueueUnits::SecondsPerDecisionMinute; }
};

/**
 * Advance 도중 상태를 바꾸면서 바로 브로드캐스트하면, 수신 측(Decision)이 재평가 후
 * 같은 프레임에 TryJoin을 다시 불러서 재진입이 생긴다. 그래서 이벤트는 여기에 쌓아뒀다가
 * 상태 변경이 전부 끝난 뒤에 subsystem이 한 번에 꺼내 쏜다.
 */
USTRUCT()
struct FFF_QueueEvent
{
	GENERATED_BODY()

	UPROPERTY()
	EFF_QueueEventType Type = EFF_QueueEventType::None;

	UPROPERTY()
	int32 FacilityId = INDEX_NONE;

	UPROPERTY()
	int32 PartyId = INDEX_NONE;

	// PartyJoined일 때만 유효
	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	// PartyJoined = 예상 대기(초), ServiceStarted/PartyExited = 실제 대기(초)
	UPROPERTY()
	float WaitSeconds = 0.f;

	// PartyRejected일 때만 유효
	UPROPERTY()
	EFF_QueueJoinResult RejectReason = EFF_QueueJoinResult::Accepted;

	// PartyExited일 때만 유효
	UPROPERTY()
	EFF_QueueExit ExitType = EFF_QueueExit::Served;

	// PartyExited + Served일 때 확정된 실제 이용 인원. 취소·포기는 0이며 정산하지 않는다.
	UPROPERTY()
	int32 ServedMemberCount = 0;
};
