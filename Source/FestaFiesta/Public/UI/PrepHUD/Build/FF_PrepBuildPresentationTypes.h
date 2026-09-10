// Prep HUD 배치 패널이 소비하는 표현 전용 상태와 필터를 정의한다.
// 도메인 카탈로그/경제 상태를 바꾸지 않고 화면이 구분해서 안내해야 할 경우만 담는다.

#pragma once

#include "CoreMinimal.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "FF_PrepBuildPresentationTypes.generated.h"

/** 배치 패널 본문이 현재 보여 줄 최상위 상태입니다. */
UENUM(BlueprintType)
enum class EFF_PrepBuildSurfaceState : uint8
{
	Loading		UMETA(DisplayName = "불러오는 중"),
	Ready		UMETA(DisplayName = "목록 준비됨"),
	Empty		UMETA(DisplayName = "빈 목록"),
	Failed		UMETA(DisplayName = "불러오기 실패")
};

/**
 * 시설 카드의 선택 가능 상태입니다.
 *
 * 색만 흐리게 하지 않고 이 값을 문구·아이콘과 함께 표시할 수 있도록 원인을 분리합니다.
 */
UENUM(BlueprintType)
enum class EFF_PrepBuildTileState : uint8
{
	Available			UMETA(DisplayName = "배치 가능"),
	InsufficientFunds	UMETA(DisplayName = "잔액 부족"),
	PriceUnavailable	UMETA(DisplayName = "가격 미확정"),
	GoldUnavailable		UMETA(DisplayName = "잔액 미확정"),
	BuildLimitReached	UMETA(DisplayName = "건축 한도 도달")
};

/** 카탈로그 원본을 변경하지 않고 적용하는 화면 전용 필터입니다. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PrepBuildFilter
{
	GENERATED_BODY()

	/** false면 모든 대분류를 표시합니다. enum의 특정 값을 '전체'로 재사용하지 않습니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Build")
	bool bFilterByCategory = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Build",
		meta = (EditCondition = "bFilterByCategory"))
	EFFZSFacilityCategory Category = EFFZSFacilityCategory::Booth;

	/** NAME_None이면 모든 소분류를 표시합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Build")
	FName MinorToken = NAME_None;

	/** 표시명·정의 ID·소분류 토큰을 대소문자 구분 없이 검색합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Build")
	FString SearchText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Build")
	bool bFavoritesOnly = false;

	/** Snapshot of personal preferences. Filtering never changes the authoritative catalog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Prep|Build")
	TSet<FName> FavoriteIds;
};

/** 패널이 한 번에 적용할 목록/빈 상태 스냅샷입니다. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PrepBuildPresentationSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Build")
	EFF_PrepBuildSurfaceState SurfaceState = EFF_PrepBuildSurfaceState::Loading;

	/** 로딩·빈 상태·오류일 때 목록 대신 보여 줄 설명입니다. Ready에서는 비어 있습니다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Build")
	FText StatusText;

	/** ID가 유효하고 중복 제거된 카탈로그 항목 수입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Build")
	int32 TotalItemCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Build")
	int32 VisibleItemCount = 0;

	/** Ready이며 필터를 통과한 항목이 있을 때만 true입니다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Build")
	bool bCanInteract = false;
};

