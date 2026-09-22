// FF_FacilitySynergyEvaluator.cpp
// 역할: 주변 시설을 공간 색인으로 조회해 조합 보너스와 이동 전후 효과를 계산한다.

#include "Building/Synergy/FF_FacilitySynergyEvaluator.h"
#include "Building/Synergy/FF_FacilitySynergyRulePolicy.h"
#include <algorithm>

namespace
{
	double DistanceSq(const FVector& A, const FVector& B) { return FMath::Square(A.X - B.X) + FMath::Square(A.Y - B.Y); }
	bool GuidLess(const FGuid& A, const FGuid& B)
	{
		if (A.A != B.A) return A.A < B.A;
		if (A.B != B.B) return A.B < B.B;
		if (A.C != B.C) return A.C < B.C;
		return A.D < B.D;
	}
	const FFF_FacilitySynergyMatch* FindMatch(const FFF_FacilitySynergyEvaluation& Evaluation, int32 RuleIndex)
	{
		return Evaluation.Matches.FindByPredicate([RuleIndex](const FFF_FacilitySynergyMatch& Match) { return Match.RuleIndex == RuleIndex; });
	}
}

// 검증·정렬한 조합에서 수익 시설과 도움 시설을 색인화한다. 카탈로그 추천에는 월드 검색이 필요 없다.
void FFF_FacilitySynergyEvaluator::SetRules(TArray<FFF_FacilitySynergyRule> InRules, const float InMaxPercent)
{
	Rules = MoveTemp(InRules);
	FFFacilitySynergyRulePolicy::Normalize(Rules);
	MaxPercent = FMath::IsFinite(InMaxPercent) ? FMath::Clamp(InMaxPercent, 0.f, 1000.f) : 0.f;
	TargetRules.Reset();
	PartnerRules.Reset();
	RelatedDefinitionIds.Reset();
	TMap<FName, TSet<FName>> RelatedSets;
	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		TargetRules.FindOrAdd(Rules[Index].Row.TargetFacilityId).Add(Index);
		PartnerRules.FindOrAdd(Rules[Index].Row.PartnerFacilityId).Add(Index);
		RelatedSets.FindOrAdd(Rules[Index].Row.TargetFacilityId).Add(Rules[Index].Row.PartnerFacilityId);
		RelatedSets.FindOrAdd(Rules[Index].Row.PartnerFacilityId).Add(Rules[Index].Row.TargetFacilityId);
	}
	for (const auto& Pair : RelatedSets)
	{
		TArray<FName>& Related = RelatedDefinitionIds.Add(Pair.Key, Pair.Value.Array());
		Related.Sort([](FName A, FName B) { return A.LexicalLess(B); });
	}
}

const TArray<FName>& FFF_FacilitySynergyEvaluator::GetRelatedFacilityDefinitionIds(const FName DefinitionId) const
{
	static const TArray<FName> Empty;
	const TArray<FName>* Related = RelatedDefinitionIds.Find(DefinitionId);
	return Related ? *Related : Empty;
}

// 유효하고 조합에 쓰이는 시설만 공간 색인에 넣는다. 시설 변경 때 한 번 갱신하고 매 수익 계산에는 재사용한다.
void FFF_FacilitySynergyEvaluator::SetFacilities(TArray<FFF_FacilitySynergyPoint> InFacilities)
{
	Points.Reset();
	PointById.Reset();
	Trees.Reset();
	TMap<FName, TArray<int32>> ByDefinition;
	for (const FFF_FacilitySynergyPoint& Point : InFacilities)
	{
		if (!Point.InstanceId.IsValid() || Point.DefinitionId.IsNone() || Point.WorldLocation.ContainsNaN() || PointById.Contains(Point.InstanceId)) continue;
		// Definitions unused by any row do not occupy the runtime spatial index.
		if (!TargetRules.Contains(Point.DefinitionId) && !PartnerRules.Contains(Point.DefinitionId)) continue;
		const int32 Index = Points.Add(Point);
		PointById.Add(Point.InstanceId, Index);
		ByDefinition.FindOrAdd(Point.DefinitionId).Add(Index);
	}
	for (auto& Pair : ByDefinition)
	{
		FTree& Tree = Trees.Add(Pair.Key);
		Tree.Nodes.Reserve(Pair.Value.Num());
		Tree.Root = BuildTree(Tree, Pair.Value, 0, Pair.Value.Num(), 0);
	}
}

