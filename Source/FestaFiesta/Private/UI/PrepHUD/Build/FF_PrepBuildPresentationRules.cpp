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
		EFFZSFacilityCategory::PhotoSpot
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
	default:
		return LOCTEXT("CategoryUnknown", "기타");
	}
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

FText FFF_PrepBuildPresentationRules::ResolveTileStateText(
	const EFF_PrepBuildTileState TileState)
{
	switch (TileState)
	{
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
