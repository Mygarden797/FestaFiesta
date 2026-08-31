// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/FF_AudioEventLink.h"

#include "Audio/FF_AudioSubsystem.h"
#include "Engine/World.h"
#include "Finale/FF_FinaleShowSubsystem.h"
#include "Queue/FF_QueueSubsystem.h"

bool UFF_AudioEventLink::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFF_AudioEventLink::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Initialize 가 아니라 여기서 묶는다. 서브시스템 생성 순서는 보장되지 않아서
	// Initialize 시점엔 상대가 아직 없을 수 있다.
	BindAll();
}

void UFF_AudioEventLink::Deinitialize()
{
	UnbindAll();
	Super::Deinitialize();
}

UFF_AudioSubsystem* UFF_AudioEventLink::GetAudio() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFF_AudioSubsystem>() : nullptr;
}

void UFF_AudioEventLink::BindAll()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!bBoundFinale)
	{
		if (UFF_FinaleShowSubsystem* Finale = World->GetSubsystem<UFF_FinaleShowSubsystem>())
		{
			Finale->OnBurst.AddDynamic(this, &UFF_AudioEventLink::HandleFinaleBurst);
			Finale->OnShowStateChanged.AddDynamic(this, &UFF_AudioEventLink::HandleFinaleShowStateChanged);
			bBoundFinale = true;
		}
	}

	if (!bBoundQueue)
	{
		if (UFF_QueueSubsystem* Queue = World->GetSubsystem<UFF_QueueSubsystem>())
		{
			Queue->OnPartyJoined.AddDynamic(this, &UFF_AudioEventLink::HandleQueuePartyJoined);
			Queue->OnPartyRejected.AddDynamic(this, &UFF_AudioEventLink::HandleQueuePartyRejected);
			Queue->OnServiceStarted.AddDynamic(this, &UFF_AudioEventLink::HandleQueueServiceStarted);
			Queue->OnPartyExited.AddDynamic(this, &UFF_AudioEventLink::HandleQueuePartyExited);
			bBoundQueue = true;
		}
	}
}

void UFF_AudioEventLink::UnbindAll()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		bBoundFinale = false;
		bBoundQueue = false;
		return;
	}

	if (bBoundFinale)
	{
		if (UFF_FinaleShowSubsystem* Finale = World->GetSubsystem<UFF_FinaleShowSubsystem>())
		{
			Finale->OnBurst.RemoveDynamic(this, &UFF_AudioEventLink::HandleFinaleBurst);
			Finale->OnShowStateChanged.RemoveDynamic(this, &UFF_AudioEventLink::HandleFinaleShowStateChanged);
		}
		bBoundFinale = false;
	}

	if (bBoundQueue)
	{
		if (UFF_QueueSubsystem* Queue = World->GetSubsystem<UFF_QueueSubsystem>())
		{
			Queue->OnPartyJoined.RemoveDynamic(this, &UFF_AudioEventLink::HandleQueuePartyJoined);
			Queue->OnPartyRejected.RemoveDynamic(this, &UFF_AudioEventLink::HandleQueuePartyRejected);
			Queue->OnServiceStarted.RemoveDynamic(this, &UFF_AudioEventLink::HandleQueueServiceStarted);
			Queue->OnPartyExited.RemoveDynamic(this, &UFF_AudioEventLink::HandleQueuePartyExited);
		}
		bBoundQueue = false;
	}
}

void UFF_AudioEventLink::HandleFinaleBurst(const FVector WorldLocation, const FLinearColor Color)
{
	(void)Color;

	if (!bLinkEnabled)
	{
		return;
	}

	// 터진 자리에서 소리를 낸다. 멀리서 터지면 작게 들려야 거리감이 산다.
	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->PlayEventAtLocation(EFF_AudioEvent::FinaleBurst, WorldLocation);
	}
}

void UFF_AudioEventLink::HandleFinaleShowStateChanged(const bool bInProgress)
{
	if (!bLinkEnabled)
	{
		return;
	}

	UFF_AudioSubsystem* Audio = GetAudio();
	if (!Audio)
	{
		return;
	}

	if (bInProgress)
	{
		Audio->PlayEvent2D(EFF_AudioEvent::FinaleShowStarted);
		Audio->SetAmbience(EFF_AudioAmbience::Finale);
		return;
	}

	Audio->PlayEvent2D(EFF_AudioEvent::FinaleShowEnded);

	// 연출이 끝나면 배경음을 되돌린다. 어느 단계로 갈지는 진행 쪽이 알고 있으므로
	// 여기서는 피날레 곡만 걷어내고 다음 요청을 기다린다.
	if (Audio->GetCurrentAmbience() == EFF_AudioAmbience::Finale)
	{
		Audio->SetAmbience(EFF_AudioAmbience::Closing);
	}
}

void UFF_AudioEventLink::HandleQueuePartyJoined(
	const int32 FacilityId, const int32 PartyId, const int32 SlotIndex, const float EstimatedWaitSeconds)
{
	(void)FacilityId; (void)PartyId; (void)SlotIndex; (void)EstimatedWaitSeconds;

	if (!bLinkEnabled)
	{
		return;
	}

	// 위치를 모르므로 2D 로 낸다. 좌표가 필요해지면 이벤트에 담아야 하는데,
	// 그건 Queue 계약을 바꾸는 일이라 소리 하나 때문에 할 일은 아니다.
	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->PlayEvent2D(EFF_AudioEvent::QueueJoined);
	}
}

void UFF_AudioEventLink::HandleQueuePartyRejected(
	const int32 FacilityId, const int32 PartyId, const EFF_QueueJoinResult Reason)
{
	(void)FacilityId; (void)PartyId; (void)Reason;

	if (!bLinkEnabled)
	{
		return;
	}

	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->PlayEvent2D(EFF_AudioEvent::QueueRejected);
	}
}

void UFF_AudioEventLink::HandleQueueServiceStarted(
	const int32 FacilityId, const int32 PartyId, const float WaitedSeconds)
{
	(void)FacilityId; (void)PartyId; (void)WaitedSeconds;

	if (!bLinkEnabled)
	{
		return;
	}

	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->PlayEvent2D(EFF_AudioEvent::QueueServiceStarted);
	}
}

void UFF_AudioEventLink::HandleQueuePartyExited(
	const int32 FacilityId, const int32 PartyId, const EFF_QueueExit ExitType, const float WaitedSeconds)
{
	(void)FacilityId; (void)PartyId; (void)WaitedSeconds;

	if (!bLinkEnabled)
	{
		return;
	}

	UFF_AudioSubsystem* Audio = GetAudio();
	if (!Audio)
	{
		return;
	}

	// 서비스를 받고 나간 것과 기다리다 포기한 것은 다른 소리여야 한다.
	// 둘을 같은 소리로 내면 플레이어가 시설이 잘 돌아가는지 알 수 없다.
	switch (ExitType)
	{
	case EFF_QueueExit::Served:
		Audio->PlayEvent2D(EFF_AudioEvent::QueueServed);
		break;
	case EFF_QueueExit::Abandoned:
		Audio->PlayEvent2D(EFF_AudioEvent::QueueAbandoned);
		break;
	default:
		// 폐쇄로 인한 취소는 플레이어 잘못이 아니므로 별도 소리를 내지 않는다.
		break;
	}
}
