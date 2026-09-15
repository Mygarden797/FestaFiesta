// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/Atlas/FF_AtlasGeo.h"
#include "FF_FestivalAtlasCatalog.generated.h"

class UFF_FestivalDefinition;
class UStaticMesh;
class UTexture2D;
class UWorld;

/**
 * 지구본에 조형물 하나로 서는 **국가** 한 개.
 *
 * === 왜 국가를 따로 두는가 (2026-08-21) ===
 * 이전 카탈로그는 축제 단위 목록이고 국가는 항목의 `CountryName`(FText) 이었다. 그래서
 * **"국가당 조형물 1개 + 축제 개수 배지"** 를 만들 수 없었다 — 국가 하나에 무엇이 있는지 아는
 * 곳이 없고, 조형물을 어느 축제에 붙일지도 정할 수 없다.
 *
 * === 🔴 왜 `FText CountryName` 으로 묶지 않는가 ===
 * `FText` 는 **현지화 대상**이다. 언어를 바꾸면 `대한민국` 이 `Korea` 가 되고, 그것으로
 * 그룹핑하던 코드는 조용히 국가를 둘로 쪼갠다. 조인 키는 언어와 무관해야 하므로 `FName` 이다.
 * 화면에 나가는 이름은 `DisplayName` 이 따로 갖는다.
 *
 * === 없는 에셋이 기본값이다 ===
 * 조형물 메시·국가 지도 이미지·프리뷰가 아직 하나도 없다. **비어 있어도 동작해야 한다.**
 * 조형물은 프리미티브(큐브·원뿔)로 대신 세우고, 국가 지도는 네모 박스로 대신 그린다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_AtlasCountryEntry
{
	GENERATED_BODY()

	/** 조인 키. 축제 항목의 `CountryId` 와 같은 값을 넣는다. 현지화되지 않는 이름이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName CountryId;

	/** 화면에 나가는 국가 이름. `대한민국` 처럼 현지화 대상이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	/** 지구본 소개 문구. 국가 선택 화면 왼쪽 패널의 짧은 소개다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText Description;

	/** 국가 대표 이미지. 없으면 칸을 숨긴다 (`0` 이나 `-` 를 넣지 않는다). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> PreviewImage;

	/**
	 * ★ **지구본 핀 위치는 국가가 갖는다.** 조형물이 국가당 하나이므로 축제가 아니라 여기다.
	 *
	 * 축제 하나짜리 국가라면 그 축제 자리를 써도 되고, 여러 개면 나라의 중심을 쓴다.
	 * 비워 두면 (0, 0) — **기니 만 앞바다**에 핀이 찍힌다. 그래서 에디터에서 경고를 남긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Globe")
	FFF_AtlasLatLong Location;

	/**
	 * 지구본에 서는 국가 대표 조형물. 현재 대표 축제를 상징한다.
	 * **비어 있어도 동작해야 한다** — 액터가 프리미티브로 대신 세운다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Globe")
	TSoftObjectPtr<UStaticMesh> LandmarkMesh;

	/** 국가별 조형물 높이. 0이면 지구 액터의 공통 높이를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Globe", meta = (ClampMin = "0.0"))
	float LandmarkHeightOverride = 0.f;

	/** 넓은 건물이 작은 나라 전체를 덮지 않도록 폭·깊이를 함께 제한한다. 0은 제한 없음. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Globe", meta = (ClampMin = "0.0"))
	float LandmarkMaxWidth = 0.f;

	/** 표면 수직축을 기준으로 돌리는 방향과 지표 돌출 보정(uu). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Globe")
	float LandmarkHeading = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Globe", meta = (ClampMin = "0.0"))
	float LandmarkSurfaceOffset = 0.f;

	/**
	 * 국가 지도 배경.
	 *
	 * 🔴 **알파가 있는 이미지를 넣는다.** 바다까지 칠해진 불투명 합성본을 넣으면 배경 위에
	 * 지도 사각형의 경계선이 그대로 보인다 (2026-08-27에 실제로 그랬다). 육지만 남기고
	 * 나머지는 투명해야 화면의 바다 배경과 이어진다.
	 *
	 * 비어 있으면 국가 지도 화면이 네모 박스 + 팔레트 색으로 대신 그린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Country Map")
	TSoftObjectPtr<UTexture2D> MapImage;

	/**
	 * 행정구역 경계선 레이어. `MapImage` 위에 같은 사각형으로 겹쳐 그린다.
	 *
	 * ★ **`MapImage` 와 좌표계·크기가 같은 이미지여야 한다.** 지형에 경계를 구워 넣지 않고
	 *   나누는 이유는, 경계 두께·색을 지형을 다시 만들지 않고 바꿀 수 있어서다.
	 *   비어 있으면 경계 없이 지형만 그린다 — 없다고 화면이 죽지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Country Map")
	TSoftObjectPtr<UTexture2D> MapBordersImage;

	/** 고를 수 있는지. 잠긴 국가는 자물쇠 + `LockReason` 을 함께 띄우고 진입을 막는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lock")
	bool bUnlocked = false;

	/**
	 * 잠긴 이유. `준비 중` 처럼 화면에 그대로 나가는 문구다.
	 * **색만으로 잠금을 알리지 않는다** (AT-011). 자물쇠 아이콘과 이 문구를 함께 쓴다.
	 * 잠긴 핀에도 **포커스는 간다** — 선택만 막고 이 문구를 띄운다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lock")
	FText LockReason;
};

/**
 * Festival Atlas 에 뜨는 축제 한 개.
 *
 * === 이 구조체가 존재하는 이유 ===
 * `UFF_FestivalDefinition`(김시리님 소유)에는 **레벨 참조가 없다.** 운영 일수·초기 자원·매출·
 * 홍보·뒷정리·피날레 값만 있다. 지구본에 핀을 찍고 "이 축제를 시작" 을 눌렀을 때 어느 레벨을
 * 열어야 하는지 아는 곳이 없었다.
 *
 * 남의 데이터 에셋에 필드를 임의로 넣지 않으므로(AGENTS.md 규칙 3) UI 가 소유하는 카탈로그를
 * 따로 둔다. `FestivalId` 로 김시리님 정의와 이어 붙인다. U3 Atlas 가 국가·지역·축제 카드를
 * 그리려면 어차피 필요한 데이터이므로 새로 늘어나는 개념이 아니다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FestivalAtlasEntry
{
	GENERATED_BODY()

	/** `UFF_FestivalDefinition::FestivalId` 와 같은 값을 넣는다. 두 데이터를 잇는 키다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName FestivalId;

	/**
	 * 어느 국가에 속하나. `FFF_AtlasCountryEntry::CountryId` 와 같은 값이다.
	 *
	 * 비워 두면 지구본·국가 지도에 **나타나지 않는다.** 단 `FF.Flow.Start` 나 카탈로그 직접
	 * 조회로 시작하는 경로는 그대로 동작한다 — 국가 개념이 들어오기 전에 만든 항목이
	 * 갑자기 못 열리게 되지 않도록 일부러 그렇게 둔다. 에디터에서 경고만 남긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName CountryId;

	/** 운영 규칙과 밸런스. 비어 있으면 `UFF_FestivalSettings::DefaultFestival` 을 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	TSoftObjectPtr<UFF_FestivalDefinition> Definition;

	/**
	 * ★ **여기가 레벨을 넣는 칸이다.** 축제를 시작하면 이 레벨을 연다.
	 *
	 * `FName` 으로 레벨 이름을 문자열로 들지 않는다. 두 가지가 깨진다 —
	 * 에디터에서 에셋 선택창이 안 나오고, **쿠킹 목록에 안 들어가 패키징에서 조용히 빠진다.**
	 * 소프트 참조는 참조만으로 쿠킹에 잡힌다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (DisplayName = "Festival Level"))
	TSoftObjectPtr<UWorld> Level;

	// --- 화면에 나가는 문구. 전부 FText 다 (현지화 대상) ---

	/**
	 * 지역 이름. `부산` 처럼 국가 지도 위에 라벨로 띄운다.
	 * **지역 선택 단계는 만들지 않는다** — 라벨만 보여주고 핀은 축제를 가리킨다 (2026-08-21 확정).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText RegionName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText FestivalName;

	/** 국가 지도의 축제 카드에 들어가는 한 줄 설명. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText Description;

	/** 국가 지도 축제 카드 프리뷰. 없으면 칸을 숨긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> PreviewImage;

	// --- 국가 지도 위 위치 ---

	/**
	 * ★ 국가 지도 위 핀 위치. **0~1 정규화 좌표다** — 픽셀이 아니다.
	 * `(0, 0)` 이 이미지 좌상단, `(1, 1)` 이 우하단이다.
	 *
	 * 픽셀로 두면 해상도·UI 배율·레터박스가 바뀔 때마다 핀이 어긋난다. 정규화해 두면
	 * 화면이 얼마나 커지든 이미지가 실제로 그려진 사각형에 다시 매핑하기만 하면 된다.
	 * **"지도 비율이 바뀌어도 핀 위치 유지" 의 구현이 이 한 줄이다.**
	 *
	 * 지구본 핀은 여기가 아니라 국가의 `Location`(위경도) 이 갖는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Country Map", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D MapPosition = FVector2D(0.5, 0.5);

	/**
	 * 이 축제를 가리킬 때 지도 위에 겹치는 선택 오버레이.
	 *
	 * 부산 축제라면 부산 ADM1만 들어 있는 투명 PNG를 넣는다. 영남권 전체를 공통 코드에서
	 * 칠하지 않는다. 축제마다 데이터로 들기 때문에 다른 나라·도시가 추가되어도 화면 코드는
	 * 같은 방식으로 해당 오버레이를 읽는다. 비어 있으면 선택 핀만 강조한다.
	 * `MapImage`와 정확히 같은 캔버스·Crop·비율을 사용해야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Country Map")
	TSoftObjectPtr<UTexture2D> SelectionOverlayImage;

	// --- 카드에 표시하는 값 ---

	/**
	 * 난이도. 별 개수로 그린다.
	 * `assumption` — 산정 기준이 아직 없다. 화면에 `잠정` 을 함께 띄운다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card", meta = (ClampMin = "1", ClampMax = "5"))
	int32 Difficulty = 1;

	/**
	 * 운영 일수. **표시용 사본이다.** 진짜 값은 `Definition` 이 갖는다.
	 * 카드를 그릴 때 정의 에셋을 동기 로드하지 않으려고 여기 둔다. 둘이 다르면 정의가 맞다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Card", meta = (ClampMin = "1"))
	int32 OperatingDays = 3;

	// --- 잠금 ---

	/** 고를 수 있는지. 잠긴 축제는 자물쇠 자리 + LockReason 을 함께 띄운다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lock")
	bool bUnlocked = false;

	/**
	 * 잠긴 이유. `준비 중` 처럼 화면에 그대로 나가는 문구다.
	 * **색만으로 잠금을 알리지 않는다** (AT-011). 자물쇠 아이콘과 이 문구를 함께 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lock")
	FText LockReason;
};

/**
 * 지구본과 국가 지도가 읽는 국가·축제 목록.
 *
 * 에디터에서 `DA_FestivalAtlas` 를 만들고 `UFF_UIFlowSettings` 에 지정한다.
 * **다대포 한 줄만 채우면 타이틀에서 축제까지 한 바퀴가 돈다.**
 *
 * === 국가와 축제를 한 에셋에 두는 이유 ===
 * 국가 에셋을 따로 쪼개면 참조가 양방향으로 늘고, 지금 국가가 하나뿐이라 빈 껍데기만 생긴다.
 * 두 배열을 `CountryId` 로 잇는 편이 조회도 검증도 한 파일에서 끝난다. 국가가 수십 개로
 * 늘면 그때 국가만 별도 에셋으로 뽑는다 — 그때도 조인 키는 그대로 `CountryId` 다.
 */
