// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/Rules/FF_ProgramScheduleRules.h"

const FFF_ProgramCard* FFF_ProgramScheduleRules::FindCard(
	const TArray<FFF_ProgramCard>& Catalog, const FName ProgramId)
{
	if (ProgramId.IsNone())
	{
		return nullptr;
	}

	return Catalog.FindByPredicate(
		[ProgramId](const FFF_ProgramCard& Card) { return Card.ProgramId == ProgramId; });
}

EFF_ProgramPurchaseResult FFF_ProgramScheduleRules::EvaluatePurchase(
	const TArray<FFF_ProgramCard>& Catalog,
	const FName ProgramId,
	const TArray<FName>& ScheduledToday,
	const int32 CurrentGold,
	FFF_ProgramCard& OutCard)
{
	const FFF_ProgramCard* Card = FindCard(Catalog, ProgramId);

	// 저작이 깨진 카드도 UnknownProgram 으로 묶는다. 플레이어에게는 같은 상황이고,
	// 저작 오류를 잡는 것은 부팅 때 도는 IsDefinitionValid 의 몫이다.
	if (Card == nullptr || !Card->IsValid())
	{
		return EFF_ProgramPurchaseResult::UnknownProgram;
	}

	OutCard = *Card;

	// 같은 카드를 하루에 두 번 열어 정원을 배로 만드는 것을 막는다.
	if (ScheduledToday.Contains(ProgramId))
	{
		return EFF_ProgramPurchaseResult::AlreadyScheduled;
	}

	if (CurrentGold < Card->CostGold)
	{
		return EFF_ProgramPurchaseResult::InsufficientGold;
	}

	return EFF_ProgramPurchaseResult::Success;
}

int32 FFF_ProgramScheduleRules::CalcProgramRevenue(
	const FFF_ProgramCard& Card, const int32 AttendeeCount)
{
	// 정원을 넘는 배정은 규칙상 나오지 않지만, 넘어와도 산 것보다 더 받지는 않는다.
	const int32 PaidCount = FMath::Clamp(AttendeeCount, 0, Card.Capacity);
	return PaidCount * Card.FeePerPerson;
}

bool FFF_ProgramScheduleRules::IsFacilityTypeAllowed(
	const FFF_ProgramCard& Card, const FName MajorToken, const FName MinorToken)
{
	// 저작이 비면 제약이 없는 것으로 본다. 여기서 거절하면 목록을 안 채운 카드가
	// 전부 편성 불가가 되어, 밸런스 저작 중에 기능이 통째로 막힌다.
	if (Card.AllowedFacilityTypes.Num() == 0)
	{
		return true;
	}

	if (MajorToken.IsNone() || MinorToken.IsNone())
	{
		return false;
	}

	const FName FacilityType(*FString::Printf(
		TEXT("%s_%s"), *MajorToken.ToString(), *MinorToken.ToString()));

	return Card.AllowedFacilityTypes.Contains(FacilityType);
}

TArray<FFF_ReservedPartyRequest> FFF_ProgramScheduleRules::BuildReservedPartyRequests(
	const FFF_ProgramCard& Card, const FGuid& ReservationId)
{
	// 그룹 분포(visitor_group_size_distribution_v0)의 상한과 같은 값이다.
	// 여기만 6 이고 저쪽이 다르면 빌더가 주문서를 통째로 거절한다.
	constexpr int32 MaxGroupSize = 6;

	TArray<FFF_ReservedPartyRequest> Requests;
	if (Card.Capacity <= 0 || !ReservationId.IsValid())
	{
		return Requests;
	}

	int32 Remaining = Card.Capacity;
	while (Remaining > 0)
	{
		FFF_ReservedPartyRequest Request;
		Request.ReservationId = ReservationId;
		Request.AdultArchetypeId = Card.PreferredArchetypeId;
		Request.GroupSize = FMath::Min(Remaining, MaxGroupSize);
		Request.ChildCount = 0;
		Requests.Add(Request);

		Remaining -= Request.GroupSize;
	}

	return Requests;
}
