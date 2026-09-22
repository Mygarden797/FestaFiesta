// FF_BuildFavoritesSubsystem.cpp
// 역할: 즐겨찾기 시설을 개인 설정에 저장하고 관련 화면에 변경을 알린다.

#include "Building/Presentation/FF_BuildFavoritesSubsystem.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
	constexpr const TCHAR* FavoritesSection = TEXT("FestaFiesta.BuildFavorites");
	constexpr const TCHAR* FavoritesKey = TEXT("FacilityDefinitionIds");
}

// 개인 설정 파일에 저장된 시설 ID를 읽고 중복·빈 값을 제외한다.
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

// 한 번 누르면 등록, 다시 누르면 해제하며 변경된 상태를 반환한다.
bool UFF_BuildFavoritesSubsystem::ToggleFavorite(const FName FacilityDefinitionId)
{
	if (FacilityDefinitionId.IsNone()) { return false; }
	const bool bNewState = !IsFavorite(FacilityDefinitionId);
	SetFavorite(FacilityDefinitionId, bNewState);
	return bNewState;
}

// 상태가 실제로 달라질 때만 저장하고 화면들에 변경을 알린다.
void UFF_BuildFavoritesSubsystem::SetFavorite(const FName FacilityDefinitionId, const bool bFavorite)
{
	if (FacilityDefinitionId.IsNone() || IsFavorite(FacilityDefinitionId) == bFavorite) { return; }
	if (bFavorite) { FavoriteIds.Add(FacilityDefinitionId); }
	else { FavoriteIds.Remove(FacilityDefinitionId); }
	SavePreferences();
	OnFavoritesChanged.Broadcast();
	OnFavoritesUpdated.Broadcast();
}

// 즐겨찾기를 정렬하여 개인 설정에 저장한다. 축제 진행 저장과 구분된다.
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
