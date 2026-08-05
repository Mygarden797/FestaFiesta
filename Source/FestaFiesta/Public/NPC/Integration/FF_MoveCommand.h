// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_MoveCommand.generated.h"

// 이동 의도. Crowd 가 같은 "걷기"라도 다르게 처리해야 하는 경우를 구분한다.
UENUM(BlueprintType)
enum class EFF_MoveIntent : uint8
{
	Visit     UMETA(DisplayName = "목적지 방문"),
	Wander    UMETA(DisplayName = "배회"),        // no_choice 가 선택됐을 때
	Exit      UMETA(DisplayName = "퇴장"),        // 체류시간 만료
	Evacuate  UMETA(DisplayName = "대피")         // Live Event. P0 범위 밖
};

/**
 * Decision(김시리) → Crowd(이정원) 명령.
 *
 * [중요] Party 단위로 보낸다.
 * 리더에게만 목적지를 주고 나머지 멤버는 포메이션 오프셋을 따른다
 * (멤버별로 개별 목적지를 주면 각자 최단 경로를 잡아서 일행이 갈라진다.)
 *
 * !!! 스텁 !!! 이 명령을 발행하는 쪽도 수신하는 쪽도 아직 없다. 현재 목적지 선택기는
 * FFF_DestinationDecision 을 반환만 하고 그것을 이동 명령으로 바꾸는 글루가 없다.
 */
USTRUCT(BlueprintType)
struct FFF_MoveCommand
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Move")
	int32 PartyId = INDEX_NONE;

	// FFF_DestinationCandidate::DestinationId 와 같은 키를 쓴다
	UPROPERTY(BlueprintReadOnly, Category = "Move")
	FName TargetDestinationId;

	/**
	 * 대기열 입구. **퇴장 지점과 반드시 분리한다** — 겹치면 나가는 사람과 들어오는 사람이
	 * 서로를 막아서 인위적인 데드락이 생긴다. 시설 데이터(한광훈) 쪽에 EntryPoint/ExitPoint 가
	 * 따로 있어야 한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Move")
	FName TargetEntryPointId;

	UPROPERTY(BlueprintReadOnly, Category = "Move")
	EFF_MoveIntent Intent = EFF_MoveIntent::Visit;

	// 결과 이벤트와 짝을 맞추는 식별자. 같은 CommandId 중복 요청은 거부 대상
	UPROPERTY(BlueprintReadOnly, Category = "Move")
	int32 CommandId = INDEX_NONE;
};

// 이동 결과. Crowd 가 목적지에 도달했거나 못 했을 때 결정 측에 알린다.
UENUM(BlueprintType)
enum class EFF_MoveResult : uint8
{
	Arrived      UMETA(DisplayName = "도착"),
	PathFailed   UMETA(DisplayName = "경로 없음"),
	Blocked      UMETA(DisplayName = "막힘"),
	Interrupted  UMETA(DisplayName = "중단")      // 시설 폐쇄·Event 등 외부 요인
};

/**
 * Crowd(이정원) → Decision(김시리) 이벤트.
 *
 * 병렬화를 위해 폴링하지 않고 이벤트로만 받는다.
 *
 * [주의] Arrived 는 "입구에 도달했다"일 뿐 "줄에 들어갔다"가 아니다. 
 * 걸어가는 동안 줄이 꽉 찰 수 있으므로 도착 판정과 가입 판정을 분리한다. 
 * 가입 실패(전원 거절)는 재평가 요청으로 이어진다.
 */
USTRUCT(BlueprintType)
struct FFF_MoveResultEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Move")
	int32 PartyId = INDEX_NONE;

	// 어느 명령에 대한 결과인지
	UPROPERTY(BlueprintReadOnly, Category = "Move")
	int32 CommandId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Move")
	EFF_MoveResult Result = EFF_MoveResult::Arrived;

	// 현실 분 기준 시뮬레이션 시각
	UPROPERTY(BlueprintReadOnly, Category = "Move")
	float SimTimeMinutes = 0.f;
};
