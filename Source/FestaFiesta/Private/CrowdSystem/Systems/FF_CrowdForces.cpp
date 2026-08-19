// Fill out your copyright notice in the Description page of Project Settings.

#include "CrowdSystem/Systems/FF_CrowdForces.h"
#include "CrowdSystem/Systems/FF_CrowdSpatialGrid.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "Async/ParallelFor.h"
#include "Async/TaskGraphInterfaces.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	// 거리가 이보다 작으면 방향 벡터를 만들 수 없다. 나눗셈을 막기 위한 하한(m).
	constexpr float MinSeparationMeters = 1.e-4f;

	// 파티 응집력 계수(N/m)와 정렬력 계수(N*s/m).
	// 문헌값이 아니라 튜닝값이므로 Config로 뺄 후보다.
	constexpr float GroupCohesionGain = 60.f;
	constexpr float GroupAlignmentGain = 20.f;

	// 병렬 청크 하나가 최소한 이 정도 개체는 맡아야 태스크 오버헤드가 상쇄된다.
	constexpr int32 MinAgentsPerChunk = 64;

	FORCEINLINE FVector2f SafeNormalize(const FVector2f& V, float& OutLength)
	{
		OutLength = V.Size();
		if (OutLength < MinSeparationMeters)
		{
			OutLength = MinSeparationMeters;
			// 완전히 겹친 경우의 대체 방향. 여기서 RNG를 쓰면 결정성이 깨지므로
			// 고정 방향을 쓰고, 겹침 해소는 ComputeUnstuckImpulse의 결정론적 지터가 담당한다.
			return FVector2f(1.f, 0.f);
		}
		return V / OutLength;
	}
}

//======================================================================
// 개별 항
//======================================================================

FVector2f FFF_CrowdForceSolver::ComputeDrivingForce(const FVector2f& DesiredVel, const FVector2f& Vel,
                                                    float MassKg, float Tau)
{
	// Helbing 구동력: 목표 속도와 현재 속도의 차이를 tau 안에 메우는 힘.
	// tau가 작을수록 민첩하게 움직인다. 0.5s가 보행자 문헌값이다.
	const float SafeTau = FMath::Max(Tau, KINDA_SMALL_NUMBER);
	return (DesiredVel - Vel) * (MassKg / SafeTau);
}

FVector2f FFF_CrowdForceSolver::ComputeAgentInteraction(
	const FVector2f& PosI, const FVector2f& VelI, float RadiusI,
	const FVector2f& PosJ, const FVector2f& VelJ, float RadiusJ,
	const FFF_CrowdAvoidanceConfig& Config)
{
	float Dist = 0.f;
	// N은 j에서 i를 향한다 — i가 밀려나는 방향.
	const FVector2f N = SafeNormalize(PosI - PosJ, Dist);

	const float RSum = RadiusI + RadiusJ;
	const float Overlap = RSum - Dist;

	//──────────────────────────────────────────────────────────
	// 심리적 반발 — 닿지 않아도 작용한다
	//──────────────────────────────────────────────────────────
	// [중요] exp 인자 클램프. crush 밀도에서 Overlap이 커지면 exp()가 오버플로되어
	// inf가 되고, 다음 스텝에 inf - inf = NaN이 되어 배열 전체로 전파된다.
	// NaN이 퍼진 뒤에는 원인 지점을 역추적할 수 없으므로 사후 검사로는 늦다.
	const float Arg = FMath::Clamp(Overlap / FMath::Max(Config.RepulsionB, KINDA_SMALL_NUMBER),
	                               -Config.MaxExponentArg, Config.MaxExponentArg);
	float Magnitude = Config.RepulsionA * FMath::Exp(Arg);

	//──────────────────────────────────────────────────────────
	// 이방성 — 사람은 뒤에서 오는 사람을 덜 피한다
	//──────────────────────────────────────────────────────────
	// 이게 없으면 군중이 전방향으로 균등하게 퍼져서, 한 방향으로 흐르는 인파가
	// 실제보다 넓게 벌어지고 밀도가 과소평가된다.
	float SpeedI = 0.f;
	const FVector2f HeadingI = SafeNormalize(VelI, SpeedI);
	if (SpeedI > MinSeparationMeters)
	{
		// -N은 i에서 j를 향하는 방향. i의 진행 방향과 이룬 각의 코사인.
		const float CosPhi = FVector2f::DotProduct(HeadingI, -N);
		const float W = Config.Anisotropy + (1.f - Config.Anisotropy) * 0.5f * (1.f + CosPhi);
		Magnitude *= W;
	}

	FVector2f Force = N * Magnitude;

	//──────────────────────────────────────────────────────────
	// 접촉 — 실제로 겹쳤을 때만
	//──────────────────────────────────────────────────────────
	if (Overlap > 0.f)
	{
		// 법선 방향 밀어내기. 관통을 물리적으로 막는 항이다.
		Force += N * (Config.ContactStiffness * Overlap);

		// 접선 마찰. 이게 없으면 밀집 구간에서 사람들이 서로를 미끄러져 지나가
		// 병목에서 아치가 생기지 않는다 — 실제 군중의 특징적 현상이 사라진다.
		const FVector2f T(-N.Y, N.X);
		const float RelTangential = FVector2f::DotProduct(VelJ - VelI, T);
		Force += T * (Config.ContactFriction * Overlap * RelTangential);
	}

	return Force;
}

