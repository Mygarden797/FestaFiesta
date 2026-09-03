// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Party/FF_ProgramReservationSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogProgramReservation, Log, All);

FGuid UFF_ProgramReservationSubsystem::RegisterProgram(
	FGuid FacilityInstanceId, float StartMinutes, float EndMinutes, int32 Capacity)
{
	FFF_ProgramReservation Reservation;
	Reservation.FacilityInstanceId = FacilityInstanceId;
	Reservation.StartMinutes = StartMinutes;
	Reservation.EndMinutes = EndMinutes;
	Reservation.Capacity = Capacity;

	// 아직 아무도 안 왔으므로 빈자리는 정원 전체다. 기본값 0 으로 두면
	// `GetRemainingSeats` 가 갓 만든 예약에 "남은 자리 없음"을 돌려준다
	// (ClearAssignments 가 하루 경계에서 하는 것과 같은 초기화다).
	Reservation.UnfilledSeats = Capacity;

	// IsValid 가 세 조건(유효 시설·정원 양수·종료가 시작보다 뒤)을 한 곳에서 본다.
	// 여기서 거부하지 않으면 아무도 배정되지 않는 프로그램이 조용히 목록에 남는다.
	if (!Reservation.IsValid())
	{
		UE_LOG(LogProgramReservation, Warning,
			TEXT("[예약] 프로그램 등록 거부: 시설 %s, 시작 %.1f분, 종료 %.1f분, 정원 %d"),
			*FacilityInstanceId.ToString(), StartMinutes, EndMinutes, Capacity);
		return FGuid();
	}

	const FGuid ReservationId = FGuid::NewGuid();
	Reservations.Add(ReservationId, Reservation);

	UE_LOG(LogProgramReservation, Log,
		TEXT("[예약] 프로그램 등록: 시설 %s, %.1f~%.1f분, 정원 %d명"),
		*FacilityInstanceId.ToString(), StartMinutes, EndMinutes, Capacity);

	OnReservationChanged.Broadcast(ReservationId);
	return ReservationId;
}

bool UFF_ProgramReservationSubsystem::RemoveReservation(FGuid ReservationId)
{
	if (Reservations.Remove(ReservationId) == 0)
	{
		return false;
	}

	// 지운 예약에 배정돼 있던 파티는 색인에서도 빠져야 한다. 안 그러면 없는 예약을 가리키는
	// 파티가 남아서 잠금 판정이 계속 true 로 나온다.
	RebuildPartyIndex();

	OnReservationChanged.Broadcast(ReservationId);
	return true;
}

void UFF_ProgramReservationSubsystem::ClearAssignments()
{
	for (TPair<FGuid, FFF_ProgramReservation>& Pair : Reservations)
	{
		FFF_ProgramReservation& Reservation = Pair.Value;
		Reservation.AssignedGroupIds.Reset();
		Reservation.AssignedHeadcount = 0;

		// 아직 아무도 배정 안 된 상태이므로 못 채운 자리는 정원 전체다.
		Reservation.UnfilledSeats = Reservation.Capacity;

		OnReservationChanged.Broadcast(Pair.Key);
	}

	PartyToReservation.Reset();
}

void UFF_ProgramReservationSubsystem::ResetAll()
{
	Reservations.Reset();
	PartyToReservation.Reset();
}

bool UFF_ProgramReservationSubsystem::ConfirmReservedParty(
	FGuid ReservationId, int32 PartyId, int32 MemberCount)
{
	FFF_ProgramReservation* Reservation = Reservations.Find(ReservationId);
	if (Reservation == nullptr)
	{
		// 어제 산 프로그램의 예약 핸들을 오늘 스폰이 들고 오는 경우가 여기다.
		UE_LOG(LogProgramReservation, Warning,
			TEXT("[예약] 확정 실패: 없는 예약 %s (파티 %d)"), *ReservationId.ToString(), PartyId);
		return false;
	}

	if (PartyId == INDEX_NONE || MemberCount <= 0)
	{
		UE_LOG(LogProgramReservation, Warning,
			TEXT("[예약] 확정 실패: 파티 %d, 인원 %d — 스폰이 값을 못 채웠다"), PartyId, MemberCount);
		return false;
	}

	if (const FGuid* Existing = PartyToReservation.Find(PartyId))
	{
		// 같은 통지가 두 번 와도 인원이 두 배가 되면 안 된다. 스폰 쪽 재시도에 안전하게 둔다.
		if (*Existing == ReservationId)
		{
			return true;
		}

		UE_LOG(LogProgramReservation, Warning,
			TEXT("[예약] 확정 실패: 파티 %d 는 이미 예약 %s 에 속한다"),
			PartyId, *Existing->ToString());
		return false;
	}

	// 초과는 허용하지 않는다(규칙 3). 넘겨 받으면 정원이 의미를 잃고 체험비가 부풀려진다.
	if (Reservation->AssignedHeadcount + MemberCount > Reservation->Capacity)
	{
		UE_LOG(LogProgramReservation, Warning,
			TEXT("[예약] 확정 거부: 정원 초과 %d+%d > %d (예약 %s, 파티 %d)"),
			Reservation->AssignedHeadcount, MemberCount, Reservation->Capacity,
			*ReservationId.ToString(), PartyId);
		return false;
	}

	Reservation->AssignedGroupIds.Add(PartyId);

	// 통지 순서가 아니라 파티 번호로 정렬해 둔다.
	// 목록을 읽는 쪽(UI·테스트)이 스폰 순서에 의존하면 같은 Seed 인데 화면이 달라진다.
	Reservation->AssignedGroupIds.Sort();

	Reservation->AssignedHeadcount += MemberCount;
	Reservation->UnfilledSeats = Reservation->Capacity - Reservation->AssignedHeadcount;
	PartyToReservation.Add(PartyId, ReservationId);

	UE_LOG(LogProgramReservation, Log,
		TEXT("[예약] 파티 %d(%d명) 확정. %d/%d명"),
		PartyId, MemberCount, Reservation->AssignedHeadcount, Reservation->Capacity);

	OnReservationChanged.Broadcast(ReservationId);
	return true;
}

