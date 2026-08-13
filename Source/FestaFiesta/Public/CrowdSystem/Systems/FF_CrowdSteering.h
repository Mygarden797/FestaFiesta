// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

struct FUnitData;

/**
 * 군중 이동 계산의 순수 함수 모음.
 *
 * 여기 있는 함수는 UWorld, Actor, Subsystem을 전혀 모른다. FUnitData와 숫자만 받아 FUnitData를 고친다.
 * 엔진에서 떼어낸 이유는 두 가지다.
 *   1. 테스트 — 월드를 띄우지 않고 자동화 테스트로 검증할 수 있다. 실제로 FF_CrowdSystemTests.cpp가 이걸 부른다
 *   2. 책임 분리 — "어떻게 움직이는가"(여기)와 "누가 언제 움직이고 누가 화면에 보이는가"(Subsystem)를 나눈다
 *
 * 목적지 선정, 난수, Actor 바인딩은 여기 없다. 전부 호출자인 UFF_CrowdSimulationSubsystem의 몫이다.
 */
namespace CrowdSteering
{
	inline constexpr float FacingSpeedThresholdUU = 5.f;

	/** 한 스텝 이동에 필요한 설정값. Agent마다 달라지지 않는 값들만 모았다. */
	struct FStepParams
	{
		float ArrivalRadiusUU = 150.f;

		float BrakingDistanceUU = 450.f;

		float MaxTurnRateDegrees = 360.f;
	};


	FESTAFIESTA_API float ComputeVelocityAlpha(float SmoothTime, float FixedDt);

	FESTAFIESTA_API bool HasArrived(const FUnitData& Unit, float ArrivalRadiusUU);

	//Agent의 스탭을 진행시킨다.
	FESTAFIESTA_API void StepAgent(FUnitData& Unit, const FStepParams& Params, float VelocityAlpha, float FixedDt);

	FESTAFIESTA_API int32 ConsumeFixedSteps(float& Accumulator, float DeltaTime, float FixedStepSeconds, int32 MaxStepsPerFrame);
}
