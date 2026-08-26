// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Party/FF_VisitorDistributionBuilder.h"
#include "NPC/IndividualVariation/FF_DeterministicRandom.h"
#include "DataTables/FF_GroupSizeDistributionRow.h"
#include "DataTables/FF_GroupChildCompositionRow.h"
#include "DataTables/FF_ArchetypeRow.h"
#include "Engine/DataTable.h"

namespace
{
	// 가중 이산분포 한 칸. Weight 는 % 든 비율이든 상관없다 — 합으로 정규화한다.
	struct FWeightedChoice
	{
		int32 Value = 0;
		FName Name;
		float Weight = 0.f;
	};

	/**
	 * 균등난수 하나로 가중 이산분포에서 한 칸을 고른다.
	 * 누적합 방식이라 목록 순서가 결과를 바꾼다 — 호출부가 순서를 안정적으로 유지해야 한다.
	 */
	int32 PickIndexByWeight(const TArray<FWeightedChoice>& Choices, float UniformFraction01)
	{
		float TotalWeight = 0.f;
		for (const FWeightedChoice& Choice : Choices)
		{
			TotalWeight += FMath::Max(0.f, Choice.Weight);
		}
		if (Choices.Num() == 0 || TotalWeight <= 0.f)
		{
			return INDEX_NONE;
		}

		const float Target = FMath::Clamp(UniformFraction01, 0.f, 1.f) * TotalWeight;
		float Accumulated = 0.f;
		for (int32 Index = 0; Index < Choices.Num(); ++Index)
		{
			Accumulated += FMath::Max(0.f, Choices[Index].Weight);
			if (Target < Accumulated)
			{
				return Index;
			}
		}
		// 부동소수 반올림으로 Target 이 합계에 딱 걸리는 경우만 여기 온다
		return Choices.Num() - 1;
	}

	float UniformFor(int32 GlobalSimulationSeed, int32 GroupOrdinal, FName TraitKey)
	{
		// MakeTraitSeed 는 의도적으로 도메인 무관하게 설계돼 있어(individual_variation_spec.md 2절)
		// StableAgentId 자리에 그룹 순번을 넣어 그대로 재사용한다. RandomPurpose 는 Generation —
		// 이 값들은 생성 시 한 번 정해지고 유지되는 성격이다.
		const uint32 Seed = FFF_DeterministicRandom::MakeTraitSeed(
			GlobalSimulationSeed, GroupOrdinal, TraitKey, 0, TEXT("Generation"));
		return FFF_DeterministicRandom::MakeUniformFraction(Seed);
	}

	TArray<FWeightedChoice> ReadGroupSizeChoices(const UDataTable* Table, FName ScopeId)
	{
		TArray<FWeightedChoice> Choices;
		if (Table == nullptr)
		{
			return Choices;
		}
		Table->ForeachRow<FFF_GroupSizeDistributionRow>(TEXT("FFF_VisitorDistributionBuilder"),
			[&Choices, ScopeId](const FName&, const FFF_GroupSizeDistributionRow& Row)
			{
				if (Row.ScopeId == ScopeId && Row.GroupSize >= 1 && Row.SharePct > 0.f)
				{
					Choices.Add({ Row.GroupSize, NAME_None, Row.SharePct });
				}
			});
		// ForeachRow 순회 순서가 DataTable 내부 맵 순서라 실행마다 달라질 수 있다 —
		// 결정성을 위해 반드시 정렬한다(PickIndexByWeight 가 순서에 의존한다).
		Choices.Sort([](const FWeightedChoice& A, const FWeightedChoice& B) { return A.Value < B.Value; });
		return Choices;
	}

