// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_QueueTypes.generated.h"

/**
 * 대기열 모듈 계약 타입 모음 (설계문서 3·8·9·10장).
 *
 * 이 헤더는 "Queue가 소유한다"고 합의된 타입만 담는다. Crowd(FFF_CrowdReadModel)·
 * Decision(FFF_MoveCommand) 쪽 타입은 여기에 두지 않는다.
 *
 * ─────────────────────────────────────────────────────────────────────────
 * TODO(3자 합의): 공용 ID 타입
 *   설계문서는 FFF_PartyId / FFF_AgentId / FFF_CauseId 를 전제하지만 아직
 *   공용 헤더가 없다. 세 모듈이 전부 쓰는 타입이라 Queue가 임의로 정의하면
 *   통합 시 재정의 충돌이 난다. 확정 전까지 PartyId/AgentId 는 int32,
 *   CauseId 는 FName 으로 둔다. 공용 헤더가 커밋되면 이 파일의 시그니처만
 *   치환하면 된다.
 *
 * TODO(3자 합의): 시간 단위
 *   이 모듈은 대기시간을 전부 "초"로 다룬다. FFF_DestinationCandidate 쪽은
 *   EstimatedWaitMinutes(분)라서 그대로 꽂으면 60배 과대평가된다.
 *   변환은 FFF_FacilityQueue::GetExpectedWaitMinutes() 어댑터를 쓸 것.
 * ─────────────────────────────────────────────────────────────────────────
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
 * TryJoin 결과. Party 단위 전부-아니면-전무이므로 부분 수락은 없다 (설계문서 8.2①).
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

/** 대기열을 빠져나간 방식 (설계문서 10.2 EFF_QueueExit). */
UENUM(BlueprintType)
enum class EFF_QueueExit : uint8
{
	Served     UMETA(DisplayName = "서비스완료"),
	Abandoned  UMETA(DisplayName = "대기포기"),
	Cancelled  UMETA(DisplayName = "취소")
};

/** 시설이 닫힌 이유. 만족도 패널티 계산에 쓰인다 (설계문서 9.3). */
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

	// 슬롯 총량. Party 수가 아니라 "인원" 기준이다 — 4인 가족이 2칸 남은 줄에 오면 전원 거절된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	int32 MaxCapacityMembers = 20;

	// 한 사이클에 동시에 처리하는 인원. P0은 1 (설계문서 12.1).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	int32 ConcurrentServiceSlots = 1;

	// 1인 서비스 소요 시간(초). 4인 가족 · 동시서비스 1명이면 이 값의 4배가 걸린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	float ServiceSecondsPerMember = 20.f;

	// 서비스 종료 후 Crowd가 데려갈 퇴장 지점. 대기열 입구와 겹치면 데드락이 생긴다 (설계문서 8.2③).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	FVector ExitLocation = FVector::ZeroVector;

	// true면 Crowd의 NotifyPartyArrived 통지를 받은 뒤에야 서비스 타이머가 돈다.
	// P0 기본값 false — Crowd 슬롯 도착 콜백이 아직 합의 전이라 순번만으로 서비스를 시작한다.
	// 합의되면 true로 올려야 "걸어가는 중인데 서비스가 끝나 있는" 현상이 사라진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	bool bRequireArrivalConfirmation = false;
};

/**
 * P0 대기열 형상: 직선 1열 (설계문서 12.1). 사행·다열은 P1.
 * 스플라인이 들어오면 이 구조체를 스플라인 평가로 교체한다 — 교체 지점은 ResolveSlot 하나뿐이다.
 */
USTRUCT(BlueprintType)
struct FFF_QueueLaneGeometry
{
	GENERATED_BODY()

	// 슬롯 0 = 창구 바로 앞 위치
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	FVector HeadLocation = FVector::ZeroVector;

	// 줄이 뒤로 늘어나는 방향. Configure에서 정규화한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	FVector TailDirection = FVector(1.f, 0.f, 0.f);

	// 설계문서 8.4 기준값. 1열 115cm면 약 0.87인/㎡ — 그 자체는 위험 밀도가 아니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	float SlotSpacingCm = 115.f;

	// 슬롯에서 바라볼 방향(창구 쪽). Crowd가 도착 후 회전에 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue")
	float SlotYawDegrees = 0.f;
};

