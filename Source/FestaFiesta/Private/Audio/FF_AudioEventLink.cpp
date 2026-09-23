// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/FF_AudioEventLink.h"

#include "Audio/FF_AudioSubsystem.h"
#include "Engine/World.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Finale/FF_FinaleShowSubsystem.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"

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

	if (!bBoundStage)
	{
		// 단계는 게임 인스턴스가 소유한다. 월드가 바뀌어도 축제는 이어지기 때문이다.
		const UGameInstance* GameInstance = World->GetGameInstance();
		if (UFF_FestivalStageSubsystem* Stage =
			GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr)
		{
			Stage->OnStageChanged.AddDynamic(this, &UFF_AudioEventLink::HandleStageChanged);
			Stage->OnEnvironmentStateChanged.AddDynamic(this, &UFF_AudioEventLink::HandleEnvironmentChanged);
			bBoundStage = true;

			// 신호는 바뀔 때만 온다. 지금 상태를 한 번 읽어서 배경음을 맞춘다.
			// 밤낮도 같이 읽는다. 빠뜨리면 밤에 레벨이 열렸을 때 다음 밤 신호가 올 때까지
			// 낮 곡이 돈다 — 그 신호는 하루에 한 번뿐이다.
			LastStage = Stage->CurrentStage;
			bLastIsNight = Stage->GetIsNightActive();
			RefreshAmbience();
		}
	}

	if (!bBoundBuild)
	{
		if (UFF_BuildModeSubsystem* Build = World->GetSubsystem<UFF_BuildModeSubsystem>())
		{
			Build->OnBuildModeStateChanged.AddDynamic(this, &UFF_AudioEventLink::HandleBuildModeStateChanged);
			bBoundBuild = true;
		}
	}

	if (!bBoundZoneWorld)
	{
		// 시설 설치·철거는 Zone 이 확정한 뒤에만 방송된다.
		// 건축 모드 상태만 보면 카탈로그를 열고 닫은 것까지 설치로 잡힌다.
		if (UFFZSWorldSubsystem* ZoneWorld = World->GetSubsystem<UFFZSWorldSubsystem>())
		{
			ZoneWorld->OnWorldChanged.AddDynamic(this, &UFF_AudioEventLink::HandleWorldChanged);
			ZoneWorld->OnPlacementRejected.AddDynamic(this, &UFF_AudioEventLink::HandlePlacementRejected);
			bBoundZoneWorld = true;
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
		bBoundStage = false;
		bBoundBuild = false;
		bBoundZoneWorld = false;
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

	if (bBoundStage)
	{
		const UGameInstance* GameInstance = World->GetGameInstance();
		if (UFF_FestivalStageSubsystem* Stage =
			GameInstance ? GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr)
		{
			Stage->OnStageChanged.RemoveDynamic(this, &UFF_AudioEventLink::HandleStageChanged);
			Stage->OnEnvironmentStateChanged.RemoveDynamic(this, &UFF_AudioEventLink::HandleEnvironmentChanged);
		}
		bBoundStage = false;
	}

	if (bBoundBuild)
	{
		if (UFF_BuildModeSubsystem* Build = World->GetSubsystem<UFF_BuildModeSubsystem>())
		{
			Build->OnBuildModeStateChanged.RemoveDynamic(this, &UFF_AudioEventLink::HandleBuildModeStateChanged);
		}
		bBoundBuild = false;
	}

	if (bBoundZoneWorld)
	{
		if (UFFZSWorldSubsystem* ZoneWorld = World->GetSubsystem<UFFZSWorldSubsystem>())
		{
			ZoneWorld->OnWorldChanged.RemoveDynamic(this, &UFF_AudioEventLink::HandleWorldChanged);
			ZoneWorld->OnPlacementRejected.RemoveDynamic(this, &UFF_AudioEventLink::HandlePlacementRejected);
		}
		bBoundZoneWorld = false;
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

void UFF_AudioEventLink::HandleStageChanged(
	const EFF_FestivalStage PreviousStage, const EFF_FestivalStage NewStage)
{
	(void)PreviousStage;

	LastStage = NewStage;

	if (!bLinkEnabled)
	{
		return;
	}

	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->PlayEvent2D(EFF_AudioEvent::StageChanged);
	}

	RefreshAmbience();
}

void UFF_AudioEventLink::HandleEnvironmentChanged(const EFF_WeatherType Weather, const bool bIsNight)
{
	(void)Weather;

	// 같은 값이 다시 오면 넘긴다. 날씨만 바뀌었는데 배경음을 다시 걸 이유가 없다.
	if (bLastIsNight == bIsNight)
	{
		return;
	}
	bLastIsNight = bIsNight;

	if (!bLinkEnabled)
	{
		return;
	}

	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->PlayEvent2D(bIsNight ? EFF_AudioEvent::NightStarted : EFF_AudioEvent::DayStarted);
	}

	RefreshAmbience();
}

void UFF_AudioEventLink::RefreshAmbience()
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

	// 연출 중에는 건드리지 않는다. 피날레 곡이 걸려 있는데 단계 신호가 오면
	// 그 위에 덮어써서 연출이 끊긴다.
	if (Audio->GetCurrentAmbience() == EFF_AudioAmbience::Finale)
	{
		return;
	}

	EFF_AudioAmbience Next = EFF_AudioAmbience::None;

	switch (LastStage)
	{
	case EFF_FestivalStage::Setup:
	case EFF_FestivalStage::MorningCheck:
	case EFF_FestivalStage::PromotionPreview:
		Next = EFF_AudioAmbience::Setup;
		break;

	case EFF_FestivalStage::Live:
	case EFF_FestivalStage::SunsetTransition:
		// 운영 중에는 밤낮으로 갈린다. 같은 곡이면 시간이 흐르는 느낌이 안 난다.
		Next = bLastIsNight ? EFF_AudioAmbience::NightLive : EFF_AudioAmbience::DayLive;
		break;

	case EFF_FestivalStage::Finale:
		Next = EFF_AudioAmbience::Finale;
		break;

	case EFF_FestivalStage::Closing:
	case EFF_FestivalStage::Report:
		Next = EFF_AudioAmbience::Closing;
		break;

	default:
		break;
	}

	Audio->SetAmbience(Next);
}

