// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/Systems/FF_CrowdDensityGrid.h"

#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "DrawDebugHelpers.h"

namespace CrowdDensity
{
	static constexpr float UUToMeters = 0.01f;

	static constexpr float LOS_A_Max = 0.31f;
	static constexpr float LOS_B_Max = 0.43f;
	static constexpr float LOS_C_Max = 0.72f;
	static constexpr float LOS_D_Max = 1.08f;
	static constexpr float LOS_E_Max = 2.17f;
}

void FCrowdDensityGrid::Build(const FVector& InOriginUU, const FVector2D& InSizeUU, float InCellSizeUU)
{
	CellSizeUU = FMath::Max(InCellSizeUU, 10.f);
	OriginUU = InOriginUU;

	// 반올림이 아니라 올림이다. 내림하면 그리드 오른쪽/위쪽 끝의 자투리 구역이 통째로 빠져서
	// 그 구역 인원이 아무 셀에도 집계되지 않는다.
	NumX = FMath::Max(1, FMath::CeilToInt(FMath::Abs(InSizeUU.X) / CellSizeUU));
	NumY = FMath::Max(1, FMath::CeilToInt(FMath::Abs(InSizeUU.Y) / CellSizeUU));

	const float CellSizeMeters = CellSizeUU * CrowdDensity::UUToMeters;
	CellAreaSqM = CellSizeMeters * CellSizeMeters;

	const int32 NumCells = NumX * NumY;
	CellCounts.Init(0, NumCells);
	CellThroughput.Init(0, NumCells);
	Density.Init(0.f, NumCells);

	// 격자를 다시 지으면 셀 인덱스 공간 자체가 달라진다. 직전 셀 기억을 남기면
	// 옛 인덱스가 새 격자의 엉뚱한 셀을 가리켜 유량이 한 번 크게 튄다.
	LastCellIndexByAgent.Reset();

	MaxDensity = 0.f;
	CongestedCellCount = 0;
	SampledUnitCount = 0;

	UE_LOG(LogTemp, Log,
		TEXT("[CrowdDensityGrid] Built %dx%d cells, cell %.0fUU (%.2f m^2/cell), %d cells total"),
		NumX, NumY, CellSizeUU, CellAreaSqM, NumCells);
}

void FCrowdDensityGrid::Reset()
{
	for (int32& Count : CellCounts)
	{
		Count = 0;
	}
	for (float& Value : Density)
	{
		Value = 0.f;
	}

	// Reset은 "격자를 비운다"이므로 유량과 직전 셀 기억까지 지운다.
	// 구간만 끊는 것은 ResetThroughputWindow다 — 둘을 섞지 말 것.
	ResetThroughputWindow();
	LastCellIndexByAgent.Reset();

	MaxDensity = 0.f;
	CongestedCellCount = 0;
	SampledUnitCount = 0;
}

void FCrowdDensityGrid::ResetThroughputWindow()
{
	for (int32& Value : CellThroughput)
	{
		Value = 0;
	}
}

int32 FCrowdDensityGrid::WorldToCellIndex(const FVector& WorldLocation) const
{
	if (!IsBuilt())
	{
		return INDEX_NONE;
	}

	const int32 X = FMath::FloorToInt((WorldLocation.X - OriginUU.X) / CellSizeUU);
	const int32 Y = FMath::FloorToInt((WorldLocation.Y - OriginUU.Y) / CellSizeUU);

	if (X < 0 || X >= NumX || Y < 0 || Y >= NumY)
	{
		return INDEX_NONE;
	}

	return Y * NumX + X;
}

