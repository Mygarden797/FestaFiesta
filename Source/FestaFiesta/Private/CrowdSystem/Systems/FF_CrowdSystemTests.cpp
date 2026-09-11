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
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Tests/AutomationCommon.h"
#include "Time/FF_FestivalStageSubsystem.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdDensity_BuildSizesGridWithCeiling, "FestaFiesta.Crowd.Density.BuildSizesGridWithCeiling", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdDensity_BuildSizesGridWithCeiling::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid;
	Grid.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), 200.f);

	TestTrue(TEXT("그리드가 만들어졌다"), Grid.IsBuilt());
	TestEqual(TEXT("1000UU / 200UU = 5칸"), Grid.GetNumX(), 5);
	TestEqual(TEXT("1000UU / 200UU = 5칸"), Grid.GetNumY(), 5);

	FCrowdDensityGrid Ragged;
	Ragged.Build(FVector::ZeroVector, FVector2D(1050.f, 1050.f), 200.f);
	TestEqual(TEXT("1050UU는 올림해서 6칸이어야 한다"), Ragged.GetNumX(), 6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdDensity_DensityIsCountPerCellArea, "FestaFiesta.Crowd.Density.DensityIsCountPerCellArea", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdDensity_DensityIsCountPerCellArea::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid = FFF_CrowdTestHelpers::MakeTestGrid();

	TArray<FUnitData> Units;
	const FVector CellCenter(100.f, 100.f, 0.f);
	FFF_CrowdTestHelpers::AddUnitsAt(Units, CellCenter, 8);

	Grid.Update(Units);

	TestEqual(TEXT("8명 전부 집계됐다"), Grid.GetSampledUnitCount(), 8);
	TestEqual(TEXT("8명 / 4m^2 = 2.0명/m^2"), Grid.GetDensityAtLocation(CellCenter), 2.0f, 0.001f);
	TestEqual(TEXT("최대 밀도도 같은 값"), Grid.GetMaxDensity(), 2.0f, 0.001f);

	TestEqual(TEXT("빈 셀은 0"), Grid.GetDensityAtLocation(FVector(700.f, 700.f, 0.f)), 0.f, 0.001f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdDensity_InactiveAndOutOfGridAreExcluded, "FestaFiesta.Crowd.Density.InactiveAndOutOfGridAreExcluded", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdDensity_InactiveAndOutOfGridAreExcluded::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid = FFF_CrowdTestHelpers::MakeTestGrid();

	TArray<FUnitData> Units;
	const FVector CellCenter(100.f, 100.f, 0.f);

	FFF_CrowdTestHelpers::AddUnitsAt(Units, CellCenter, 4);

	for (int32 i = 0; i < 4; ++i)
	{
		FUnitData Idle = FFF_CrowdTestHelpers::MakeUnitAt(CellCenter);
		Idle.bIsActive = false;
		Units.Add(Idle);
	}

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

	TestEqual(TEXT("반복 Update에도 밀도는 그대로"), Grid.GetDensityAtLocation(First), 1.0f, 0.001f);

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

	FFF_CrowdTestHelpers::AddUnitsAt(Units, FVector(100.f, 100.f, 0.f), 4);

	FFF_CrowdTestHelpers::AddUnitsAt(Units, FVector(500.f, 100.f, 0.f), 8);

	FFF_CrowdTestHelpers::AddUnitsAt(Units, FVector(900.f, 900.f, 0.f), 12);

	Grid.Update(Units);

	TestEqual(TEXT("LOS D 초과 셀은 2개"), Grid.GetCongestedCellCount(), 2);
	TestEqual(TEXT("최대 밀도는 3.0명/m^2"), Grid.GetMaxDensity(), 3.0f, 0.001f);
	TestEqual(TEXT("전원 집계됐다"), Grid.GetSampledUnitCount(), 24);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_FixedStepConsumesWholeStepsOnly, "FestaFiesta.Crowd.Steering.FixedStepConsumesWholeStepsOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_FixedStepConsumesWholeStepsOnly::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;
	constexpr int32 MaxSteps = 3;

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

	const int32 HitchFrame = CrowdSteering::ConsumeFixedSteps(Accumulator, 1.0f, FixedStep, MaxSteps);
	TestEqual(TEXT("히칭에도 한도까지만 돈다"), HitchFrame, MaxSteps);

	TestEqual(TEXT("따라잡기 실패 시 잔여 시간을 버린다"), Accumulator, 0.f, 0.0001f);

	const int32 NextFrame = CrowdSteering::ConsumeFixedSteps(Accumulator, 1.f / 60.f, FixedStep, MaxSteps);
	TestEqual(TEXT("다음 프레임은 정상으로 돌아온다"), NextFrame, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_AgentMovesTowardTarget, "FestaFiesta.Crowd.Steering.AgentMovesTowardTarget", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_AgentMovesTowardTarget::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;

	FUnitData Unit = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Unit.TargetLocation = FVector(5000.f, 0.f, 0.f);

	CrowdSteering::FStepParams Params;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);

	const float StartDistance = FVector::Dist2D(Unit.Position, Unit.TargetLocation);

	for (int32 i = 0; i < 30; ++i)
	{
		CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
	}

	const float EndDistance = FVector::Dist2D(Unit.Position, Unit.TargetLocation);

	TestTrue(TEXT("목적지에 가까워졌다"), EndDistance < StartDistance);
	TestTrue(TEXT("+X 방향으로 이동했다"), Unit.Position.X > 0.f);
	TestEqual(TEXT("옆으로 새지 않았다"), static_cast<float>(Unit.Position.Y), 0.f, 0.01f);
	TestEqual(TEXT("높이는 변하지 않는다"), static_cast<float>(Unit.Position.Z), 0.f, 0.001f);
	TestEqual(TEXT("+X를 바라본다"), static_cast<float>(Unit.Rotation.Yaw), 0.f, 0.5f);

	TestTrue(TEXT("PrevPosition이 현재 위치보다 뒤에 있다"), Unit.PrevPosition.X < Unit.Position.X);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_SpeedRampsUpAndClampsToMax, "FestaFiesta.Crowd.Steering.SpeedRampsUpAndClampsToMax", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_SpeedRampsUpAndClampsToMax::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;

	FUnitData Unit = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Unit.TargetLocation = FVector(100000.f, 0.f, 0.f);
	Unit.Speed = 150.f;
	Unit.MaxSpeed = 150.f;

	CrowdSteering::FStepParams Params;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);

	CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
	TestTrue(TEXT("첫 스텝에 최고 속도가 되지 않는다"), Unit.Velocity.Size() < Unit.Speed);
	TestTrue(TEXT("그래도 움직이기는 한다"), Unit.Velocity.Size() > 0.f);

	for (int32 i = 0; i < 120; ++i)
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

	TestFalse(TEXT("출발 시점에는 도착이 아니다"), CrowdSteering::HasArrived(Unit, Unit.ArrivalRadiusUU));

	bool bArrived = false;
	float MaxOvershootX = 0.f;
	for (int32 i = 0; i < 900 && !bArrived; ++i)
	{
		CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
		MaxOvershootX = FMath::Max(MaxOvershootX, Unit.Position.X);
		bArrived = CrowdSteering::HasArrived(Unit, Unit.ArrivalRadiusUU);
	}

	TestTrue(TEXT("목적지에 도착한다"), bArrived);

	TestTrue(TEXT("목적지를 크게 지나치지 않는다"), MaxOvershootX <= Unit.TargetLocation.X + 1.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_TurnRateIsLimited, "FestaFiesta.Crowd.Steering.TurnRateIsLimited", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_TurnRateIsLimited::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;

	FUnitData Unit = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Unit.Velocity = FVector(150.f, 0.f, 0.f);
	Unit.Rotation = FRotator::ZeroRotator;
	Unit.TargetLocation = FVector(-5000.f, 0.f, 0.f);

	CrowdSteering::FStepParams Params;
	Params.MaxTurnRateDegrees = 180.f;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);

	const float BeforeYaw = Unit.Rotation.Yaw;
	CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
	const float TurnedDegrees = FMath::Abs(FMath::FindDeltaAngleDegrees(BeforeYaw, Unit.Rotation.Yaw));

	TestTrue(TEXT("한 스텝 회전량이 한도 이하"), TurnedDegrees <= 6.f + 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_IsDeterministic, "FestaFiesta.Crowd.Steering.IsDeterministic", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_IsDeterministic::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;

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
	TestEqual(TEXT("SmoothTime 0이면 즉시 반영"), CrowdSteering::ComputeVelocityAlpha(0.f, 1.f / 30.f), 1.f);

	const float Normal = CrowdSteering::ComputeVelocityAlpha(0.35f, 1.f / 30.f);
	TestTrue(TEXT("정상 범위 안"), Normal > 0.f && Normal < 1.f);

	const float Extreme = CrowdSteering::ComputeVelocityAlpha(0.01f, 1.0f);
	TestTrue(TEXT("Dt >> SmoothTime 이어도 1 이하"), Extreme <= 1.f);

	const float Slow = CrowdSteering::ComputeVelocityAlpha(2.0f, 1.f / 30.f);
	TestTrue(TEXT("SmoothTime이 크면 계수가 작다"), Slow < Normal);

	return true;
}

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

	TSet<int32> UniqueSlots(AssignedSlots);
	TestEqual(TEXT("같은 슬롯이 두 번 나가지 않았다"), UniqueSlots.Num(), 3);

	TestFalse(TEXT("초과분은 미바인딩"), Pool.IsBound(9));
	TestTrue(TEXT("불변식 유지"), FFF_CrowdPoolTestHelpers::HoldsInvariant(Pool));

	Pool.Release(0);
	const int32 Recycled = Pool.Bind(9);
	TestTrue(TEXT("해제된 슬롯이 재사용된다"), Recycled != INDEX_NONE);
	TestTrue(TEXT("불변식 유지"), FFF_CrowdPoolTestHelpers::HoldsInvariant(Pool));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPool_ChurnKeepsInvariant, "FestaFiesta.Crowd.Pool.ChurnKeepsInvariant", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPool_ChurnKeepsInvariant::RunTest(const FString& Parameters)
{
	FCrowdActorPoolBinding Pool;
	Pool.Initialise(16, 64);

	FRandomStream Rng(1234);

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

	for (int32 AgentIndex = 0; AgentIndex < 64; ++AgentIndex)
	{
		Pool.Release(AgentIndex);
	}
	TestEqual(TEXT("전부 해제하면 슬롯이 전량 회수된다"), Pool.GetFreeSlotCount(), 16);
	TestEqual(TEXT("바인딩 수 0"), Pool.GetBoundCount(), 0);

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

	FCrowdActorPoolBinding Empty;
	Empty.Initialise(0, 10);
	TestEqual(TEXT("슬롯 0개면 바인딩 불가"), Empty.Bind(0), (int32)INDEX_NONE);
	TestTrue(TEXT("불변식 유지"), FFF_CrowdPoolTestHelpers::HoldsInvariant(Empty));

	return true;
}

namespace FFF_CrowdWorldTestHelpers
{
	constexpr int32 MaxVisitorPartySize = 6;

	void TestWholePartySpawnCount(FAutomationTestBase& Test, const TCHAR* Context,
		int32 Requested, int32 Spawned)
	{
		Test.TestTrue(
			*FString::Printf(TEXT("%s: 실제 인원이 0보다 크다 (%d명)"), Context, Spawned),
			Spawned > 0);
		Test.TestTrue(
			*FString::Printf(TEXT("%s: 실제 인원이 요청 %d명을 넘지 않는다 (%d명)"),
				Context, Requested, Spawned),
			Spawned <= Requested);
		Test.TestTrue(
			*FString::Printf(TEXT("%s: whole-party 부족분이 최대 파티 크기보다 작다 (%d명)"),
				Context, Requested - Spawned),
			Requested - Spawned < MaxVisitorPartySize);
	}

	// 엔진 공용 테스트 월드가 GameInstance 연결과 Begin/EndPlay, Subsystem 종료를 모두 맡는다.
	// Stage는 Crowd Tick의 런타임 전제만 세우고 다른 시스템의 전환 부작용은 만들지 않는다.
	struct FScopedTestWorld
	{
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;

		FScopedTestWorld()
		{
			if (!TestWorld.CreateTestWorld(EWorldType::Game))
			{
				return;
			}

			World = TestWorld.GetTestWorld();
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			UFF_FestivalStageSubsystem* Stage = GameInstance
				? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>()
				: nullptr;
			if (!World || !GameInstance || !Stage)
			{
				World = nullptr;
				return;
			}

			Stage->CurrentStage = EFF_FestivalStage::Live;
			if (!TestWorld.BeginPlayInTestWorld())
			{
				World = nullptr;
			}
		}

		~FScopedTestWorld() = default;

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

	TestEqual(TEXT("활성 Agent 없음"), CrowdSim->GetActiveAgentCount(), 0);
	TestEqual(TEXT("보이는 Actor 없음"), FFF_CrowdWorldTestHelpers::CountVisibleActors(*CrowdSim), 0);

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
	constexpr int32 ActorPoolSize = 8;
	constexpr int32 Requested = 5;

	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(
		FVector::ZeroVector, 10000.f, ActorPoolSize, 20, AFF_BaseCrowd::StaticClass());

	const int32 Spawned = CrowdSim->SpawnAgents(Requested, FVector::ZeroVector, 500.f);

	FFF_CrowdWorldTestHelpers::TestWholePartySpawnCount(
		*this, TEXT("Actor 바인딩 스폰"), Requested, Spawned);
	TestEqual(TEXT("활성 Agent 수 = 실제 스폰 수"), CrowdSim->GetActiveAgentCount(), Spawned);
	TestEqual(TEXT("도착 커서 = 실제 스폰 수"), CrowdSim->GetArrivalSpawnedCount(), Spawned);
	TestEqual(TEXT("바인딩 Actor 수 = 실제 스폰 수"), CrowdSim->GetBoundActorCount(), Spawned);
	TestEqual(TEXT("보이는 Actor 수 = 실제 스폰 수"),
		FFF_CrowdWorldTestHelpers::CountVisibleActors(*CrowdSim), Spawned);
	TestEqual(TEXT("빈 Actor 슬롯 = 풀 크기 - 실제 스폰 수"),
		CrowdSim->PoolBinding.GetFreeSlotCount(), ActorPoolSize - Spawned);

	for (int32 AgentIndex = 0; AgentIndex < Spawned; ++AgentIndex)
	{
		TestTrue(*FString::Printf(TEXT("스폰된 Agent %d는 활성"), AgentIndex),
			CrowdSim->IsAgentActive(AgentIndex));
	}

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

	constexpr int32 Requested = 4;
	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 4, Requested, AFF_BaseCrowd::StaticClass());
	const int32 Spawned = CrowdSim->SpawnAgents(Requested, FVector::ZeroVector, 0.f);
	FFF_CrowdWorldTestHelpers::TestWholePartySpawnCount(
		*this, TEXT("Movement"), Requested, Spawned);

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

	const float Gap = FVector::Dist2D(EndAgentPosition, EndActorLocation);
	TestTrue(FString::Printf(TEXT("Actor가 Agent를 따라간다 (간격 %.1fUU)"), Gap), Gap < 10.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdWorld_ActorShortageDoesNotAffectMetrics, "FestaFiesta.Crowd.World.ActorShortageDoesNotAffectMetrics", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdWorld_ActorShortageDoesNotAffectMetrics::RunTest(const FString& Parameters)
{
	constexpr int32 ActorPoolSize = 3;
	constexpr int32 Requested = 12;

	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(
		FVector::ZeroVector, 10000.f, ActorPoolSize, Requested, AFF_BaseCrowd::StaticClass());

	const int32 Spawned = CrowdSim->SpawnAgents(Requested, FVector::ZeroVector, 300.f);
	const int32 ExpectedBoundActors = FMath::Min(Spawned, ActorPoolSize);

	FFF_CrowdWorldTestHelpers::TestWholePartySpawnCount(
		*this, TEXT("Actor shortage"), Requested, Spawned);
	TestTrue(TEXT("실제 스폰 수가 Actor 풀보다 크다"), Spawned > ActorPoolSize);
	TestEqual(TEXT("활성 Agent 수 = 실제 스폰 수"), CrowdSim->GetActiveAgentCount(), Spawned);
	TestEqual(TEXT("바인딩 Actor 수 = min(실제 스폰, Actor 풀)"),
		CrowdSim->GetBoundActorCount(), ExpectedBoundActors);
	TestEqual(TEXT("보이는 Actor 수 = 바인딩 Actor 수"),
		FFF_CrowdWorldTestHelpers::CountVisibleActors(*CrowdSim), ExpectedBoundActors);

	FFF_CrowdWorldTestHelpers::TickFor(*CrowdSim, 0.5f);

	TestEqual(TEXT("밀도 집계 수 = 실제 스폰 수"),
		CrowdSim->GetDensityGrid().GetSampledUnitCount(), Spawned);
	TestTrue(TEXT("밀도가 0보다 크다"), CrowdSim->GetMaxDensity() > 0.f);

	return true;
}

namespace FFF_CounterFlowScenario
{
	constexpr int32 PerSide = 40;			// 한쪽 40명, 총 80명
	constexpr float StartOffsetUU = 1200.f;	// 시작 중심 ±12m
	constexpr float SpawnRadiusUU = 300.f;	// 3m 반경에 40명 -> 1.4명/m^2, 흐르는 인파 수준
	constexpr float TargetOffsetUU = 4000.f;// 목적지 ±40m. 20초 안에는 도착하지 않는 거리다
	constexpr float CrossLineUU = 400.f;	// 반대편 4m를 넘어가면 '통과'로 센다
	constexpr float SimSeconds = 20.f;

	struct FResult
	{
		int32 Spawned = 0;
		int32 Crossed = 0;			// 반대편으로 넘어간 인원(양쪽 합)
		float MeanProgressUU = 0.f;	// 목적지 방향 평균 전진량
		float MaxDensity = 0.f;
		int32 ActiveAtEnd = 0;

		// 끝난 시점의 이동 상태. "안 움직인 사람이 도착해서 선 것인지 막힌 것인지"를 가른다
		FFF_CrowdFlowBreakdown Flow;
	};

	FResult Run(UFF_CrowdSimulationSubsystem& CrowdSim, bool bAvoidance, bool bLegacyClamps,
	            bool bDrive, float LaneBias, float MaxOpposing)
	{
		CrowdSim.bEnablePartyGrouping = false;
		CrowdSim.bEnableAvoidance = bAvoidance;
		CrowdSim.bAvoidanceDrivesVelocity = bDrive;
		CrowdSim.AvoidanceConfig.LaneBiasGain = LaneBias;
		CrowdSim.AvoidanceConfig.MaxOpposingAccelMps2 = MaxOpposing;

		if (bLegacyClamps)
		{
			CrowdSim.AvoidanceConfig.MaxSocialAccelMps2 = CrowdSim.AvoidanceConfig.MaxAccelerationMps2;
		}

		CrowdSim.InitializeSimulation(FVector::ZeroVector, 10000.f, 2, PerSide * 2 + 8,
			AFF_BaseCrowd::StaticClass());

		const int32 SpawnedLeft = CrowdSim.SpawnAgents(PerSide, FVector(-StartOffsetUU, 0.f, 0.f), SpawnRadiusUU);
		const int32 SpawnedRight = CrowdSim.SpawnAgents(PerSide, FVector(StartOffsetUU, 0.f, 0.f), SpawnRadiusUU);

		const int32 Total = SpawnedLeft + SpawnedRight;
		TArray<float> StartX;
		StartX.Reserve(Total);
		for (int32 i = 0; i < Total; ++i)
		{
			StartX.Add(static_cast<float>(CrowdSim.GetAgentPosition(i).X));
		}

		constexpr float FrameDelta = 1.f / 60.f;
		const int32 FrameCount = FMath::CeilToInt(SimSeconds / FrameDelta);
		for (int32 Frame = 0; Frame < FrameCount; ++Frame)
		{
			for (int32 i = 0; i < Total; ++i)
			{
				const float TargetX = (StartX[i] < 0.f) ? TargetOffsetUU : -TargetOffsetUU;
				CrowdSim.SetAgentDestination(i, FVector(TargetX, 0.f, 0.f));
			}
			CrowdSim.Tick(FrameDelta);
		}

		FResult Out;
		Out.Spawned = Total;
		Out.MaxDensity = CrowdSim.GetMaxDensity();
		Out.ActiveAtEnd = CrowdSim.GetActiveAgentCount();
		Out.Flow = CrowdSim.GetFlowBreakdown();

		float ProgressSum = 0.f;
		for (int32 i = 0; i < Total; ++i)
		{
			const float EndX = static_cast<float>(CrowdSim.GetAgentPosition(i).X);
			const bool bGoesRight = (StartX[i] < 0.f);

			ProgressSum += bGoesRight ? (EndX - StartX[i]) : (StartX[i] - EndX);

			if (bGoesRight ? (EndX > CrossLineUU) : (EndX < -CrossLineUU))
			{
				++Out.Crossed;
			}
		}

		Out.MeanProgressUU = (Total > 0) ? (ProgressSum / Total) : 0.f;
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_CounterFlowPathOpens, "FestaFiesta.Crowd.Avoidance.CounterFlowPathOpens", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_CounterFlowPathOpens::RunTest(const FString& Parameters)
{
	using namespace FFF_CounterFlowScenario;

	FResult Off, Legacy, Current, DriveBias;
	{
		FFF_CrowdWorldTestHelpers::FScopedTestWorld W;
		UFF_CrowdSimulationSubsystem* Sim = W.GetCrowdSim();
		if (!Sim) { AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다")); return false; }
		Off = Run(*Sim, false, false, false, 0.f, -1.f);
	}
	{
		FFF_CrowdWorldTestHelpers::FScopedTestWorld W;
		UFF_CrowdSimulationSubsystem* Sim = W.GetCrowdSim();
		if (!Sim) { AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다")); return false; }
		Legacy = Run(*Sim, true, true, false, 0.f, -1.f);
	}
	{
		FFF_CrowdWorldTestHelpers::FScopedTestWorld W;
		UFF_CrowdSimulationSubsystem* Sim = W.GetCrowdSim();
		if (!Sim) { AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다")); return false; }
		Current = Run(*Sim, true, false, false, 0.f, 1.f);
	}
	{
		FFF_CrowdWorldTestHelpers::FScopedTestWorld W;
		UFF_CrowdSimulationSubsystem* Sim = W.GetCrowdSim();
		if (!Sim) { AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다")); return false; }
		DriveBias = Run(*Sim, true, false, true, 0.6f, -1.f);
	}

	auto Report = [this](const TCHAR* Label, const FResult& R)
	{
		AddInfo(FString::Printf(
			TEXT("[%s] 통과 %d/%d, 평균 전진 %.0fUU, 최대밀도 %.2f/m^2, 활성 %d | 걷는중 %d, 도착 %d, 막힘 %d(탈출 %d, 최장 %d스텝)"),
			Label, R.Crossed, R.Spawned, R.MeanProgressUU, R.MaxDensity, R.ActiveAtEnd,
			R.Flow.Walking, R.Flow.ArrivedIdle, R.Flow.Blocked, R.Flow.UnstuckActive, R.Flow.MaxStuckSteps));
	};

	Report(TEXT("off      "), Off);
	Report(TEXT("old      "), Legacy);
	Report(TEXT("기본값   "), Current);
	Report(TEXT("구동+편향"), DriveBias);

	TestTrue(TEXT("[전제] 회피 없이는 실제 스폰 인원 전원이 반대편으로 넘어간다"),
		Off.Spawned > 0 && Off.Crossed >= Off.Spawned - 2);
	TestEqual(TEXT("old와 기본값의 실제 스폰 수가 같다"), Current.Spawned, Legacy.Spawned);

	TestTrue(*FString::Printf(TEXT("평균 전진량이 양수다 (%.0fUU)"), Current.MeanProgressUU),
		Current.MeanProgressUU > 0.f);

	TestTrue(TEXT("최대 밀도가 유한하다"), FMath::IsFinite(Current.MaxDensity) && Current.MaxDensity > 0.f);
	TestEqual(TEXT("아무도 사라지지 않았다"), Current.ActiveAtEnd, Current.Spawned);

	// Both runs can have no stalled agents by the final frame. A strict comparison
	// of that instantaneous count would then demand an impossible negative count.
	// Measure the scenario's actual goal: the opposing flows pass one another,
	// make more forward progress, and do not introduce extra final-frame stalls.
	TestTrue(TEXT("기본 회피로 실제 스폰 인원 거의 전원이 반대편을 통과한다"),
		Current.Spawned > 0 && Current.Crossed >= Current.Spawned - 2);
	TestTrue(TEXT("기본 회피의 통과 인원이 이전 방식보다 줄지 않는다"),
		Current.Crossed >= Legacy.Crossed);
	TestTrue(
		*FString::Printf(TEXT("기본 회피의 평균 전진량이 이전 방식보다 크다 (%.0f > %.0fUU)"),
			Current.MeanProgressUU, Legacy.MeanProgressUU),
		Current.MeanProgressUU > Legacy.MeanProgressUU);
	TestTrue(
		*FString::Printf(TEXT("마지막 프레임의 막힌 인원이 이전 방식보다 늘지 않는다 (%d <= %d)"),
			Current.Flow.Blocked, Legacy.Flow.Blocked),
		Current.Flow.Blocked <= Legacy.Flow.Blocked);

	TestTrue(
		*FString::Printf(TEXT("절반 이상이 걷고 있다 (%d/%d)"), Current.Flow.Walking, Current.Spawned),
		Current.Flow.Walking > Current.Spawned / 2);

	return true;
}

namespace FFF_MillingScenario
{
	constexpr int32 AgentCount = 160;
	constexpr float RingRadiusUU = 1200.f;	// 12m 원. 160명이 중앙을 통과하려 한다
	constexpr float SimSeconds = 20.f;

	struct FSetup
	{
		const TCHAR* Label = TEXT("");
		bool bAvoidance = true;
		bool bDriveByForce = false;		// false = Lerp 경로
		float LaneBiasGain = 0.f;
		float MaxOpposingAccelMps2 = 1.f;	// 음수면 역방향 상한 끔

		// 0이면 기본값을 그대로 쓴다.
		float ContactStiffness = 0.f;
		float ContactFriction = -1.f;	// 음수면 기본값 유지(0도 유효한 값이라 -1을 센티넬로 쓴다)
	};

	struct FResult
	{
		float MeanProgressUU = 0.f;
		float MaxDensity = 0.f;
		FFF_CrowdFlowBreakdown Flow;
		float BlockedPercent = 0.f;

		// 시간 평균 막힘 비율(%). 판정은 이 값으로 한다.
		float MeanBlockedPercent = 0.f;
		float PeakBlockedPercent = 0.f;

		float OverlapPercent = 0.f;		// 최근접 이웃과 인체 반경 안으로 겹친 사람 비율
		float MeanNearestUU = 0.f;		// 평균 최근접 이웃 거리(접촉 거리는 68UU)
		float WorstNearestUU = 0.f;		// 가장 심하게 겹친 쌍의 거리
	};

	void MeasureOverlap(const UFF_CrowdSimulationSubsystem& CrowdSim, int32 Count, FResult& Out)
	{
		constexpr float ContactDistanceUU = 68.f;

		int32 Overlapping = 0;
		float NearestSum = 0.f;
		float Worst = TNumericLimits<float>::Max();

		for (int32 i = 0; i < Count; ++i)
		{
			float Nearest = TNumericLimits<float>::Max();
			for (int32 j = 0; j < Count; ++j)
			{
				if (i == j)
				{
					continue;
				}
				Nearest = FMath::Min(Nearest,
					static_cast<float>(FVector::Dist2D(CrowdSim.GetAgentPosition(i), CrowdSim.GetAgentPosition(j))));
			}

			if (Nearest < ContactDistanceUU)
			{
				++Overlapping;
			}
			NearestSum += Nearest;
			Worst = FMath::Min(Worst, Nearest);
		}

		Out.OverlapPercent = (Count > 0) ? (100.f * Overlapping / Count) : 0.f;
		Out.MeanNearestUU = (Count > 0) ? (NearestSum / Count) : 0.f;
		Out.WorstNearestUU = (Count > 0) ? Worst : 0.f;
	}

	FResult Run(UFF_CrowdSimulationSubsystem& CrowdSim, const FSetup& Setup)
	{
		CrowdSim.bEnablePartyGrouping = false;
		CrowdSim.bEnableAvoidance = Setup.bAvoidance;
		CrowdSim.bAvoidanceDrivesVelocity = Setup.bDriveByForce;
		CrowdSim.AvoidanceConfig.LaneBiasGain = Setup.LaneBiasGain;
		CrowdSim.AvoidanceConfig.MaxOpposingAccelMps2 = Setup.MaxOpposingAccelMps2;

		if (Setup.ContactStiffness > 0.f)
		{
			CrowdSim.AvoidanceConfig.ContactStiffness = Setup.ContactStiffness;
		}
		if (Setup.ContactFriction >= 0.f)
		{
			CrowdSim.AvoidanceConfig.ContactFriction = Setup.ContactFriction;
		}

		CrowdSim.InitializeSimulation(FVector::ZeroVector, 10000.f, 2, AgentCount + 8,
			AFF_BaseCrowd::StaticClass());

		const int32 Spawned = CrowdSim.SpawnAgents(AgentCount, FVector::ZeroVector, RingRadiusUU);

		TArray<FVector> StartPos;
		TArray<FVector> Targets;
		StartPos.Reserve(Spawned);
		Targets.Reserve(Spawned);
		for (int32 i = 0; i < Spawned; ++i)
		{
			const FVector P = CrowdSim.GetAgentPosition(i);
			StartPos.Add(P);
			Targets.Add(FVector(-P.X, -P.Y, 0.f));
		}

		constexpr float FrameDelta = 1.f / 60.f;
		const int32 FrameCount = FMath::CeilToInt(SimSeconds / FrameDelta);

		constexpr int32 SampleEveryFrames = 30;
		constexpr float WarmupSeconds = 2.f;
		float BlockedSum = 0.f;
		float BlockedPeak = 0.f;
		int32 SampleCount = 0;

		FResult Out;

		for (int32 Frame = 0; Frame < FrameCount; ++Frame)
		{
			for (int32 i = 0; i < Spawned; ++i)
			{
				CrowdSim.SetAgentDestination(i, Targets[i]);
			}
			CrowdSim.Tick(FrameDelta);

			const bool bPastWarmup = (Frame * FrameDelta) >= WarmupSeconds;
			if (bPastWarmup && (Frame % SampleEveryFrames) == 0)
			{
				const FFF_CrowdFlowBreakdown Sample = CrowdSim.GetFlowBreakdown();
				const float Percent = (Sample.Total() > 0)
					? (100.f * Sample.Blocked / Sample.Total())
					: 0.f;
				BlockedSum += Percent;
				BlockedPeak = FMath::Max(BlockedPeak, Percent);
				++SampleCount;
			}
		}

		Out.MeanBlockedPercent = (SampleCount > 0) ? (BlockedSum / SampleCount) : 0.f;
		Out.PeakBlockedPercent = BlockedPeak;
		Out.MaxDensity = CrowdSim.GetMaxDensity();
		Out.Flow = CrowdSim.GetFlowBreakdown();
		Out.BlockedPercent = (Out.Flow.Total() > 0)
			? (100.f * Out.Flow.Blocked / Out.Flow.Total())
			: 0.f;
		MeasureOverlap(CrowdSim, Spawned, Out);

		float ProgressSum = 0.f;
		for (int32 i = 0; i < Spawned; ++i)
		{
			const float Was = FVector::Dist2D(StartPos[i], Targets[i]);
			const float Now = FVector::Dist2D(CrowdSim.GetAgentPosition(i), Targets[i]);
			ProgressSum += (Was - Now);
		}
		Out.MeanProgressUU = (Spawned > 0) ? (ProgressSum / Spawned) : 0.f;

		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_MillingCrowdDoesNotGridlock, "FestaFiesta.Crowd.Avoidance.MillingCrowdDoesNotGridlock", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_MillingCrowdDoesNotGridlock::RunTest(const FString& Parameters)
{
	using namespace FFF_MillingScenario;

	const FSetup Setups[] = {
		{ TEXT("off          "),  false, false, 0.f,  1.f },
		{ TEXT("상한없음     "), true,  false, 0.f, -1.f },
		{ TEXT("상한1.0      "), true,  false, 0.f,  1.f },
		{ TEXT("상한0.5      "), true,  false, 0.f,  0.5f },
		{ TEXT("상한0.25     "), true,  false, 0.f,  0.25f },
		{ TEXT("상한0.5+마찰0"), true,  false, 0.f,  0.5f, 0.f, 0.f },
		{ TEXT("상한0.5+마찰0+강성1/4"), true, false, 0.f, 0.5f, 30000.f, 0.f },
		{ TEXT("구동+편향    "), true,  true,  0.6f, -1.f },
	};

	FResult Results[UE_ARRAY_COUNT(Setups)];

	for (int32 s = 0; s < UE_ARRAY_COUNT(Setups); ++s)
	{
		FFF_CrowdWorldTestHelpers::FScopedTestWorld W;
		UFF_CrowdSimulationSubsystem* Sim = W.GetCrowdSim();
		if (!Sim)
		{
			AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
			return false;
		}
		Results[s] = Run(*Sim, Setups[s]);

		AddInfo(FString::Printf(
			TEXT("[%s] 막힘 평균 %.1f%% 최대 %.1f%% (끝 %.1f%%), 전진 %.0fUU | 겹침 %.1f%%, 최근접 평균 %.0fUU, 밀도 %.2f"),
			Setups[s].Label, Results[s].MeanBlockedPercent, Results[s].PeakBlockedPercent,
			Results[s].BlockedPercent, Results[s].MeanProgressUU,
			Results[s].OverlapPercent, Results[s].MeanNearestUU, Results[s].MaxDensity));
	}

	const FResult& Off = Results[0];
	const FResult& NoCap = Results[1];
	const FResult& WithCap = Results[2];

	TestTrue(*FString::Printf(TEXT("[전제] 회피 없이는 막히지 않는다 (평균 %.1f%%)"), Off.MeanBlockedPercent),
		Off.MeanBlockedPercent < 5.f);

	TestTrue(
		*FString::Printf(TEXT("역방향 상한이 막힘을 줄인다 (평균 %.1f%% < %.1f%%)"),
			WithCap.MeanBlockedPercent, NoCap.MeanBlockedPercent),
		WithCap.MeanBlockedPercent < NoCap.MeanBlockedPercent);

	TestTrue(
		*FString::Printf(TEXT("역방향 상한이 전진량을 늘린다 (%.0fUU >= %.0fUU)"),
			WithCap.MeanProgressUU, NoCap.MeanProgressUU),
		WithCap.MeanProgressUU >= NoCap.MeanProgressUU);

	TestTrue(TEXT("밀도가 유한하다"), FMath::IsFinite(WithCap.MaxDensity) && WithCap.MaxDensity > 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdQueue_SlotArrivalRadiusIsSmallerThanSlotSpacing, "FestaFiesta.Crowd.Queue.SlotArrivalRadiusIsSmallerThanSlotSpacing", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdQueue_SlotArrivalRadiusIsSmallerThanSlotSpacing::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	constexpr float BoothSlotSpacingUU = 90.f;
	constexpr float QueueOwnToleranceUU = 25.f;

	TestTrue(
		*FString::Printf(TEXT("큐 슬롯 도착 반경(%.0f) < 슬롯 간격(%.0f)"),
			CrowdSim->QueueSlotArrivalRadiusUU, BoothSlotSpacingUU),
		CrowdSim->QueueSlotArrivalRadiusUU < BoothSlotSpacingUU);

	TestTrue(
		*FString::Printf(TEXT("큐 슬롯 도착 반경(%.0f) >= 큐 자신의 오차(%.0f)"),
			CrowdSim->QueueSlotArrivalRadiusUU, QueueOwnToleranceUU),
		CrowdSim->QueueSlotArrivalRadiusUU >= QueueOwnToleranceUU);

	TestTrue(
		*FString::Printf(TEXT("일반 도착 반경(%.0f) > 큐 슬롯 반경(%.0f)"),
			CrowdSim->ArrivalRadiusUU, CrowdSim->QueueSlotArrivalRadiusUU),
		CrowdSim->ArrivalRadiusUU > CrowdSim->QueueSlotArrivalRadiusUU);

	TestTrue(
		*FString::Printf(TEXT("감속 거리가 명시값이다 (%.0fUU, 0이면 반경 연동 폴백)"),
			CrowdSim->BrakingDistanceUU),
		CrowdSim->BrakingDistanceUU > 0.f);

	TestTrue(
		*FString::Printf(TEXT("접근점(%.0f) > 큐 슬롯(%.0f)"),
			CrowdSim->FacilityApproachRadiusUU, CrowdSim->QueueSlotArrivalRadiusUU),
		CrowdSim->FacilityApproachRadiusUU > CrowdSim->QueueSlotArrivalRadiusUU);

	TestTrue(
		*FString::Printf(TEXT("배회(%.0f) >= 접근점(%.0f)"),
			CrowdSim->WanderArrivalRadiusUU, CrowdSim->FacilityApproachRadiusUU),
		CrowdSim->WanderArrivalRadiusUU >= CrowdSim->FacilityApproachRadiusUU);

	constexpr float NarrowestQueueReservationWidthUU = 280.f;
	TestTrue(
		*FString::Printf(TEXT("접근점(%.0f) <= 가장 좁은 예약 폭(%.0f)"),
			CrowdSim->FacilityApproachRadiusUU, NarrowestQueueReservationWidthUU),
		CrowdSim->FacilityApproachRadiusUU <= NarrowestQueueReservationWidthUU);

	if (CrowdSim->FacilityApproachRadiusUU > 150.f)
	{
		TestTrue(TEXT("접근점 반경을 키웠으면 재결정 쿨다운이 켜져 있어야 한다"),
			CrowdSim->DecisionCooldownSeconds > 0.f);
	}

	constexpr int32 Requested = 4;
	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 2, 8, AFF_BaseCrowd::StaticClass());
	const int32 Spawned = CrowdSim->SpawnAgents(Requested, FVector::ZeroVector, 300.f);
	FFF_CrowdWorldTestHelpers::TestWholePartySpawnCount(
		*this, TEXT("Arrival radius"), Requested, Spawned);
	FFF_CrowdWorldTestHelpers::TickFor(*CrowdSim, 0.2f);

	TestEqual(TEXT("시설 ID가 없는 목적지는 배회 반경을 쓴다"),
		CrowdSim->GetAgentArrivalRadius(0), CrowdSim->WanderArrivalRadiusUU, 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdDecision_ArrivalCooldownThrottlesRechoice, "FestaFiesta.Crowd.Decision.ArrivalCooldownThrottlesRechoice", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdDecision_ArrivalCooldownThrottlesRechoice::RunTest(const FString& Parameters)
{
	constexpr int32 Requested = 4;
	constexpr float FrameDelta = 1.f / 60.f;
	int32 Spawned = 0;
	int32 Processed = 0;
	int32 Skipped = 0;
	{
		FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
		UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

		if (!CrowdSim)
		{
			AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
			return false;
		}

		CrowdSim->DecisionCooldownSeconds = 2.f;
		CrowdSim->bEnablePartyGrouping = false;

		CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 2, 8, AFF_BaseCrowd::StaticClass());
		Spawned = CrowdSim->SpawnAgents(Requested, FVector::ZeroVector, 100.f);
		FFF_CrowdWorldTestHelpers::TestWholePartySpawnCount(
			*this, TEXT("Arrival cooldown treatment"), Requested, Spawned);

		for (int32 Frame = 0; Frame < 300; ++Frame)
		{
			for (int32 i = 0; i < Spawned; ++i)
			{
				CrowdSim->SetAgentDestination(i, CrowdSim->GetAgentPosition(i));
			}
			CrowdSim->Tick(FrameDelta);
		}

		Processed = CrowdSim->GetDecisionsProcessed();
		Skipped = CrowdSim->GetDecisionCooldownSkips();
	}

	AddInfo(FString::Printf(TEXT("5초간 결정 처리 %d회, 쿨다운으로 버림 %d회 (%d명)"), Processed, Skipped, Spawned));

	TestTrue(TEXT("쿨다운이 재결정을 막았다"), Skipped > 0);

	const int32 UpperBound = Spawned * 3 + Spawned + 2;
	TestTrue(
		*FString::Printf(TEXT("결정 횟수가 쿨다운 상한 안이다 (%d <= %d)"), Processed, UpperBound),
		Processed <= UpperBound);

	int32 NoCooldownSpawned = 0;
	int32 NoCooldownProcessed = 0;
	int32 NoCooldownSkipped = 0;
	{
		// GameInstance subsystem console commands are global, so keep the two worlds sequential.
		FFF_CrowdWorldTestHelpers::FScopedTestWorld NoCooldownWorld;
		UFF_CrowdSimulationSubsystem* NoCooldown = NoCooldownWorld.GetCrowdSim();
		if (!NoCooldown)
		{
			AddError(TEXT("대조군 월드를 얻지 못했다"));
			return false;
		}

		NoCooldown->DecisionCooldownSeconds = 0.f;
		NoCooldown->bEnablePartyGrouping = false;
		NoCooldown->InitializeSimulation(FVector::ZeroVector, 10000.f, 2, 8, AFF_BaseCrowd::StaticClass());
		NoCooldownSpawned = NoCooldown->SpawnAgents(Requested, FVector::ZeroVector, 100.f);
		FFF_CrowdWorldTestHelpers::TestWholePartySpawnCount(
			*this, TEXT("Arrival cooldown control"), Requested, NoCooldownSpawned);

		for (int32 Frame = 0; Frame < 300; ++Frame)
		{
			for (int32 i = 0; i < NoCooldownSpawned; ++i)
			{
				NoCooldown->SetAgentDestination(i, NoCooldown->GetAgentPosition(i));
			}
			NoCooldown->Tick(FrameDelta);
		}

		NoCooldownProcessed = NoCooldown->GetDecisionsProcessed();
		NoCooldownSkipped = NoCooldown->GetDecisionCooldownSkips();
	}

	AddInfo(FString::Printf(TEXT("[쿨다운 끔] 결정 처리 %d회, 버림 %d회"),
		NoCooldownProcessed, NoCooldownSkipped));

	TestEqual(TEXT("Treatment and control spawned the same actual count"), NoCooldownSpawned, Spawned);
	TestEqual(TEXT("끄면 아무것도 버리지 않는다"), NoCooldownSkipped, 0);
	TestTrue(
		*FString::Printf(TEXT("끄면 결정 횟수가 크게 늘어난다 (%d > %d)"),
			NoCooldownProcessed, Processed),
		NoCooldownProcessed > Processed);

	return true;
}

// 한 점으로 몰려 서로를 막으면 목적지를 포기한다 — 인기 시설 앞 병목이 스스로 풀리는 유일한 경로다.
// 임계를 0으로 두면 예전처럼 영원히 밀어붙인다(대조군).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdDecision_StuckPartyGivesUpDestination, "FestaFiesta.Crowd.Decision.StuckPartyGivesUpDestination", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdDecision_StuckPartyGivesUpDestination::RunTest(const FString& Parameters)
{
	// 60명이 반경 300UU 안에서 같은 한 점을 향한다. 안쪽 고리가 그 점을 차지하면 바깥은
	// 목적지 방향 진척이 0에 붙는다 — 부스 앞에서 실제로 일어나는 모양 그대로다.
	const FName CrushTarget(TEXT("CrushTarget"));

	// 포기한 목적지가 다음 요청에 실려 오는가. 조정자가 그것을 후보에서 뺄 수 있으려면
	// 이 값이 반드시 채워져 있어야 한다 — 비어 있으면 같은 잼으로 되돌아간다
	int32 MoveFailedWithPrevious = 0;
	int32 MoveFailedWithoutPrevious = 0;
	int32 SpawnedWithPrevious = 0;

	auto RunCrush = [this, CrushTarget, &MoveFailedWithPrevious, &MoveFailedWithoutPrevious, &SpawnedWithPrevious](
		int32 StuckStepThreshold, int32& OutStuckRequests, int32& OutMaxStuckSteps)
	{
		FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
		UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();
		if (!CrowdSim)
		{
			AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
			return false;
		}

		CrowdSim->bEnablePartyGrouping = false;
		CrowdSim->bEnableQueueJoin = false;
		CrowdSim->DecisionStuckSteps = StuckStepThreshold;
		// 쿨다운은 켜 둔다. 잼에서 churn 이 나는지도 같이 보려면 실제 설정이어야 한다
		CrowdSim->DecisionCooldownSeconds = 2.f;

		CrowdSim->OnPartyNeedsDecision.BindLambda(
			[CrushTarget, &MoveFailedWithPrevious, &MoveFailedWithoutPrevious, &SpawnedWithPrevious](
				const FFF_PartyDecisionContext& Context, FFF_PartyDecisionResult& OutResult)
			{
				if (Context.Reason == EFF_DecisionReason::MoveFailed)
				{
					if (Context.PreviousDestinationId == CrushTarget)
					{
						++MoveFailedWithPrevious;
					}
					else
					{
						++MoveFailedWithoutPrevious;
					}
				}
				else if (Context.Reason == EFF_DecisionReason::Spawned
					&& !Context.PreviousDestinationId.IsNone())
				{
					// 스폰 직후에는 직전 목적지가 있을 수 없다. 세어지면 상태가 새고 있는 것이다
					++SpawnedWithPrevious;
				}

				// 매번 같은 곳을 준다 = 잼이 풀릴 다른 선택지가 없는 최악 상황.
				// 직전 목적지 감점은 조정자 몫이라 여기서는 흉내내지 않는다
				OutResult.DestinationId = CrushTarget;
				OutResult.Destination = FVector::ZeroVector;
				return true;
			});

		CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 2, 64, AFF_BaseCrowd::StaticClass());
		CrowdSim->SpawnAgents(60, FVector::ZeroVector, 300.f);

		FFF_CrowdWorldTestHelpers::TickFor(*CrowdSim, 10.f);

		CrowdSim->OnPartyNeedsDecision.Unbind();

		OutStuckRequests = CrowdSim->GetStuckDecisionRequests();
		OutMaxStuckSteps = CrowdSim->GetFlowBreakdown().MaxStuckSteps;
		return true;
	};

	// 60스텝 = 2초. 조향의 탈출 임계(40스텝)보다 크고 실전값(300)보다 짧게 잡아 테스트를 짧게 유지한다
	int32 OnRequests = 0;
	int32 OnMaxStuck = 0;
	if (!RunCrush(60, OnRequests, OnMaxStuck))
	{
		return false;
	}

	AddInfo(FString::Printf(TEXT("[임계 60스텝] 막혀서 포기 %d회, 최장 막힘 %d스텝"), OnRequests, OnMaxStuck));

	if (OnMaxStuck == 0)
	{
		AddError(TEXT("아무도 막히지 않았다 — 이 설정으로는 잼이 만들어지지 않아 판정이 무의미하다. "
			"인원·반경을 다시 잡을 것"));
		return false;
	}

	TestTrue(TEXT("막힌 파티가 목적지를 포기했다"), OnRequests > 0);

	AddInfo(FString::Printf(TEXT("MoveFailed 요청 중 직전 목적지가 실려 온 것 %d건, 빈 것 %d건"),
		MoveFailedWithPrevious, MoveFailedWithoutPrevious));

	TestTrue(TEXT("포기 요청이 조정자까지 MoveFailed 사유로 도달했다"), MoveFailedWithPrevious > 0);
	TestEqual(TEXT("MoveFailed 인데 직전 목적지가 비어 온 요청은 없다"), MoveFailedWithoutPrevious, 0);
	TestEqual(TEXT("스폰 요청에는 직전 목적지가 없다"), SpawnedWithPrevious, 0);

	int32 OffRequests = 0;
	int32 OffMaxStuck = 0;
	if (!RunCrush(0, OffRequests, OffMaxStuck))
	{
		return false;
	}

	AddInfo(FString::Printf(TEXT("[끔] 막혀서 포기 %d회, 최장 막힘 %d스텝"), OffRequests, OffMaxStuck));

	TestEqual(TEXT("0으로 끄면 막혀도 포기하지 않는다"), OffRequests, 0);
	TestTrue(TEXT("[전제] 끈 쪽도 실제로 막혀 있었다"), OffMaxStuck > 0);

	return true;
}

// 같은 배치에서 결정하는 파티들이 서로의 목적지를 본다 — 시설 몰림을 막는 값의 핵심 성질이다.
// 이 성질이 깨지면(전원이 0을 본다) 계수를 어떻게 만져도 전원이 같은 빈 시설을 고른다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdDecision_InTransitCountsAccumulateWithinBatch, "FestaFiesta.Crowd.Decision.InTransitCountsAccumulateWithinBatch", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdDecision_InTransitCountsAccumulateWithinBatch::RunTest(const FString& Parameters)
{
	const FName FixedDestination(TEXT("SoleFacility"));

	TArray<int32> ObservedBeforeEachDecision;
	int32 SpawnedSoloParties = 0;
	int32 FinalCount = -1;
	int32 FinalDestinations = -1;
	int32 FinalMax = -1;

	// ★ 월드는 한 번에 하나만 살려 둔다. 두 개를 동시에 들고 있으면 월드 서브시스템의
	// 콘솔 명령 등록이 이름을 다투다 해제 시점에 죽는다(UFF_FinaleShowSubsystem, 이 파일 밖 문제).
	// 그래서 결과를 지역 변수로 빼낸 뒤 월드를 놓는다.
	{
		FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
		UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

		if (!CrowdSim)
		{
			AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
			return false;
		}

		// 파티를 묶으면 인원수가 섞여 "누적됐는가"와 "인원을 맞게 셌는가"가 한 값에 겹친다
		CrowdSim->bEnablePartyGrouping = false;
		// 큐에 들어가면 이 값에서 빠지는 것이 정상이라, 그 경로를 아예 닫아 누적만 본다
		CrowdSim->bEnableQueueJoin = false;

		// 전원에게 같은 목적지를 내려주는 조정자. 몰림이 발생하는 최악 상황을 그대로 만든다
		CrowdSim->OnPartyNeedsDecision.BindLambda(
			[&ObservedBeforeEachDecision, FixedDestination](
				const FFF_PartyDecisionContext& Context, FFF_PartyDecisionResult& OutResult)
			{
				ObservedBeforeEachDecision.Add(Context.InTransitMemberCounts
					? Context.InTransitMemberCounts->FindRef(FixedDestination)
					: -1);

				OutResult.DestinationId = FixedDestination;
				// 도착하면 재평가가 걸려 배치가 갈린다. 이 틱 안에 못 닿을 거리로 둔다
				OutResult.Destination = FVector(100000.f, 0.f, 0.f);
				return true;
			});

		CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 8, 16, AFF_BaseCrowd::StaticClass());
		// ★ 실제 스폰 수는 요청 수와 다르다. 방문객 분포 빌더가 그룹을 통째로만 방출하므로
		// 8명을 요청해도 6~8명이 나온다(로그: "요청 N명 중 M명"). 그래서 아래 검증은 요청 수가
		// 아니라 실제로 온 결정 건수를 기준으로 한다 — 숫자를 박아두면 분포가 바뀔 때마다 깨진다.
		SpawnedSoloParties = CrowdSim->SpawnAgents(8, FVector::ZeroVector, 100.f);

		FFF_CrowdWorldTestHelpers::TickFor(*CrowdSim, 0.1f);

		CrowdSim->OnPartyNeedsDecision.Unbind();

		FinalCount = CrowdSim->GetInTransitMemberCount(FixedDestination);
		FinalDestinations = CrowdSim->GetInTransitDestinationCount();
		FinalMax = CrowdSim->GetMaxInTransitMemberCount();
	}

	const int32 DecisionCount = ObservedBeforeEachDecision.Num();

	AddInfo(FString::Printf(TEXT("솔로 파티 %d개 스폰, 첫 배치에서 결정 %d건"),
		SpawnedSoloParties, DecisionCount));

	if (DecisionCount < 2)
	{
		AddError(FString::Printf(TEXT("누적을 보려면 한 배치에 결정이 2건 이상 필요하다 (%d건)"),
			DecisionCount));
		return false;
	}

	// 핵심 성질: N번째로 결정하는 파티는 앞서 결정한 N-1개를 본다.
	// 전원이 0을 보면 같은 배치의 파티들이 서로를 못 보는 것이고, 그게 몰림의 원인이다.
	for (int32 Index = 0; Index < DecisionCount; ++Index)
	{
		TestEqual(
			*FString::Printf(TEXT("%d번째 파티는 앞서 결정한 %d명을 본다"), Index + 1, Index),
			ObservedBeforeEachDecision[Index], Index);
	}

	TestEqual(TEXT("배치가 끝나면 결정한 전원이 그 목적지로 집계된다"), FinalCount, DecisionCount);
	TestEqual(TEXT("집계된 목적지는 그 한 곳뿐이다"), FinalDestinations, 1);
	TestEqual(TEXT("한 곳 최대 인원이 총원과 같다"), FinalMax, DecisionCount);

	// 조정자가 없으면(배회만 하면) 이 값은 비어 있어야 한다 — 배회 목적지가 섞이면 안 된다
	int32 WanderDestinations = -1;
	{
		FFF_CrowdWorldTestHelpers::FScopedTestWorld WanderWorld;
		UFF_CrowdSimulationSubsystem* Wander = WanderWorld.GetCrowdSim();
		if (!Wander)
		{
			AddError(TEXT("대조군 월드를 얻지 못했다"));
			return false;
		}

		Wander->bEnablePartyGrouping = false;
		Wander->OnPartyNeedsDecision.Unbind();
		Wander->InitializeSimulation(FVector::ZeroVector, 10000.f, 2, 8, AFF_BaseCrowd::StaticClass());
		Wander->SpawnAgents(4, FVector::ZeroVector, 100.f);
		FFF_CrowdWorldTestHelpers::TickFor(*Wander, 0.1f);

		WanderDestinations = Wander->GetInTransitDestinationCount();
	}

	TestEqual(TEXT("배회는 목적지로 집계되지 않는다"), WanderDestinations, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdWorld_ReleaseReturnsActorAndHidesIt, "FestaFiesta.Crowd.World.ReleaseReturnsActorAndHidesIt", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdWorld_ReleaseReturnsActorAndHidesIt::RunTest(const FString& Parameters)
{
	constexpr int32 ActorPoolSize = 4;
	constexpr int32 Requested = 2;

	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(
		FVector::ZeroVector, 10000.f, ActorPoolSize, 4, AFF_BaseCrowd::StaticClass());
	const int32 Spawned = CrowdSim->SpawnAgents(Requested, FVector::ZeroVector, 100.f);
	FFF_CrowdWorldTestHelpers::TestWholePartySpawnCount(
		*this, TEXT("Actor release"), Requested, Spawned);

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
	TestEqual(TEXT("빈 슬롯이 회수됐다"), CrowdSim->PoolBinding.GetFreeSlotCount(),
		ActorPoolSize - Spawned + 1);
	TestEqual(TEXT("바인딩 수 = 실제 스폰 수 - 1"),
		CrowdSim->GetBoundActorCount(), Spawned - 1);

	TestTrue(TEXT("Agent는 여전히 활성"), CrowdSim->IsAgentActive(0));
	TestEqual(TEXT("활성 Agent 수는 실제 스폰 수 그대로"),
		CrowdSim->GetActiveAgentCount(), Spawned);

	FFF_CrowdWorldTestHelpers::TickFor(*CrowdSim, 0.5f);
	TestEqual(TEXT("Actor 없는 Agent도 밀도에 잡힌다"),
		CrowdSim->GetDensityGrid().GetSampledUnitCount(), Spawned);

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

	AddExpectedError(TEXT("Already initialized"), EAutomationExpectedErrorFlags::Contains, 0);
	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 4, 10, AFF_BaseCrowd::StaticClass());

	TestEqual(TEXT("두 번째 초기화는 무시된다"), CrowdSim->CrowdPool.Num(), 4);
	TestEqual(TEXT("장부도 그대로"), CrowdSim->PoolBinding.GetNumActorSlots(), 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdParty_SpawnHitsExactRequestedCount, "FestaFiesta.Crowd.Party.SpawnHitsExactRequestedCount", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdParty_SpawnHitsExactRequestedCount::RunTest(const FString& Parameters)
{
	constexpr int32 Requested = 37;

	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 0, 64, AFF_BaseCrowd::StaticClass());

	const int32 Spawned = CrowdSim->SpawnAgents(Requested, FVector::ZeroVector, 2000.f);

	FFF_CrowdWorldTestHelpers::TestWholePartySpawnCount(
		*this, TEXT("파티 스폰"), Requested, Spawned);
	TestEqual(TEXT("활성 Agent 수 = 실제 스폰 수"), CrowdSim->GetActiveAgentCount(), Spawned);
	TestEqual(TEXT("도착 커서 = 실제 스폰 수"), CrowdSim->GetArrivalSpawnedCount(), Spawned);

	const int32 PartyCount = CrowdSim->GetActivePartyCount();
	TestTrue(FString::Printf(TEXT("파티 수(%d)가 실제 인원(%d)보다 적다 = 실제로 묶였다"),
		PartyCount, Spawned), PartyCount < Spawned);
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

		if (CrowdSim->GetAgentPartyID(LeaderIndex) != PartyID)
		{
			AddError(FString::Printf(TEXT("Agent %d(파티 %d)와 리더 %d(파티 %d)의 PartyID가 다르다"),
				AgentIndex, PartyID, LeaderIndex, CrowdSim->GetAgentPartyID(LeaderIndex)));
			return false;
		}

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
	constexpr int32 Requested = 20;

	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 0, 32, AFF_BaseCrowd::StaticClass());

	CrowdSim->bEnablePartyGrouping = false;

	const int32 Spawned = CrowdSim->SpawnAgents(Requested, FVector::ZeroVector, 2000.f);

	FFF_CrowdWorldTestHelpers::TestWholePartySpawnCount(
		*this, TEXT("그룹 비활성 스폰"), Requested, Spawned);
	TestEqual(TEXT("활성 Agent 수 = 실제 스폰 수"), CrowdSim->GetActiveAgentCount(), Spawned);
	TestEqual(TEXT("도착 커서 = 실제 스폰 수"), CrowdSim->GetArrivalSpawnedCount(), Spawned);
	TestEqual(TEXT("파티 수 = 실제 인원 수 (전원 1인)"),
		CrowdSim->GetActivePartyCount(), Spawned);

	for (int32 AgentIndex = 0; AgentIndex < Spawned; ++AgentIndex)
	{
		if (!CrowdSim->IsAgentActive(AgentIndex))
		{
			AddError(FString::Printf(TEXT("Agent %d가 비활성이다"), AgentIndex));
			return false;
		}
		if (!CrowdSim->IsPartyLeader(AgentIndex))
		{
			AddError(FString::Printf(TEXT("Agent %d가 1인 파티가 아니다"), AgentIndex));
			return false;
		}

		const int32 PartyID = CrowdSim->GetAgentPartyID(AgentIndex);
		if (PartyID == INDEX_NONE)
		{
			AddError(FString::Printf(TEXT("1인 Agent %d에 PartyID가 없다"), AgentIndex));
			return false;
		}
		TestEqual(*FString::Printf(TEXT("Agent %d의 파티는 정확히 1명"), AgentIndex),
			CrowdSim->GetPartyMembers(PartyID).Num(), 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdParty_MemberDestinationRedirectsToLeader, "FestaFiesta.Crowd.Party.MemberDestinationRedirectsToLeader", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdParty_MemberDestinationRedirectsToLeader::RunTest(const FString& Parameters)
{
	FFF_CrowdWorldTestHelpers::FScopedTestWorld TestWorld;
	UFF_CrowdSimulationSubsystem* CrowdSim = TestWorld.GetCrowdSim();

	if (!CrowdSim)
	{
		AddError(TEXT("월드에서 CrowdSimulationSubsystem을 얻지 못했다"));
		return false;
	}

	CrowdSim->InitializeSimulation(FVector::ZeroVector, 10000.f, 0, 32, AFF_BaseCrowd::StaticClass());
	CrowdSim->bEnablePartyGrouping = true;

	const int32 Spawned = CrowdSim->SpawnAgents(20, FVector::ZeroVector, 2000.f);

	int32 MemberIndex = INDEX_NONE;
	for (int32 AgentIndex = 0; AgentIndex < Spawned; ++AgentIndex)
	{
		if (CrowdSim->IsAgentActive(AgentIndex) && CrowdSim->GetPartyLeader(AgentIndex) != AgentIndex)
		{
			MemberIndex = AgentIndex;
			break;
		}
	}

	if (MemberIndex == INDEX_NONE)
	{
		AddError(TEXT("2인 이상 파티가 하나도 만들어지지 않아 리다이렉트를 검증할 수 없다"));
		return false;
	}

	const int32 LeaderIndex = CrowdSim->GetPartyLeader(MemberIndex);
	const FVector SlotLocation(1234.f, 5678.f, 0.f);

	CrowdSim->SetAgentDestination(MemberIndex, SlotLocation);

	TestEqual(TEXT("멤버에게 건 목적지가 리더에게 적용됐다"),
		CrowdSim->GetAgentDestination(LeaderIndex), SlotLocation);

	const FVector ExitLocation(-500.f, 900.f, 0.f);
	CrowdSim->SetAgentDestination(LeaderIndex, ExitLocation);

	TestEqual(TEXT("리더 직접 호출은 그대로 적용된다"),
		CrowdSim->GetAgentDestination(LeaderIndex), ExitLocation);

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

namespace FFF_AvoidanceTestHelpers
{
	// 성인 보행자 한 명. 반경 0.34m = 팀 레포의 Radius 34UU와 같은 사람이다.
	constexpr float PersonRadiusMeters = 0.34f;
	constexpr float PersonMassKg = 70.f;
	constexpr float FixedStep = 1.f / 30.f;

	FFF_CrowdAvoidanceConfig MakeConfig()
	{
		return FFF_CrowdAvoidanceConfig();
	}

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
	const float ContactDistance = PersonRadiusMeters * 2.f;

	bool bSawNonFinite = false;
	const float MinDistance = SimulateHeadOn(120, 2.f, 1.5f, Config, &bSawNonFinite);

	TestFalse(TEXT("적분 중 NaN/inf가 생기지 않았다"), bSawNonFinite);
	TestTrue(TEXT("서로를 통과하지 않았다(거리가 0을 지나지 않음)"), MinDistance > 0.f);

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

	TestEqual(TEXT("같은 입력은 비트 단위로 같은 결과를 낸다"), First, Second, 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_RepulsionIsInMetresNotUU, "FestaFiesta.Crowd.Avoidance.RepulsionIsInMetresNotUU", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_RepulsionIsInMetresNotUU::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();

	const FVector2f AtContact = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(PersonRadiusMeters * 2.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

	TestEqual(TEXT("접촉 거리에서 반발력 = RepulsionA"), AtContact.Size(), Config.RepulsionA, 1.f);
	TestTrue(TEXT("-X 방향으로 밀려난다(상대는 +X에 있다)"), AtContact.X < 0.f);

	const FVector2f OneB = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(PersonRadiusMeters * 2.f + Config.RepulsionB, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

	TestEqual(TEXT("B만큼 멀어지면 1/e로 감쇠한다"),
		OneB.Size(), Config.RepulsionA * FMath::Exp(-1.f), 1.f);

	const FVector2f AtNeighbourEdge = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(Config.NeighbourRadiusMeters, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

	TestTrue(
		*FString::Printf(TEXT("이웃 반경(%.1fm) 끝에서는 기여가 무시할 만하다 (%.3fN)"),
			Config.NeighbourRadiusMeters, AtNeighbourEdge.Size()),
		AtNeighbourEdge.Size() < 1.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_AnisotropyWeakensRearApproach, "FestaFiesta.Crowd.Avoidance.AnisotropyWeakensRearApproach", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_AnisotropyWeakensRearApproach::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();
	const float Gap = PersonRadiusMeters * 2.f + 0.2f;

	const FVector2f Heading(1.f, 0.f);

	const FVector2f FromFront = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), Heading, PersonRadiusMeters,
		FVector2f(Gap, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

	const FVector2f FromRear = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), Heading, PersonRadiusMeters,
		FVector2f(-Gap, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

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

	const FVector2f Force = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(10.f, 10.f), FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(10.f, 10.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

	TestTrue(TEXT("완전 겹침에서도 유한하다"), FMath::IsFinite(Force.X) && FMath::IsFinite(Force.Y));
	TestTrue(TEXT("0이 아닌 힘으로 밀어낸다"), Force.Size() > 0.f);

	FFF_CrowdAvoidanceConfig Loose = Config;
	Loose.MaxExponentArg = 700.f;
	const FVector2f LooseForce = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(0.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Loose);

	const FVector2f Accel = FFF_CrowdForceSolver::ClampAcceleration(
		LooseForce / PersonMassKg, Config.MaxAccelerationMps2);
	TestTrue(TEXT("가속 클램프를 통과한 값은 유한하고 상한 안이다"),
		FMath::IsFinite(Accel.Size()) && Accel.Size() <= Config.MaxAccelerationMps2 + 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_SocialTermStaysAtWalkingScale, "FestaFiesta.Crowd.Avoidance.SocialTermStaysAtWalkingScale", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_SocialTermStaysAtWalkingScale::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();

	const FFF_CrowdForceTerms Near = FFF_CrowdForceSolver::ComputeAgentInteractionTerms(
		FVector2f(0.f, 0.f), FVector2f(1.5f, 0.f), PersonRadiusMeters,
		FVector2f(0.75f, 0.f), FVector2f(-1.5f, 0.f), PersonRadiusMeters, Config);

	TestTrue(TEXT("닿지 않았으므로 접촉항은 0이다"), Near.Contact.IsNearlyZero());
	TestTrue(TEXT("비접촉항만으로 이미 상한을 넘는 가속도가 나온다(분리 클램프가 필요한 이유)"),
		(Near.Social / PersonMassKg).Size() > Config.MaxSocialAccelMps2);

	const FVector2f SocialAccel = FFF_CrowdForceSolver::ClampAcceleration(
		Near.Social / PersonMassKg, Config.MaxSocialAccelMps2);
	TestTrue(
		*FString::Printf(TEXT("비접촉 가속도는 보행 스케일 안이다 (%.2f <= %.2f m/s^2)"),
			SocialAccel.Size(), Config.MaxSocialAccelMps2),
		SocialAccel.Size() <= Config.MaxSocialAccelMps2 + KINDA_SMALL_NUMBER);

	constexpr float MaxSpeedUU = 150.f;
	const float DeltaVUU = SocialAccel.Size() * FFF_CrowdUnits::UUPerMeter * FixedStep;
	TestTrue(
		*FString::Printf(TEXT("한 스텝 속도 변화가 MaxSpeed 안이다 (%.1f < %.1f UU/s)"), DeltaVUU, MaxSpeedUU),
		DeltaVUU < MaxSpeedUU);

	const FFF_CrowdForceTerms Overlapping = FFF_CrowdForceSolver::ComputeAgentInteractionTerms(
		FVector2f(0.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(0.6f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);

	TestTrue(TEXT("겹쳤을 때는 접촉항이 살아 있다"), !Overlapping.Contact.IsNearlyZero());
	TestTrue(TEXT("접촉항은 관통 방지용이므로 보행 스케일보다 커도 된다"),
		(Overlapping.Contact / PersonMassKg).Size() > Config.MaxSocialAccelMps2);

	const FVector2f Summed = FFF_CrowdForceSolver::ComputeAgentInteraction(
		FVector2f(0.f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(0.6f, 0.f), FVector2f::ZeroVector, PersonRadiusMeters, Config);
	TestEqual(TEXT("합계 래퍼 = 비접촉 + 접촉"), Summed.X, Overlapping.Total().X, 0.f);
	TestEqual(TEXT("합계 래퍼 = 비접촉 + 접촉"), Summed.Y, Overlapping.Total().Y, 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_DrivingForceCompetesWithRepulsion, "FestaFiesta.Crowd.Avoidance.DrivingForceCompetesWithRepulsion", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_DrivingForceCompetesWithRepulsion::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();

	const FVector2f Desired(1.5f, 0.f);
	const FVector2f Drive = FFF_CrowdForceSolver::ComputeDrivingForce(
		Desired, FVector2f::ZeroVector, PersonMassKg, Config.RelaxationTau);

	const float DriveAccel = (Drive / PersonMassKg).Size();
	TestTrue(*FString::Printf(TEXT("정지 상태의 구동 가속도는 v/tau = %.1f m/s^2다"), DriveAccel),
		FMath::IsNearlyEqual(DriveAccel, 1.5f / Config.RelaxationTau, 0.01f));

	const float Ratio = Config.MaxSocialAccelMps2 / DriveAccel;
	TestTrue(*FString::Printf(TEXT("회피 상한 / 구동 가속도 = %.2f (같은 자릿수여야 한다)"), Ratio),
		Ratio > 0.2f && Ratio < 5.f);

	const FVector2f AtRest = FFF_CrowdForceSolver::ComputeDrivingForce(
		FVector2f::ZeroVector, FVector2f::ZeroVector, PersonMassKg, Config.RelaxationTau);
	TestTrue(TEXT("희망 속도와 현재 속도가 같으면 구동력은 0이다"), AtRest.IsNearlyZero());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_OpposingCapCannotStopProgress, "FestaFiesta.Crowd.Avoidance.OpposingCapCannotStopProgress", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_OpposingCapCannotStopProgress::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();
	const FVector2f Forward(1.f, 0.f);

	const FVector2f HeadOn(-40.f, 0.f);
	const FVector2f Capped = FFF_CrowdForceSolver::CapOpposingAccel(
		HeadOn, Forward, Config.MaxOpposingAccelMps2);

	TestEqual(TEXT("전진 반대 성분이 상한까지 잘린다"),
		Capped.X, -Config.MaxOpposingAccelMps2, 0.01f);

	const float DriveAccel = 1.5f / Config.RelaxationTau;
	TestTrue(*FString::Printf(TEXT("역방향 상한(%.1f) < 구동 가속도(%.1f)"),
		Config.MaxOpposingAccelMps2, DriveAccel),
		Config.MaxOpposingAccelMps2 < DriveAccel);

	const FVector2f Oblique(-40.f, 12.f);
	const FVector2f CappedOblique = FFF_CrowdForceSolver::CapOpposingAccel(
		Oblique, Forward, Config.MaxOpposingAccelMps2);

	TestEqual(TEXT("수직 성분은 손대지 않는다"), CappedOblique.Y, 12.f, 0.01f);
	TestEqual(TEXT("전진 반대 성분만 잘린다"), CappedOblique.X, -Config.MaxOpposingAccelMps2, 0.01f);

	const FVector2f FromBehind(30.f, 5.f);
	const FVector2f Untouched = FFF_CrowdForceSolver::CapOpposingAccel(
		FromBehind, Forward, Config.MaxOpposingAccelMps2);
	TestEqual(TEXT("전진 방향 성분은 그대로 통과한다"), Untouched.X, 30.f, 0.01f);
	TestEqual(TEXT("그때 수직 성분도 그대로다"), Untouched.Y, 5.f, 0.01f);

	const FVector2f NoGoal = FFF_CrowdForceSolver::CapOpposingAccel(
		HeadOn, FVector2f::ZeroVector, Config.MaxOpposingAccelMps2);
	TestEqual(TEXT("갈 곳이 없으면 그대로 밀려난다"), NoGoal.X, HeadOn.X, 0.01f);

	const FVector2f Disabled = FFF_CrowdForceSolver::CapOpposingAccel(HeadOn, Forward, -1.f);
	TestEqual(TEXT("음수 상한은 '끔'이다"), Disabled.X, HeadOn.X, 0.01f);

	const FVector2f Unnormalised = FFF_CrowdForceSolver::CapOpposingAccel(
		HeadOn, FVector2f(1.5f, 0.f), Config.MaxOpposingAccelMps2);
	TestEqual(TEXT("희망 속도 크기와 무관하다"), Unnormalised.X, Capped.X, 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_OrientedRectWallsMatchFootprint, "FestaFiesta.Crowd.Avoidance.OrientedRectWallsMatchFootprint", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_OrientedRectWallsMatchFootprint::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	constexpr float HalfX = 5.f;
	constexpr float HalfY = 2.5f;
	const FVector2f Center(20.f, -8.f);

	TArray<FFF_CrowdWallSegment> Walls;
	FFF_CrowdWalls::AppendOrientedRect(Walls, Center, 0.f, HalfX, HalfY);

	TestEqual(TEXT("사각형 하나는 선분 4개다"), Walls.Num(), 4);

	auto Perimeter = [](const TArray<FFF_CrowdWallSegment>& In)
	{
		float Sum = 0.f;
		for (const FFF_CrowdWallSegment& W : In) { Sum += (W.B - W.A).Size(); }
		return Sum;
	};
	TestEqual(TEXT("둘레가 30m다"), Perimeter(Walls), 2.f * (2.f * HalfX + 2.f * HalfY), 0.01f);

	for (int32 i = 0; i < Walls.Num(); ++i)
	{
		const FFF_CrowdWallSegment& Cur = Walls[i];
		const FFF_CrowdWallSegment& Next = Walls[(i + 1) % Walls.Num()];
		TestEqual(*FString::Printf(TEXT("선분 %d의 끝이 다음 선분의 시작이다"), i),
			(Next.A - Cur.B).Size(), 0.f, 0.001f);
	}

	TArray<FFF_CrowdWallSegment> Rotated;
	FFF_CrowdWalls::AppendOrientedRect(Rotated, Center, HALF_PI, HalfX, HalfY);

	TestEqual(TEXT("90° 회전에서도 둘레는 같다"), Perimeter(Rotated), Perimeter(Walls), 0.01f);

	float MaxAbsY = 0.f;
	float MaxAbsX = 0.f;
	for (const FFF_CrowdWallSegment& W : Rotated)
	{
		MaxAbsX = FMath::Max(MaxAbsX, FMath::Abs(W.A.X - Center.X));
		MaxAbsY = FMath::Max(MaxAbsY, FMath::Abs(W.A.Y - Center.Y));
	}
	TestEqual(TEXT("90° 회전하면 X 반너비 5m가 Y로 옮겨간다"), MaxAbsY, HalfX, 0.01f);
	TestEqual(TEXT("그때 X 반너비는 2.5m가 된다"), MaxAbsX, HalfY, 0.01f);

	TArray<FFF_CrowdWallSegment> Oblique;
	FFF_CrowdWalls::AppendOrientedRect(Oblique, Center, PI * 0.25f, HalfX, HalfY);

	FVector2f CornerSum = FVector2f::ZeroVector;
	for (const FFF_CrowdWallSegment& W : Oblique) { CornerSum += W.A; }
	const FVector2f Centroid = CornerSum / 4.f;

	TestEqual(TEXT("45°에서도 네 꼭짓점의 평균이 중심이다"), (Centroid - Center).Size(), 0.f, 0.01f);
	TestEqual(TEXT("45°에서도 둘레는 같다"), Perimeter(Oblique), Perimeter(Walls), 0.01f);

	TArray<FFF_CrowdWallSegment> Degenerate;
	FFF_CrowdWalls::AppendOrientedRect(Degenerate, Center, 0.f, 0.f, HalfY);
	FFF_CrowdWalls::AppendOrientedRect(Degenerate, Center, 0.f, HalfX, 0.f);
	TestEqual(TEXT("반너비가 0이면 선분을 만들지 않는다"), Degenerate.Num(), 0);

	FFF_CrowdWalls::AppendOrientedRect(Walls, FVector2f(100.f, 100.f), 0.f, HalfX, HalfY);
	TestEqual(TEXT("기존 선분 뒤에 붙는다"), Walls.Num(), 8);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_WallCullingKeepsForceIntact, "FestaFiesta.Crowd.Avoidance.WallCullingKeepsForceIntact", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_WallCullingKeepsForceIntact::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();

	TestTrue(TEXT("컷오프가 기본으로 켜져 있다"), Config.WallCullRadiusMeters > 0.f);

	const FVector2f SegA(-50.f, 0.f);
	const FVector2f SegB(50.f, 0.f);
	const float CullDist = PersonRadiusMeters + Config.WallCullRadiusMeters;

	const FVector2f JustInside(0.f, CullDist * 0.99f);
	const FVector2f Force = FFF_CrowdForceSolver::ComputeWallForce(
		JustInside, FVector2f::ZeroVector, PersonRadiusMeters, SegA, SegB, Config);

	const float DiscardedAccel = Force.Size() / PersonMassKg;
	TestTrue(
		*FString::Printf(TEXT("컷오프 경계에서 버려지는 가속도가 무의미하다 (%.5f m/s^2)"), DiscardedAccel),
		DiscardedAccel < 0.01f);

	const FVector2f NearContact(0.f, PersonRadiusMeters * 1.05f);
	const FVector2f StrongForce = FFF_CrowdForceSolver::ComputeWallForce(
		NearContact, FVector2f::ZeroVector, PersonRadiusMeters, SegA, SegB, Config);

	TestTrue(
		*FString::Printf(TEXT("접촉 직전에서는 벽이 세게 밀어낸다 (%.1f m/s^2)"),
			StrongForce.Size() / PersonMassKg),
		StrongForce.Size() / PersonMassKg > Config.MaxSocialAccelMps2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_WallsAreNotSubjectToOpposingCap, "FestaFiesta.Crowd.Avoidance.WallsAreNotSubjectToOpposingCap", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_WallsAreNotSubjectToOpposingCap::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();

	const FVector2f Desired(1.5f, 0.f);
	const FVector2f Vel(0.f, 0.f);

	FFF_CrowdForceTerms WallTerms;
	WallTerms.Social = FVector2f(-Config.WallRepulsionA, 0.f);

	const FFF_CrowdForceTerms NoAgents;

	TestTrue(TEXT("기본값은 벽 면제(음수)다"), Config.MaxWallOpposingAccelMps2 < 0.f);

	const FVector2f WithExemption = FFF_CrowdForceSolver::ComposeAcceleration(
		NoAgents, WallTerms, Vel, Desired, true, true,
		PersonMassKg, Config);

	TestTrue(
		*FString::Printf(TEXT("면제 시 합성 가속도가 벽 쪽(-X)이다 (%.2f m/s^2)"), WithExemption.X),
		WithExemption.X < 0.f);

	FFF_CrowdAvoidanceConfig Capped = Config;
	Capped.MaxWallOpposingAccelMps2 = Capped.MaxOpposingAccelMps2;

	const FVector2f WithCap = FFF_CrowdForceSolver::ComposeAcceleration(
		NoAgents, WallTerms, Vel, Desired, true, true, PersonMassKg, Capped);

	TestTrue(
		*FString::Printf(TEXT("상한을 걸면 구동력이 이겨 벽 안으로 들어간다 (%.2f m/s^2)"), WithCap.X),
		WithCap.X > 0.f);

	TestTrue(TEXT("면제와 상한이 서로 반대 방향을 낸다"),
		(WithExemption.X < 0.f) && (WithCap.X > 0.f));

	FFF_CrowdForceTerms AgentTerms;
	AgentTerms.Social = FVector2f(-Config.RepulsionA * 20.f, 0.f);

	const FVector2f AgentsOnly = FFF_CrowdForceSolver::ComposeAcceleration(
		AgentTerms, FFF_CrowdForceTerms(), Vel, Desired, true, true, PersonMassKg, Config);

	TestTrue(
		*FString::Printf(TEXT("개체 반발만으로는 전진이 뒤집히지 않는다 (%.2f m/s^2)"), AgentsOnly.X),
		AgentsOnly.X > 0.f);

	FFF_CrowdForceTerms HugeWall;
	HugeWall.Social = FVector2f(-Config.WallRepulsionA * 100.f, 0.f);

	const FVector2f Clamped = FFF_CrowdForceSolver::ComposeAcceleration(
		NoAgents, HugeWall, Vel, Desired, true, false, PersonMassKg, Config);

	TestTrue(
		*FString::Printf(TEXT("벽 비접촉분이 보행 스케일 상한 안이다 (%.2f <= %.2f)"),
			Clamped.Size(), Config.MaxSocialAccelMps2),
		Clamped.Size() <= Config.MaxSocialAccelMps2 + 0.01f);

	FFF_CrowdForceTerms Penetrating;
	Penetrating.Contact = FVector2f(-Config.ContactStiffness * 0.1f, 0.f);

	const FVector2f Contact = FFF_CrowdForceSolver::ComposeAcceleration(
		NoAgents, Penetrating, Vel, Desired, true, true, PersonMassKg, Config);

	TestTrue(
		*FString::Printf(TEXT("벽 접촉분은 보행 스케일 상한을 넘어선다 (%.1f m/s^2)"), Contact.Size()),
		Contact.Size() > Config.MaxSocialAccelMps2);

	const FVector2f NoDesired = FFF_CrowdForceSolver::ComposeAcceleration(
		NoAgents, WallTerms, Vel, FVector2f::ZeroVector, false, true,
		PersonMassKg, Config);

	TestEqual(TEXT("희망 속도가 없으면 크기 상한만 걸린 벽 반발이 남는다"),
		NoDesired.X, -Config.MaxSocialAccelMps2, 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_LaneBiasSplitsOncomingPairs, "FestaFiesta.Crowd.Avoidance.LaneBiasSplitsOncomingPairs", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_LaneBiasSplitsOncomingPairs::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	FFF_CrowdAvoidanceConfig Config = MakeConfig();
	Config.LaneBiasGain = 0.6f;

	const FVector2f PosI(0.f, 0.f);
	const FVector2f PosJ(0.75f, 0.f);
	const FVector2f WantsRight(1.5f, 0.f);
	const FVector2f WantsLeft(-1.5f, 0.f);

	const FVector2f BiasI = FFF_CrowdForceSolver::ComputeLaneBias(
		PosI, PersonRadiusMeters, PosJ, PersonRadiusMeters, WantsRight, WantsLeft, Config);
	const FVector2f BiasJ = FFF_CrowdForceSolver::ComputeLaneBias(
		PosJ, PersonRadiusMeters, PosI, PersonRadiusMeters, WantsLeft, WantsRight, Config);

	TestTrue(TEXT("마주 오는 쌍에 접선력이 걸린다"), BiasI.Size() > 0.f && BiasJ.Size() > 0.f);
	TestTrue(TEXT("+X로 가는 사람은 자기 오른쪽(+Y)으로 비킨다"), BiasI.Y > 0.f);
	TestTrue(TEXT("-X로 가는 사람은 자기 오른쪽(-Y)으로 비킨다"), BiasJ.Y < 0.f);
	TestTrue(TEXT("두 사람이 서로 반대쪽으로 갈라진다"), BiasI.Y * BiasJ.Y < 0.f);

	TestEqual(TEXT("접선력에 전진 방향 성분이 없다"), BiasI.X, 0.f, 0.01f);

	const FVector2f SameWay = FFF_CrowdForceSolver::ComputeLaneBias(
		PosI, PersonRadiusMeters, PosJ, PersonRadiusMeters, WantsRight, WantsRight, Config);
	TestTrue(TEXT("같은 방향으로 걷는 사람에게는 걸리지 않는다"), SameWay.IsNearlyZero());

	const FVector2f Standing = FFF_CrowdForceSolver::ComputeLaneBias(
		PosI, PersonRadiusMeters, PosJ, PersonRadiusMeters, WantsRight, FVector2f::ZeroVector, Config);
	TestTrue(TEXT("서 있는 사람에게는 걸리지 않는다"), Standing.IsNearlyZero());

	const FVector2f Behind = FFF_CrowdForceSolver::ComputeLaneBias(
		PosI, PersonRadiusMeters, FVector2f(-0.75f, 0.f), PersonRadiusMeters, WantsRight, WantsLeft, Config);
	TestTrue(TEXT("뒤에 있는 사람에게는 걸리지 않는다"), Behind.IsNearlyZero());

	const FVector2f Far = FFF_CrowdForceSolver::ComputeLaneBias(
		PosI, PersonRadiusMeters, FVector2f(Config.NeighbourRadiusMeters, 0.f), PersonRadiusMeters,
		WantsRight, WantsLeft, Config);
	TestTrue(
		*FString::Printf(TEXT("이웃 반경(%.1fm) 끝에서는 무시할 만하다 (%.3fN)"),
			Config.NeighbourRadiusMeters, Far.Size()),
		Far.Size() < 1.f);

	FFF_CrowdAvoidanceConfig Disabled = Config;
	Disabled.LaneBiasGain = 0.f;
	const FVector2f Off = FFF_CrowdForceSolver::ComputeLaneBias(
		PosI, PersonRadiusMeters, PosJ, PersonRadiusMeters, WantsRight, WantsLeft, Disabled);
	TestTrue(TEXT("LaneBiasGain=0이면 완전히 꺼진다"), Off.IsNearlyZero());

	const FVector2f Oblique = FFF_CrowdForceSolver::ComputeLaneBias(
		PosI, PersonRadiusMeters, PosJ, PersonRadiusMeters,
		WantsRight, FVector2f(-1.06f, -1.06f), Config);
	TestTrue(TEXT("비스듬히 마주칠 때는 정면보다 약하다"), Oblique.Size() < BiasI.Size());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_WallPushesInward, "FestaFiesta.Crowd.Avoidance.WallPushesInward", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_WallPushesInward::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();

	const FVector2f SegA(-10.f, 0.f);
	const FVector2f SegB(10.f, 0.f);

	const FVector2f Force = FFF_CrowdForceSolver::ComputeWallForce(
		FVector2f(0.f, 0.3f), FVector2f(0.f, -0.5f), PersonRadiusMeters, SegA, SegB, Config);

	TestTrue(TEXT("벽에서 멀어지는 방향(+Y)으로 밀어낸다"), Force.Y > 0.f);

	const FVector2f Corner = FFF_CrowdForceSolver::ComputeWallForce(
		FVector2f(10.2f, 0.1f), FVector2f::ZeroVector, PersonRadiusMeters, SegA, SegB, Config);
	TestTrue(TEXT("끝점 밖에서도 유한하다"), FMath::IsFinite(Corner.X) && FMath::IsFinite(Corner.Y));

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

	TestEqual(TEXT("같은 ID는 같은 방향"), A1.X, A2.X, 0.f);
	TestEqual(TEXT("같은 ID는 같은 방향"), A1.Y, A2.Y, 0.f);

	TestTrue(TEXT("다른 ID는 다른 방향으로 흩어진다"),
		!FMath::IsNearlyEqual(A1.Y, B.Y, 1.e-4f));

	TestEqual(TEXT("단위 벡터다"), A1.Size(), 1.f, 1.e-3f);

	const float CosAngle = FVector2f::DotProduct(A1, Suggested);
	TestTrue(TEXT("제안 방향에서 60도 이내다"), CosAngle >= FMath::Cos(PI / 3.f) - 1.e-3f);

	const FVector2f NoHint = FFF_CrowdForceSolver::ComputeUnstuckImpulse(7, FVector2f::ZeroVector);
	TestEqual(TEXT("제안이 없어도 단위 벡터다"), NoHint.Size(), 1.f, 1.e-3f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdGrid_GatherIsAscendingAndBounded, "FestaFiesta.Crowd.Grid.GatherIsAscendingAndBounded", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdGrid_GatherIsAscendingAndBounded::RunTest(const FString& Parameters)
{
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

	TArray<int32> Neighbours;
	Grid.GatherNearest(Pos, Pos[10], 2.f, 32, Neighbours);

	TestEqual(TEXT("반경 2m 안의 인원(자기 포함)"), Neighbours.Num(), 9);
	TestTrue(TEXT("자기 자신이 포함된다(호출자가 건너뛴다)"), Neighbours.Contains(10));

	for (int32 k = 1; k < Neighbours.Num(); ++k)
	{
		if (Neighbours[k] <= Neighbours[k - 1])
		{
			AddError(TEXT("GatherNearest 결과가 인덱스 오름차순이 아니다 — 결정성이 깨진다"));
			return false;
		}
	}

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
		Active.Add((i % 2 == 0) ? 1 : 0);
		ActiveIds.Add(i);
	}

	FFF_CrowdSpatialGrid Grid;
	Grid.Build(Pos, Active, ActiveIds, 2.f);

	TArray<int32> Neighbours;
	Grid.GatherNearest(Pos, Pos[0], 2.f, 32, Neighbours);

	TestEqual(TEXT("살아있는 3명만 잡힌다"), Neighbours.Num(), 3);
	TestFalse(TEXT("죽은 슬롯은 들어오지 않는다"), Neighbours.Contains(1));

	FFF_CrowdSpatialGrid Empty;
	TestFalse(TEXT("빈 격자는 무효다"), Empty.IsValid());
	Empty.GatherNearest(Pos, Pos[0], 2.f, 32, Neighbours);
	TestEqual(TEXT("무효 격자는 빈 결과"), Neighbours.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdGrid_LeastCrowdedNeighbourIsStable, "FestaFiesta.Crowd.Grid.LeastCrowdedNeighbourIsStable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdGrid_LeastCrowdedNeighbourIsStable::RunTest(const FString& Parameters)
{
	TArray<FVector2f> Pos;
	TArray<uint8> Active;
	TArray<int32> ActiveIds;
	for (int32 i = 0; i < 12; ++i)
	{
		Pos.Add(FVector2f(-3.f + (i % 3) * 0.1f, 0.f));
		Active.Add(1);
		ActiveIds.Add(i);
	}
	Pos.Add(FVector2f(5.f, 0.f));
	Active.Add(1);
	ActiveIds.Add(12);

	FFF_CrowdSpatialGrid Grid;
	Grid.Build(Pos, Active, ActiveIds, 2.f);

	FVector2f First, Second;
	const bool bOk1 = Grid.FindLeastCrowdedNeighbourCell(Pos[0], First);
	const bool bOk2 = Grid.FindLeastCrowdedNeighbourCell(Pos[0], Second);

	TestTrue(TEXT("인접 셀을 찾았다"), bOk1 && bOk2);
	TestEqual(TEXT("같은 상황에서 항상 같은 셀을 고른다"), First.X, Second.X, 0.f);
	TestEqual(TEXT("같은 상황에서 항상 같은 셀을 고른다"), First.Y, Second.Y, 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_ZeroForceChangesNothing, "FestaFiesta.Crowd.Steering.ZeroForceChangesNothing", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_ZeroForceChangesNothing::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);

	CrowdSteering::FStepParams Params;

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

	FUnitData Pushed = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Pushed.TargetLocation = FVector(5000.f, 0.f, 0.f);
	for (int32 i = 0; i < 60; ++i)
	{
		Pushed.Force = FVector(0.f, 500.f, 0.f);
		CrowdSteering::StepAgent(Pushed, Params, Alpha, FixedStep);
	}

	TestTrue(TEXT("회피 가속도가 실제로 속도에 반영된다"), Pushed.Position.Y > 1.f);
	TestTrue(TEXT("그래도 MaxSpeed는 넘지 않는다"), Pushed.Velocity.Size() <= Pushed.MaxSpeed + 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_DrivingForcePathReachesTarget, "FestaFiesta.Crowd.Steering.DrivingForcePathReachesTarget", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_DrivingForcePathReachesTarget::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;
	constexpr float MassKg = 70.f;

	CrowdSteering::FStepParams Params;
	Params.bDrivingForceInForce = true;

	const FFF_CrowdAvoidanceConfig Config;

	constexpr int32 NumSteps = 900;
	const FVector Target(1000.f, 0.f, 0.f);

	FUnitData Unit = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Unit.TargetLocation = Target;

	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);

	for (int32 i = 0; i < NumSteps; ++i)
	{
		const FVector DesiredUU = CrowdSteering::ComputeDesiredVelocity(Unit, Params);
		const FVector2f DesiredM(
			static_cast<float>(DesiredUU.X) * FFF_CrowdUnits::MetersPerUU,
			static_cast<float>(DesiredUU.Y) * FFF_CrowdUnits::MetersPerUU);
		const FVector2f VelM(
			static_cast<float>(Unit.Velocity.X) * FFF_CrowdUnits::MetersPerUU,
			static_cast<float>(Unit.Velocity.Y) * FFF_CrowdUnits::MetersPerUU);

		const FVector2f Accel = FFF_CrowdForceSolver::ClampAcceleration(
			FFF_CrowdForceSolver::ComputeDrivingForce(DesiredM, VelM, MassKg, Config.RelaxationTau) / MassKg,
			Config.MaxAccelerationMps2);

		Unit.Force = FVector(Accel.X * FFF_CrowdUnits::UUPerMeter, Accel.Y * FFF_CrowdUnits::UUPerMeter, 0.f);
		CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
	}

	TestTrue(TEXT("구동력만으로 목적지에 도착한다"),
		CrowdSteering::HasArrived(Unit, Unit.ArrivalRadiusUU));
	TestEqual(TEXT("옆으로 새지 않는다"), static_cast<float>(Unit.Position.Y), 0.f, 0.01f);
	TestTrue(TEXT("MaxSpeed를 넘지 않는다"), Unit.Velocity.Size() <= Unit.MaxSpeed + 0.01f);
	TestEqual(TEXT("도착해서 선 사람은 막힌 것이 아니다"), Unit.StuckSteps, 0);

	CrowdSteering::FStepParams LerpParams;
	FUnitData ViaLerp = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	ViaLerp.TargetLocation = Target;
	for (int32 i = 0; i < NumSteps; ++i)
	{
		CrowdSteering::StepAgent(ViaLerp, LerpParams, Alpha, FixedStep);
	}
	TestTrue(TEXT("Lerp 경로도 여전히 목적지에 도착한다"),
		CrowdSteering::HasArrived(ViaLerp, ViaLerp.ArrivalRadiusUU));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_StuckCounterOnlyCountsBlockedAgents, "FestaFiesta.Crowd.Steering.StuckCounterOnlyCountsBlockedAgents", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_StuckCounterOnlyCountsBlockedAgents::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);
	CrowdSteering::FStepParams Params;

	FUnitData Walking = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Walking.TargetLocation = FVector(5000.f, 0.f, 0.f);
	for (int32 i = 0; i < 30; ++i)
	{
		CrowdSteering::StepAgent(Walking, Params, Alpha, FixedStep);
	}
	TestEqual(TEXT("걷는 사람의 막힘 카운터는 0이다"), Walking.StuckSteps, 0);

	FUnitData Arrived = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Arrived.TargetLocation = FVector::ZeroVector;
	for (int32 i = 0; i < 30; ++i)
	{
		CrowdSteering::StepAgent(Arrived, Params, Alpha, FixedStep);
	}
	TestEqual(TEXT("도착해서 서 있는 사람의 카운터는 0이다"), Arrived.StuckSteps, 0);

	FUnitData Blocked = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Blocked.TargetLocation = FVector(5000.f, 0.f, 0.f);
	Blocked.Speed = 0.f;
	for (int32 i = 0; i < 30; ++i)
	{
		CrowdSteering::StepAgent(Blocked, Params, Alpha, FixedStep);
	}
	TestEqual(TEXT("막힌 사람은 스텝마다 카운터가 오른다"), Blocked.StuckSteps, 30);

	Blocked.Speed = 150.f;
	for (int32 i = 0; i < 5; ++i)
	{
		CrowdSteering::StepAgent(Blocked, Params, Alpha, FixedStep);
	}
	TestEqual(TEXT("다시 움직이면 카운터가 초기화된다"), Blocked.StuckSteps, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_StuckCounterMeasuresProgressNotDisplacement, "FestaFiesta.Crowd.Steering.StuckCounterMeasuresProgressNotDisplacement", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_StuckCounterMeasuresProgressNotDisplacement::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);
	CrowdSteering::FStepParams Params;

	FUnitData Shoved = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Shoved.TargetLocation = FVector(5000.f, 0.f, 0.f);
	Shoved.Speed = 0.f;

	for (int32 i = 0; i < 60; ++i)
	{
		Shoved.Force = FVector(0.f, 3000.f, 0.f);
		CrowdSteering::StepAgent(Shoved, Params, Alpha, FixedStep);
	}

	TestTrue(*FString::Printf(TEXT("옆으로 실제로 밀려났다 (%.0fUU)"), Shoved.Position.Y),
		Shoved.Position.Y > 100.f);
	TestTrue(TEXT("그러나 목적지 쪽으로는 나아가지 못했다"), Shoved.Position.X < 1.f);

	TestEqual(TEXT("옆으로 밀리는 사람도 막힘으로 잡힌다"), Shoved.StuckSteps, 60);

	FUnitData PushedBack = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	PushedBack.TargetLocation = FVector(5000.f, 0.f, 0.f);
	PushedBack.Speed = 0.f;
	for (int32 i = 0; i < 10; ++i)
	{
		PushedBack.Force = FVector(-3000.f, 0.f, 0.f);
		CrowdSteering::StepAgent(PushedBack, Params, Alpha, FixedStep);
	}
	TestTrue(TEXT("뒤로 밀렸다"), PushedBack.Position.X < -1.f);
	TestEqual(TEXT("후퇴도 막힘이다"), PushedBack.StuckSteps, 10);

	FUnitData Carried = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Carried.TargetLocation = FVector(5000.f, 0.f, 0.f);
	Carried.Speed = 0.f;
	for (int32 i = 0; i < 10; ++i)
	{
		Carried.Force = FVector(3000.f, 0.f, 0.f);
		CrowdSteering::StepAgent(Carried, Params, Alpha, FixedStep);
	}
	TestTrue(TEXT("목적지 쪽으로 밀려갔다"), Carried.Position.X > 1.f);
	TestEqual(TEXT("목적지 쪽으로 밀려가는 것은 막힘이 아니다"), Carried.StuckSteps, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAvoidance_ContactFrictionCannotReverseTangentialMotion, "FestaFiesta.Crowd.Avoidance.ContactFrictionCannotReverseTangentialMotion", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAvoidance_ContactFrictionCannotReverseTangentialMotion::RunTest(const FString& Parameters)
{
	using namespace FFF_AvoidanceTestHelpers;

	const FFF_CrowdAvoidanceConfig Config = MakeConfig();

	constexpr float OverlapMeters = 0.06f;
	constexpr float TangentialSpeedMps = 0.4f;

	const FVector2f PosI(0.f, 0.f);
	const FVector2f PosJ(PersonRadiusMeters * 2.f - OverlapMeters, 0.f);
	const FVector2f VelJ(0.f, TangentialSpeedMps);

	const FVector2f Tangent(0.f, -1.f);

	const float UnclampedDeltaV = Config.ContactFriction * OverlapMeters * TangentialSpeedMps
		/ PersonMassKg * FixedStep;

	TestTrue(
		*FString::Printf(TEXT("클램프가 없다면 한 스텝 접선 속도 변화가 상대 속도를 넘긴다 (%.2f > %.2f m/s)"),
			UnclampedDeltaV, TangentialSpeedMps),
		UnclampedDeltaV > TangentialSpeedMps);

	const FFF_CrowdForceTerms Terms = FFF_CrowdForceSolver::ComputeAgentInteractionTerms(
		PosI, FVector2f::ZeroVector, PersonRadiusMeters,
		PosJ, VelJ, PersonRadiusMeters, Config, PersonMassKg, FixedStep);

	const float TangentialAccel = FMath::Abs(
		FVector2f::DotProduct(Terms.Contact / PersonMassKg, Tangent));
	const float DeltaV = TangentialAccel * FixedStep;

	TestTrue(
		*FString::Printf(TEXT("마찰 임펄스가 상대 접선 속도의 절반을 넘지 않는다 (%.3f <= %.3f m/s)"),
			DeltaV, TangentialSpeedMps * 0.5f),
		DeltaV <= TangentialSpeedMps * 0.5f + KINDA_SMALL_NUMBER);

	const FFF_CrowdForceTerms NoSlide = FFF_CrowdForceSolver::ComputeAgentInteractionTerms(
		PosI, FVector2f::ZeroVector, PersonRadiusMeters,
		PosJ, FVector2f::ZeroVector, PersonRadiusMeters, Config, PersonMassKg, FixedStep);

	TestEqual(TEXT("접선 상대 속도가 없으면 마찰도 없다"),
		FVector2f::DotProduct(NoSlide.Contact, Tangent), 0.f, KINDA_SMALL_NUMBER);

	constexpr float ShallowOverlapMeters = 0.003f;

	const FFF_CrowdForceTerms Shallow = FFF_CrowdForceSolver::ComputeAgentInteractionTerms(
		PosI, FVector2f::ZeroVector, PersonRadiusMeters,
		FVector2f(PersonRadiusMeters * 2.f - ShallowOverlapMeters, 0.f), VelJ, PersonRadiusMeters,
		Config, PersonMassKg, FixedStep);

	const float ShallowFriction = FMath::Abs(FVector2f::DotProduct(Shallow.Contact, Tangent));
	const float ShallowUnclamped = Config.ContactFriction * ShallowOverlapMeters * TangentialSpeedMps;

	TestEqual(TEXT("안정 영역의 얕은 겹침은 클램프가 손대지 않는다"),
		ShallowFriction, ShallowUnclamped, ShallowUnclamped * 0.01f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_FacingIgnoresJamJitter, "FestaFiesta.Crowd.Steering.FacingIgnoresJamJitter", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_FacingIgnoresJamJitter::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);
	CrowdSteering::FStepParams Params;

	FUnitData Jammed = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Jammed.TargetLocation = FVector(5000.f, 0.f, 0.f);
	Jammed.Speed = 0.f;

	float TotalYawSwing = 0.f;
	float MaxLateralSpeed = 0.f;

	for (int32 i = 0; i < 120; ++i)
	{
		Jammed.Force = FVector(0.f, (i % 2 == 0) ? 3000.f : -3000.f, 0.f);

		const float BeforeYaw = Jammed.Rotation.Yaw;
		CrowdSteering::StepAgent(Jammed, Params, Alpha, FixedStep);

		TotalYawSwing += FMath::Abs(FMath::FindDeltaAngleDegrees(BeforeYaw, Jammed.Rotation.Yaw));
		MaxLateralSpeed = FMath::Max(MaxLateralSpeed, FMath::Abs(static_cast<float>(Jammed.Velocity.Y)));
	}

	TestTrue(
		*FString::Printf(TEXT("순간 속도는 실제로 좌우로 흔들렸다 (%.1f UU/s)"), MaxLateralSpeed),
		MaxLateralSpeed > 50.f);

	TestTrue(
		*FString::Printf(TEXT("그래도 도리도리하지 않는다 (총 회전 %.1f도)"), TotalYawSwing),
		TotalYawSwing < 10.f);

	TestEqual(TEXT("막힌 사람은 목적지 쪽을 계속 바라본다"),
		static_cast<float>(Jammed.Rotation.Yaw), 0.f, Params.FacingDeadzoneDegrees);

	FUnitData Walking = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Walking.TargetLocation = FVector(0.f, 5000.f, 0.f);

	for (int32 i = 0; i < 120; ++i)
	{
		CrowdSteering::StepAgent(Walking, Params, Alpha, FixedStep);
	}

	TestTrue(TEXT("걷는 사람은 여전히 진행 방향을 향한다"), Walking.bFacingFollowsVelocity);
	TestEqual(TEXT("+Y로 걸으면 +Y를 바라본다"),
		static_cast<float>(Walking.Rotation.Yaw), 90.f, 1.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdSteering_UnstuckHoldSurvivesCounterReset, "FestaFiesta.Crowd.Steering.UnstuckHoldSurvivesCounterReset", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdSteering_UnstuckHoldSurvivesCounterReset::RunTest(const FString& Parameters)
{
	constexpr float FixedStep = 1.f / 30.f;
	const float Alpha = CrowdSteering::ComputeVelocityAlpha(0.35f, FixedStep);

	CrowdSteering::FStepParams Params;

	FUnitData Unit = FFF_CrowdTestHelpers::MakeUnitAt(FVector::ZeroVector);
	Unit.TargetLocation = FVector(5000.f, 0.f, 0.f);
	Unit.Speed = 0.f;

	for (int32 i = 0; i < Params.StuckDetectSteps - 1; ++i)
	{
		CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
	}
	TestEqual(TEXT("임계 직전에는 유지 카운터가 아직 0이다"), Unit.UnstuckHoldSteps, 0);

	CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
	TestEqual(TEXT("임계에 닿으면 유지 카운터가 채워진다"),
		Unit.UnstuckHoldSteps, Params.UnstuckHoldSteps);

	Unit.Speed = 150.f;
	Unit.Velocity = FVector(150.f, 0.f, 0.f);
	CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);

	TestEqual(TEXT("다시 움직이면 막힘 카운터는 초기화된다"), Unit.StuckSteps, 0);
	TestEqual(TEXT("그래도 탈출 가속은 한 스텝에 사라지지 않는다"),
		Unit.UnstuckHoldSteps, Params.UnstuckHoldSteps - 1);

	for (int32 i = 0; i < Params.UnstuckHoldSteps; ++i)
	{
		CrowdSteering::StepAgent(Unit, Params, Alpha, FixedStep);
	}
	TestEqual(TEXT("유지 구간이 끝나면 0으로 내려간다"), Unit.UnstuckHoldSteps, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdMetrics_CellSizeHasSingleSource, "FestaFiesta.Crowd.Metrics.CellSizeHasSingleSource", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdMetrics_CellSizeHasSingleSource::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("m와 UU 표현이 100배 관계다"),
		FFF_CrowdMetrics::AnalysisCellSizeUU,
		FFF_CrowdMetrics::AnalysisCellSizeMeters * 100.f, 0.001f);

	const FFF_CrowdReadModel DefaultModel;
	TestEqual(TEXT("ReadModel 기본 셀 크기 = 단일 출처"),
		DefaultModel.CellSizeMeters, FFF_CrowdMetrics::AnalysisCellSizeMeters, 0.f);

	FCrowdDensityGrid Grid;
	Grid.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), FFF_CrowdMetrics::AnalysisCellSizeUU);

	TestEqual(TEXT("격자 셀 크기(m) = 단일 출처"),
		Grid.GetCellSizeMeters(), FFF_CrowdMetrics::AnalysisCellSizeMeters, 0.001f);
	TestEqual(TEXT("셀 면적 = 셀 크기의 제곱"),
		Grid.GetCellAreaSqM(),
		FFF_CrowdMetrics::AnalysisCellSizeMeters * FFF_CrowdMetrics::AnalysisCellSizeMeters, 0.001f);

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
	Units.Add(FFF_CrowdTestHelpers::MakeUnitAt(FVector(50.f, 50.f, 0.f)));

	Grid.Update(Units);
	TestEqual(TEXT("첫 등장이 진입 1회로 잡힌다"), Grid.GetCellThroughput()[0], 1);

	Units[0].Position = FVector(150.f, 150.f, 0.f);
	Grid.Update(Units);
	Grid.Update(Units);
	Grid.Update(Units);
	TestEqual(TEXT("같은 칸에 머무는 동안은 늘지 않는다"), Grid.GetCellThroughput()[0], 1);

	Units[0].Position = FVector(250.f, 50.f, 0.f);
	Grid.Update(Units);
	TestEqual(TEXT("목적 칸의 진입이 1"), Grid.GetCellThroughput()[1], 1);
	TestEqual(TEXT("떠난 칸은 변하지 않는다"), Grid.GetCellThroughput()[0], 1);

	Units[0].Position = FVector(50.f, 50.f, 0.f);
	Grid.Update(Units);
	TestEqual(TEXT("되돌아온 칸의 진입이 2"), Grid.GetCellThroughput()[0], 2);

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

	Grid.ResetThroughputWindow();
	TestEqual(TEXT("구간을 끊으면 0"), Grid.GetCellThroughput()[0], 0);

	Grid.Update(Units);
	Grid.Update(Units);
	TestEqual(TEXT("제자리인 사람은 새 구간에서도 0이다"), Grid.GetCellThroughput()[0], 0);

	TestEqual(TEXT("점유는 그대로 1"), Grid.GetCellCounts()[0], 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdMetrics_SlotReuseIsCountedAsNewEntry, "FestaFiesta.Crowd.Metrics.SlotReuseIsCountedAsNewEntry", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdMetrics_SlotReuseIsCountedAsNewEntry::RunTest(const FString& Parameters)
{
	FCrowdDensityGrid Grid;
	Grid.Build(FVector::ZeroVector, FVector2D(1000.f, 1000.f), 200.f);

	TArray<FUnitData> Units;
	Units.Add(FFF_CrowdTestHelpers::MakeUnitAt(FVector(50.f, 50.f, 0.f)));
	Grid.Update(Units);

	Units[0].bIsActive = false;
	Grid.Update(Units);
	TestEqual(TEXT("퇴장자는 점유에서 빠진다"), Grid.GetCellCounts()[0], 0);

	Units[0].bIsActive = true;
	Grid.Update(Units);
	TestEqual(TEXT("슬롯 재사용이 새 진입으로 잡힌다"), Grid.GetCellThroughput()[0], 2);

	Units[0].bIsActive = false;
	Grid.Update(Units);
	Units[0].Position = FVector(850.f, 850.f, 0.f);
	Units[0].bIsActive = true;
	Grid.Update(Units);

	const int32 FarCell = 4 * Grid.GetNumX() + 4;
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

	TestEqual(TEXT("Tick 전에는 발행 0회"), CrowdSim->GetSnapshotCount(), 0);

	CrowdSim->SnapshotIntervalSteps = 10;
	FFF_CrowdWorldTestHelpers::TickFor(*CrowdSim, 1.0f);

	TestTrue(TEXT("주기마다 발행된다"), CrowdSim->GetSnapshotCount() >= 2);

	const FFF_CrowdReadModel& ReadModel = CrowdSim->GetCrowdReadModel();

	TestEqual(TEXT("스냅샷 셀 크기 = 격자 셀 크기"),
		ReadModel.CellSizeMeters, CrowdSim->GetDensityGrid().GetCellSizeMeters(), 0.001f);
	TestEqual(TEXT("스냅샷 원점 = 격자 원점"),
		static_cast<float>(ReadModel.GridOriginUU.X),
		static_cast<float>(CrowdSim->GetDensityGrid().GetOriginUU().X), 0.01f);

	TestTrue(TEXT("유효 셀이 담겼다"), ReadModel.OccupiedCells.Num() > 0);
	TestTrue(TEXT("스냅샷 프레임이 기록됐다"), ReadModel.SnapshotFrame != INDEX_NONE);

	TestTrue(TEXT("빈 칸은 담기지 않는다"),
		ReadModel.OccupiedCells.Num() < CrowdSim->GetDensityGrid().GetNumX() * CrowdSim->GetDensityGrid().GetNumY());

	int32 SumOccupancy = 0;
	for (const FFF_CrowdCellSample& Cell : ReadModel.OccupiedCells)
	{
		SumOccupancy += Cell.Occupancy;

		const float Expected = Cell.Occupancy / (ReadModel.CellSizeMeters * ReadModel.CellSizeMeters);
		TestEqual(TEXT("밀도 = 인원 / 셀 면적"), Cell.DensityPerSqm, Expected, 0.001f);
	}

	TestEqual(TEXT("스냅샷 인원 합 = 격자 집계 인원"),
		SumOccupancy, CrowdSim->GetDensityGrid().GetSampledUnitCount());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