void UFF_AudioEventLink::HandleBuildModeStateChanged(
	const EFF_BuildModeState PreviousState, const EFF_BuildModeState NewState)
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

	// 카탈로그를 열고 닫는 소리다. 실제 설치·철거음은 Zone 이 확정한 뒤
	// HandleWorldChanged 에서 낸다.
	const bool bWasClosed = PreviousState == EFF_BuildModeState::Closed;
	const bool bIsClosed = NewState == EFF_BuildModeState::Closed;

	if (bWasClosed && !bIsClosed)
	{
		Audio->PlayEvent2D(EFF_AudioEvent::UiConfirm);
		return;
	}

	if (!bWasClosed && bIsClosed)
	{
		Audio->PlayEvent2D(EFF_AudioEvent::UiCancel);
	}
}

void UFF_AudioEventLink::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
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

	// 종류마다 다른 소리를 낸다. 설치와 철거가 같은 소리면 뭘 했는지 귀로 구분이 안 된다.
	EFF_AudioEvent Sound = EFF_AudioEvent::None;

	switch (Event.Type)
	{
	case EFFZSCommandType::Install:        Sound = EFF_AudioEvent::BuildPlaced;         break;
	case EFFZSCommandType::Move:           Sound = EFF_AudioEvent::BuildMoved;          break;
	case EFFZSCommandType::Remove:         Sound = EFF_AudioEvent::BuildRemoved;        break;
	case EFFZSCommandType::Open:           Sound = EFF_AudioEvent::BuildFacilityOpened; break;
	case EFFZSCommandType::Close:
	case EFFZSCommandType::EmergencyClose: Sound = EFF_AudioEvent::BuildFacilityClosed; break;
	default: return;
	}

	// 위치를 모르므로 2D 로 낸다. 시설 좌표를 얻으려면 Zone 에 다시 물어야 하는데,
	// 소리 하나 때문에 그 조회를 매번 하는 건 과하다.
	Audio->PlayEvent2D(Sound);
}

void UFF_AudioEventLink::HandlePlacementRejected(
	const FFFZSCommand& Command, const FFFZSCommandResult& Result)
{
	(void)Command; (void)Result;

	if (!bLinkEnabled)
	{
		return;
	}

	if (UFF_AudioSubsystem* Audio = GetAudio())
	{
		Audio->PlayEvent2D(EFF_AudioEvent::BuildRejected);
	}
}
