// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/Systems/FF_CrowdSteering.h"

#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"

namespace CrowdSteering
{
	float ComputeVelocityAlpha(float SmoothTime, float FixedDt)
	{
		if (SmoothTime <= KINDA_SMALL_NUMBER)
		{
			return 1.f;
		}

		return 1.f - FMath::Exp(-FixedDt / SmoothTime);
	}

	bool HasArrived(const FUnitData& Unit, float ArrivalRadiusUU)
	{
		FVector ToTarget = Unit.TargetLocation - Unit.Position;
		ToTarget.Z = 0.f;

		return ToTarget.SizeSquared() <= FMath::Square(ArrivalRadiusUU);
	}

	FVector ComputeDesiredVelocity(const FUnitData& Unit, const FStepParams& Params)
	{
		FVector ToTarget = Unit.GetSteerTarget() - Unit.Position;
		ToTarget.Z = 0.f;

		if (ToTarget.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}

		const FVector Direction = ToTarget.GetSafeNormal2D();

		const float BrakingDistance = FMath::Max(Params.BrakingDistanceUU, KINDA_SMALL_NUMBER);
		const float SpeedScale = FMath::Clamp(ToTarget.Size() / BrakingDistance, 0.f, 1.f);

		return Direction * (Unit.Speed * SpeedScale);
	}

