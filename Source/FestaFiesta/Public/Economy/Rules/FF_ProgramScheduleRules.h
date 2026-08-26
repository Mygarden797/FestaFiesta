// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "NPC/Party/FF_VisitorDistributionBuilder.h"

/**
 * 프로그램 편성 판정 — 상태를 갖지 않는 순수 함수. FFF_FinaleRules 와 같은 형태다.
 *
 * FFF_ProgramReservationRules(NPC/Party) 와 다른 것이다.
 * 그쪽은 **정원에 그룹을 배정**하고, 이쪽은 **편성할 수 있는가**만 본다.
 * 차감·상태 변경은 UFF_FestivalProgressionSubsystem 이 한다.
 */
struct FESTAFIESTA_API FFF_ProgramScheduleRules
{
	// 카탈로그에서 카드를 찾는다. 없으면 nullptr.
	static const FFF_ProgramCard* FindCard(
		const TArray<FFF_ProgramCard>& Catalog, FName ProgramId);

	/**
	 * 이 프로그램을 오늘 편성할 수 있는가.
	 *
	 * 판정 순서
	 * ① 카드 존재·유효 → ② 오늘 중복 → ③ 인원 → ④ 잔액.
	 *
	 * **인원을 잔액보다 먼저 본다.** 둘 다 부족할 때 플레이어가 할 수 있는 행동이 다르다 —
	 * 인원은 알바 고용으로 풀린다. 더 가까운 행동을 먼저 알려 준다.
	 *
	 * @param AvailableStaffCount 전체 인원에서 이미 다른 프로그램에 묶인 인원을 뺀 값
	 * @param OutCard 성공·실패와 무관하게, 카드를 찾았으면 채워진다(UI 가 비용을 다시 조회하지 않게)
	 */
	static EFF_ProgramPurchaseResult EvaluatePurchase(
		const TArray<FFF_ProgramCard>& Catalog,
		FName ProgramId,
		const TArray<FName>& ScheduledToday,
		int32 CurrentGold,
		int32 AvailableStaffCount,
		FFF_ProgramCard& OutCard);

	// 편성된 카드들이 묶고 있는 인원 합. 없는 카드는 0 으로 친다.
	static int32 CalcStaffAssignedToPrograms(
		const TArray<FFF_ProgramCard>& Catalog, const TArray<FName>& ScheduledToday);

	// 참가 인원에 대한 그날 체험비 매출. 정원을 넘겨 받아도 정원에서 자른다.
	static int32 CalcProgramRevenue(const FFF_ProgramCard& Card, int32 AttendeeCount);

	/**
	 * 이 카드의 정원을 그룹 단위 주문서로 쪼갠다. 스폰에 넘길 목록이다.
	 * 
	 * 큰 그룹부터 채우고 나머지를 마지막 그룹에 담는다 — 그룹 수가 최소가 되어 파티가 잘게 쪼개지지 않는다.
	 * 일단, 아동은 넣지 않는다.
	 * 정원 합은 항상 `Card.Capacity` 와 같다 — 이것이 "산 정원만큼 온다"의 근거다.
	 */
	static TArray<FFF_ReservedPartyRequest> BuildReservedPartyRequests(
		const FFF_ProgramCard& Card, const FGuid& ReservationId);

	/**
	 * 이 시설에서 이 프로그램을 열 수 있는가.
	 *
	 * 시설 식별자는 `대분류_소분류_고유이름` 형태라, 앞 두 토큰만 합치면 시설 종류가 된다.
	 * 카드의 허용 목록과 그 값을 비교한다. 시설 데이터를 읽지 않는 순수 비교라
	 * 월드 없이 검증할 수 있다.
	 *
	 * @param MajorToken 시설 식별자의 첫 토큰 (`Booth`, `Performance` 등)
	 * @param MinorToken 두 번째 토큰 (`CraftExperience`, `ExperienceStage` 등)
	 * @return 허용 목록이 비어 있으면 항상 true — 제약 없음으로 본다
	 */
	static bool IsFacilityTypeAllowed(
		const FFF_ProgramCard& Card, FName MajorToken, FName MinorToken);
};
