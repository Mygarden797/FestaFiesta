// FF_BuildDragPlacementPathTests.cpp
// 역할: 다음 기능의 정상 동작과 오류 처리를 자동 검사한다: 드래그 경로의 격자 좌표를 빠짐없이 순서대로 생성한다.
// Pure path contracts: edge connectivity, sparse samples, resumable work and integer bounds.

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildDragPlacementPath.h"
#include "Misc/AutomationTest.h"

namespace
{
	int64 GridDistance(const FIntPoint& A, const FIntPoint& B)
	{
		return FMath::Abs(static_cast<int64>(A.X) - B.X)
			+ FMath::Abs(static_cast<int64>(A.Y) - B.Y);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildDragPathStationaryTest,
	"FestaFiesta.Building.DragPlacement.Path.StationaryAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildDragPathStationaryTest::RunTest(const FString& Parameters)
{
	FFF_BuildDragPlacementPath Path;
	const FIntPoint Start(9, -4);
	Path.Reset(Start);
	Path.SetTarget(Start);
	FIntPoint EmittedCell(100, 100);
	TestFalse(TEXT("An unchanged cursor does not repeat its starting placement"), Path.TryStep(EmittedCell));
	TestEqual(TEXT("Exhaustion leaves the output untouched"), EmittedCell, FIntPoint(100, 100));
	Path.SetTarget(FIntPoint(50, 20));
	TestTrue(TEXT("A distant sample starts work"), Path.TryStep(EmittedCell));
	Path.Reset(FIntPoint(-20, 30));
	TestFalse(TEXT("Reset discards outstanding work from the previous gesture"), Path.HasPendingCells());
	TestEqual(TEXT("Reset establishes the next gesture's origin"), Path.GetCurrentCell(), FIntPoint(-20, 30));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildDragPathConnectivityTest,
	"FestaFiesta.Building.DragPlacement.Path.CardinalConnectivity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildDragPathConnectivityTest::RunTest(const FString& Parameters)
{
	const FIntPoint Start(7, -3);
	const FIntPoint Targets[] = {
		FIntPoint(24, -3), FIntPoint(-10, -3), FIntPoint(7, 14), FIntPoint(7, -20),
		FIntPoint(24, 8), FIntPoint(-10, 8), FIntPoint(24, -14), FIntPoint(-10, -14)
	};
	for (const FIntPoint& Target : Targets)
	{
		FFF_BuildDragPlacementPath Path;
		Path.Reset(Start);
		Path.SetTarget(Target);
		FIntPoint Previous = Start;
		FIntPoint Current;
		TArray<FIntPoint, TInlineAllocator<64>> ForwardCells;
		ForwardCells.Add(Start);
		const int64 ExpectedSteps = GridDistance(Start, Target);
		for (int64 Step = 0; Step < ExpectedSteps; ++Step)
		{
			if (!TestTrue(TEXT("A sparse cursor sample yields every intervening cell"), Path.TryStep(Current)))
			{
				break;
			}
			TestEqual(TEXT("Every facility shares an edge with the preceding facility"), GridDistance(Previous, Current), int64(1));
			TestEqual(TEXT("Each step approaches the target without duplicate visits"),
				GridDistance(Current, Target), ExpectedSteps - Step - 1);
			Previous = Current;
			ForwardCells.Add(Current);
		}
		TestEqual(TEXT("All cardinal directions and quadrants reach the exact target"), Path.GetCurrentCell(), Target);
		TestFalse(TEXT("The segment stops after its final tile"), Path.TryStep(Current));

		Path.Reset(Target);
		Path.SetTarget(Start);
		for (int32 Index = ForwardCells.Num() - 2; Index >= 0; --Index)
		{
			TestTrue(TEXT("Retracing can consume the segment in reverse"), Path.TryStep(Current));
			TestEqual(TEXT("Retracing the same line does not widen the road at corner ties"), Current, ForwardCells[Index]);
		}
	}

	FFF_BuildDragPlacementPath Diagonal;
	Diagonal.Reset(FIntPoint::ZeroValue);
	Diagonal.SetTarget(FIntPoint(2, 2));
	const FIntPoint ExpectedCornerPath[] = {
		FIntPoint(1, 0), FIntPoint(1, 1), FIntPoint(2, 1), FIntPoint(2, 2)
	};
	for (const FIntPoint& Expected : ExpectedCornerPath)
	{
		FIntPoint Current;
		TestTrue(TEXT("Diagonal corners include a connecting road tile"), Diagonal.TryStep(Current));
		TestEqual(TEXT("Positive X corner ties step along X before Y"), Current, Expected);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildDragPathBudgetResumeTest,
	"FestaFiesta.Building.DragPlacement.Path.BudgetedSparseSample",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildDragPathBudgetResumeTest::RunTest(const FString& Parameters)
{
	const FIntPoint Start(-37, 21);
	const FIntPoint Target(4096, -2033);
	FFF_BuildDragPlacementPath ContinuousPath;
	FFF_BuildDragPlacementPath BudgetedPath;
	ContinuousPath.Reset(Start);
	BudgetedPath.Reset(Start);
	ContinuousPath.SetTarget(Target);
	BudgetedPath.SetTarget(Target);
	constexpr int32 FrameBudget = 17;
	int64 EmittedCount = 0;
	const int64 ExpectedSteps = GridDistance(Start, Target);
	const int64 MaxFrames = (ExpectedSteps + FrameBudget - 1) / FrameBudget;
	for (int64 Frame = 0; Frame < MaxFrames; ++Frame)
	{
		// A held cursor can submit the same endpoint on every frame. Its path
		// must remain identical to uninterrupted rasterization across boundaries.
		BudgetedPath.SetTarget(Target);
		for (int32 Attempt = 0; Attempt < FrameBudget && BudgetedPath.HasPendingCells(); ++Attempt)
		{
			FIntPoint ContinuousCell;
			FIntPoint BudgetedCell;
			TestTrue(TEXT("The uninterrupted path still has the matching cell"), ContinuousPath.TryStep(ContinuousCell));
			TestTrue(TEXT("The bounded path resumes without repeating a cell"), BudgetedPath.TryStep(BudgetedCell));
			TestEqual(TEXT("Frame boundaries do not change the rasterized path"), BudgetedCell, ContinuousCell);
			++EmittedCount;
		}
		TestEqual(TEXT("Each frame consumes exactly its budget or the remaining work"),
			EmittedCount, FMath::Min(ExpectedSteps, (Frame + 1) * FrameBudget));
	}
	TestEqual(TEXT("The long jump fills every required tile"), EmittedCount, ExpectedSteps);
	TestEqual(TEXT("The last bounded frame reaches the cursor"), BudgetedPath.GetCurrentCell(), Target);
	TestFalse(TEXT("The uninterrupted path is exhausted at the same time"), ContinuousPath.HasPendingCells());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildDragPathRetargetAndBoundsTest,
	"FestaFiesta.Building.DragPlacement.Path.RetargetAndIntegerBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildDragPathRetargetAndBoundsTest::RunTest(const FString& Parameters)
{
	FFF_BuildDragPlacementPath Path;
	Path.Reset(FIntPoint::ZeroValue);
	Path.SetTarget(FIntPoint(20, 7));
	FIntPoint Current;
	Path.TryStep(Current);
	const FIntPoint RetargetOrigin = Path.GetCurrentCell();
	const FIntPoint NewTarget(-4, -3);
	Path.SetTarget(NewTarget);
	TestEqual(TEXT("Retargeting keeps the last processed cell"), Path.GetCurrentCell(), RetargetOrigin);
	FIntPoint Previous = RetargetOrigin;
	for (int64 Step = 0, Count = GridDistance(RetargetOrigin, NewTarget); Step < Count; ++Step)
	{
		TestTrue(TEXT("A changed cursor endpoint can be consumed incrementally"), Path.TryStep(Current));
		TestEqual(TEXT("Retargeting introduces no teleport or diagonal gap"), GridDistance(Previous, Current), int64(1));
		Previous = Current;
	}
	TestEqual(TEXT("The replacement segment reaches the new endpoint"), Path.GetCurrentCell(), NewTarget);

	// A corrupt or unusually distant sample must remain constant work per step.
	// These deltas exceed signed int32, and direct crossing-time cross products
	// would also overflow int64 after a sufficiently long walk.
	Path.Reset(FIntPoint(MIN_int32, MAX_int32));
	Path.SetTarget(FIntPoint(MAX_int32, MIN_int32));
	Previous = Path.GetCurrentCell();
	for (int32 Step = 0; Step < 64; ++Step)
	{
		TestTrue(TEXT("Extreme coordinates retain pending work without allocating a segment"), Path.TryStep(Current));
		TestEqual(TEXT("Extreme deltas still yield exactly one adjacent cell"), GridDistance(Previous, Current), int64(1));
		Previous = Current;
	}
	TestEqual(TEXT("The extreme diagonal advances both axes without overflow"),
		Path.GetCurrentCell(), FIntPoint(MIN_int32 + 32, MAX_int32 - 32));
	TestTrue(TEXT("Stopping at the budget preserves the rest of a huge jump"), Path.HasPendingCells());
	Path.SetTarget(Path.GetCurrentCell());
	TestFalse(TEXT("Retargeting to the current cell cancels unfinished work"), Path.HasPendingCells());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