void FCrowdDensityGrid::Update(const TArray<FUnitData>& Units)
{
	if (!IsBuilt())
	{
		return;
	}

	// 1) 셀 카운트 초기화 후 다시 센다.
	//    누적하지 않고 매 스텝 새로 세는 이유: 밀도는 "지금 이 순간 몇 명이 있느냐"이지 누적량이 아니다.
	for (int32& Count : CellCounts)
	{
		Count = 0;
	}

	SampledUnitCount = 0;

	// 직전 셀 기억은 AgentIndex로 인덱싱하므로 풀과 크기를 맞춘다.
	// 풀이 커졌을 때만 늘어나고, 늘어난 자리는 INDEX_NONE이다(첫 등장 = 진입).
	if (LastCellIndexByAgent.Num() != Units.Num())
	{
		LastCellIndexByAgent.SetNumUninitialized(Units.Num());
		for (int32& Value : LastCellIndexByAgent)
		{
			Value = INDEX_NONE;
		}
	}

	for (int32 AgentIndex = 0; AgentIndex < Units.Num(); ++AgentIndex)
	{
		const FUnitData& Unit = Units[AgentIndex];

		if (!Unit.bIsActive)
		{
			// 퇴장한 슬롯의 기억을 지운다. 남기면 슬롯이 재사용될 때
			// 퇴장자의 마지막 셀에서 새 방문객이 '이동해 온' 것으로 잡힌다.
			LastCellIndexByAgent[AgentIndex] = INDEX_NONE;
			continue;
		}

		const int32 CellIndex = WorldToCellIndex(Unit.Position);
		if (CellIndex == INDEX_NONE)
		{
			// 그리드 밖. 집계에서 빠진다 — GetSampledUnitCount가 활성 인원보다 적다면
			// 그리드가 실제 이동 범위를 못 덮고 있다는 뜻이다.
			// 기억도 비운다. 밖에 나갔다 돌아오면 그건 새 진입이다.
			LastCellIndexByAgent[AgentIndex] = INDEX_NONE;
			continue;
		}

		++CellCounts[CellIndex];
		++SampledUnitCount;

		// --- 유량: 셀이 바뀐 순간에만 목적 셀에서 1 올린다 -----------------------------
		// 프레임마다 세지 않고 '전이'를 세기 때문에 프레임률이 값을 부풀리지 않는다.
		// 제자리에 서 있는 사람은 0으로 남고, 그래서
		// "Occupancy 높음 + Throughput 0 = 막힘"이라는 판정이 성립한다.
		if (LastCellIndexByAgent[AgentIndex] != CellIndex)
		{
			++CellThroughput[CellIndex];
			LastCellIndexByAgent[AgentIndex] = CellIndex;
		}
	}

	// 2) 카운트를 밀도로 환산하고 요약 통계를 뽑는다.
	MaxDensity = 0.f;
	CongestedCellCount = 0;

	for (int32 i = 0; i < CellCounts.Num(); ++i)
	{
		const float CellDensity = static_cast<float>(CellCounts[i]) / CellAreaSqM;
		Density[i] = CellDensity;

		if (CellDensity > MaxDensity)
		{
			MaxDensity = CellDensity;
		}

		if (CellDensity > CrowdDensity::LOS_D_Max)
		{
			++CongestedCellCount;
		}
	}
}

float FCrowdDensityGrid::GetDensityAtLocation(const FVector& WorldLocation) const
{
	const int32 CellIndex = WorldToCellIndex(WorldLocation);
	return Density.IsValidIndex(CellIndex) ? Density[CellIndex] : 0.f;
}

ECrowdLOS FCrowdDensityGrid::GetLOSAtLocation(const FVector& WorldLocation) const
{
	return DensityToLOS(GetDensityAtLocation(WorldLocation));
}

FVector FCrowdDensityGrid::GetCellCenterWorld(int32 X, int32 Y) const
{
	return FVector(
		OriginUU.X + (X + 0.5f) * CellSizeUU,
		OriginUU.Y + (Y + 0.5f) * CellSizeUU,
		OriginUU.Z);
}

ECrowdLOS FCrowdDensityGrid::DensityToLOS(float DensityPerSqM)
{
	if (DensityPerSqM < CrowdDensity::LOS_A_Max) { return ECrowdLOS::A; }
	if (DensityPerSqM < CrowdDensity::LOS_B_Max) { return ECrowdLOS::B; }
	if (DensityPerSqM < CrowdDensity::LOS_C_Max) { return ECrowdLOS::C; }
	if (DensityPerSqM < CrowdDensity::LOS_D_Max) { return ECrowdLOS::D; }
	if (DensityPerSqM < CrowdDensity::LOS_E_Max) { return ECrowdLOS::E; }
	return ECrowdLOS::F;
}

FColor FCrowdDensityGrid::LOSToColor(ECrowdLOS LOS)
{
	switch (LOS)
	{
	case ECrowdLOS::A: return FColor(0, 200, 0);
	case ECrowdLOS::B: return FColor(140, 220, 0);
	case ECrowdLOS::C: return FColor(230, 230, 0);
	case ECrowdLOS::D: return FColor(255, 160, 0);
	case ECrowdLOS::E: return FColor(255, 70, 0);
	case ECrowdLOS::F: return FColor(200, 0, 0);
	default: return FColor::White;
	}
}

void FCrowdDensityGrid::DrawDebug(const UWorld* World, float ZOffsetUU, float MinDensityToDraw) const
{
	if (!World || !IsBuilt())
	{
		return;
	}

	const float HalfCell = CellSizeUU * 0.5f;
	const FVector BoxExtent(HalfCell, HalfCell, 2.f);

	for (int32 Y = 0; Y < NumY; ++Y)
	{
		for (int32 X = 0; X < NumX; ++X)
		{
			const float CellDensity = Density[Y * NumX + X];
			if (CellDensity <= MinDensityToDraw)
			{
				continue;
			}

			FVector Center = GetCellCenterWorld(X, Y);
			Center.Z += ZOffsetUU;

			FColor Color = LOSToColor(DensityToLOS(CellDensity));
			Color.A = 120;

			// bPersistentLines=false, LifeTime=-1 이면 이번 프레임만 남는다.
			// 매 프레임 다시 그리므로 누적시키면 안 된다.
			DrawDebugSolidBox(World, Center, BoxExtent, Color, false, -1.f, 0);
		}
	}
}
