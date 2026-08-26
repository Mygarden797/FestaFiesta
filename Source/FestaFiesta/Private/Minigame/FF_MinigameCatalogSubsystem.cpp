// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/FF_MinigameCatalogSubsystem.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Minigame/FF_MinigameDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogFestaFiestaMinigameCatalog, Log, All);

void UFF_MinigameCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RefreshCatalog();
}

void UFF_MinigameCatalogSubsystem::Deinitialize()
{
	++RequestGeneration;
	if (ActiveLoadHandle.IsValid() && !ActiveLoadHandle->HasLoadCompleted())
	{
		ActiveLoadHandle->CancelHandle();
	}

	ActiveLoadHandle.Reset();
	RequestedAssetIds.Reset();
	DefinitionIndexById.Reset();
	DefinitionIndexByFacilityId.Reset();
	Definitions.Reset();
	LoadState = EFF_MinigameCatalogLoadState::NotRequested;

	Super::Deinitialize();
}

void UFF_MinigameCatalogSubsystem::RefreshCatalog()
{
	const uint64 ThisGeneration = ++RequestGeneration;
	if (ActiveLoadHandle.IsValid() && !ActiveLoadHandle->HasLoadCompleted())
	{
		ActiveLoadHandle->CancelHandle();
	}

	ActiveLoadHandle.Reset();
	RequestedAssetIds.Reset();
	DefinitionIndexById.Reset();
	DefinitionIndexByFacilityId.Reset();
	Definitions.Reset();
	LoadState = EFF_MinigameCatalogLoadState::Loading;

	UAssetManager& AssetManager = UAssetManager::Get();
	FPrimaryAssetTypeInfo TypeInfo;
	const FPrimaryAssetType& AssetType = UFF_MinigameDefinition::GetMinigameDefinitionAssetType();
	if (!AssetManager.GetPrimaryAssetTypeInfo(AssetType, TypeInfo))
	{
		UE_LOG(
			LogFestaFiestaMinigameCatalog,
			Error,
			TEXT("Primary Asset type '%s' is not configured."),
			*AssetType.ToString());
		FinishRefresh(EFF_MinigameCatalogLoadState::Failed);
		return;
	}

	AssetManager.GetPrimaryAssetIdList(AssetType, RequestedAssetIds);
	RequestedAssetIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B)
	{
		return A.ToString() < B.ToString();
	});

	if (RequestedAssetIds.IsEmpty())
	{
		FinishRefresh(EFF_MinigameCatalogLoadState::Ready);
		return;
	}

	FAssetManagerLoadParams LoadParams;
	LoadParams.OnComplete = FStreamableDelegateWithHandle::CreateUObject(
		this,
		&UFF_MinigameCatalogSubsystem::HandleLoadCompleted,
		ThisGeneration);
	LoadParams.OnCancel = FStreamableDelegateWithHandle::CreateUObject(
		this,
		&UFF_MinigameCatalogSubsystem::HandleLoadCanceled,
		ThisGeneration);

	ActiveLoadHandle = AssetManager.LoadPrimaryAssets(
		RequestedAssetIds,
		TArray<FName>(),
		MoveTemp(LoadParams));
}

UFF_MinigameDefinition* UFF_MinigameCatalogSubsystem::FindDefinitionById(const FName MinigameId) const
{
	const int32* DefinitionIndex = DefinitionIndexById.Find(MinigameId);
	return DefinitionIndex && Definitions.IsValidIndex(*DefinitionIndex)
		? Definitions[*DefinitionIndex]
		: nullptr;
}

UFF_MinigameDefinition* UFF_MinigameCatalogSubsystem::FindDefinitionForFacility(
	const FName FacilityDefinitionId) const
{
	const int32* DefinitionIndex = DefinitionIndexByFacilityId.Find(FacilityDefinitionId);
	return DefinitionIndex && Definitions.IsValidIndex(*DefinitionIndex)
		? Definitions[*DefinitionIndex]
		: nullptr;
}

void UFF_MinigameCatalogSubsystem::HandleLoadCompleted(
	TSharedPtr<FStreamableHandle> CompletedHandle,
	const uint64 Generation)
{
	if (Generation != RequestGeneration)
	{
		return;
	}

	(void)CompletedHandle;
	ActiveLoadHandle.Reset();
	RebuildCatalogFromLoadedAssets();
	FinishRefresh(EFF_MinigameCatalogLoadState::Ready);
}

void UFF_MinigameCatalogSubsystem::HandleLoadCanceled(
	TSharedPtr<FStreamableHandle> CanceledHandle,
	const uint64 Generation)
{
	if (Generation != RequestGeneration)
	{
		return;
	}

	(void)CanceledHandle;
	ActiveLoadHandle.Reset();
	FinishRefresh(EFF_MinigameCatalogLoadState::Failed);
}

void UFF_MinigameCatalogSubsystem::RebuildCatalogFromLoadedAssets()
{
	Definitions.Reset();
	DefinitionIndexById.Reset();
	DefinitionIndexByFacilityId.Reset();

	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FPrimaryAssetId& AssetId : RequestedAssetIds)
	{
		UFF_MinigameDefinition* Definition = Cast<UFF_MinigameDefinition>(
			AssetManager.GetPrimaryAssetObject(AssetId));
		if (!Definition)
		{
			UE_LOG(
				LogFestaFiestaMinigameCatalog,
				Warning,
				TEXT("Skipping unloaded minigame asset '%s'."),
				*AssetId.ToString());
			continue;
		}

		TArray<FText> Errors;
		if (!Definition->IsDefinitionValid(Errors))
		{
			UE_LOG(
				LogFestaFiestaMinigameCatalog,
				Warning,
				TEXT("Skipping invalid minigame '%s': %s"),
				*AssetId.ToString(),
				Errors.IsEmpty() ? TEXT("unknown validation error") : *Errors[0].ToString());
			continue;
		}

		if (DefinitionIndexById.Contains(Definition->MinigameId))
		{
			UE_LOG(
				LogFestaFiestaMinigameCatalog,
				Warning,
				TEXT("Skipping duplicate MinigameId '%s' from '%s'."),
				*Definition->MinigameId.ToString(),
				*AssetId.ToString());
			continue;
		}

		const int32 DefinitionIndex = Definitions.Add(Definition);
		DefinitionIndexById.Add(Definition->MinigameId, DefinitionIndex);
		for (const FName FacilityDefinitionId : Definition->CompatibleFacilityDefinitionIds)
		{
			if (DefinitionIndexByFacilityId.Contains(FacilityDefinitionId))
			{
				UE_LOG(
					LogFestaFiestaMinigameCatalog,
					Warning,
					TEXT("Facility '%s' is already routed; keeping the first sorted definition."),
					*FacilityDefinitionId.ToString());
				continue;
			}
			DefinitionIndexByFacilityId.Add(FacilityDefinitionId, DefinitionIndex);
		}
	}
}

void UFF_MinigameCatalogSubsystem::FinishRefresh(const EFF_MinigameCatalogLoadState NewState)
{
	LoadState = NewState;
	OnCatalogChanged.Broadcast();
}
