// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Queue/FF_QueueTypes.h"

struct FFF_DestinationCandidate;
class UFF_QueueSubsystem;

/**
 * Queue(초) ↔ Decision(분) 부패 방지 계층.
 *
 * 왜 별도 파일인가:
 *   Queue 코어(FFF_FacilityQueue)는 Decision 타입을 몰라야 하고, Decision
 *   (FFF_UtilityDestinationSelector)은 Queue 타입을 몰라야 한다. 둘 다 아는 코드가
 *   필요한데, 그걸 어느 한쪽에 넣으면 그쪽이 오염된다. 그래서 양쪽을 아는 유일한
 *   지점을 이 파일 하나로 격리한다. 단위 변환·필드 매핑 규칙이 바뀌면 여기만 고친다.
 *
 * 이 클래스는 FFF_DestinationCandidate 를 읽고 쓰지만, 그 정의가 있는
 * NPC/FF_UtilityDestinationSelector.h 는 수정하지 않는다 (읽기 전용 의존).
 *
 * 상태를 갖지 않는 순수 static 함수만 제공한다 — FFF_UtilityDestinationSelector 와 같은 형태다.
 */
class FFF_QueueWaitAdapter
{
public:
	// ── 단위 변환 ───────────────────────────────────────────────────────────
	// 상한·하한을 씌운다. 음수 대기는 의미가 없고, 도달 불가 값이 그대로 흘러가면
	// 로그·CSV에 천문학적 숫자가 찍힌다.
	static float ToDecisionMinutes(float QueueSeconds)
	{
		const float Clamped = FMath::Clamp(QueueSeconds, 0.f, FFF_QueueTimings::UnreachableWaitSeconds);
		return Clamped / FFF_QueueUnits::SecondsPerDecisionMinute;
	}

	static float ToQueueSeconds(float DecisionMinutes)
	{
		return DecisionMinutes * FFF_QueueUnits::SecondsPerDecisionMinute;
	}

	/**
	 * 설계문서 9.1 식을 스냅샷만으로 재현한다. Queue 인스턴스 없이도 계산할 수 있어야
	 * 다른 스레드에서 스냅샷을 읽는 경로가 열린다 (설계문서 원칙 3).
	 * ServiceRate 가 0이면 영원히 안 빠지는 줄이므로 큰 값을 돌려준다.
	 */
	static float ComputeExpectedWaitSeconds(const FFF_QueueState& State, int32 IncomingMemberCount);

	// ── 후보 채우기 ─────────────────────────────────────────────────────────
	/**
	 * 후보 하나에 대기열 상태를 반영한다.
	 *
	 *   EstimatedWaitMinutes  ← 예상 대기(초)를 분으로 변환해서 대입
	 *   bIsOpen               ← 기존 값과 AND. 덮어쓰지 않는다.
	 *
	 * bIsOpen 을 AND 로 합치는 이유: FFF_DestinationCandidate 주석대로 그 필드는
	 * "설치 완료 + 운영시간 + 임시 폐쇄 아님"을 호출부가 미리 합쳐서 넣은 값이다.
	 * 대입해 버리면 호출부가 이미 닫아둔 시설을 대기열이 열어주는 사고가 난다.
	 *
	 * CurrentOccupancy/Capacity 는 건드리지 않는다. 그건 Crowd 밀도 항의 입력이고,
	 * 대기 인원은 이미 셀 밀도에 잡혀 있다 (설계문서 8.4). 여기서 또 넣으면 이중 계산이다.
	 */
	static void ApplyQueueStateToCandidate(
		FFF_DestinationCandidate& InOutCandidate,
		const FFF_QueueState& State,
		int32 IncomingMemberCount);

	/**
	 * 후보 배열 전체를 한 번에 채운다.
	 *
	 * DestinationIdToFacilityId 는 호출부가 넘긴다 — 목적지 FName 과 시설 int32 의 대응은
	 * 시설 데이터(한광훈) 소관이라 Queue 가 소유하지 않는다.
	 * 매핑에 없거나 Queue 에 미등록인 목적지(벤치·포토존처럼 줄이 없는 곳)는 손대지 않는다.
	 *
	 * @return 실제로 대기열 정보가 반영된 후보 개수
	 */
	static int32 ApplyToCandidates(
		const UFF_QueueSubsystem* QueueSubsystem,
		TArray<FFF_DestinationCandidate>& InOutCandidates,
		const TMap<FName, int32>& DestinationIdToFacilityId,
		int32 IncomingMemberCount);
};
