// Prep HUD 배치 패널의 순수 표현 규칙 구현.

#include "UI/PrepHUD/Build/FF_PrepBuildPresentationRules.h"

#define LOCTEXT_NAMESPACE "FFPrepBuildPresentation"

TConstArrayView<EFFZSFacilityCategory> FFF_PrepBuildPresentationRules::GetCategoryOrder()
{
	// 저장 호환 값인 enum을 숫자로 순회하지 않는다. 새 값이 생기면 의도한 표시 순서와
	// 현지화 문구를 함께 검토하도록 목록을 명시하고 자동화 테스트로 고정한다.
	static constexpr EFFZSFacilityCategory Categories[] = {
		EFFZSFacilityCategory::Booth,
		EFFZSFacilityCategory::Toilet,
		EFFZSFacilityCategory::Info,
		EFFZSFacilityCategory::Rest,
		EFFZSFacilityCategory::Performance,
		EFFZSFacilityCategory::Safety,
		EFFZSFacilityCategory::PhotoSpot,
		EFFZSFacilityCategory::Lighting,
		EFFZSFacilityCategory::Road,
		EFFZSFacilityCategory::Tile
	};
	return Categories;
}

FText FFF_PrepBuildPresentationRules::ResolveCategoryDisplayName(
	const EFFZSFacilityCategory Category)
{
	switch (Category)
	{
	case EFFZSFacilityCategory::Booth:
		return LOCTEXT("CategoryBooth", "부스");
	case EFFZSFacilityCategory::Toilet:
		return LOCTEXT("CategoryToilet", "화장실");
	case EFFZSFacilityCategory::Info:
		return LOCTEXT("CategoryInfo", "안내");
	case EFFZSFacilityCategory::Rest:
		return LOCTEXT("CategoryRest", "휴게");
	case EFFZSFacilityCategory::Performance:
		return LOCTEXT("CategoryPerformance", "공연");
	case EFFZSFacilityCategory::Safety:
		return LOCTEXT("CategorySafety", "안전");
	case EFFZSFacilityCategory::PhotoSpot:
		return LOCTEXT("CategoryPhotoSpot", "포토스팟");
	case EFFZSFacilityCategory::Lighting:
		return LOCTEXT("CategoryLighting", "조명");
	case EFFZSFacilityCategory::Road:
		return LOCTEXT("CategoryRoad", "길");
	case EFFZSFacilityCategory::Tile:
		return LOCTEXT("CategoryTile", "타일");
	default:
		return LOCTEXT("CategoryUnknown", "기타");
	}
}

FText FFF_PrepBuildPresentationRules::ResolveMinorDisplayName(const FName MinorToken)
{
	static const TMap<FName, FText> Labels = {
		{ TEXT("Surface"), LOCTEXT("MinorSurface", "바닥") },
		{ TEXT("FoodStall"), LOCTEXT("MinorFoodStall", "먹거리·음료") },
		{ TEXT("MarketGoods"), LOCTEXT("MinorMarketGoods", "상품 판매") },
		{ TEXT("CraftExperience"), LOCTEXT("MinorCraftExperience", "공예·제작 체험") },
		{ TEXT("ActivityGame"), LOCTEXT("MinorActivityGame", "게임·활동 체험") },
		{ TEXT("Nightlife"), LOCTEXT("MinorNightlife", "야간·주점") },
		{ TEXT("Standard"), LOCTEXT("MinorStandard", "일반") },
		{ TEXT("Accessible"), LOCTEXT("MinorAccessible", "장애인 화장실") },
		{ TEXT("Family"), LOCTEXT("MinorFamily", "가족") },
		{ TEXT("Temporary"), LOCTEXT("MinorTemporary", "임시") },
		{ TEXT("GeneralInfo"), LOCTEXT("MinorGeneralInfo", "종합 안내") },
		{ TEXT("Ticketing"), LOCTEXT("MinorTicketing", "티켓") },
		{ TEXT("LostFound"), LOCTEXT("MinorLostFound", "분실물") },
		{ TEXT("Translation"), LOCTEXT("MinorTranslation", "통역") },
		{ TEXT("Seating"), LOCTEXT("MinorSeating", "좌석") },
		{ TEXT("Shade"), LOCTEXT("MinorShade", "그늘") },
		{ TEXT("Picnic"), LOCTEXT("MinorPicnic", "피크닉") },
		{ TEXT("Cooling"), LOCTEXT("MinorCooling", "무더위 쉼터") },
		{ TEXT("MainStage"), LOCTEXT("MinorMainStage", "메인 무대") },
		{ TEXT("SubStage"), LOCTEXT("MinorSubStage", "보조 무대") },
		{ TEXT("Busking"), LOCTEXT("MinorBusking", "버스킹") },
		{ TEXT("ExperienceStage"), LOCTEXT("MinorExperienceStage", "참여형 무대") },
		{ TEXT("FirstAid"), LOCTEXT("MinorFirstAid", "응급 처치") },
		{ TEXT("Security"), LOCTEXT("MinorSecurity", "보안") },
		{ TEXT("FireSafety"), LOCTEXT("MinorFireSafety", "소방") },
		{ TEXT("Evacuation"), LOCTEXT("MinorEvacuation", "대피") },
		{ TEXT("Landmark"), LOCTEXT("MinorLandmark", "랜드마크") },
		{ TEXT("Theme"), LOCTEXT("MinorTheme", "테마") },
		{ TEXT("Scenic"), LOCTEXT("MinorScenic", "경관") },
		{ TEXT("Interactive"), LOCTEXT("MinorInteractive", "상호작용") },
		{ TEXT("Decorative"), LOCTEXT("MinorDecorative", "장식 조명") },
		{ TEXT("Pendant"), LOCTEXT("MinorPendant", "펜던트 조명") },
		{ TEXT("Streetlamp"), LOCTEXT("MinorStreetlamp", "가로등") }
	};
	if (const FText* Label = Labels.Find(MinorToken))
	{
		return *Label;
	}
	return MinorToken.IsNone() ? LOCTEXT("MinorUnspecified", "미분류") : FText::FromName(MinorToken);
}

