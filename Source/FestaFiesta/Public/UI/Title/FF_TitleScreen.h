// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_TitleScreen.generated.h"

class UFF_CommonButtonBase;
class UFF_GoogleAuthSubsystem;
class UFF_GoogleAccountPanel;
class UBorder;
class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

/** 타이틀 메뉴 항목 요청. 인자가 없다 — 어느 항목인지는 델리게이트가 구분한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnTitleMenuRequested);

/**
 * 타이틀 메인 메뉴 화면.
 *
 * 최상위 메뉴는 `플레이` / `옵션` / `게임 종료` 3개뿐이다.
 * 이어하기·새 게임을 최상위에 두지 않고 `플레이` 안에서 고른다.
 * 근거: docs/ui/main-menu-and-save.md 1절
 *
 * 아직 플레이 허브와 옵션 화면이 없으므로, 그 두 개는 BP 이벤트로 넘겨 임시 연결을 허용한다.
 * 화면이 만들어지면 BP 대신 여기서 UFF_UIRootLayout::PushScreen을 호출하도록 바꾼다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Title Screen"))
class FESTAFIESTA_API UFF_TitleScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	/**
	 * `플레이`를 눌렀을 때. **화면 밖에서 구독한다.**
	 *
	 * `BP_OnPlayRequested`는 이 WBP 안에서만 구현할 수 있어서, 레벨 블루프린트나
	 * PlayerController처럼 화면을 소유한 쪽이 반응할 수 없었다. 그래서 델리게이트를 함께 둔다.
	 *
	 * **타이틀 화면이 자기를 제거하거나 HUD를 만들지 않는다.** 그건 화면을 띄운 쪽의 일이다.
	 * 여기서 그걸 하면 타이틀이 게임 흐름을 알아야 하고, 축제가 늘 때마다 이 클래스를 고치게 된다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnTitleMenuRequested OnPlayRequested;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnTitleMenuRequested OnOptionsRequested;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Google Login")
	bool IsGoogleSignInPromptVisible() const { return bGooglePromptVisible; }

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Google Login")
	void ShowGoogleSignInPrompt();

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Google Login")
	void DismissGoogleSignInPrompt();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** First presentation focuses sign-in; after dismissal the main menu starts on Play. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UFUNCTION()
	void HandleButtonClicked(UFF_CommonButtonBase* Button);

	/** Optional account connection lives beside the menu and never gates Play. */
	void EnsureGoogleAccountPanel();
	void EnsureGoogleSignInPrompt();
	void SetTitleMenuEnabled(bool bEnabled);
	void FocusAccountOrPlay();

	UFUNCTION()
	void RefreshGoogleAccount();

	UFUNCTION()
	void HandleGoogleAccountClicked();

	UFUNCTION()
	void RefreshGoogleAccountButtonStyle();

	/** 플레이 허브 화면이 생기기 전까지의 임시 연결점. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI", meta = (DisplayName = "On Play Requested"))
	void BP_OnPlayRequested();

	/** 옵션 화면이 생기기 전까지의 임시 연결점. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI", meta = (DisplayName = "On Options Requested"))
	void BP_OnOptionsRequested();

protected:
	// WBP_TitleMain_GlassOrbit의 버튼 이름이 아래와 같아야 한다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Btn_Play;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Btn_Options;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Btn_Quit;

private:
	UPROPERTY(Transient)
	TObjectPtr<UFF_GoogleAuthSubsystem> GoogleAuth;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> GoogleAccountPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoogleAccountProfile;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoogleAccountStatus;

	UPROPERTY(Transient)
	TObjectPtr<UButton> GoogleAccountButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoogleAccountButtonLabel;

	UPROPERTY(Transient)
	TObjectPtr<UImage> GoogleSignInImage;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> GoogleSignInTexture;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> GooglePromptRoot;

	UPROPERTY(Transient)
	TObjectPtr<UFF_GoogleAccountPanel> GooglePromptAccount;

	UPROPERTY(Transient)
	TObjectPtr<UButton> GoogleContinueOffline;

	bool bGooglePromptVisible = false;
	bool bInitialGooglePromptHandled = false;
};
