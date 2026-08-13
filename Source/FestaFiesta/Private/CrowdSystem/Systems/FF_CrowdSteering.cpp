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
		Unit.Velocity.Z = 0.f;

		const float CurrentSpeed = Unit.Velocity.Size();
		if (CurrentSpeed > Unit.MaxSpeed)
		{
			Unit.Velocity *= (Unit.MaxSpeed / CurrentSpeed);
		}

		Unit.Position += Unit.Velocity * FixedDt;

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
