// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_PanelFrame.generated.h"

class UBorder;
class UButton;
class UPanelWidget;
class UScrollBox;
class UTextBlock;

/** 닫기 요청. 인자가 없다 — 어느 패널인지는 구독하는 쪽이 안다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnPanelCloseRequested);

/**
 * 패널의 공통 껍데기 — 머리(색 바 + 제목 + 닫기) + 본문 스크롤 + 발.
 *
 * 준비 단계 패널 6종과 라이브 패널 7종이 이걸 공유한다. 껍데기를 13번 다시 만들지 않기 위한 것이다.
 *
 * === 스스로 닫지 않는다 ===
 * `OnCloseRequested` 를 알리기만 하고 자기를 제거하지 않는다. 준비 단계에서 **패널은 한 번에
 * 하나만 열리고 그 규칙은 `UFF_PrepHudBar` 가 소유한다** — 패널이 스스로 닫으면 소유자가
 * 무엇이 열려 있는지 알 수 없게 된다. `UFF_TitleScreen` 이 같은 이유로 같은 구조다.
 *
 * === CommonUI 를 쓰지 않는다 ===
 * 준비·라이브 패널은 비모달이고 Live 시간이 계속 흘러야 한다. 화면 스택에 올리지 않는다.
 * 풀스크린 시트(마감·결산·피날레·최종결산)만 CommonUI 다.
 * 근거: docs/ui/ui-tech-decision.md 7절
 *
 * === 색은 UFF_UIPalette 가 소유한다 ===
 * `NativePreConstruct` 에서 팔레트를 읽어 칠하므로 디자이너에서도 보인다.
 * **디자이너에서 이 색들을 손으로 바꾸지 않는다** — 제어 경로가 둘이 되면 어느 쪽이 이겼는지
 * 알 수 없다. 색을 바꾸려면 Project Settings > Game > FestaFiesta UI Palette 를 고친다.
 *
 * === 남은 것 ===
 * `Button_Close` 는 지금 일반 `UButton` 이다. 전역 `RenderFocusRule=Never` 라 포커스 링이
 * 그려지지 않으므로 게임패드로는 닫기 위치가 보이지 않는다 (C-024). 인게임 버튼 스타일이
 * 생기는 U2 에서 교체한다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Panel Frame"))
class FESTAFIESTA_API UFF_PanelFrame : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 닫기를 눌렀을 때. **패널을 실제로 닫는 것은 구독하는 쪽의 일이다.** */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnPanelCloseRequested OnCloseRequested;

	/** 머리의 제목. 화면 문구이며 내부 이름을 쓰지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetTitle(const FText& InTitle);

	/** 본문 스크롤에 자식을 붙인다. 실패하면 nullptr 을 돌려주고 이유를 로그에 남긴다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	UWidget* AddContent(UWidget* Content);

	/** 본문을 비운다. 패널 내용을 갈아끼울 때 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void ClearContent();

	/**
	 * 발(CTA 영역)을 보이거나 숨긴다.
	 * 숨길 때 `Collapsed` 를 쓴다 — `Hidden` 은 자리를 남겨 빈 칸이 생긴다.
	 * **값이 없는 칸은 숨긴다. `0` 이나 `-` 를 넣지 않는다** (docs/ui/ui-scope.md 3절).
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetFooterVisible(bool bVisible);

	/** 발에 버튼을 직접 붙일 때 쓴다. 없으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	UPanelWidget* GetFooter() const { return Panel_Footer; }

	/** 닫기 버튼을 감출 때. 닫을 수 없는 패널(점검 견적서 등)에 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetCloseButtonVisible(bool bVisible);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleCloseClicked();

	// --- 위젯 바인딩. WBP의 위젯 이름이 아래와 정확히 같아야 한다 ---

	/** 머리 바. 팔레트의 InGame.Teal 로 칠한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UBorder> Border_Header;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Title;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UScrollBox> ScrollBox_Body;

	/** 본문 배경. 팔레트의 InGame.Paper 로 칠한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UBorder> Border_Body;

	/** 닫을 수 없는 패널도 있으므로 필수가 아니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UButton> Button_Close;

	/** CTA 가 없는 패널도 있으므로 필수가 아니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UPanelWidget> Panel_Footer;
};
