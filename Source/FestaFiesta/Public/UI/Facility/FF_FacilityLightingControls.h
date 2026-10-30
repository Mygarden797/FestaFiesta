// FF_FacilityLightingControls.h
// 역할: 조명 설정을 임시로 편집하고 적용·취소·초기화 요청을 전달한다.

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
class UFF_FacilityColorPicker;
class UWidget;
class UBorder;

DECLARE_MULTICAST_DELEGATE_TwoParams(FFF_OnFacilityLightingRequested,
	FGuid, const FFFZSFacilityLightingState&);
DECLARE_MULTICAST_DELEGATE_OneParam(FFF_OnFacilityLightingResetRequested, FGuid);

/** Designer-owned child WBP. It presents authoritative snapshots and sends identity-scoped commands. */
UCLASS(Blueprintable, meta = (DisplayName = "FF Facility Lighting Controls"))
class FESTAFIESTA_API UFF_FacilityLightingControls : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Build mode can use a compact layout without changing this WBP's live-mode design. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|Build Layout")
	bool bUseCompactBuildLayout = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|Build Layout", meta = (EditCondition = "bUseCompactBuildLayout", ClampMin = "32"))
	float BuildColorWheelSize = 92.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|Build Layout", meta = (EditCondition = "bUseCompactBuildLayout", ClampMin = "0"))
	float BuildColorSwatchHeight = 32.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|Build Layout", meta = (EditCondition = "bUseCompactBuildLayout", ClampMin = "0"))
	float BuildMaximumVerticalPadding = 2.f;
	/** Empty uses the description authored in the widget tree. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|Build Layout", meta = (EditCondition = "bUseCompactBuildLayout", MultiLine = "true"))
	FText BuildLightingDescription;
	FFF_OnFacilityLightingRequested OnLightingRequested;
	FFF_OnFacilityLightingRequested OnLightingRollbackRequested;
	FFF_OnFacilityLightingResetRequested OnLightingResetRequested;
	FSimpleMulticastDelegate OnObserveRequested;
	FSimpleMulticastDelegate OnEditFinished;
	void BeginEdit();
	void CancelEdit();
	bool IsEditing() const { return bEditing; }
	bool IsEditingText() const;
	bool IsPointerCaptured() const;

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
	UFUNCTION() void HandleApplyClicked();
	UFUNCTION() void HandleCancelClicked();
	UFUNCTION() void HandleObserveClicked();
	UFUNCTION() void HandlePrecisionClicked();
	UFUNCTION() void HandleBrightnessLowClicked();
	UFUNCTION() void HandleBrightnessNormalClicked();
	UFUNCTION() void HandleBrightnessHighClicked();
	void RefreshPrecisionVisibility();
	UFUNCTION() void HandleChannelsCommitted(const FText& Text, ETextCommit::Type CommitMethod);
	UFUNCTION() void HandleChannelTextChanged(const FText& Text);
	void HandlePickerColor(FLinearColor Color);
	void RefreshColorFields();
	void RequestColor(const FLinearColor& Color, bool bOverride);
	bool CanSendCommand() const;

	FFF_FacilityInfoViewData CurrentViewData;
	bool bApplyingViewData = false;
	bool bBrightnessCaptured = false;
	FGuid BrightnessCaptureFacilityId;
	FGuid ColorEditFacilityId;
	FGuid ChannelEditFacilityId;
	FGuid EditingFacilityId;
	FGuid PickerCaptureFacilityId;
	FFFZSFacilityLightingState OriginalState;
	bool bEditing = false;
	bool bChannelEditingHSV = false;
	bool bPrecisionVisible = false;

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
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UFF_FacilityColorPicker> ColorPicker_Lighting;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> Input_LightingH;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> Input_LightingS;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> Input_LightingV;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> Input_LightingR;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> Input_LightingG;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> Input_LightingB;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_LightingApply;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_LightingCancel;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_LightingObserve;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_LightingPrecision;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_LightingPrecision;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> Panel_LightingBasic;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> Panel_LightingPrecision;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_LightingBrightnessLow;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_LightingBrightnessNormal;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_LightingBrightnessHigh;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> Border_LightingCurrentColor;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> Border_LightingNewColor;
};
