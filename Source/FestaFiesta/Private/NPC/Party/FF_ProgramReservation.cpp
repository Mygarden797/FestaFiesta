// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Party/FF_ProgramReservation.h"

namespace
{
	// 그룹 크기 상한. population_group_generation_spec.md 의 1~6 과 같아야 한다.
	// 이 값을 넘는 그룹이 후보로 들어오면 배정에서 제외된다(초과 배정을 만들지 않기 위해).
	constexpr int32 MaxGroupSize = 6;

	// DP 의 "이 자리 수는 만들 수 없다" 표식. 취향 점수가 음수일 수 있어서 0 을 쓸 수 없다
	// (preference_norm 은 -1~1 로 clamp 된 값이다).
	const float Unreachable = TNumericLimits<float>::Lowest();
}

void FFF_ProgramReservationRules::Assign(
	FFF_ProgramReservation& InOutReservation,
	const TArray<FFF_ReservationCandidateGroup>& Candidates)
{
	InOutReservation.AssignedGroupIds.Reset();
	InOutReservation.AssignedHeadcount = 0;
	InOutReservation.UnfilledSeats = 0;

	if (!InOutReservation.IsValid())
	{
		// 무효한 예약은 채울 자리가 없는 것으로 본다 — UnfilledSeats 를 정원으로 두면
		// "정원을 못 채웠다"는 진단 로그가 잘못 뜬다.
		return;
	}

	const int32 Capacity = InOutReservation.Capacity;

	// ── 1. 크기별로 묶는다. 각 버킷은 취향 내림차순 → GroupId 오름차순 ────────────────
	// 전체를 한 번 정렬한 뒤 순서대로 담으면 버킷 안의 순서가 전체 정렬 순서를 그대로 물려받는다.
	TArray<FFF_ReservationCandidateGroup> Sorted = Candidates;
	Sorted.Sort([](const FFF_ReservationCandidateGroup& A, const FFF_ReservationCandidateGroup& B)
	{
		if (A.PreferenceScore != B.PreferenceScore)
		{
			return A.PreferenceScore > B.PreferenceScore;
		}
		// 취향이 같으면 GroupId 순. 이 타이브레이커가 없으면 입력 순서에 결과가 의존한다.
		return A.GroupId < B.GroupId;
	});

	// Buckets[Size] = 그 크기 그룹들의 GroupId (취향 순)
	TArray<TArray<int32>> Buckets;
	Buckets.SetNum(MaxGroupSize + 1);
	// PrefixScore[Size][k] = 그 크기에서 상위 k 개를 썼을 때의 취향 합
	TArray<TArray<float>> PrefixScore;
	PrefixScore.SetNum(MaxGroupSize + 1);
	for (int32 Size = 0; Size <= MaxGroupSize; ++Size)
	{
		PrefixScore[Size].Add(0.f);
	}

	for (const FFF_ReservationCandidateGroup& Group : Sorted)
	{
		// 크기가 범위를 벗어난 후보는 버린다. 0 이하는 데이터 오류이고, 상한 초과는
		// 그룹 생성 규칙이 바뀌었다는 뜻이라 조용히 배정하면 정원을 넘길 수 있다.
		// 정원보다 큰 그룹도 버린다 — 어차피 못 들어가고 DP 표만 커진다.
		if (Group.MemberCount <= 0 || Group.MemberCount > MaxGroupSize || Group.MemberCount > Capacity)
		{
			continue;
		}

		Buckets[Group.MemberCount].Add(Group.GroupId);
		PrefixScore[Group.MemberCount].Add(
			PrefixScore[Group.MemberCount].Last() + Group.PreferenceScore);
	}

	// ── 2. 유계 배낭 DP — 정확히 c 자리를 채우는 최대 취향 합 ─────────────────────────
	// BestScore[c]        = 취향 합. Unreachable 이면 그 자리 수를 만들 수 없다
	// UsedCount[c][Size]  = 그 상태에 도달했을 때 각 크기를 몇 개 썼는지(역추적용)
	TArray<float> BestScore;
	BestScore.Init(Unreachable, Capacity + 1);
	BestScore[0] = 0.f;

	TArray<TArray<int32>> UsedCount;
	UsedCount.SetNum(Capacity + 1);
	for (TArray<int32>& Row : UsedCount)
	{
		Row.Init(0, MaxGroupSize + 1);
	}

	for (int32 Size = 1; Size <= MaxGroupSize; ++Size)
	{
		const int32 Available = Buckets[Size].Num();
		if (Available <= 0)
		{
			continue;
		}

		// 자리 수를 **내려가며** 갱신한다. 갱신은 항상 더 큰 Seats 로 쓰고 읽는 것은 더 작은
		// From 이므로, 이 크기를 처리하는 동안 읽는 값은 전부 "이 크기를 아직 안 쓴 상태"다 —
		// 그래서 같은 그룹이 두 번 배정되지 않는다(유계 배낭의 표준 순서).
		for (int32 Seats = Capacity; Seats >= Size; --Seats)
		{
			const int32 MaxUse = FMath::Min(Available, Seats / Size);
			for (int32 Use = 1; Use <= MaxUse; ++Use)
			{
				const int32 From = Seats - Size * Use;
				if (BestScore[From] <= Unreachable)
				{
					continue;
				}

				const float NewScore = BestScore[From] + PrefixScore[Size][Use];

				// 같은 점수면 갱신하지 않는다 — 먼저 계산된 조합을 유지해 결정성을 보장한다.
				if (NewScore > BestScore[Seats])
				{
					BestScore[Seats] = NewScore;
					UsedCount[Seats] = UsedCount[From];
					UsedCount[Seats][Size] = Use;
				}
			}
		}
	}

	// ── 3. 정확히 채우는 조합이 없으면 가장 많이 채우는 조합으로 떨어진다 ─────────────
	int32 FilledSeats = 0;
	for (int32 Seats = Capacity; Seats >= 0; --Seats)
	{
		if (BestScore[Seats] > Unreachable)
		{
			FilledSeats = Seats;
			break;
		}
	}

	// ── 4. 크기 구성을 실제 GroupId 로 되돌린다 ───────────────────────────────────────
	// 크기 오름차순으로 담으면 결과 배열 순서가 입력과 무관하게 고정된다.
	for (int32 Size = 1; Size <= MaxGroupSize; ++Size)
	{
		const int32 Use = UsedCount[FilledSeats][Size];
		for (int32 Index = 0; Index < Use; ++Index)
		{
			InOutReservation.AssignedGroupIds.Add(Buckets[Size][Index]);
		}
	}

	InOutReservation.AssignedHeadcount = FilledSeats;
	InOutReservation.UnfilledSeats = Capacity - FilledSeats;
}
