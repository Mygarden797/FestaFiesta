// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Incident/FF_IncidentTypes.h"
#include "FF_IncidentSubsystem.generated.h"


class UFF_IncidentCatalog; 
/**
 * 
 */
DECLARE_DELEGATE_OneParam(FFF_OnIncidentRaised, FFF_IncidentHandle);
DECLARE_DELEGATE_OneParam(FFF_OnIncidentClosed, FFF_IncidentHandle);



UCLASS()
class FESTAFIESTA_API UFF_IncidentSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
	
private:
	struct FDefinitionView
	{
		EFF_IncidentTone Tone = EFF_IncidentTone::Problem;
		EFF_IncidentUrgency Urgency = EFF_IncidentUrgency::Medium;
		float MinutesUntilLapse = 0.f;
		FGameplayTag EscalateTo;
	};
	
	
	struct FIncidentSlot
	{
		int32 Serial = 0;
		bool bInUse = false;
		
		FGameplayTag IncidentTag;
		FName DedupeKey = NAME_None;
		FFF_IncidentAnchor Anchor;
		FGameplayTagContainer ContextTags;
		
		EFF_IncidentTone Tone = EFF_IncidentTone::Problem;
		EFF_IncidentUrgency Urgency = EFF_IncidentUrgency::Medium;
		float MinutesUntilLapse = 0.f;
		FGameplayTag EscalateTo;
		
		EFF_IncidentState State = EFF_IncidentState::Active;
		float OccurredAtMinutes = 0.f;
		float LastSeenMinutes = 0.f;
		float Intensity = 0.f;
		float PeakIntensity = 0.f;
		int32 ChosenOptionIndex = INDEX_NONE;
	};

	
	
public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual TStatId GetStatId() const override;
	

	void Tick(float DeltaTime) override;
	FFF_IncidentHandle ReportIncident(const FFF_IncidentReport& Report);
	//사건 철회
	void WithdrawIncident(FName DedupeKey);
	//핸들 유효 관계
	bool IsHandleValid(FFF_IncidentHandle Handle) const;
	
	void GetActiveHandle(TArray<FFF_IncidentHandle>& OutHandles) const;
	
	//사건 발생, 종료 시에 전달되는 델리게이트
	FFF_OnIncidentRaised OnIncidentRaised;
	FFF_OnIncidentClosed OnIncidentClosed;
	
	
private:
	#if WITH_DEV_AUTOMATION_TESTS
	friend class FFF_IncidentCatalogLoadTest;
	#endif

	void LoadDefinitions();
	bool TryGetNowMinutes(float& OutMinutes) const;
	
	const FIncidentSlot* FindSlot(FFF_IncidentHandle Handle) const;
	
	void CloseSlot(int32 Index, EFF_IncidentState FinalState, float NowMinutes);
	
	
	
private:

	UPROPERTY()
	TObjectPtr<UFF_IncidentCatalog> LoadedCatalog;
	
	
	TArray<FIncidentSlot> IncidentSlots;
	TArray<int32> FreeSlots;
	TMap<FName, FFF_IncidentHandle> ActiveByKey;
	TMap<FGameplayTag,FDefinitionView> Definitions;
	int32 MaxConcurrent = 64;
	
};
