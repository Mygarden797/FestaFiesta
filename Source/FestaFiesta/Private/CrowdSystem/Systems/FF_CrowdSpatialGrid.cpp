// Fill out your copyright notice in the Description page of Project Settings.

#include "CrowdSystem/Systems/FF_CrowdSpatialGrid.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	// 격자 셀 수 상한. 개체가 맵 밖으로 튀어 경계가 폭발했을 때
	// 수 GB를 할당하고 죽는 것을 막는다. 넘으면 셀 크기를 키워 대응한다.
	constexpr int64 MaxGridCells = 4 * 1024 * 1024;
}

void FFF_CrowdSpatialGrid::Build(const TArray<FVector2f>& Pos, const TArray<uint8>& Active,
                                 const TArray<int32>& ActiveIds, float CellSize)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_Broadphase_Build);

	CellStart.Reset();
	CellItems.Reset();
	GridDimX = 0;
	GridDimY = 0;
	CellSizeMeters = 0.f;

	if (CellSize <= 0.f || ActiveIds.Num() == 0)
	{
		// 무효 상태로 두고 반환한다. IsValid()가 false이므로 GatherNearest가
		// 즉시 빈 결과를 낸다 — 여기서 어정쩡하게 진행하면 음수 인덱스 접근이 난다.
		return;
	}

	//──────────────────────────────────────────────────────────────
	// 1) 활성 개체의 경계 상자를 구한다
	//──────────────────────────────────────────────────────────────
	float MinX = TNumericLimits<float>::Max();
	float MinY = TNumericLimits<float>::Max();
	float MaxX = TNumericLimits<float>::Lowest();
	float MaxY = TNumericLimits<float>::Lowest();
	int32 LiveCount = 0;

	for (const int32 Id : ActiveIds)
	{
		if (!Pos.IsValidIndex(Id) || !Active.IsValidIndex(Id) || Active[Id] == 0)
		{
			continue;
		}
		const FVector2f& P = Pos[Id];
		// NaN이 하나라도 섞이면 경계가 통째로 오염되어 격자가 무의미해진다.
		// 여기서 걸러내면 힘 계산이 NaN을 만든 시점과 가까운 곳에서 증상이 드러난다.
		if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y))
		{
			continue;
		}
		MinX = FMath::Min(MinX, P.X);
		MinY = FMath::Min(MinY, P.Y);
		MaxX = FMath::Max(MaxX, P.X);
		MaxY = FMath::Max(MaxY, P.Y);
		++LiveCount;
	}

	if (LiveCount == 0)
	{
		return;
	}

	//──────────────────────────────────────────────────────────────
	// 2) 격자 크기 결정. 셀 하나만큼 패딩해 경계 개체가 범위를 벗어나지 않게 한다
	//──────────────────────────────────────────────────────────────
	float Cell = CellSize;
	GridOrigin = FVector2f(MinX - Cell, MinY - Cell);

	const float SpanX = (MaxX - MinX) + Cell * 2.f;
	const float SpanY = (MaxY - MinY) + Cell * 2.f;

	// 셀 수가 상한을 넘으면 셀을 키운다. 이웃 반경보다 커지는 것은 문제가 없다
	// (더 많은 후보를 보게 될 뿐 정확도는 유지된다).
	int32 DimX = FMath::Max(1, FMath::CeilToInt(SpanX / Cell));
	int32 DimY = FMath::Max(1, FMath::CeilToInt(SpanY / Cell));
	while ((int64)DimX * (int64)DimY > MaxGridCells)
	{
		Cell *= 2.f;
		GridOrigin = FVector2f(MinX - Cell, MinY - Cell);
		DimX = FMath::Max(1, FMath::CeilToInt(((MaxX - MinX) + Cell * 2.f) / Cell));
		DimY = FMath::Max(1, FMath::CeilToInt(((MaxY - MinY) + Cell * 2.f) / Cell));
	}

	CellSizeMeters = Cell;
	GridDimX = DimX;
	GridDimY = DimY;
	const int32 NumCells = DimX * DimY;

	//──────────────────────────────────────────────────────────────
	// 3) 카운팅 정렬 1패스 — 셀별 개수
	//──────────────────────────────────────────────────────────────
	CellCounts.Reset();
	CellCounts.SetNumZeroed(NumCells);

	for (const int32 Id : ActiveIds)
	{
		if (!Pos.IsValidIndex(Id) || !Active.IsValidIndex(Id) || Active[Id] == 0)
		{
			continue;
		}
		if (!FMath::IsFinite(Pos[Id].X) || !FMath::IsFinite(Pos[Id].Y))
		{
			continue;
		}
		int32 CX, CY;
		WorldToCell(Pos[Id], CX, CY);
		++CellCounts[CellIndex(CX, CY)];
	}

	//──────────────────────────────────────────────────────────────
	// 4) 누적합 -> 각 셀의 시작 오프셋
	//──────────────────────────────────────────────────────────────
	CellStart.SetNumUninitialized(NumCells + 1);
	int32 Running = 0;
	for (int32 c = 0; c < NumCells; ++c)
	{
		CellStart[c] = Running;
		Running += CellCounts[c];
	}
	CellStart[NumCells] = Running;

	//──────────────────────────────────────────────────────────────
	// 5) 2패스 — 실제 흘려 넣기.
	//    CellCounts를 쓰기 커서로 재사용해 배열 하나를 아낀다
	//──────────────────────────────────────────────────────────────
	for (int32 c = 0; c < NumCells; ++c)
	{
		CellCounts[c] = CellStart[c];
	}

	CellItems.SetNumUninitialized(Running);
	for (const int32 Id : ActiveIds)
	{
		if (!Pos.IsValidIndex(Id) || !Active.IsValidIndex(Id) || Active[Id] == 0)
		{
			continue;
		}
		if (!FMath::IsFinite(Pos[Id].X) || !FMath::IsFinite(Pos[Id].Y))
		{
			continue;
		}
		int32 CX, CY;
		WorldToCell(Pos[Id], CX, CY);
		CellItems[CellCounts[CellIndex(CX, CY)]++] = Id;
	}
}

