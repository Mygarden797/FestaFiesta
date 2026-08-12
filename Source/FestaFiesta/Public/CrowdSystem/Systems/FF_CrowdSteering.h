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

	/**
	 * 지수 평활 계수를 구한다. 0이면 속도가 전혀 안 변하고, 1이면 즉시 희망 속도가 된다.
	 *
	 * FixedDt가 고정이라 매 스텝 같은 값이 나오므로, Agent 루프 밖에서 한 번만 구해 재사용한다.
	 * exp를 쓰는 이유: 단순히 Dt/SmoothTime을 쓰면 Dt가 SmoothTime보다 클 때 계수가 1을 넘어 발산한다.
	 */
	FESTAFIESTA_API float ComputeVelocityAlpha(float SmoothTime, float FixedDt);

	/** 목적지에 도착했는지. XY 평면 거리로만 판정한다. */
	FESTAFIESTA_API bool HasArrived(const FUnitData& Unit, float ArrivalRadiusUU);

	/**
	 * Agent를 한 고정 스텝만큼 진행시킨다.
	 *
	 * PrevPosition 저장 -> 희망 속도 계산 -> 속도 평활/클램프 -> 위치 적분 -> 회전 순으로 진행한다.
	 * 도착 판정과 새 목적지 선정은 하지 않는다. 호출 전에 HasArrived로 확인하고 TargetLocation을 갱신할 것.
	 *
	 * @param VelocityAlpha ComputeVelocityAlpha의 결과
	 */
	FESTAFIESTA_API void StepAgent(FUnitData& Unit, const FStepParams& Params, float VelocityAlpha, float FixedDt);

	/**
	 * 프레임 시간을 누산기에 넣고, 이번 프레임에 실행할 고정 스텝 수를 반환한다.
	 * Accumulator는 소비한 만큼 줄어들고, 남은 값이 렌더링 보간 계수가 된다.
	 *
	 * MaxStepsPerFrame까지 돌고도 한 스텝 이상이 남아 있으면 남은 시간을 버린다.
	 * 버리지 않으면 히칭 이후 매 프레임 한도까지 돌게 되어 영영 회복하지 못한다(death spiral).
	 * 시뮬레이션 시간이 실시간보다 느려지는 대신 프레임률을 지키는 쪽을 택한 것이다.
	 */
	FESTAFIESTA_API int32 ConsumeFixedSteps(float& Accumulator, float DeltaTime, float FixedStepSeconds, int32 MaxStepsPerFrame);
}
