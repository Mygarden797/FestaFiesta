// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/Lane/FF_CrowdLaneBake.h"

namespace FFF_LaneBakeTestHelpers
{
	constexpr float AgentRadiusCm = 34.f;

	CrowdLaneBake::FBakeParams MakeParams()
	{
		CrowdLaneBake::FBakeParams Params;
		Params.LaneHalfWidthCm = 150.f;
		Params.SimplifyToleranceCm = 150.f;
		Params.MinSegmentLengthRatio = 2.f;
		Params.CornerOverlapCm = 150.f;
		return Params;
	}

	float SegmentEnd(const CrowdLaneBake::FLaneSegment& Segment, float Sign)
	{
		const float Yaw = FMath::DegreesToRadians(Segment.YawDegrees);
		return static_cast<float>(Segment.CenterCm.X) + Sign * Segment.HalfLengthCm * FMath::Cos(Yaw);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LaneBake_SimplifyDropsCollinearKeepsCorners, "FestaFiesta.Crowd.LaneBake.SimplifyDropsCollinearKeepsCorners", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LaneBake_SimplifyDropsCollinearKeepsCorners::RunTest(const FString& Parameters)
{
	const TArray<FVector> Straight = {
		FVector(0, 0, 0), FVector(500, 0, 0), FVector(1000, 0, 0), FVector(1500, 0, 0)
	};

	TArray<FVector> Simplified;
	CrowdLaneBake::SimplifyPolyline(Straight, 150.f, Simplified);

	TestEqual(TEXT("직선 위 중간 점은 전부 사라진다"), Simplified.Num(), 2);

	const TArray<FVector> Corner = {
		FVector(0, 0, 0), FVector(500, 0, 0), FVector(1000, 0, 0), FVector(1000, 1000, 0)
	};

	CrowdLaneBake::SimplifyPolyline(Corner, 150.f, Simplified);

	TestEqual(TEXT("코너는 남는다"), Simplified.Num(), 3);
	TestTrue(TEXT("남은 가운데 점이 코너다"), Simplified[1].Equals(FVector(1000, 0, 0), 1.f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LaneBake_SimplifyDoesNotCollapseClosedLoop, "FestaFiesta.Crowd.LaneBake.SimplifyDoesNotCollapseClosedLoop", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LaneBake_SimplifyDoesNotCollapseClosedLoop::RunTest(const FString& Parameters)
{
	const TArray<FVector> Loop = {
		FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 1000, 0), FVector(0, 1000, 0), FVector(0, 0, 0)
	};

	TArray<FVector> Simplified;
	CrowdLaneBake::SimplifyPolyline(Loop, 150.f, Simplified);

	TestEqual(TEXT("네 모서리와 닫는 점이 모두 남는다"), Simplified.Num(), 5);
	TestTrue(TEXT("여전히 닫혀 있다"), Simplified[0].Equals(Simplified.Last(), 1.f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LaneBake_SegmentsStayLongerThanWide, "FestaFiesta.Crowd.LaneBake.SegmentsStayLongerThanWide", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LaneBake_SegmentsStayLongerThanWide::RunTest(const FString& Parameters)
{
	using namespace FFF_LaneBakeTestHelpers;

	// 짧고 뚱뚱한 박스는 RequiredClearWidthCm이 단축을 부풀려 길이 방향으로 번진다.
	const TArray<FVector> ShortSteps = {
		FVector(0, 0, 0), FVector(100, 0, 0), FVector(200, 0, 0), FVector(300, 0, 0),
		FVector(400, 0, 0), FVector(500, 0, 0), FVector(600, 0, 0)
	};

	TArray<CrowdLaneBake::FLaneSegment> Segments;
	CrowdLaneBake::EmitSegments(ShortSteps, MakeParams(), Segments);

	TestTrue(TEXT("세그먼트가 하나 이상 나온다"), Segments.Num() > 0);

	for (const CrowdLaneBake::FLaneSegment& Segment : Segments)
	{
		TestTrue(TEXT("모든 세그먼트가 폭보다 길다"), Segment.HalfLengthCm > Segment.HalfWidthCm);
	}

	TestTrue(TEXT("100cm 스텝 여섯 개가 박스 여섯 개가 되지는 않는다"), Segments.Num() < 6);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LaneBake_MergeNeverLeavesCenterlineUncovered, "FestaFiesta.Crowd.LaneBake.MergeNeverLeavesCenterlineUncovered", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LaneBake_MergeNeverLeavesCenterlineUncovered::RunTest(const FString& Parameters)
{
	using namespace FFF_LaneBakeTestHelpers;

	// 짧은 구간이라도 붙여 버리면 중심선이 박스 밖으로 나가는 지그재그.
	const TArray<FVector> Zigzag = {
		FVector(0, 0, 0), FVector(100, 400, 0), FVector(200, 0, 0), FVector(300, 400, 0), FVector(400, 0, 0)
	};

	TArray<CrowdLaneBake::FLaneSegment> Segments;
	CrowdLaneBake::EmitSegments(Zigzag, MakeParams(), Segments);

	TestTrue(TEXT("길이를 채우려고 지그재그를 한 박스로 뭉치지 않는다"), Segments.Num() >= 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LaneBake_CornersOverlapSoNoGapRemains, "FestaFiesta.Crowd.LaneBake.CornersOverlapSoNoGapRemains", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LaneBake_CornersOverlapSoNoGapRemains::RunTest(const FString& Parameters)
{
	using namespace FFF_LaneBakeTestHelpers;

	const TArray<FVector> Straight = { FVector(0, 0, 0), FVector(1000, 0, 0), FVector(2000, 0, 0) };

	CrowdLaneBake::FBakeParams Params = MakeParams();
	Params.MinSegmentLengthRatio = 0.f;

	TArray<CrowdLaneBake::FLaneSegment> Segments;
	CrowdLaneBake::EmitSegments(Straight, Params, Segments);

	TestEqual(TEXT("전제: 박스 둘이 나온다"), Segments.Num(), 2);

	const float FirstEnd = SegmentEnd(Segments[0], +1.f);
	const float SecondStart = SegmentEnd(Segments[1], -1.f);

	TestTrue(TEXT("앞 박스 끝이 뒤 박스 시작을 지나 겹친다"), FirstEnd > SecondStart);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LaneBake_WidthInvariantRejectsDefaultGeneralRouteWidth, "FestaFiesta.Crowd.LaneBake.WidthInvariantRejectsDefaultGeneralRouteWidth", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LaneBake_WidthInvariantRejectsDefaultGeneralRouteWidth::RunTest(const FString& Parameters)
{
	using namespace FFF_LaneBakeTestHelpers;

	FString Error;

	TestFalse(TEXT("GeneralRouteWidthCm 300은 레인 반폭 150을 못 받는다"),
		CrowdLaneBake::ValidateWidthInvariant(150.f, 300.f, AgentRadiusCm, Error));
	TestTrue(TEXT("실패 사유가 비어 있지 않다"), !Error.IsEmpty());

	TestTrue(TEXT("440으로 올리면 통과한다"),
		CrowdLaneBake::ValidateWidthInvariant(150.f, 440.f, AgentRadiusCm, Error));

	TestTrue(TEXT("경계값 428도 통과한다"),
		CrowdLaneBake::ValidateWidthInvariant(150.f, 428.f, AgentRadiusCm, Error));

	TestFalse(TEXT("경계 바로 아래 427은 막힌다"),
		CrowdLaneBake::ValidateWidthInvariant(150.f, 427.f, AgentRadiusCm, Error));

	TestTrue(TEXT("300을 유지하려면 반폭을 86까지 줄여야 한다"),
		CrowdLaneBake::ValidateWidthInvariant(86.f, 300.f, AgentRadiusCm, Error));

	TestFalse(TEXT("반폭 0은 거부한다"),
		CrowdLaneBake::ValidateWidthInvariant(0.f, 1000.f, AgentRadiusCm, Error));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LaneBake_ResampleRespectsMaxSpacing, "FestaFiesta.Crowd.LaneBake.ResampleRespectsMaxSpacing", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LaneBake_ResampleRespectsMaxSpacing::RunTest(const FString& Parameters)
{
	const TArray<FVector> Sparse = { FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 300, 0) };

	TArray<FVector> Resampled;
	CrowdLaneBake::ResamplePolyline(Sparse, 200.f, Resampled);

	TestTrue(TEXT("점이 늘어난다"), Resampled.Num() > Sparse.Num());
	TestTrue(TEXT("시작점이 보존된다"), Resampled[0].Equals(Sparse[0], 1.f));
	TestTrue(TEXT("끝점이 보존된다"), Resampled.Last().Equals(Sparse.Last(), 1.f));

	for (int32 Index = 1; Index < Resampled.Num(); ++Index)
	{
		TestTrue(TEXT("어떤 간격도 상한을 넘지 않는다"),
			FVector::Dist2D(Resampled[Index - 1], Resampled[Index]) <= 200.f + 1.f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