void FFF_CrowdSpatialGrid::GatherNearest(const TArray<FVector2f>& Pos, const FVector2f& P,
                                         float Radius, int32 MaxOut, TArray<int32>& OutIndices) const
{
	// 버퍼는 호출자가 재사용한다. Reset은 용량을 유지하므로 할당이 없다.
	OutIndices.Reset();

	if (!IsValid() || MaxOut <= 0 || Radius <= 0.f)
	{
		return;
	}

	const float R2 = Radius * Radius;

	int32 MinCX, MinCY, MaxCX, MaxCY;
	WorldToCell(FVector2f(P.X - Radius, P.Y - Radius), MinCX, MinCY);
	WorldToCell(FVector2f(P.X + Radius, P.Y + Radius), MaxCX, MaxCY);

	for (int32 y = MinCY; y <= MaxCY; ++y)
	{
		for (int32 x = MinCX; x <= MaxCX; ++x)
		{
			const int32 c = CellIndex(x, y);
			const int32 Begin = CellStart[c];
			const int32 End = CellStart[c + 1];
			for (int32 k = Begin; k < End; ++k)
			{
				const int32 j = CellItems[k];
				if ((Pos[j] - P).SizeSquared() <= R2)
				{
					OutIndices.Add(j);
				}
			}
		}
	}

	// 상한을 넘으면 가까운 것부터 남긴다.
	// 먼 이웃의 기여는 exp 감쇠로 이미 무시할 만하므로 정확도 손실이 거의 없다.
	if (OutIndices.Num() > MaxOut)
	{
		OutIndices.Sort([&Pos, &P](int32 A, int32 B)
		{
			const float DA = (Pos[A] - P).SizeSquared();
			const float DB = (Pos[B] - P).SizeSquared();
			// 거리가 같으면 인덱스로 가른다 — 동률에서 순서가 흔들리면
			// 잘려나가는 이웃이 실행마다 달라져 결정성이 깨진다.
			return (DA != DB) ? (DA < DB) : (A < B);
		});
		OutIndices.SetNum(MaxOut);
	}

	// [중요] 최종 반환은 반드시 인덱스 오름차순이다.
	// 같은 이웃 집합이라도 힘을 더하는 순서가 다르면 부동소수 합산의 마지막 비트가
	// 달라지고, 그 차이가 60초 뒤엔 눈에 보이는 위치 차이가 된다.
	OutIndices.Sort();
}

bool FFF_CrowdSpatialGrid::FindLeastCrowdedNeighbourCell(const FVector2f& P, FVector2f& OutCellCenter) const
{
	if (!IsValid())
	{
		return false;
	}

	int32 CX, CY;
	WorldToCell(P, CX, CY);

	int32 BestCount = TNumericLimits<int32>::Max();
	int32 BestX = INDEX_NONE;
	int32 BestY = INDEX_NONE;

	// 순회 순서를 고정하고 동률에서 먼저 만난 쪽을 유지한다(엄격한 <).
	// 그래야 같은 상황에서 항상 같은 방향으로 빠져나간다.
	for (int32 dy = -1; dy <= 1; ++dy)
	{
		for (int32 dx = -1; dx <= 1; ++dx)
		{
			if (dx == 0 && dy == 0)
			{
				continue;
			}
			const int32 X = CX + dx;
			const int32 Y = CY + dy;
			if (X < 0 || X >= GridDimX || Y < 0 || Y >= GridDimY)
			{
				continue;
			}
			const int32 c = CellIndex(X, Y);
			const int32 Count = CellStart[c + 1] - CellStart[c];
			if (Count < BestCount)
			{
				BestCount = Count;
				BestX = X;
				BestY = Y;
			}
		}
	}

	if (BestX == INDEX_NONE)
	{
		return false;
	}

	OutCellCenter = GridOrigin + FVector2f((BestX + 0.5f) * CellSizeMeters, (BestY + 0.5f) * CellSizeMeters);
	return true;
}

void FFF_CrowdSpatialGrid::WorldToCell(const FVector2f& P, int32& OutX, int32& OutY) const
{
	// 클램프가 필수다. 격자를 지은 뒤 개체가 이동해 범위를 벗어날 수 있고,
	// 클램프가 없으면 음수 인덱스로 CellStart를 읽어 즉시 크래시한다.
	const int32 RawX = FMath::FloorToInt((P.X - GridOrigin.X) / CellSizeMeters);
	const int32 RawY = FMath::FloorToInt((P.Y - GridOrigin.Y) / CellSizeMeters);
	OutX = FMath::Clamp(RawX, 0, GridDimX - 1);
	OutY = FMath::Clamp(RawY, 0, GridDimY - 1);
}

int32 FFF_CrowdSpatialGrid::CellIndex(int32 X, int32 Y) const
{
	return Y * GridDimX + X;
}
