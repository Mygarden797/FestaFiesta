// Copyright Epic Games, Inc. All Rights Reserved.

#include "Incident/Rules/FF_IncidentRules.h"

#include "VectorUtil.h"

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

float FFF_IncidentRules::MergeIntensity(float ExistingPeak, float IncomingIntensity)
{
	
	return FMath::Max(ExistingPeak, IncomingIntensity);
}