	void StepAgent(FUnitData& Unit, const FStepParams& Params, float VelocityAlpha, float FixedDt)
	{
		Unit.PrevPosition = Unit.Position;

		FVector ToFinal = Unit.TargetLocation - Unit.Position;
		ToFinal.Z = 0.f;

		FVector ToSteer = Unit.GetSteerTarget() - Unit.Position;
		ToSteer.Z = 0.f;

		if (Params.bDrivingForceInForce)
		{
			// 구동력 경로. Force = 구동 + 회피(둘 다 UU/s^2)이므로 적분 한 번으로 끝난다.
			//
			// Lerp를 걸지 않는다 — 목표 추종이 이미 힘 안에 있어서 완화가 두 번 걸리고,
			// 무엇보다 Lerp가 매 스텝 속도를 희망 속도 쪽으로 되돌려 회피분을 지운다.
			// 그게 "밀려도 곧바로 원래 방향으로 되돌아가 같은 잼에 계속 머리를 박는" 원인이었다.
			Unit.Velocity += Unit.Force * FixedDt;
		}
		else
		{
			// 이식 이전 경로. 목표 추종은 지수 평활이 맡고 Force는 회피분뿐이다.
			const FVector DesiredVelocity = ComputeDesiredVelocity(Unit, Params);

			Unit.Velocity = FMath::Lerp(Unit.Velocity, DesiredVelocity, VelocityAlpha);

			// 회피 가속도(UU/s^2)를 속도에 더한다. 목표로 다가가는 완화(위의 Lerp) 뒤,
			// 속도 상한 앞이다.
			//
			// 순서가 중요하다. Lerp 앞에 더하면 회피분이 곧바로 Lerp에 희석되어 밀어내기가 약해지고,
			// 속도 상한 뒤에 더하면 MaxSpeed를 넘긴 채로 적분해 밀집 구간에서 사람이 튄다.
			//
			// bEnableAvoidance가 false면 Force가 0이고, 0을 더하는 것은 부동소수에서도 무손실이라
			// 회피를 끈 실행은 이식 전과 비트 단위로 같은 결과를 낸다.
			Unit.Velocity += Unit.Force * FixedDt;
		}

		Unit.Velocity.Z = 0.f;

		const float CurrentSpeed = Unit.Velocity.Size();
		if (CurrentSpeed > Unit.MaxSpeed)
		{
			Unit.Velocity *= (Unit.MaxSpeed / CurrentSpeed);
		}

		Unit.Position += Unit.Velocity * FixedDt;

		// --- 막힘 카운터 ---------------------------------------------------------------
		// 갈 곳이 남았는데 나아가지 못한 스텝을 센다. 도착해서 서 있는 것과 구분해야
		// 대기·체류 중인 사람에게까지 탈출 가속이 걸린다.
		//
		// ★ 이동 거리가 아니라 '목적지 방향 진척'을 센다.
		//
		// 거리로 재면 잼 안에서 좌우로 밀려 흔들리는 사람이 매 스텝 임계를 넘겨 카운터가
		// 리셋되고, 정작 탈출 가속이 필요한 사람에게 영원히 걸리지 않는다.
		// 실제로 그랬다 — 80명 대향류에서 막힌 9명 중 탈출 가속을 받은 사람은 1명뿐이었다.
		//
		// 목적지 방향으로 투영하면 밀려서 후퇴하는 사람은 진척이 음수가 되어 확실히 잡힌다.
		// 옆으로 비켜 가는 중(접선 편향)에도 잠깐 잡히지만, 임계가 40스텝(1.33초)이라
		// 정상적인 옆걸음질은 그 안에 끝난다.
		// 도착 반경은 개체값을 쓴다. 큐 슬롯에 서 있는 사람과 배회 중인 사람의 기준이 다르다 —
		// 전역 값을 쓰면 슬롯 45UU 기준으로는 아직 못 온 사람이 150UU 기준으로 '도착'이 되어
		// 막힘 집계에서 사라진다.
		const bool bWantsToMove = ToFinal.SizeSquared() > FMath::Square(Unit.ArrivalRadiusUU);

		FVector Displacement = Unit.Position - Unit.PrevPosition;
		Displacement.Z = 0.f;

		// 방향 기준은 스텝 시작 시점의 ToSteer다(적분 전에 구해 둔 값).
		const float ProgressUU = FVector::DotProduct(Displacement, ToSteer.GetSafeNormal2D());

		if (bWantsToMove && ProgressUU < Params.StuckSpeedThresholdUU * FixedDt)
		{
			++Unit.StuckSteps;
		}
		else
		{
			Unit.StuckSteps = 0;
		}

		if (Params.StuckDetectSteps > 0 && Unit.StuckSteps >= Params.StuckDetectSteps)
		{
			Unit.UnstuckHoldSteps = FMath::Max(0, Params.UnstuckHoldSteps);
		}
		else if (Unit.UnstuckHoldSteps > 0)
		{
			--Unit.UnstuckHoldSteps;
		}

		const float FacingAlpha = ComputeVelocityAlpha(Params.FacingSmoothSeconds, FixedDt);
		Unit.FacingVelocity = FMath::Lerp(Unit.FacingVelocity, Unit.Velocity, FacingAlpha);
		Unit.FacingVelocity.Z = 0.f;

		const float FacingThresholdUU = Unit.bFacingFollowsVelocity
			? Params.FacingExitSpeedUU
			: Params.FacingEnterSpeedUU;

		Unit.bFacingFollowsVelocity =
			Unit.FacingVelocity.SizeSquared() > FMath::Square(FacingThresholdUU);

		FVector FacingDirection = FVector::ZeroVector;
		if (Unit.bFacingFollowsVelocity)
		{
			FacingDirection = Unit.FacingVelocity.GetSafeNormal2D();
		}
		else if (bWantsToMove)
		{
			FacingDirection = ToSteer.GetSafeNormal2D();
		}

		if (!FacingDirection.IsNearlyZero())
		{
			const float TargetYaw = FacingDirection.Rotation().Yaw;
			const float DeltaYaw = FMath::FindDeltaAngleDegrees(Unit.Rotation.Yaw, TargetYaw);

			if (FMath::Abs(DeltaYaw) > Params.FacingDeadzoneDegrees)
			{
				const float MaxTurnThisStep = Params.MaxTurnRateDegrees * FixedDt;
				Unit.Rotation.Yaw += FMath::Clamp(DeltaYaw, -MaxTurnThisStep, MaxTurnThisStep);
			}
		}
	}

	int32 ConsumeFixedSteps(float& Accumulator, float DeltaTime, float FixedStepSeconds, int32 MaxStepsPerFrame)
	{
		if (FixedStepSeconds <= KINDA_SMALL_NUMBER || MaxStepsPerFrame <= 0)
		{
			return 0;
		}

		Accumulator += DeltaTime;

		int32 Steps = 0;
		while (Accumulator >= FixedStepSeconds && Steps < MaxStepsPerFrame)
		{
			Accumulator -= FixedStepSeconds;
			++Steps;
		}

		if (Steps >= MaxStepsPerFrame && Accumulator >= FixedStepSeconds)
		{
			Accumulator = 0.f;
		}

		return Steps;
	}
}
