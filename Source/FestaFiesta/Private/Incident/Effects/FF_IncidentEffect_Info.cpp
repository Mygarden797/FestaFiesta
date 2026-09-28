// Fill out your copyright notice in the Description page of Project Settings.

#include "Incident/Effects/FF_IncidentEffect_Info.h"

#include "GameFramework/Actor.h"

void UFF_IncidentEffect_Info::Apply(const FFF_IncidentContext& Context)
{
	const AActor* Actor = Context.Anchor.Actor.Get();

	UE_LOG(LogTemp, Log, TEXT("[IncidentEffect_Info] %s H=%d/%d Option=%d Actor=%s Note=%s"),
		*Context.IncidentTag.ToString(),
		Context.Handle.Index, Context.Handle.Serial,
		Context.OptionIndex,
		Actor ? *Actor->GetName() : TEXT("-"),
		*Note.ToString());
}