FVector2f FFF_CrowdForceSolver::ComputeWallForce(const FVector2f& Pos, const FVector2f& Vel, float Radius,
                                                 const FVector2f& SegA, const FVector2f& SegB,
                                                 const FFF_CrowdAvoidanceConfig& Config)
{
	// 선분 위의 최근접점을 구한다. 끝점 밖은 끝점으로 클램프되므로
	// 모서리에서도 자연스럽게 처리된다.
	const FVector2f AB = SegB - SegA;
	const float LenSq = AB.SizeSquared();

	FVector2f Closest = SegA;
	if (LenSq > KINDA_SMALL_NUMBER)
	{
		const float T = FMath::Clamp(FVector2f::DotProduct(Pos - SegA, AB) / LenSq, 0.f, 1.f);
		Closest = SegA + AB * T;
	}

	float Dist = 0.f;
	const FVector2f N = SafeNormalize(Pos - Closest, Dist);

	const float Overlap = Radius - Dist;

	const float Arg = FMath::Clamp(Overlap / FMath::Max(Config.WallRepulsionB, KINDA_SMALL_NUMBER),
	                               -Config.MaxExponentArg, Config.MaxExponentArg);
	FVector2f Force = N * (Config.WallRepulsionA * FMath::Exp(Arg));

	if (Overlap > 0.f)
	{
		Force += N * (Config.ContactStiffness * Overlap);

		// 벽은 정지해 있으므로 상대 속도는 -Vel이다.
		const FVector2f T(-N.Y, N.X);
		const float RelTangential = -FVector2f::DotProduct(Vel, T);
		Force += T * (Config.ContactFriction * Overlap * RelTangential);
	}

	return Force;
}

FVector2f FFF_CrowdForceSolver::ComputeGroupForce(const FVector2f& Pos, const FVector2f& Vel,
                                                  const FVector2f& Centroid, const FVector2f& AvgVel,
                                                  float CohesionRadiusMeters)
{
	FVector2f Force = FVector2f::ZeroVector;

	// 응집: 반경 안에서는 아무 힘도 주지 않는다.
	// 항상 당기면 일행이 한 점으로 뭉쳐서 서로 밀어내는 힘과 영원히 싸운다.
	float Dist = 0.f;
	const FVector2f ToCentroid = SafeNormalize(Centroid - Pos, Dist);
	if (Dist > CohesionRadiusMeters)
	{
		Force += ToCentroid * (GroupCohesionGain * (Dist - CohesionRadiusMeters));
	}

	// 정렬: 일행의 평균 속도를 따라간다. 응집만 있으면 신호등처럼 멈췄다 뭉쳤다 한다.
	Force += (AvgVel - Vel) * GroupAlignmentGain;

	return Force;
}

