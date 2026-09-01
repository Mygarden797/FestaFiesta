// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Workspace/FF_FloatingPanelStateSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

UFF_FloatingPanelStateSubsystem* UFF_FloatingPanelStateSubsystem::Get(
	const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::ReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UFF_FloatingPanelStateSubsystem>() : nullptr;
}

void UFF_FloatingPanelStateSubsystem::SavePrep(
	const FFF_PrepFloatingPanelSnapshot& Snapshot)
{
	PrepSnapshot = Snapshot;
}

void UFF_FloatingPanelStateSubsystem::SaveLive(
	const FFF_LiveFloatingPanelSnapshot& Snapshot)
{
	LiveSnapshot = Snapshot;
}
