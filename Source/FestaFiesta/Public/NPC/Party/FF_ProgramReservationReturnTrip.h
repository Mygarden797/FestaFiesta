// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NPC/Party/FF_ProgramReservation.h"
#include "FF_ProgramReservationReturnTrip.generated.h"

/**
 * 예약 귀환 후보 필터의 입력. Queue 대기시간을 채운 뒤 Choose 앞에서 평가한다.
 * 통과 후보가 없어도 복귀 마감 전에는 배회한다. 복귀 마감은 Crowd가 별도로 감시한다.
 * 시간 입력은 모두 예약 Start/End와 같은 단위여야 한다.
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

	// 후보 시설의 예상 대기시간. 예약 시각과 같은 분 단위로 환산해서 넣는다.
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

	// 순수 계산 기본은 실제 분. 런타임은 Stage 시간 API의 축제 시간 환산을 전달한다.
	UPROPERTY(BlueprintReadWrite, Category = "Reservation")
	float MinutesPerTravelSecond = 1.f / 60.f;
	
	// 후보 방문 후 프로그램 시작보다 이만큼 일찍 도착할 수 있어야 한다.
	// 런타임은 Crowd의 ProgramReturnBufferMinutes를 전달한다.
	// 후보가 없어도 직접 복귀 마감 전에는 배회·재탐색한다.
	UPROPERTY(BlueprintReadWrite, Category = "Reservation")
	float ReturnBufferMinutes = 0.f;
};

// 귀환 판정 결과. 통과·탈락하는 주어는 NPC 가 아니라 후보 시설이다.
USTRUCT(BlueprintType)
struct FFF_ReturnTripVerdict
{
	GENERATED_BODY()

	// true 면 이 시설이 후보 목록에 남는다. false 면 후보에서 제외한다
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	bool bCanReturnInTime = true;

	// 판정 불가(예약 없음 / 속도 0)면 후보를 유지한다. 퇴장 차단은 Crowd가 별도로 보장한다.
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	bool bWasEvaluated = false;

	// 왕복에 필요한 총 분 (이동 + 대기 + 이용 + 복귀 이동)
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	float RequiredMinutes = 0.f;

	// 쓸 수 있는 분 (시작까지 남은 분 - 귀환 여유). 음수면 0 으로 본다
	UPROPERTY(BlueprintReadOnly, Category = "Reservation")
	float AvailableMinutes = 0.f;
};

// 귀환 판정 순수 함수. 런타임 호출은 CrowdDecisionSubsystem이 맡는다.
class FFF_ProgramReservationReturnTrip
{
public:
	/**
	 *     필요 분 = (현재 → 후보 이동) + 대기 + 이용 + (후보 → 예약 시설 이동)
	 *     가용 분 = (시작 시각 - 현재) - 귀환 여유
	 *     남긴다  = 필요 분 <= 가용 분
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
