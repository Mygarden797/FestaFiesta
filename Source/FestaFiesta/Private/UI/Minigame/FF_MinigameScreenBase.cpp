// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Minigame/FF_MinigameScreenBase.h"

#include "InputCoreTypes.h"

UFF_MinigameScreenBase::UFF_MinigameScreenBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A minigame is modal from CommonUI's perspective. The coordinator separately
	// enables the FestivalPlayerController ModalUi block so raw world picking and
	// overview-camera input cannot leak through the screen.
	ScreenInputMode = EFF_ScreenInputMode::Menu;
	bShowMouseCursor = true;
	bIgnoreInputConfig = false;
	bForceFocusOnActivate = true;
	SetIsFocusable(true);
}

void UFF_MinigameScreenBase::SetSessionId(const FGuid& InSessionId)
{
	SessionId = InSessionId;
	bCloseRequested = false;
}

bool UFF_MinigameScreenBase::BindRuntime(UFF_MinigameRuntime* InRuntime)
{
	return false;
}

void UFF_MinigameScreenBase::ShowResult(const FFF_MinigameResult& Result)
{
}

void UFF_MinigameScreenBase::RequestClose()
{
	if (bCloseRequested)
	{
		return;
	}

	bCloseRequested = true;
	OnCloseRequested.Broadcast(this, SessionId);
}

FReply UFF_MinigameScreenBase::NativeOnPreviewKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (!InKeyEvent.IsRepeat()
		&& (InKeyEvent.GetKey() == EKeys::Escape
			|| InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right))
	{
		RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
