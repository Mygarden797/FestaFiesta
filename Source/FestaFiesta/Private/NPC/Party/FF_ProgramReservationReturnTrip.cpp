// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Party/FF_ProgramReservationReturnTrip.h"

FFF_ReturnTripVerdict FFF_ProgramReservationReturnTrip::Evaluate(
	const FFF_ProgramReservation& Reservation,
	float NowMinutes,
	const FFF_ReturnTripInput& Input)
{
	FFF_ReturnTripVerdict Verdict;

	// 예약이 없으면 귀환 제약이 없다. 판정하지 않았음을 남기고 후보로 남긴다.
	if (!Reservation.IsValid())
	{
		return Verdict;
	}

	// 속도가 0 이하면 이동시간을 계산할 수 없다. 후보를 남기는 이유는 헤더 주석 참조 —
	// 후보를 전부 닫으면 no_choice → Leave 로 이어져 정원이 비고 매출이 비결정적이 된다.
	if (Input.WalkSpeedMetersPerSecond <= 0.f)
	{
		return Verdict;
	}

	Verdict.bWasEvaluated = true;

	// 거리는 m, 속도는 m/s, 결과는 분. 음수 거리는 데이터 오류이므로 0 으로 본다.
	const float SecondsPerMinute = 60.f;
	const auto ToMinutes = [&Input, SecondsPerMinute](float Meters)
	{
		return FMath::Max(Meters, 0.f) / Input.WalkSpeedMetersPerSecond / SecondsPerMinute;
	};

	Verdict.RequiredMinutes =
		ToMinutes(Input.DistanceToCandidateMeters)
		+ FMath::Max(Input.CandidateWaitMinutes, 0.f)
		+ FMath::Max(Input.CandidateUseMinutes, 0.f)
		+ ToMinutes(Input.CandidateToProgramMeters);

	const float UntilStart = GetMinutesUntilStart(Reservation, NowMinutes);
	Verdict.AvailableMinutes = FMath::Max(UntilStart - FMath::Max(Input.ReturnBufferMinutes, 0.f), 0.f);

	Verdict.bCanReturnInTime = Verdict.RequiredMinutes <= Verdict.AvailableMinutes;
	return Verdict;
}
