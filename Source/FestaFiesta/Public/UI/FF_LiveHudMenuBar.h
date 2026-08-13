// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_LiveHudMenuBar.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UFF_LiveHudTabButton;

/**
 * Live HUD 하단 메뉴 탭.
 *
 * 목업 `docs/ui/mockups/live-hud-menu-states-v2-58px`의 9개 탭이다.
 * 8/14 기준 실제 내용이 있는 것은 운영·리포트·밀집도뿐이고 나머지는 준비 중 패널을 띄운다
 * (2026-08-12 결정 — 탭은 9개 다 두고 나중에 뺀다).
 */
UENUM(BlueprintType)
enum class EFF_LiveHudTab : uint8
{
	Operations	UMETA(DisplayName = "운영"),
	Facilities	UMETA(DisplayName = "시설"),
	Guests		UMETA(DisplayName = "관객"),
	Queues		UMETA(DisplayName = "대기열"),
	Density		UMETA(DisplayName = "밀집도"),
	Events		UMETA(DisplayName = "사건"),
	SNS			UMETA(DisplayName = "SNS"),
	Report		UMETA(DisplayName = "리포트"),
	Staff		UMETA(DisplayName = "스태프")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnLiveHudTabSelected, EFF_LiveHudTab, Tab);

/**
 * 메뉴 탭 하나. 아이콘 + 라벨 + 선택 상태.
 *
 * === CommonUI를 쓰지 않는다 ===
 * 실시간 HUD는 입력 모드를 가져가면 안 된다. `UCommonActivatableWidget` 계열을 쓰면
 * 화면 스택과 입력 모드가 따라붙어 Live 중 월드 조작이 막힐 수 있다.
 * 그래서 일반 `UUserWidget` + 일반 `UButton`이다. 근거: docs/ui/ui-tech-decision.md 7절
 *
 * 브러시 4종(Default/Hover/Selected/Pressed)은 WBP에서 지정한다.
 * **선택 상태 브러시는 `Pressed` 슬롯에 넣는다** — `RefreshVisualState`가 그것을 Normal에 끼운다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live HUD Tab Button"))
class FESTAFIESTA_API UFF_LiveHudTabButton : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 이 탭이 무엇인지·문구·아이콘을 함께 설정한다.
	 * 따로 두면 라벨만 바뀌고 아이콘이 남아 "시설 탭에 운영 아이콘" 같은 조합이 화면에 뜬다.
	 * `InIconBrush`의 리소스가 비어 있으면 WBP에 설정된 브러시를 유지한다.
	 */
	void Configure(EFF_LiveHudTab InTab, const FText& InLabel, const FSlateBrush& InIconBrush);

	/**
	 * 선택 상태를 반영한다.
	 *
	 * !! 색만 바꾸지 않는다 !! (AT-011)
	 * 선택된 탭은 배경 브러시가 바뀌고 라벨·아이콘 색이 반전된다. 배경 교체가 형태 변화를 담당한다.
	 */
	void SetSelected(bool bInSelected);

	EFF_LiveHudTab GetTab() const { return Tab; }

	/** 클릭 알림. 메뉴 바가 구독해 선택을 바꾼다. */
	DECLARE_DELEGATE_OneParam(FOnTabButtonClicked, EFF_LiveHudTab);
	FOnTabButtonClicked OnTabButtonClicked;

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleClicked();

protected:
	// --- 위젯 바인딩 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UButton> Button_Root;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Label;

	// --- 선택 상태 표현 ---

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Tint")
	FLinearColor NormalContentColor = FLinearColor::FromSRGBColor(FColor(0x10, 0x2B, 0x4E));

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Tint")
	FLinearColor SelectedContentColor = FLinearColor::FromSRGBColor(FColor(0xFF, 0xFF, 0xFF));

private:
	EFF_LiveHudTab Tab = EFF_LiveHudTab::Operations;
	bool bSelected = false;

	void RefreshVisualState();
};