FText FFF_PrepBuildPresentationRules::ResolveCategoryPathText(
	const EFFZSFacilityCategory Category, const FName MinorToken)
{
	return FText::Format(LOCTEXT("CategoryPath", "{0} > {1}"),
		ResolveCategoryDisplayName(Category), ResolveMinorDisplayName(MinorToken));
}

bool FFF_PrepBuildPresentationRules::IsEntryVisible(
	const FFF_BuildCatalogEntry& Entry,
	const FFF_PrepBuildFilter& Filter)
{
	// 배치 명령의 유일한 키가 없으면 화면에 눌러도 수행할 수 없는 카드를 만들지 않습니다.
	if (Entry.FacilityDefinitionId.IsNone())
	{
		return false;
	}

	if (Filter.bFilterByCategory && Entry.Category != Filter.Category)
	{
		return false;
	}

	if (Filter.bFavoritesOnly && !Filter.FavoriteIds.Contains(Entry.FacilityDefinitionId))
	{
		return false;
	}

	if (!Filter.MinorToken.IsNone() && Entry.MinorToken != Filter.MinorToken)
	{
		return false;
	}

	const FString Search = Filter.SearchText.TrimStartAndEnd();
	if (Search.IsEmpty())
	{
		return true;
	}

	return Entry.DisplayName.ToString().Contains(Search, ESearchCase::IgnoreCase)
		|| ResolveCategoryDisplayName(Entry.Category).ToString().Contains(Search, ESearchCase::IgnoreCase)
		|| ResolveMinorDisplayName(Entry.MinorToken).ToString().Contains(Search, ESearchCase::IgnoreCase)
		|| Entry.UniqueNameToken.Contains(Search, ESearchCase::IgnoreCase)
		|| Entry.FacilityDefinitionId.ToString().Contains(Search, ESearchCase::IgnoreCase)
		|| Entry.MinorToken.ToString().Contains(Search, ESearchCase::IgnoreCase);
}

FText FFF_PrepBuildPresentationRules::ResolveDisplayName(const FFF_BuildCatalogEntry& Entry)
{
	if (!Entry.DisplayName.IsEmptyOrWhitespace())
	{
		return Entry.DisplayName;
	}
	if (!Entry.UniqueNameToken.IsEmpty())
	{
		return FText::FromString(Entry.UniqueNameToken);
	}
	return Entry.FacilityDefinitionId.IsNone()
		? LOCTEXT("UnnamedFacility", "이름 없는 시설")
		: FText::FromName(Entry.FacilityDefinitionId);
}

EFF_PrepBuildTileState FFF_PrepBuildPresentationRules::ResolveTileState(
	const FFF_BuildCatalogEntry& Entry,
	const int32 Gold,
	const bool bHasAuthoritativeGold)
{
	// 한도 도달은 잔액을 바꿔도 해소되지 않으므로 가격 상태보다 먼저 명확한 원인으로 노출합니다.
	if (Entry.bBuildLimitReached)
	{
		return EFF_PrepBuildTileState::BuildLimitReached;
	}

	// 가격과 잔액 중 어느 값을 못 읽었는지 보존해야 UI가 잘못된 해결 방법을 안내하지 않습니다.
	if (!Entry.bHasResolvedInstallCost || Entry.InstallCost <= 0)
	{
		return EFF_PrepBuildTileState::PriceUnavailable;
	}
	if (!bHasAuthoritativeGold)
	{
		return EFF_PrepBuildTileState::GoldUnavailable;
	}

	return FMath::Max(0, Gold) >= Entry.InstallCost
		? EFF_PrepBuildTileState::Available
		: EFF_PrepBuildTileState::InsufficientFunds;
}

