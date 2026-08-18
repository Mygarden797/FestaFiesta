// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Queue/FF_QueueTypes.h"
#include "Templates/Function.h"

struct FFF_DestinationCandidate;
class UFF_QueueSubsystem;
class UFF_ZoneSystemQueueBridgeSubsystem;

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
	 * AND 하는 대상은 State.bIsOpen 이지 State.bAcceptingNew 가 아니다. 후자는 '잠깐 만석'까지
	 * 포함하는데, 만석은 앞 Party 하나 빠지면 풀리는 순간 상태다. 그걸로 후보를 닫으면 붐비는
	 * 부스가 결정 시점마다 사라졌다 나타났다 하고, "줄이 길어서 딴 데 간다"는 행동 자체가
	 * 표현되지 않는다. 설계문서 8.2① 도 도착 판정과 가입 판정을 분리하라고 정했다 —
	 * 걸어가는 동안 자리가 날 수도, 반대로 찰 수도 있고, 후자는 RejectedFull → 재평가로 처리된다.
	 * 만석은 후보에서 빼는 대신 EstimatedWaitMinutes 가 커지는 것으로 표현한다.
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

	/**
	 * 후보 하나에 직접 반영한다. FacilityId 를 이미 아는 호출부용(GUID→int32 매핑 소유자 등).
	 * 배열 버전과 같은 규칙을 따른다.
	 *
	 * @return 반영했으면 true. 미등록 시설이면 후보를 닫고 false.
	 */
	static bool ApplyToCandidate(
		const UFF_QueueSubsystem* QueueSubsystem,
		FFF_DestinationCandidate& InOutCandidate,
		int32 FacilityId,
		int32 IncomingMemberCount);

	/**
	 * 매핑을 표가 아니라 함수로 받는 버전.
	 *
	 * 목적지→시설 대응이 TMap 한 장으로 안 끝나는 경우가 있다. 이름을 파싱해야 하거나,
	 * 다른 서브시스템에 물어봐야 하거나, 중간에 GUID 를 한 번 거쳐야 하는 식이다.
	 * 그때마다 임시 TMap 을 만들면 매 결정마다 할당이 생긴다.
	 *
	 * Resolver 는 줄이 없는 목적지(벤치·포토존)에 INDEX_NONE 을 돌려주면 된다.
	 *
	 * @return 실제로 대기열 정보가 반영된 후보 개수
	 */
	static int32 ApplyToCandidates(
		const UFF_QueueSubsystem* QueueSubsystem,
		TArray<FFF_DestinationCandidate>& InOutCandidates,
		TFunctionRef<int32(FName)> ResolveFacilityId,
		int32 IncomingMemberCount);

	/**
	 * Zone 브리지를 통해 대기시간을 채운다. 결정 계층이 부를 최종 진입점이다.
	 *
	 * 목적지 ID 는 시설 인스턴스 GUID 를 문자열로 담고 있으므로, 이름을 파싱해 GUID 를 얻고
	 * 브리지에 물어 내부 Queue ID 를 받는다. 호출부가 매핑표를 따로 들 필요가 없다.
	 *
	 * 월드에서 Queue 서브시스템과 브리지를 직접 찾는다. 둘 중 하나라도 없으면
	 * 후보를 건드리지 않고 0 을 돌려준다 — 대기열이 없는 월드에서도 결정은 돌아야 한다.
	 *
	 * @return 실제로 대기열 정보가 반영된 후보 개수
	 */
	static int32 ApplyFromZoneBridge(
		const UWorld* World,
		TArray<FFF_DestinationCandidate>& InOutCandidates,
		int32 IncomingMemberCount);

	/**
	 * 위와 같지만 서브시스템을 직접 받는다. 이미 들고 있는 호출부와 테스트용이다.
	 */
	static int32 ApplyFromZoneBridge(
		const UFF_QueueSubsystem* QueueSubsystem,
		const UFF_ZoneSystemQueueBridgeSubsystem* Bridge,
		TArray<FFF_DestinationCandidate>& InOutCandidates,
		int32 IncomingMemberCount);
};
