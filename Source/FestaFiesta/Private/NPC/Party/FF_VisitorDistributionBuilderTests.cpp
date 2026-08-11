// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Party/FF_VisitorDistributionBuilder.h"
#include "DataTables/FF_GroupSizeDistributionRow.h"
#include "DataTables/FF_GroupChildCompositionRow.h"
#include "DataTables/FF_ArchetypeRow.h"
#include "Engine/DataTable.h"

namespace FFF_VisitorDistributionTestHelpers
{
	const FName Scope = TEXT("tourism_daytrip");
	const FName Scenario = TEXT("resident_neutral_v0");

	// 실제 data/unreal/visitor_group_size_distribution_v0.csv 의 tourism_daytrip 값
	UDataTable* MakeSizeTable()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FFF_GroupSizeDistributionRow::StaticStruct();
		auto Add = [Table](int32 Size, const TCHAR* Tag, float Pct, bool bCap)
		{
			FFF_GroupSizeDistributionRow Row;
			Row.ScopeId = Scope;
			Row.GroupSize = Size;
			Row.CompanionSizeTag = Tag;
			Row.SharePct = Pct;
			Row.bIsCapBucket = bCap;
			Table->AddRow(*FString::Printf(TEXT("%s_%d"), *Scope.ToString(), Size), Row);
		};
		Add(1, TEXT("solo"), 3.0715f, false);
		Add(2, TEXT("pair"), 34.9961f, false);
		Add(3, TEXT("group_3_plus"), 23.1617f, false);
		Add(4, TEXT("group_3_plus"), 27.3776f, false);
		Add(5, TEXT("group_3_plus"), 5.3590f, false);
		Add(6, TEXT("group_3_plus"), 6.0341f, true);
		return Table;
	}

	// 실제 group_child_composition_v0.csv 의 tourism_daytrip 값 (6인 그룹의 아동 6명 칸 포함 —
	// 리더 성인 불변식 때문에 빌더가 이 칸을 걸러내는지 확인하는 재료다)
	UDataTable* MakeChildTable()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FFF_GroupChildCompositionRow::StaticStruct();
		auto Add = [Table](int32 Size, int32 Children, float Pct)
		{
			FFF_GroupChildCompositionRow Row;
			Row.ScopeId = Scope;
			Row.GroupSize = Size;
			Row.ChildCount = Children;
			Row.ShareWithinGroupSizePct = Pct;
			Table->AddRow(*FString::Printf(TEXT("%s_%d_%d"), *Scope.ToString(), Size, Children), Row);
		};
		Add(1, 0, 100.f);
		Add(2, 0, 99.6171f); Add(2, 1, 0.3829f);
		Add(3, 0, 71.1904f); Add(3, 1, 28.5076f); Add(3, 2, 0.3020f);
		Add(4, 0, 79.4845f); Add(4, 1, 5.0328f);  Add(4, 2, 15.4464f); Add(4, 3, 0.0363f);
		Add(5, 0, 80.1973f); Add(5, 1, 11.6531f); Add(5, 2, 5.0990f);  Add(5, 3, 3.0506f);
		Add(6, 0, 85.6390f); Add(6, 1, 3.2400f);  Add(6, 2, 7.7600f);  Add(6, 3, 1.9000f);
		Add(6, 4, 0.5000f);  Add(6, 5, 0.3000f);  Add(6, 6, 0.6000f);
		return Table;
	}

	UDataTable* MakeVisitorArchetypeTable()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FFF_ArchetypeRow::StaticStruct();
		auto Add = [Table](const TCHAR* Id, float Resident, float Datalab)
		{
			FFF_ArchetypeRow Row;
			Row.ArchetypeId = Id;
			Row.PopulationSharePct_ResidentNeutral = Resident;
			Row.PopulationSharePct_DatalabObserved = Datalab;
			Table->AddRow(Id, Row);
		};
		Add(TEXT("life_stage_teen"), 4.1503f, 2.0401f);
		Add(TEXT("life_stage_20s"), 9.9487f, 11.7805f);
		Add(TEXT("life_stage_30_40s"), 24.0140f, 28.6165f);
		Add(TEXT("life_stage_50s"), 17.0423f, 19.1366f);
		Add(TEXT("life_stage_60plus"), 36.9254f, 34.5336f);
		return Table;
	}

	FFF_VisitorDistribution BuildDefault(int32 Count, int32 Seed)
	{
		return FFF_VisitorDistributionBuilder::Build(
			MakeSizeTable(), MakeChildTable(), MakeVisitorArchetypeTable(), Scope, Scenario, Count, Seed);
	}
}

