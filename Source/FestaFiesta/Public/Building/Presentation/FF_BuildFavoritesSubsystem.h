// FF_BuildFavoritesSubsystem.h
// 역할: 즐겨찾기 시설을 개인 설정에 저장하고 관련 화면에 변경을 알린다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_BuildFavoritesSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnBuildFavoritesUpdated);

/** Personal catalog preferences, shared by the build catalog and facility inspector. */
UCLASS(meta = (DisplayName = "FF Build Favorites"))
class FESTAFIESTA_API UFF_BuildFavoritesSubsystem : public UGameInstanceSubsystem
{

	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Favorites")
	bool IsFavorite(FName FacilityDefinitionId) const;

	/** Returns the new favorite state. A missing definition ID is never stored. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Favorites")
	bool ToggleFavorite(FName FacilityDefinitionId);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Favorites")
	void SetFavorite(FName FacilityDefinitionId, bool bFavorite);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Favorites")
	TSet<FName> GetFavoriteIds() const { return FavoriteIds; }

	FSimpleMulticastDelegate OnFavoritesChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Building|Favorites")
	FFF_OnBuildFavoritesUpdated OnFavoritesUpdated;

private:
	void SavePreferences() const;

	UPROPERTY(Transient)
	TSet<FName> FavoriteIds;
};
