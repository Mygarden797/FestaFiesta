// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"
#include "FF_PrepPanelBase.generated.h"

class UFF_PrepHudBar;
class UTextBlock;

/**
 * 준비 단계 하단 바에서 열리는 패널의 베이스. **U5 의 패널 6종이 여기서 파생한다.**
 *
 * === 패널은 자기 가시성을 소유하지 않는다 ===
 * 여는 것도 닫는 것도 `UFF_PrepHudBar` 다. 패널이 스스로 `SetVisibility` 를 부르면 배타 규칙의
 * 주체가 둘로 갈려 "어느 쪽이 이겼는지 알 수 없는" 상태가 된다 (ui-antipatterns-to-avoid 10번).
 * 닫아야 할 일이 있으면 `RequestClose()` 로 **바에 요청**한다.
 *
 * === CommonUI 를 쓰지 않는다 ===
 * 준비 단계에도 Live 시간이 흐르는 단계(홍보)가 있고, HUD 가 입력 모드를 가져가면 월드 조작이
 * 막힌다. 전체 화면 시트가 아니라 HUD 위의 패널이므로 일반 `UUserWidget` 이다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Panel Base"))
class FESTAFIESTA_API UFF_PrepPanelBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 바가 패널을 만든 직후 한 번 부른다. 어느 탭의 패널인지와 제목을 함께 넣는다.
	 * 따로 두면 제목만 바뀌고 탭이 남아 "인력 패널에 홍보 제목"이 뜬다.
	 */
	void InitializePanel(UFF_PrepHudBar* InOwningBar, EFF_PrepHudTab InTab, const FText& InTitle);

	/** 패널이 화면에 올라올 때마다. 값 갱신·구독은 여기서 한다. */
	void HandlePanelOpened();

	/** 패널이 내려갈 때마다. 구독 해제는 여기서 한다. */
	void HandlePanelClosed();

	EFF_PrepHudTab GetTab() const { return Tab; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep")
	UFF_PrepHudBar* GetOwningBar() const { return OwningBar.Get(); }

	/**
	 * 이 패널을 닫아 달라고 **바에 요청한다.** 점검 패널의 `배치 계속하기` 처럼 패널 안에서
	 * 닫아야 하는 버튼이 쓴다. 직접 숨기지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep")
	void RequestClose();

protected:
	/** C++ 파생용. `Super::` 를 부른 뒤 자기 것을 한다. */
	virtual void NativeOnPanelOpened() {}
	virtual void NativeOnPanelClosed() {}

	/** BP 파생용. 이름으로 BP 함수를 찾지 않기 위해 명시적으로 선언한다 (안티패턴 17번). */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI|Prep", meta = (DisplayName = "On Panel Opened"))
	void K2_OnPanelOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI|Prep", meta = (DisplayName = "On Panel Closed"))
	void K2_OnPanelClosed();

	/** 제목. 바가 탭 라벨에서 넣어 주므로 WBP 에 문구를 박지 않는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep")
	TObjectPtr<UTextBlock> Text_PanelTitle;

private:
	UPROPERTY()
	TWeakObjectPtr<UFF_PrepHudBar> OwningBar;

	EFF_PrepHudTab Tab = EFF_PrepHudTab::Build;
};

/**
 * 아직 만들지 않은 탭에 띄우는 자리표시 패널. **U5 가 실제 패널을 만들면 매핑에서 빠진다.**
 *
 * 빈 패널을 띄우는 대신 "무엇이 여기 들어올 예정인지"를 적는다. 아무것도 안 뜨면 탭이
 * 고장 난 것으로 보이고, 빈 상자만 뜨면 만들다 만 것으로 보인다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Placeholder Panel"))
class FESTAFIESTA_API UFF_PrepPlaceholderPanel : public UFF_PrepPanelBase
{
	GENERATED_BODY()

public:
	/** 바가 넣어 준다. `U5 에서 만듭니다` 같은 한 줄. */
	void SetPlaceholderMessage(const FText& InMessage);

protected:
	virtual void NativeOnPanelOpened() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep")
	TObjectPtr<UTextBlock> Text_Placeholder;

private:
	FText PlaceholderMessage;
};