// 1. 순수 함수 — 같은 입력은 항상 같은 결과를 낸다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_VisitorDistribution_Deterministic, "FestaFiesta.NPC.VisitorDistribution.Deterministic", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_VisitorDistribution_Deterministic::RunTest(const FString& Parameters)
{
	const FFF_VisitorDistribution First = FFF_VisitorDistributionTestHelpers::BuildDefault(500, 42);
	const FFF_VisitorDistribution Second = FFF_VisitorDistributionTestHelpers::BuildDefault(500, 42);

	TestEqual(TEXT("개체 수가 같다"), Second.ActualVisitorCount, First.ActualVisitorCount);
	TestEqual(TEXT("그룹 수가 같다"), Second.PlannedGroupCount, First.PlannedGroupCount);
	TestEqual(TEXT("아동 수가 같다"), Second.DependentChildCount, First.DependentChildCount);
	if (First.Visitors.Num() == Second.Visitors.Num())
	{
		bool bAllSame = true;
		for (int32 i = 0; i < First.Visitors.Num(); ++i)
		{
			bAllSame &= First.Visitors[i].ArchetypeId == Second.Visitors[i].ArchetypeId
				&& First.Visitors[i].TargetGroupSize == Second.Visitors[i].TargetGroupSize
				&& First.Visitors[i].TargetGroupChildCount == Second.Visitors[i].TargetGroupChildCount
				&& First.Visitors[i].bIsDependentChildSlot == Second.Visitors[i].bIsDependentChildSlot;
		}
		TestTrue(TEXT("모든 개체의 태그가 순서까지 동일하다"), bAllSame);
	}

	// Seed를 바꾸면 구성이 달라진다.
	//
	// !!! PlannedGroupCount 하나만 비교하면 안 된다 !!! (2026-08-10 수정)
	// 예전엔 TestNotEqual(DifferentSeed.PlannedGroupCount, First.PlannedGroupCount) 였는데,
	// 그룹 개수는 정수 하나라 서로 다른 Seed 가 우연히 같은 값을 낼 수 있다 — 실제로 그래서
	// 실패했다. 구성이 달라졌는지는 **개체 태그 자체**를 비교해야 알 수 있다.
	const FFF_VisitorDistribution DifferentSeed = FFF_VisitorDistributionTestHelpers::BuildDefault(500, 99);
	bool bAnyTagDiffers = (DifferentSeed.Visitors.Num() != First.Visitors.Num());
	for (int32 i = 0; !bAnyTagDiffers && i < First.Visitors.Num(); ++i)
	{
		bAnyTagDiffers = First.Visitors[i].ArchetypeId != DifferentSeed.Visitors[i].ArchetypeId
			|| First.Visitors[i].TargetGroupSize != DifferentSeed.Visitors[i].TargetGroupSize
			|| First.Visitors[i].TargetGroupChildCount != DifferentSeed.Visitors[i].TargetGroupChildCount
			|| First.Visitors[i].bIsDependentChildSlot != DifferentSeed.Visitors[i].bIsDependentChildSlot;
	}
	TestTrue(TEXT("Seed를 바꾸면 개체 태그 구성이 달라진다"), bAnyTagDiffers);
	return true;
}