int32 FFF_FacilitySynergyEvaluator::BuildTree(FTree& Tree, TArray<int32>& Indices, const int32 Begin, const int32 End, const uint8 Axis)
{
	if (Begin == End) return INDEX_NONE;
	const int32 Middle = Begin + (End - Begin) / 2;
	std::nth_element(Indices.GetData() + Begin, Indices.GetData() + Middle, Indices.GetData() + End,
		[this, Axis](const int32 A, const int32 B)
		{
			const double AV = Points[A].WorldLocation[Axis], BV = Points[B].WorldLocation[Axis];
			return AV != BV ? AV < BV : GuidLess(Points[A].InstanceId, Points[B].InstanceId);
		});
	const int32 NodeIndex = Tree.Nodes.AddDefaulted();
	Tree.Nodes[NodeIndex].Point = Indices[Middle];
	Tree.Nodes[NodeIndex].Axis = Axis;
	const int32 Left = BuildTree(Tree, Indices, Begin, Middle, 1 - Axis);
	const int32 Right = BuildTree(Tree, Indices, Middle + 1, End, 1 - Axis);
	Tree.Nodes[NodeIndex].Left = Left;
	Tree.Nodes[NodeIndex].Right = Right;
	return NodeIndex;
}

const FFF_FacilitySynergyPoint* FFF_FacilitySynergyEvaluator::FindFacility(const FGuid Id) const
{
	const int32* Index = PointById.Find(Id);
	return Index ? &Points[*Index] : nullptr;
}

void FFF_FacilitySynergyEvaluator::NearestNode(const FTree& Tree, const int32 NodeIndex, const FVector& Location,
	const FGuid SelfId, const FGuid ExcludedId, int32& Best, double& BestDistanceSq) const
{
	if (NodeIndex == INDEX_NONE) return;
	const FNode& Node = Tree.Nodes[NodeIndex];
	const FFF_FacilitySynergyPoint& Point = Points[Node.Point];
	const double DistSq = DistanceSq(Location, Point.WorldLocation);
	if (Point.InstanceId != SelfId && Point.InstanceId != ExcludedId &&
		(DistSq < BestDistanceSq || (DistSq == BestDistanceSq && (Best == INDEX_NONE || GuidLess(Point.InstanceId, Points[Best].InstanceId)))))
	{
		Best = Node.Point;
		BestDistanceSq = DistSq;
	}
	const double Delta = Location[Node.Axis] - Point.WorldLocation[Node.Axis];
	NearestNode(Tree, Delta <= 0 ? Node.Left : Node.Right, Location, SelfId, ExcludedId, Best, BestDistanceSq);
	if (Delta * Delta <= BestDistanceSq)
		NearestNode(Tree, Delta <= 0 ? Node.Right : Node.Left, Location, SelfId, ExcludedId, Best, BestDistanceSq);
}

const FFF_FacilitySynergyPoint* FFF_FacilitySynergyEvaluator::Nearest(const FName DefinitionId,
	const FVector& Location, const FGuid SelfId, const FGuid ExcludedId) const
{
	const FTree* Tree = Trees.Find(DefinitionId);
	if (!Tree) return nullptr;
	int32 Best = INDEX_NONE;
	double BestDistanceSq = TNumericLimits<double>::Max();
	NearestNode(*Tree, Tree->Root, Location, SelfId, ExcludedId, Best, BestDistanceSq);
	return Best == INDEX_NONE ? nullptr : &Points[Best];
}