TArray<FGuid> UFF_ProgramReservationSubsystem::GetReservationIds() const
{
	TArray<FGuid> Ids;
	Reservations.GetKeys(Ids);

	// 시작 시각 오름차순. 같은 시각이면 핸들 문자열로 갈라 완전한 순서를 만든다 —
	// 동점을 남겨두면 그 둘의 순서가 TMap 내부 순서에 도로 의존한다.
	Ids.Sort([this](const FGuid& A, const FGuid& B)
	{
		const FFF_ProgramReservation& LeftReservation = Reservations[A];
		const FFF_ProgramReservation& RightReservation = Reservations[B];
		if (LeftReservation.StartMinutes != RightReservation.StartMinutes)
		{
			return LeftReservation.StartMinutes < RightReservation.StartMinutes;
		}
		return A.ToString() < B.ToString();
	});

	return Ids;
}

bool UFF_ProgramReservationSubsystem::FindReservation(
	FGuid ReservationId, FFF_ProgramReservation& OutReservation) const
{
	if (const FFF_ProgramReservation* Found = Reservations.Find(ReservationId))
	{
		OutReservation = *Found;
		return true;
	}
	return false;
}

int32 UFF_ProgramReservationSubsystem::GetRemainingSeats(FGuid ReservationId) const
{
	const FFF_ProgramReservation* Found = Reservations.Find(ReservationId);
	return Found ? Found->UnfilledSeats : 0;
}

bool UFF_ProgramReservationSubsystem::FindActiveReservationForParty(
	int32 PartyId, float NowMinutes, FFF_ProgramReservation& OutReservation) const
{
	const FGuid* ReservationId = PartyToReservation.Find(PartyId);
	if (ReservationId == nullptr)
	{
		return false;
	}

	const FFF_ProgramReservation* Found = Reservations.Find(*ReservationId);
	if (Found == nullptr)
	{
		return false;
	}

	// 끝난 예약은 없는 것과 같다 — 잠금도 퇴장 금지도 풀린 상태다.
	if (NowMinutes >= Found->EndMinutes)
	{
		return false;
	}

	OutReservation = *Found;
	return true;
}

bool UFF_ProgramReservationSubsystem::IsProgramFacility(const FGuid FacilityInstanceId) const
{
	if (!FacilityInstanceId.IsValid()) return false;
	for (const auto& Pair : Reservations)
	{
		if (Pair.Value.FacilityInstanceId == FacilityInstanceId) return true;
	}
	return false;
}

bool UFF_ProgramReservationSubsystem::IsPartyLockedAt(int32 PartyId, float NowMinutes) const
{
	FFF_ProgramReservation Reservation;
	if (!FindActiveReservationForParty(PartyId, NowMinutes, Reservation))
	{
		return false;
	}
	return FFF_ProgramReservationRules::IsLockedAt(Reservation, NowMinutes);
}

bool UFF_ProgramReservationSubsystem::IsPartyExitBlockedAt(int32 PartyId, float NowMinutes) const
{
	FFF_ProgramReservation Reservation;
	if (!FindActiveReservationForParty(PartyId, NowMinutes, Reservation))
	{
		return false;
	}
	return FFF_ProgramReservationRules::IsExitBlockedAt(Reservation, NowMinutes);
}

void UFF_ProgramReservationSubsystem::RebuildPartyIndex()
{
	PartyToReservation.Reset();
	for (const TPair<FGuid, FFF_ProgramReservation>& Pair : Reservations)
	{
		for (const int32 GroupId : Pair.Value.AssignedGroupIds)
		{
			PartyToReservation.Add(GroupId, Pair.Key);
		}
	}
}
