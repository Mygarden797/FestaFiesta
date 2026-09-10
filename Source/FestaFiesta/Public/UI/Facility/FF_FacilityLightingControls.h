// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "UI/Facility/FF_FacilityInfoTypes.h"
#include "FF_FacilityLightingControls.generated.h"

class UButton;
class UCommonTextBlock;
class UEditableTextBox;
class USlider;

DECLARE_MULTICAST_DELEGATE_TwoParams(FFF_OnFacilityLightingRequested,
	FGuid, const FFFZSFacilityLightingState&);
DECLARE_MULTICAST_DELEGATE_OneParam(FFF_OnFacilityLightingResetRequested, FGuid);

/** Designer-owned child WBP. It presents authoritative snapshots and sends identity-scoped commands. */
UCLASS(Blueprintable, meta = (DisplayName = "FF Facility Lighting Controls"))
class FESTAFIESTA_API UFF_FacilityLightingControls : public UUserWidget
{
	GENERATED_BODY()

public:
	FFF_OnFacilityLightingRequested OnLightingRequested;
	FFF_OnFacilityLightingResetRequested OnLightingResetRequested;

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Lighting")
	void ApplyViewData(const FFF_FacilityInfoViewData& ViewData);

	/** Strict six-digit sRGB input. Invalid edits never send a gameplay command. */
	static bool TryParseColor(const FString& Text, FLinearColor& OutColor);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry,
		const FPointerEvent& MouseEvent) override;

private:
	UFUNCTION() void HandleToggleClicked();
	UFUNCTION() void HandleBrightnessChanged(float Value);
	UFUNCTION() void HandleBrightnessCaptureBegin();
	UFUNCTION() void HandleBrightnessCaptureEnd();
	UFUNCTION() void HandleOriginalColorClicked();
	UFUNCTION() void HandleTealClicked();
	UFUNCTION() void HandleSunsetClicked();
	UFUNCTION() void HandleVioletClicked();
	UFUNCTION() void HandleColorTextChanged(const FText& Text);
	UFUNCTION() void HandleColorCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleResetClicked();
	void RequestColor(const FLinearColor& Color, bool bOverride);
	bool CanSendCommand() const;

	FFF_FacilityInfoViewData CurrentViewData;
	bool bApplyingViewData = false;
	bool bBrightnessCaptured = false;
	FGuid BrightnessCaptureFacilityId;
	FGuid ColorEditFacilityId;

	/** Preset colors can be changed in WBP Class Defaults without editing C++. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lighting|Presets", meta = (AllowPrivateAccess = "true"))
	FLinearColor TealColor = FLinearColor::FromSRGBColor(FColor(0x56, 0xDC, 0xE7));
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lighting|Presets", meta = (AllowPrivateAccess = "true"))
	FLinearColor SunsetColor = FLinearColor::FromSRGBColor(FColor(0xFF, 0xB5, 0x6B));
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lighting|Presets", meta = (AllowPrivateAccess = "true"))
	FLinearColor VioletColor = FLinearColor::FromSRGBColor(FColor(0xBC, 0xA2, 0xFF));

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true")) TObjectPtr<UButton> Button_LightingToggle;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true")) TObjectPtr<UCommonTextBlock> Text_LightingToggle;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true")) TObjectPtr<USlider> Slider_LightingBrightness;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true")) TObjectPtr<UCommonTextBlock> Text_LightingBrightness;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true")) TObjectPtr<UButton> Button_LightingOriginal;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true")) TObjectPtr<UButton> Button_LightingTeal;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true")) TObjectPtr<UButton> Button_LightingSunset;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true")) TObjectPtr<UButton> Button_LightingViolet;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true")) TObjectPtr<UEditableTextBox> Input_LightingColor;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true")) TObjectPtr<UCommonTextBlock> Text_LightingColorStatus;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true")) TObjectPtr<UButton> Button_LightingReset;
};
