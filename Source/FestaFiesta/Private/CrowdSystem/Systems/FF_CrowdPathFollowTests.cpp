// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/Systems/FF_CrowdPathFollow.h"
#include "CrowdSystem/Systems/FF_CrowdSteering.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"

namespace FFF_PathFollowTestHelpers
{
	TArray<FVector> MakeCornerPath()
	{
		return { FVector(1000, 0, 0), FVector(1000, 1000, 0) };
	}

	constexpr float AcceptanceUU = 120.f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPath_ConsumesWaypointsInOrder, "FestaFiesta.Crowd.Path.ConsumesWaypointsInOrder", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPath_ConsumesWaypointsInOrder::RunTest(const FString& Parameters)
{
	using namespace FFF_PathFollowTestHelpers;

	const TArray<FVector> Path = MakeCornerPath();

	TestEqual(TEXT("출발 지점에서는 아직 아무것도 소비하지 않는다"),
		CrowdPathFollow::AdvanceToReachableWaypoint(Path, 0, FVector::ZeroVector, AcceptanceUU), 0);

	TestEqual(TEXT("코너 직전에도 소비하지 않는다"),
		CrowdPathFollow::AdvanceToReachableWaypoint(Path, 0, FVector(800, 0, 0), AcceptanceUU), 0);

	TestEqual(TEXT("코너에 닿으면 첫 웨이포인트를 소비한다"),
		CrowdPathFollow::AdvanceToReachableWaypoint(Path, 0, FVector(1050, 0, 0), AcceptanceUU), 1);

	TestEqual(TEXT("끝점에 닿으면 전부 소비한다(= Points.Num())"),
		CrowdPathFollow::AdvanceToReachableWaypoint(Path, 1, FVector(1000, 1000, 0), AcceptanceUU),
		Path.Num());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPath_DoesNotSkipBackwardsOrAcrossCorners, "FestaFiesta.Crowd.Path.DoesNotSkipBackwardsOrAcrossCorners", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPath_DoesNotSkipBackwardsOrAcrossCorners::RunTest(const FString& Parameters)
{
	using namespace FFF_PathFollowTestHelpers;

	const TArray<FVector> Path = MakeCornerPath();

	const FVector InsideCorner(900, 700, 0);

	TestTrue(TEXT("전제: 안쪽에서는 끝점이 코너보다 가깝다"),
		FVector::Dist2D(InsideCorner, Path[1]) < FVector::Dist2D(InsideCorner, Path[0]));

	TestEqual(TEXT("그래도 코너를 먼저 찍는다"),
		CrowdPathFollow::AdvanceToReachableWaypoint(Path, 0, InsideCorner, AcceptanceUU), 0);

	TestEqual(TEXT("소비한 웨이포인트로 되돌아가지 않는다"),
		CrowdPathFollow::AdvanceToReachableWaypoint(Path, 1, FVector(500, 0, 0), AcceptanceUU), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPath_ConsumesClusteredWaypointsInOneStep, "FestaFiesta.Crowd.Path.ConsumesClusteredWaypointsInOneStep", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPath_ConsumesClusteredWaypointsInOneStep::RunTest(const FString& Parameters)
{
	using namespace FFF_PathFollowTestHelpers;

	const TArray<FVector> Dense = {
		FVector(10, 0, 0), FVector(20, 0, 0), FVector(30, 0, 0), FVector(2000, 0, 0)
	};

	TestEqual(TEXT("반경 안에 겹친 웨이포인트는 한 번에 전부 소비한다"),
		CrowdPathFollow::AdvanceToReachableWaypoint(Dense, 0, FVector::ZeroVector, AcceptanceUU), 3);

	TestEqual(TEXT("빈 경로는 0을 돌려준다"),
		CrowdPathFollow::AdvanceToReachableWaypoint({}, 0, FVector::ZeroVector, AcceptanceUU), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPath_SteeringFallsBackToFinalTargetWithoutPath, "FestaFiesta.Crowd.Path.SteeringFallsBackToFinalTargetWithoutPath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPath_SteeringFallsBackToFinalTargetWithoutPath::RunTest(const FString& Parameters)
{
	FUnitData Unit;
	Unit.Position = FVector::ZeroVector;
	Unit.TargetLocation = FVector(1000, 0, 0);
	Unit.SteerTargetLocation = FVector(0, -5000, 0);
	Unit.bHasSteerTarget = false;

	TestEqual(TEXT("플래그가 꺼져 있으면 최종 목적지를 쓴다"),
		Unit.GetSteerTarget(), Unit.TargetLocation);

	CrowdSteering::FStepParams Params;
	const FVector Desired = CrowdSteering::ComputeDesiredVelocity(Unit, Params);
	TestTrue(TEXT("조향이 최종 목적지 방향(+X)이다"), Desired.X > 0.f);
	TestEqual(TEXT("엉뚱한 SteerTarget 쪽(-Y)으로 가지 않는다"), Desired.Y, 0.0, 0.01);

	Unit.bHasSteerTarget = true;
	Unit.SteerTargetLocation = FVector(0, 1000, 0);

	const FVector Detoured = CrowdSteering::ComputeDesiredVelocity(Unit, Params);
	TestTrue(TEXT("플래그가 켜지면 웨이포인트 방향(+Y)으로 꺾는다"), Detoured.Y > 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdPath_DetourDoesNotCountAsStuck, "FestaFiesta.Crowd.Path.DetourDoesNotCountAsStuck", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdPath_DetourDoesNotCountAsStuck::RunTest(const FString& Parameters)
{
	FUnitData Unit;
	Unit.Position = FVector::ZeroVector;
	Unit.TargetLocation = FVector(1000, 0, 0);
	Unit.SteerTargetLocation = FVector(0, 1000, 0);
	Unit.bHasSteerTarget = true;
	Unit.Velocity = FVector(0, 150, 0);
	Unit.StuckSteps = 0;

	CrowdSteering::FStepParams Params;
	Params.bDrivingForceInForce = true;

	constexpr float FixedDt = 1.f / 30.f;
	for (int32 Step = 0; Step < 10; ++Step)
	{
		CrowdSteering::StepAgent(Unit, Params, 1.f, FixedDt);
	}

	TestEqual(TEXT("우회 방향으로 나아가는 동안 막힘이 쌓이지 않는다"), Unit.StuckSteps, 0);
	TestTrue(TEXT("실제로 +Y로 이동했다"), Unit.Position.Y > 40.f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
