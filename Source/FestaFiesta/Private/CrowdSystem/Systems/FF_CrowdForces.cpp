// Fill out your copyright notice in the Description page of Project Settings.

#include "CrowdSystem/Systems/FF_CrowdForces.h"
#include "CrowdSystem/Systems/FF_CrowdSpatialGrid.h"
#include "CrowdSystem/Systems/FF_CrowdSteering.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "Async/ParallelFor.h"
#include "Async/TaskGraphInterfaces.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	// 거리가 이보다 작으면 방향 벡터를 만들 수 없다. 나눗셈을 막기 위한 하한(m).
	constexpr float MinSeparationMeters = 1.e-4f;

	// 파티 응집력 계수(N/m)와 정렬력 계수(N*s/m). 문헌값이 아니라 튜닝값이므로 Config로 뺄 후보다.
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
			return FVector2f(1.f, 0.f);
		}
		return V / OutLength;
	}

	float PointSegmentDistSq(const FVector2f& P, const FVector2f& A, const FVector2f& B)
	{
		const FVector2f AB = B - A;
		const float LenSq = AB.SizeSquared();
		if (LenSq <= KINDA_SMALL_NUMBER)
		{
			return FVector2f::DistSquared(P, A);
		}

		const float T = FMath::Clamp(FVector2f::DotProduct(P - A, AB) / LenSq, 0.f, 1.f);
		return FVector2f::DistSquared(P, A + AB * T);
	}

	bool SegmentsCrossProperly(const FVector2f& P1, const FVector2f& P2, const FVector2f& Q1, const FVector2f& Q2)
	{
		const FVector2f R = P2 - P1;
		const FVector2f S = Q2 - Q1;

		const float D1 = FVector2f::CrossProduct(R, Q1 - P1);
		const float D2 = FVector2f::CrossProduct(R, Q2 - P1);
		const float D3 = FVector2f::CrossProduct(S, P1 - Q1);
		const float D4 = FVector2f::CrossProduct(S, P2 - Q1);

		return ((D1 > 0.f && D2 < 0.f) || (D1 < 0.f && D2 > 0.f))
			&& ((D3 > 0.f && D4 < 0.f) || (D3 < 0.f && D4 > 0.f));
	}

	bool SegmentsHaveClearance(const FVector2f& From, const FVector2f& To,
		const FVector2f& WallA, const FVector2f& WallB, float ClearanceSq)
	{
		return !SegmentsCrossProperly(From, To, WallA, WallB)
			&& PointSegmentDistSq(From, WallA, WallB) > ClearanceSq
			&& PointSegmentDistSq(To, WallA, WallB) > ClearanceSq
			&& PointSegmentDistSq(WallA, From, To) > ClearanceSq
			&& PointSegmentDistSq(WallB, From, To) > ClearanceSq;
	}
}

FVector2f FFF_CrowdForceSolver::ComputeDrivingForce(const FVector2f& DesiredVel, const FVector2f& Vel,
                                                    float MassKg, float Tau)
{
	const float SafeTau = FMath::Max(Tau, KINDA_SMALL_NUMBER);
	return (DesiredVel - Vel) * (MassKg / SafeTau);
}

FFF_CrowdForceTerms FFF_CrowdForceSolver::ComputeAgentInteractionTerms(
	const FVector2f& PosI, const FVector2f& VelI, float RadiusI,
	const FVector2f& PosJ, const FVector2f& VelJ, float RadiusJ,
	const FFF_CrowdAvoidanceConfig& Config,
	float MassKg, float FixedDtSeconds)
{
	float Dist = 0.f;
	const FVector2f N = SafeNormalize(PosI - PosJ, Dist);

	const float RSum = RadiusI + RadiusJ;
	const float Overlap = RSum - Dist;

	const float Arg = FMath::Clamp(Overlap / FMath::Max(Config.RepulsionB, KINDA_SMALL_NUMBER),
	                               -Config.MaxExponentArg, Config.MaxExponentArg);
	float Magnitude = Config.RepulsionA * FMath::Exp(Arg);

	float SpeedI = 0.f;
	const FVector2f HeadingI = SafeNormalize(VelI, SpeedI);
	if (SpeedI > MinSeparationMeters)
	{
		const float CosPhi = FVector2f::DotProduct(HeadingI, -N);
		const float W = Config.Anisotropy + (1.f - Config.Anisotropy) * 0.5f * (1.f + CosPhi);
		Magnitude *= W;
	}

	FFF_CrowdForceTerms Terms;

	Terms.Social = N * Magnitude;

	if (Overlap > 0.f)
	{
		Terms.Contact += N * (Config.ContactStiffness * Overlap);

		const FVector2f T(-N.Y, N.X);
		const float RelTangential = FVector2f::DotProduct(VelJ - VelI, T);

		const float Friction = Config.ContactFriction * Overlap * RelTangential;
		const float MaxFriction = 0.5f * FMath::Max(MassKg, KINDA_SMALL_NUMBER)
			* FMath::Abs(RelTangential) / FMath::Max(FixedDtSeconds, KINDA_SMALL_NUMBER);

		Terms.Contact += T * FMath::Clamp(Friction, -MaxFriction, MaxFriction);
	}

	return Terms;
}