// 2. crowdAI가 태그만 보고 묶을 수 있다 — (크기, 아동수, 아키타입) 버킷이 정확히 나누어떨어진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_VisitorDistribution_BucketsAreGroupable, "FestaFiesta.NPC.VisitorDistribution.BucketsAreGroupable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_VisitorDistribution_BucketsAreGroupable::RunTest(const FString& Parameters)
{
	const FFF_VisitorDistribution Dist = FFF_VisitorDistributionTestHelpers::BuildDefault(2000, 7);
	TestTrue(TEXT("개체가 생성됐다"), Dist.Visitors.Num() > 0);

	// GroupPlan 이 약속한 성인 수와 실제 개체 목록의 성인 수가 버킷별로 일치해야 한다.
	// 이게 맞아야 crowdAI가 태그만 보고 그룹을 만들 수 있다.
	for (const FFF_VisitorGroupPlanBucket& Bucket : Dist.GroupPlan)
	{
		const int32 ExpectedAdults = (Bucket.GroupSize - Bucket.ChildCount) * Bucket.GroupCount;
		int32 ActualAdults = 0;
		for (const FFF_TaggedVisitor& Visitor : Dist.Visitors)
		{
			if (!Visitor.bIsDependentChildSlot
				&& Visitor.TargetGroupSize == Bucket.GroupSize
				&& Visitor.TargetGroupChildCount == Bucket.ChildCount
				&& Visitor.ArchetypeId == Bucket.AdultArchetypeId)
			{
				++ActualAdults;
			}
		}
		TestEqual(*FString::Printf(TEXT("버킷(크기%d 아동%d %s)의 성인 수가 그룹 수로 나누어떨어진다"),
			Bucket.GroupSize, Bucket.ChildCount, *Bucket.AdultArchetypeId.ToString()), ActualAdults, ExpectedAdults);
	}

	// (크기, 아동수) 단위로 아동 수도 맞아야 한다 — 아동은 아키타입이 없어 이 키까지만 구분된다.
	TMap<TTuple<int32, int32>, int32> PlannedChildren;
	for (const FFF_VisitorGroupPlanBucket& Bucket : Dist.GroupPlan)
	{
		// 아동이 0명인 버킷은 아동 개체를 하나도 만들지 않으므로 비교 대상에서 뺀다 —
		// 넣으면 값 0인 키가 생겨 "종류 수" 비교가 실제 아동 버킷 수와 어긋난다.
		if (Bucket.ChildCount == 0)
		{
			continue;
		}
		PlannedChildren.FindOrAdd(TTuple<int32, int32>(Bucket.GroupSize, Bucket.ChildCount)) += Bucket.ChildCount * Bucket.GroupCount;
	}
	TMap<TTuple<int32, int32>, int32> ActualChildren;
	for (const FFF_TaggedVisitor& Visitor : Dist.Visitors)
	{
		if (Visitor.bIsDependentChildSlot)
		{
			ActualChildren.FindOrAdd(TTuple<int32, int32>(Visitor.TargetGroupSize, Visitor.TargetGroupChildCount)) += 1;
		}
	}
	TestEqual(TEXT("아동 버킷 종류 수가 계획과 같다"), ActualChildren.Num(), PlannedChildren.Num());
	for (const TPair<TTuple<int32, int32>, int32>& Pair : PlannedChildren)
	{
		const int32* Actual = ActualChildren.Find(Pair.Key);
		TestEqual(*FString::Printf(TEXT("(크기%d 아동%d) 아동 수가 계획과 같다"), Pair.Key.Get<0>(), Pair.Key.Get<1>()),
			Actual != nullptr ? *Actual : 0, Pair.Value);
	}

	int32 PlanTotal = 0;
	for (const FFF_VisitorGroupPlanBucket& Bucket : Dist.GroupPlan)
	{
		PlanTotal += Bucket.GroupSize * Bucket.GroupCount;
	}
	TestEqual(TEXT("GroupPlan 인원 합계가 개체 수와 같다"), PlanTotal, Dist.Visitors.Num());
	return true;
}

// 3. 모든 그룹에 성인이 최소 1명 있다 (리더가 될 수 있어야 한다).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_VisitorDistribution_EveryGroupHasAdult, "FestaFiesta.NPC.VisitorDistribution.EveryGroupHasAdult", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_VisitorDistribution_EveryGroupHasAdult::RunTest(const FString& Parameters)
{
	// 입력 데이터에는 "6인 그룹 전원 아동"(6,6) 칸이 0.6% 들어 있다. 빌더가 이걸 걸러야 한다.
	const FFF_VisitorDistribution Dist = FFF_VisitorDistributionTestHelpers::BuildDefault(5000, 3);
	for (const FFF_VisitorGroupPlanBucket& Bucket : Dist.GroupPlan)
	{
		TestTrue(*FString::Printf(TEXT("크기%d 그룹의 아동 수 %d 는 크기보다 작다"), Bucket.GroupSize, Bucket.ChildCount),
			Bucket.ChildCount < Bucket.GroupSize);
	}
	// solo 는 아동일 수 없다 (혼자 온 만 15세 미만은 이 모델의 대상이 아니다)
	for (const FFF_TaggedVisitor& Visitor : Dist.Visitors)
	{
		if (Visitor.TargetGroupSize == 1)
		{
			TestFalse(TEXT("solo 개체는 아동 슬롯이 아니다"), Visitor.bIsDependentChildSlot);
		}
	}
	return true;
}

