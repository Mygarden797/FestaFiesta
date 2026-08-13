// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/FF_BuildCatalogEconomyReadModel.h"

#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "UI/FF_UILog.h"

void UFF_BuildCatalogEconomyReadModel::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFF_FestivalProgressionSubsystem>();
	Super::Initialize(Collection);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		ProgressionSubsystem =
			GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	}

	if (!ProgressionSubsystem)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[BuildCatalogEconomy] FestivalProgressionSubsystem을 찾지 못했습니다."));
		return;
	}

	ProgressionSubsystem->OnGoldChanged.AddUniqueDynamic(
		this, &ThisClass::HandleProgressionGoldChanged);
	// Progression은 이 읽기 모델보다 먼저 초기 Festival을 시작하므로 과거 이벤트에 의존하지 않고
	// 구독 직후 현재 값을 함께 동기화합니다.
	HandleProgressionGoldChanged(ProgressionSubsystem->GetGold());
}

void UFF_BuildCatalogEconomyReadModel::Deinitialize()
{
	if (ProgressionSubsystem)
	{
		ProgressionSubsystem->OnGoldChanged.RemoveDynamic(
			this, &ThisClass::HandleProgressionGoldChanged);
	}
	ProgressionSubsystem = nullptr;
	bHasAuthoritativeGold = false;
	Super::Deinitialize();
}

void UFF_BuildCatalogEconomyReadModel::HandleProgressionGoldChanged(const int32 NewGold)
{
	const int32 SanitizedGold = FMath::Max(0, NewGold);
	const bool bValueChanged = !bHasAuthoritativeGold || Gold != SanitizedGold;
	Gold = SanitizedGold;
	bHasAuthoritativeGold = true;

	if (bValueChanged)
	{
		OnGoldChanged.Broadcast(Gold);
	}
}
