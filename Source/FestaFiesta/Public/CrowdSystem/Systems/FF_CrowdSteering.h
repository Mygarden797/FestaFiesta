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
	// 지정 자리의 마지막 2m에서는 일반 보행의 개인 공간보다 배정 좌표를 우선한다.
	inline constexpr float AssignedSlotApproachDistanceUU = 200.f;
	FESTAFIESTA_API bool IsApproachingAssignedSlot(const FUnitData& Unit);

	/** 한 스텝 이동에 필요한 설정값. Agent마다 달라지지 않는 값들만 모았다. */
	struct FStepParams
	{
		// ★ 도착 반경은 여기 없다. 개체마다 다르므로 FUnitData::ArrivalRadiusUU를 쓴다 —
		//   큐 슬롯(45UU)과 접근점·배회(150UU 이상)의 기준이 다르기 때문이다.
		//   두 곳에 두면 갈리고, 갈리면 "줄은 안 서는데 도착으로 잡히는" 상태가 된다.

		float BrakingDistanceUU = 450.f;

		float MaxTurnRateDegrees = 180.f;

		float FacingSmoothSeconds = 0.35f;

		float FacingEnterSpeedUU = 40.f;

		float FacingExitSpeedUU = 15.f;

		float FacingDeadzoneDegrees = 4.f;

		int32 StuckDetectSteps = 40;

		int32 UnstuckHoldSteps = 30;

		int32 UnstuckCooldownSteps = 60;

		/**
		 * 목적지 방향 진척이 이 속도(UU/s) 미만이면 막힌 것으로 보고 Unit.StuckSteps를 올린다.
		 *
		 * ★ '이동 거리'가 아니라 '목적지 방향 진척'이다. 거리로 재면 잼 안에서 좌우로 밀려
		 *   흔들리는 사람이 매 스텝 카운터를 리셋해, 정작 탈출이 필요한 사람에게 걸리지 않는다.
		 *
		 * 목적지가 도착 반경 밖일 때만 센다 — 도착해서 서 있는 사람은 막힌 것이 아니다.
		 * 카운터를 여기서 올리는 이유: 실제로 얼마나 나아갔는지는 적분을 한 이 함수만 안다.
		 * 소비는 회피 힘 계산(FFF_CrowdForceSolver)이 한다.
		 */
		float StuckSpeedThresholdUU = 20.f;

		/**
		 * Unit.Force에 회피분뿐 아니라 **구동력까지** 들어 있다는 뜻.
		 *
		 * true면 StepAgent은 지수 평활(Lerp)을 건너뛰고 Force만 적분한다.
		 * 목표 추종이 이미 힘으로 들어와 있으므로 Lerp를 같이 걸면 완화가 두 번 걸리고,
		 * 출발이 두 배로 굼실거린다.
		 *
		 * ★ false(기본값)가 이식 이전 경로다 — Force는 회피분뿐이고 Lerp가 목표 추종을 맡는다.
		 *   회피를 끈 실행은 Force가 0이므로 이 경로에서 기존과 비트 단위로 같은 결과를 낸다.
		 *
		 * 켜는 쪽은 UFF_CrowdSimulationSubsystem::bAvoidanceDrivesVelocity다.
		 * 켤 때는 반드시 FFF_CrowdForceInput::DesiredVelMeters도 같이 채워야 한다 —
		 * 한쪽만 켜면 목표 추종이 아예 사라져 전원이 제자리에 선다.
		 */
		bool bDrivingForceInForce = false;
	};


	FESTAFIESTA_API float ComputeVelocityAlpha(float SmoothTime, float FixedDt);

	FESTAFIESTA_API bool HasArrived(const FUnitData& Unit, float ArrivalRadiusUU);

	/**
	 * 이 개체가 지금 내고 싶은 속도(UU/s). 감속·정지 규칙이 여기 한 곳에만 있다.
	 *
	 * 두 곳에서 부른다 — StepAgent(Lerp 경로)과 회피 미러 생성(구동력 경로).
	 * 그래서 감속 거리 규칙을 고칠 때 한 곳만 고치면 된다. 복사해 두면 두 경로가 조용히 갈린다.
	 */
	FESTAFIESTA_API FVector ComputeDesiredVelocity(const FUnitData& Unit, const FStepParams& Params);

	//Agent의 스탭을 진행시킨다.
	FESTAFIESTA_API void StepAgent(FUnitData& Unit, const FStepParams& Params, float VelocityAlpha, float FixedDt);

	// OutDiscardedSeconds 는 스텝 상한에 걸려 버려진 시뮬 시간이다. 이 값이 0보다 크면
	// 시뮬레이션이 실시간보다 뒤처진 것이고, 그만큼의 축제 시간이 통째로 사라진다.
	// 프레임률은 멀쩡한데 이 값이 쌓이는 구간이 인원수 한계선이다.
	FESTAFIESTA_API int32 ConsumeFixedSteps(float& Accumulator, float DeltaTime, float FixedStepSeconds, int32 MaxStepsPerFrame,
		float* OutDiscardedSeconds = nullptr);
}