	/**
	 * 그룹 크기별 아동 수 분포. **아동 수가 그룹 크기와 같은 칸은 제외하고 재정규화한다** —
	 * 리더는 반드시 성인이어야 하므로(group_composition_rules_v0 의 leader_must_be_adult)
	 * 전원이 아동인 그룹은 만들 수 없다. 사후에 clamp 하면 경계값(크기-1)에 확률이 몰리는
	 * 부작용이 있어서, 뽑기 전에 후보에서 빼는 방식을 쓴다.
	 */
	TMap<int32, TArray<FWeightedChoice>> ReadChildChoicesBySize(const UDataTable* Table, FName ScopeId, float& OutExcludedWeightPct)
	{
		TMap<int32, TArray<FWeightedChoice>> BySize;
		OutExcludedWeightPct = 0.f;
		if (Table == nullptr)
		{
			return BySize;
		}
		float ExcludedWeight = 0.f;
		float TotalWeight = 0.f;
		Table->ForeachRow<FFF_GroupChildCompositionRow>(TEXT("FFF_VisitorDistributionBuilder"),
			[&BySize, &ExcludedWeight, &TotalWeight, ScopeId](const FName&, const FFF_GroupChildCompositionRow& Row)
			{
				if (Row.ScopeId != ScopeId || Row.GroupSize < 1 || Row.ShareWithinGroupSizePct <= 0.f)
				{
					return;
				}
				TotalWeight += Row.ShareWithinGroupSizePct;
				if (Row.ChildCount >= Row.GroupSize)
				{
					ExcludedWeight += Row.ShareWithinGroupSizePct;
					return;
				}
				BySize.FindOrAdd(Row.GroupSize).Add({ Row.ChildCount, NAME_None, Row.ShareWithinGroupSizePct });
			});
		for (TPair<int32, TArray<FWeightedChoice>>& Pair : BySize)
		{
			Pair.Value.Sort([](const FWeightedChoice& A, const FWeightedChoice& B) { return A.Value < B.Value; });
		}
		OutExcludedWeightPct = TotalWeight > 0.f ? (ExcludedWeight / TotalWeight) * 100.f : 0.f;
		return BySize;
	}

	TArray<FWeightedChoice> ReadArchetypeChoices(const UDataTable* Table, FName ArchetypeScenarioId)
	{
		TArray<FWeightedChoice> Choices;
		if (Table == nullptr)
		{
			return Choices;
		}
		const bool bUseDatalab = (ArchetypeScenarioId == FName(TEXT("datalab_observed_v0")));
		Table->ForeachRow<FFF_ArchetypeRow>(TEXT("FFF_VisitorDistributionBuilder"),
			[&Choices, bUseDatalab](const FName&, const FFF_ArchetypeRow& Row)
			{
				const float Share = bUseDatalab ? Row.PopulationSharePct_DatalabObserved : Row.PopulationSharePct_ResidentNeutral;
				if (!Row.ArchetypeId.IsNone() && Share > 0.f)
				{
					Choices.Add({ 0, Row.ArchetypeId, Share });
				}
			});
		// 아키타입 인구비중은 0~14세를 이미 뺀 성인 몫이라 합계가 100 이 아니다 —
		// PickIndexByWeight 가 합으로 정규화하므로 그대로 넘겨도 된다.
		Choices.Sort([](const FWeightedChoice& A, const FWeightedChoice& B) { return A.Name.LexicalLess(B.Name); });
		return Choices;
	}
}

FName FFF_VisitorDistributionBuilder::CompanionSizeTagForGroupSize(int32 GroupSize)
{
	if (GroupSize <= 1)
	{
		return TEXT("solo");
	}
	if (GroupSize == 2)
	{
		return TEXT("pair");
	}
	return TEXT("group_3_plus");
}

