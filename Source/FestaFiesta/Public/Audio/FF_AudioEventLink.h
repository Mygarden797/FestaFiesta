// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Audio/FF_AudioTypes.h"
#include "CoreMinimal.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "FF_FFZSTypes.h"
#include "Finale/FF_FinaleShowTypes.h"
#include "Queue/FF_QueueTypes.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_AudioEventLink.generated.h"

class UFF_AudioSubsystem;

// 우리 파트에서 나오는 신호를 사운드로 잇는다.
//
// ─── 왜 별도인가 ────────────────────────────────────────────────────────────
// 오디오 서브시스템은 "무슨 소리를 어떻게 낼지"만 안다. 어떤 순간에 소리가 나야 하는지는
// 각 파트가 알고 있고, 그 둘을 잇는 코드는 어딘가에 있어야 한다. 그 자리를 여기로 둔다.
//
// 이렇게 나눠두면 소리를 붙이려고 Queue 나 Finale 코드를 고칠 일이 없다.
// 새 소리가 필요해지면 여기에 구독 한 줄만 늘리면 된다.
//
// ─── 무엇이 없어도 돌아간다 ─────────────────────────────────────────────────
// 구독 대상이 없으면 그냥 안 붙는다. 사운드 뱅크가 비어 있어도 조용히 넘어간다.
UCLASS()
class FESTAFIESTA_API UFF_AudioEventLink : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	// 결선을 끊는다. 소리 때문에 다른 확인이 방해될 때 쓴다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Audio")
	void SetLinkEnabled(bool bEnabled) { bLinkEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Audio")
	bool IsLinkEnabled() const { return bLinkEnabled; }

private:
	UFUNCTION()
	void HandleFinaleBurst(FVector WorldLocation, FLinearColor Color);

	UFUNCTION()
	void HandleFinaleShowStateChanged(bool bInProgress);

	UFUNCTION()
	void HandleFinaleTourActChanged(EFF_FinaleTourAct NewAct);

	// 피날레 곡이 걸려 있는지. 엔딩곡도 피날레 곡이다. 단계 신호가 덮어쓰면 안 된다.
	static bool IsFinaleAmbience(EFF_AudioAmbience Ambience);

	UFUNCTION()
	void HandleQueuePartyJoined(int32 FacilityId, int32 PartyId, int32 SlotIndex, float EstimatedWaitSeconds);

	UFUNCTION()
	void HandleQueuePartyRejected(int32 FacilityId, int32 PartyId, EFF_QueueJoinResult Reason);

	UFUNCTION()
	void HandleQueueServiceStarted(int32 FacilityId, int32 PartyId, float WaitedSeconds);

	UFUNCTION()
	void HandleQueuePartyExited(int32 FacilityId, int32 PartyId, EFF_QueueExit ExitType, float WaitedSeconds);

	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	UFUNCTION()
	void HandleEnvironmentChanged(EFF_WeatherType Weather, bool bIsNight);

	UFUNCTION()
	void HandleBuildModeStateChanged(EFF_BuildModeState PreviousState, EFF_BuildModeState NewState);

	// 시설이 실제로 설치·이동·철거·개폐될 때. Zone 이 확정한 뒤에만 방송한다.
	UFUNCTION()
	void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);

	// 배치가 거절됐을 때. 설치음과 다른 소리를 내야 실패한 걸 안다.
	UFUNCTION()
	void HandlePlacementRejected(const FFFZSCommand& Command, const FFFZSCommandResult& Result);

	// 지금 단계와 밤낮으로 배경음을 정한다.
	//
	// 단계만 보면 낮과 밤이 같은 곡이 되고, 밤낮만 보면 준비 단계와 운영이 구분되지 않는다.
	// 두 신호가 따로 오므로 어느 쪽이 바뀌든 여기서 다시 계산한다.
	void RefreshAmbience();

	void BindAll();
	void UnbindAll();

	UFF_AudioSubsystem* GetAudio() const;

	// 불꽃 소리가 듣는 자리까지 오는 데 걸리는 시간. 설정이 꺼져 있으면 0.
	float ResolveBurstSoundDelay(const FVector& WorldLocation) const;

	bool bLinkEnabled = true;
	bool bBoundFinale = false;
	bool bBoundQueue = false;
	bool bBoundStage = false;
	bool bBoundBuild = false;
	bool bBoundZoneWorld = false;

	// 이 월드가 로비(프론트엔드 레벨)인지. 로비면 축제 단계와 상관없이 로비 곡을 튼다.
	bool bIsLobbyWorld = false;

	// 로비 곡이 뱅크에 없다는 것을 한 번만 알린다.
	bool bWarnedMissingLobbyTrack = false;

	// 마지막으로 받은 상태. 두 신호를 합쳐서 배경음을 정하려면 둘 다 들고 있어야 한다.
	EFF_FestivalStage LastStage = EFF_FestivalStage::Setup;
	bool bLastIsNight = false;
};
