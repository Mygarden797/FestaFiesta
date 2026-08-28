// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NPC/Party/FF_ProgramReservation.h"
#include "FF_ProgramReservationReturnTrip.generated.h"

/**
 * 예약 귀환 안전장치 1차 — v0 범위 밖. (지금 아무도 부르지 않는다.)
 *
 * ### 왜 별 파일로 빼놨나 (2026-08-11)
 * 규칙은 확정이고 계산도 테스트로 고정됐지만 **켤 수가 없다.** 
 * `CandidateWaitMinutes` 가 아직 0이라(Queue 어댑터 결선 전) 판정이 "줄이 없다"로 낙관적으로 기운다.
 *
 * ### 켤 때 할 일
 * 1. `EstimatedWaitMinutes` 가 실제 값이 되었는지 확인한다(그게 유일한 선행 조건이다)
 * 2. `BuildCandidates` 에서 예약 있는 Party 의 후보에 이 판정을 걸고
 *    `bCanReturnInTime == false` 인 후보를 제외한다
 * 3. 🔴 같은 커밋에 "통과 후보 0개 → 잠금" 분기를 넣는다. 안 넣으면 시작 직전에 후보가
 *    말라 `no_choice` 2회 → `Leave` 로 예약자가 퇴장하고 정원이 빈다 —
 *    (`reservation_program_spec.md` §3.5)
 */
USTRUCT(BlueprintType)
struct FFF_ReturnTripInput
{
	GENERATED_BODY()

	/**
	 * 보행 속도 기본값 (m/s).
	 *
	 * `derived` — `FUnitData::Speed` 기본값 150 cm/s
	 * (출처: `CrowdSystem/Types/FF_FestivalCrowdTypes.h`). UE 는 1uu = 1cm 이므로 1.5 m/s 다.
	 */
	static constexpr float DefaultWalkSpeedMetersPerSecond = 1.5f;

	// 현재 위치 → 후보 시설. 직선거리 근사
	UPROPERTY(BlueprintReadWrite, Category = "Reservation")
	float DistanceToCandidateMeters = 0.f;

	// 후보 시설의 예상 대기시간(분). ⚠️ 어댑터 결선 전에는 항상 0 이다
	UPROPERTY(BlueprintReadWrite, Category = "Reservation")
	float CandidateWaitMinutes = 0.f;

	// 후보 시설 이용에 걸리는 시간(분). 모르면 0 — 그만큼 낙관적으로 판정된다
	UPROPERTY(BlueprintReadWrite, Category = "Reservation")
	float CandidateUseMinutes = 0.f;

	// 후보 시설 → 예약 시설. 직선거리 근사
	UPROPERTY(BlueprintReadWrite, Category = "Reservation")
	float CandidateToProgramMeters = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Reservation")
	float WalkSpeedMetersPerSecond = DefaultWalkSpeedMetersPerSecond;

	/**
	 * 귀환 여유(분). 이만큼 '일찍' 돌아오도록 가용 시간을 깎는다.
	 * 키우면 후보가 가까운 것만 남아 NPC 가 예약 시설 근처에 머문다.
	 * 0 은 가장 느슨한 값이다 — 남은 시간을 끝까지 써서 시작 시각에 딱 맞춰 온다.
	 *
	 * !!! 출처 없음 !!! 기본 0. 실제 보행거리 행렬이 들어오면 정한다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Reservation")
	float ReturnBufferMinutes = 0.f;
};

/** 귀환 판정 결과. 통과·탈락하는 주어는 NPC 가 아니라 **후보 시설**이다. */
USTRUCT(BlueprintType)
struct FFF_ReturnTripVerdict
{
	GENERATED_BODY()

	// true 면 이 시설이 후보 목록에 남는다. false 면 후보에서 제외한다
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	bool bCanReturnInTime = true;

	/**
	 * 판정을 실제로 했는가. false 면 위 true 는 "확인해서 괜찮다"가 아니라
	 * "확인 못 하고 그냥 남겼다"는 뜻이다 — 로그에서 둘을 구분하기 위한 값이다.
	 *
	 * 판정 불가(예약 없음 / 속도 0)일 때 **후보를 남긴다.** 빼는 쪽이 더 위험하다 —
	 * 후보 0개는 `no_choice` 이고 2회면 `Leave` 다. 리더가 나가면 그룹 전체가 나가 정원이 빈다.
	 * 지각보다 퇴장이 나쁘다. (대기열 어댑터는 반대로 후보를 뺀다 — 거기서 남기면
	 * "열려 있고 대기 0"이 되어 존재하지 않는 시설이 1등 후보가 된다)
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	bool bWasEvaluated = false;

	// 왕복에 필요한 총 분 (이동 + 대기 + 이용 + 복귀 이동)
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	float RequiredMinutes = 0.f;

	// 쓸 수 있는 분 (시작까지 남은 분 - 귀환 여유). 음수면 0 으로 본다
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	float AvailableMinutes = 0.f;
};

/** 귀환 판정 순수 함수. 파일 상단 주석 참조 — v0 에서는 호출하지 않는다. */
class FFF_ProgramReservationReturnTrip
{
public:
	/**
	 *     필요 분 = (현재 → 후보 이동) + 대기 + 이용 + (후보 → 예약 시설 이동)
	 *     가용 분 = (시작 시각 - 현재) - 귀환 여유
	 *     남긴다  = 필요 분 <= 가용 분
	 *
	 * 강제 순간이동을 쓰지 않는 이유: 관찰 게임에서 NPC 가 사라졌다 나타나는 것은 눈에 보이는
	 * 사고다. 늦으면 늦게 참여한다 — 프로그램에서 빼거나 정원을 다시 채우지 않는다.
	 */
	static FFF_ReturnTripVerdict Evaluate(
		const FFF_ProgramReservation& Reservation,
		float NowMinutes,
		const FFF_ReturnTripInput& Input);

	/** 예약 시작까지 남은 분. 이미 시작했거나 예약이 무효면 0 이다. 위 판정의 입력이다 */
	static float GetMinutesUntilStart(const FFF_ProgramReservation& Reservation, float NowMinutes)
	{
		if (!Reservation.IsValid())
		{
			return 0.f;
		}
		return FMath::Max(Reservation.StartMinutes - NowMinutes, 0.f);
	}
};
