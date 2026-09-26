// 역할: 조합 도감의 발견 개수별 기본 보상을 만들고 사용할 수 있는 보상만 골라낸다.
// 중복 ID·중복 목표 개수·달성할 수 없는 목표를 제외한 뒤 쉬운 목표부터 정렬한다.
#include "Building/Codex/FF_FacilityCodexSettings.h"

UFF_FacilityCodexSettings::UFF_FacilityCodexSettings()
{
	const int32 Counts[] = {10, 25, 50, 100};
	const int32 Gold[] = {500, 1000, 2000, 4000};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Counts); ++Index)
	{
		FFF_FacilityCodexCollectionReward& Reward = CollectionRewards.AddDefaulted_GetRef();
		Reward.MilestoneId = FName(*FString::Printf(TEXT("Collection_%03d"), Counts[Index]));
		Reward.RequiredDiscoveries = Counts[Index];
		Reward.RewardGold = Gold[Index];
	}
}

TArray<FFF_FacilityCodexCollectionReward> UFF_FacilityCodexSettings::GetValidCollectionRewards(const int32 TotalRecipeCount) const
{
	TArray<FFF_FacilityCodexCollectionReward> Result;
	TSet<FName> Ids;
	TSet<int32> Thresholds;
	for (const FFF_FacilityCodexCollectionReward& Reward : CollectionRewards)
	{
		if (Reward.MilestoneId.IsNone() || Reward.RequiredDiscoveries <= 0
			|| Reward.RequiredDiscoveries > TotalRecipeCount || Reward.RewardGold <= 0
			|| Ids.Contains(Reward.MilestoneId) || Thresholds.Contains(Reward.RequiredDiscoveries)) { continue; }
		Ids.Add(Reward.MilestoneId);
		Thresholds.Add(Reward.RequiredDiscoveries);
		Result.Add(Reward);
	}
	Result.Sort([](const FFF_FacilityCodexCollectionReward& A, const FFF_FacilityCodexCollectionReward& B)
	{
		return A.RequiredDiscoveries < B.RequiredDiscoveries;
	});
	return Result;
}
