// Fill out your copyright notice in the Description page of Project Settings.

#include "Incident/Effects/FF_IncidentEffect_Reward.h"

#define LOCTEXT_NAMESPACE "FestaFiestaIncidentReward"

void UFF_IncidentEffect_Reward::Apply(const FFF_IncidentContext& Context)
{
	UE_LOG(LogTemp, Log, TEXT("[IncidentEffect_Reward] %s H=%d/%d Option=%d RewardGold=%d (지급 입구 미정)"),
		*Context.IncidentTag.ToString(),
		Context.Handle.Index, Context.Handle.Serial,
		Context.OptionIndex,
		RewardGold);
}

FText UFF_IncidentEffect_Reward::GetDescription() const
{
	if (RewardGold <= 0)
	{
		return FText::GetEmpty();
	}
	return FText::Format(LOCTEXT("RewardGold", "{0} G"), FText::AsNumber(RewardGold));
}

#undef LOCTEXT_NAMESPACE
