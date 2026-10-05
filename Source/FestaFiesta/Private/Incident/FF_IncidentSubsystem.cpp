// Fill out your copyright notice in the Description page of Project Settings.

#define FF_INCIDENT_LOG(Format, ...) \
UE_LOG(LogTemp, Warning, TEXT(Format), ##__VA_ARGS__)


#include "Incident/FF_IncidentSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Incident/Effects/FF_IncidentEffect.h"
#include "Incident/FF_IncidentSettings.h"
#include "Staff/FF_StaffDispatchSubsystem.h"
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
	CachedDef.Reset();
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



	
}

bool UFF_IncidentSubsystem::ResolveIncident(FFF_IncidentHandle Handle, int32 OptionIndex, int32 StaffCount,
	EFF_IncidentResolveResult* OutResult)
{
	EFF_IncidentResolveResult LocalResult = EFF_IncidentResolveResult::DispatchFailed;
	EFF_IncidentResolveResult& Result = OutResult ? *OutResult : LocalResult;

	const FIncidentSlot* Slot = FindSlot(Handle);
	if (!Slot)
	{
		FF_INCIDENT_LOG("ResolveIncident: handle %d/%d is not active", Handle.Index, Handle.Serial);
		Result = EFF_IncidentResolveResult::InvalidIncident;
		return false;
	}

	const UFF_IncidentDefinition* Definition = CachedDef.FindRef(Slot->IncidentTag);
	if (!Definition)
	{
		Result = EFF_IncidentResolveResult::InvalidIncident;
		return false;
	}
	const UFF_IncidentOption* Option = Definition->GetOption(OptionIndex);
	if (OptionIndex != INDEX_NONE && !Option)
	{
		Result = EFF_IncidentResolveResult::InvalidOption;
		return false;
	}
	int32 RequiredStaff = FMath::Max(0, Definition->RequiredStaff);
	float WorkAmount = 0.f;
	if (Option)
	{
		RequiredStaff = FMath::Max(FMath::Max(1, Option->MinStaff), StaffCount);
		WorkAmount = FMath::Max(0.f, Option->WorkAmount);
	}
	UFF_StaffDispatchSubsystem* Dispatch = GetWorld() ? GetWorld()->GetSubsystem<UFF_StaffDispatchSubsystem>() : nullptr;
	if (Dispatch && Dispatch->IsResponding(Handle))
	{
		Result = EFF_IncidentResolveResult::AlreadyResponding;
		return false;
	}
	if (RequiredStaff > 0)
	{
		if (!Dispatch)
		{
			Result = EFF_IncidentResolveResult::DispatchFailed;
			return false;
		}
		return Dispatch->TryDispatch(Handle, OptionIndex, RequiredStaff, Slot->Anchor, WorkAmount, &Result);
	}
	const bool bResolved = CompleteStaffResponse(Handle, OptionIndex);
	Result = bResolved ? EFF_IncidentResolveResult::Resolved : EFF_IncidentResolveResult::InvalidIncident;
	return bResolved;
}

/**
 * 만료는 플레이어가 아무것도 고르지 않은 상태라 Context 의 OptionIndex 는 INDEX_NONE 이다.
 * Context·상태·정의를 먼저 잡고 닫는다 — CloseSlot 의 방송을 받은 쪽이 이 슬롯을 곧바로 재사용할 수 있다.
 * 정의를 못 찾아도 사건은 닫는다. 그러지 않으면 매 틱 다시 만료 판정에 걸린다.
 */
void UFF_IncidentSubsystem::LapseIncident(FFF_IncidentHandle Handle, float NowMinutes)
{
	const FIncidentSlot* Slot = FindSlot(Handle);
	if (!Slot)
	{
		FF_INCIDENT_LOG("LapseIncident: handle %d/%d is not active", Handle.Index, Handle.Serial);
		return;
	}

	const FFF_IncidentContext Context = MakeContext(Handle, *Slot, INDEX_NONE);
	const EFF_IncidentState FinalState = FFF_IncidentRules::LapseStateFor(Slot->Tone);
	const UFF_IncidentDefinition* Definition = CachedDef.FindRef(Slot->IncidentTag);

	CloseSlot(Handle.Index, FinalState, NowMinutes);

	if (Definition && FinalState == EFF_IncidentState::Ignored)
	{
		ApplyEffects(Definition->OnFailedEffect, Context);
	}
}