// 4. 태그 값이 규약대로다 — CompanionSizeTag 매핑, 아동의 ArchetypeId 는 NAME_None.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_VisitorDistribution_TagContract, "FestaFiesta.NPC.VisitorDistribution.TagContract", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_VisitorDistribution_TagContract::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("1명은 solo"), FFF_VisitorDistributionBuilder::CompanionSizeTagForGroupSize(1), FName(TEXT("solo")));
	TestEqual(TEXT("2명은 pair"), FFF_VisitorDistributionBuilder::CompanionSizeTagForGroupSize(2), FName(TEXT("pair")));
	TestEqual(TEXT("3명은 group_3_plus"), FFF_VisitorDistributionBuilder::CompanionSizeTagForGroupSize(3), FName(TEXT("group_3_plus")));
	TestEqual(TEXT("6명도 group_3_plus"), FFF_VisitorDistributionBuilder::CompanionSizeTagForGroupSize(6), FName(TEXT("group_3_plus")));

	const FFF_VisitorDistribution Dist = FFF_VisitorDistributionTestHelpers::BuildDefault(1000, 11);
	for (const FFF_TaggedVisitor& Visitor : Dist.Visitors)
	{
		TestEqual(TEXT("CompanionSizeTag 가 TargetGroupSize 와 일치한다"),
			Visitor.CompanionSizeTag, FFF_VisitorDistributionBuilder::CompanionSizeTagForGroupSize(Visitor.TargetGroupSize));
		TestTrue(TEXT("TargetGroupSize 가 1~6 범위다"), Visitor.TargetGroupSize >= 1 && Visitor.TargetGroupSize <= 6);
		if (Visitor.bIsDependentChildSlot)
		{
			TestTrue(TEXT("아동 슬롯의 ArchetypeId 는 NAME_None 이다"), Visitor.ArchetypeId.IsNone());
		}
		else
		{
			TestFalse(TEXT("성인의 ArchetypeId 는 비어 있지 않다"), Visitor.ArchetypeId.IsNone());
		}
	}
	return true;
}

// 5. 그룹을 통째로만 만든다 — 요청 인원을 넘지 않고, 모자란 양은 그룹 크기 상한보다 작다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_VisitorDistribution_WholeGroupsOnly, "FestaFiesta.NPC.VisitorDistribution.WholeGroupsOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_VisitorDistribution_WholeGroupsOnly::RunTest(const FString& Parameters)
{
	for (const int32 Requested : { 1, 7, 100, 951, 20000 })
	{
		const FFF_VisitorDistribution Dist = FFF_VisitorDistributionTestHelpers::BuildDefault(Requested, 5);
		TestTrue(*FString::Printf(TEXT("요청 %d명을 넘지 않는다"), Requested), Dist.ActualVisitorCount <= Requested);
		TestTrue(*FString::Printf(TEXT("요청 %d명에서 부족분이 6명 미만이다"), Requested), Requested - Dist.ActualVisitorCount < 6);
		TestEqual(*FString::Printf(TEXT("요청 %d명: 개체 배열 길이가 ActualVisitorCount 와 같다"), Requested),
			Dist.Visitors.Num(), Dist.ActualVisitorCount);
	}
	// 요청 0명이면 빈 결과
	const FFF_VisitorDistribution Empty = FFF_VisitorDistributionTestHelpers::BuildDefault(0, 5);
	TestEqual(TEXT("요청 0명이면 개체가 없다"), Empty.Visitors.Num(), 0);
	return true;
}

// 6. 아동 비율은 그룹 구성에서 나오는 파생값이다 (2026-08-06 결정).
//    실측 6.4506% 근처여야 하지만, 리더 성인 불변식 때문에 (크기==아동수) 칸이 빠져 조금 낮아진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_VisitorDistribution_ChildShareMatchesSource, "FestaFiesta.NPC.VisitorDistribution.ChildShareMatchesSource", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_VisitorDistribution_ChildShareMatchesSource::RunTest(const FString& Parameters)
{
	// 표본 오차를 줄이려고 크게 뽑는다. 여러 Seed 로 평균을 본다.
	float TotalPct = 0.f;
	int32 Runs = 0;
	for (const int32 Seed : { 1, 2, 3, 4, 5 })
	{
		const FFF_VisitorDistribution Dist = FFF_VisitorDistributionTestHelpers::BuildDefault(60000, Seed);
		TotalPct += Dist.DependentChildSharePct;
		++Runs;
	}
	const float MeanPct = TotalPct / Runs;
	AddInfo(FString::Printf(TEXT("아동 비율 평균 %.4f%% (원자료 실측 6.4506%%)"), MeanPct));
	// 원자료 실측(6.4506%)에서 크게 벗어나면 샘플링이나 재정규화가 잘못된 것이다.
	TestTrue(*FString::Printf(TEXT("아동 비율 %.4f%% 가 4~8%% 범위에 있다"), MeanPct), MeanPct > 4.f && MeanPct < 8.f);

	// 계산된 비율이 실제 개수와 일치하는지도 확인
	const FFF_VisitorDistribution One = FFF_VisitorDistributionTestHelpers::BuildDefault(1000, 1);
	const float Expected = (static_cast<float>(One.DependentChildCount) / static_cast<float>(One.ActualVisitorCount)) * 100.f;
	TestNearlyEqual(TEXT("DependentChildSharePct 가 실제 개수와 맞는다"), One.DependentChildSharePct, Expected, 0.0001f);
	return true;
}

