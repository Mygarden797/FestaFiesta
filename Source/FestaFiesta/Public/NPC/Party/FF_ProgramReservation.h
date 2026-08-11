// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_ProgramReservation.generated.h"

/**
 * 유료 프로그램 하나의 예약.
 *
 * 설계·근거는 `data/metadata/spec/reservation_program_spec.md`.
 *
 * ### 개체보다 슬롯이 먼저 생기는 이유
 * 플레이어가 프로그램을 사는 시점은 Setup·MorningCheck 이고, 그때 NPC 는 아직 스폰되지 않았다.
 * 그래서 예약을 개체에 붙이지 않고 슬롯을 먼저 만들어 두고 그룹을 배정한다.
 *
 * [참고] 모든 시간 필드의 기준은 Live 경과분이다.
 */
USTRUCT(BlueprintType)
struct FFF_ProgramReservation
{
	GENERATED_BODY()

	// 어느 시설의 프로그램인가. ZoneSystem 의 FacilityInstanceId 다
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	FGuid FacilityInstanceId;

	// Live 경과분. 이 시각 전에 배정된 그룹은 반드시 장내에 있어야 한다
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	float StartMinutes = 0.f;

	// Live 경과분. 이 시각 이후부터 퇴장할 수 있다
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	float EndMinutes = 0.f;

	// 프로그램 정원(명). 그룹 크기 조합으로 정확히 채운다
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	int32 Capacity = 0;

	// 배정된 그룹. Assign 이 채운다. 크기 오름차순이라 입력 순서와 무관하게 고정된다
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	TArray<int32> AssignedGroupIds;

	// 배정된 인원 합. 정확히 채웠으면 Capacity 와 같다
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	int32 AssignedHeadcount = 0;

	// 못 채운 자리(명). 0 이면 정원을 정확히 채웠다.
	// 0 이 아니면 호출부가 로그를 남긴다
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	int32 UnfilledSeats = 0;

	bool IsValid() const
	{
		return FacilityInstanceId.IsValid() && Capacity > 0 && EndMinutes > StartMinutes;
	}

	bool HasGroup(int32 GroupId) const { return AssignedGroupIds.Contains(GroupId); }
};

// 예약 배정 대상이 되는 그룹 하나. 배정에 필요한 최소 정보만 담는다.
USTRUCT(BlueprintType)
struct FFF_ReservationCandidateGroup
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	int32 GroupId = INDEX_NONE;

	// 그룹 인원수 1~6 (아동 포함). 정원을 채우는 단위다
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	int32 MemberCount = 1;

	/**
	 * 이 프로그램에 대한 취향 일치도. 클수록 먼저 배정된다.
	 * 호출부가 `preference_vector` 에서 프로그램의 취향 카테고리 성분을 뽑아 넣는다 —
	 * v0 은 전부 0 이다(취향 결선 전). 그러면 GroupId 순 배정이 되고 동작은 한다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	float PreferenceScore = 0.f;
};

/**
 * 예약 규칙 — 상태를 갖지 않는 순수 함수만 제공한다.
 *
 * `FFF_VisitorDistributionBuilder` 와 같은 형태다. 같은 입력이면 항상 같은 결과이고
 * `FMath::Rand()` 를 쓰지 않는다(프로젝트 전역 규칙).
 * 예약 목록을 들고 구매 흐름을 태우는 것은 `UFF_FestivalProgressionSubsystem` 이다.
 *
 * ### 확정된 규칙 (사용자)
 * 1. 배정 단위는 그룹이다 (1인도 그룹이라 예외가 없다)
 * 2. 정원을 그룹 크기 조합으로 정확히 채운다 (10명이면 5+2+2+1 처럼 섞인다)
 * 3. 초과는 허용하지 않는다
 * 4. 예약이 확정된 뒤로는 프로그램이 끝날 때까지 퇴장하지 않는다
 */
class FFF_ProgramReservationRules
{
public:
	/**
	 * 정원에 그룹을 배정하고 결과를 `InOutReservation` 에 채운다.
	 *
	 * ### 방식 — 그룹 크기(1~6)별로 묶은 유계 배낭(bounded knapsack) DP
	 *
	 * DP 의 목표는 취향 합 최대화다. 각 크기 안에서는 취향 내림차순으로 앞에서부터 쓰고,
	 * 같은 점수면 먼저 계산된 조합을 유지해 결정성을 보장한다(갱신 조건이 `>` 다).
	 * 정확히 채우는 조합이 없으면 가장 많이 채우고 `UnfilledSeats` 가 0 보다 커진다.
	 *
	 * @param Candidates 배정 후보. **호출부가 이미 필터링한 목록이다**
	 *                   (예: 아동 있는 그룹을 성인 전용 프로그램에서 뺀 뒤)
	 */
	static void Assign(
		FFF_ProgramReservation& InOutReservation,
		const TArray<FFF_ReservationCandidateGroup>& Candidates);

	/**
	 * 이 시각에 예약이 활성인가 = 목적지 선택을 건너뛰어야 하는가.
	 *
	 * 진행 중(시작~종료)에만 true 다. 시작 전에는 자유 행동이 규칙이라 false 다 —
	 * 퇴장 금지와 구간이 다르다(`IsExitBlockedAt` 참조).
	 */
	static bool IsLockedAt(const FFF_ProgramReservation& Reservation, float NowMinutes)
	{
		return Reservation.IsValid()
			&& NowMinutes >= Reservation.StartMinutes
			&& NowMinutes < Reservation.EndMinutes;
	}

	/**
	 * 이 시각에 퇴장이 금지되는가.
	 *
	 * | 구간                 | 재평가         | 퇴장  |
	 * |---------------------|---------------|------|
	 * | 확정 ~ StartMinutes | 한다(자유 행동) | 금지 |
	 * | StartMinutes ~ End  | 안 한다(잠금)   | 금지 |
	 * | EndMinutes 이후      | 한다           | 가능 |
	 */
	static bool IsExitBlockedAt(const FFF_ProgramReservation& Reservation, float NowMinutes)
	{
		return Reservation.IsValid() && NowMinutes < Reservation.EndMinutes;
	}

	/**
	 * 프로그램이 끝나 퇴장 금지가 풀리는 순간의 체류 예산.
	 * 새 예산 = max(현재 예산, 이탈유예)
	 *
	 * 퇴장을 막는 동안 예산이 0 에 붙어 있을 수 있어서, 그대로 풀면 프로그램이 끝나는 순간
	 * 사라진다. 그게 부자연스러워서 유예를 준다.
	 */
	static float ApplyExitGrace(float CurrentRemainingStayMinutes, float ExitGraceMinutes)
	{
		return FMath::Max(CurrentRemainingStayMinutes, FMath::Max(ExitGraceMinutes, 0.f));
	}
};
