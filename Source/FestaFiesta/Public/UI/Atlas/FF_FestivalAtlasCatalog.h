// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FF_FestivalAtlasCatalog.generated.h"

class UFF_FestivalDefinition;
class UTexture2D;
class UWorld;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText CountryName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText RegionName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText FestivalName;

	/** 국가 지도의 축제 카드에 들어가는 한 줄 설명. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText Description;

	/** 지구본·국가 지도 프리뷰. 없으면 칸을 숨긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	TSoftObjectPtr<UTexture2D> PreviewImage;

	// --- 지구본 핀 위치 ---

	/** 위도. -90 ~ 90. 지구본 표면에 핀을 놓는 데 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Globe", meta = (ClampMin = "-90.0", ClampMax = "90.0"))
	float Latitude = 0.f;

	/** 경도. -180 ~ 180. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Globe", meta = (ClampMin = "-180.0", ClampMax = "180.0"))
	float Longitude = 0.f;

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
 * 지구본과 국가 지도가 읽는 축제 목록.
 *
 * 에디터에서 `DA_FestivalAtlas` 를 만들고 `UFF_UIFlowSettings` 에 지정한다.
 * **다대포 한 줄만 채우면 타이틀에서 축제까지 한 바퀴가 돈다.**
 *
 * 국가·지역은 따로 에셋을 두지 않고 항목의 `CountryName` · `RegionName` 으로 묶는다.
 * 축제가 수십 개로 늘면 그때 국가 에셋을 분리한다 — 지금 미리 나누면 빈 껍데기만 늘어난다.
 */
UCLASS(BlueprintType, meta = (DisplayName = "FF Festival Atlas Catalog"))
class FESTAFIESTA_API UFF_FestivalAtlasCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 못 찾으면 nullptr. 호출부는 화면에 `축제 목록이 비어 있습니다` 를 띄운다. */
	const FFF_FestivalAtlasEntry* FindEntry(FName InFestivalId) const;

	/** 잠기지 않은 첫 항목. 카탈로그가 비었거나 전부 잠겨 있으면 nullptr. */
	const FFF_FestivalAtlasEntry* FindFirstUnlocked() const;

	/** 화면에 그릴 목록. 순서가 곧 표시 순서다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Atlas")
	TArray<FFF_FestivalAtlasEntry> Entries;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
