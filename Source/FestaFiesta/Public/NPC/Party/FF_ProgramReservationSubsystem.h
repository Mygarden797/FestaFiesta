// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NPC/Party/FF_ProgramReservation.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_ProgramReservationSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnProgramReservationChanged, FGuid, ReservationId);

/**
 * 유료 프로그램 예약의 런타임 소유자.
 * [참고] `data/metadata/spec/reservation_program_spec.md`.
 *
 * ### 계층
 * 이 서브시스템은 **보관·수명주기·조회**만 한다. 잠금·퇴장 판정은
 * `FFF_ProgramReservationRules` 의 순수 함수가 갖는다.
 *
 * 프로그램은 Setup·MorningCheck 에서 사고 그날치로 편성된다 - GameInstance 스코프다.
 * 그룹 확정(`ConfirmReservedParty`)은 매일 새로 한다 — 예약 손님이 날마다 새로 스폰된다.
 *
 * ⚠️ **"날마다 새로 스폰된다"는 아직 확인된 사실이 아니다.** 크라우드는 레벨 BeginPlay 에
 * 한 번만 유입을 시작하고 하루 경계를 구독하지 않는다.
 * `crowdai_integration_guide.md` §13.3 E1 로 물어둔 상태다.
 */
UCLASS()
class FESTAFIESTA_API UFF_ProgramReservationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// --- 슬롯 수명주기 -----------------------------------------------------

	// 프로그램 슬롯을 만든다. 구매가 성사된 뒤 경제가 부르는 창구다.
	// 시간은 전부 "Live 경과분"이다. 
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Reservation")
	FGuid RegisterProgram(FGuid FacilityInstanceId, float StartMinutes, float EndMinutes, int32 Capacity);

	// 슬롯을 지운다(구매 취소). 배정된 그룹도 같이 풀린다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Reservation")
	bool RemoveReservation(FGuid ReservationId);

	// 하루 경계에서 부른다 - 모든 예약의 "배정"만 비운다. 슬롯은 남는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Reservation")
	void ClearAssignments();

	// 축제를 새로 시작할 때. 슬롯까지 전부 지운다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Reservation")
	void ResetAll();

	// --- 배정 -------------------------------------------------------------

	/**
	 * 스폰된 예약 그룹을 확정한다. **crowdAI 가 그룹을 만든 직후 부르는 창구다.**
	 *
	 * 프로그램을 사면 정원만큼의 손님이 추가로 방문한다. 그 손님은
	 * `FFF_TaggedVisitor::ReservationId` 를 달고 태어나므로, 스폰한 쪽은 선두의 그 값과
	 * 방금 발급한 `PartyId` 만 넘기면 된다 — 몇 번째 그룹이 예약이었는지 셀 필요가 없다.
	 *
	 * 고를 것이 없어서 계산도 없다 — 그 그룹은 **이 프로그램 때문에 생겼다.** 검사만 한다.
	 *
	 * @return 확정되면 true. 없는 예약·정원 초과·다른 예약에 이미 속한 파티면 false.
	 *         같은 예약에 같은 파티를 다시 넣는 것은 성공으로 친다(중복 통지에 안전하다).
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Reservation")
	bool ConfirmReservedParty(FGuid ReservationId, int32 PartyId, int32 MemberCount);

	// --- 조회 -------------------------------------------------------------

	// 예약 핸들 목록. **시작 시각 오름차순으로 정렬해서 돌려준다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Reservation")
	TArray<FGuid> GetReservationIds() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Reservation")
	bool FindReservation(FGuid ReservationId, FFF_ProgramReservation& OutReservation) const;

	// 남은 자리(명). 없는 예약이면 0.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Reservation")
	int32 GetRemainingSeats(FGuid ReservationId) const;

	// 이 파티가 배정된, 아직 끝나지 않은 예약을 찾는다. 결정 계층이 쓰는 창구다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Reservation")
	bool FindActiveReservationForParty(int32 PartyId, float NowMinutes, FFF_ProgramReservation& OutReservation) const;

	// 이 파티가 지금 잠금 상태인가 = 목적지 재평가를 건너뛰어야 하는가.
	// 진행 중(시작~종료)에만 true 다. 시작 전 자유 행동 구간은 false 다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Reservation")
	bool IsPartyLockedAt(int32 PartyId, float NowMinutes) const;

	// 이 파티의 퇴장이 지금 금지되는가.
	// 잠금과 구간이 다르다 — 확정 시점부터 종료까지 전부 금지다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Reservation")
	bool IsPartyExitBlockedAt(int32 PartyId, float NowMinutes) const;

	// 값이 바뀐 예약을 알린다. UI 목록·잔여석 표시가 여기 붙는다.
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Reservation")
	FFF_OnProgramReservationChanged OnReservationChanged;

private:
	// 배정된 파티를 예약 핸들로 되짚는 색인. 파티 조회가 예약 수에 비례해 느려지지 않게 한다.
	void RebuildPartyIndex();

	// 핸들 → 예약. 순회 결과를 그대로 노출하지 않는다(정렬은 GetReservationIds 가 한다).
	UPROPERTY()
	TMap<FGuid, FFF_ProgramReservation> Reservations;

	// PartyId → 배정된 예약 핸들.
	UPROPERTY()
	TMap<int32, FGuid> PartyToReservation;
};