FFF_VisitorDistribution FFF_VisitorDistributionBuilder::Build(
	const UDataTable* GroupSizeTable,
	const UDataTable* ChildCompositionTable,
	const UDataTable* ArchetypeTable,
	FName ScopeId,
	FName ArchetypeScenarioId,
	int32 RequestedVisitorCount,
	int32 GlobalSimulationSeed,
	const TArray<FFF_ReservedPartyRequest>& ReservedParties)
{
	FFF_VisitorDistribution Result;
	Result.RequestedVisitorCount = FMath::Max(0, RequestedVisitorCount);

	// (크기, 아동수, 아키타입) → GroupPlan 인덱스
	TMap<TTuple<int32, int32, FName>, int32> BucketIndex;

	// 그룹 하나를 개체로 펼쳐 담는다. 예약석과 일반이 같은 배출 규칙을 쓰게 한 곳에 둔다 —
	// 갈라 놓으면 아동 슬롯 순서 같은 불변식이 한쪽에서만 지켜진다.
	auto EmitGroup = [&Result, &BucketIndex](
		int32 GroupSize, int32 ChildCount, FName AdultArchetypeId, const FGuid& ReservationId)
	{
		const FName SizeTag = CompanionSizeTagForGroupSize(GroupSize);
		const int32 AdultCount = GroupSize - ChildCount;

		for (int32 Member = 0; Member < GroupSize; ++Member)
		{
			// 성인을 먼저 배출한다. crowdAI 가 선두를 리더로 삼으므로 순서가 계약이다.
			const bool bIsChild = Member >= AdultCount;
			FFF_TaggedVisitor Visitor;
			// 아동은 ArchetypeId 를 갖지 않는다 — 헤더 주석 참조
			Visitor.ArchetypeId = bIsChild ? NAME_None : AdultArchetypeId;
			Visitor.CompanionSizeTag = SizeTag;
			Visitor.TargetGroupSize = GroupSize;
			Visitor.TargetGroupChildCount = ChildCount;
			Visitor.bIsDependentChildSlot = bIsChild;
			Visitor.ReservationId = ReservationId;
			Result.Visitors.Add(Visitor);
		}
		Result.ActualVisitorCount += GroupSize;
		Result.DependentChildCount += ChildCount;
		++Result.PlannedGroupCount;

		const TTuple<int32, int32, FName> Key(GroupSize, ChildCount, AdultArchetypeId);
		if (int32* Found = BucketIndex.Find(Key))
		{
			++Result.GroupPlan[*Found].GroupCount;
		}
		else
		{
			FFF_VisitorGroupPlanBucket Bucket;
			Bucket.GroupSize = GroupSize;
			Bucket.ChildCount = ChildCount;
			Bucket.AdultArchetypeId = AdultArchetypeId;
			Bucket.GroupCount = 1;
			BucketIndex.Add(Key, Result.GroupPlan.Add(Bucket));
		}
	};

	// --- 예약석 먼저 ------------------------------------------------------
	// 일반 분포보다 앞에 둔다. 배열 위치가 곧 도착 순서라 이래야 프로그램 시작 전에 도착한다.
	// 입력 테이블을 안 쓰므로 표가 비어 있어도 예약석은 나온다.
	for (const FFF_ReservedPartyRequest& Request : ReservedParties)
	{
		// 크기 상한 6 은 그룹 분포의 상한과 같다. 넘는 주문을 잘라서 담으면 산 정원보다
		// 적게 오는 것을 아무도 모르므로, 자르지 않고 통째로 뺀 뒤 개수를 돌려준다.
		const bool bValidSize = Request.GroupSize >= 1 && Request.GroupSize <= 6;
		const bool bValidChildren = Request.ChildCount >= 0 && Request.ChildCount < Request.GroupSize;
		if (!bValidSize || !bValidChildren)
		{
			++Result.RejectedReservedCount;
			continue;
		}

		EmitGroup(Request.GroupSize, Request.ChildCount, Request.AdultArchetypeId, Request.ReservationId);
		Result.ReservedVisitorCount += Request.GroupSize;
		++Result.ReservedGroupCount;
	}

	if (Result.RequestedVisitorCount == 0)
	{
		return Result;
	}

	const TArray<FWeightedChoice> SizeChoices = ReadGroupSizeChoices(GroupSizeTable, ScopeId);
	float ExcludedChildWeightPct = 0.f;
	const TMap<int32, TArray<FWeightedChoice>> ChildChoicesBySize = ReadChildChoicesBySize(ChildCompositionTable, ScopeId, ExcludedChildWeightPct);
	const TArray<FWeightedChoice> ArchetypeChoices = ReadArchetypeChoices(ArchetypeTable, ArchetypeScenarioId);
	if (SizeChoices.Num() == 0 || ArchetypeChoices.Num() == 0)
	{
		// 입력 테이블이 비었거나 ScopeId/ArchetypeScenarioId 가 안 맞는다. 빈 결과를 돌려주고 호출부가 판단한다.
		return Result;
	}

	// 일반 몫만 세는 예산 카운터. ActualVisitorCount 는 예약석을 이미 품고 있어서
	// 그걸로 예산을 재면 예약을 산 만큼 일반 방문객이 줄어든다 — 그러면 "추가로 온다"가 아니다.
	int32 GeneralVisitorCount = 0;

	int32 GroupOrdinal = 0;
	while (true)
	{
		const int32 SizePick = PickIndexByWeight(SizeChoices, UniformFor(GlobalSimulationSeed, GroupOrdinal, TEXT("group_size")));
		if (SizePick == INDEX_NONE)
		{
			break;
		}
		const int32 GroupSize = SizeChoices[SizePick].Value;

		// 그룹은 통째로만 만든다 — 남은 자리에 다 안 들어가면 여기서 멈춘다.
		// 요청 인원을 넘기지 않으려고 이 그룹만 건너뛰고 계속 돌면 작은 그룹 쪽으로 분포가
		// 치우치므로(작은 그룹만 남은 자리에 들어감) 그렇게 하지 않는다.
		if (GeneralVisitorCount + GroupSize > Result.RequestedVisitorCount)
		{
			break;
		}

		int32 ChildCount = 0;
		if (const TArray<FWeightedChoice>* ChildChoices = ChildChoicesBySize.Find(GroupSize))
		{
			const int32 ChildPick = PickIndexByWeight(*ChildChoices, UniformFor(GlobalSimulationSeed, GroupOrdinal, TEXT("group_child_count")));
			if (ChildPick != INDEX_NONE)
			{
				ChildCount = (*ChildChoices)[ChildPick].Value;
			}
		}
		// 방어: 재정규화에서 이미 걸러지지만, 입력 데이터가 바뀌어도 리더가 성인이라는 불변식은 지킨다
		ChildCount = FMath::Clamp(ChildCount, 0, GroupSize - 1);

		const int32 ArchetypePick = PickIndexByWeight(ArchetypeChoices, UniformFor(GlobalSimulationSeed, GroupOrdinal, TEXT("group_adult_archetype")));
		const FName AdultArchetypeId = ArchetypeChoices[ArchetypePick == INDEX_NONE ? 0 : ArchetypePick].Name;

		// 일반 방문객은 예약이 없다 — 무효 Guid 가 그 표시다.
		EmitGroup(GroupSize, ChildCount, AdultArchetypeId, FGuid());
		GeneralVisitorCount += GroupSize;

		++GroupOrdinal;
	}

	// GroupPlan 순서도 결정적이어야 한다 (TMap 삽입 순서에 의존하지 않도록 정렬)
	Result.GroupPlan.Sort([](const FFF_VisitorGroupPlanBucket& A, const FFF_VisitorGroupPlanBucket& B)
	{
		if (A.GroupSize != B.GroupSize) return A.GroupSize < B.GroupSize;
		if (A.ChildCount != B.ChildCount) return A.ChildCount < B.ChildCount;
		return A.AdultArchetypeId.LexicalLess(B.AdultArchetypeId);
	});

	Result.DependentChildSharePct = Result.ActualVisitorCount > 0
		? (static_cast<float>(Result.DependentChildCount) / static_cast<float>(Result.ActualVisitorCount)) * 100.f
		: 0.f;
	return Result;
}
