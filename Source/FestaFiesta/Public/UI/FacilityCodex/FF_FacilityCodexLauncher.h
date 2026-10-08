// FF_FacilityCodexLauncher.h
// 역할: 시설 도감을 여는 버튼을 도감 화면 관리자에 연결한다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_FacilityCodexLauncher.generated.h"

class UButton;
class UBorder;
class UFF_FacilityCodexProgressSubsystem;
class UFF_FacilityCodexSubsystem;

/** Opens the complete collection from a facility inspector, including undiscovered recipes. */
UCLASS(Abstract, Blueprintable, meta=(DisplayName="FF Facility Codex Launcher"))
class FESTAFIESTA_API UFF_FacilityCodexLauncher : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Opt-in filter for specialized panels. None (the default) shows every facility's recipes. */
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Facility Codex")
	void SetFacilityContext(FName FacilityId) { ExplicitFacilityId = FacilityId; }
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> Button_OpenCodex;
	/** Red first-use badge is consumed by a successfully opened explanation. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UBorder> Border_IntroductionHint;
	/** Earned rewards remain visible separately after the first-use badge clears. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UBorder> Border_RewardHint;
private:
	UFUNCTION() void HandleOpen();
	UFUNCTION() void RefreshNotificationHint();
	UPROPERTY(Transient) TObjectPtr<UFF_FacilityCodexProgressSubsystem> Progress;
	UPROPERTY(Transient) TObjectPtr<UFF_FacilityCodexSubsystem> Codex;
	FName ExplicitFacilityId;
};