UCLASS(BlueprintType, meta = (DisplayName = "FF Festival Atlas Catalog"))
class FESTAFIESTA_API UFF_FestivalAtlasCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// --- 축제 조회 ---

	/** 못 찾으면 nullptr. 호출부는 화면에 `축제 목록이 비어 있습니다` 를 띄운다. */
	const FFF_FestivalAtlasEntry* FindEntry(FName InFestivalId) const;

	/** 잠기지 않은 첫 항목. 카탈로그가 비었거나 전부 잠겨 있으면 nullptr. */
	const FFF_FestivalAtlasEntry* FindFirstUnlocked() const;

	// --- 국가 조회 ---

	/** 못 찾으면 nullptr. `CountryId` 가 비면 항상 nullptr 다 (빈 이름은 국가가 아니다). */
	const FFF_AtlasCountryEntry* FindCountry(FName InCountryId) const;

	/** 지구본에 핀을 찍는 순서가 곧 이 배열 순서다. */
	const TArray<FFF_AtlasCountryEntry>& GetCountries() const { return Countries; }

	/**
	 * 이 국가에 속한 축제들. 순서는 `Entries` 순서를 유지한다.
	 *
	 * 포인터를 돌려주는 이유는 항목이 `FText` 여섯 개를 갖고 있어 복사가 싸지 않고,
	 * 호출부(지구본·국가 지도 화면)가 카탈로그를 잡고 있는 동안만 쓰기 때문이다.
	 * **카탈로그보다 오래 들고 있지 않는다.**
	 */
	void GetFestivalsInCountry(FName InCountryId, TArray<const FFF_FestivalAtlasEntry*>& OutFestivals) const;

	/** 축제 개수 배지에 쓴다. 목록이 필요 없을 때 배열을 만들지 않으려고 따로 둔다. */
	int32 CountFestivalsInCountry(FName InCountryId) const;

	// --- 데이터 ---

	/**
	 * 지구본에 서는 국가들. **순서가 곧 핀 순서다.**
	 * 좌우 입력의 핀 이동은 이 순서가 아니라 경도로 판정한다 (`FFF_AtlasGeo::FindAdjacentPinIndex`).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas")
	TArray<FFF_AtlasCountryEntry> Countries;

	/** 화면에 그릴 축제 목록. 순서가 곧 표시 순서다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas")
	TArray<FFF_FestivalAtlasEntry> Entries;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/**
	 * 런타임에 "축제가 안 열린다" 로만 드러나는 실수들을 에디터에서 소리 내어 잡는다.
	 * `PostEditChangeProperty` 가 부른다. 검사를 늘릴 때 여기 한 곳만 고친다.
	 */
	void ValidateCatalog() const;
#endif
};
