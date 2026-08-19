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

	void StepAgent(FUnitData& Unit, const FStepParams& Params, float VelocityAlpha, float FixedDt)
	{
		Unit.PrevPosition = Unit.Position;


		FVector ToTarget = Unit.TargetLocation - Unit.Position;
		ToTarget.Z = 0.f;

		FVector DesiredVelocity = FVector::ZeroVector;
		if (!ToTarget.IsNearlyZero())
		{
			const FVector Direction = ToTarget.GetSafeNormal2D();

			const float BrakingDistance = FMath::Max(Params.BrakingDistanceUU, KINDA_SMALL_NUMBER);
			const float SpeedScale = FMath::Clamp(ToTarget.Size() / BrakingDistance, 0.f, 1.f);

			DesiredVelocity = Direction * (Unit.Speed * SpeedScale);
		}

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
		Unit.Velocity.Z = 0.f;

		const float CurrentSpeed = Unit.Velocity.Size();
		if (CurrentSpeed > Unit.MaxSpeed)
		{
			Unit.Velocity *= (Unit.MaxSpeed / CurrentSpeed);
		}

		Unit.Position += Unit.Velocity * FixedDt;

		// --- 막힘 카운터 ---------------------------------------------------------------
		// 갈 곳이 남았는데 실제로 움직이지 못한 스텝을 센다. 도착해서 서 있는 것과 구분해야
		// 대기·체류 중인 사람에게까지 탈출 가속이 걸린다.
		const bool bWantsToMove = ToTarget.SizeSquared() > FMath::Square(Params.ArrivalRadiusUU);
		const float MovedUU = FVector::Dist2D(Unit.Position, Unit.PrevPosition);
		if (bWantsToMove && MovedUU < Params.StuckSpeedThresholdUU * FixedDt)
		{
			++Unit.StuckSteps;
		}
		else
		{
			Unit.StuckSteps = 0;
		}

		if (Unit.Velocity.SizeSquared() > FMath::Square(FacingSpeedThresholdUU))
		{
			const float TargetYaw = Unit.Velocity.Rotation().Yaw;
			const float DeltaYaw = FMath::FindDeltaAngleDegrees(Unit.Rotation.Yaw, TargetYaw);

			const float MaxTurnThisStep = Params.MaxTurnRateDegrees * FixedDt;
			Unit.Rotation.Yaw += FMath::Clamp(DeltaYaw, -MaxTurnThisStep, MaxTurnThisStep);
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
