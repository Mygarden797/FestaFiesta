// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Minigame/FF_SurfingInputFocusSubsystem.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "Minigame/FF_MinigameSubsystem.h"
#include "Minigame/FF_MinigameTypes.h"
#include "Misc/CoreDelegates.h"
#include "TimerManager.h"
#include "UI/Minigame/FF_SurfingInputFocusPolicy.h"
#include "UI/Minigame/FF_SurfingScreen.h"
#include "UObject/UObjectIterator.h"

namespace
{
	constexpr int32 FocusRecoveryAttemptCount = 3;
}

void UFF_SurfingInputFocusSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFF_MinigameSubsystem>();
	Super::Initialize(Collection);
	CachedScreen.Reset();
	PendingSessionId.Invalidate();
	RemainingFocusAttempts = 0;
	bFocusAttemptScheduled = false;

	if (UFF_MinigameSubsystem* Minigames = GetWorld()
		? GetWorld()->GetSubsystem<UFF_MinigameSubsystem>()
		: nullptr)
	{
		Minigames->OnMinigameStarted.AddUniqueDynamic(
			this, &ThisClass::HandleMinigameStarted);
	}
	FCoreDelegates::ApplicationHasReactivatedDelegate.AddUObject(
		this, &ThisClass::HandleApplicationReactivated);
}

void UFF_SurfingInputFocusSubsystem::Deinitialize()
{
	if (UFF_MinigameSubsystem* Minigames = GetWorld()
		? GetWorld()->GetSubsystem<UFF_MinigameSubsystem>()
		: nullptr)
	{
		Minigames->OnMinigameStarted.RemoveAll(this);
	}
	FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll(this);

	CachedScreen.Reset();
	PendingSessionId.Invalidate();
	RemainingFocusAttempts = 0;
	bFocusAttemptScheduled = false;
	Super::Deinitialize();
}

bool UFF_SurfingInputFocusSubsystem::DoesSupportWorldType(
	const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFF_SurfingInputFocusSubsystem::HandleMinigameStarted(const FGuid SessionId)
{
	const UFF_MinigameSubsystem* Minigames = GetWorld()
		? GetWorld()->GetSubsystem<UFF_MinigameSubsystem>()
		: nullptr;
	if (!IsValid(Minigames)
		|| !IsValid(Minigames->GetActiveDefinition())
		|| !Minigames->GetActiveDefinition()->IsA<UFF_SurfingDefinition>())
	{
		return;
	}

	ScheduleFocusRecovery(SessionId);
}

void UFF_SurfingInputFocusSubsystem::HandleApplicationReactivated()
{
	const UFF_MinigameSubsystem* Minigames = GetWorld()
		? GetWorld()->GetSubsystem<UFF_MinigameSubsystem>()
		: nullptr;
	if (!IsValid(Minigames)
		|| !IsValid(Minigames->GetActiveDefinition())
		|| !Minigames->GetActiveDefinition()->IsA<UFF_SurfingDefinition>())
	{
		return;
	}

	const FFF_MinigameSessionSnapshot Snapshot = Minigames->GetActiveSnapshot();
	if (Snapshot.State == EFF_MinigameSessionState::Playing)
	{
		ScheduleFocusRecovery(Snapshot.Context.SessionId);
	}
}

void UFF_SurfingInputFocusSubsystem::ScheduleFocusRecovery(const FGuid SessionId)
{
	if (!SessionId.IsValid())
	{
		return;
	}

	PendingSessionId = SessionId;
	CachedScreen.Reset();
	RemainingFocusAttempts = FocusRecoveryAttemptCount;
	if (bFocusAttemptScheduled)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		bFocusAttemptScheduled = true;
		World->GetTimerManager().SetTimerForNextTick(
			this, &ThisClass::TryRepairFocus);
	}
}

void UFF_SurfingInputFocusSubsystem::TryRepairFocus()
{
	bFocusAttemptScheduled = false;
	if (RemainingFocusAttempts <= 0 || !PendingSessionId.IsValid())
	{
		return;
	}
	--RemainingFocusAttempts;

	UFF_SurfingScreen* Screen = CachedScreen.Get();
	if (!IsValid(Screen)
		|| Screen->GetWorld() != GetWorld()
		|| !Screen->IsInViewport()
		|| Screen->GetDisplayedSnapshot().Session.Context.SessionId
			!= PendingSessionId)
	{
		Screen = FindActiveSurfingScreen(PendingSessionId);
		CachedScreen = Screen;
	}

	if (IsValid(Screen))
	{
		APlayerController* OwningController = Screen->GetOwningPlayer();
		if (IsValid(OwningController))
		{
			const ESlateVisibility Visibility = Screen->GetVisibility();
			const bool bScreenActive = Screen->IsActivated()
				&& Visibility != ESlateVisibility::Collapsed
				&& Visibility != ESlateVisibility::Hidden;
			const bool bSurfingRunPlaying =
				Screen->GetDisplayedSnapshot().Session.State
					== EFF_MinigameSessionState::Playing;
			const bool bApplicationActive = FSlateApplication::IsInitialized()
				&& FSlateApplication::Get().IsActive();
			const bool bGamePaused = UGameplayStatics::IsGamePaused(this);
			const bool bScreenHasFocus = Screen->HasUserFocus(OwningController)
				|| Screen->HasUserFocusedDescendants(OwningController);

			if (FFSurfingInputFocusPolicy::ShouldRepairFocus(
				bScreenActive,
				bSurfingRunPlaying,
				bApplicationActive,
				bGamePaused,
				bScreenHasFocus))
			{
				Screen->SetUserFocus(OwningController);
			}
		}
	}

	if (RemainingFocusAttempts <= 0)
	{
		RemainingFocusAttempts = 0;
		PendingSessionId.Invalidate();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		bFocusAttemptScheduled = true;
		World->GetTimerManager().SetTimerForNextTick(
			this, &ThisClass::TryRepairFocus);
	}
}

UFF_SurfingScreen* UFF_SurfingInputFocusSubsystem::FindActiveSurfingScreen(
	const FGuid SessionId) const
{
	for (TObjectIterator<UFF_SurfingScreen> It; It; ++It)
	{
		UFF_SurfingScreen* Candidate = *It;
		if (!IsValid(Candidate)
			|| Candidate->GetWorld() != GetWorld()
			|| !Candidate->IsInViewport()
			|| !Candidate->IsActivated()
			|| Candidate->GetDisplayedSnapshot().Session.Context.SessionId
				!= SessionId)
		{
			continue;
		}

		return Candidate;
	}

	return nullptr;
}
