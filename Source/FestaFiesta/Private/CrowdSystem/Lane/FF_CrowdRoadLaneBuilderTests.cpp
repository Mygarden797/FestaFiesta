// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "CrowdSystem/Lane/FF_CrowdRoadLaneBuilder.h"

// 타일 좌표는 전부 격자 칸 단위로 적고 여기서 cm로 바꾼다. 도로 한 칸은 400cm다.
namespace FFF_RoadLaneTestHelpers
{
	constexpr float TileSizeCm = 400.f;

	TArray<FFF_CrowdRoadTile> MakeTiles(const TArray<FIntPoint>& Cells)
	{
		TArray<FFF_CrowdRoadTile> Tiles;
		Tiles.Reserve(Cells.Num());

		for (const FIntPoint& Cell : Cells)
		{
			FFF_CrowdRoadTile& Tile = Tiles.AddDefaulted_GetRef();
			Tile.CenterCm = FVector(Cell.X * TileSizeCm, Cell.Y * TileSizeCm, 0.f);
			Tile.HalfExtentCm = FVector2D(TileSizeCm * 0.5f, TileSizeCm * 0.5f);
			Tile.YawDegrees = 0.f;
		}

		return Tiles;
	}

	TArray<FIntPoint> MakeRow(const int32 FromX, const int32 ToX, const int32 Y)
	{
		TArray<FIntPoint> Cells;
		for (int32 X = FromX; X <= ToX; ++X)
		{
			Cells.Add(FIntPoint(X, Y));
		}
		return Cells;
	}