FFF_CrowdForceTerms FFF_CrowdForceSolver::ComputeWallForceTerms(
	const FVector2f& Pos, const FVector2f& Vel, float Radius,
	const FVector2f& SegA, const FVector2f& SegB,
	const FFF_CrowdAvoidanceConfig& Config,
	float MassKg, float FixedDtSeconds)
{
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

	FFF_CrowdForceTerms Terms;
	Terms.Social = N * (Config.WallRepulsionA * FMath::Exp(Arg));

	if (Overlap > 0.f)
	{
		Terms.Contact += N * (Config.ContactStiffness * Overlap);

		const FVector2f T(-N.Y, N.X);
		const float RelTangential = -FVector2f::DotProduct(Vel, T);

		const float Friction = Config.ContactFriction * Overlap * RelTangential;
		const float MaxFriction = FMath::Max(MassKg, KINDA_SMALL_NUMBER)
			* FMath::Abs(RelTangential) / FMath::Max(FixedDtSeconds, KINDA_SMALL_NUMBER);

		Terms.Contact += T * FMath::Clamp(Friction, -MaxFriction, MaxFriction);
	}

	return Terms;
}

FVector2f FFF_CrowdForceSolver::ComputeLaneBias(const FVector2f& PosI, float RadiusI,
                                                const FVector2f& PosJ, float RadiusJ,
                                                const FVector2f& DesiredVelI, const FVector2f& DesiredVelJ,
                                                const FFF_CrowdAvoidanceConfig& Config)
{
	if (Config.LaneBiasGain <= 0.f)
	{
		return FVector2f::ZeroVector;
	}

	float SpeedI = 0.f;
	const FVector2f DirI = SafeNormalize(DesiredVelI, SpeedI);
	if (SpeedI < Config.LaneBiasMinDesiredSpeed)
	{
		return FVector2f::ZeroVector;
	}

	float SpeedJ = 0.f;
	const FVector2f DirJ = SafeNormalize(DesiredVelJ, SpeedJ);
	if (SpeedJ < Config.LaneBiasMinDesiredSpeed)
	{
		return FVector2f::ZeroVector;
	}

	const float Opposition = -FVector2f::DotProduct(DirI, DirJ);
	if (Opposition <= 0.f)
	{
		return FVector2f::ZeroVector;
	}

	float Dist = 0.f;
	const FVector2f N = SafeNormalize(PosI - PosJ, Dist);

	const float Ahead = FVector2f::DotProduct(DirI, -N);
	if (Ahead <= 0.f)
	{
		return FVector2f::ZeroVector;
	}

	const float Overlap = (RadiusI + RadiusJ) - Dist;
	const float Arg = FMath::Clamp(Overlap / FMath::Max(Config.RepulsionB, KINDA_SMALL_NUMBER),
	                               -Config.MaxExponentArg, Config.MaxExponentArg);
	const float Magnitude = Config.RepulsionA * FMath::Exp(Arg) * Config.LaneBiasGain * Ahead * Opposition;

	const FVector2f RightI(-DirI.Y, DirI.X);

	return RightI * Magnitude;
}

FVector2f FFF_CrowdForceSolver::ComputeGroupForce(const FVector2f& Pos, const FVector2f& Vel,
                                                  const FVector2f& Centroid, const FVector2f& AvgVel,
                                                  float CohesionRadiusMeters)
{
	FVector2f Force = FVector2f::ZeroVector;

	float Dist = 0.f;
	const FVector2f ToCentroid = SafeNormalize(Centroid - Pos, Dist);
	if (Dist > CohesionRadiusMeters)
	{
		Force += ToCentroid * (GroupCohesionGain * (Dist - CohesionRadiusMeters));
	}

	Force += (AvgVel - Vel) * GroupAlignmentGain;

	return Force;
}

