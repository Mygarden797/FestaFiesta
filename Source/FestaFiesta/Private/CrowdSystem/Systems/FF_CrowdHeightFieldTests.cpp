// Fill out your copyright notice in the Description page of Project Settings.

#include "CrowdSystem/Systems/FF_CrowdHeightField.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace FFF_HeightFieldTestHelpers
{
	// 시뮬 반경 100m 규모. 실제 SimRadiusUU 기본값과 같은 자릿수다
	constexpr double HalfSizeCm = 10000.0;
	constexpr double SpacingCm = 200.0;

	bool SlopeSampler(const FVector2D& QueryCm, double& OutHeightCm)
	{
		OutHeightCm = 0.1 * QueryCm.X;
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdHeightField_InterpolatesSlopeExactly, "FestaFiesta.Crowd.HeightField.InterpolatesSlopeExactly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdHeightField_InterpolatesSlopeExactly::RunTest(const FString& Parameters)
{
	using namespace FFF_HeightFieldTestHelpers;

	FFF_CrowdHeightField Field;
	const bool bBuilt = Field.Build(FVector2D::ZeroVector, HalfSizeCm, SpacingCm, &SlopeSampler);

	TestTrue(TEXT("경사면 격자가 지어진다"), bBuilt);
	TestTrue(TEXT("격자가 유효하다"), Field.IsValid());

	TestEqual(TEXT("꼭짓점이 양 끝을 포함한다"), Field.GetNumX(), 101);
	TestEqual(TEXT("적중 수가 꼭짓점 수와 같다"), Field.GetStats().HitCount, 101 * 101);

	const double Probes[] = { -9999.0, -3300.0, -100.0, 0.0, 100.0, 3300.0, 9999.0 };
	for (const double X : Probes)
	{
		double Height = 0.0;
		const bool bOk = Field.TryGetHeightCm(FVector2D(X, 1234.0), Height);

		TestTrue(*FString::Printf(TEXT("x=%.0f에서 높이를 얻는다"), X), bOk);
		TestEqual(*FString::Printf(TEXT("x=%.0f의 높이가 0.1*x다"), X), Height, 0.1 * X, 0.01);
	}

	double AtY1 = 0.0;
	double AtY2 = 0.0;
	Field.TryGetHeightCm(FVector2D(2500.0, -8000.0), AtY1);
	Field.TryGetHeightCm(FVector2D(2500.0, 8000.0), AtY2);
	TestEqual(TEXT("y는 높이에 영향이 없다(축 혼동 검출)"), AtY1, AtY2, 0.01);

	TestEqual(TEXT("기복이 경사면 전체 범위다"), Field.GetStats().HeightSpanCm(), 2000.0, 1.0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdHeightField_ClampsOutsideInsteadOfExtrapolating, "FestaFiesta.Crowd.HeightField.ClampsOutsideInsteadOfExtrapolating", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdHeightField_ClampsOutsideInsteadOfExtrapolating::RunTest(const FString& Parameters)
{
	using namespace FFF_HeightFieldTestHelpers;

	FFF_CrowdHeightField Field;
	Field.Build(FVector2D::ZeroVector, HalfSizeCm, SpacingCm, &SlopeSampler);

	double AtEdge = 0.0;
	double FarOutside = 0.0;
	TestTrue(TEXT("가장자리에서 높이를 얻는다"), Field.TryGetHeightCm(FVector2D(HalfSizeCm, 0.0), AtEdge));
	TestTrue(TEXT("격자 밖에서도 실패하지 않는다"),
		Field.TryGetHeightCm(FVector2D(HalfSizeCm * 10.0, 0.0), FarOutside));

	TestEqual(TEXT("격자 밖은 가장자리 값으로 고정된다"), FarOutside, AtEdge, 0.01);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdHeightField_HolesDoNotPullAgentsDown, "FestaFiesta.Crowd.HeightField.HolesDoNotPullAgentsDown", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdHeightField_HolesDoNotPullAgentsDown::RunTest(const FString& Parameters)
{
	using namespace FFF_HeightFieldTestHelpers;

	auto HalfMissing = [](const FVector2D& QueryCm, double& OutHeightCm) -> bool
	{
		if (QueryCm.X > 0.0)
		{
			return false;
		}
		OutHeightCm = 500.0;
		return true;
	};

	FFF_CrowdHeightField Field;
	const bool bBuilt = Field.Build(FVector2D::ZeroVector, HalfSizeCm, SpacingCm, HalfMissing);

	TestTrue(TEXT("절반만 적중해도 격자는 지어진다"), bBuilt);
	TestTrue(TEXT("적중이 꼭짓점 수보다 적다"),
		Field.GetStats().HitCount < Field.GetStats().SampleCount);

	double AtBoundary = 0.0;
	TestTrue(TEXT("구멍 경계에서 높이를 얻는다"),
		Field.TryGetHeightCm(FVector2D(-100.0, 0.0), AtBoundary));
	TestEqual(TEXT("실패한 꼭짓점이 높이를 끌어내리지 않는다"), AtBoundary, 500.0, 0.01);

	double InHole = 0.0;
	TestFalse(TEXT("구멍 한가운데는 실패를 돌려준다"),
		Field.TryGetHeightCm(FVector2D(5000.0, 0.0), InHole));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdHeightField_EmptyAndDegenerateAreSafe, "FestaFiesta.Crowd.HeightField.EmptyAndDegenerateAreSafe", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdHeightField_EmptyAndDegenerateAreSafe::RunTest(const FString& Parameters)
{
	using namespace FFF_HeightFieldTestHelpers;

	auto NeverHits = [](const FVector2D&, double&) -> bool { return false; };

	{
		FFF_CrowdHeightField Field;
		TestFalse(TEXT("적중이 없으면 빌드가 false다"),
			Field.Build(FVector2D::ZeroVector, HalfSizeCm, SpacingCm, NeverHits));
		TestFalse(TEXT("그 격자는 유효하지 않다"), Field.IsValid());

		double Height = 123.0;
		TestFalse(TEXT("조회도 실패한다"), Field.TryGetHeightCm(FVector2D::ZeroVector, Height));
		TestEqual(TEXT("실패 시 출력값을 건드리지 않는다"), Height, 123.0, 0.0);
	}

	{
		FFF_CrowdHeightField Field;
		TestFalse(TEXT("반너비 0은 거부한다"),
			Field.Build(FVector2D::ZeroVector, 0.0, SpacingCm, &SlopeSampler));
		TestFalse(TEXT("간격 0은 거부한다"),
			Field.Build(FVector2D::ZeroVector, HalfSizeCm, 0.0, &SlopeSampler));
		TestFalse(TEXT("간격 음수는 거부한다"),
			Field.Build(FVector2D::ZeroVector, HalfSizeCm, -100.0, &SlopeSampler));
	}

	{
		FFF_CrowdHeightField Field;
		TestTrue(TEXT("간격이 영역보다 커도 지어진다"),
			Field.Build(FVector2D::ZeroVector, 100.0, 99999.0, &SlopeSampler));
		TestEqual(TEXT("최소 2x2 꼭짓점을 갖는다"), Field.GetNumX(), 2);

		double Height = 0.0;
		TestTrue(TEXT("2x2에서도 조회가 된다"), Field.TryGetHeightCm(FVector2D::ZeroVector, Height));
		TestEqual(TEXT("2x2 보간도 선형 함수를 재현한다"), Height, 0.0, 0.01);
	}

	{
		FFF_CrowdHeightField Field;
		Field.Build(FVector2D::ZeroVector, HalfSizeCm, SpacingCm, &SlopeSampler);
		TestTrue(TEXT("빌드 후 유효"), Field.IsValid());

		Field.Reset();
		TestFalse(TEXT("Reset 후 무효"), Field.IsValid());
		TestEqual(TEXT("Reset이 통계도 비운다"), Field.GetStats().HitCount, 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdHeightField_BuildIsDeterministic, "FestaFiesta.Crowd.HeightField.BuildIsDeterministic", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdHeightField_BuildIsDeterministic::RunTest(const FString& Parameters)
{
	using namespace FFF_HeightFieldTestHelpers;

	auto Bumpy = [](const FVector2D& QueryCm, double& OutHeightCm) -> bool
	{
		OutHeightCm = 300.0 * FMath::Sin(QueryCm.X * 0.001) + 120.0 * FMath::Cos(QueryCm.Y * 0.0007);
		return true;
	};

	FFF_CrowdHeightField First;
	FFF_CrowdHeightField Second;
	First.Build(FVector2D(1500.0, -2500.0), HalfSizeCm, SpacingCm, Bumpy);
	Second.Build(FVector2D(1500.0, -2500.0), HalfSizeCm, SpacingCm, Bumpy);

	const double Probes[][2] = { {0.0, 0.0}, {-4321.0, 7654.0}, {9999.0, -9999.0}, {123.0, 456.0} };
	for (const auto& P : Probes)
	{
		double A = 0.0;
		double B = 0.0;
		First.TryGetHeightCm(FVector2D(P[0], P[1]), A);
		Second.TryGetHeightCm(FVector2D(P[0], P[1]), B);
		TestEqual(*FString::Printf(TEXT("(%.0f, %.0f)에서 비트 단위로 같다"), P[0], P[1]), A, B, 0.0);
	}

	return true;
}

// 통행 상한. 경사면(0.1 * x)이므로 상한 Z에 대응하는 x는 상한 / 0.1 이고, 판정이 네 꼭짓점의
// 최댓값 기준이라 통행 경계는 그 x보다 한 칸(SpacingCm) 앞에 선다 — 그 보수성까지 같이 못박는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdHeightField_WalkableCeilingBlocksHighGround, "FestaFiesta.Crowd.HeightField.WalkableCeilingBlocksHighGround", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdHeightField_WalkableCeilingBlocksHighGround::RunTest(const FString& Parameters)
{
	using namespace FFF_HeightFieldTestHelpers;

	FFF_CrowdHeightField Field;
	Field.Build(FVector2D::ZeroVector, HalfSizeCm, SpacingCm, &SlopeSampler);

	TestTrue(TEXT("상한을 세우기 전엔 전부 통행 가능하다"),
		Field.IsWalkable(FVector2D(9999.0, 0.0)));

	// 상한 510cm는 경사면에서 x=5100에 해당하고, 그 x는 꼭짓점 5000~5200 사이에 떨어진다.
	Field.SetWalkableCeilingCm(510.0);

	TestTrue(TEXT("낮은 쪽은 통행 가능하다"), Field.IsWalkable(FVector2D(-9999.0, 0.0)));
	TestTrue(TEXT("상한보다 한 칸 이상 낮으면 통행 가능하다"), Field.IsWalkable(FVector2D(4000.0, 0.0)));
	TestTrue(TEXT("상한을 넘는 꼭짓점이 없는 마지막 칸까지는 통행 가능하다"),
		Field.IsWalkable(FVector2D(4900.0, 0.0)));
	TestFalse(TEXT("상한을 넘는 지형은 통행 불가다"), Field.IsWalkable(FVector2D(9999.0, 0.0)));
	TestFalse(TEXT("상한 등고선이 든 칸은 지면이 상한 아래여도 통째로 막힌다"),
		Field.IsWalkable(FVector2D(5000.0, 0.0)));

	Field.ClearWalkableCeiling();
	TestTrue(TEXT("상한을 치우면 다시 통행 가능하다"), Field.IsWalkable(FVector2D(9999.0, 0.0)));

	return true;
}

// 구멍 처리는 손잡이로 갈린다. 기본이 통과인 이유는 트레이스가 놓친 지형에 없던 벽이 생기는
// 쪽이 더 위험하기 때문이고, 실제 구멍을 막고 싶은 맵은 켜서 쓴다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdHeightField_UnknownGroundFollowsKnob, "FestaFiesta.Crowd.HeightField.UnknownGroundFollowsKnob", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdHeightField_UnknownGroundFollowsKnob::RunTest(const FString& Parameters)
{
	using namespace FFF_HeightFieldTestHelpers;

	auto HalfMissing = [](const FVector2D& QueryCm, double& OutHeightCm) -> bool
	{
		if (QueryCm.X > 0.0)
		{
			return false;
		}
		OutHeightCm = 100.0;
		return true;
	};

	FFF_CrowdHeightField Field;
	Field.Build(FVector2D::ZeroVector, HalfSizeCm, SpacingCm, HalfMissing);
	Field.SetWalkableCeilingCm(500.0);

	TestTrue(TEXT("기본값에서는 구멍도 통행 가능하다"), Field.IsWalkable(FVector2D(5000.0, 0.0)));

	Field.SetBlockUnknownGround(true);
	TestFalse(TEXT("켜면 구멍이 통행 불가가 된다"), Field.IsWalkable(FVector2D(5000.0, 0.0)));
	TestTrue(TEXT("켜도 지면이 있는 쪽은 통행 가능하다"), Field.IsWalkable(FVector2D(-5000.0, 0.0)));

	return true;
}

// 격자가 없는 맵에서 상한이 전원을 얼려버리면 안 된다 — 손잡이만 켜고 bEnableHeightField를
// 끈 조합이 실제로 나온다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdHeightField_WalkableCeilingIsInertWithoutGrid, "FestaFiesta.Crowd.HeightField.WalkableCeilingIsInertWithoutGrid", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdHeightField_WalkableCeilingIsInertWithoutGrid::RunTest(const FString& Parameters)
{
	FFF_CrowdHeightField Field;
	Field.SetWalkableCeilingCm(0.0);
	Field.SetBlockUnknownGround(true);

	TestFalse(TEXT("격자가 없다"), Field.IsValid());
	TestTrue(TEXT("격자가 없으면 통행을 막지 않는다"), Field.IsWalkable(FVector2D::ZeroVector));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
