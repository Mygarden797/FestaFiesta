#include "Building/Presentation/FF_BuildFavoritesSubsystem.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
	constexpr const TCHAR* FavoritesSection = TEXT("FestaFiesta.BuildFavorites");
	constexpr const TCHAR* FavoritesKey = TEXT("FacilityDefinitionIds");
}

void UFF_BuildFavoritesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FavoriteIds.Reset();
	if (GConfig)
	{
		TArray<FString> SavedIds;
		GConfig->GetArray(FavoritesSection, FavoritesKey, SavedIds, GGameUserSettingsIni);
		for (const FString& Id : SavedIds)
		{
			const FName Name(*Id.TrimStartAndEnd());
			if (!Name.IsNone()) { FavoriteIds.Add(Name); }
		}
	}
}

void UFF_BuildFavoritesSubsystem::Deinitialize()
{
	OnFavoritesChanged.Clear();
	OnFavoritesUpdated.Clear();
	Super::Deinitialize();
}

bool UFF_BuildFavoritesSubsystem::IsFavorite(const FName FacilityDefinitionId) const
{
	return !FacilityDefinitionId.IsNone() && FavoriteIds.Contains(FacilityDefinitionId);
}

bool UFF_BuildFavoritesSubsystem::ToggleFavorite(const FName FacilityDefinitionId)
{
	if (FacilityDefinitionId.IsNone()) { return false; }
	const bool bNewState = !IsFavorite(FacilityDefinitionId);
	SetFavorite(FacilityDefinitionId, bNewState);
	return bNewState;
}

void UFF_BuildFavoritesSubsystem::SetFavorite(const FName FacilityDefinitionId, const bool bFavorite)
{
	if (FacilityDefinitionId.IsNone() || IsFavorite(FacilityDefinitionId) == bFavorite) { return; }
	if (bFavorite) { FavoriteIds.Add(FacilityDefinitionId); }
	else { FavoriteIds.Remove(FacilityDefinitionId); }
	SavePreferences();
	OnFavoritesChanged.Broadcast();
	OnFavoritesUpdated.Broadcast();
}

void UFF_BuildFavoritesSubsystem::SavePreferences() const
{
	if (!GConfig || GGameUserSettingsIni.IsEmpty()) { return; }
	TArray<FString> SavedIds;
	SavedIds.Reserve(FavoriteIds.Num());
	for (const FName Id : FavoriteIds) { SavedIds.Add(Id.ToString()); }
	SavedIds.Sort();
	// User settings live under Saved/Config. Never modify a festival save or a shared project default.
	GConfig->SetArray(FavoritesSection, FavoritesKey, SavedIds, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}