void FFF_FacilitySynergyEvaluator::RadiusNode(const FTree& Tree, const int32 NodeIndex, const FVector& Location,
	const double RadiusSq, const FGuid ExcludedId, TSet<int32>& Out) const
{
	if (NodeIndex == INDEX_NONE) return;
	const FNode& Node = Tree.Nodes[NodeIndex];
	const FFF_FacilitySynergyPoint& Point = Points[Node.Point];
	if (Point.InstanceId != ExcludedId && DistanceSq(Location, Point.WorldLocation) <= RadiusSq) Out.Add(Node.Point);
	const double Delta = Location[Node.Axis] - Point.WorldLocation[Node.Axis];
	RadiusNode(Tree, Delta <= 0 ? Node.Left : Node.Right, Location, RadiusSq, ExcludedId, Out);
	if (Delta * Delta <= RadiusSq)
		RadiusNode(Tree, Delta <= 0 ? Node.Right : Node.Left, Location, RadiusSq, ExcludedId, Out);
}

void FFF_FacilitySynergyEvaluator::QueryRadius(const FName DefinitionId, const FVector& Location,
	const double RadiusCm, const FGuid ExcludedId, TSet<int32>& Out) const
{
	if (const FTree* Tree = Trees.Find(DefinitionId)) RadiusNode(*Tree, Tree->Root, Location, RadiusCm * RadiusCm, ExcludedId, Out);
}

// 가까운 도움 시설을 찾고 같은 그룹은 가장 큰 보너스 하나만 적용한 뒤 시설당 총 상한을 제한한다.
FFF_FacilitySynergyEvaluation FFF_FacilitySynergyEvaluator::Evaluate(const FFF_FacilitySynergyPoint& Target,
	const FFF_FacilitySynergyPoint* Proposed, const FGuid ExcludedId) const
{
	FFF_FacilitySynergyEvaluation Result;
	const TArray<int32>* Applicable = TargetRules.Find(Target.DefinitionId);
	if (!Applicable || Target.WorldLocation.ContainsNaN()) return Result;
	TArray<int32> Active;
	for (const int32 RuleIndex : *Applicable)
	{
		const FFF_FacilitySynergyRow& Row = Rules[RuleIndex].Row;
		FFF_FacilitySynergyMatch& Match = Result.Matches.AddDefaulted_GetRef();
		Match.RuleIndex = RuleIndex;
		const FFF_FacilitySynergyPoint* Partner = Nearest(Row.PartnerFacilityId, Target.WorldLocation, Target.InstanceId, ExcludedId);
		// The proposed point is an overlay; its previous instance was excluded above. It cannot pair with itself.
		if (Proposed && Proposed->InstanceId != Target.InstanceId && Proposed->DefinitionId == Row.PartnerFacilityId &&
			(!Partner || DistanceSq(Target.WorldLocation, Proposed->WorldLocation) < DistanceSq(Target.WorldLocation, Partner->WorldLocation))) Partner = Proposed;
		if (Partner)
		{
			Match.Partner = *Partner;
			Match.bHasPartner = true;
			Match.bInRange = DistanceSq(Target.WorldLocation, Partner->WorldLocation) <= FMath::Square(static_cast<double>(Row.MaxDistanceMeters) * 100.);
			if (Match.bInRange) Active.Add(Result.Matches.Num() - 1);
		}
	}
	// Highest benefit wins within a group; cap is distributed in the same deterministic order.
	Active.Sort([this, &Result](const int32 A, const int32 B)
	{
		const FFF_FacilitySynergyRule& ARule = Rules[Result.Matches[A].RuleIndex];
		const FFF_FacilitySynergyRule& BRule = Rules[Result.Matches[B].RuleIndex];
		return ARule.Row.RevenueBonusPercent != BRule.Row.RevenueBonusPercent
			? ARule.Row.RevenueBonusPercent > BRule.Row.RevenueBonusPercent : ARule.Id.LexicalLess(BRule.Id);
	});
	TSet<FName> AppliedGroups;
	for (const int32 MatchIndex : Active)
	{
		FFF_FacilitySynergyMatch& Match = Result.Matches[MatchIndex];
		const FFF_FacilitySynergyRow& Row = Rules[Match.RuleIndex].Row;
		if (!Row.StackingGroup.IsNone() && AppliedGroups.Contains(Row.StackingGroup)) continue;
		if (!Row.StackingGroup.IsNone()) AppliedGroups.Add(Row.StackingGroup);
		Match.AppliedPercent = FMath::Min(Row.RevenueBonusPercent, FMath::Max(0.f, MaxPercent - Result.TotalPercent));
		Result.TotalPercent += Match.AppliedPercent;
	}
	return Result;
}

