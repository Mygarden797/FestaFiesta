// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityLightingControls.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "FFFacilityLightingControls"

void UFF_FacilityLightingControls::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_LightingToggle) Button_LightingToggle->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleToggleClicked);
	if (Button_LightingOriginal) Button_LightingOriginal->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOriginalColorClicked);
	if (Button_LightingTeal) Button_LightingTeal->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTealClicked);
	if (Button_LightingSunset) Button_LightingSunset->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSunsetClicked);
	if (Button_LightingViolet) Button_LightingViolet->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleVioletClicked);
	if (Button_LightingReset) Button_LightingReset->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResetClicked);
	if (Slider_LightingBrightness)
	{
		Slider_LightingBrightness->OnValueChanged.AddUniqueDynamic(this, &ThisClass::HandleBrightnessChanged);
		Slider_LightingBrightness->OnMouseCaptureBegin.AddUniqueDynamic(this, &ThisClass::HandleBrightnessCaptureBegin);
		Slider_LightingBrightness->OnMouseCaptureEnd.AddUniqueDynamic(this, &ThisClass::HandleBrightnessCaptureEnd);
		Slider_LightingBrightness->OnControllerCaptureBegin.AddUniqueDynamic(this, &ThisClass::HandleBrightnessCaptureBegin);
		Slider_LightingBrightness->OnControllerCaptureEnd.AddUniqueDynamic(this, &ThisClass::HandleBrightnessCaptureEnd);
	}
	if (Input_LightingColor)
	{
		Input_LightingColor->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleColorTextChanged);
		Input_LightingColor->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleColorCommitted);
	}
}

FReply UFF_FacilityLightingControls::NativeOnMouseButtonDown(
	const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	return MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		? FReply::Handled() : Super::NativeOnMouseButtonDown(Geometry, MouseEvent);
}

