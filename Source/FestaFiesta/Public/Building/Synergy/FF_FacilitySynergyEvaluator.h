// FF_FacilitySynergyEvaluator.h
// 역할: 주변 시설을 공간 색인으로 조회해 조합 보너스와 이동 전후 효과를 계산한다.

#pragma once

#include "CoreMinimal.h"
#include "Building/Synergy/FF_FacilitySynergyTypes.h"

/** UObject나 월드에 의존하지 않는 공유 평가기. 인덱스는 구조가 바뀔 때만 다시 만든다. */
struct FESTAFIESTA_API FFF_FacilitySynergyPoint
{
	FGuid InstanceId;
	FName DefinitionId;
	FVector WorldLocation = FVector::ZeroVector;
};

struct FESTAFIESTA_API FFF_FacilitySynergyRule
{
	FName Id;
	FFF_FacilitySynergyRow Row;
};

struct FESTAFIESTA_API FFF_FacilitySynergyMatch
{
	int32 RuleIndex = INDEX_NONE;
	FFF_FacilitySynergyPoint Partner;
	bool bHasPartner = false;
	bool bInRange = false;
	float AppliedPercent = 0.f;
};

struct FESTAFIESTA_API FFF_FacilitySynergyEvaluation
{
	float TotalPercent = 0.f;
	TArray<FFF_FacilitySynergyMatch> Matches;
};

class FESTAFIESTA_API FFF_FacilitySynergyEvaluator
{
public:
	void SetRules(TArray<FFF_FacilitySynergyRule> InRules, float InMaxPercent);
	/** Enabled/valid authored relations in both directions; no world or distance evaluation. */
	const TArray<FName>& GetRelatedFacilityDefinitionIds(FName DefinitionId) const;
	void SetFacilities(TArray<FFF_FacilitySynergyPoint> InFacilities);
	const FFF_FacilitySynergyPoint* FindFacility(FGuid Id) const;
	FFF_FacilitySynergyEvaluation Evaluate(const FFF_FacilitySynergyPoint& Target,
		const FFF_FacilitySynergyPoint* Proposed = nullptr, FGuid ExcludedId = FGuid()) const;
	void BuildPreview(FName DefinitionId, const FVector& WorldLocation, FGuid MovingId,
		FFF_FacilitySynergyPreview& Out) const;
	bool HasRules() const { return !Rules.IsEmpty(); }

private:
	struct FNode { int32 Point = INDEX_NONE; int32 Left = INDEX_NONE; int32 Right = INDEX_NONE; uint8 Axis = 0; };
	struct FTree { TArray<FNode> Nodes; int32 Root = INDEX_NONE; };
	TArray<FFF_FacilitySynergyRule> Rules;
	TArray<FFF_FacilitySynergyPoint> Points;
	TMap<FGuid, int32> PointById;
	TMap<FName, FTree> Trees;
	TMap<FName, TArray<int32>> TargetRules;
	TMap<FName, TArray<int32>> PartnerRules;
	TMap<FName, TArray<FName>> RelatedDefinitionIds;
	float MaxPercent = 0.f;
	int32 BuildTree(FTree& Tree, TArray<int32>& Indices, int32 Begin, int32 End, uint8 Axis);
	const FFF_FacilitySynergyPoint* Nearest(FName DefinitionId, const FVector& Location,
		FGuid SelfId, FGuid ExcludedId) const;
	void QueryRadius(FName DefinitionId, const FVector& Location, double RadiusCm,
		FGuid ExcludedId, TSet<int32>& Out) const;
	void NearestNode(const FTree& Tree, int32 NodeIndex, const FVector& Location,
		FGuid SelfId, FGuid ExcludedId, int32& Best, double& BestDistanceSq) const;
	void RadiusNode(const FTree& Tree, int32 NodeIndex, const FVector& Location,
		double RadiusSq, FGuid ExcludedId, TSet<int32>& Out) const;
};