// 아직 설치하지 않은 위치를 임시 후보로 평가해 현재·변경 후 수익 차이를 계산한다. 월드의 실제 시설은 바꾸지 않는다.
void FFF_FacilitySynergyEvaluator::BuildPreview(const FName DefinitionId, const FVector& WorldLocation,
	const FGuid MovingId, FFF_FacilitySynergyPreview& Out) const
{
	Out = FFF_FacilitySynergyPreview();
	if (DefinitionId.IsNone() || WorldLocation.ContainsNaN()) return;
	const TArray<int32>* AsTarget = TargetRules.Find(DefinitionId);
	const TArray<int32>* AsPartner = PartnerRules.Find(DefinitionId);
	Out.bRulesAvailable = AsTarget || AsPartner;
	if (!Out.bRulesAvailable) return;
	// An invalid GUID denotes a prospective install and never occurs in the indexed live world.
	const FFF_FacilitySynergyPoint Candidate{MovingId, DefinitionId, WorldLocation};
	const FFF_FacilitySynergyPoint* Previous = FindFacility(MovingId);
	auto Append = [this, &Out](int32 RuleIndex, const FFF_FacilitySynergyPoint& Target,
		const FFF_FacilitySynergyEvaluation& Before, const FFF_FacilitySynergyEvaluation& After,
		const FFF_FacilitySynergyPoint* Partner, bool bCandidateIsTarget)
	{
		const FFF_FacilitySynergyRule& Rule = Rules[RuleIndex];
		FFF_FacilitySynergyPreviewEntry& Entry = Out.Entries.AddDefaulted_GetRef();
		Entry.RuleId = Rule.Id;
		// Keep text identity/localization intact. Only the presentation layer can resolve
		// current catalog names for an absent title; a rule ID is not player-facing text.
		Entry.DisplayName = Rule.Row.DisplayName;
		Entry.TargetFacilityId = Rule.Row.TargetFacilityId;
		Entry.PartnerFacilityId = Rule.Row.PartnerFacilityId;
		Entry.TargetInstanceId = Target.InstanceId;
		Entry.TargetWorldLocation = Target.WorldLocation;
		Entry.MaxDistanceMeters = Rule.Row.MaxDistanceMeters;
		Entry.ConfiguredBonusPercent = Rule.Row.RevenueBonusPercent;
		Entry.CurrentTotalBonusPercent = Before.TotalPercent;
		Entry.ProposedTotalBonusPercent = After.TotalPercent;
		Entry.bCandidateIsTarget = bCandidateIsTarget;
		Entry.bHasPartner = Partner != nullptr;
		if (const FFF_FacilitySynergyMatch* Match = FindMatch(After, RuleIndex)) Entry.AppliedBonusPercent = Match->AppliedPercent;
		if (Partner)
		{
			Entry.PartnerInstanceId = Partner->InstanceId;
			Entry.PartnerWorldLocation = Partner->WorldLocation;
			const double SquaredDistance = DistanceSq(Target.WorldLocation, Partner->WorldLocation);
			Entry.DistanceMeters = static_cast<float>(FMath::Sqrt(SquaredDistance) / 100.);
			Entry.bInRange = SquaredDistance <= FMath::Square(static_cast<double>(Rule.Row.MaxDistanceMeters) * 100.);
		}
	};
	if (AsTarget)
	{
		const FFF_FacilitySynergyEvaluation Before = Previous ? Evaluate(*Previous) : FFF_FacilitySynergyEvaluation();
		const FFF_FacilitySynergyEvaluation After = Evaluate(Candidate, &Candidate, MovingId);
		for (const int32 RuleIndex : *AsTarget)
		{
			const FFF_FacilitySynergyMatch* Match = FindMatch(After, RuleIndex);
			Append(RuleIndex, Candidate, Before, After, Match && Match->bHasPartner ? &Match->Partner : nullptr, true);
		}
	}
	if (AsPartner)
	{
		// Cache per affected beneficiary so several matching rows don't repeat all rule evaluation.
		TMap<int32, FFF_FacilitySynergyEvaluation> BeforeByTarget, AfterByTarget;
		for (const int32 RuleIndex : *AsPartner)
		{
			const FFF_FacilitySynergyRow& Row = Rules[RuleIndex].Row;
			TSet<int32> Affected;
			QueryRadius(Row.TargetFacilityId, WorldLocation, Row.MaxDistanceMeters * 100., MovingId, Affected);
			if (Previous) QueryRadius(Row.TargetFacilityId, Previous->WorldLocation, Row.MaxDistanceMeters * 100., MovingId, Affected);
			if (Affected.IsEmpty())
			{
				if (const FFF_FacilitySynergyPoint* Closest = Nearest(Row.TargetFacilityId, WorldLocation, MovingId, MovingId))
					Affected.Add(PointById.FindChecked(Closest->InstanceId));
			}
			if (Affected.IsEmpty())
			{
				// A same-type row already has its missing-counterpart entry in AsTarget.
				// Keep real reverse beneficiaries; only suppress this duplicate empty fallback.
				if (Row.TargetFacilityId == DefinitionId) continue;
				const FFF_FacilitySynergyPoint Missing{FGuid(), Row.TargetFacilityId, WorldLocation};
				Append(RuleIndex, Missing, {}, {}, nullptr, false);
				Out.Entries.Last().PartnerWorldLocation = WorldLocation;
				continue;
			}
			for (const int32 Index : Affected)
			{
				const FFF_FacilitySynergyPoint& Target = Points[Index];
				if (!BeforeByTarget.Contains(Index))
				{
					BeforeByTarget.Add(Index, Evaluate(Target));
					AfterByTarget.Add(Index, Evaluate(Target, &Candidate, MovingId));
				}
				Append(RuleIndex, Target, BeforeByTarget.FindChecked(Index), AfterByTarget.FindChecked(Index), &Candidate, false);
			}
		}
	}
	Out.Entries.Sort([](const FFF_FacilitySynergyPreviewEntry& A, const FFF_FacilitySynergyPreviewEntry& B)
	{
		const float ADelta = A.ProposedTotalBonusPercent - A.CurrentTotalBonusPercent;
		const float BDelta = B.ProposedTotalBonusPercent - B.CurrentTotalBonusPercent;
		if (FMath::Abs(ADelta) != FMath::Abs(BDelta)) return FMath::Abs(ADelta) > FMath::Abs(BDelta);
		if (ADelta != BDelta) return ADelta < BDelta; // Same-size loss before gain.
		if (A.bInRange != B.bInRange) return A.bInRange;
		if (A.bHasPartner != B.bHasPartner) return A.bHasPartner;
		if (A.DistanceMeters != B.DistanceMeters) return A.DistanceMeters < B.DistanceMeters;
		return A.RuleId != B.RuleId ? A.RuleId.LexicalLess(B.RuleId) : GuidLess(A.TargetInstanceId, B.TargetInstanceId);
	});
}