FVector2f FFF_CrowdForceSolver::CapOpposingAccel(const FVector2f& Accel, const FVector2f& DesiredDir,
                                                 float MaxOpposingAccelMps2)
{
	if (MaxOpposingAccelMps2 < 0.f)
	{
		return Accel;
	}

	const float DirLenSq = DesiredDir.SizeSquared();
	if (DirLenSq < KINDA_SMALL_NUMBER)
	{
		return Accel;
	}

	const FVector2f Dir = DesiredDir * (1.f / FMath::Sqrt(DirLenSq));

	const float Along = FVector2f::DotProduct(Accel, Dir);
	if (Along >= 0.f)
	{
		return Accel;
	}

	const FVector2f Perpendicular = Accel - Dir * Along;

	return Perpendicular + Dir * FMath::Max(Along, -MaxOpposingAccelMps2);
}

FVector2f FFF_CrowdForceSolver::ClampAcceleration(const FVector2f& Accel, float MaxAccelMps2)
{
	const float Mag = Accel.Size();
	if (Mag > MaxAccelMps2 && Mag > KINDA_SMALL_NUMBER)
	{
		return Accel * (MaxAccelMps2 / Mag);
	}
	return Accel;
}

FVector2f FFF_CrowdForceSolver::ComputeUnstuckImpulse(int32 StableAgentId, const FVector2f& SuggestedDir)
{
	const uint32 Hash = HashCombine(GetTypeHash(StableAgentId), 0x9E3779B9u);
	const float Frac = static_cast<float>(Hash & 0xFFFFu) / 65535.f;

	float Len = 0.f;
	FVector2f Base = SafeNormalize(SuggestedDir, Len);
	if (Len <= MinSeparationMeters)
	{
		const float FullAngle = Frac * 2.f * PI;
		return FVector2f(FMath::Cos(FullAngle), FMath::Sin(FullAngle));
	}

	const float AngleRad = (Frac * 2.f - 1.f) * (PI / 3.f);
	const float C = FMath::Cos(AngleRad);
	const float S = FMath::Sin(AngleRad);
	return FVector2f(Base.X * C - Base.Y * S, Base.X * S + Base.Y * C);
}