/**
 * 탭 9개를 담는 레일.
 *
 * === 배치는 WBP가 소유한다 (2026-08-13 결정) ===
 * 탭을 런타임에 만들지 않는다. **디자이너에서 9개를 직접 놓고** 이 클래스가 이름으로 찾아
 * 라벨·아이콘·선택 상태만 채운다. 탭마다 위치·크기를 다르게 주거나 사이에 구분선을 넣을 수 있다.
 *
 * 이전 판은 `Box_Tabs` 하나만 바인딩하고 9개를 런타임 생성했는데, 그러면 디자이너에서
 * 탭이 보이지 않아 배치를 손으로 조정할 수 없었다.
 *
 * === 라벨과 아이콘은 여전히 코드가 채운다 ===
 * WBP에 문구를 박아두면 enum과 화면이 갈라진다. 아이콘도 WBP에 박으면 9개가 같은 그림이 된다
 * (2026-08-12에 실제로 그랬다). 둘 다 `EFF_LiveHudTab` 항목 이름에서 해석한다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live HUD Menu Bar"))
class FESTAFIESTA_API UFF_LiveHudMenuBar : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 탭이 선택될 때. 패널을 띄우는 쪽이 구독한다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnLiveHudTabSelected OnTabSelected;

	/** 코드에서 선택을 바꾼다. 델리게이트도 함께 쏜다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SelectTab(EFF_LiveHudTab Tab);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	EFF_LiveHudTab GetSelectedTab() const { return SelectedTab; }

	/**
	 * 탭 아이콘을 찾을 폴더. 테마를 바꿀 때 이 한 줄만 고치면 9개가 함께 바뀐다.
	 * 파일 이름은 `TabIconNameFormat`에 enum 이름을 넣어 만든다
	 * (`Operations` → `T_FF_LH_Icon_Operations_White_256`).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Icons")
	FString TabIconFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation");

	/** `{0}`에 `EFF_LiveHudTab`의 항목 이름이 들어간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Icons")
	FString TabIconNameFormat = TEXT("T_FF_LH_Icon_{0}_White_256");

	/** 아이콘 표시 크기(px). 목업 기준 13. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Icons")
	float TabIconSize = 13.f;

	/**
	 * 자동 해석을 덮어쓰고 싶은 탭만 넣는다. 보통은 비워 둔다.
	 * 폴더 규칙에서 벗어나는 아이콘 하나를 예외로 둘 때만 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Icons")
	TMap<EFF_LiveHudTab, FSlateBrush> TabIconOverrides;

protected:
	virtual void NativeOnInitialized() override;

	void HandleTabButtonClicked(EFF_LiveHudTab Tab);

protected:
	// --- 위젯 바인딩 ---
	//
	// WBP에 아래 이름으로 `WBP_FF_LiveHudTabButton` 인스턴스를 놓는다. 위치는 자유다.
	// 전부 Optional이라 일부만 만든 WBP도 통과한다 — 빠진 탭은 로그로 알린다.
	// 탭을 늘리려면 enum에 항목을 추가하고 여기에 같은 이름의 필드를 하나 더 만든다.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Tabs")
	TObjectPtr<UFF_LiveHudTabButton> Tab_Operations;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Tabs")
	TObjectPtr<UFF_LiveHudTabButton> Tab_Facilities;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Tabs")
	TObjectPtr<UFF_LiveHudTabButton> Tab_Guests;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Tabs")
	TObjectPtr<UFF_LiveHudTabButton> Tab_Queues;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Tabs")
	TObjectPtr<UFF_LiveHudTabButton> Tab_Density;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Tabs")
	TObjectPtr<UFF_LiveHudTabButton> Tab_Events;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Tabs")
	TObjectPtr<UFF_LiveHudTabButton> Tab_SNS;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Tabs")
	TObjectPtr<UFF_LiveHudTabButton> Tab_Report;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Tabs")
	TObjectPtr<UFF_LiveHudTabButton> Tab_Staff;

private:
	/** 바인딩된 탭 위젯을 enum과 짝지어 모은다. 없는 탭은 건너뛰고 로그에 남긴다. */
	void CollectTabs();

	/** 탭 아이콘 브러시를 만든다. 예외 지정이 있으면 그것을, 없으면 폴더 규칙으로 찾는다. */
	FSlateBrush ResolveTabIcon(EFF_LiveHudTab Tab, const FString& EnumName) const;

	EFF_LiveHudTab SelectedTab = EFF_LiveHudTab::Operations;

	UPROPERTY()
	TArray<TObjectPtr<UFF_LiveHudTabButton>> TabButtons;
};