// 7. 입력 테이블이 없거나 ScopeId/ArchetypeScenarioId 가 안 맞으면 크래시 없이 빈 결과를 낸다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_VisitorDistribution_MissingInputIsSafe, "FestaFiesta.NPC.VisitorDistribution.MissingInputIsSafe", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_VisitorDistribution_MissingInputIsSafe::RunTest(const FString& Parameters)
{
	using namespace FFF_VisitorDistributionTestHelpers;

	const FFF_VisitorDistribution NoTables = FFF_VisitorDistributionBuilder::Build(
		nullptr, nullptr, nullptr, Scope, Scenario, 100, 1);
	TestEqual(TEXT("테이블이 전부 없으면 개체가 없다"), NoTables.Visitors.Num(), 0);

	const FFF_VisitorDistribution WrongScope = FFF_VisitorDistributionBuilder::Build(
		MakeSizeTable(), MakeChildTable(), MakeVisitorArchetypeTable(), TEXT("does_not_exist"), Scenario, 100, 1);
	TestEqual(TEXT("없는 ScopeId 면 개체가 없다"), WrongScope.Visitors.Num(), 0);

	// 아동 테이블만 없으면 — 그룹은 만들어지고 아동만 0명이 된다 (분포 정보가 없으니 아동을 만들 수 없다)
	const FFF_VisitorDistribution NoChildTable = FFF_VisitorDistributionBuilder::Build(
		MakeSizeTable(), nullptr, MakeVisitorArchetypeTable(), Scope, Scenario, 100, 1);
	TestTrue(TEXT("아동 테이블이 없어도 그룹은 만들어진다"), NoChildTable.Visitors.Num() > 0);
	TestEqual(TEXT("아동 테이블이 없으면 아동은 0명이다"), NoChildTable.DependentChildCount, 0);
	return true;
}

// 8. ArchetypeScenarioId 를 바꾸면 아키타입 구성이 달라진다 (인구비중 열을 실제로 갈아 쓴다).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_VisitorDistribution_ScenarioSwitchesArchetypeShares, "FestaFiesta.NPC.VisitorDistribution.ScenarioSwitchesArchetypeShares", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_VisitorDistribution_ScenarioSwitchesArchetypeShares::RunTest(const FString& Parameters)
{
	using namespace FFF_VisitorDistributionTestHelpers;

	auto CountArchetype = [](const FFF_VisitorDistribution& Dist, FName Id)
	{
		int32 Count = 0;
		for (const FFF_TaggedVisitor& Visitor : Dist.Visitors)
		{
			if (Visitor.ArchetypeId == Id) { ++Count; }
		}
		return Count;
	};

	const FFF_VisitorDistribution Resident = FFF_VisitorDistributionBuilder::Build(
		MakeSizeTable(), MakeChildTable(), MakeVisitorArchetypeTable(), Scope, TEXT("resident_neutral_v0"), 20000, 21);
	const FFF_VisitorDistribution Datalab = FFF_VisitorDistributionBuilder::Build(
		MakeSizeTable(), MakeChildTable(), MakeVisitorArchetypeTable(), Scope, TEXT("datalab_observed_v0"), 20000, 21);

	// datalab_observed_v0 은 20대 비중이 더 높다 (9.9487 → 11.7805)
	const int32 Resident20s = CountArchetype(Resident, TEXT("life_stage_20s"));
	const int32 Datalab20s = CountArchetype(Datalab, TEXT("life_stage_20s"));
	AddInfo(FString::Printf(TEXT("life_stage_20s: resident %d명 vs datalab %d명"), Resident20s, Datalab20s));
	TestTrue(TEXT("datalab 시나리오에서 20대가 더 많다"), Datalab20s > Resident20s);

	// 5종 전부 등장해야 한다
	for (const TCHAR* Id : { TEXT("life_stage_teen"), TEXT("life_stage_20s"), TEXT("life_stage_30_40s"), TEXT("life_stage_50s"), TEXT("life_stage_60plus") })
	{
		TestTrue(*FString::Printf(TEXT("%s 가 생성된다"), Id), CountArchetype(Resident, Id) > 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
