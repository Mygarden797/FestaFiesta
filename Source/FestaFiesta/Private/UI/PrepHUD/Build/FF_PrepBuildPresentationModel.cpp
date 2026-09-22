// 카탈로그 스냅샷을 UTileView 목록 객체로 조정하는 프레젠테이션 모델 구현.

#include "UI/PrepHUD/Build/FF_PrepBuildPresentationModel.h"

#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"

void UFF_PrepBuildPresentationModel::Reconcile(
	const EFF_BuildCatalogLoadState LoadState,
	const TArray<FFF_BuildCatalogEntry>& Entries,
	const FFF_PrepBuildFilter& Filter,
	const int32 Gold,
	const bool bHasAuthoritativeGold,
	const FText& FailureText)
{
	VisibleTiles.Reset();

	TSet<FName> ActiveIds;
	ActiveIds.Reserve(Entries.Num());

	for (const FFF_BuildCatalogEntry& Entry : Entries)
	{
		const FName FacilityId = Entry.FacilityDefinitionId;
		// 무효 ID와 중복 ID는 클릭 결과가 모호하므로 첫 번째 유효 항목만 유지합니다.
		if (FacilityId.IsNone() || ActiveIds.Contains(FacilityId))
		{
			continue;
		}
		ActiveIds.Add(FacilityId);

		TObjectPtr<UFF_PrepBuildTileData>& Tile = TilePool.FindOrAdd(FacilityId);
		if (!Tile)
		{
			Tile = NewObject<UFF_PrepBuildTileData>(this);
		}
		Tile->Initialize(Entry, Gold, bHasAuthoritativeGold);
		Tile->SetSynergyHighlighted(SynergyHighlightIds.Contains(FacilityId));
		Tile->SetCodexFocused(FacilityId == CodexFocusId);

		// 로딩/실패 중에는 이전 프레임 목록을 눌러 배치하지 못하게 본문을 비웁니다.
		if (LoadState == EFF_BuildCatalogLoadState::Ready
			&& FFF_PrepBuildPresentationRules::IsEntryVisible(Entry, Filter))
		{
			VisibleTiles.Add(Tile);
		}
	}

	// 카탈로그에서 사라진 시설만 풀에서 제거합니다. 단순 필터 변경은 identity를 보존합니다.
	for (auto Iterator = TilePool.CreateIterator(); Iterator; ++Iterator)
	{
		if (!ActiveIds.Contains(Iterator.Key()))
		{
			Iterator.RemoveCurrent();
		}
	}

	// Opening the unfiltered catalog should expose setup work immediately. Explicit
	// category/search/favorite choices retain their existing ordering and membership.
	const bool bPrioritizeRequired = !Filter.bFilterByCategory && Filter.MinorToken.IsNone() &&
		!Filter.bFavoritesOnly && Filter.SearchText.TrimStartAndEnd().IsEmpty();
	if (bPrioritizeRequired || !SynergyHighlightIds.IsEmpty())
	{
		VisibleTiles.StableSort([bPrioritizeRequired](const UFF_PrepBuildTileData& A, const UFF_PrepBuildTileData& B)
		{
			if (bPrioritizeRequired)
			{
				if (A.IsRequiredFacility() != B.IsRequiredFacility()) return A.IsRequiredFacility();
				if (A.IsRequiredFacility() && A.IsRequiredFacilitySatisfied() != B.IsRequiredFacilitySatisfied())
					return !A.IsRequiredFacilitySatisfied();
			}
			// Keep existing recommendation priority within each requirement group;
			// equal priorities retain source order and their pooled UObject identities.
			return A.IsSynergyHighlighted() && !B.IsSynergyHighlighted();
		});
	}

	Snapshot = FFF_PrepBuildPresentationRules::BuildSnapshot(
		LoadState,
		ActiveIds.Num(),
		VisibleTiles.Num(),
		FailureText);
	OnPresentationChanged.Broadcast();
}

void UFF_PrepBuildPresentationModel::SetSynergyHighlightIds(const TArray<FName>& DefinitionIds)
{
	SynergyHighlightIds.Reset();
	for (const FName Id : DefinitionIds)
	{
		if (!Id.IsNone()) { SynergyHighlightIds.Add(Id); }
	}
}

void UFF_PrepBuildPresentationModel::SetCodexFocusId(const FName DefinitionId)
{
	CodexFocusId = DefinitionId;
	for (const auto& Pair : TilePool)
	{
		if (Pair.Value) { Pair.Value->SetCodexFocused(!DefinitionId.IsNone() && Pair.Key == DefinitionId); }
	}
}

void UFF_PrepBuildPresentationModel::RefreshGold(
	const int32 Gold,
	const bool bHasAuthoritativeGold)
{
	for (const TPair<FName, TObjectPtr<UFF_PrepBuildTileData>>& Pair : TilePool)
	{
		if (Pair.Value)
		{
			Pair.Value->RefreshGold(Gold, bHasAuthoritativeGold);
		}
	}
	OnPresentationChanged.Broadcast();
}

TArray<UObject*> UFF_PrepBuildPresentationModel::GetListItems() const
{
	TArray<UObject*> Result;
	Result.Reserve(VisibleTiles.Num());
	for (UFF_PrepBuildTileData* Tile : VisibleTiles)
	{
		if (Tile)
		{
			Result.Add(Tile);
		}
	}
	return Result;
}

UFF_PrepBuildTileData* UFF_PrepBuildPresentationModel::FindTile(
	const FName FacilityDefinitionId) const
{
	const TObjectPtr<UFF_PrepBuildTileData>* Found = TilePool.Find(FacilityDefinitionId);
	return Found ? Found->Get() : nullptr;
}

