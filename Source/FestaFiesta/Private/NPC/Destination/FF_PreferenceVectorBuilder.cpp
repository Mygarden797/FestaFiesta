// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Destination/FF_PreferenceVectorBuilder.h"
#include "DataTables/FF_ArchetypeRow.h"
#include "DataTables/FF_CompanionModifierRow.h"
#include "Engine/DataTable.h"

TMap<FName, float> FFF_PreferenceVectorBuilder::Build(
	FName ArchetypeId,
	FName CompanionType,
	float PreferenceStrengthMultiplier,
	const UDataTable* ArchetypeTable,
	const UDataTable* CompanionModifierTable)
{
	TMap<FName, float> Result;

	const FFF_ArchetypeRow* Archetype = ArchetypeTable
		? ArchetypeTable->FindRow<FFF_ArchetypeRow>(ArchetypeId, TEXT("FFF_PreferenceVectorBuilder::Build"), /*bWarnIfRowMissing=*/false)
		: nullptr;
	const FFF_CompanionModifierRow* Companion = CompanionModifierTable
		? CompanionModifierTable->FindRow<FFF_CompanionModifierRow>(CompanionType, TEXT("FFF_PreferenceVectorBuilder::Build"), /*bWarnIfRowMissing=*/false)
		: nullptr;

	// build_unreal_datatables.mjs의 CATEGORY_FIELD와 동일한 카테고리 키를 유지해야 한다.
	auto AddCategory = [&Result, PreferenceStrengthMultiplier](FName Category, float ArchetypeDelta, float CompanionDelta)
	{
		Result.Add(Category, (ArchetypeDelta + CompanionDelta) * PreferenceStrengthMultiplier);
	};

	AddCategory(TEXT("ambience_photo_zone"), Archetype ? Archetype->AmbiencePhotoZoneDeltaPp : 0.f, Companion ? Companion->AmbiencePhotoZoneDeltaPp : 0.f);
	AddCategory(TEXT("food_stall"), Archetype ? Archetype->FoodStallDeltaPp : 0.f, Companion ? Companion->FoodStallDeltaPp : 0.f);
	AddCategory(TEXT("activity_game"), Archetype ? Archetype->ActivityGameDeltaPp : 0.f, Companion ? Companion->ActivityGameDeltaPp : 0.f);
	AddCategory(TEXT("rest_area"), Archetype ? Archetype->RestAreaDeltaPp : 0.f, Companion ? Companion->RestAreaDeltaPp : 0.f);
	AddCategory(TEXT("market_goods"), Archetype ? Archetype->MarketGoodsDeltaPp : 0.f, Companion ? Companion->MarketGoodsDeltaPp : 0.f);
	AddCategory(TEXT("performance_stage"), Archetype ? Archetype->PerformanceStageDeltaPp : 0.f, Companion ? Companion->PerformanceStageDeltaPp : 0.f);
	AddCategory(TEXT("festival_engagement_baseline"), Archetype ? Archetype->FestivalEngagementBaselineDeltaPp : 0.f, Companion ? Companion->FestivalEngagementBaselineDeltaPp : 0.f);
	AddCategory(TEXT("craft_experience"), Archetype ? Archetype->CraftExperienceDeltaPp : 0.f, Companion ? Companion->CraftExperienceDeltaPp : 0.f);
	AddCategory(TEXT("exploration_wander"), Archetype ? Archetype->ExplorationWanderDeltaPp : 0.f, Companion ? Companion->ExplorationWanderDeltaPp : 0.f);
	AddCategory(TEXT("nightlife_entertainment"), Archetype ? Archetype->NightlifeEntertainmentDeltaPp : 0.f, Companion ? Companion->NightlifeEntertainmentDeltaPp : 0.f);

	return Result;
}
