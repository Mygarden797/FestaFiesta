#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "NPC/Integration/FF_WalkDistanceMatrix.h"

// 이 절의 테스트는 NavMesh를 굽지 않는다. 경로 길이 제공자를 주입받는 설계라
// 합성 지형("강이 있다", "다리가 하나다")을 함수로 표현할 수 있다 — 그게 주입식의 목적이다.

namespace FFF_WalkDistanceTestHelpers
{
	using FAnchor = FFF_WalkDistanceMatrix::FZoneAnchor;

	FAnchor MakeAnchor(const TCHAR* ZoneId, float X, float Y, const TArray<FName>& Neighbours = {})
	{
		FAnchor Anchor;
		Anchor.ZoneId = FName(ZoneId);
		Anchor.WorldLocationCm = FVector(X, Y, 0.f);
		Anchor.NeighborZoneIds = Neighbours;
		return Anchor;
	}

	// 직선거리를 그대로 돌려주는 지형. "행렬이 직선거리와 같아지는" 기준선이다.
	bool StraightLine(const FVector& From, const FVector& To, float& OutLengthCm)
	{
		OutLengthCm = static_cast<float>(FVector::Dist(From, To));
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_WalkDistance_BuildIsDeterministic, "FestaFiesta.Npc.WalkDistance.BuildIsDeterministic", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_WalkDistance_BuildIsDeterministic::RunTest(const FString& Parameters)
{
	using namespace FFF_WalkDistanceTestHelpers;

	// 같은 존 집합을 등록 순서만 바꿔 두 번 짓는다.
	// 결과가 다르면 인덱스가 등록 순서에 의존하는 것이고, 그러면 실행마다 거리가 달라져
	// 결정 자체가 갈린다(같은 시드로 두 번 돌려도 다른 결과가 나온다).
	const TArray<FAnchor> Forward = {
		MakeAnchor(TEXT("ZoneA"), 0.f, 0.f),
		MakeAnchor(TEXT("ZoneB"), 1000.f, 0.f),
		MakeAnchor(TEXT("ZoneC"), 0.f, 2000.f),
	};

	const TArray<FAnchor> Shuffled = {
		MakeAnchor(TEXT("ZoneC"), 0.f, 2000.f),
		MakeAnchor(TEXT("ZoneA"), 0.f, 0.f),
		MakeAnchor(TEXT("ZoneB"), 1000.f, 0.f),
	};

	FFF_WalkDistanceMatrix First;
	FFF_WalkDistanceMatrix Second;
	First.Build(Forward, &StraightLine);
	Second.Build(Shuffled, &StraightLine);

	TestEqual(TEXT("존 수가 같다"), First.GetZoneCount(), Second.GetZoneCount());

	const TArray<FName> Zones = { TEXT("ZoneA"), TEXT("ZoneB"), TEXT("ZoneC") };
	for (const FName& From : Zones)
	{
		for (const FName& To : Zones)
		{
			float A = -1.f;
			float B = -1.f;
			const bool bOkA = First.TryGetWalkDistanceCm(From, To, A);
			const bool bOkB = Second.TryGetWalkDistanceCm(From, To, B);

			TestEqual(TEXT("같은 쌍의 조회 성공 여부가 같다"), bOkA, bOkB);
			// 오차 허용 없이 같아야 한다. 등록 순서가 섞이면 여기서 걸린다.
			TestEqual(*FString::Printf(TEXT("%s->%s 거리가 등록 순서와 무관하다"),
				*From.ToString(), *To.ToString()), A, B, 0.f);
		}
	}

	// 자기 자신은 0이다. 여기가 0이 아니면 같은 존 후보가 이유 없이 멀어진다.
	float Self = -1.f;
	TestTrue(TEXT("자기 존 조회는 성공한다"), First.TryGetWalkDistanceCm(TEXT("ZoneA"), TEXT("ZoneA"), Self));
	TestEqual(TEXT("자기 존까지는 0이다"), Self, 0.f, 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_WalkDistance_DetourIsLongerThanStraightLine, "FestaFiesta.Npc.WalkDistance.DetourIsLongerThanStraightLine", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_WalkDistance_DetourIsLongerThanStraightLine::RunTest(const FString& Parameters)
{
	using namespace FFF_WalkDistanceTestHelpers;

	// ── 합성 지형 ──
	// y = 1000에 강이 흐르고 다리는 x = 5000 한 곳뿐이다.
	// 강을 건너는 경로는 다리를 경유하므로 직선거리보다 훨씬 길다.
	//
	// 이게 행렬을 도입하는 유일한 이유다 — 강 건너편 부스가 직선으로는 "가깝다"고 판정되어
	// 선택되고, 이동은 크게 우회하는 상태를 없애는 것.
	const FVector BridgeCm(5000.f, 1000.f, 0.f);

	auto RiverTerrain = [&BridgeCm](const FVector& From, const FVector& To, float& OutLengthCm) -> bool
	{
		const bool bCrossesRiver = (From.Y < 1000.f) != (To.Y < 1000.f);
		if (!bCrossesRiver)
		{
			OutLengthCm = static_cast<float>(FVector::Dist(From, To));
			return true;
		}

		OutLengthCm = static_cast<float>(FVector::Dist(From, BridgeCm) + FVector::Dist(BridgeCm, To));
		return true;
	};

	const TArray<FAnchor> Anchors = {
		MakeAnchor(TEXT("South"), 0.f, 0.f),
		MakeAnchor(TEXT("North"), 0.f, 2000.f),		// 강 건너편. 직선으로는 2000cm
		MakeAnchor(TEXT("SouthEast"), 3000.f, 0.f),	// 같은 편
	};

	FFF_WalkDistanceMatrix Matrix;
	FFF_WalkDistanceMatrix::FBuildStats Stats;
	Matrix.Build(Anchors, RiverTerrain, &Stats);

	TestEqual(TEXT("존 3개가 등록됐다"), Stats.ZoneCount, 3);
	TestEqual(TEXT("질의는 N(N-1)회다"), Stats.QueryCount, 6);
	TestEqual(TEXT("실패한 쌍이 없다"), Stats.FailedPairs, 0);

	float AcrossRiver = 0.f;
	float SameSide = 0.f;
	TestTrue(TEXT("강 건너편 조회 성공"), Matrix.TryGetWalkDistanceCm(TEXT("South"), TEXT("North"), AcrossRiver));
	TestTrue(TEXT("같은 편 조회 성공"), Matrix.TryGetWalkDistanceCm(TEXT("South"), TEXT("SouthEast"), SameSide));

	const float StraightAcross = 2000.f;
	TestTrue(
		*FString::Printf(TEXT("강 건너편은 직선(%.0f)보다 멀다 (%.0f)"), StraightAcross, AcrossRiver),
		AcrossRiver > StraightAcross * 2.f);

	// 같은 편은 직선과 같아야 한다. 여기가 부풀면 우회 판정이 지형과 무관하게 걸린 것이다.
	TestEqual(TEXT("같은 편은 직선거리와 같다"), SameSide, 3000.f, 1.f);

	// 직선거리로는 North(2000)가 SouthEast(3000)보다 가깝지만, 보행거리로는 뒤집힌다.
	// 이 역전이 일어나야 "강 건너편을 고르는" 결정이 사라진다.
	TestTrue(
		*FString::Printf(TEXT("보행거리에서는 순서가 뒤집힌다 (건너편 %.0f > 같은편 %.0f)"),
			AcrossRiver, SameSide),
		AcrossRiver > SameSide);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_WalkDistance_UnreachablePairFallsBackAndIsReported, "FestaFiesta.Npc.WalkDistance.UnreachablePairFallsBackAndIsReported", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_WalkDistance_UnreachablePairFallsBackAndIsReported::RunTest(const FString& Parameters)
{
	using namespace FFF_WalkDistanceTestHelpers;

	// Island로는 경로가 없다. 그런데 존 그래프에는 South의 이웃으로 저작돼 있다 —
	// 배치와 지형이 어긋난 상태이고, 이걸 빌드 시점에 잡는 것이 이 테스트의 목적이다.
	// 조용히 두면 "후보에는 들어오는데 도달할 수 없는 시설"이 생겨
	// 화면에는 "가끔 안 오는 손님"으로만 보인다.
	auto NoPathToIsland = [](const FVector& From, const FVector& To, float& OutLengthCm) -> bool
	{
		constexpr float IslandX = 9999.f;
		if (FMath::IsNearlyEqual(static_cast<float>(From.X), IslandX)
			|| FMath::IsNearlyEqual(static_cast<float>(To.X), IslandX))
		{
			return false;
		}

		OutLengthCm = static_cast<float>(FVector::Dist(From, To));
		return true;
	};

	const TArray<FAnchor> Anchors = {
		MakeAnchor(TEXT("South"), 0.f, 0.f, { TEXT("Island"), TEXT("Missing") }),
		MakeAnchor(TEXT("Island"), 9999.f, 0.f),
	};

	FFF_WalkDistanceMatrix Matrix;
	FFF_WalkDistanceMatrix::FBuildStats Stats;
	Matrix.Build(Anchors, NoPathToIsland, &Stats);

	TestEqual(TEXT("실패한 쌍이 잡힌다"), Stats.FailedPairs, 2);	// 양방향

	float Unused = 0.f;
	TestFalse(TEXT("경로 없는 쌍은 조회가 실패한다"),
		Matrix.TryGetWalkDistanceCm(TEXT("South"), TEXT("Island"), Unused));

	// ★ 실패를 '아주 먼 거리'로 돌려주지 않는다. 그러면 후보에서 조용히 빠지고,
	//   "왜 저 부스에 아무도 안 가는가"가 어디에도 남지 않는다.
	//   호출자가 직선거리로 폴백하고 경고를 찍는 것이 계약이다.
	TestEqual(TEXT("실패 시 출력값을 건드리지 않는다"), Unused, 0.f, 0.f);

	// 존 그래프 불일치 두 건: 경로 없는 이웃(Island) + 등록되지 않은 이웃(Missing)
	TestEqual(TEXT("불일치 2건이 보고된다"), Stats.Mismatches.Num(), 2);

	const bool bReportsIsland = Stats.Mismatches.ContainsByPredicate(
		[](const FString& Line) { return Line.Contains(TEXT("Island")) && Line.Contains(TEXT("경로가 없다")); });
	const bool bReportsMissing = Stats.Mismatches.ContainsByPredicate(
		[](const FString& Line) { return Line.Contains(TEXT("Missing")) && Line.Contains(TEXT("등록되지 않았다")); });

	TestTrue(TEXT("경로 없는 이웃이 보고된다"), bReportsIsland);
	TestTrue(TEXT("등록되지 않은 이웃이 보고된다"), bReportsMissing);

	// 등록되지 않은 존은 조회도 실패해야 한다(빈 이름으로 0을 돌려주면 전부 같은 존처럼 보인다).
	TestFalse(TEXT("등록되지 않은 존은 조회 실패"),
		Matrix.TryGetWalkDistanceCm(TEXT("South"), TEXT("Missing"), Unused));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_WalkDistance_EmptyAndSingleZoneAreSafe, "FestaFiesta.Npc.WalkDistance.EmptyAndSingleZoneAreSafe", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_WalkDistance_EmptyAndSingleZoneAreSafe::RunTest(const FString& Parameters)
{
	using namespace FFF_WalkDistanceTestHelpers;

	// 존이 없는 맵(시설을 아직 안 지은 상태)에서도 죽지 않아야 한다.
	FFF_WalkDistanceMatrix Empty;
	FFF_WalkDistanceMatrix::FBuildStats EmptyStats;
	Empty.Build({}, &StraightLine, &EmptyStats);

	TestFalse(TEXT("빈 행렬은 유효하지 않다"), Empty.IsValid());
	TestEqual(TEXT("질의를 하지 않는다"), EmptyStats.QueryCount, 0);

	float Unused = 0.f;
	TestFalse(TEXT("빈 행렬 조회는 실패한다"), Empty.TryGetWalkDistanceCm(TEXT("A"), TEXT("B"), Unused));

	// 존이 하나면 행렬이 할 일이 없다 — 전부 같은 존이므로 호출자가 직선거리를 쓴다.
	FFF_WalkDistanceMatrix Single;
	Single.Build({ MakeAnchor(TEXT("Only"), 0.f, 0.f) }, &StraightLine);
	TestFalse(TEXT("존 1개는 유효하지 않다"), Single.IsValid());

	// 그래도 자기 자신 조회는 되어야 한다(0). 여기서 실패하면 단일 존 맵에서 전부 폴백을 탄다.
	float Self = -1.f;
	TestTrue(TEXT("존 1개에서도 자기 조회는 된다"), Single.TryGetWalkDistanceCm(TEXT("Only"), TEXT("Only"), Self));
	TestEqual(TEXT("자기 자신은 0"), Self, 0.f, 0.f);

	// 같은 ZoneId가 중복 등록되면 뒤엣것을 버린다. 받아 주면 행렬 인덱스가 어긋난다.
	FFF_WalkDistanceMatrix Duplicated;
	FFF_WalkDistanceMatrix::FBuildStats DupStats;
	Duplicated.Build({
		MakeAnchor(TEXT("Same"), 0.f, 0.f),
		MakeAnchor(TEXT("Same"), 5000.f, 0.f),
		MakeAnchor(TEXT("Other"), 1000.f, 0.f),
	}, &StraightLine, &DupStats);

	TestEqual(TEXT("중복 ZoneId는 하나로 접힌다"), DupStats.ZoneCount, 2);

	return true;
}

#endif
