// Fill out your copyright notice in the Description page of Project Settings.

#define FF_INCIDENT_LOG(Format, ...) \
UE_LOG(LogTemp, Warning, TEXT(Format), ##__VA_ARGS__)


#include "Incident/FF_IncidentSubsystem.h"

#include "Incident/FF_IncidentSettings.h"
#include "Incident/Rules/FF_IncidentRules.h"
#include "Time/FF_FestivalStageSubsystem.h"

bool UFF_IncidentSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return Super::DoesSupportWorldType(WorldType);
}

void UFF_IncidentSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	
	LoadDefinitions();
	
	
	
	
	
}

void UFF_IncidentSubsystem::Deinitialize()
{
	Super::Deinitialize();
	
	LoadedCatalog = nullptr;
	IncidentSlots.Reset();
	FreeSlots.Reset();
	ActiveByKey.Reset();
	Definitions.Reset();
	MaxConcurrent = 0;
}

TStatId UFF_IncidentSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(
	UFF_IncidentSubsystem,
	STATGROUP_Tickables);
}

void UFF_IncidentSubsystem::WithdrawIncident(FName DedupeKey)
{
	FFF_IncidentHandle* Handle =  ActiveByKey.Find(DedupeKey);
	
	if (!Handle)
	{
		return;
	}
	
	const int32 Index = Handle->Index;
	float Now = 0.f;
	TryGetNowMinutes(Now);
	
	CloseSlot(Index,EFF_IncidentState::Expired, Now);
	
	ActiveByKey.Remove(DedupeKey);
	
}

bool UFF_IncidentSubsystem::IsHandleValid(FFF_IncidentHandle Handle) const
{
	
	return FindSlot(Handle) !=nullptr;
}

void UFF_IncidentSubsystem::GetActiveHandle(TArray<FFF_IncidentHandle>& OutHandles) const
{
	OutHandles.Reset();
	OutHandles.Reserve(IncidentSlots.Num());
	
	for (int32 i = 0;i<IncidentSlots.Num(); i++)
	{
		const FIncidentSlot& Slot = IncidentSlots[i];
		if (Slot.bInUse)
		{
			FFF_IncidentHandle& Handle = OutHandles.AddDefaulted_GetRef();
			Handle.Index = i;
			Handle.Serial = Slot.Serial;
		}
	}
}

void UFF_IncidentSubsystem::LoadDefinitions()
{
	const UFF_IncidentSettings* Settings =   UFF_IncidentSettings::Get();
	if (!Settings)
	{
		FF_INCIDENT_LOG("Setting is not exist.");
		return;
	}
	MaxConcurrent =  Settings->MaxConcurrentIncidents;
	if (!Settings->Catalog.IsNull())
	{
		LoadedCatalog = Settings->Catalog.LoadSynchronous();
	}

	if (IsValid(LoadedCatalog))
	{
		for (FFF_IncidentCatalogEntry Entry : LoadedCatalog->Entries)
		{
			if (Entry.bEnabled)
			{
				UFF_IncidentDefinition* Def =  Entry.Definition.LoadSynchronous();
				if (!IsValid(Def))
				{
					FF_INCIDENT_LOG("Definition is not exist");
					continue;
				}
				
				FDefinitionView NewDef;
				NewDef.Tone = Def->Tone;
				NewDef.Urgency = Def->Urgency;
				NewDef.MinutesUntilLapse = Def->MinuteUntileLapse;
				NewDef.EscalateTo = Def->EscalateTo;
				
				Definitions.Add(Def->IncidentTag,NewDef);
			}
		}
		
	}
	else
	{
		FF_INCIDENT_LOG("Catalog is not exist");
	}
}

bool UFF_IncidentSubsystem::TryGetNowMinutes(float& OutMinutes) const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	
	if (!GameInstance)
	{
		FF_INCIDENT_LOG("GameInstance is invalid");
		return false;
	}
	
	UFF_FestivalStageSubsystem* FestivalStageSubsystem = 
		GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>();
	
	if (!FestivalStageSubsystem)
	{
		FF_INCIDENT_LOG("FestivalSubSystem is null");
		return false;
	}
	
	
	
	
	return FestivalStageSubsystem->TryGetLiveElapsedMinutes(OutMinutes);
}