FVector2f FFF_CrowdForceSolver::ClampAcceleration(const FVector2f& Accel, float MaxAccelMps2)
{
	// 클램프가 없으면 접촉력이 지배하는 순간(밀집 구간) 개체가 한 스텝에서
	// 수 미터를 튀어 벽을 통과한다. 접촉 강성을 낮추는 것으로는 못 막는다 —
	// 낮추면 이번엔 겹침이 생긴다.
	const float Mag = Accel.Size();
	if (Mag > MaxAccelMps2 && Mag > KINDA_SMALL_NUMBER)
	{
		return Accel * (MaxAccelMps2 / Mag);
	}
	return Accel;
}

FVector2f FFF_CrowdForceSolver::ComputeUnstuckImpulse(int32 StableAgentId, const FVector2f& SuggestedDir)
{
	// ID 해시 기반 결정론적 지터.
	// RNG를 쓰면 같은 시드로 돌려도 결과가 달라진다.
	// 그러나 같은 위치에 겹친 두 개체는 서로 다른 방향으로 빠져나가야 하므로
	// 지터 자체는 반드시 필요하다 — ID로 만든다.
	const uint32 Hash = HashCombine(GetTypeHash(StableAgentId), 0x9E3779B9u);
	const float Frac = static_cast<float>(Hash & 0xFFFFu) / 65535.f; // [0,1]

	float Len = 0.f;
	FVector2f Base = SafeNormalize(SuggestedDir, Len);
	if (Len <= MinSeparationMeters)
	{
		// 방향 제안이 없으면 전방향에서 고른다.
		const float FullAngle = Frac * 2.f * PI;
		return FVector2f(FMath::Cos(FullAngle), FMath::Sin(FullAngle));
	}

	// 제안 방향에서 ±60도. 완전히 반대로 보내면 원래 가려던 곳에서 멀어지기만 한다.
	const float AngleRad = (Frac * 2.f - 1.f) * (PI / 3.f);
	const float C = FMath::Cos(AngleRad);
	const float S = FMath::Sin(AngleRad);
	return FVector2f(Base.X * C - Base.Y * S, Base.X * S + Base.Y * C);
}

//======================================================================
// 전체 계산
//======================================================================

