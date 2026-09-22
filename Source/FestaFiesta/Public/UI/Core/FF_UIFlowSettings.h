// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Time/FF_FestivalStage.h"
#include "UI/Core/FF_UIFlowTypes.h"
#include "FF_UIFlowSettings.generated.h"

class UFF_ActivatableScreenBase;
class UFF_FestivalAtlasCatalog;
class UFF_LoadingScreenWidget;
class UFF_UIRootLayout;
class UWorld;

/**
 * UI 흐름의 배선표. `Project Settings > Game > FestaFiesta UI Flow` 에서 조정한다.
 *
 * `UFF_UIFlowSubsystem` 이 이 표를 읽어 타이틀부터 최종 결산까지 화면을 올린다.
 * 화면을 갈아끼울 때 **코드를 고치지 않고 이 표만 고친다.**
 *
 * === 왜 팔레트에 넣지 않았는가 ===
 * 계획서 초안은 카탈로그 참조를 `UFF_UIPalette` 에 두려 했지만, 팔레트는 색과 모션만 갖는
 * 것이 맞다. 흐름 배선과 색은 고치는 사람도 고치는 시점도 다르다.
 *
 * === 남의 레벨을 가로채지 않는다 ===
 * `FrontendLevel` 에 지정한 레벨에서만 자동으로 타이틀을 띄운다. `Test_Main` 이나 팀원의
 * 테스트 맵에서 PIE 를 눌렀을 때 UI 가 끼어들면 전원의 작업이 막힌다.
 * 아무 레벨에서 흐름을 태워 보려면 콘솔에서 `FF.Flow.Start <FestivalId>` 를 쓴다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta UI Flow"))
class FESTAFIESTA_API UFF_UIFlowSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	static const UFF_UIFlowSettings* Get();

	// --- 토대 ---

	/**
	 * 뷰포트에 올리는 최상위 레이아웃. 보통 `WBP_FF_UIRootLayout` 이다.
	 * 비어 있으면 흐름 서브시스템이 아무것도 하지 않고 이유를 로그에 남긴다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Foundation")
	TSoftClassPtr<UFF_UIRootLayout> RootLayoutClass;

	/**
	 * 타이틀·지구본·국가 지도가 있는 레벨. **이 레벨에서만 자동으로 타이틀을 띄운다.**
	 * 비워 두면 자동 시작을 하지 않는다 — 그때는 콘솔 명령으로만 흐름을 태운다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Foundation", meta = (DisplayName = "Frontend Level"))
	TSoftObjectPtr<UWorld> FrontendLevel;

	/** 지구본·국가 지도가 읽는 축제 목록. 레벨 참조가 여기 들어 있다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Foundation")
	TSoftObjectPtr<UFF_FestivalAtlasCatalog> AtlasCatalog;

	// --- 레벨 로딩 ---

	/**
	 * 축제 레벨을 여는 동안 MoviePlayer가 보여줄 화면. 일반 화면 스택에는 올라가지 않는다.
	 * 비어 있으면 경고만 남기고 레벨 전환은 계속한다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Loading")
	TSoftClassPtr<UFF_LoadingScreenWidget> LoadingScreenClass;

	/** 짧은 로드에서 한 프레임만 번쩍이지 않도록 유지할 최소 시간. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Loading", meta = (ClampMin = "0.0", ClampMax = "10.0", ForceUnits = "s"))
	float MinimumLoadingScreenDisplayTime = 0.75f;

	// --- Atlas 화면 전환 ---

	/** 지구본과 국가 지도 사이에서 화면 전체를 덮고 교체한다. 게임 시간에는 관여하지 않는다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Atlas Transition")
	bool bEnableAtlasTransition = true;

	/** 현재 화면을 가리는 시간. 0초여도 불투명 프레임을 그린 뒤 교체한다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Atlas Transition",
		meta = (ClampMin = "0.0", ClampMax = "5.0", ForceUnits = "s", EditCondition = "bEnableAtlasTransition"))
	float AtlasTransitionFadeOutDuration = 0.15f;

	/** 교체 뒤 새 화면이 준비되어도 이 시간만큼은 덮개를 유지한다. 최소 0.05초. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Atlas Transition",
		meta = (ClampMin = "0.05", ClampMax = "5.0", ForceUnits = "s", EditCondition = "bEnableAtlasTransition"))
	float AtlasTransitionOpaqueHoldDuration = 0.05f;

	/** 준비된 화면을 다시 드러내는 시간. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Atlas Transition",
		meta = (ClampMin = "0.0", ClampMax = "5.0", ForceUnits = "s", EditCondition = "bEnableAtlasTransition"))
	float AtlasTransitionFadeInDuration = 0.25f;

	/** 전환 덮개의 색. 화면 교체를 완전히 가리도록 알파는 항상 1로 사용한다. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Atlas Transition",
		meta = (HideAlphaChannel, EditCondition = "bEnableAtlasTransition"))
	FLinearColor AtlasTransitionColor = FLinearColor::Black;

	// --- 화면 배선 ---

	/**
	 * 프론트엔드 화면. 전부 `UFF_ActivatableScreenBase` 파생이고 ScreenStack 에 올라간다.
	 * 없는 항목은 그 화면으로 넘어가지 않고 이유를 로그에 남긴다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Screens")
	TMap<EFF_UIFlowScreen, TSoftClassPtr<UFF_ActivatableScreenBase>> FrontendScreens;

	/**
	 * Stage 별 인게임 화면. **U2~U7 은 여기 클래스를 바꾸는 것으로 스텁을 교체한다.**
	 * 흐름 코드를 다시 고치지 않는다.
	 *
	 * `SunsetTransition`(노을 전환)을 빼먹지 않는다 — `EFF_FestivalStage` 는 코드에 8종인데
	 * `module-ownership.md` 는 7종으로 적고 있어 매핑에서 누락되기 쉽다. 빠지면 그 단계에서
	 * 화면이 바뀌지 않는다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Screens")
	TMap<EFF_FestivalStage, FFF_UIFlowStageScreen> StageScreens;

	/**
	 * 최종 결산. **Stage 표에 넣을 수 없다** — `OnFestivalEnded` 는 Day 3 의 `Report` Stage 안에서
	 * 울리므로 Stage 만으로는 일자별 결산과 구분되지 않는다. 그래서 별도 칸으로 둔다.
	 * 여기서 `로비로` 를 누르면 국가 지도로 돌아가 한 사이클이 닫힌다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Screens")
	TSoftClassPtr<UFF_ActivatableScreenBase> FinalReportScreenClass;

	// --- 더미 ---

	/**
	 * 저장 기록 목록에 더미를 채운다.
	 *
	 * **기본은 꺼짐이다.** 첫 실행이 기본 상태이고 플레이 허브는 `기록 없음` 을 보여줘야 한다.
	 * 저장 기록 화면의 카드 배치를 확인할 때만 켠다. 저장 API 가 실제로 오면 이 칸은 지운다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Dummy")
	bool bUseDummySaveSlots = false;

	// --- 진단 ---

	/**
	 * 켜면 Stage 전환과 화면 Push 를 매번 로그로 남긴다.
	 * 스텁 단계에서 어느 Stage 가 매핑되지 않았는지 찾는 데 쓴다. 기본은 켬이다.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Diagnostics")
	bool bVerboseFlowLogging = true;
};