const UFF_IncidentSubsystem::FIncidentSlot* UFF_IncidentSubsystem::FindSlot(FFF_IncidentHandle Handle) const
{
	if (!IncidentSlots.IsValidIndex(Handle.Index))
	{
		return nullptr;
	}
	
	const FIncidentSlot& Slot = IncidentSlots[Handle.Index];
	if (!Slot.bInUse || Slot.Serial != Handle.Serial)
	{
		return nullptr;
	}
	
	return &Slot;

}



void UFF_IncidentSubsystem::CloseSlot(int32 Index, EFF_IncidentState FinalState, float NowMinutes)
{
	if (!IncidentSlots.IsValidIndex(Index))
	{
		FF_INCIDENT_LOG("IncidentSlot Index is inValid");
		return;
	}
	
	
	FIncidentSlot& Slot = IncidentSlots[Index];
	Slot.Serial++;
	Slot.bInUse = false;
	Slot.State = FinalState;
	
	ActiveByKey.Remove(Slot.DedupeKey);
	FreeSlots.Push(Index);
	

	
	
}

void UFF_IncidentSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	float Now = 0.f;
	
	if (!TryGetNowMinutes(Now))
	{
		return;
	}
	
	for (int32 index =0;index < IncidentSlots.Num();index++)
	{
		const FIncidentSlot& Slot = IncidentSlots[index];
		if (!Slot.bInUse)
		{
			continue;
		}
		
		
		if (FFF_IncidentRules::HasLapsed(Now,Slot.OccurredAtMinutes,Slot.MinutesUntilLapse))
		{
			CloseSlot(index,FFF_IncidentRules::LapseStateFor(Slot.Tone),Now);
		}
	}
	
	
}

FFF_IncidentHandle UFF_IncidentSubsystem::ReportIncident(const FFF_IncidentReport& Report)
{
	FFF_IncidentHandle Handle;
	
	
	FDefinitionView* IncDef = Definitions.Find(Report.IncidentTag);
	if (!IncDef)
	{
		FF_INCIDENT_LOG("DefinitionView is Null. it is wrong IncidentTag.");
		return FFF_IncidentHandle();
	}
	
	float Now = 0.f;
	
	if (!TryGetNowMinutes(Now))
	{
		FF_INCIDENT_LOG("Current Time has not found");
		return FFF_IncidentHandle();
	}
	
	
	const FName Key = FFF_IncidentRules::MakeDedupeKey(Report.IncidentTag,Report.Anchor);
	if (const FFF_IncidentHandle* Found = ActiveByKey.Find(Key))
	{
		FIncidentSlot& ExistingSlot  = IncidentSlots[Found->Index];
		ExistingSlot.Intensity = Report.Intensity;
		ExistingSlot.PeakIntensity = 
			FFF_IncidentRules::MergeIntensity(ExistingSlot.PeakIntensity, Report.Intensity);
		ExistingSlot.LastSeenMinutes = Now;
		return *Found;


	}

	if (ActiveByKey.Num() >= MaxConcurrent)
	{
		FF_INCIDENT_LOG("IncidentSlot is Max");
		return FFF_IncidentHandle();
	}

	const int32 Index = FreeSlots.Num() > 0 ? FreeSlots.Pop() : IncidentSlots.AddDefaulted();
	
	
	

	Handle.Index = Index;
	Handle.Serial = IncidentSlots[Index].Serial;
	
	
	
	
	
	FIncidentSlot& Slot = IncidentSlots[Index];
	const int32 KeepSerial = Slot.Serial;
	Slot = FIncidentSlot();
	Slot.Serial = KeepSerial;
	Slot.bInUse = true;
	Slot.IncidentTag = Report.IncidentTag;
	Slot.Anchor = Report.Anchor;
	Slot.DedupeKey = Key;
	Slot.Intensity = Report.Intensity;
	Slot.PeakIntensity = Report.Intensity;
	Slot.ContextTags = Report.ContextTags;
	

	Slot.EscalateTo = IncDef->EscalateTo;
	Slot.Tone = IncDef->Tone;
	Slot.Urgency = IncDef->Urgency;
	Slot.MinutesUntilLapse = IncDef->MinutesUntilLapse;
	Slot.OccurredAtMinutes = Now;
	Slot.LastSeenMinutes = Now;
	
	
	
	

	
	ActiveByKey.Add(Key, Handle);
	
	
	return Handle;
}