/**
 * Queue → Crowd. Queue가 하는 일은 이 배열을 갱신하고 알리는 것뿐이다 (설계문서 8.2②).
 * Queue는 절대 트랜스폼을 직접 건드리지 않는다 — 순간이동도 Lerp도 아니고, Crowd가 걸어서 간다.
 * 그래야 앞사람 지연이 뒷사람에게 전파되고 실제 대기시간에 반영된다.
 */
USTRUCT(BlueprintType)
struct FFF_SlotAssignment
{
	GENERATED_BODY()

	// TODO(3자 합의): FFF_AgentId 확정 시 치환
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 AgentId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 PartyId = INDEX_NONE;

	// 0 = 창구 바로 앞
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float TargetYaw = 0.f;
};

/**
 * Decision → Queue. 가입 시점에 1회만 전달한다 (설계문서 9.2).
 * MaxWaitSeconds를 여기서 받으므로 Queue가 타이머를 들고 있다가 초과하면 이벤트를 쏜다 — 폴링 없음.
 */
USTRUCT(BlueprintType)
struct FFF_JoinRequest
{
	GENERATED_BODY()

	// TODO(3자 합의): FFF_PartyId 확정 시 치환
	UPROPERTY(BlueprintReadWrite, Category = "Queue")
	int32 PartyId = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "Queue")
	int32 FacilityId = INDEX_NONE;

	// 슬롯 배정에 쓴다. MemberAgentIds가 비어 있으면 이 값만 보고 익명 슬롯을 만든다.
	UPROPERTY(BlueprintReadWrite, Category = "Queue")
	int32 MemberCount = 1;

	// Crowd에 슬롯 목표를 내려보내려면 실제 AgentId가 필요하다. 비면 SlotAssignment의 AgentId가 INDEX_NONE이 된다.
	UPROPERTY(BlueprintReadWrite, Category = "Queue")
	TArray<int32> MemberAgentIds;

	// Need 압력에서 Decision이 계산한 개인별 인내심(초). 음수면 무제한 대기.
	// 화장실이 급한 Party는 오래 기다리고 심심해서 온 Party는 금방 포기한다 (설계문서 9.2).
	UPROPERTY(BlueprintReadWrite, Category = "Queue")
	float MaxWaitSeconds = 600.f;

	// TODO(3자 합의): FFF_CauseId 확정 시 치환. DecisionLog와 QueueLog를 잇는 조인 키다.
	UPROPERTY(BlueprintReadWrite, Category = "Queue")
	FName CauseId;
};

/**
 * Queue → Decision(스냅샷 경유). 설계문서 9.1의 FFF_QueueState.
 *
 * 문서의 CurrentLength는 "인원"인지 "Party 수"인지 모호해서 셋으로 쪼갰다.
 * ExpectedWait 분자는 RemainingServiceMemberCount, 용량 판정은 OccupiedMemberCount다.
 * 하나로 합치면 4인 가족이 들어오는 순간 반드시 틀린다.  ← 13장 합의 안건
 */
USTRUCT(BlueprintType)
struct FFF_QueueState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 FacilityId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 WaitingPartyCount = 0;

	// 슬롯을 점유 중인 총 인원. 밀도 집계·용량 판정이 봐야 하는 값 (설계문서 8.4).
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 OccupiedMemberCount = 0;

	// 아직 서비스를 못 받은 인원. ExpectedWait 계산의 분자.
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 RemainingServiceMemberCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	int32 MaxCapacityMembers = 0;

	// 초당 처리 인원 = ConcurrentServiceSlots / ServiceSecondsPerMember
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	float ServiceRate = 0.f;

	// 운영 중이고 최소 1인 이상 들어갈 자리가 남았는지. 만석·폐쇄면 false.
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	bool bAcceptingNew = true;

	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	bool bIsOpen = true;
};

/**
 * 설계문서 10.2. 예상 대기와 실제 대기의 괴리가 Utility AI 튜닝의 직접 지표다 —
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

	// TODO(3자 합의): FFF_CauseId 확정 시 치환
	UPROPERTY(BlueprintReadOnly, Category = "Queue")
	FName CauseId;
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
};
