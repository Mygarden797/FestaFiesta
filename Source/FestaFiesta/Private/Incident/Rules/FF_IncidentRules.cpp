// Copyright Epic Games, Inc. All Rights Reserved.

#include "Incident/Rules/FF_IncidentRules.h"

#include "VectorUtil.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Incident/FF_IncidentDefinition.h"
#include "Incident/FF_IncidentOption.h"
#include "Incident/Effects/FF_IncidentEffect.h"

FString FFF_IncidentRules::MakeAnchorId(const FFF_IncidentAnchor& Anchor)
{
	switch (Anchor.Kind)
	{
	case EFF_IncidentAnchorKind::WorldPoint:
		{
			int32 GX = FMath::FloorToInt(Anchor.WorldPoint.X / WorldPointDedupeGridUU);
			int32 GY = FMath::FloorToInt(Anchor.WorldPoint.Y / WorldPointDedupeGridUU);
			FString SGX = FString::FromInt(GX);
			FString SGY = FString::FromInt(GY);
			return TEXT("W") + SGX + TEXT("_") +SGY;
		}
	

	case EFF_IncidentAnchorKind::Actor:
		
		return Anchor.Actor.IsValid() ? FString::Printf(TEXT("A%s"),*Anchor.Actor->GetName()) : FString::Printf(TEXT("A_Invalid"));	


	case EFF_IncidentAnchorKind::Zone:
		return FString::Printf(TEXT("Z%s"), *Anchor.ZoneId.ToString());


	case EFF_IncidentAnchorKind::DensityCell:
		return FString::Printf(TEXT("C%d_%d"), Anchor.DensityCell.X, Anchor.DensityCell.Y);


	case EFF_IncidentAnchorKind::Screenspace:
		return TEXT("Screenspace");

	}

	return TEXT("?");
}

FName FFF_IncidentRules::MakeDedupeKey(const FGameplayTag& IncidentTag, const FFF_IncidentAnchor& Anchor)
{
	
	FString SIncidentTag = IncidentTag.ToString();
	FString AnchorId = MakeAnchorId(Anchor);
	FString DedupeString = SIncidentTag + TEXT("_") + AnchorId;
	
	return FName(*DedupeString);
}

bool FFF_IncidentRules::HasLapsed(float NowMinutes, float OccurredAtMinutes, float MinutesUntilLapse)
{
	if (MinutesUntilLapse <=0)
	{
		return false;
	}
	
	return  (NowMinutes - OccurredAtMinutes >= MinutesUntilLapse);
}

EFF_IncidentState FFF_IncidentRules::LapseStateFor(EFF_IncidentTone Tone)
{
	switch (Tone)
	{
	case EFF_IncidentTone::Problem:    
		return EFF_IncidentState::Ignored;
	case EFF_IncidentTone::Opportunity:
		return EFF_IncidentState::Missed;
	case EFF_IncidentTone::Notice:     
		return EFF_IncidentState::Expired;
	}

	return EFF_IncidentState::Expired;
}

bool FFF_IncidentRules::ShouldToastClosed(const FFF_IncidentClosedInfo& Info)
{
	switch (Info.FinalState)
	{
	case EFF_IncidentState::Resolved:
	case EFF_IncidentState::Ignored:
	case EFF_IncidentState::Missed:
		return true;
	case EFF_IncidentState::Expired:
		return Info.bWasResponding;
	case EFF_IncidentState::Active:
	case EFF_IncidentState::Acknowledged:
	case EFF_IncidentState::Taken:
		return false;
	}

	return false;
}

float FFF_IncidentRules::MergeIntensity(float ExistingPeak, float IncomingIntensity)
{
	
	return FMath::Max(ExistingPeak, IncomingIntensity);
}

bool FFF_IncidentRules::TryGetAnchorLocation(const UWorld* World, const FFF_IncidentAnchor& Anchor, FVector& OutLocation)
{
	switch (Anchor.Kind)
	{
	case EFF_IncidentAnchorKind::Actor:
		{
			const AActor* Actor = Anchor.Actor.Get();
			if (!IsValid(Actor) || Actor->IsHidden())
			{
				return false;
			}
			OutLocation = Actor->GetActorLocation();
			return !OutLocation.ContainsNaN();
		}

	case EFF_IncidentAnchorKind::WorldPoint:
	case EFF_IncidentAnchorKind::DensityCell:
		OutLocation = Anchor.WorldPoint;
		return !OutLocation.ContainsNaN();

	case EFF_IncidentAnchorKind::Zone:
		{
			const UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
			const AFFZSGameplayZone* Zone = Zones ? Zones->FindZone(Anchor.ZoneId) : nullptr;
			if (!Zone)
			{
				return false;
			}
			OutLocation = Zone->EntryExitAnchors.IsEmpty() ? Zone->GetActorLocation()
				: Zone->GetActorTransform().TransformPosition(Zone->EntryExitAnchors[0].GetLocation());
			return !OutLocation.ContainsNaN();
		}

	case EFF_IncidentAnchorKind::Screenspace:
		return false;
	}

	return false;
}

void FFF_IncidentRules::BuildOptionViews(const UFF_IncidentDefinition& Definition, TArray<FFF_IncidentOptionView>& OutViews)
{
	OutViews.Reset(Definition.Options.Num());

	for (int32 Index = 0; Index < Definition.Options.Num(); ++Index)
	{
		FFF_IncidentOptionView& View = OutViews.AddDefaulted_GetRef();
		View.OptionIndex = Index;

		const UFF_IncidentOption* Option = Definition.GetOption(Index);
		if (!Option)
		{
			continue;
		}

		View.bValid = true;
		View.Label = Option->Label;
		View.CostGold = FMath::Max(0, Option->CostGold);
		View.CostStaff = FMath::Max(0, Option->CostStaff);
		View.MinStaff = FMath::Max(1, Option->MinStaff);
		View.WorkAmount = FMath::Max(0.f, Option->WorkAmount);
		TArray<FText> Descriptions;
		for (const UFF_IncidentEffect* Effect : Option->OnResolvedEffect)
		{
			if (!Effect)
			{
				continue;
			}
			const FText Description = Effect->GetDescription();
			if (!Description.IsEmpty())
			{
				Descriptions.Add(Description);
			}
		}
		View.RewardText = FText::Join(FText::FromString(TEXT(", ")), Descriptions);
	}
}
