// Fill out your copyright notice in the Description page of Project Settings.


#include "Incident/Effects/FF_IncidentEffect_Satisfactory.h"

#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Incident/Rules/FF_IncidentRules.h"

void UFF_IncidentEffect_Satisfactory::Apply(const FFF_IncidentContext& Context)
{
	Super::Apply(Context);
	
	UWorld* World = Context.World;
	if (!World)
	{
		return;
	}
	
	UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	
	if (!Crowd)
	{
		return;
	}
	
	FVector Center; 
	
	
	if (!FFF_IncidentRules::TryGetAnchorLocation(World,Context.Anchor,Center))
	{
		return;
	}
	
	Crowd->ApplySatisfactionPenaltyInArea(Center,Radius,SatisfactoryAmount);

}

FText UFF_IncidentEffect_Satisfactory::GetDescription() const
{
	if (SatisfactoryAmount <= 0 || Radius <= 0)
	{
		return FText::GetEmpty();
	}
	return FText::Format(NSLOCTEXT("FestaFiestaIncidentSatisfactory", "SatisfactionDrop", "주변 만족도 -{0}"),
		FText::AsNumber(SatisfactoryAmount));
}
