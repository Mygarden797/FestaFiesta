// Fill out your copyright notice in the Description page of Project Settings.

#include "Incident/Effects/FF_IncidentEffect_Reward.h"

#include "Economy/FF_FestivalEconomyTypes.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaIncidentReward"

class UFF_FestivalProgressionSubsystem;

void UFF_IncidentEffect_Reward::Apply(const FFF_IncidentContext& Context)
{
	ApplyWithResult(Context);
}

FFF_IncidentEffectResult UFF_IncidentEffect_Reward::ApplyWithResult(const FFF_IncidentContext& Context)
{
	FFF_IncidentEffectResult Result;
	UWorld* World = Context.World;
	if (!World)
	{
		return Result;
	}

	const UGameInstance* GameInstance = World->GetGameInstance();
	UFF_FestivalProgressionSubsystem* Progression =
	GameInstance ? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	if (Progression && RewardGold > 0)
	{
		Result.GrantedGold = Progression->AddRevenueWithReceipt(RewardGold, EFF_EarnReason::IncidentReward);
	}
	UE_LOG(LogTemp, Log, TEXT("[IncidentEffect_Reward] %s H=%d/%d Option=%d 지급=%d"),
		*Context.IncidentTag.ToString(), Context.Handle.Index, Context.Handle.Serial,
		Context.OptionIndex, Result.GrantedGold);
	return Result;
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
