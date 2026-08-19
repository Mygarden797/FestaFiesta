// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdActorPool.h"
#include "CrowdSystem/Systems/FF_CrowdDensityGrid.h"
#include "CrowdSystem/Systems/FF_CrowdForces.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "CrowdSystem/Systems/FF_CrowdSpatialGrid.h"
#include "CrowdSystem/Systems/FF_CrowdSteering.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"

// 테스트는 두 층으로 나뉜다.
//
// 1) 순수 로직 — 밀도 그리드(FCrowdDensityGrid), 이동 계산(CrowdSteering),
//    Agent<->Actor 슬롯 장부(FCrowdActorPoolBinding). UWorld 없이 돈다. 빠르고 실패 원인이 명확하다.
//
// 2) 통합 — 실제 UWorld를 만들고 진짜 AFF_BaseCrowd를 스폰해서 UFF_CrowdSimulationSubsystem을
//    Tick시킨다. 순수 로직이 전부 맞아도 배선이 틀리면 화면에는 아무 일도 안 일어나므로,
//    "정말 Actor가 움직이는가"는 이쪽에서만 확인할 수 있다. 파일 아래쪽에 모아 두었다.

namespace FFF_CrowdTestHelpers
{
	FUnitData MakeUnitAt(const FVector& Position)
	{
		FUnitData Unit;
		Unit.Position = Position;
		Unit.PrevPosition = Position;
		Unit.bIsActive = true;
		return Unit;
	}

	void AddUnitsAt(TArray<FUnitData>& Units, const FVector& Position, int32 Count)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			Units.Add(MakeUnitAt(Position));
		}
	}

	FCrowdDensityGrid MakeTestGrid()
	{
		FCrowdDensityGrid Grid;
		Grid.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), 200.f);
		return Grid;
	}
}

