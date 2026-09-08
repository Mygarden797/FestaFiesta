// Prep HUD 배치 패널의 필터·가격 상태·빈 상태 판단을 순수 함수로 제공한다.

#pragma once

#include "CoreMinimal.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationTypes.h"

/** 에디터나 위젯 없이 자동화 테스트할 수 있는 배치 패널 표현 규칙입니다. */
struct FESTAFIESTA_API FFF_PrepBuildPresentationRules
{
	/** ZoneSystem이 지원하는 일곱 대분류를 카탈로그와 같은 결정적 순서로 제공합니다. */
	static TConstArrayView<EFFZSFacilityCategory> GetCategoryOrder();

	/** 카테고리 칩에 사용하는 짧은 현지화 표기입니다. */
	static FText ResolveCategoryDisplayName(EFFZSFacilityCategory Category);
	static FText ResolveMinorDisplayName(FName MinorToken);
	static FText ResolveCategoryPathText(EFFZSFacilityCategory Category, FName MinorToken);

	/** 유효 ID, 대분류, 소분류, 검색어를 모두 만족하는지 판정합니다. */
	static bool IsEntryVisible(
		const FFF_BuildCatalogEntry& Entry,
		const FFF_PrepBuildFilter& Filter);

	/** 표시명이 비었을 때 UniqueNameToken, 정의 ID 순으로 안전하게 대체합니다. */
	static FText ResolveDisplayName(const FFF_BuildCatalogEntry& Entry);

	/** 가격과 권위 잔액을 서로 다른 미확정 상태로 보존합니다. */
	static EFF_PrepBuildTileState ResolveTileState(
		const FFF_BuildCatalogEntry& Entry,
		int32 Gold,
		bool bHasAuthoritativeGold);

	/** 가격을 표시할 수 없을 때 0 Gold라고 거짓 표시하지 않습니다. */
	static FText ResolveCostText(const FFF_BuildCatalogEntry& Entry);

	/** 화면 투영 시점의 현재 수와 Data Asset의 정적 상한을 표시합니다. 0 상한은 제한 없음입니다. */
	static FText ResolveBuildLimitText(const FFF_BuildCatalogEntry& Entry);

	/** 고정 크기 카드에 한 줄로 표시할 짧은 상태 라벨입니다. */
	static FText ResolveTileStateLabel(EFF_PrepBuildTileState TileState);

	/** 툴팁과 패널 상태 영역에 표시할 상세 비활성 원인입니다. Available이면 빈 텍스트입니다. */
	static FText ResolveTileStateText(EFF_PrepBuildTileState TileState);

	/** 카탈로그 로드 상태와 필터 결과를 패널 최상위 상태 하나로 합칩니다. */
	static FFF_PrepBuildPresentationSnapshot BuildSnapshot(
		EFF_BuildCatalogLoadState LoadState,
		int32 TotalItemCount,
		int32 VisibleItemCount,
		const FText& FailureText);
};
