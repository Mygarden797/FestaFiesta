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

int32 UFF_ProgramReservationSubsystem::AssignGroups(
	FGuid ReservationId, const TArray<FFF_ReservationCandidateGroup>& Candidates)
{
	FFF_ProgramReservation* Reservation = Reservations.Find(ReservationId);
	if (Reservation == nullptr)
	{
		UE_LOG(LogProgramReservation, Warning,
			TEXT("[예약] 배정 실패: 없는 예약 %s"), *ReservationId.ToString());
		return 0;
	}

	// 다른 예약에 이미 들어간 그룹을 뺀다. 규칙 함수는 "호출부가 이미 필터링한 목록"을
	// 전제하고, 그 호출부가 여기다. 같은 파티가 두 프로그램에 동시에 들어가면
	// 잠금 구간이 겹쳐서 어느 쪽 시각으로 판정할지가 없어진다.
	TArray<FFF_ReservationCandidateGroup> Unassigned;
	Unassigned.Reserve(Candidates.Num());
	for (const FFF_ReservationCandidateGroup& Candidate : Candidates)
	{
		const FGuid* Existing = PartyToReservation.Find(Candidate.GroupId);
		if (Existing == nullptr || *Existing == ReservationId)
		{
			Unassigned.Add(Candidate);
		}
	}

	// 재배정이면 이전 배정을 먼저 걷어낸다 — 안 그러면 색인에 옛 그룹이 남는다.
	for (const int32 PreviousGroupId : Reservation->AssignedGroupIds)
	{
		PartyToReservation.Remove(PreviousGroupId);
	}

	FFF_ProgramReservationRules::Assign(*Reservation, Unassigned);

	for (const int32 GroupId : Reservation->AssignedGroupIds)
	{
		PartyToReservation.Add(GroupId, ReservationId);
	}

	// 정원을 못 채운 것은 실패가 아니라 관찰 대상이다. 그룹 크기 조합으로 정확히 채우는
	// 규칙이라 후보 구성에 따라 남을 수 있다(§3.4). 조용히 넘어가면 정원 설계가 틀린 것을 못 본다.
	if (Reservation->UnfilledSeats > 0)
	{
		UE_LOG(LogProgramReservation, Warning,
			TEXT("[예약] 정원을 다 못 채웠다: %d/%d명, 빈자리 %d (후보 %d그룹)"),
			Reservation->AssignedHeadcount, Reservation->Capacity,
			Reservation->UnfilledSeats, Unassigned.Num());
	}

	OnReservationChanged.Broadcast(ReservationId);
	return Reservation->AssignedHeadcount;
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
