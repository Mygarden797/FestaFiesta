// Fill out your copyright notice in the Description page of Project Settings.

#include "NPC/Integration/FF_WalkDistanceMatrix.h"

void FFF_WalkDistanceMatrix::Reset()
{
	ZoneIds.Reset();
	AnchorsCm.Reset();
	DistancesCm.Reset();
}

void FFF_WalkDistanceMatrix::Build(const TArray<FZoneAnchor>& Anchors, FPathLengthProvider PathLength,
                                   FBuildStats* OutStats)
{
	Reset();

	if (OutStats)
	{
		*OutStats = FBuildStats();
	}

	// ── 1. 존 순서 확정 ────────────────────────────────────────────────
	// ZoneId 사전순으로 정렬한다. 등록 순서(액터 스폰 순서, 스트리밍 순서)가 행렬 인덱스에
	// 새어 들어가면 같은 배치에서도 실행마다 다른 행렬이 나온다.
	TArray<const FZoneAnchor*> Sorted;
	Sorted.Reserve(Anchors.Num());
	for (const FZoneAnchor& Anchor : Anchors)
	{
		if (Anchor.ZoneId.IsNone())
		{
			continue;
		}
		// 같은 ZoneId가 두 번 들어오면 뒤엣것을 버린다. 중복은 등록 쪽 결함이지만
		// 여기서 받아 주면 행렬 인덱스가 어긋나 엉뚱한 존의 거리가 나온다.
		const bool bDuplicate = Sorted.ContainsByPredicate(
			[&Anchor](const FZoneAnchor* Existing) { return Existing->ZoneId == Anchor.ZoneId; });
		if (bDuplicate)
		{
			continue;
		}
		Sorted.Add(&Anchor);
	}

	Sorted.Sort([](const FZoneAnchor& A, const FZoneAnchor& B)
	{
		return A.ZoneId.Compare(B.ZoneId) < 0;
	});

	const int32 N = Sorted.Num();
	if (N == 0)
	{
		return;
	}

	ZoneIds.Reserve(N);
	AnchorsCm.Reserve(N);
	for (const FZoneAnchor* Anchor : Sorted)
	{
		ZoneIds.Add(Anchor->ZoneId);
		AnchorsCm.Add(Anchor->WorldLocationCm);
	}

	// ── 2. 전 쌍 질의 ─────────────────────────────────────────────────
	// 자기 자신은 0으로 두고 나머지는 실제로 잰다. 비대칭을 유지하므로 N(N-1)회다.
	DistancesCm.Init(NoPath, N * N);

	int32 Queries = 0;
	int32 Failed = 0;

	for (int32 From = 0; From < N; ++From)
	{
		DistancesCm[From * N + From] = 0.f;

		for (int32 To = 0; To < N; ++To)
		{
			if (From == To)
			{
				continue;
			}

			float LengthCm = 0.f;
			++Queries;

			if (PathLength(AnchorsCm[From], AnchorsCm[To], LengthCm) && LengthCm >= 0.f)
			{
				DistancesCm[From * N + To] = LengthCm;
			}
			else
			{
				++Failed;
			}
		}
	}

	// ── 3. 존 그래프 ↔ NavMesh 연결성 대조 ────────────────────────────
	// 존 그래프는 후보 필터링에 쓰인다(인접 존 밖은 후보가 못 된다). 그래프상 이웃인데
	// 걸어갈 수 없으면 "후보에는 들어오는데 도달할 수 없는 시설"이 생긴다 —
	// 그 상태에서 파티는 목적지를 받고 걸어가려 하다가 도착하지 못한다.
	// 조용히 두면 "가끔 안 오는 손님"으로만 보이므로 빌드 시점에 잡는다.
	if (OutStats)
	{
		for (int32 From = 0; From < N; ++From)
		{
			for (const FName& Neighbour : Sorted[From]->NeighborZoneIds)
			{
				const int32 To = IndexOfZone(Neighbour);
				if (To == INDEX_NONE)
				{
					OutStats->Mismatches.Add(FString::Printf(
						TEXT("%s의 이웃 %s가 등록되지 않았다"),
						*ZoneIds[From].ToString(), *Neighbour.ToString()));
					continue;
				}

				if (DistancesCm[From * N + To] < 0.f)
				{
					OutStats->Mismatches.Add(FString::Printf(
						TEXT("%s -> %s: 존 그래프는 이웃인데 NavMesh 경로가 없다"),
						*ZoneIds[From].ToString(), *ZoneIds[To].ToString()));
				}
			}
		}

		OutStats->ZoneCount = N;
		OutStats->QueryCount = Queries;
		OutStats->FailedPairs = Failed;
	}
}

int32 FFF_WalkDistanceMatrix::IndexOfZone(FName ZoneId) const
{
	// 사전순 정렬을 유지하므로 이분 탐색이 가능하지만, N이 한 자리 수라 선형 탐색이 더 빠르다.
	// (FName 비교는 인덱스 비교라 싸다)
	return ZoneIds.IndexOfByKey(ZoneId);
}

bool FFF_WalkDistanceMatrix::TryGetWalkDistanceCm(FName FromZoneId, FName ToZoneId, float& OutLengthCm) const
{
	const int32 From = IndexOfZone(FromZoneId);
	const int32 To = IndexOfZone(ToZoneId);
	if (From == INDEX_NONE || To == INDEX_NONE)
	{
		return false;
	}

	const float Stored = DistancesCm[From * ZoneIds.Num() + To];
	if (Stored < 0.f)
	{
		return false;	// 경로 없음. 호출자가 직선거리로 폴백한다
	}

	OutLengthCm = Stored;
	return true;
}

bool FFF_WalkDistanceMatrix::TryGetZoneAnchorCm(FName ZoneId, FVector& OutWorldLocationCm) const
{
	const int32 Index = IndexOfZone(ZoneId);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	OutWorldLocationCm = AnchorsCm[Index];
	return true;
}
