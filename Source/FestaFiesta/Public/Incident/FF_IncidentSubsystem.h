// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Incident/FF_IncidentTypes.h"
#include "FF_IncidentSubsystem.generated.h"


class UFF_IncidentDefinition;
class UFF_IncidentCatalog;
class UFF_IncidentEffect;
/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnIncidentRaised, FFF_IncidentHandle,IncidentHandle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnIncidentUpdated, FFF_IncidentHandle,IncidentHandle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnIncidentClosed, FFF_IncidentHandle,IncidentHandle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnIncidentFinished, const FFF_IncidentClosedInfo&, Info);



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
		float AcknowledgedAtMinutes = 0.f;
		
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
	void WithdrawIncident(FName DedupeKey, EFF_IncidentCloseReason Reason = EFF_IncidentCloseReason::NaturalResolution);
	// 필요한 스태프가 있으면 배정 시설에서 출동한다. 전원 도착해 작업량을 마친 뒤 사건을 닫는다.
	// 선택지가 있으면 플레이어가 고른 StaffCount 를 보내되 선택지의 MinStaff 아래로는 내리지 않는다.
	// 작업 시간(축제 분) = 선택지의 WorkAmount ÷ 보낸 인원. 선택지가 없으면 정의의 RequiredStaff 를 쓰고 작업량은 0이다.
	// OutResult 를 주면 접수(Dispatched/Resolved)인지, 거부라면 왜인지를 채운다.
	bool ResolveIncident(FFF_IncidentHandle Handle, int32 OptionIndex = INDEX_NONE, int32 StaffCount = INDEX_NONE,
		EFF_IncidentResolveResult* OutResult = nullptr);

	//핸들 유효 관계
	bool IsHandleValid(FFF_IncidentHandle Handle) const;
	
	void GetActiveHandle(TArray<FFF_IncidentHandle>& OutHandles) const;
	
	bool GetIncidentView(FFF_IncidentHandle Handle, FFF_IncidentView& OutView) const;
	
	
	//사건 발생, 종료 시에 전달되는 델리게이트
	FFF_OnIncidentRaised OnIncidentRaised;
	FFF_OnIncidentClosed OnIncidentClosed;

	//닫힌 이유·제목·출동 여부를 값으로 실어 OnIncidentClosed 바로 뒤에 방송한다.
	FFF_OnIncidentFinished OnIncidentFinished;
FFF_OnIncidentUpdated OnIncidentUpdated;
	
	
private:
	friend class UFF_StaffDispatchSubsystem;
	bool CompleteStaffResponse(FFF_IncidentHandle Handle, int32 OptionIndex);

	#if WITH_DEV_AUTOMATION_TESTS
	friend class FFF_IncidentCatalogLoadTest;
	friend class FFF_StaffDispatchIncidentTest;
	friend class FFF_StaffDispatchWarpTest;
	friend class FFF_StaffFacilityDispatchTest;
	friend class FFF_CrowdIncidentBridgeTest;
	#endif

	void LoadDefinitions();
	bool TryGetNowMinutes(float& OutMinutes) const;
	
	const FIncidentSlot* FindSlot(FFF_IncidentHandle Handle) const;
	
	void CloseSlot(int32 Index, EFF_IncidentState FinalState, float NowMinutes,
		EFF_IncidentCloseReason Reason = EFF_IncidentCloseReason::Unknown);

	// 수명이 다한 사건을 톤에 맞는 상태로 닫는다. Ignored 면 정의의 OnFailedEffect 를 적용한다. Tick 만 부른다.
	void LapseIncident(FFF_IncidentHandle Handle, float NowMinutes);

	void SetResponding(FFF_IncidentHandle Handle, bool bRespond);
	
	void ApplyEffects(const TArray<TObjectPtr<UFF_IncidentEffect>>& Effects, const FFF_IncidentContext& Context);
	
	FFF_IncidentContext MakeContext(FFF_IncidentHandle Handle,const FIncidentSlot& Slot, int32 OptionIndex);
	
private:

	UPROPERTY()
	TObjectPtr<UFF_IncidentCatalog> LoadedCatalog;
	
	
	TArray<FIncidentSlot> IncidentSlots;
	TArray<int32> FreeSlots;
	TMap<FName, FFF_IncidentHandle> ActiveByKey;
	
	UPROPERTY()
	TMap<FGameplayTag,TObjectPtr<UFF_IncidentDefinition>> CachedDef;
	
	
	TMap<FGameplayTag, int32> IncidentMaxCheck;
	
	int32 MaxConcurrent = 64;
	
};