FText FFF_PrepBuildPresentationRules::ResolveCostText(const FFF_BuildCatalogEntry& Entry)
{
	if (!Entry.bHasResolvedInstallCost || Entry.InstallCost <= 0)
	{
		return LOCTEXT("PricePending", "가격 확인 중");
	}

	return FText::Format(
		LOCTEXT("InstallCostFormat", "{0} Gold"),
		FText::AsNumber(Entry.InstallCost));
}

FText FFF_PrepBuildPresentationRules::ResolveBuildLimitText(
	const FFF_BuildCatalogEntry& Entry)
{
	if (Entry.MaxInstancesPerWorld <= 0)
	{
		return LOCTEXT("UnlimitedBuildCount", "설치 제한 없음");
	}

	return FText::Format(
		LOCTEXT("LimitedBuildCount", "설치 {0}/{1}"),
		FText::AsNumber(FMath::Max(0, Entry.CurrentInstanceCount)),
		FText::AsNumber(Entry.MaxInstancesPerWorld));
}

FText FFF_PrepBuildPresentationRules::ResolveTileStateLabel(
	const EFF_PrepBuildTileState TileState)
{
	switch (TileState)
	{
	case EFF_PrepBuildTileState::Available:
		return LOCTEXT("AvailableLabel", "배치 가능");
	case EFF_PrepBuildTileState::BuildLimitReached:
		return LOCTEXT("BuildLimitReachedLabel", "한도 도달");
	case EFF_PrepBuildTileState::InsufficientFunds:
		return LOCTEXT("InsufficientFundsLabel", "잔액 부족");
	case EFF_PrepBuildTileState::PriceUnavailable:
		return LOCTEXT("PriceUnavailableLabel", "가격 미확정");
	case EFF_PrepBuildTileState::GoldUnavailable:
		return LOCTEXT("GoldUnavailableLabel", "잔액 확인 중");
	default:
		return FText::GetEmpty();
	}
}

FText FFF_PrepBuildPresentationRules::ResolveTileStateText(
	const EFF_PrepBuildTileState TileState)
{
	switch (TileState)
	{
	case EFF_PrepBuildTileState::BuildLimitReached:
		return LOCTEXT("BuildLimitReached", "이 시설의 건축 한도에 도달했습니다.");
	case EFF_PrepBuildTileState::InsufficientFunds:
		return LOCTEXT("InsufficientFunds", "잔액이 부족합니다.");
	case EFF_PrepBuildTileState::PriceUnavailable:
		return LOCTEXT("PriceUnavailable", "설치비를 확인할 수 없습니다.");
	case EFF_PrepBuildTileState::GoldUnavailable:
		return LOCTEXT("GoldUnavailable", "잔액을 확인하는 중입니다.");
	default:
		return FText::GetEmpty();
	}
}

FFF_PrepBuildPresentationSnapshot FFF_PrepBuildPresentationRules::BuildSnapshot(
	const EFF_BuildCatalogLoadState LoadState,
	const int32 TotalItemCount,
	const int32 VisibleItemCount,
	const FText& FailureText)
{
	FFF_PrepBuildPresentationSnapshot Result;
	Result.TotalItemCount = FMath::Max(0, TotalItemCount);
	Result.VisibleItemCount = FMath::Clamp(VisibleItemCount, 0, Result.TotalItemCount);

	switch (LoadState)
	{
	case EFF_BuildCatalogLoadState::NotRequested:
		Result.SurfaceState = EFF_PrepBuildSurfaceState::Loading;
		Result.StatusText = LOCTEXT("CatalogPreparing", "시설 목록을 준비하고 있습니다.");
		break;

	case EFF_BuildCatalogLoadState::Loading:
		Result.SurfaceState = EFF_PrepBuildSurfaceState::Loading;
		Result.StatusText = LOCTEXT("CatalogLoading", "시설 목록을 불러오는 중입니다.");
		break;

	case EFF_BuildCatalogLoadState::Failed:
		Result.SurfaceState = EFF_PrepBuildSurfaceState::Failed;
		Result.StatusText = FailureText.IsEmptyOrWhitespace()
			? LOCTEXT("CatalogFailed", "시설 목록을 불러오지 못했습니다. 다시 시도해 주세요.")
			: FailureText;
		break;

	case EFF_BuildCatalogLoadState::Ready:
		if (Result.TotalItemCount == 0)
		{
			Result.SurfaceState = EFF_PrepBuildSurfaceState::Empty;
			Result.StatusText = LOCTEXT("CatalogEmpty", "배치할 수 있는 시설이 없습니다.");
		}
		else if (Result.VisibleItemCount == 0)
		{
			Result.SurfaceState = EFF_PrepBuildSurfaceState::Empty;
			Result.StatusText = LOCTEXT("FilterEmpty", "조건에 맞는 시설이 없습니다.");
		}
		else
		{
			Result.SurfaceState = EFF_PrepBuildSurfaceState::Ready;
			Result.StatusText = FText::GetEmpty();
			Result.bCanInteract = true;
		}
		break;
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
