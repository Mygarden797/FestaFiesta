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
	/** 이 속도(UU/s) 아래에서는 방향을 갱신하지 않는다. 서 있는 Agent가 미세 속도로 빙빙 도는 것을 막는다. */
	inline constexpr float FacingSpeedThresholdUU = 5.f;

	/** 한 스텝 이동에 필요한 설정값. Agent마다 달라지지 않는 값들만 모았다. */
	struct FStepParams
	{
		/** 목적지에 이만큼 가까워지면 도착으로 본다, UU */
		float ArrivalRadiusUU = 150.f;

		/** 목적지 앞에서 감속을 시작하는 거리, UU. 없으면 목적지를 지나쳤다 돌아오길 반복한다 */
		float BrakingDistanceUU = 450.f;

		/** 초당 회전 한계, 도 */
		float MaxTurnRateDegrees = 360.f;
	};


	FESTAFIESTA_API float ComputeVelocityAlpha(float SmoothTime, float FixedDt);

	/** 목적지에 도착했는지. XY 평면 거리로만 판정한다. */
	FESTAFIESTA_API bool HasArrived(const FUnitData& Unit, float ArrivalRadiusUU);

	//Agent의 스탭을 진행시킨다.
	FESTAFIESTA_API void StepAgent(FUnitData& Unit, const FStepParams& Params, float VelocityAlpha, float FixedDt);

	FESTAFIESTA_API int32 ConsumeFixedSteps(float& Accumulator, float DeltaTime, float FixedStepSeconds, int32 MaxStepsPerFrame);
}