void UFF_FacilityLightingControls::ApplyViewData(const FFF_FacilityInfoViewData& ViewData)
{
	TGuardValue<bool> ApplyingGuard(bApplyingViewData, true);
	const bool bSelectionChanged = CurrentViewData.FacilityInstanceId != ViewData.FacilityInstanceId;
	CurrentViewData = ViewData;
	SetVisibility(ViewData.bHasLightingRig ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SetIsEnabled(ViewData.bCanEditLighting);
	if (Text_LightingToggle) Text_LightingToggle->SetText(ViewData.LightingState.bEnabled
		? LOCTEXT("Enabled", "켜짐 · 끄기") : LOCTEXT("Disabled", "꺼짐 · 켜기"));
	if (Text_LightingBrightness) Text_LightingBrightness->SetText(FText::Format(
		LOCTEXT("Brightness", "{0}%"), FText::AsNumber(FMath::RoundToInt(ViewData.LightingState.BrightnessMultiplier * 100.0f))));
	if (Slider_LightingBrightness) Slider_LightingBrightness->SetValue(ViewData.LightingState.BrightnessMultiplier);
	if (Input_LightingColor && (bSelectionChanged || !Input_LightingColor->HasKeyboardFocus()))
	{
		ColorEditFacilityId.Invalidate();
		Input_LightingColor->SetText(ViewData.LightingState.bOverrideColor
			? FText::FromString(TEXT("#") + ViewData.LightingState.Color.ToFColorSRGB().ToHex().Left(6))
			: FText::GetEmpty());
		Input_LightingColor->ClearError();
	}
	if (Text_LightingColorStatus) Text_LightingColorStatus->SetText(ViewData.LightingState.bOverrideColor
		? LOCTEXT("ColorOverride", "선택한 색을 모든 조명에 적용 중")
		: LOCTEXT("OriginalColors", "Blueprint의 개별 조명 색상 사용 중"));
}

bool UFF_FacilityLightingControls::CanSendCommand() const
{
	return !bApplyingViewData && CurrentViewData.FacilityInstanceId.IsValid() &&
		CurrentViewData.bHasLightingRig && CurrentViewData.bCanEditLighting;
}

void UFF_FacilityLightingControls::HandleToggleClicked()
{
	if (!CanSendCommand()) return;
	FFFZSFacilityLightingState Requested = CurrentViewData.LightingState;
	Requested.bEnabled = !Requested.bEnabled;
	OnLightingRequested.Broadcast(CurrentViewData.FacilityInstanceId, Requested);
}

void UFF_FacilityLightingControls::HandleBrightnessCaptureBegin()
{
	bBrightnessCaptured = true;
	BrightnessCaptureFacilityId = CurrentViewData.FacilityInstanceId;
}

void UFF_FacilityLightingControls::HandleBrightnessCaptureEnd()
{
	bBrightnessCaptured = false;
	BrightnessCaptureFacilityId.Invalidate();
}

void UFF_FacilityLightingControls::HandleBrightnessChanged(const float Value)
{
	if (!CanSendCommand() || !FMath::IsFinite(Value) ||
		(bBrightnessCaptured && BrightnessCaptureFacilityId != CurrentViewData.FacilityInstanceId)) return;
	FFFZSFacilityLightingState Requested = CurrentViewData.LightingState;
	Requested.BrightnessMultiplier = FMath::Clamp(Value, 0.0f, 5.0f);
	if (!FMath::IsNearlyEqual(Requested.BrightnessMultiplier, CurrentViewData.LightingState.BrightnessMultiplier))
		OnLightingRequested.Broadcast(CurrentViewData.FacilityInstanceId, Requested);
}

void UFF_FacilityLightingControls::RequestColor(const FLinearColor& Color, const bool bOverride)
{
	if (!CanSendCommand()) return;
	FFFZSFacilityLightingState Requested = CurrentViewData.LightingState;
	Requested.bOverrideColor = bOverride;
	Requested.Color = Color;
	OnLightingRequested.Broadcast(CurrentViewData.FacilityInstanceId, Requested);
}

void UFF_FacilityLightingControls::HandleOriginalColorClicked() { RequestColor(CurrentViewData.LightingState.Color, false); }
void UFF_FacilityLightingControls::HandleTealClicked() { RequestColor(TealColor, true); }
void UFF_FacilityLightingControls::HandleSunsetClicked() { RequestColor(SunsetColor, true); }
void UFF_FacilityLightingControls::HandleVioletClicked() { RequestColor(VioletColor, true); }

bool UFF_FacilityLightingControls::TryParseColor(const FString& Text, FLinearColor& OutColor)
{
	FString Hex = Text.TrimStartAndEnd();
	Hex.RemoveFromStart(TEXT("#"));
	if (Hex.Len() != 6) return false;
	for (const TCHAR Character : Hex)
	{
		if (!FChar::IsHexDigit(Character)) return false;
	}
	OutColor = FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
	return true;
}

void UFF_FacilityLightingControls::HandleColorTextChanged(const FText& Text)
{
	if (bApplyingViewData) return;
	if (!ColorEditFacilityId.IsValid()) ColorEditFacilityId = CurrentViewData.FacilityInstanceId;
	if (Input_LightingColor) Input_LightingColor->ClearError();
}

void UFF_FacilityLightingControls::HandleColorCommitted(const FText& Text, const ETextCommit::Type CommitMethod)
{
	if (!CanSendCommand() || CommitMethod == ETextCommit::OnCleared ||
		!ColorEditFacilityId.IsValid() || ColorEditFacilityId != CurrentViewData.FacilityInstanceId) return;
	FLinearColor Color;
	if (!TryParseColor(Text.ToString(), Color))
	{
		if (Input_LightingColor) Input_LightingColor->SetError(LOCTEXT("InvalidColor", "#56DCE7처럼 6자리 색상 코드를 입력하세요."));
		return;
	}
	ColorEditFacilityId.Invalidate();
	RequestColor(Color, true);
}

void UFF_FacilityLightingControls::HandleResetClicked()
{
	if (CanSendCommand()) OnLightingResetRequested.Broadcast(CurrentViewData.FacilityInstanceId);
}

#undef LOCTEXT_NAMESPACE
