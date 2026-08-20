// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_UIFlowTypes.generated.h"

class UUserWidget;

/**
 * 레벨을 열기 전에 보여주는 화면들.
 *
 * === Stage 와 왜 따로 두는가 ===
 * `EFF_FestivalStage` 는 축제가 시작된 뒤의 진행 단계다. 타이틀·플레이 허브·지구본은 축제가
 * 시작되기 전이라 Stage 로 표현할 수 없다. 그래서 프론트엔드 흐름은 별도 상태로 둔다.
 */
UENUM(BlueprintType)
enum class EFF_UIFlowScreen : uint8
{
	/** 프론트엔드 화면이 없는 상태. 축제 레벨에 있다는 뜻이다. */
	None			UMETA(DisplayName = "없음"),

	Title			UMETA(DisplayName = "타이틀"),
	PlayHub			UMETA(DisplayName = "플레이 허브"),
	SaveList		UMETA(DisplayName = "저장 기록"),
	Options			UMETA(DisplayName = "옵션"),
	AtlasGlobe		UMETA(DisplayName = "지구본"),
	AtlasCountry	UMETA(DisplayName = "국가 지도")
};

/**
 * 화면을 어느 레이어에 올릴지.
 *
 * `UFF_UIRootLayout` 의 레이어 구성과 1:1 이다. 준비·라이브 HUD 는 비모달이어야 하므로
 * HUD 레이어에 올리고, 결산 계열 풀스크린 시트만 화면 스택에 올린다.
 */
UENUM(BlueprintType)
enum class EFF_UIFlowLayer : uint8
{
	/**
	 * HUDLayer. 일반 `UUserWidget`. 입력을 가져가지 않는다.
	 * **`Self Hit Test Invisible` 이어야 한다** — `Visible` 이면 화면 전체가 월드 클릭을 먹어
	 * 건축 배치가 죽는다. 실제로 발생한 적이 있다.
	 */
	HUD				UMETA(DisplayName = "HUD 레이어"),

	/** ScreenStack. `UFF_ActivatableScreenBase` 파생만 올라간다. 풀스크린 시트용. */
	Screen			UMETA(DisplayName = "화면 스택")
};

/**
 * Stage 하나에 대응하는 화면.
 *
 * **`switch` 로 매핑하지 않는다.** Stage 가 늘면 코드가 아니라 표가 늘어야 하고,
 * U2~U7 이 스텁을 실제 화면으로 갈아끼울 때 흐름 코드를 다시 고치지 않아도 된다.
 * enum ordinal 에 의존하지 않는다 (module-ownership.md 3절 경고).
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_UIFlowStageScreen
{
	GENERATED_BODY()

	/**
	 * 올릴 위젯. 비워 두면 그 Stage 에서는 아무것도 올리지 않는다.
	 * `Layer == Screen` 이면 `UFF_ActivatableScreenBase` 파생이어야 한다 — 아니면 런타임에 거른다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Stage Screen")
	TSoftClassPtr<UUserWidget> WidgetClass;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Stage Screen")
	EFF_UIFlowLayer Layer = EFF_UIFlowLayer::HUD;
};
