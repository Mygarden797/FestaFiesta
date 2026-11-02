#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_GoogleAccountPanel.generated.h"

class UFF_GoogleAuthSubsystem;
class UButton;
class UImage;
class UTextBlock;
class UTexture2D;

/** Shared account surface for the title sign-in prompt and both options screens. */
UCLASS(meta = (DisplayName = "FF Google Account Panel"))
class FESTAFIESTA_API UFF_GoogleAccountPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Optional explicit session owner, including playerless UI fixtures. Does not start authentication. */
	void SetAuthSubsystem(UFF_GoogleAuthSubsystem* InAuth);

	UFUNCTION()
	void RefreshAccount();

	UWidget* GetPrimaryFocusTarget() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildAccountTree();
	void BindSession();
	void UnbindSession();

	UFUNCTION()
	void HandleAccountAction();

	UFUNCTION()
	void RefreshActionStyle();

	UPROPERTY(Transient)
	TObjectPtr<UFF_GoogleAuthSubsystem> Auth;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EmailValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StateValue;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusMessage;
	UPROPERTY(Transient)
	TObjectPtr<UButton> AccountAction;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ActionLabel;
	UPROPERTY(Transient)
	TObjectPtr<UImage> SignInArtwork;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> SignInTexture;

	bool bHasExplicitSession = false;
	bool bConstructed = false;
};
