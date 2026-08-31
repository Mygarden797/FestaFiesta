// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Audio/FF_AudioTypes.h"
#include "CoreMinimal.h"
#include "Queue/FF_QueueTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_AudioEventLink.generated.h"

class UFF_AudioSubsystem;

/**
 * 우리 파트에서 나오는 신호를 사운드로 잇는다.
 *
 * ─── 왜 별도인가 ────────────────────────────────────────────────────────────
 * 오디오 서브시스템은 "무슨 소리를 어떻게 낼지"만 안다. 어떤 순간에 소리가 나야 하는지는
 * 각 파트가 알고 있고, 그 둘을 잇는 코드는 어딘가에 있어야 한다. 그 자리를 여기로 둔다.
 *
 * 이렇게 나눠두면 소리를 붙이려고 Queue 나 Finale 코드를 고칠 일이 없다.
 * 새 소리가 필요해지면 여기에 구독 한 줄만 늘리면 된다.
 *
 * ─── 무엇이 없어도 돌아간다 ─────────────────────────────────────────────────
 * 구독 대상이 없으면 그냥 안 붙는다. 사운드 뱅크가 비어 있어도 조용히 넘어간다.
 */
UCLASS()
class FESTAFIESTA_API UFF_AudioEventLink : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	/** 결선을 끊는다. 소리 때문에 다른 확인이 방해될 때 쓴다. */
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
	void HandleQueuePartyJoined(int32 FacilityId, int32 PartyId, int32 SlotIndex, float EstimatedWaitSeconds);

	UFUNCTION()
	void HandleQueuePartyRejected(int32 FacilityId, int32 PartyId, EFF_QueueJoinResult Reason);

	UFUNCTION()
	void HandleQueueServiceStarted(int32 FacilityId, int32 PartyId, float WaitedSeconds);

	UFUNCTION()
	void HandleQueuePartyExited(int32 FacilityId, int32 PartyId, EFF_QueueExit ExitType, float WaitedSeconds);

	void BindAll();
	void UnbindAll();

	UFF_AudioSubsystem* GetAudio() const;

	bool bLinkEnabled = true;
	bool bBoundFinale = false;
	bool bBoundQueue = false;
};
