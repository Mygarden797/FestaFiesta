// FF_PrepBuildTileData.cpp
// 역할: 시설 카드에 표시할 정보를 담고 그림 로딩 상태를 관리한다.

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
	// Required facilities explain the next step without repeating an installation ratio.
	// Generic finite facilities retain their capacity indicator.
	StateLabelText = Entry.bRequiredForSetup
		? (Entry.bRequiredSetupSatisfied
			? NSLOCTEXT("FestaFiestaPrepBuild", "RequiredInstalledLabel", "설치 완료")
			: FText::Format(NSLOCTEXT("FestaFiestaPrepBuild", "RequiredInstallLabel", "{0}개 설치"),
				FText::AsNumber(FMath::Max(1, Entry.RequiredMinimumCount))))
		: Entry.MaxInstancesPerWorld > 0
		? FText::Format(NSLOCTEXT("FestaFiestaPrepBuild", "InstalledCountLabel", "{0}/{1}"),
			FText::AsNumber(FMath::Max(0, Entry.CurrentInstanceCount)), FText::AsNumber(Entry.MaxInstancesPerWorld))
		: FFF_PrepBuildPresentationRules::ResolveTileStateLabel(TileState);
	StateText = FFF_PrepBuildPresentationRules::ResolveTileStateText(TileState);
	if (TileState == EFF_PrepBuildTileState::BuildLimitReached)
	{
		// A compact lock belongs on the card; the full explanation is used by click feedback and tooltips.
		StateLabelText = NSLOCTEXT("FestaFiestaPrepBuild", "BuildLimitLockedLabel", "잠김");
		StateText = FText::Format(NSLOCTEXT("FestaFiestaPrepBuild", "BuildLimitLockedReason",
			"{0}: 건축 한도에 도달했습니다. (설치 {1}/{2})\n기존 시설을 철거하면 다시 건축할 수 있습니다."),
			DisplayName, FText::AsNumber(FMath::Max(0, Entry.CurrentInstanceCount)),
			FText::AsNumber(Entry.MaxInstancesPerWorld));
	}
}