// =================================================================================================
// FCrowdDensityGrid
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdDensity_BuildSizesGridWithCeiling, "FestaFiesta.Crowd.Density.BuildSizesGridWithCeiling", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdDensity_BuildSizesGridWithCeiling::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid;
	Grid.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), 200.f);

	TestTrue(TEXT("그리드가 만들어졌다"), Grid.IsBuilt());
	TestEqual(TEXT("1000UU / 200UU = 5칸"), Grid.GetNumX(), 5);
	TestEqual(TEXT("1000UU / 200UU = 5칸"), Grid.GetNumY(), 5);

	// 나누어떨어지지 않을 때 내림하면 오른쪽 끝 자투리가 통째로 빠져 그 구역 인원이 집계되지 않는다.
	FCrowdDensityGrid Ragged;
	Ragged.Build(FVector::ZeroVector, FVector2D(1050.f, 1050.f), 200.f);
	TestEqual(TEXT("1050UU는 올림해서 6칸이어야 한다"), Ragged.GetNumX(), 6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdDensity_DensityIsCountPerCellArea, "FestaFiesta.Crowd.Density.DensityIsCountPerCellArea", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdDensity_DensityIsCountPerCellArea::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid = FFF_CrowdTestHelpers::MakeTestGrid();

	// 셀 하나(2m x 2m = 4m^2)에 8명 -> 8 / 4 = 2.0명/m^2
	TArray<FUnitData> Units;
	const FVector CellCenter(100.f, 100.f, 0.f);
	FFF_CrowdTestHelpers::AddUnitsAt(Units, CellCenter, 8);

	Grid.Update(Units);

	TestEqual(TEXT("8명 전부 집계됐다"), Grid.GetSampledUnitCount(), 8);
	TestEqual(TEXT("8명 / 4m^2 = 2.0명/m^2"), Grid.GetDensityAtLocation(CellCenter), 2.0f, 0.001f);
	TestEqual(TEXT("최대 밀도도 같은 값"), Grid.GetMaxDensity(), 2.0f, 0.001f);

	// 사람이 없는 셀은 0이어야 한다.
	TestEqual(TEXT("빈 셀은 0"), Grid.GetDensityAtLocation(FVector(700.f, 700.f, 0.f)), 0.f, 0.001f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdDensity_InactiveAndOutOfGridAreExcluded, "FestaFiesta.Crowd.Density.InactiveAndOutOfGridAreExcluded", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdDensity_InactiveAndOutOfGridAreExcluded::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid = FFF_CrowdTestHelpers::MakeTestGrid();

	TArray<FUnitData> Units;
	const FVector CellCenter(100.f, 100.f, 0.f);

	// 활성 4명
	FFF_CrowdTestHelpers::AddUnitsAt(Units, CellCenter, 4);

	// 같은 자리의 비활성 4명 — 풀에 놀고 있는 슬롯을 흉내낸 것. 집계에 들어가면 안 된다.
	for (int32 i = 0; i < 4; ++i)
	{
		FUnitData Idle = FFF_CrowdTestHelpers::MakeUnitAt(CellCenter);
		Idle.bIsActive = false;
		Units.Add(Idle);
	}

	// 그리드 밖(음수 좌표, 그리고 반대쪽 끝 너머) 각 1명
	Units.Add(FFF_CrowdTestHelpers::MakeUnitAt(FVector(-500.f, 100.f, 0.f)));
	Units.Add(FFF_CrowdTestHelpers::MakeUnitAt(FVector(5000.f, 100.f, 0.f)));

	Grid.Update(Units);

	TestEqual(TEXT("활성 + 그리드 안 = 4명만 집계"), Grid.GetSampledUnitCount(), 4);
	TestEqual(TEXT("4명 / 4m^2 = 1.0명/m^2"), Grid.GetDensityAtLocation(CellCenter), 1.0f, 0.001f);
	TestEqual(TEXT("그리드 밖 조회는 0"), Grid.GetDensityAtLocation(FVector(-500.f, 100.f, 0.f)), 0.f, 0.001f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdDensity_UpdateDoesNotAccumulate, "FestaFiesta.Crowd.Density.UpdateDoesNotAccumulate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdDensity_UpdateDoesNotAccumulate::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid = FFF_CrowdTestHelpers::MakeTestGrid();

	TArray<FUnitData> Units;
	const FVector First(100.f, 100.f, 0.f);
	FFF_CrowdTestHelpers::AddUnitsAt(Units, First, 4);

	Grid.Update(Units);
	Grid.Update(Units);
	Grid.Update(Units);

	// 밀도는 "지금 몇 명이 있느냐"이지 누적량이 아니다. 세 번 돌려도 값이 같아야 한다.
	TestEqual(TEXT("반복 Update에도 밀도는 그대로"), Grid.GetDensityAtLocation(First), 1.0f, 0.001f);

	// 전원이 다른 셀로 이동하면 이전 셀은 0으로 비워져야 한다.
	const FVector Second(700.f, 700.f, 0.f);
	for (FUnitData& Unit : Units)
	{
		Unit.Position = Second;
	}
	Grid.Update(Units);

	TestEqual(TEXT("떠난 셀은 0으로 비워진다"), Grid.GetDensityAtLocation(First), 0.f, 0.001f);
	TestEqual(TEXT("옮겨간 셀에 밀도가 잡힌다"), Grid.GetDensityAtLocation(Second), 1.0f, 0.001f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdDensity_LOSMatchesFruinBands, "FestaFiesta.Crowd.Density.LOSMatchesFruinBands", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdDensity_LOSMatchesFruinBands::RunTest(const FString& Parameters)
{
	// Fruin 보행로 구간 경계: 0.31 / 0.43 / 0.72 / 1.08 / 2.17 명/m^2
	// 경계값은 "미만이면 아래 등급"이므로, 경계 바로 아래/위를 둘 다 확인한다.
	TestEqual(TEXT("0.0 -> A"), FCrowdDensityGrid::DensityToLOS(0.0f), ECrowdLOS::A);
	TestEqual(TEXT("0.30 -> A"), FCrowdDensityGrid::DensityToLOS(0.30f), ECrowdLOS::A);
	TestEqual(TEXT("0.31 -> B (경계 포함은 위 등급)"), FCrowdDensityGrid::DensityToLOS(0.31f), ECrowdLOS::B);
	TestEqual(TEXT("0.42 -> B"), FCrowdDensityGrid::DensityToLOS(0.42f), ECrowdLOS::B);
	TestEqual(TEXT("0.43 -> C"), FCrowdDensityGrid::DensityToLOS(0.43f), ECrowdLOS::C);
	TestEqual(TEXT("0.72 -> D"), FCrowdDensityGrid::DensityToLOS(0.72f), ECrowdLOS::D);
	TestEqual(TEXT("1.08 -> E"), FCrowdDensityGrid::DensityToLOS(1.08f), ECrowdLOS::E);
	TestEqual(TEXT("2.17 -> F (군중 사고 위험)"), FCrowdDensityGrid::DensityToLOS(2.17f), ECrowdLOS::F);
	TestEqual(TEXT("5.0 -> F"), FCrowdDensityGrid::DensityToLOS(5.0f), ECrowdLOS::F);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdDensity_CongestedCellCountUsesLOSD, "FestaFiesta.Crowd.Density.CongestedCellCountUsesLOSD", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdDensity_CongestedCellCountUsesLOSD::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid = FFF_CrowdTestHelpers::MakeTestGrid();

	TArray<FUnitData> Units;

	// 셀 A: 4명 / 4m^2 = 1.0명/m^2 -> LOS D 상한(1.08) 미만이므로 혼잡으로 세지 않는다
	FFF_CrowdTestHelpers::AddUnitsAt(Units, FVector(100.f, 100.f, 0.f), 4);

	// 셀 B: 8명 / 4m^2 = 2.0명/m^2 -> 혼잡
	FFF_CrowdTestHelpers::AddUnitsAt(Units, FVector(500.f, 100.f, 0.f), 8);

	// 셀 C: 12명 / 4m^2 = 3.0명/m^2 -> 혼잡
	FFF_CrowdTestHelpers::AddUnitsAt(Units, FVector(900.f, 900.f, 0.f), 12);

	Grid.Update(Units);

	TestEqual(TEXT("LOS D 초과 셀은 2개"), Grid.GetCongestedCellCount(), 2);
	TestEqual(TEXT("최대 밀도는 3.0명/m^2"), Grid.GetMaxDensity(), 3.0f, 0.001f);
	TestEqual(TEXT("전원 집계됐다"), Grid.GetSampledUnitCount(), 24);

	return true;
}

// =================================================================================================
// CrowdSteering — 고정 스텝 누산기
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_FixedStepConsumesWholeStepsOnly, "FestaFiesta.Crowd.Steering.FixedStepConsumesWholeStepsOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_FixedStepConsumesWholeStepsOnly::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;	// 0.03333
	constexpr int32 MaxSteps = 3;

	// 60fps: 프레임 시간이 스텝의 절반이라 두 프레임에 한 번씩만 스텝이 돈다.
	float Accumulator = 0.f;

	const int32 FirstFrame = CrowdSteering::ConsumeFixedSteps(Accumulator, 1.f / 60.f, FixedStep, MaxSteps);
	TestEqual(TEXT("첫 60fps 프레임에는 스텝이 안 돈다"), FirstFrame, 0);

	const int32 SecondFrame = CrowdSteering::ConsumeFixedSteps(Accumulator, 1.f / 60.f, FixedStep, MaxSteps);
	TestEqual(TEXT("두 번째 프레임에 한 스텝"), SecondFrame, 1);
	TestTrue(TEXT("잔여는 한 스텝 미만"), Accumulator < FixedStep);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_FixedStepIsFrameRateIndependent, "FestaFiesta.Crowd.Steering.FixedStepIsFrameRateIndependent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_FixedStepIsFrameRateIndependent::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;
	constexpr int32 MaxSteps = 3;

	// 같은 1초를 60fps로 쪼개 돌리든 30fps로 돌리든 총 스텝 수는 30이어야 한다.
	// 이게 깨지면 프레임률에 따라 시뮬레이션 결과가 달라져서 분석 수치를 믿을 수 없다.
	auto CountStepsOverOneSecond = [](float FrameDelta, int32 FrameCount)
	{
		float Accumulator = 0.f;
		int32 Total = 0;
		for (int32 i = 0; i < FrameCount; ++i)
		{
			Total += CrowdSteering::ConsumeFixedSteps(Accumulator, FrameDelta, FixedStep, MaxSteps);
		}
		return Total;
	};

	TestEqual(TEXT("60fps로 1초 = 30스텝"), CountStepsOverOneSecond(1.f / 60.f, 60), 30);
	TestEqual(TEXT("30fps로 1초 = 30스텝"), CountStepsOverOneSecond(1.f / 30.f, 30), 30);
	TestEqual(TEXT("120fps로 1초 = 30스텝"), CountStepsOverOneSecond(1.f / 120.f, 120), 30);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_HitchDoesNotDeathSpiral, "FestaFiesta.Crowd.Steering.HitchDoesNotDeathSpiral", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_HitchDoesNotDeathSpiral::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;
	constexpr int32 MaxSteps = 3;

	float Accumulator = 0.f;

	// 1초짜리 히칭. 밀린 시간은 30스텝치지만 한도인 3스텝만 돌아야 한다.
	const int32 HitchFrame = CrowdSteering::ConsumeFixedSteps(Accumulator, 1.0f, FixedStep, MaxSteps);
	TestEqual(TEXT("히칭에도 한도까지만 돈다"), HitchFrame, MaxSteps);

	// 그리고 남은 시간은 버려야 한다. 안 버리면 다음 프레임도, 그 다음도 계속 한도까지 돌아
	// 프레임률이 회복되지 못한다.
	TestEqual(TEXT("따라잡기 실패 시 잔여 시간을 버린다"), Accumulator, 0.f, 0.0001f);

	const int32 NextFrame = CrowdSteering::ConsumeFixedSteps(Accumulator, 1.f / 60.f, FixedStep, MaxSteps);
	TestEqual(TEXT("다음 프레임은 정상으로 돌아온다"), NextFrame, 0);

	return true;
}

// =================================================================================================
// CrowdSteering — 이동
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_AgentMovesTowardTarget, "FestaFiesta.Crowd.Steering.AgentMovesTowardTarget", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_AgentMovesTowardTarget::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;

	FUnitData Unit = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Unit.TargetLocation = FVector(5000.f, 0.f, 0.f);

	CrowdSteering::FStepParams Params;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);

	const float StartDistance = FVector::Dist2D(Unit.Position, Unit.TargetLocation);

	for (int32 i = 0; i < 30; ++i)	// 1초
	{
		CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
	}

	const float EndDistance = FVector::Dist2D(Unit.Position, Unit.TargetLocation);

	TestTrue(TEXT("목적지에 가까워졌다"), EndDistance < StartDistance);
	TestTrue(TEXT("+X 방향으로 이동했다"), Unit.Position.X > 0.f);
	// FVector/FRotator는 double 기반이라 float 기대값과 섞으면 TestEqual 오버로드가 모호해진다. 명시적으로 맞춘다.
	TestEqual(TEXT("옆으로 새지 않았다"), static_cast<float>(Unit.Position.Y), 0.f, 0.01f);
	TestEqual(TEXT("높이는 변하지 않는다"), static_cast<float>(Unit.Position.Z), 0.f, 0.001f);
	TestEqual(TEXT("+X를 바라본다"), static_cast<float>(Unit.Rotation.Yaw), 0.f, 0.5f);

	// PrevPosition은 직전 스텝 시작 위치여야 한다. 렌더링 보간이 여기 의존한다.
	TestTrue(TEXT("PrevPosition이 현재 위치보다 뒤에 있다"), Unit.PrevPosition.X < Unit.Position.X);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_SpeedRampsUpAndClampsToMax, "FestaFiesta.Crowd.Steering.SpeedRampsUpAndClampsToMax", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_SpeedRampsUpAndClampsToMax::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;

	FUnitData Unit = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Unit.TargetLocation = FVector(100000.f, 0.f, 0.f);	// 감속 구간에 절대 안 걸리도록 아주 멀리
	Unit.Speed = 150.f;
	Unit.MaxSpeed = 150.f;

	CrowdSteering::FStepParams Params;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);

	// 첫 스텝: 아직 희망 속도에 못 미쳐야 한다. 사람은 순간 가속하지 않는다.
	CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
	TestTrue(TEXT("첫 스텝에 최고 속도가 되지 않는다"), Unit.Velocity.Size() < Unit.Speed);
	TestTrue(TEXT("그래도 움직이기는 한다"), Unit.Velocity.Size() > 0.f);

	// 충분히 지나면 희망 속도에 수렴하고, 절대 MaxSpeed를 넘지 않는다.
	for (int32 i = 0; i < 120; ++i)	// 4초
	{
		CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
		TestTrue(TEXT("MaxSpeed를 넘지 않는다"), Unit.Velocity.Size() <= Unit.MaxSpeed + 0.01f);
	}

	TestEqual(TEXT("희망 속도에 수렴한다"), static_cast<float>(Unit.Velocity.Size()), Unit.Speed, 1.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_AgentBrakesNearTargetAndArrives, "FestaFiesta.Crowd.Steering.AgentBrakesNearTargetAndArrives", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_AgentBrakesNearTargetAndArrives::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;

	FUnitData Unit = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Unit.TargetLocation = FVector(2000.f, 0.f, 0.f);

	CrowdSteering::FStepParams Params;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);

	TestFalse(TEXT("출발 시점에는 도착이 아니다"), CrowdSteering::HasArrived(Unit, Params.ArrivalRadiusUU));

	// 20m를 1.5m/s로 가면 약 13초. 넉넉히 30초를 돌린다.
	bool bArrived = false;
	float MaxOvershootX = 0.f;
	for (int32 i = 0; i < 900 && !bArrived; ++i)
	{
		CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
		MaxOvershootX = FMath::Max(MaxOvershootX, Unit.Position.X);
		bArrived = CrowdSteering::HasArrived(Unit, Params.ArrivalRadiusUU);
	}

	TestTrue(TEXT("목적지에 도착한다"), bArrived);

	// 감속이 없으면 목적지를 지나쳤다가 되돌아오길 반복한다. 오버슛이 없어야 한다.
	TestTrue(TEXT("목적지를 크게 지나치지 않는다"), MaxOvershootX <= Unit.TargetLocation.X + 1.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_TurnRateIsLimited, "FestaFiesta.Crowd.Steering.TurnRateIsLimited", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_TurnRateIsLimited::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;

	// 이미 +X로 빠르게 달리는 상태에서 목적지를 정반대(-X)로 바꾼다.
	FUnitData Unit = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Unit.Velocity = FVector(150.f, 0.f, 0.f);
	Unit.Rotation = FRotator::ZeroRotator;
	Unit.TargetLocation = FVector(-5000.f, 0.f, 0.f);

	CrowdSteering::FStepParams Params;
	Params.MaxTurnRateDegrees = 180.f;	// 한 스텝(1/30초)에 최대 6도
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);

	const float BeforeYaw = Unit.Rotation.Yaw;
	CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
	const float TurnedDegrees = FMath::Abs(FMath::FindDeltaAngleDegrees(BeforeYaw, Unit.Rotation.Yaw));

	// 제한이 없으면 한 스텝에 180도를 홱 돈다. 6도(= 180 * 1/30)를 넘으면 안 된다.
	TestTrue(TEXT("한 스텝 회전량이 한도 이하"), TurnedDegrees <= 6.f + 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_IsDeterministic, "FestaFiesta.Crowd.Steering.IsDeterministic", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_IsDeterministic::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;

	// 고정 스텝을 쓰는 이유가 재현성이다. 같은 초기 상태에서 같은 횟수를 돌리면 결과가 비트 단위로 같아야 한다.
	auto RunSimulation = []()
	{
		FUnitData Unit = FFF_CrowdTestHelpers::MakeUnitAt(FVector(10.f, -20.f, 0.f));
		Unit.TargetLocation = FVector(3000.f, 1500.f, 0.f);

		CrowdSteering::FStepParams Params;
		const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);

		for (int32 i = 0; i < 200; ++i)
		{
			CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
		}
		return Unit;
	};

	const FUnitData A = RunSimulation();
	const FUnitData B = RunSimulation();

	TestTrue(TEXT("같은 입력이면 위치가 정확히 같다"), A.Position.Equals(B.Position, 0.f));
	TestTrue(TEXT("같은 입력이면 속도가 정확히 같다"), A.Velocity.Equals(B.Velocity, 0.f));
	TestEqual(TEXT("같은 입력이면 회전이 정확히 같다"), A.Rotation.Yaw, B.Rotation.Yaw);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_VelocityAlphaStaysInRange, "FestaFiesta.Crowd.Steering.VelocityAlphaStaysInRange", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_VelocityAlphaStaysInRange::RunTest(const FString& Parameters)
{
	// 단순히 Dt/SmoothTime을 쓰면 Dt가 SmoothTime보다 클 때 1을 넘어 발산한다.
	// exp 기반이면 어떤 값을 넣어도 0~1에 머문다.
	TestEqual(TEXT("SmoothTime 0이면 즉시 반영"), CrowdSteering::ComputeVelocityAlpha(0.f, 1.f / 30.f), 1.f);

	const float Normal = CrowdSteering::ComputeVelocityAlpha(0.35f, 1.f / 30.f);
	TestTrue(TEXT("정상 범위 안"), Normal > 0.f && Normal < 1.f);

	// Dt가 SmoothTime보다 훨씬 큰 극단적인 경우에도 1을 넘지 않는다.
	const float Extreme = CrowdSteering::ComputeVelocityAlpha(0.01f, 1.0f);
	TestTrue(TEXT("Dt >> SmoothTime 이어도 1 이하"), Extreme <= 1.f);

	// SmoothTime이 클수록 천천히 붙어야 한다.
	const float Slow = CrowdSteering::ComputeVelocityAlpha(2.0f, 1.f / 30.f);
	TestTrue(TEXT("SmoothTime이 크면 계수가 작다"), Slow < Normal);

	return true;
}

// =================================================================================================
// FCrowdActorPoolBinding — Agent <-> Actor 슬롯 장부
// =================================================================================================

namespace FFF_CrowdPoolTestHelpers
{
	bool HoldsInvariant(const FCrowdActorPoolBinding& Pool)
	{
		return Pool.GetFreeSlotCount() + Pool.GetBoundCount() == Pool.GetNumActorSlots();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPool_StartsEmptyAndUnbound, "FestaFiesta.Crowd.Pool.StartsEmptyAndUnbound", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPool_StartsEmptyAndUnbound::RunTest(const FString& Parameters)
{
	FCrowdActorPoolBinding Pool;
	Pool.Initialise(10, 100);

	TestEqual(TEXT("Actor 슬롯 10개"), Pool.GetNumActorSlots(), 10);
	TestEqual(TEXT("Agent 100개"), Pool.GetNumAgents(), 100);
	TestEqual(TEXT("전부 비어 있다"), Pool.GetFreeSlotCount(), 10);
	TestEqual(TEXT("바인딩 0"), Pool.GetBoundCount(), 0);
	TestFalse(TEXT("아무도 바인딩돼 있지 않다"), Pool.IsBound(0));
	TestEqual(TEXT("미바인딩 조회는 INDEX_NONE"), Pool.GetBoundSlot(0), (int32)INDEX_NONE);
	TestTrue(TEXT("불변식 유지"), FFF_CrowdPoolTestHelpers::HoldsInvariant(Pool));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPool_BindAndReleaseRoundTrip, "FestaFiesta.Crowd.Pool.BindAndReleaseRoundTrip", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPool_BindAndReleaseRoundTrip::RunTest(const FString& Parameters)
{
	FCrowdActorPoolBinding Pool;
	Pool.Initialise(4, 10);

	const int32 Slot = Pool.Bind(3);
	TestTrue(TEXT("슬롯이 하나 붙었다"), Slot != INDEX_NONE);
	TestTrue(TEXT("바인딩 상태"), Pool.IsBound(3));
	TestEqual(TEXT("조회 결과가 같은 슬롯"), Pool.GetBoundSlot(3), Slot);
	TestEqual(TEXT("빈 슬롯이 하나 줄었다"), Pool.GetFreeSlotCount(), 3);
	TestEqual(TEXT("바인딩 수 1"), Pool.GetBoundCount(), 1);
	TestTrue(TEXT("불변식 유지"), FFF_CrowdPoolTestHelpers::HoldsInvariant(Pool));

	const int32 Released = Pool.Release(3);
	TestEqual(TEXT("붙었던 슬롯이 그대로 반환된다"), Released, Slot);
	TestFalse(TEXT("해제됐다"), Pool.IsBound(3));
	TestEqual(TEXT("빈 슬롯이 원래대로"), Pool.GetFreeSlotCount(), 4);
	TestEqual(TEXT("바인딩 수 0"), Pool.GetBoundCount(), 0);
	TestTrue(TEXT("불변식 유지"), FFF_CrowdPoolTestHelpers::HoldsInvariant(Pool));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPool_DoubleBindDoesNotLeakSlot, "FestaFiesta.Crowd.Pool.DoubleBindDoesNotLeakSlot", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPool_DoubleBindDoesNotLeakSlot::RunTest(const FString& Parameters)
{
	FCrowdActorPoolBinding Pool;
	Pool.Initialise(4, 10);

	const int32 First = Pool.Bind(0);
	TestTrue(TEXT("첫 바인딩 성공"), First != INDEX_NONE);

	// 같은 Agent를 또 바인딩하면 거절해야 한다.
	// 여기서 새 슬롯을 붙여 버리면 기존 슬롯이 장부에서 사라져 영영 회수되지 않는다.
	const int32 Second = Pool.Bind(0);
	TestEqual(TEXT("이중 바인딩은 거절된다"), Second, (int32)INDEX_NONE);
	TestEqual(TEXT("슬롯이 새어 나가지 않았다"), Pool.GetFreeSlotCount(), 3);
	TestEqual(TEXT("바인딩 수는 그대로 1"), Pool.GetBoundCount(), 1);
	TestEqual(TEXT("붙어 있던 슬롯이 유지된다"), Pool.GetBoundSlot(0), First);
	TestTrue(TEXT("불변식 유지"), FFF_CrowdPoolTestHelpers::HoldsInvariant(Pool));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPool_DoubleReleaseIsSafe, "FestaFiesta.Crowd.Pool.DoubleReleaseIsSafe", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPool_DoubleReleaseIsSafe::RunTest(const FString& Parameters)
{
	FCrowdActorPoolBinding Pool;
	Pool.Initialise(4, 10);

	Pool.Bind(0);
	Pool.Release(0);

	// 두 번째 해제는 아무 일도 하지 않아야 한다.
	// 여기서 슬롯을 또 밀어 넣으면 같은 슬롯이 빈 목록에 두 번 들어가 두 Agent가 한 Actor를 공유하게 된다.
	const int32 SecondRelease = Pool.Release(0);
	TestEqual(TEXT("이중 해제는 INDEX_NONE"), SecondRelease, (int32)INDEX_NONE);
	TestEqual(TEXT("빈 슬롯이 늘어나지 않았다"), Pool.GetFreeSlotCount(), 4);
	TestEqual(TEXT("바인딩 수 0 유지"), Pool.GetBoundCount(), 0);
	TestTrue(TEXT("불변식 유지"), FFF_CrowdPoolTestHelpers::HoldsInvariant(Pool));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPool_ExhaustionIsGracefulNotAnError, "FestaFiesta.Crowd.Pool.ExhaustionIsGracefulNotAnError", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPool_ExhaustionIsGracefulNotAnError::RunTest(const FString& Parameters)
{
	// Agent 10명에 Actor는 3개. 이 시스템의 핵심 시나리오다 —
	// 논리적 개체 수와 화면에 보이는 수가 독립적이어야 한다.
	FCrowdActorPoolBinding Pool;
	Pool.Initialise(3, 10);

	TArray<int32> AssignedSlots;
	for (int32 AgentIndex = 0; AgentIndex < 10; ++AgentIndex)
	{
		const int32 Slot = Pool.Bind(AgentIndex);
		if (Slot != INDEX_NONE)
		{
			AssignedSlots.Add(Slot);
		}
	}

	TestEqual(TEXT("Actor 수만큼만 바인딩된다"), AssignedSlots.Num(), 3);
	TestEqual(TEXT("빈 슬롯 없음"), Pool.GetFreeSlotCount(), 0);
	TestEqual(TEXT("바인딩 수 3"), Pool.GetBoundCount(), 3);

	// 나눠 준 슬롯에 중복이 있으면 두 Agent가 한 Actor를 공유해 한쪽이 순간이동한다.
	TSet<int32> UniqueSlots(AssignedSlots);
	TestEqual(TEXT("같은 슬롯이 두 번 나가지 않았다"), UniqueSlots.Num(), 3);

	// 바인딩 못 받은 Agent도 정상 상태여야 한다 — 화면에 안 보일 뿐이다.
	TestFalse(TEXT("초과분은 미바인딩"), Pool.IsBound(9));
	TestTrue(TEXT("불변식 유지"), FFF_CrowdPoolTestHelpers::HoldsInvariant(Pool));

	// 하나를 놓아 주면 대기 중이던 Agent가 받을 수 있다.
	Pool.Release(0);
	const int32 Recycled = Pool.Bind(9);
	TestTrue(TEXT("해제된 슬롯이 재사용된다"), Recycled != INDEX_NONE);
	TestTrue(TEXT("불변식 유지"), FFF_CrowdPoolTestHelpers::HoldsInvariant(Pool));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPool_ChurnKeepsInvariant, "FestaFiesta.Crowd.Pool.ChurnKeepsInvariant", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPool_ChurnKeepsInvariant::RunTest(const FString& Parameters)
{
	// 실제 운영에서는 관객이 계속 들고 난다. 바인딩/해제를 뒤섞어 반복해도 장부가 새면 안 된다.
	FCrowdActorPoolBinding Pool;
	Pool.Initialise(16, 64);

	FRandomStream Rng(1234);	// 시드 고정 — 실패하면 그대로 재현된다

	for (int32 Iteration = 0; Iteration < 2000; ++Iteration)
	{
		const int32 AgentIndex = Rng.RandRange(0, 63);

		if (Rng.FRand() < 0.5f)
		{
			Pool.Bind(AgentIndex);
		}
		else
		{
			Pool.Release(AgentIndex);
		}

		if (!FFF_CrowdPoolTestHelpers::HoldsInvariant(Pool))
		{
			AddError(FString::Printf(TEXT("불변식이 깨졌다 (반복 %d): free=%d bound=%d total=%d"),
				Iteration, Pool.GetFreeSlotCount(), Pool.GetBoundCount(), Pool.GetNumActorSlots()));
			return false;
		}

		if (Pool.GetBoundCount() > Pool.GetNumActorSlots())
		{
			AddError(FString::Printf(TEXT("Actor 수보다 많이 바인딩됐다 (반복 %d)"), Iteration));
			return false;
		}
	}

	// 전부 해제하면 원래대로 돌아와야 한다. 안 돌아오면 어딘가에서 슬롯이 샜다는 뜻이다.
	for (int32 AgentIndex = 0; AgentIndex < 64; ++AgentIndex)
	{
		Pool.Release(AgentIndex);
	}
	TestEqual(TEXT("전부 해제하면 슬롯이 전량 회수된다"), Pool.GetFreeSlotCount(), 16);
	TestEqual(TEXT("바인딩 수 0"), Pool.GetBoundCount(), 0);

	// 나눠 줄 수 있는 슬롯도 원래대로여야 한다.
	int32 Rebound = 0;
	for (int32 AgentIndex = 0; AgentIndex < 64; ++AgentIndex)
	{
		if (Pool.Bind(AgentIndex) != INDEX_NONE)
		{
			++Rebound;
		}
	}
	TestEqual(TEXT("다시 16개를 나눠 줄 수 있다"), Rebound, 16);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPool_OutOfRangeAgentIsRejected, "FestaFiesta.Crowd.Pool.OutOfRangeAgentIsRejected", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPool_OutOfRangeAgentIsRejected::RunTest(const FString& Parameters)
{
	FCrowdActorPoolBinding Pool;
	Pool.Initialise(4, 10);

	TestEqual(TEXT("음수 인덱스 거절"), Pool.Bind(-1), (int32)INDEX_NONE);
	TestEqual(TEXT("범위 초과 인덱스 거절"), Pool.Bind(10), (int32)INDEX_NONE);
	TestEqual(TEXT("범위 밖 해제도 거절"), Pool.Release(999), (int32)INDEX_NONE);
	TestEqual(TEXT("슬롯이 소모되지 않았다"), Pool.GetFreeSlotCount(), 4);
	TestTrue(TEXT("불변식 유지"), FFF_CrowdPoolTestHelpers::HoldsInvariant(Pool));

	// Actor 슬롯이 0개인 경우(스폰이 전부 실패한 상황)에도 죽지 않아야 한다.
	FCrowdActorPoolBinding Empty;
	Empty.Initialise(0, 10);
	TestEqual(TEXT("슬롯 0개면 바인딩 불가"), Empty.Bind(0), (int32)INDEX_NONE);
	TestTrue(TEXT("불변식 유지"), FFF_CrowdPoolTestHelpers::HoldsInvariant(Empty));

	return true;
}

// =================================================================================================
// 통합 테스트 — 실제 UWorld + 실제 Actor
// =================================================================================================

namespace FFF_CrowdWorldTestHelpers
{
	/**
	 * 테스트용 게임 월드를 만들고 소멸 시 정리한다.
	 * 엔진의 FActorTestSpawner(Developer/CQTest)와 같은 절차다.
	 *
	 * 스택에 두고 쓴다. 소멸자가 액터 정리 -> 월드 파괴 -> 월드 컨텍스트 제거를 순서대로 한다.
	 */
	struct FScopedTestWorld
	{
		UWorld* World = nullptr;

		FScopedTestWorld()
		{
			if (!GEngine)
			{
				return;
			}

			const FName WorldName = MakeUniqueObjectName(
				nullptr, UWorld::StaticClass(), NAME_None, EUniqueObjectNameOptions::GloballyUnique);

			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
			if (!World)
			{
				return;
			}

			World->AddToRoot();
			WorldContext.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());

			// 서브시스템의 OnWorldBeginPlay를 태운다(난수 시드 초기화).
			World->BeginPlay();

			// GameMode가 없으면 UWorld::BeginPlay가 액터 BeginPlay를 돌리지 않아
			// 이후 스폰되는 액터도 BeginPlay를 받지 못한다. 여기서 직접 켠다.
			if (AWorldSettings* Settings = World->GetWorldSettings())
			{
				Settings->NotifyBeginPlay();
			}
		}

		~FScopedTestWorld()
		{
			if (!World || !GEngine)
			{
				return;
			}

			if (World->AreActorsInitialized())
			{
				for (AActor* Actor : FActorRange(World))
				{
					if (Actor)
					{
						Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
					}
				}
			}

			GEngine->ShutdownWorldNetDriver(World);
			World->DestroyWorld(true);
			World->SetPhysicsScene(nullptr);
			GEngine->DestroyWorldContext(World);
			World->RemoveFromRoot();
			World = nullptr;
		}

		UFF_CrowdSimulationSubsystem* GetCrowdSim() const
		{
			return World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
		}
	};

	void TickFor(UFF_CrowdSimulationSubsystem& CrowdSim, float Seconds)
	{
		constexpr float FrameDelta = 1.f / 60.f;
		const int32 FrameCount = FMath::CeilToInt(Seconds / FrameDelta);

		for (int32 i = 0; i < FrameCount; ++i)
		{
			CrowdSim.Tick(FrameDelta);
		}
	}

	int32 CountVisibleActors(const UFF_CrowdSimulationSubsystem& CrowdSim)
	{
		int32 Visible = 0;
		for (const AFF_BaseCrowd* Actor : CrowdSim.CrowdPool)
		{
			if (IsValid(Actor) && !Actor->IsHidden())
			{
				++Visible;
			}
		}
		return Visible;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdWorld_InitializeSpawnsHiddenPool, "FestaFiesta.Crowd.World.InitializeSpawnsHiddenPool", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdWorld_InitializeSpawnsHiddenPool::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 8, 20, AFF_BaseCrowd::StaticClass());

	TestTrue(TEXT("초기화 완료 표시"), CrowdSim->IsSimulationInitialized());
	TestEqual(TEXT("요청한 만큼 Actor가 스폰됐다"), CrowdSim->CrowdPool.Num(), 8);
	TestEqual(TEXT("장부 슬롯 수가 실제 Actor 수와 일치"), CrowdSim->PoolBinding.GetNumActorSlots(), 8);
	TestEqual(TEXT("Agent 풀 크기"), CrowdSim->PoolBinding.GetNumAgents(), 20);

	// 스폰 직후에는 아무도 활성이 아니고 전부 숨겨져 있어야 한다.
	TestEqual(TEXT("활성 Agent 없음"), CrowdSim->GetActiveAgentCount(), 0);
	TestEqual(TEXT("보이는 Actor 없음"), FFF_CrowdWorldTestHelpers::CountVisibleActors(*CrowdSim), 0);

	// APawn 기본값(AdjustIfPossibleButDontSpawnIfColliding) 때문에 같은 자리 스폰이 실패하면
	// 여기서 8보다 작게 나온다.
	for (const AFF_BaseCrowd* Actor : CrowdSim->CrowdPool)
	{
		if (!IsValid(Actor))
		{
			AddError(TEXT("풀에 유효하지 않은 Actor가 섞였다"));
			return false;
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdWorld_SpawnAgentsBindsAndRevealsActors, "FestaFiesta.Crowd.World.SpawnAgentsBindsAndRevealsActors", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdWorld_SpawnAgentsBindsAndRevealsActors::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 8, 20, AFF_BaseCrowd::StaticClass());

	const int32 Spawned = CrowdSim->SpawnAgents(5, FVector::ZeroVector, 500.f);

	TestEqual(TEXT("요청한 5명이 스폰됐다"), Spawned, 5);
	TestEqual(TEXT("활성 Agent 5"), CrowdSim->GetActiveAgentCount(), 5);
	TestEqual(TEXT("Actor 5개가 바인딩됐다"), CrowdSim->GetBoundActorCount(), 5);
	TestEqual(TEXT("Actor 5개가 화면에 드러났다"), FFF_CrowdWorldTestHelpers::CountVisibleActors(*CrowdSim), 5);
	TestEqual(TEXT("남은 빈 슬롯 3"), CrowdSim->PoolBinding.GetFreeSlotCount(), 3);

	TestTrue(TEXT("스폰된 Agent는 활성"), CrowdSim->IsAgentActive(0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdWorld_TickMovesAgentsAndTheirActors, "FestaFiesta.Crowd.World.TickMovesAgentsAndTheirActors", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdWorld_TickMovesAgentsAndTheirActors::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 4, 4, AFF_BaseCrowd::StaticClass());
	CrowdSim->SpawnAgents(4, FVector::ZeroVector, 0.f);	// 전원 원점에서 출발

	// 이게 이 테스트의 존재 이유다. 순수 로직 테스트는 전부 통과해도
	// Subsystem -> Actor 배선이 끊겨 있으면 화면에서는 아무 일도 일어나지 않는다.
	const int32 ActorSlot = CrowdSim->PoolBinding.GetBoundSlot(0);
	if (!CrowdSim->CrowdPool.IsValidIndex(ActorSlot))
	{
		AddError(TEXT("0번 Agent에 Actor가 바인딩되지 않았다"));
		return false;
	}

	AFF_BaseCrowd* Actor = CrowdSim->CrowdPool[ActorSlot];
	const FVector StartActorLocation = Actor->GetActorLocation();

	FFF_CrowdWorldTestHelpers::TickFor(*CrowdSim, 2.0f);

	const FVector EndAgentPosition = CrowdSim->GetAgentPosition(0);
	const FVector EndActorLocation = Actor->GetActorLocation();

	TestTrue(TEXT("Agent가 실제로 이동했다"), !EndAgentPosition.IsNearlyZero());
	TestTrue(TEXT("Actor도 따라 움직였다"), !EndActorLocation.Equals(StartActorLocation, 1.f));

	// Actor 위치는 보간된 값이라 Agent 위치와 정확히 같지는 않다.
	// 다만 한 스텝(1/30초 x 1.5m/s = 5cm) 이상 벌어지면 배선이 잘못된 것이다.
	const float Gap = FVector::Dist2D(EndAgentPosition, EndActorLocation);
	TestTrue(FString::Printf(TEXT("Actor가 Agent를 따라간다 (간격 %.1fUU)"), Gap), Gap < 10.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdWorld_ActorShortageDoesNotAffectMetrics, "FestaFiesta.Crowd.World.ActorShortageDoesNotAffectMetrics", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdWorld_ActorShortageDoesNotAffectMetrics::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	// 이 시스템 전체의 핵심 주장을 검증한다:
	// 화면에 보이는 수(Actor 3개)와 분석에 반영되는 수(Agent 12명)는 독립적이어야 한다.
	// 렌더링 예산이 분석 결과를 오염시키면 디지털 트윈으로서 의미가 없다.
	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 3, 12, AFF_BaseCrowd::StaticClass());

	const int32 Spawned = CrowdSim->SpawnAgents(12, FVector::ZeroVector, 300.f);

	TestEqual(TEXT("Agent는 12명 전원 스폰된다"), Spawned, 12);
	TestEqual(TEXT("활성 Agent 12"), CrowdSim->GetActiveAgentCount(), 12);
	TestEqual(TEXT("하지만 Actor는 3개만 붙는다"), CrowdSim->GetBoundActorCount(), 3);
	TestEqual(TEXT("보이는 Actor도 3개"), FFF_CrowdWorldTestHelpers::CountVisibleActors(*CrowdSim), 3);

	FFF_CrowdWorldTestHelpers::TickFor(*CrowdSim, 0.5f);

	// 밀도 계측에는 12명 전원이 들어가야 한다. 3명만 잡히면 혼잡도가 1/4로 과소평가된다.
	TestEqual(TEXT("밀도 집계는 12명 전원"), CrowdSim->GetDensityGrid().GetSampledUnitCount(), 12);
	TestTrue(TEXT("밀도가 0보다 크다"), CrowdSim->GetMaxDensity() > 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdWorld_ReleaseReturnsActorAndHidesIt, "FestaFiesta.Crowd.World.ReleaseReturnsActorAndHidesIt", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdWorld_ReleaseReturnsActorAndHidesIt::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 4, 4, AFF_BaseCrowd::StaticClass());
	CrowdSim->SpawnAgents(2, FVector::ZeroVector, 100.f);

	const int32 ActorSlot = CrowdSim->PoolBinding.GetBoundSlot(0);
	if (!CrowdSim->CrowdPool.IsValidIndex(ActorSlot))
	{
		AddError(TEXT("0번 Agent에 Actor가 바인딩되지 않았다"));
		return false;
	}

	AFF_BaseCrowd* Actor = CrowdSim->CrowdPool[ActorSlot];
	TestFalse(TEXT("바인딩된 Actor는 보인다"), Actor->IsHidden());

	CrowdSim->ReleaseAgentToPool(0);

	TestTrue(TEXT("해제된 Actor는 숨겨진다"), Actor->IsHidden());
	TestFalse(TEXT("장부에서도 풀렸다"), CrowdSim->PoolBinding.IsBound(0));
	TestEqual(TEXT("빈 슬롯이 회수됐다"), CrowdSim->PoolBinding.GetFreeSlotCount(), 3);
	TestEqual(TEXT("바인딩 수 1"), CrowdSim->GetBoundActorCount(), 1);

	// 해제해도 Agent 자체는 살아 있다 — 시뮬레이션과 밀도 집계는 계속된다.
	TestTrue(TEXT("Agent는 여전히 활성"), CrowdSim->IsAgentActive(0));
	TestEqual(TEXT("활성 Agent 수는 그대로 2"), CrowdSim->GetActiveAgentCount(), 2);

	FFF_CrowdWorldTestHelpers::TickFor(*CrowdSim, 0.5f);
	TestEqual(TEXT("Actor 없는 Agent도 밀도에 잡힌다"), CrowdSim->GetDensityGrid().GetSampledUnitCount(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdWorld_DoubleInitializeIsIgnored, "FestaFiesta.Crowd.World.DoubleInitializeIsIgnored", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdWorld_DoubleInitializeIsIgnored::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 4, 10, AFF_BaseCrowd::StaticClass());
	TestEqual(TEXT("첫 초기화로 4개"), CrowdSim->CrowdPool.Num(), 4);

	// 레벨에 스포너를 실수로 두 개 놓는 일은 흔하다. 그때 Actor가 두 배로 늘면 안 된다.
	AddExpectedError(TEXT("Already initialized"), EAutomationExpectedErrorFlags::Contains, 0);
	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 4, 10, AFF_BaseCrowd::StaticClass());

	TestEqual(TEXT("두 번째 초기화는 무시된다"), CrowdSim->CrowdPool.Num(), 4);
	TestEqual(TEXT("장부도 그대로"), CrowdSim->PoolBinding.GetNumActorSlots(), 4);

	return true;
}

// =================================================================================================
// 파티(동반 그룹)
//
// 스폰과 목적지 상속은 서브시스템에 있으므로 UWorld가 필요하다.
// 검증 대상은 "같이 온 사람들이 같이 다니는가"의 두 축이다:
//   구조 — 멤버가 리더를 정확히 가리키고 같은 PartyID를 갖는가
//   행동 — Tick을 돌린 뒤에도 응집 반경 안에 머무는가
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdParty_SpawnHitsExactRequestedCount, "FestaFiesta.Crowd.Party.SpawnHitsExactRequestedCount", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdParty_SpawnHitsExactRequestedCount::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 0, 64, AFF_BaseCrowd::StaticClass());

	// 파티 단위로 스폰하면 마지막 파티가 요청 인원을 넘길 수 있다.
	// 넘치는 파티를 잘라 넣지 않으면 "500명 스폰"이 503명이 되어 밀도 수치가 어긋난다.
	const int32 Spawned = CrowdSim->SpawnAgents(37, FVector::ZeroVector, 2000.f);

	TestEqual(TEXT("요청한 37명이 정확히 스폰됐다"), Spawned, 37);
	TestEqual(TEXT("활성 Agent도 37"), CrowdSim->GetActiveAgentCount(), 37);

	// 기본 가중치라면 2인 이상 파티가 반드시 섞인다. 파티 수가 인원 수와 같으면 그룹핑이 안 걸린 것이다.
	const int32 PartyCount = CrowdSim->GetActivePartyCount();
	TestTrue(FString::Printf(TEXT("파티 수(%d)가 인원 수보다 적다 = 실제로 묶였다"), PartyCount), PartyCount < 37);
	TestTrue(TEXT("파티가 최소 하나는 있다"), PartyCount > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdParty_MembersPointAtLiveLeaderOfSameParty, "FestaFiesta.Crowd.Party.MembersPointAtLiveLeaderOfSameParty", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdParty_MembersPointAtLiveLeaderOfSameParty::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 0, 64, AFF_BaseCrowd::StaticClass());
	const int32 Spawned = CrowdSim->SpawnAgents(50, FVector::ZeroVector, 3000.f);

	int32 MemberCount = 0;

	for (int32 AgentIndex = 0; AgentIndex < Spawned; ++AgentIndex)
	{
		const int32 LeaderIndex = CrowdSim->GetPartyLeader(AgentIndex);
		const int32 PartyID = CrowdSim->GetAgentPartyID(AgentIndex);

		if (!CrowdSim->IsAgentActive(LeaderIndex))
		{
			AddError(FString::Printf(TEXT("Agent %d의 리더 %d가 비활성이다"), AgentIndex, LeaderIndex));
			return false;
		}

		// PartyID가 다르면 UpdatePartyMemberTarget의 승격 규칙이 매 스텝 발동해 파티가 즉시 해체된다.
		if (CrowdSim->GetAgentPartyID(LeaderIndex) != PartyID)
		{
			AddError(FString::Printf(TEXT("Agent %d(파티 %d)와 리더 %d(파티 %d)의 PartyID가 다르다"),
				AgentIndex, PartyID, LeaderIndex, CrowdSim->GetAgentPartyID(LeaderIndex)));
			return false;
		}

		// 리더의 리더는 자기 자신이어야 한다. 아니면 상속 사슬이 2단이 되어 대형 오프셋이 중첩된다.
		if (!CrowdSim->IsPartyLeader(LeaderIndex))
		{
			AddError(FString::Printf(TEXT("Agent %d의 리더 %d가 스스로는 리더가 아니다(상속 사슬)"), AgentIndex, LeaderIndex));
			return false;
		}

		if (!CrowdSim->IsPartyLeader(AgentIndex))
		{
			++MemberCount;
		}
	}

	TestEqual(TEXT("리더 수 + 멤버 수 = 전체"), CrowdSim->GetActivePartyCount() + MemberCount, Spawned);
	TestTrue(FString::Printf(TEXT("멤버가 존재한다 (%d명)"), MemberCount), MemberCount > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdParty_GroupingOffMakesEveryoneSolo, "FestaFiesta.Crowd.Party.GroupingOffMakesEveryoneSolo", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdParty_GroupingOffMakesEveryoneSolo::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 0, 32, AFF_BaseCrowd::StaticClass());

	// 그룹 이동이 혼잡도에 주는 영향을 A/B로 비교하려면 이 스위치가 실제로 전원을 1인으로 만들어야 한다.
	CrowdSim->bEnablePartyGrouping = false;

	const int32 Spawned = CrowdSim->SpawnAgents(20, FVector::ZeroVector, 2000.f);

	TestEqual(TEXT("20명 스폰"), Spawned, 20);
	TestEqual(TEXT("파티 수 = 인원 수 (전원 1인)"), CrowdSim->GetActivePartyCount(), 20);

	for (int32 AgentIndex = 0; AgentIndex < Spawned; ++AgentIndex)
	{
		if (!CrowdSim->IsPartyLeader(AgentIndex))
		{
			AddError(FString::Printf(TEXT("Agent %d가 1인 파티가 아니다"), AgentIndex));
			return false;
		}
	}

	// 1인 파티도 PartyID를 받는다 — 파티 없는 경로를 따로 두지 않기 위해서다.
	TestNotEqual(TEXT("1인도 PartyID를 갖는다"), CrowdSim->GetAgentPartyID(0), static_cast<int32>(INDEX_NONE));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdParty_MembersStayWithinCohesionRadius, "FestaFiesta.Crowd.Party.MembersStayWithinCohesionRadius", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdParty_MembersStayWithinCohesionRadius::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 5000.f, 0, 64, AFF_BaseCrowd::StaticClass());
	const int32 Spawned = CrowdSim->SpawnAgents(48, FVector::ZeroVector, 2000.f);

	// 목적지를 여러 번 갈아탈 만큼 돌린다. 한 번만 돌리면 스폰 직후 배치가 그대로 남아 통과해 버린다.
	FFF_CrowdWorldTestHelpers::TickFor(*CrowdSim, 30.f);

	float WorstDistance = 0.f;
	int32 WorstAgent = INDEX_NONE;

	for (int32 AgentIndex = 0; AgentIndex < Spawned; ++AgentIndex)
	{
		if (CrowdSim->IsPartyLeader(AgentIndex))
		{
			continue;
		}

		const int32 LeaderIndex = CrowdSim->GetPartyLeader(AgentIndex);
		const float Distance = FVector::Dist2D(
			CrowdSim->GetAgentPosition(AgentIndex), CrowdSim->GetAgentPosition(LeaderIndex));

		if (Distance > WorstDistance)
		{
			WorstDistance = Distance;
			WorstAgent = AgentIndex;
		}
	}

	if (WorstAgent == INDEX_NONE)
	{
		AddError(TEXT("멤버가 한 명도 없어 응집을 검증할 수 없다"));
		return false;
	}

	// 응집 반경을 넘으면 리더 추종으로 전환되지만 즉시 붙지는 않는다.
	// 감속 거리와 한 스텝 이동량을 감안해 2배까지 허용한다. 그래도 못 지키면 상속 규칙이 깨진 것이다.
	const float Allowed = CrowdSim->PartyCohesionRadiusUU * 2.f;
	TestTrue(
		FString::Printf(TEXT("멤버가 리더 곁에 머문다 (최악 Agent %d, %.0fUU <= %.0fUU)"), WorstAgent, WorstDistance, Allowed),
		WorstDistance <= Allowed);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdParty_MembersDoNotPickOwnDestination, "FestaFiesta.Crowd.Party.MembersDoNotPickOwnDestination", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdParty_MembersDoNotPickOwnDestination::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 5000.f, 0, 64, AFF_BaseCrowd::StaticClass());
	const int32 Spawned = CrowdSim->SpawnAgents(48, FVector::ZeroVector, 1000.f);

	FFF_CrowdWorldTestHelpers::TickFor(*CrowdSim, 5.f);

	// 멤버 목적지 = 리더 목적지 + 대형 오프셋. 오프셋은 PartySpacing * sqrt(slot)이라
	// 6인 파티 기준 최대 PartySpacing * sqrt(5) ≈ 2.24배를 넘지 않는다.
	// 이 검증이 깨지는 경우는 멤버가 PickRandomDestination을 부르는 것뿐이다 —
	// 그러면 SimRadius(5000UU) 안 임의 지점이 나오므로 오차가 자릿수째 달라진다.
	const float MaxOffset = CrowdSim->PartySpacingUU * 3.f;

	for (int32 AgentIndex = 0; AgentIndex < Spawned; ++AgentIndex)
	{
		if (CrowdSim->IsPartyLeader(AgentIndex))
		{
			continue;
		}

		const int32 LeaderIndex = CrowdSim->GetPartyLeader(AgentIndex);
		const float TargetGap = FVector::Dist2D(
			CrowdSim->GetAgentDestination(AgentIndex), CrowdSim->GetAgentDestination(LeaderIndex));

		// 리더를 향해 복귀 중인 멤버는 목적지가 리더 '위치'라 이 비교에서 벗어난다. 그 경우는 건너뛴다.
		const float DistanceToLeader = FVector::Dist2D(
			CrowdSim->GetAgentPosition(AgentIndex), CrowdSim->GetAgentPosition(LeaderIndex));
		if (DistanceToLeader > CrowdSim->PartyCohesionRadiusUU)
		{
			continue;
		}

		if (TargetGap > MaxOffset)
		{
			AddError(FString::Printf(
				TEXT("Agent %d의 목적지가 리더 %d와 %.0fUU 떨어졌다 (허용 %.0fUU) — 스스로 목적지를 뽑았다"),
				AgentIndex, LeaderIndex, TargetGap, MaxOffset));
			return false;
		}
	}

	return true;
}

// =================================================================================================
// 회피(사회력) — 순수 함수
//
// 여기 있는 테스트가 "단위를 100배 틀렸다"를 잡는 유일한 조기 경보다.
// 계수는 SI(m, kg, N)이고, 아래 모든 좌표는 미터다. UU가 등장하면 그 자체로 버그다.
// =================================================================================================

namespace FFF_AvoidanceTestHelpers
{
	// 성인 보행자 한 명. 반경 0.34m = 팀 레포의 Radius 34UU와 같은 사람이다.
	constexpr float PersonRadiusMeters = 0.34f;
	constexpr float PersonMassKg = 70.f;
	constexpr float FixedStep = 1.f / 30.f;

	FFF_CrowdAvoidanceConfig MakeConfig()
	{
		// 기본값이 곧 이식 계획의 초기값이다. 테스트가 기본값을 그대로 쓰는 것이 의도다 —
		// 누가 계수를 만지면 이 테스트가 먼저 반응해야 한다.
		return FFF_CrowdAvoidanceConfig();
	}

	/**
	 * 2인 정면 충돌을 30Hz로 적분한다. 구동력은 넣지 않는다 —
	 * StepAgent의 지수 평활이 그 역할을 하므로, 여기서 검증할 것은 반발·접촉항뿐이다.
	 *
	 * @return 시뮬 동안의 최소 거리(m). NaN이 생기면 그대로 NaN이 나온다
	 */
	float SimulateHeadOn(int32 NumSteps, float StartGapMeters, float ApproachSpeedMps,
	                     const FFF_CrowdAvoidanceConfig& Config, bool* OutSawNonFinite = nullptr)
	{
		FVector2f PosA(-StartGapMeters * 0.5f, 0.f);
		FVector2f PosB(StartGapMeters * 0.5f, 0.f);
		FVector2f VelA(ApproachSpeedMps, 0.f);
		FVector2f VelB(-ApproachSpeedMps, 0.f);

		float MinDistance = (PosB - PosA).Size();
		if (OutSawNonFinite)
		{
			*OutSawNonFinite = false;
		}

		for (int32 Step = 0; Step < NumSteps; ++Step)
		{
			// [규약] 대칭 최적화 금지. 한 쌍에 대해 두 번 계산한다 — 계산량 2배를 내고 재현성을 산다.
			const FVector2f ForceA = FFF_CrowdForceSolver::ComputeAgentInteraction(
				PosA, VelA, PersonRadiusMeters, PosB, VelB, PersonRadiusMeters, Config);
			const FVector2f ForceB = FFF_CrowdForceSolver::ComputeAgentInteraction(
				PosB, VelB, PersonRadiusMeters, PosA, VelA, PersonRadiusMeters, Config);

			const FVector2f AccelA = FFF_CrowdForceSolver::ClampAcceleration(
				ForceA / PersonMassKg, Config.MaxAccelerationMps2);
			const FVector2f AccelB = FFF_CrowdForceSolver::ClampAcceleration(
				ForceB / PersonMassKg, Config.MaxAccelerationMps2);

			VelA += AccelA * FixedStep;
			VelB += AccelB * FixedStep;

			// MaxSpeed 클램프는 StepAgent에도 있다. 없으면 접촉 반발로 사람이 날아간다.
			constexpr float MaxSpeedMps = 1.5f;
			if (VelA.Size() > MaxSpeedMps) { VelA *= MaxSpeedMps / VelA.Size(); }
			if (VelB.Size() > MaxSpeedMps) { VelB *= MaxSpeedMps / VelB.Size(); }

			PosA += VelA * FixedStep;
			PosB += VelB * FixedStep;

			const float Distance = (PosB - PosA).Size();
			if (OutSawNonFinite && (!FMath::IsFinite(Distance) || !FMath::IsFinite(VelA.X)))
			{
				*OutSawNonFinite = true;
			}
			MinDistance = FMath::Min(MinDistance, Distance);
		}

		return MinDistance;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_HeadOnDoesNotPenetrate, "FestaFiesta.Crowd.Avoidance.HeadOnDoesNotPenetrate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_HeadOnDoesNotPenetrate::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();
	const float ContactDistance = PersonRadiusMeters * 2.f;	// 0.68m

	bool bSawNonFinite = false;
	// 2m 떨어진 두 사람이 서로를 향해 1.5m/s로 걷는다. 회피가 없으면 서로를 통과한다.
	const float MinDistance = SimulateHeadOn(120, 2.f, 1.5f, Config, &bSawNonFinite);

	TestFalse(TEXT("적분 중 NaN/inf가 생기지 않았다"), bSawNonFinite);
	TestTrue(TEXT("서로를 통과하지 않았다(거리가 0을 지나지 않음)"), MinDistance > 0.f);

	// 이 테스트가 잡는 것은 '단위 오류'다. 계수를 UU로 잘못 넘기면 반발이 100배 약해져
	// 두 사람이 그냥 겹쳐 지나가고 MinDistance가 0에 가까워진다.
	TestTrue(
		*FString::Printf(TEXT("인체 반경 안으로 깊게 파고들지 않았다 (최소 %.3fm, 접촉 %.3fm)"),
			MinDistance, ContactDistance),
		MinDistance > ContactDistance * 0.75f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_HeadOnIsDeterministic, "FestaFiesta.Crowd.Avoidance.HeadOnIsDeterministic", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_HeadOnIsDeterministic::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();

	const float First = SimulateHeadOn(120, 2.f, 1.5f, Config);
	const float Second = SimulateHeadOn(120, 2.f, 1.5f, Config);

	// 오차 허용 없이 '완전히 같은 값'을 요구한다. 힘 계산에 RNG나 순서 의존이 섞이면 여기서 걸린다.
	TestEqual(TEXT("같은 입력은 비트 단위로 같은 결과를 낸다"), First, Second, 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_RepulsionIsInMetresNotUU, "FestaFiesta.Crowd.Avoidance.RepulsionIsInMetresNotUU", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_RepulsionIsInMetresNotUU::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();

	// 접촉 거리(0.68m)에서는 A만큼(2000N) 나와야 한다. exp(0) = 1이므로 정확히 A다.
	// 좌표를 UU로 넘겼다면 거리가 100배가 되어 exp가 0에 가까워지고 이 값이 0으로 떨어진다.
	const FVector2f AtContact = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(PersonRadiusMeters * 2.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

	TestEqual(TEXT("접촉 거리에서 반발력 = RepulsionA"), AtContact.Size(), Config.RepulsionA, 1.f);
	TestTrue(TEXT("-X 방향으로 밀려난다(상대는 +X에 있다)"), AtContact.X < 0.f);

	// B(0.08m)만큼 더 떨어지면 1/e로 감쇠한다. 이 관계가 깨지면 B를 UU로 해석한 것이다.
	const FVector2f OneB = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(PersonRadiusMeters * 2.f + Config.RepulsionB, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

	TestEqual(TEXT("B만큼 멀어지면 1/e로 감쇠한다"),
		OneB.Size(), Config.RepulsionA * FMath::Exp(-1.f), 1.f);

	// 2m(이웃 반경 끝)에서는 사실상 0이다 — 그래서 MaxNeighbours로 잘라도 정확도 손실이 없다.
	const FVector2f AtNeighbourEdge = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(2.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

	TestTrue(TEXT("이웃 반경 끝에서는 기여가 무시할 만하다"), AtNeighbourEdge.Size() < 1.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_AnisotropyWeakensRearApproach, "FestaFiesta.Crowd.Avoidance.AnisotropyWeakensRearApproach", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_AnisotropyWeakensRearApproach::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();
	const float Gap = PersonRadiusMeters * 2.f + 0.2f;

	// +X로 걷는 사람. 앞(+X)에 있는 사람과 뒤(-X)에 있는 사람에 대한 반발 크기를 비교한다.
	const FVector2f Heading(1.f, 0.f);

	const FVector2f FromFront = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), Heading, PersonRadiusMeters,
		FVector2f(Gap, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

	const FVector2f FromRear = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), Heading, PersonRadiusMeters,
		FVector2f(-Gap, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

	// 사람은 뒤에서 오는 사람을 덜 피한다. 이 비대칭이 없으면 인파가 실제보다 넓게 퍼져
	// 밀도가 과소평가되고, 그건 이 프로젝트에서 곧 LOS 오류다.
	TestTrue(TEXT("앞쪽 사람을 더 강하게 피한다"), FromFront.Size() > FromRear.Size());
	TestEqual(TEXT("뒤쪽 가중치는 Anisotropy 배다"),
		FromRear.Size(), FromFront.Size() * Config.Anisotropy, FromFront.Size() * 0.02f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_ExactOverlapStaysFinite, "FestaFiesta.Crowd.Avoidance.ExactOverlapStaysFinite", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_ExactOverlapStaysFinite::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();

	// 완전히 같은 좌표. 스폰이 겹쳤을 때 실제로 일어나는 상황이다.
	// exp 인자 클램프와 SafeNormalize가 없으면 여기서 inf가 나오고,
	// 다음 스텝에 inf - inf = NaN이 되어 배열 전체로 퍼진다.
	const FVector2f Force = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(10.f, 10.f), FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(10.f, 10.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

	TestTrue(TEXT("완전 겹침에서도 유한하다"), FMath::IsFinite(Force.X) && FMath::IsFinite(Force.Y));
	TestTrue(TEXT("0이 아닌 힘으로 밀어낸다"), Force.Size() > 0.f);

	// 클램프를 사실상 열어 둔 상태(반발이 수백만 N이 되는 상태)에서도
	// 가속 클램프가 마지막 방어선으로 남아 있어야 한다. 방어선이 하나뿐이면
	// 계수 하나를 잘못 만진 날 NaN이 배열 전체로 퍼진다.
	FFF_CrowdAvoidanceConfig Loose = Config;
	Loose.MaxExponentArg = 700.f;
	const FVector2f LooseForce = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(0.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Loose);

	// 여기서 inf가 나오는 것 자체는 클램프 값을 잘못 준 결과다. 가속 클램프가 마지막 방어선이다.
	const FVector2f Accel = FFF_CrowdForceSolver::ClampAcceleration(
		LooseForce / PersonMassKg, Config.MaxAccelerationMps2);
	TestTrue(TEXT("가속 클램프를 통과한 값은 유한하고 상한 안이다"),
		FMath::IsFinite(Accel.Size()) && Accel.Size() <= Config.MaxAccelerationMps2 + 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_WallPushesInward, "FestaFiesta.Crowd.Avoidance.WallPushesInward", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_WallPushesInward::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();

	// y = 0 위에 놓인 벽. 사람은 y = +0.3에 있다.
	const FVector2f SegA(-10.f, 0.f);
	const FVector2f SegB(10.f, 0.f);

	const FVector2f Force = FFF_CrowdForceSolver::ComputeWallForce(
		FVector2f(0.f, 0.3f), FVector2f(0.f, -0.5f), PersonRadiusMeters, SegA, SegB, Config);

	TestTrue(TEXT("벽에서 멀어지는 방향(+Y)으로 밀어낸다"), Force.Y > 0.f);

	// 선분 끝점 밖에서도 유한해야 한다. 사각 경계의 모서리가 매 스텝 이 경로를 탄다.
	const FVector2f Corner = FFF_CrowdForceSolver::ComputeWallForce(
		FVector2f(10.2f, 0.1f), FVector2f::ZeroVector, PersonRadiusMeters, SegA, SegB, Config);
	TestTrue(TEXT("끝점 밖에서도 유한하다"), FMath::IsFinite(Corner.X) && FMath::IsFinite(Corner.Y));

	// 충분히 떨어지면 벽은 없는 것과 같다. 아니면 열린 광장에서도 사람이 중앙으로 밀린다.
	const FVector2f Far = FFF_CrowdForceSolver::ComputeWallForce(
		FVector2f(0.f, 3.f), FVector2f::ZeroVector, PersonRadiusMeters, SegA, SegB, Config);
	TestTrue(TEXT("멀리 떨어지면 벽 반발이 사라진다"), Far.Size() < 1.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_UnstuckIsDeterministicAndDiverges, "FestaFiesta.Crowd.Avoidance.UnstuckIsDeterministicAndDiverges", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_UnstuckIsDeterministicAndDiverges::RunTest(const FString& Parameters)
{
	const FVector2f Suggested(1.f, 0.f);

	const FVector2f A1 = FFF_CrowdForceSolver::ComputeUnstuckImpulse(1234, Suggested);
	const FVector2f A2 = FFF_CrowdForceSolver::ComputeUnstuckImpulse(1234, Suggested);
	const FVector2f B = FFF_CrowdForceSolver::ComputeUnstuckImpulse(1235, Suggested);

	// RNG를 쓰면 여기서 깨진다. 같은 개체는 항상 같은 방향으로 빠져나가야 한다.
	TestEqual(TEXT("같은 ID는 같은 방향"), A1.X, A2.X, 0.f);
	TestEqual(TEXT("같은 ID는 같은 방향"), A1.Y, A2.Y, 0.f);

	// 그러나 겹친 두 사람이 같은 방향으로 나가면 영원히 겹쳐 있다. 지터 자체는 필요하다.
	TestTrue(TEXT("다른 ID는 다른 방향으로 흩어진다"),
		!FMath::IsNearlyEqual(A1.Y, B.Y, 1.e-4f));

	TestEqual(TEXT("단위 벡터다"), A1.Size(), 1.f, 1.e-3f);

	// 제안 방향에서 ±60도 안이다. 반대로 보내면 원래 가려던 곳에서 멀어지기만 한다.
	const float CosAngle = FVector2f::DotProduct(A1, Suggested);
	TestTrue(TEXT("제안 방향에서 60도 이내다"), CosAngle >= FMath::Cos(PI / 3.f) - 1.e-3f);

	// 제안이 없으면(완전 겹침) 전방향에서 고르되 여전히 단위 벡터여야 한다.
	const FVector2f NoHint = FFF_CrowdForceSolver::ComputeUnstuckImpulse(7, FVector2f::ZeroVector);
	TestEqual(TEXT("제안이 없어도 단위 벡터다"), NoHint.Size(), 1.f, 1.e-3f);

	return true;
}

// =================================================================================================
// 브로드페이즈 격자
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdGrid_GatherIsAscendingAndBounded, "FestaFiesta.Crowd.Grid.GatherIsAscendingAndBounded", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdGrid_GatherIsAscendingAndBounded::RunTest(const FString& Parameters)
{
	// 0.5m 간격으로 21명을 한 줄로 세운다(-5m ~ +5m).
	TArray<FVector2f> Pos;
	TArray<uint8> Active;
	TArray<int32> ActiveIds;
	for (int32 i = 0; i < 21; ++i)
	{
		Pos.Add(FVector2f(-5.f + i * 0.5f, 0.f));
		Active.Add(1);
		ActiveIds.Add(i);
	}

	FFF_CrowdSpatialGrid Grid;
	Grid.Build(Pos, Active, ActiveIds, 2.f);

	TestTrue(TEXT("격자가 유효하다"), Grid.IsValid());
	TestEqual(TEXT("전원이 격자에 들어갔다"), Grid.GetNumItems(), 21);

	// 가운데 사람(인덱스 10, x=0) 주변 2m. 좌우 4명씩 + 자기 자신 = 9명이 반경 안이다.
	TArray<int32> Neighbours;
	Grid.GatherNearest(Pos, Pos[10], 2.f, 32, Neighbours);

	TestEqual(TEXT("반경 2m 안의 인원(자기 포함)"), Neighbours.Num(), 9);
	TestTrue(TEXT("자기 자신이 포함된다(호출자가 건너뛴다)"), Neighbours.Contains(10));

	// [중요] 오름차순이어야 한다. 힘을 더하는 순서가 다르면 부동소수 합산의 마지막 비트가
	// 달라지고, 그 차이가 60초 뒤엔 눈에 보이는 위치 차이가 된다.
	for (int32 k = 1; k < Neighbours.Num(); ++k)
	{
		if (Neighbours[k] <= Neighbours[k - 1])
		{
			AddError(TEXT("GatherNearest 결과가 인덱스 오름차순이 아니다 — 결정성이 깨진다"));
			return false;
		}
	}

	// 상한을 걸면 가까운 것부터 남고, 결과는 여전히 오름차순이다.
	Grid.GatherNearest(Pos, Pos[10], 2.f, 5, Neighbours);
	TestEqual(TEXT("상한을 넘지 않는다"), Neighbours.Num(), 5);
	TestTrue(TEXT("가장 가까운 쪽(자기 자신)이 남는다"), Neighbours.Contains(10));
	for (int32 k = 1; k < Neighbours.Num(); ++k)
	{
		if (Neighbours[k] <= Neighbours[k - 1])
		{
			AddError(TEXT("상한을 걸어 자른 뒤에도 오름차순이어야 한다"));
			return false;
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdGrid_InactiveIsFilteredAtBuild, "FestaFiesta.Crowd.Grid.InactiveIsFilteredAtBuild", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdGrid_InactiveIsFilteredAtBuild::RunTest(const FString& Parameters)
{
	TArray<FVector2f> Pos;
	TArray<uint8> Active;
	TArray<int32> ActiveIds;
	for (int32 i = 0; i < 5; ++i)
	{
		Pos.Add(FVector2f(i * 0.4f, 0.f));
		// 홀수 번호는 죽은 슬롯이다. 미러 배열은 압축하지 않으므로 자리는 남아 있다.
		Active.Add((i % 2 == 0) ? 1 : 0);
		ActiveIds.Add(i);
	}

	FFF_CrowdSpatialGrid Grid;
	Grid.Build(Pos, Active, ActiveIds, 2.f);

	TArray<int32> Neighbours;
	Grid.GatherNearest(Pos, Pos[0], 2.f, 32, Neighbours);

	// Active 필터링을 Build에서 하는 이유: GatherNearest 시점에 거르면 죽은 에이전트의
	// 낡은 위치가 "가장 가까운 N개" 자리를 차지해 살아있는 개체의 반발력이 조용히 약해진다.
	TestEqual(TEXT("살아있는 3명만 잡힌다"), Neighbours.Num(), 3);
	TestFalse(TEXT("죽은 슬롯은 들어오지 않는다"), Neighbours.Contains(1));

	// 무효 격자는 빈 결과를 낸다 — 여기서 어정쩡하게 진행하면 음수 인덱스 접근이 난다.
	FFF_CrowdSpatialGrid Empty;
	TestFalse(TEXT("빈 격자는 무효다"), Empty.IsValid());
	Empty.GatherNearest(Pos, Pos[0], 2.f, 32, Neighbours);
	TestEqual(TEXT("무효 격자는 빈 결과"), Neighbours.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdGrid_LeastCrowdedNeighbourIsStable, "FestaFiesta.Crowd.Grid.LeastCrowdedNeighbourIsStable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdGrid_LeastCrowdedNeighbourIsStable::RunTest(const FString& Parameters)
{
	// 왼쪽 셀에 여러 명, 오른쪽은 비어 있게 만든다.
	TArray<FVector2f> Pos;
	TArray<uint8> Active;
	TArray<int32> ActiveIds;
	for (int32 i = 0; i < 12; ++i)
	{
		Pos.Add(FVector2f(-3.f + (i % 3) * 0.1f, 0.f));
		Active.Add(1);
		ActiveIds.Add(i);
	}
	// 오른쪽 끝에 한 명만 두어 격자 폭을 넓힌다.
	Pos.Add(FVector2f(5.f, 0.f));
	Active.Add(1);
	ActiveIds.Add(12);

	FFF_CrowdSpatialGrid Grid;
	Grid.Build(Pos, Active, ActiveIds, 2.f);

	FVector2f First, Second;
	const bool bOk1 = Grid.FindLeastCrowdedNeighbourCell(Pos[0], First);
	const bool bOk2 = Grid.FindLeastCrowdedNeighbourCell(Pos[0], Second);

	TestTrue(TEXT("인접 셀을 찾았다"), bOk1 && bOk2);
	// 동률에서 순서가 흔들리면 같은 상황에서 다른 방향으로 빠져나가 재현성이 깨진다.
	TestEqual(TEXT("같은 상황에서 항상 같은 셀을 고른다"), First.X, Second.X, 0.f);
	TestEqual(TEXT("같은 상황에서 항상 같은 셀을 고른다"), First.Y, Second.Y, 0.f);

	return true;
}

// =================================================================================================
// StepAgent <-> 회피 배선
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_ZeroForceChangesNothing, "FestaFiesta.Crowd.Steering.ZeroForceChangesNothing", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_ZeroForceChangesNothing::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);

	CrowdSteering::FStepParams Params;

	// 회피가 꺼진 실행(Force = 0)은 이식 전과 완전히 같은 결과여야 한다.
	// 이 테스트가 "off 회귀"의 코드 레벨 근거다 — P0 기준선과 어긋나면 다른 것을 건드린 것이다.
	FUnitData WithZeroForce = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	WithZeroForce.TargetLocation = FVector(5000.f, 0.f, 0.f);

	for (int32 i = 0; i < 60; ++i)
	{
		CrowdSteering::StepAgent(WithZeroForce, Params, Alpha, FixedStep);
	}

	TestEqual(TEXT("Force가 0이면 옆으로 새지 않는다"),
		static_cast<float>(WithZeroForce.Position.Y), 0.f, 0.f);
	TestEqual(TEXT("Force는 스스로 바뀌지 않는다"),
		static_cast<float>(WithZeroForce.Force.SizeSquared()), 0.f, 0.f);

	// 같은 개체에 +Y 회피 가속도를 주면 옆으로 밀려야 한다. 안 밀리면 배선이 끊긴 것이다.
	FUnitData Pushed = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Pushed.TargetLocation = FVector(5000.f, 0.f, 0.f);
	for (int32 i = 0; i < 60; ++i)
	{
		Pushed.Force = FVector(0.f, 500.f, 0.f);	// 5 m/s^2를 UU/s^2로
		CrowdSteering::StepAgent(Pushed, Params, Alpha, FixedStep);
	}

	TestTrue(TEXT("회피 가속도가 실제로 속도에 반영된다"), Pushed.Position.Y > 1.f);
	TestTrue(TEXT("그래도 MaxSpeed는 넘지 않는다"), Pushed.Velocity.Size() <= Pushed.MaxSpeed + 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_StuckCounterOnlyCountsBlockedAgents, "FestaFiesta.Crowd.Steering.StuckCounterOnlyCountsBlockedAgents", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_StuckCounterOnlyCountsBlockedAgents::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);
	CrowdSteering::FStepParams Params;

	// 1) 정상적으로 걷는 사람은 막힌 것이 아니다.
	FUnitData Walking = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Walking.TargetLocation = FVector(5000.f, 0.f, 0.f);
	for (int32 i = 0; i < 30; ++i)
	{
		CrowdSteering::StepAgent(Walking, Params, Alpha, FixedStep);
	}
	TestEqual(TEXT("걷는 사람의 막힘 카운터는 0이다"), Walking.StuckSteps, 0);

	// 2) 도착해서 서 있는 사람도 막힌 것이 아니다.
	//    이걸 구분하지 않으면 체류·대기 중인 사람에게까지 탈출 가속이 걸린다.
	FUnitData Arrived = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Arrived.TargetLocation = FVector::ZeroVector;
	for (int32 i = 0; i < 30; ++i)
	{
		CrowdSteering::StepAgent(Arrived, Params, Alpha, FixedStep);
	}
	TestEqual(TEXT("도착해서 서 있는 사람의 카운터는 0이다"), Arrived.StuckSteps, 0);

	// 3) 갈 곳은 남았는데 못 움직이는 사람. 희망 속도를 0으로 만들어 재현한다
	//    (실제로는 사방이 막혀 반발력이 상쇄되는 상황이다).
	FUnitData Blocked = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Blocked.TargetLocation = FVector(5000.f, 0.f, 0.f);
	Blocked.Speed = 0.f;
	for (int32 i = 0; i < 30; ++i)
	{
		CrowdSteering::StepAgent(Blocked, Params, Alpha, FixedStep);
	}
	TestEqual(TEXT("막힌 사람은 스텝마다 카운터가 오른다"), Blocked.StuckSteps, 30);

	// 다시 움직이면 즉시 0으로 돌아간다. 안 그러면 한 번 막힌 사람이 계속 탈출 가속을 받는다.
	Blocked.Speed = 150.f;
	for (int32 i = 0; i < 5; ++i)
	{
		CrowdSteering::StepAgent(Blocked, Params, Alpha, FixedStep);
	}
	TestEqual(TEXT("다시 움직이면 카운터가 초기화된다"), Blocked.StuckSteps, 0);

	return true;
}

// =================================================================================================
// 지표 — 셀 크기 단일 출처 + 유량(Throughput)
//
// 이 절의 테스트가 지키는 것은 "같은 밀도가 두 개의 면적으로 리포트되지 않는다"다.
// 셀 크기가 두 곳에 적혀 있으면 컴파일도 되고 크래시도 없고, 리포트만 조용히 틀린다.
// =================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdMetrics_CellSizeHasSingleSource, "FestaFiesta.Crowd.Metrics.CellSizeHasSingleSource", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdMetrics_CellSizeHasSingleSource::RunTest(const FString& Parameters)
{
	// m와 UU 표현이 서로 100배여야 한다. 둘을 따로 적어 두었으므로 어긋날 수 있다 —
	// 어긋나면 격자는 2m로 짓고 리포트는 5m로 나가는, 정확히 막으려던 상태가 된다.
	TestEqual(TEXT("m와 UU 표현이 100배 관계다"),
		FFF_CrowdMetrics::AnalysisCellSizeUU,
		FFF_CrowdMetrics::AnalysisCellSizeMeters * 100.f, 0.001f);

	// ReadModel 기본값이 단일 출처를 그대로 쓴다.
	const FFF_CrowdReadModel DefaultModel;
	TestEqual(TEXT("ReadModel 기본 셀 크기 = 단일 출처"),
		DefaultModel.CellSizeMeters, FFF_CrowdMetrics::AnalysisCellSizeMeters, 0.f);

	// 격자 기본값도 같은 곳에서 온다.
	FCrowdDensityGrid Grid;
	Grid.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), FFF_CrowdMetrics::AnalysisCellSizeUU);

	TestEqual(TEXT("격자 셀 크기(m) = 단일 출처"),
		Grid.GetCellSizeMeters(), FFF_CrowdMetrics::AnalysisCellSizeMeters, 0.001f);
	TestEqual(TEXT("셀 면적 = 셀 크기의 제곱"),
		Grid.GetCellAreaSqM(),
		FFF_CrowdMetrics::AnalysisCellSizeMeters * FFF_CrowdMetrics::AnalysisCellSizeMeters, 0.001f);

	// 2m 격자에서 한 칸에 4명이면 1.00 명/㎡다. 5m 격자였다면 0.16이 되어 LOS가 A로 떨어진다 —
	// 이 숫자가 "면적이 6.25배 다르면 등급이 통째로 달라진다"의 실제 크기다.
	FCrowdDensityGrid Counted;
	Counted.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), FFF_CrowdMetrics::AnalysisCellSizeUU);
	TArray<FUnitData> Units;
	FFF_CrowdTestHelpers::AddUnitsAt(Units, FVector(100.f, 100.f, 0.f), 4);
	Counted.Update(Units);

	TestEqual(TEXT("4㎡에 4명 = 1.00 명/m^2"),
		Counted.GetDensityAtLocation(FVector(100.f, 100.f, 0.f)), 1.f, 0.001f);
	TestEqual(TEXT("그 밀도의 LOS는 D다(2m 격자 기준)"),
		static_cast<int32>(FCrowdDensityGrid::DensityToLOS(1.f)), static_cast<int32>(ECrowdLOS::D));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdMetrics_ThroughputCountsCrossingsOnly, "FestaFiesta.Crowd.Metrics.ThroughputCountsCrossingsOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdMetrics_ThroughputCountsCrossingsOnly::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid;
	Grid.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), 200.f);

	TArray<FUnitData> Units;
	Units.Add(FFF_CrowdTestHelpers::MakeUnitAt(FVector(50.f, 50.f, 0.f)));	// 셀 (0,0)

	// 1) 첫 등장은 진입이다. 축제에 들어온 사람이 그 칸을 처음 밟은 것이다.
	Grid.Update(Units);
	TestEqual(TEXT("첫 등장이 진입 1회로 잡힌다"), Grid.GetCellThroughput()[0], 1);

	// 2) 같은 칸 안에서 움직여도 진입이 아니다. 프레임을 여러 번 돌려도 늘지 않는다.
	//    이게 성립해야 "인원 많음 + 유량 0 = 막힘" 판정이 의미를 갖는다.
	Units[0].Position = FVector(150.f, 150.f, 0.f);
	Grid.Update(Units);
	Grid.Update(Units);
	Grid.Update(Units);
	TestEqual(TEXT("같은 칸에 머무는 동안은 늘지 않는다"), Grid.GetCellThroughput()[0], 1);

	// 3) 옆 칸으로 넘어가면 '목적 칸'에서 1 오른다. 떠난 칸은 그대로다.
	Units[0].Position = FVector(250.f, 50.f, 0.f);	// 셀 (1,0)
	Grid.Update(Units);
	TestEqual(TEXT("목적 칸의 진입이 1"), Grid.GetCellThroughput()[1], 1);
	TestEqual(TEXT("떠난 칸은 변하지 않는다"), Grid.GetCellThroughput()[0], 1);

	// 4) 되돌아오면 다시 진입이다. 왕복하는 사람은 양쪽 유량을 모두 올린다.
	Units[0].Position = FVector(50.f, 50.f, 0.f);
	Grid.Update(Units);
	TestEqual(TEXT("되돌아온 칸의 진입이 2"), Grid.GetCellThroughput()[0], 2);

	// 5) 인원(순간값)은 항상 1명이다 — 유량과 초기화 주기가 다르다는 것의 확인
	TestEqual(TEXT("점유는 누적되지 않는다"), Grid.GetCellCounts()[0], 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdMetrics_ThroughputWindowResetsWithoutFakeFlow, "FestaFiesta.Crowd.Metrics.ThroughputWindowResetsWithoutFakeFlow", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdMetrics_ThroughputWindowResetsWithoutFakeFlow::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid;
	Grid.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), 200.f);

	TArray<FUnitData> Units;
	Units.Add(FFF_CrowdTestHelpers::MakeUnitAt(FVector(50.f, 50.f, 0.f)));
	Grid.Update(Units);
	TestEqual(TEXT("구간 안에서 진입 1"), Grid.GetCellThroughput()[0], 1);

	// 구간을 끊는다(스냅샷 발행 시점).
	Grid.ResetThroughputWindow();
	TestEqual(TEXT("구간을 끊으면 0"), Grid.GetCellThroughput()[0], 0);

	// ★ 구간을 끊은 뒤 제자리에 있는 사람이 다시 진입으로 세어지면 안 된다.
	// 직전 셀 기억까지 지웠다면 여기서 1이 되고, 유량이 구간마다 인원수만큼 부풀려진다.
	Grid.Update(Units);
	Grid.Update(Units);
	TestEqual(TEXT("제자리인 사람은 새 구간에서도 0이다"), Grid.GetCellThroughput()[0], 0);

	// 점유는 계속 잡힌다 — 유량 0인데 인원이 있는 상태, 즉 '막힘'의 신호다
	TestEqual(TEXT("점유는 그대로 1"), Grid.GetCellCounts()[0], 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdMetrics_SlotReuseIsCountedAsNewEntry, "FestaFiesta.Crowd.Metrics.SlotReuseIsCountedAsNewEntry", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdMetrics_SlotReuseIsCountedAsNewEntry::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid;
	Grid.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), 200.f);

	TArray<FUnitData> Units;
	Units.Add(FFF_CrowdTestHelpers::MakeUnitAt(FVector(50.f, 50.f, 0.f)));	// 셀 (0,0)
	Grid.Update(Units);

	// 퇴장. 슬롯은 남는다(Agent 풀은 슬롯을 재사용한다).
	Units[0].bIsActive = false;
	Grid.Update(Units);
	TestEqual(TEXT("퇴장자는 점유에서 빠진다"), Grid.GetCellCounts()[0], 0);

	// ★ 같은 슬롯을 같은 칸에서 재사용. 다른 사람이 그 칸에 들어온 것이므로 진입이다.
	// 비활성 시점에 직전 셀 기억을 지우지 않으면 "이미 그 칸에 있던 사람"으로 보여
	// 이 진입이 통째로 누락된다 — 스폰 지점의 유량이 항상 0으로 나오는 증상이 된다.
	Units[0].bIsActive = true;
	Grid.Update(Units);
	TestEqual(TEXT("슬롯 재사용이 새 진입으로 잡힌다"), Grid.GetCellThroughput()[0], 2);

	// 반대 방향 오염도 없어야 한다: 퇴장자의 마지막 칸에서 '이동해 온' 것으로 잡히면
	// 맵 반대편 칸의 유량이 엉뚱하게 오른다.
	Units[0].bIsActive = false;
	Grid.Update(Units);
	Units[0].Position = FVector(850.f, 850.f, 0.f);	// 멀리 떨어진 칸에서 재스폰
	Units[0].bIsActive = true;
	Grid.Update(Units);

	const int32 FarCell = 4 * Grid.GetNumX() + 4;	// (4,4)
	TestEqual(TEXT("재스폰 칸에 진입 1"), Grid.GetCellThroughput()[FarCell], 1);
	TestEqual(TEXT("옛 칸의 유량은 그대로다"), Grid.GetCellThroughput()[0], 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdMetrics_RebuildClearsStaleCellMemory, "FestaFiesta.Crowd.Metrics.RebuildClearsStaleCellMemory", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdMetrics_RebuildClearsStaleCellMemory::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid;
	Grid.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), 200.f);

	TArray<FUnitData> Units;
	Units.Add(FFF_CrowdTestHelpers::MakeUnitAt(FVector(50.f, 50.f, 0.f)));
	Grid.Update(Units);

	// 시뮬 영역이 바뀌면(ConfigureSimArea) 격자를 다시 짓는다.
	// 셀 인덱스 공간 자체가 달라지므로 직전 셀 기억이 남아 있으면 엉뚱한 셀을 가리킨다.
	Grid.Build(FVector::ZeroVector, FVector2D(2000.f, 2000.f), 400.f);

	TestEqual(TEXT("재구축하면 유량이 비어 있다"), Grid.GetCellThroughput()[0], 0);

	Grid.Update(Units);
	TestEqual(TEXT("재구축 후 첫 갱신은 새 진입으로 잡힌다"), Grid.GetCellThroughput()[0], 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdWorld_ReadModelIsPublishedOnInterval, "FestaFiesta.Crowd.World.ReadModelIsPublishedOnInterval", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdWorld_ReadModelIsPublishedOnInterval::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	if (!TestWorld.World)
	{
		AddError(TEXT("테스트 월드를 만들지 못했다"));
		return false;
	}

	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();
	if (!CrowdSim)
	{
		AddError(TEXT("CrowdSimulationSubsystem이 없다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 5000.f, 8, 32, AFF_BaseCrowd::StaticClass());
	CrowdSim->SpawnAgents(8, FVector::ZeroVector, 500.f);

	// 발행 전. 스냅샷은 아직 비어 있어야 한다 — 기본값이 이미 채워진 것처럼 보이면
	// 소비자가 "0회 발행"과 "빈 격자"를 구별할 수 없다.
	TestEqual(TEXT("Tick 전에는 발행 0회"), CrowdSim->GetSnapshotCount(), 0);

	// 주기를 짧게 해서 테스트 시간을 줄인다. 손잡이가 실제로 먹는지도 같이 본다.
	CrowdSim->SnapshotIntervalSteps = 10;
	FFF_CrowdWorldTestHelpers::TickFor(*CrowdSim, 1.0f);	// 30Hz면 약 30스텝 = 3회 발행

	TestTrue(TEXT("주기마다 발행된다"), CrowdSim->GetSnapshotCount() >= 2);

	const FFF_CrowdReadModel& ReadModel = CrowdSim->GetCrowdReadModel();

	// ★ 셀 크기는 격자에서 읽어 채워야 한다. 상수를 다시 적었다면 여기서 어긋난다.
	TestEqual(TEXT("스냅샷 셀 크기 = 격자 셀 크기"),
		ReadModel.CellSizeMeters, CrowdSim->GetDensityGrid().GetCellSizeMeters(), 0.001f);
	// FVector는 double 기반이라 float 기대값과 섞으면 TestEqual 오버로드가 모호해진다. 명시적으로 맞춘다.
	TestEqual(TEXT("스냅샷 원점 = 격자 원점"),
		static_cast<float>(ReadModel.GridOriginUU.X),
		static_cast<float>(CrowdSim->GetDensityGrid().GetOriginUU().X), 0.01f);

	TestTrue(TEXT("유효 셀이 담겼다"), ReadModel.OccupiedCells.Num() > 0);
	TestTrue(TEXT("스냅샷 프레임이 기록됐다"), ReadModel.SnapshotFrame != INDEX_NONE);

	// 인원 0 + 유량 0인 칸은 담기지 않는다. 그리드는 수천 칸인데
	// 8명이 만드는 유효 칸은 한 자리 수여야 한다.
	TestTrue(TEXT("빈 칸은 담기지 않는다"),
		ReadModel.OccupiedCells.Num() < CrowdSim->GetDensityGrid().GetNumX() * CrowdSim->GetDensityGrid().GetNumY());

	int32 SumOccupancy = 0;
	for (const FFF_CrowdCellSample& Cell : ReadModel.OccupiedCells)
	{
		SumOccupancy += Cell.Occupancy;

		// 밀도는 인원/면적이어야 한다. 여기가 틀리면 면적을 두 번 나눈 것이다.
		const float Expected = Cell.Occupancy / (ReadModel.CellSizeMeters * ReadModel.CellSizeMeters);
		TestEqual(TEXT("밀도 = 인원 / 셀 면적"), Cell.DensityPerSqm, Expected, 0.001f);
	}

	TestEqual(TEXT("스냅샷 인원 합 = 격자 집계 인원"),
		SumOccupancy, CrowdSim->GetDensityGrid().GetSampledUnitCount());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