	TArray<FIntPoint> MakeColumn(const int32 X, const int32 FromY, const int32 ToY)
	{
		TArray<FIntPoint> Cells;
		for (int32 Y = FromY; Y <= ToY; ++Y)
		{
			Cells.Add(FIntPoint(X, Y));
		}
		return Cells;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RoadLane_StraightRoadBakesOneLane, "FestaFiesta.Crowd.RoadLane.StraightRoadBakesOneLane", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_RoadLane_StraightRoadBakesOneLane::RunTest(const FString& Parameters)
{
	using namespace FFF_RoadLaneTestHelpers;

	TArray<FFF_CrowdSplineLane> Lanes;
	const FFF_CrowdRoadLaneBuildStats Stats =
		CrowdRoadLanes::BuildLanes(MakeTiles(MakeRow(0, 5, 0)), FFF_CrowdRoadLaneBuildParams(), Lanes);

	TestEqual(TEXT("직선 도로는 레인 하나다"), Lanes.Num(), 1);
	TestEqual(TEXT("양 끝 두 칸이 막다른 길이다"), Stats.JunctionCount, 2);

	if (Lanes.Num() == 1)
	{
		// 직선이므로 중간 정점은 전부 접혀 양 끝만 남는다.
		TestEqual(TEXT("직선은 점 두 개로 접힌다"), Lanes[0].Points.Num(), 2);
		TestEqual(TEXT("길이는 다섯 칸이다"), Lanes[0].TotalLengthCm(), 5.f * TileSizeCm, 1.f);
		TestFalse(TEXT("직선은 루프가 아니다"), Lanes[0].bClosedLoop);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RoadLane_CornerKeepsItsBend, "FestaFiesta.Crowd.RoadLane.CornerKeepsItsBend", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_RoadLane_CornerKeepsItsBend::RunTest(const FString& Parameters)
{
	using namespace FFF_RoadLaneTestHelpers;

	// ㄱ자. 꺾이는 칸은 degree 2라 자르지 않고 한 레인 안에 남는다.
	TArray<FIntPoint> Cells = MakeRow(0, 3, 0);
	Cells.Append(MakeColumn(3, 1, 3));

	TArray<FFF_CrowdSplineLane> Lanes;
	CrowdRoadLanes::BuildLanes(MakeTiles(Cells), FFF_CrowdRoadLaneBuildParams(), Lanes);

	TestEqual(TEXT("ㄱ자는 레인 하나다"), Lanes.Num(), 1);

	if (Lanes.Num() == 1)
	{
		TestEqual(TEXT("꺾임점이 남아 점이 셋이다"), Lanes[0].Points.Num(), 3);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RoadLane_JunctionSplitsIntoBranches, "FestaFiesta.Crowd.RoadLane.JunctionSplitsIntoBranches", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_RoadLane_JunctionSplitsIntoBranches::RunTest(const FString& Parameters)
{
	using namespace FFF_RoadLaneTestHelpers;

	// T자. 가운데 칸이 degree 3이므로 가지 세 개로 갈린다 —
	// 이 갈림이 곧 개체가 교차로에서 뽑을 선택지다.
	TArray<FIntPoint> Cells = MakeRow(-3, 3, 0);
	Cells.Append(MakeColumn(0, 1, 3));

	TArray<FFF_CrowdSplineLane> Lanes;
	const FFF_CrowdRoadLaneBuildStats Stats =
		CrowdRoadLanes::BuildLanes(MakeTiles(Cells), FFF_CrowdRoadLaneBuildParams(), Lanes);

	TestEqual(TEXT("T자는 가지 셋이다"), Lanes.Num(), 3);
	TestEqual(TEXT("교차점 하나와 막다른 길 셋을 센다"), Stats.JunctionCount, 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RoadLane_RingBakesClosedLoop, "FestaFiesta.Crowd.RoadLane.RingBakesClosedLoop", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_RoadLane_RingBakesClosedLoop::RunTest(const FString& Parameters)
{
	using namespace FFF_RoadLaneTestHelpers;

	// 갈림이 하나도 없는 고리. 전 칸이 degree 2라 교차점 순회가 건드리지 않는다.
	TArray<FIntPoint> Cells;
	for (int32 X = 0; X <= 3; ++X)
	{
		Cells.Add(FIntPoint(X, 0));
		Cells.Add(FIntPoint(X, 3));
	}
	for (int32 Y = 1; Y <= 2; ++Y)
	{
		Cells.Add(FIntPoint(0, Y));
		Cells.Add(FIntPoint(3, Y));
	}

	TArray<FFF_CrowdSplineLane> Lanes;
	const FFF_CrowdRoadLaneBuildStats Stats =
		CrowdRoadLanes::BuildLanes(MakeTiles(Cells), FFF_CrowdRoadLaneBuildParams(), Lanes);

	TestEqual(TEXT("고리는 레인 하나다"), Lanes.Num(), 1);
	TestEqual(TEXT("루프로 센다"), Stats.ClosedLoopCount, 1);
	TestEqual(TEXT("교차점이 없다"), Stats.JunctionCount, 0);

	if (Lanes.Num() == 1)
	{
		TestTrue(TEXT("닫힌 레인으로 구워진다"), Lanes[0].bClosedLoop);
		TestEqual(TEXT("둘레는 열두 칸이다"), Lanes[0].TotalLengthCm(), 12.f * TileSizeCm, 1.f);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RoadLane_DiagonalTouchIsNotConnected, "FestaFiesta.Crowd.RoadLane.DiagonalTouchIsNotConnected", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_RoadLane_DiagonalTouchIsNotConnected::RunTest(const FString& Parameters)
{
	using namespace FFF_RoadLaneTestHelpers;

	// 모서리만 닿은 두 칸으로는 실제로 지나갈 수 없다. 이어 붙이면 개체가
	// 대각으로 가로지르는 목적지를 받는다.
	TArray<FFF_CrowdSplineLane> Lanes;
	const FFF_CrowdRoadLaneBuildStats Stats = CrowdRoadLanes::BuildLanes(
		MakeTiles({ FIntPoint(0, 0), FIntPoint(1, 1) }), FFF_CrowdRoadLaneBuildParams(), Lanes);

	TestEqual(TEXT("대각 타일은 간선을 만들지 않는다"), Stats.EdgeCount, 0);
	TestEqual(TEXT("레인이 나오지 않는다"), Lanes.Num(), 0);
	TestEqual(TEXT("두 칸 모두 고립으로 센다"), Stats.IsolatedTileCount, 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RoadLane_SeparateRoadsStaySeparate, "FestaFiesta.Crowd.RoadLane.SeparateRoadsStaySeparate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_RoadLane_SeparateRoadsStaySeparate::RunTest(const FString& Parameters)
{
	using namespace FFF_RoadLaneTestHelpers;

	TArray<FIntPoint> Cells = MakeRow(0, 3, 0);
	Cells.Append(MakeRow(0, 3, 10));

	TArray<FFF_CrowdSplineLane> Lanes;
	CrowdRoadLanes::BuildLanes(MakeTiles(Cells), FFF_CrowdRoadLaneBuildParams(), Lanes);

	TestEqual(TEXT("끊긴 도로 둘은 레인 둘이다"), Lanes.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RoadLane_HeightGapBreaksConnection, "FestaFiesta.Crowd.RoadLane.HeightGapBreaksConnection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_RoadLane_HeightGapBreaksConnection::RunTest(const FString& Parameters)
{
	using namespace FFF_RoadLaneTestHelpers;

	TArray<FFF_CrowdRoadTile> Tiles = MakeTiles(MakeRow(0, 3, 0));
	// 가운데에서 단차가 진다. 이어 붙이면 개체가 절벽을 가로지르는 목적지를 받는다.
	Tiles[2].CenterCm.Z = 500.f;
	Tiles[3].CenterCm.Z = 500.f;

	TArray<FFF_CrowdSplineLane> Lanes;
	CrowdRoadLanes::BuildLanes(Tiles, FFF_CrowdRoadLaneBuildParams(), Lanes);

	TestEqual(TEXT("단차에서 끊겨 레인이 둘이다"), Lanes.Num(), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RoadLane_InputOrderDoesNotChangeResult, "FestaFiesta.Crowd.RoadLane.InputOrderDoesNotChangeResult", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_RoadLane_InputOrderDoesNotChangeResult::RunTest(const FString& Parameters)
{
	using namespace FFF_RoadLaneTestHelpers;

	// 레인 인덱스가 배회 추첨에 들어간다. 시설 순회 순서가 결과에 새어들면
	// 같은 맵인데 실행마다 다른 개체가 다른 길로 간다.
	TArray<FIntPoint> Cells = MakeRow(-3, 3, 0);
	Cells.Append(MakeColumn(0, 1, 3));

	TArray<FFF_CrowdSplineLane> Forward;
	CrowdRoadLanes::BuildLanes(MakeTiles(Cells), FFF_CrowdRoadLaneBuildParams(), Forward);

	Algo::Reverse(Cells);
	TArray<FFF_CrowdSplineLane> Reversed;
	CrowdRoadLanes::BuildLanes(MakeTiles(Cells), FFF_CrowdRoadLaneBuildParams(), Reversed);

	if (!TestEqual(TEXT("레인 수가 같다"), Reversed.Num(), Forward.Num()))
	{
		return false;
	}

	for (int32 LaneIndex = 0; LaneIndex < Forward.Num(); ++LaneIndex)
	{
		TestEqual(*FString::Printf(TEXT("레인 %d의 점 수가 같다"), LaneIndex),
			Reversed[LaneIndex].Points.Num(), Forward[LaneIndex].Points.Num());
		TestEqual(*FString::Printf(TEXT("레인 %d의 길이가 같다"), LaneIndex),
			Reversed[LaneIndex].TotalLengthCm(), Forward[LaneIndex].TotalLengthCm(), 1.f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