bool UFF_IncidentSubsystem::CompleteStaffResponse(FFF_IncidentHandle Handle, int32 OptionIndex)
{
	const FIncidentSlot* Slot = FindSlot(Handle);
	if (!Slot)
	{
		return false;
	}
	// Copy effect context before CloseSlot broadcasts; observers may immediately reuse this slot.
	FFF_IncidentContext Context;
	Context.World = GetWorld();
	Context.Handle = Handle;
	Context.IncidentTag = Slot->IncidentTag;
	Context.Anchor = Slot->Anchor;
	Context.ContextTags = Slot->ContextTags;
	Context.OptionIndex = OptionIndex;
	Context.PeakIntensity = Slot->PeakIntensity;
	const UFF_IncidentDefinition* Definition = CachedDef.FindRef(Slot->IncidentTag);
	const UFF_IncidentOption* Option = Definition ? Definition->GetOption(OptionIndex) : nullptr;
	float Now = 0.f;
	TryGetNowMinutes(Now);

	IncidentSlots[Handle.Index].ChosenOptionIndex = OptionIndex;
	CloseSlot(Handle.Index, EFF_IncidentState::Resolved, Now);
	if (Option)
	{
		ApplyEffects(Option->OnResolvedEffect, Context);
	}
	return true;
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

bool UFF_IncidentSubsystem::GetIncidentView(FFF_IncidentHandle Handle, FFF_IncidentView& OutView) const
{
	const FIncidentSlot* Slot = FindSlot(Handle);

	
	FFF_IncidentView View;
	if (!Slot)
	{
		FF_INCIDENT_LOG("Slot is nllptr");
		return false;
	}
	
    TObjectPtr<UFF_IncidentDefinition> Def = CachedDef.FindRef(Slot->IncidentTag);
	
	if (!IsValid(Def))
	{
		FF_INCIDENT_LOG("Definition is nullptr.")
		return false;
	}
	
	
	OutView.IncidentTag = Slot->IncidentTag;
	OutView.MinuteUntileLapse = Slot->MinutesUntilLapse;
	OutView.Title = Def->Title;
	OutView.Body = Def->Body;
	OutView.MarkerIcon = Def->MarkerIcon;
	OutView.Anchor = Slot->Anchor;
	FFF_IncidentRules::BuildOptionViews(*Def, OutView.Options);

	return true;
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
				
				CachedDef.Add(Def->IncidentTag,Def);
				
				if (Def->MaxActive != INDEX_NONE)
				{
					IncidentMaxCheck.Add(Def->IncidentTag,0);
				}
				
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
	
	bool bEscalateTo = ((FinalState == EFF_IncidentState::Ignored) && Slot.EscalateTo.IsValid());
	FFF_IncidentReport EscalateToReport;
	
	if (bEscalateTo)
	{
		
		EscalateToReport.IncidentTag = Slot.EscalateTo; 
		EscalateToReport.Anchor = Slot.Anchor;
		EscalateToReport.ContextTags = Slot.ContextTags;
		EscalateToReport.Intensity = Slot.PeakIntensity;
	}
	
	FFF_IncidentHandle Handle;
	
	int32* IncidentMaxNum = IncidentMaxCheck.Find(Slot.IncidentTag);
	if (IncidentMaxNum != nullptr)
	{
		*IncidentMaxNum -=1;
	}
	
	Handle.Index = Index;
	Handle.Serial = Slot.Serial;
	
	Slot.Serial++;
	Slot.bInUse = false;
	Slot.State = FinalState;
	

	

	

	
	ActiveByKey.Remove(Slot.DedupeKey);
	FreeSlots.Push(Index);
	
	
	OnIncidentClosed.Broadcast(Handle);
	
	if (bEscalateTo)
	{
		ReportIncident(EscalateToReport);
	}

	
	
	

	
	
}

void UFF_IncidentSubsystem::SetResponding(FFF_IncidentHandle Handle, bool bRespond)
{
	if (!FindSlot(Handle))
	{
		FF_INCIDENT_LOG("SetResponding: handle %d/%d is not active", Handle.Index, Handle.Serial);
		return;
	}
	
	
	float Now = 0.f;
	const bool bHasNow = TryGetNowMinutes(Now);
	FIncidentSlot& Slot = IncidentSlots[Handle.Index];
	
	if (bRespond)
	{
		
		if (!bHasNow)
		{
			FF_INCIDENT_LOG("AcknowledgeAtMinutes is not recorded");
			return;
		}
		Slot.State = EFF_IncidentState::Acknowledged;
		Slot.AcknowledgedAtMinutes = Now;
		return;
		
		
	}
	if (Slot.State != EFF_IncidentState::Acknowledged)
	{
		return;
	}
	
	Slot.State = EFF_IncidentState::Active;
	if (bHasNow)
	{
		Slot.OccurredAtMinutes +=FMath::Max(0.f, Now-Slot.AcknowledgedAtMinutes);
	}
	
	

}

void UFF_IncidentSubsystem::ApplyEffects(const TArray<TObjectPtr<UFF_IncidentEffect>>& Effects, const FFF_IncidentContext& Context)
{
	for (UFF_IncidentEffect* Effect : Effects)
	{
		if (Effect && Effect->CanApply(Context))
		{
			Effect->Apply(Context);
		}
	}
}

FFF_IncidentContext UFF_IncidentSubsystem::MakeContext(FFF_IncidentHandle Handle, const FIncidentSlot& Slot,
	int32 OptionIndex)
{
	FFF_IncidentContext Context;
	Context.World = GetWorld();
	Context.Handle = Handle;
	Context.IncidentTag = Slot.IncidentTag;
	Context.Anchor = Slot.Anchor;
	Context.ContextTags = Slot.ContextTags;
	Context.OptionIndex = OptionIndex;
	Context.PeakIntensity = Slot.PeakIntensity;
	return Context;
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
		
		if (Slot.State ==EFF_IncidentState::Acknowledged)
			continue;
		
		
		if (FFF_IncidentRules::HasLapsed(Now,Slot.OccurredAtMinutes,Slot.MinutesUntilLapse))
		{
			FFF_IncidentHandle Handle;
			Handle.Index = index;
			Handle.Serial = Slot.Serial;
			LapseIncident(Handle, Now);
		}
	}
	
	
}

FFF_IncidentHandle UFF_IncidentSubsystem::ReportIncident(const FFF_IncidentReport& Report)
{
	FFF_IncidentHandle Handle;
	
	
	TObjectPtr<UFF_IncidentDefinition> IncDef = CachedDef.FindRef(Report.IncidentTag);

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
		OnIncidentUpdated.Broadcast(*Found);
		
		return *Found;


	}

	if (ActiveByKey.Num() >= MaxConcurrent)
	{
		FF_INCIDENT_LOG("IncidentSlot is Max");
		return FFF_IncidentHandle();
	}

	int32* IncidentMaxNum = IncidentMaxCheck.Find(Report.IncidentTag);
	if (IncidentMaxNum)
	{
		if (*IncidentMaxNum >= IncDef->MaxActive)
		{
			return FFF_IncidentHandle();
		}
		(*IncidentMaxNum)++;
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
	Slot.MinutesUntilLapse = IncDef->MinuteUntileLapse;
	Slot.OccurredAtMinutes = Now;
	Slot.LastSeenMinutes = Now;
	
	

	
	ActiveByKey.Add(Key, Handle);
	
	OnIncidentRaised.Broadcast(Handle);
	
	return Handle;
}