void FFF_CrowdForceSolver::ComputeAll(const TArray<FUnitData>& Units, const FFF_CrowdForceInput& In,
                                      const FFF_CrowdSpatialGrid& Grid, const TArray<FFF_CrowdWallSegment>& Walls,
                                      const FFF_CrowdAvoidanceConfig& Config,
                                      TArray<FVector2f>& OutAccelMps2, TArray<int32>* OutNeighbourCount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_ComputeForces);

	// 결과 배열은 매 스텝 통째로 0으로 되돌린다.
	// 부분 갱신으로 두면 퇴장한 개체의 낡은 가속도가 슬롯 재사용 시 새 방문객에게 붙는다.
	OutAccelMps2.Reset();
	OutAccelMps2.SetNumZeroed(Units.Num());
	if (OutNeighbourCount)
	{
		OutNeighbourCount->Reset();
		OutNeighbourCount->SetNumZeroed(Units.Num());
	}

	if (!In.IsValid() || !Grid.IsValid())
	{
		return;
	}

	const TArray<FVector2f>& Pos = *In.PosMeters;
	const TArray<FVector2f>& Vel = *In.VelMeters;
	const TArray<float>& Radius = *In.RadiusMeters;
	const TArray<int32>& ActiveIds = *In.ActiveIds;

	const int32 N = ActiveIds.Num();
	if (N == 0)
	{
		return;
	}

	//──────────────────────────────────────────────────────────
	// 청크 분할.
	// ParallelFor에 N을 그대로 주면 이웃 버퍼를 개체마다 새로 잡아야 한다.
	// 청크 단위로 나누면 버퍼 하나를 청크 전체가 재사용한다.
	//──────────────────────────────────────────────────────────
	const int32 NumWorkers = FMath::Max(1, FTaskGraphInterface::Get().GetNumWorkerThreads());
	const int32 NumChunks = FMath::Clamp(NumWorkers, 1, FMath::Max(1, N / MinAgentsPerChunk));
	NeighbourScratch.SetNum(NumChunks);

	const float SearchRadius = Config.NeighbourRadiusMeters;
	const bool bUnstuckEnabled = (Config.StuckDetectSteps > 0) && (Config.UnstuckAccelMps2 > 0.f);

	ParallelFor(NumChunks, [&](int32 ChunkIdx)
	{
		TArray<int32>& Neighbours = NeighbourScratch[ChunkIdx];

		const int32 Begin = static_cast<int32>((static_cast<int64>(N) * ChunkIdx) / NumChunks);
		const int32 End = static_cast<int32>((static_cast<int64>(N) * (ChunkIdx + 1)) / NumChunks);

		for (int32 k = Begin; k < End; ++k)
		{
			const int32 i = ActiveIds[k];
			if (!Units.IsValidIndex(i) || !Pos.IsValidIndex(i))
			{
				continue;
			}

			const FUnitData& U = Units[i];

			FVector2f TotalForce = FVector2f::ZeroVector;

			//──────────────────────────────────────────────
			// 이웃. MaxNeighbours + 1인 이유는 자기 자신이 결과에 포함되기 때문
			//──────────────────────────────────────────────
			Grid.GatherNearest(Pos, Pos[i], SearchRadius, Config.MaxNeighbours + 1, Neighbours);

			int32 NeighbourCount = 0;
			for (const int32 j : Neighbours)
			{
				if (j == i)
				{
					continue;
				}
				TotalForce += ComputeAgentInteraction(
					Pos[i], Vel[i], Radius[i],
					Pos[j], Vel[j], Radius[j],
					Config);
				++NeighbourCount;
			}

			//──────────────────────────────────────────────
			// 벽
			//──────────────────────────────────────────────
			for (const FFF_CrowdWallSegment& Wall : Walls)
			{
				TotalForce += ComputeWallForce(Pos[i], Vel[i], Radius[i], Wall.A, Wall.B, Config);
			}

			//──────────────────────────────────────────────
			// 힘(N) -> 가속도(m/s^2). 질량으로 나누는 지점은 여기 한 곳이다
			//──────────────────────────────────────────────
			const float MassKg = FMath::Max(U.MassKg, KINDA_SMALL_NUMBER);
			FVector2f Accel = TotalForce / MassKg;

			//──────────────────────────────────────────────
			// 막힘 해소. 힘이 아니라 가속도로 더한다 —
			// 탈출 세기를 개인 질량과 무관하게 두고 싶기 때문이다
			//──────────────────────────────────────────────
			if (bUnstuckEnabled && U.StuckSteps >= Config.StuckDetectSteps)
			{
				FVector2f Suggested = FVector2f::ZeroVector;
				FVector2f CellCenter;
				if (Grid.FindLeastCrowdedNeighbourCell(Pos[i], CellCenter))
				{
					Suggested = CellCenter - Pos[i];
				}
				Accel += ComputeUnstuckImpulse(U.StableAgentID, Suggested) * Config.UnstuckAccelMps2;
			}

			// [규약] 쓰기는 자기 인덱스에만.
			// Out[j] -= f 같은 대칭 최적화를 넣으면 데이터 레이스이자 결정성 파괴다.
			OutAccelMps2[i] = ClampAcceleration(Accel, Config.MaxAccelerationMps2);
			if (OutNeighbourCount)
			{
				(*OutNeighbourCount)[i] = NeighbourCount;
			}
		}
	});
}
