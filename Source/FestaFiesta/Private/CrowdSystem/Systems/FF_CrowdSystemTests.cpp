// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdActorPool.h"
#include "CrowdSystem/Systems/FF_CrowdDensityGrid.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
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

#endif // WITH_DEV_AUTOMATION_TESTS