void FFF_CrowdWalls::AppendOrientedRect(TArray<FFF_CrowdWallSegment>& OutWalls,
                                        const FVector2f& CenterMeters, float YawRadians,
                                        float HalfExtentXMeters, float HalfExtentYMeters)
{
	if (HalfExtentXMeters <= KINDA_SMALL_NUMBER || HalfExtentYMeters <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float S = FMath::Sin(YawRadians);
	const float C = FMath::Cos(YawRadians);

	const FVector2f Local[4] = {
		FVector2f(-HalfExtentXMeters, -HalfExtentYMeters),
		FVector2f( HalfExtentXMeters, -HalfExtentYMeters),
		FVector2f( HalfExtentXMeters,  HalfExtentYMeters),
		FVector2f(-HalfExtentXMeters,  HalfExtentYMeters)
	};

	FVector2f World[4];
	for (int32 i = 0; i < 4; ++i)
	{
		World[i] = CenterMeters + FVector2f(
			Local[i].X * C - Local[i].Y * S,
			Local[i].X * S + Local[i].Y * C);
	}

	OutWalls.Reserve(OutWalls.Num() + 4);
	for (int32 i = 0; i < 4; ++i)
	{
		FFF_CrowdWallSegment& Wall = OutWalls.AddDefaulted_GetRef();
		Wall.A = World[i];
		Wall.B = World[(i + 1) % 4];
	}
}

bool FFF_CrowdWalls::IsSegmentClear(const TArray<FFF_CrowdWallSegment>& Walls,
                                    const FVector2f& FromMeters, const FVector2f& ToMeters, float ClearanceMeters)
{
	const float Clearance = FMath::Max(ClearanceMeters, 0.f);
	const float ClearanceSq = FMath::Square(Clearance);

	const float MinX = FMath::Min(FromMeters.X, ToMeters.X) - Clearance;
	const float MaxX = FMath::Max(FromMeters.X, ToMeters.X) + Clearance;
	const float MinY = FMath::Min(FromMeters.Y, ToMeters.Y) - Clearance;
	const float MaxY = FMath::Max(FromMeters.Y, ToMeters.Y) + Clearance;

	for (const FFF_CrowdWallSegment& Wall : Walls)
	{
		if (FMath::Max(Wall.A.X, Wall.B.X) < MinX || FMath::Min(Wall.A.X, Wall.B.X) > MaxX
			|| FMath::Max(Wall.A.Y, Wall.B.Y) < MinY || FMath::Min(Wall.A.Y, Wall.B.Y) > MaxY)
		{
			continue;
		}

		if (!SegmentsHaveClearance(FromMeters, ToMeters, Wall.A, Wall.B, ClearanceSq))
		{
			return false;
		}
	}

	return true;
}

FVector2f FFF_CrowdForceSolver::ComposeAcceleration(
	const FFF_CrowdForceTerms& AgentTerms, const FFF_CrowdForceTerms& WallTerms,
	const FVector2f& Vel, const FVector2f& DesiredVel,
	bool bHasDesired, bool bDrivingForce, float MassKg,
	const FFF_CrowdAvoidanceConfig& Config)
{
	const float SafeMass = FMath::Max(MassKg, KINDA_SMALL_NUMBER);

	FVector2f Accel = ClampAcceleration(AgentTerms.Social / SafeMass, Config.MaxSocialAccelMps2);

	if (bHasDesired && Config.MaxOpposingAccelMps2 >= 0.f)
	{
		Accel = CapOpposingAccel(Accel, DesiredVel, Config.MaxOpposingAccelMps2);
	}

	FVector2f WallAccel = ClampAcceleration(WallTerms.Social / SafeMass, Config.MaxSocialAccelMps2);
	if (bHasDesired && Config.MaxWallOpposingAccelMps2 >= 0.f)
	{
		WallAccel = CapOpposingAccel(WallAccel, DesiredVel, Config.MaxWallOpposingAccelMps2);
	}
	Accel += WallAccel;

	Accel += (AgentTerms.Contact + WallTerms.Contact) / SafeMass;

	if (bHasDesired && bDrivingForce)
	{
		Accel += ComputeDrivingForce(DesiredVel, Vel, SafeMass, Config.RelaxationTau) / SafeMass;
	}

	return Accel;
}

void FFF_CrowdForceSolver::ComputeAll(const TArray<FUnitData>& Units, const FFF_CrowdForceInput& In,
                                      const FFF_CrowdSpatialGrid& Grid, const TArray<FFF_CrowdWallSegment>& Walls,
                                      const FFF_CrowdAvoidanceConfig& Config,
                                      TArray<FVector2f>& OutAccelMps2, TArray<int32>* OutNeighbourCount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CrowdSim_ComputeForces);

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

	const bool bHasDesired = In.HasDesiredVelocities();
	const bool bUseDrivingForce = In.HasDrivingForce();
	const bool bUseLaneBias = bHasDesired && (Config.LaneBiasGain > 0.f);
	const bool bCullWalls = (Config.WallCullRadiusMeters > 0.f);
	const TArray<FVector2f>* DesiredVel = In.DesiredVelMeters;
	const TArray<float>* SocialScale = In.SocialScale;

	const int32 N = ActiveIds.Num();
	if (N == 0)
	{
		return;
	}

	const int32 NumWorkers = FMath::Max(1, FTaskGraphInterface::Get().GetNumWorkerThreads());
	const int32 NumChunks = FMath::Clamp(NumWorkers, 1, FMath::Max(1, N / MinAgentsPerChunk));
	NeighbourScratch.SetNum(NumChunks);

	const float SearchRadius = Config.NeighbourRadiusMeters;
	const float FixedDt = In.FixedDtSeconds;
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

			const float MassKg = FMath::Max(U.MassKg, KINDA_SMALL_NUMBER);

			FFF_CrowdForceTerms Terms;

			Grid.GatherNearest(Pos, Pos[i], SearchRadius, Config.MaxNeighbours + 1, Neighbours);

			int32 NeighbourCount = 0;
			for (const int32 j : Neighbours)
			{
				if (j == i)
				{
					continue;
				}
				FFF_CrowdForceTerms Interaction = ComputeAgentInteractionTerms(
					Pos[i], Vel[i], Radius[i],
					Pos[j], Vel[j], Radius[j],
					Config, MassKg, FixedDt);
				// 서로 다른, 몸이 겹치지 않는 자리를 배정받은 사람끼리는 가까이 설 수 있다.
				// 일반 보행자의 개인 공간을 그대로 적용하면 90cm 자리 밖에서 평형을 이룬다.
				// 접촉력은 유지하므로 걸어가는 동안 몸을 통과할 수는 없다.
				const bool bCompatibleAssignedSlots = CrowdSteering::IsApproachingAssignedSlot(U)
					&& Units.IsValidIndex(j) && CrowdSteering::IsApproachingAssignedSlot(Units[j])
					&& FVector::DistSquared2D(U.QueueSlotLocation, Units[j].QueueSlotLocation)
						>= FMath::Square(U.Radius + Units[j].Radius
							+ U.ArrivalRadiusUU + Units[j].ArrivalRadiusUU);
				if (bCompatibleAssignedSlots)
				{
					Interaction.Social = FVector2f::ZeroVector;
				}
				Terms += Interaction;

				if (bUseLaneBias && !bCompatibleAssignedSlots)
				{
					Terms.Social += ComputeLaneBias(
						Pos[i], Radius[i], Pos[j], Radius[j],
						(*DesiredVel)[i], (*DesiredVel)[j], Config);
				}

				++NeighbourCount;
			}

			// 개인 공간만 좁힌다. Contact 는 그대로라 몸은 여전히 안 겹친다.
			const float SocialGain = SocialScale ? (*SocialScale)[i] : 1.f;
			Terms.Social *= SocialGain;

			FFF_CrowdForceTerms WallTerms;
			{
				const float CullDist = (bCullWalls ? (Radius[i] + Config.WallCullRadiusMeters) : 0.f);
				const bool bApproachingAssignedSlot = CrowdSteering::IsApproachingAssignedSlot(U);
				const FVector2f AssignedSlotMeters(U.QueueSlotLocation.X * FFF_CrowdUnits::MetersPerUU,
					U.QueueSlotLocation.Y * FFF_CrowdUnits::MetersPerUU);
				const float SlotClearanceSq = FMath::Square(Radius[i]
					+ U.ArrivalRadiusUU * FFF_CrowdUnits::MetersPerUU);

				for (const FFF_CrowdWallSegment& Wall : Walls)
				{
					if (bCullWalls)
					{
						const float MinX = FMath::Min(Wall.A.X, Wall.B.X) - CullDist;
						const float MaxX = FMath::Max(Wall.A.X, Wall.B.X) + CullDist;
						const float MinY = FMath::Min(Wall.A.Y, Wall.B.Y) - CullDist;
						const float MaxY = FMath::Max(Wall.A.Y, Wall.B.Y) + CullDist;

						if (Pos[i].X < MinX || Pos[i].X > MaxX || Pos[i].Y < MinY || Pos[i].Y > MaxY)
						{
							continue;
						}
					}

					FFF_CrowdForceTerms WallInteraction = ComputeWallForceTerms(Pos[i], Vel[i], Radius[i], Wall.A, Wall.B,
						Config, MassKg, FixedDt);
					// 벽 밖 40cm의 유효한 자리도 일반 벽 반발과 감속이 균형을 이루면 도착하지 못한다.
					// 몸체·도착 오차를 포함한 접근 선분 전체가 이 벽을 피할 때만 개인 공간을 줄인다.
					// 벽 너머의 잘못된 목표와 실제 접촉은 기존 힘을 그대로 받는다.
					if (bApproachingAssignedSlot && SegmentsHaveClearance(Pos[i], AssignedSlotMeters,
						Wall.A, Wall.B, SlotClearanceSq))
					{
						WallInteraction.Social = FVector2f::ZeroVector;
					}
					WallTerms += WallInteraction;
				}
			}

			const FVector2f Desired = bHasDesired ? (*DesiredVel)[i] : FVector2f::ZeroVector;

			FVector2f Accel = ComposeAcceleration(Terms, WallTerms, Vel[i], Desired,
				bHasDesired, bUseDrivingForce, MassKg, Config);

			if (bUnstuckEnabled && !CrowdSteering::IsApproachingAssignedSlot(U))
			{
				const int32 HoldMax = FMath::Max(1, Config.UnstuckHoldSteps);
				const float HoldScale = (U.StuckSteps >= Config.StuckDetectSteps)
					? 1.f
					: static_cast<float>(FMath::Min(U.UnstuckHoldSteps, HoldMax)) / static_cast<float>(HoldMax);

				if (HoldScale > 0.f && SocialGain > 0.f)
				{
					FVector2f Suggested = FVector2f::ZeroVector;
					FVector2f CellCenter;
					if (Grid.FindLeastCrowdedNeighbourCell(Pos[i], CellCenter))
					{
						Suggested = CellCenter - Pos[i];
					}
					Accel += ComputeUnstuckImpulse(U.StableAgentID, Suggested)
						* (Config.UnstuckAccelMps2 * HoldScale * SocialGain);
				}
			}

			OutAccelMps2[i] = ClampAcceleration(Accel, Config.MaxAccelerationMps2);
			if (OutNeighbourCount)
			{
				(*OutNeighbourCount)[i] = NeighbourCount;
			}
		}
	});
}
