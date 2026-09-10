// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/Systems/FF_CrowdPathFollow.h"
#include "CrowdSystem/Systems/FF_CrowdSplinePath.h"

namespace FFF_SplinePathTestHelpers
{
	FFF_CrowdSplineLane MakeLane(const TArray<FVector>& Points, bool bOneWay = false)
	{
		FFF_CrowdSplineLane Lane;
		Lane.Points = Points;
		Lane.bOneWay = bOneWay;
		Lane.RebuildDerived();
		return Lane;
	}

	FFF_CrowdSplineLane MakeStraightLane(bool bOneWay = false)
	{
		return MakeLane({ FVector(0, 0, 0), FVector(1000, 0, 0), FVector(2000, 0, 0), FVector(3000, 0, 0) },
			bOneWay);
	}

	FFF_CrowdSplineLane MakeSquareLoop()
	{
		return MakeLane({ FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 1000, 0),
			FVector(0, 1000, 0), FVector(0, 0, 0) });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SplinePath_FindsClosestPointWithArcLength, "FestaFiesta.Crowd.SplinePath.FindsClosestPointWithArcLength", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SplinePath_FindsClosestPointWithArcLength::RunTest(const FString& Parameters)
{
	using namespace FFF_SplinePathTestHelpers;

	const FFF_CrowdSplineLane Lane = MakeStraightLane();

	TestEqual(TEXT("전체 호길이는 3000이다"), Lane.TotalLengthCm(), 3000.f, 0.1f);
	TestFalse(TEXT("열린 레인은 루프가 아니다"), Lane.bClosedLoop);

	FVector Closest = FVector::ZeroVector;
	float ArcCm = 0.f;
	float DistCm = 0.f;

	TestTrue(TEXT("최근접점을 찾는다"),
		CrowdSplinePath::FindClosestOnLane(Lane, FVector(1500, 400, 0), Closest, ArcCm, DistCm));

	TestTrue(TEXT("중심선 위로 떨어진다"), Closest.Equals(FVector(1500, 0, 0), 1.f));
	TestEqual(TEXT("호길이가 1500이다"), ArcCm, 1500.f, 1.f);
	TestEqual(TEXT("횡거리가 400이다"), DistCm, 400.f, 1.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SplinePath_BuildsPathThroughLaneVertices, "FestaFiesta.Crowd.SplinePath.BuildsPathThroughLaneVertices", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SplinePath_BuildsPathThroughLaneVertices::RunTest(const FString& Parameters)
{
	using namespace FFF_SplinePathTestHelpers;

	const TArray<FFF_CrowdSplineLane> Lanes = { MakeStraightLane() };

	CrowdSplinePath::FQueryParams Params;
	TArray<FVector> Points;
	int32 LaneIndex = INDEX_NONE;

	TestTrue(TEXT("경로가 나온다"),
		CrowdSplinePath::TryBuildPath(Lanes, FVector(100, 200, 0), FVector(2900, 250, 0), Params,
			Points, LaneIndex));

	TestEqual(TEXT("레인 0을 골랐다"), LaneIndex, 0);
	TestEqual(TEXT("진입점 + 중간 정점 둘 + 이탈점 + 목적지"), Points.Num(), 5);
	TestTrue(TEXT("진입점이 중심선 위다"), Points[0].Equals(FVector(100, 0, 0), 1.f));
	TestTrue(TEXT("마지막은 언제나 최종 목적지다"), Points.Last().Equals(FVector(2900, 250, 0), 1.f));

	for (int32 Index = 1; Index < Points.Num() - 1; ++Index)
	{
		TestTrue(TEXT("중간 점은 진행 방향으로만 간다"), Points[Index].X > Points[Index - 1].X);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SplinePath_RejectsLaneThatDetoursTooFar, "FestaFiesta.Crowd.SplinePath.RejectsLaneThatDetoursTooFar", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SplinePath_RejectsLaneThatDetoursTooFar::RunTest(const FString& Parameters)
{
	using namespace FFF_SplinePathTestHelpers;

	const TArray<FFF_CrowdSplineLane> Lanes = { MakeStraightLane() };

	CrowdSplinePath::FQueryParams Params;
	TArray<FVector> Points;
	int32 LaneIndex = INDEX_NONE;

	// 목적지가 코앞인데 레인을 타면 내려갔다 올라와야 한다.
	TestFalse(TEXT("우회 비율을 넘는 레인은 버린다"),
		CrowdSplinePath::TryBuildPath(Lanes, FVector(0, -100, 0), FVector(100, -100, 0), Params,
			Points, LaneIndex));

	TestEqual(TEXT("실패하면 레인 인덱스가 없다"), LaneIndex, INDEX_NONE);
	TestEqual(TEXT("실패하면 점도 비어 있다"), Points.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SplinePath_IgnoresLaneOutsideSnapRadius, "FestaFiesta.Crowd.SplinePath.IgnoresLaneOutsideSnapRadius", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SplinePath_IgnoresLaneOutsideSnapRadius::RunTest(const FString& Parameters)
{
	using namespace FFF_SplinePathTestHelpers;

	const TArray<FFF_CrowdSplineLane> Lanes = {
		MakeLane({ FVector(0, 5000, 0), FVector(3000, 5000, 0) })
	};

	CrowdSplinePath::FQueryParams Params;
	TArray<FVector> Points;
	int32 LaneIndex = INDEX_NONE;

	TestFalse(TEXT("멀리 있는 레인은 잡지 않는다"),
		CrowdSplinePath::TryBuildPath(Lanes, FVector(100, 0, 0), FVector(2900, 0, 0), Params,
			Points, LaneIndex));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SplinePath_RespectsOneWayDirection, "FestaFiesta.Crowd.SplinePath.RespectsOneWayDirection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SplinePath_RespectsOneWayDirection::RunTest(const FString& Parameters)
{
	using namespace FFF_SplinePathTestHelpers;

	CrowdSplinePath::FQueryParams Params;
	TArray<FVector> Points;
	int32 LaneIndex = INDEX_NONE;

	const TArray<FFF_CrowdSplineLane> OneWay = { MakeStraightLane(true) };

	TestFalse(TEXT("일방통행을 거슬러 올라가지 않는다"),
		CrowdSplinePath::TryBuildPath(OneWay, FVector(2900, 100, 0), FVector(100, 100, 0), Params,
			Points, LaneIndex));

	const TArray<FFF_CrowdSplineLane> TwoWay = { MakeStraightLane(false) };

	TestTrue(TEXT("양방향이면 같은 요청이 통한다"),
		CrowdSplinePath::TryBuildPath(TwoWay, FVector(2900, 100, 0), FVector(100, 100, 0), Params,
			Points, LaneIndex));

	for (int32 Index = 1; Index < Points.Num() - 1; ++Index)
	{
		TestTrue(TEXT("역방향 순서로 뒤집혀 나온다"), Points[Index].X < Points[Index - 1].X);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SplinePath_SkipsBlockedLane, "FestaFiesta.Crowd.SplinePath.SkipsBlockedLane", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SplinePath_SkipsBlockedLane::RunTest(const FString& Parameters)
{
	using namespace FFF_SplinePathTestHelpers;

	TArray<FFF_CrowdSplineLane> Lanes = { MakeStraightLane(), MakeStraightLane() };
	Lanes[0].bBlocked = true;

	CrowdSplinePath::FQueryParams Params;
	TArray<FVector> Points;
	int32 LaneIndex = INDEX_NONE;

	TestTrue(TEXT("막힌 레인 대신 다음 레인을 쓴다"),
		CrowdSplinePath::TryBuildPath(Lanes, FVector(100, 200, 0), FVector(2900, 250, 0), Params,
			Points, LaneIndex));

	TestEqual(TEXT("레인 1을 골랐다"), LaneIndex, 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SplinePath_PicksLowestIndexOnTie, "FestaFiesta.Crowd.SplinePath.PicksLowestIndexOnTie", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SplinePath_PicksLowestIndexOnTie::RunTest(const FString& Parameters)
{
	using namespace FFF_SplinePathTestHelpers;

	const TArray<FFF_CrowdSplineLane> Lanes = { MakeStraightLane(), MakeStraightLane(), MakeStraightLane() };

	CrowdSplinePath::FQueryParams Params;
	TArray<FVector> Points;
	int32 LaneIndex = INDEX_NONE;

	for (int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		TestTrue(TEXT("경로가 나온다"),
			CrowdSplinePath::TryBuildPath(Lanes, FVector(100, 200, 0), FVector(2900, 250, 0), Params,
				Points, LaneIndex));

		TestEqual(TEXT("동률이면 항상 가장 낮은 인덱스다"), LaneIndex, 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SplinePath_TakesShorterArcOnClosedLoop, "FestaFiesta.Crowd.SplinePath.TakesShorterArcOnClosedLoop", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SplinePath_TakesShorterArcOnClosedLoop::RunTest(const FString& Parameters)
{
	using namespace FFF_SplinePathTestHelpers;

	const TArray<FFF_CrowdSplineLane> Lanes = { MakeSquareLoop() };

	TestTrue(TEXT("전제: 루프로 인식한다"), Lanes[0].bClosedLoop);
	TestEqual(TEXT("전제: 둘레가 4000이다"), Lanes[0].TotalLengthCm(), 4000.f, 0.1f);

	CrowdSplinePath::FQueryParams Params;
	TArray<FVector> Points;
	int32 LaneIndex = INDEX_NONE;

	TestTrue(TEXT("루프에서 경로가 나온다"),
		CrowdSplinePath::TryBuildPath(Lanes, FVector(-100, 900, 0), FVector(-100, 100, 0), Params,
			Points, LaneIndex));

	TestEqual(TEXT("모서리를 세 개 도는 대신 같은 변을 탄다"), Points.Num(), 3);

	TestTrue(TEXT("반대 방향 요청도 짧은 호를 고른다"),
		CrowdSplinePath::TryBuildPath(Lanes, FVector(-100, 100, 0), FVector(-100, 900, 0), Params,
			Points, LaneIndex));

	TestEqual(TEXT("뒤집어도 같은 변이다"), Points.Num(), 3);

	return true;
}

// 레인 경로가 경로추종의 Points 규약을 그대로 만족하는지 잠근다.
//
// UFF_CrowdSimulationSubsystem::TryBuildLanePathForAgent는 TryBuildPath의 출력을
// FFF_CrowdPathFollower::Points에 그대로 넣는다. 두 쪽 규약이 어긋나면
// (시작점 포함 여부, 마지막 원소가 목적지인지) 개체는 조용히 엉뚱한 데서 도착 처리된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SplinePath_OutputFeedsPathFollower, "FestaFiesta.Crowd.SplinePath.OutputFeedsPathFollower", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SplinePath_OutputFeedsPathFollower::RunTest(const FString& Parameters)
{
	using namespace FFF_SplinePathTestHelpers;

	const TArray<FFF_CrowdSplineLane> Lanes = { MakeStraightLane() };

	const FVector StartCm(100, 200, 0);
	const FVector TargetCm(2900, 250, 0);

	CrowdSplinePath::FQueryParams Params;
	TArray<FVector> Points;
	int32 LaneIndex = INDEX_NONE;

	if (!TestTrue(TEXT("전제: 레인 경로가 나온다"),
		CrowdSplinePath::TryBuildPath(Lanes, StartCm, TargetCm, Params, Points, LaneIndex)))
	{
		return false;
	}

	FFF_CrowdPathFollower Follower;
	Follower.Points = Points;
	Follower.NextIndex = 0;
	Follower.bHasPath = true;
	Follower.PlannedForTarget = TargetCm;

	TestTrue(TEXT("남은 웨이포인트가 있다"), Follower.HasRemainingWaypoint());
	TestFalse(TEXT("시작점은 경로에 들어가지 않는다"), Points[0].Equals(StartCm, 1.f));
	TestTrue(TEXT("마지막 원소가 최종 목적지다"), Points.Last().Equals(TargetCm, 1.f));

	const float AcceptanceUU = 120.f;

	int32 Guard = 0;
	FVector Position = StartCm;
	while (Follower.HasRemainingWaypoint() && Guard++ < 64)
	{
		const int32 Previous = Follower.NextIndex;

		Position = Follower.Points[Follower.NextIndex];
		Follower.NextIndex = CrowdPathFollow::AdvanceToReachableWaypoint(
			Follower.Points, Follower.NextIndex, Position, AcceptanceUU);

		TestTrue(TEXT("웨이포인트에 서면 인덱스가 앞으로만 간다"), Follower.NextIndex > Previous);
	}

	TestEqual(TEXT("전부 소비하면 NextIndex가 Points.Num()이다"), Follower.NextIndex, Points.Num());
	TestTrue(TEXT("마지막으로 선 자리가 목적지다"), Position.Equals(TargetCm, 1.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
