// UTileView용 시설 카드 데이터 객체 구현.

#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"

#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"

void UFF_PrepBuildTileData::Initialize(
	const FFF_BuildCatalogEntry& InEntry,
	const int32 InGold,
	const bool bInHasAuthoritativeGold)
{
	Entry = InEntry;
	Gold = FMath::Max(0, InGold);
	bHasAuthoritativeGold = bInHasAuthoritativeGold;
	RefreshDerivedValues();
}

void UFF_PrepBuildTileData::RefreshGold(
	const int32 InGold,
	const bool bInHasAuthoritativeGold)
{
	Gold = FMath::Max(0, InGold);
	bHasAuthoritativeGold = bInHasAuthoritativeGold;
	RefreshDerivedValues();
}

void UFF_PrepBuildTileData::RefreshDerivedValues()
{
	DisplayName = FFF_PrepBuildPresentationRules::ResolveDisplayName(Entry);
	CategoryPathText = FFF_PrepBuildPresentationRules::ResolveCategoryPathText(Entry.Category, Entry.MinorToken);
	CostText = FFF_PrepBuildPresentationRules::ResolveCostText(Entry);
	BuildLimitText = FFF_PrepBuildPresentationRules::ResolveBuildLimitText(Entry);
	TileState = FFF_PrepBuildPresentationRules::ResolveTileState(
		Entry, Gold, bHasAuthoritativeGold);
	StateLabelText = FFF_PrepBuildPresentationRules::ResolveTileStateLabel(TileState);
	StateText = FFF_PrepBuildPresentationRules::ResolveTileStateText(TileState);
}

