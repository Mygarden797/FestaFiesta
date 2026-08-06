// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "FF_ActivatableScreenBase.generated.h"

class UWidget;

/**
 * 화면이 요구하는 입력 모드.
 * CommonUI의 ECommonInputMode를 에디터에 직접 노출하지 않고 한 겹 감싼다.
 */
UENUM(BlueprintType)
enum class EFF_ScreenInputMode : uint8
{
	/** 게임 입력만 받는다. HUD처럼 입력을 가져가지 않는 화면. */
	Game		UMETA(DisplayName = "Game"),

	/** 메뉴 입력만 받는다. 타이틀·옵션처럼 화면이 입력을 독점하는 경우. */
	Menu		UMETA(DisplayName = "Menu"),

	/** 둘 다. Overview 패널처럼 게임을 계속 조작하면서 UI도 쓰는 경우. */
	All			UMETA(DisplayName = "All")
};

/**
 * FestaFiesta 전체 화면·패널의 베이스.
 *
 * 하는 일은 두 가지뿐이다.
 *   1. 이 화면이 어떤 입력 모드를 원하는지 CommonUI에 알린다.
 *   2. 게임패드 탐색을 위해 최초 포커스 대상을 돌려준다.
 *
 * !! 시간 정지 권한은 여기에 없다 !!
 * 화면을 열었다고 시뮬레이션을 멈추면 안 된다. 시간이 멈추는 곳은 MorningCheck와 ESC·옵션뿐이고,
 * 그 판단은 Stage 시스템과 PlayerController가 한다.
 * 근거: docs/design/stage-and-time.md 4절, docs/ui/ui-tech-decision.md 7절
 */
UCLASS(Abstract, meta = (DisplayName = "FF Activatable Screen Base"))
class FESTAFIESTA_API UFF_ActivatableScreenBase : public UCommonActivatableWidget
{
	GENERATED_BODY()

protected:
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

protected:
	/**
	 * 활성화됐을 때 처음 포커스를 줄 위젯.
	 * 지정하지 않으면 CommonUI 기본 동작에 맡긴다.
	 * 타이틀은 항상 `플레이`에 포커스를 준다(규격 7절).
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UWidget> InitialFocusTarget;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Input")
	EFF_ScreenInputMode ScreenInputMode = EFF_ScreenInputMode::Menu;

	/** 이 화면이 떠 있는 동안 마우스 커서를 보일지. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Input")
	bool bShowMouseCursor = true;

	/**
	 * 입력 설정 자체를 건드리지 않는 화면(HUD 위에 얹히는 비모달 패널 등)이면 true.
	 * Event 알림·토스트처럼 조작을 가로채면 안 되는 UI가 여기 해당한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Input")
	bool bIgnoreInputConfig = false;
};
