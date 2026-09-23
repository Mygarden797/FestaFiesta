// FF_FacilityLightingControls.cpp
// 역할: 조명 설정을 임시로 편집하고 적용·취소·초기화 요청을 전달한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityLightingControls.h"
#include "UI/Facility/FF_FacilityColorPicker.h"
#include "Framework/Application/SlateApplication.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "FFFacilityLightingControls"

namespace
{
	bool IsLightingFieldFocused(const UWidget* Field)
	{ return Field && (Field->HasAnyUserFocus() || Field->HasFocusedDescendants()); }
}

void UFF_FacilityLightingControls::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (ColorPicker_Lighting)
	{
		ColorPicker_Lighting->OnColorPicked.AddUObject(this, &ThisClass::HandlePickerColor);
		ColorPicker_Lighting->OnInteractionStarted.AddWeakLambda(this,[this](){PickerCaptureFacilityId=CurrentViewData.FacilityInstanceId;});
	}
	if (Button_LightingApply) Button_LightingApply->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleApplyClicked);
	if (Button_LightingCancel) Button_LightingCancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelClicked);
	if (Button_LightingObserve) Button_LightingObserve->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleObserveClicked);
	if (Button_LightingPrecision) Button_LightingPrecision->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePrecisionClicked);
	if (Button_LightingBrightnessLow) Button_LightingBrightnessLow->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBrightnessLowClicked);
	if (Button_LightingBrightnessNormal) Button_LightingBrightnessNormal->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBrightnessNormalClicked);
	if (Button_LightingBrightnessHigh) Button_LightingBrightnessHigh->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBrightnessHighClicked);
	for (UEditableTextBox* Field : {Input_LightingH.Get(),Input_LightingS.Get(),Input_LightingV.Get(),Input_LightingR.Get(),Input_LightingG.Get(),Input_LightingB.Get()})
		if (Field)
		{
			Field->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleChannelsCommitted);
			Field->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleChannelTextChanged);
		}
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
	if (bSelectionChanged) { CancelEdit(); bPrecisionVisible=false; RefreshPrecisionVisibility(); }
	CurrentViewData = ViewData;
	SetVisibility(ViewData.bHasLightingRig ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SetIsEnabled(ViewData.bCanEditLighting);
	if (Text_LightingToggle) Text_LightingToggle->SetText(ViewData.LightingState.bEnabled
		? LOCTEXT("Enabled", "켜짐 · 끄기") : LOCTEXT("Disabled", "꺼짐 · 켜기"));
	if (Text_LightingBrightness) Text_LightingBrightness->SetText(FText::Format(
		LOCTEXT("Brightness", "{0}%"), FText::AsNumber(FMath::RoundToInt(ViewData.LightingState.BrightnessMultiplier * 100.0f))));
	if (Slider_LightingBrightness) Slider_LightingBrightness->SetValue(ViewData.LightingState.BrightnessMultiplier);
	if (Input_LightingColor && (bSelectionChanged || !IsLightingFieldFocused(Input_LightingColor)))
	{
		ColorEditFacilityId.Invalidate();
		Input_LightingColor->SetText(ViewData.LightingState.bOverrideColor
			? FText::FromString(TEXT("#") + ViewData.LightingState.Color.ToFColorSRGB().ToHex().Left(6))
			: FText::GetEmpty());
		Input_LightingColor->ClearError();
	}
	if (Text_LightingColorStatus) Text_LightingColorStatus->SetText(ViewData.LightingState.bOverrideColor
		? LOCTEXT("ColorOverride", "선택한 색을 모든 조명에 적용 중")
		: LOCTEXT("OriginalColors", "원래 조명 색상 사용 중"));
	RefreshColorFields();
}

bool UFF_FacilityLightingControls::CanSendCommand() const
{
	return !bApplyingViewData && CurrentViewData.FacilityInstanceId.IsValid() &&
		CurrentViewData.bHasLightingRig && CurrentViewData.bCanEditLighting;
}

void UFF_FacilityLightingControls::HandleToggleClicked()
{
	if (!CanSendCommand()) return;
	BeginEdit();
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
	BeginEdit();
	FFFZSFacilityLightingState Requested = CurrentViewData.LightingState;
	Requested.BrightnessMultiplier = FMath::Clamp(FMath::RoundToFloat(Value * 20.f) / 20.f, 0.0f, 5.0f);
	if (!FMath::IsNearlyEqual(Requested.BrightnessMultiplier, CurrentViewData.LightingState.BrightnessMultiplier))
		OnLightingRequested.Broadcast(CurrentViewData.FacilityInstanceId, Requested);
}

void UFF_FacilityLightingControls::RequestColor(const FLinearColor& Color, const bool bOverride)
{
	if (!CanSendCommand()) return;
	BeginEdit();
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
	if (CanSendCommand()) { BeginEdit(); OnLightingResetRequested.Broadcast(CurrentViewData.FacilityInstanceId); }
}

void UFF_FacilityLightingControls::RefreshPrecisionVisibility()
{
	if(Panel_LightingBasic) Panel_LightingBasic->SetVisibility(bPrecisionVisible?ESlateVisibility::Collapsed:ESlateVisibility::SelfHitTestInvisible);
	if(Panel_LightingPrecision) Panel_LightingPrecision->SetVisibility(bPrecisionVisible?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);
	if(Text_LightingPrecision) Text_LightingPrecision->SetText(bPrecisionVisible?LOCTEXT("ColorWheelTab","색상 휠"):LOCTEXT("PrecisionColorTab","정밀 값"));
}

void UFF_FacilityLightingControls::HandlePrecisionClicked()
{
	if(!Panel_LightingBasic || !Panel_LightingPrecision || !CanSendCommand()) return;
	if(ColorEditFacilityId.IsValid() && Input_LightingColor)
	{
		HandleColorCommitted(Input_LightingColor->GetText(),ETextCommit::OnEnter);
		if(ColorEditFacilityId.IsValid()) return;
	}
	if(ChannelEditFacilityId.IsValid())
	{
		HandleChannelsCommitted(FText::GetEmpty(),ETextCommit::OnEnter);
		if(ChannelEditFacilityId.IsValid()) return;
	}
	bPrecisionVisible=!bPrecisionVisible;
	RefreshPrecisionVisibility();
	if(FSlateApplication::IsInitialized()) FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
}

void UFF_FacilityLightingControls::HandleBrightnessLowClicked() { HandleBrightnessChanged(.5f); }
void UFF_FacilityLightingControls::HandleBrightnessNormalClicked() { HandleBrightnessChanged(1.f); }
void UFF_FacilityLightingControls::HandleBrightnessHighClicked() { HandleBrightnessChanged(1.5f); }

void UFF_FacilityLightingControls::BeginEdit()
{
	if (!bEditing && CurrentViewData.bCanEditLighting && CurrentViewData.FacilityInstanceId.IsValid())
	{
		OriginalState = CurrentViewData.LightingState;
		EditingFacilityId = CurrentViewData.FacilityInstanceId;
		bEditing = true;
	}
}

void UFF_FacilityLightingControls::CancelEdit()
{
	ColorEditFacilityId.Invalidate(); ChannelEditFacilityId.Invalidate();
	if (IsEditingText() && FSlateApplication::IsInitialized()) FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
	if (!bEditing) return;
	const FGuid Target = EditingFacilityId;
	const FFFZSFacilityLightingState Restore = OriginalState;
	bEditing = false; EditingFacilityId.Invalidate();
	// Clear the transaction before broadcasting: world changes can synchronously refresh this WBP.
	OnLightingRollbackRequested.Broadcast(Target, Restore);
}

void UFF_FacilityLightingControls::HandleApplyClicked()
{
	if (!CanSendCommand()) return;
	if (ColorEditFacilityId.IsValid() && Input_LightingColor)
	{
		HandleColorCommitted(Input_LightingColor->GetText(), ETextCommit::OnEnter);
		if (ColorEditFacilityId.IsValid()) return;
	}
	if (ChannelEditFacilityId.IsValid())
	{
		HandleChannelsCommitted(FText::GetEmpty(), ETextCommit::OnEnter);
		if (ChannelEditFacilityId.IsValid()) return;
	}
	bEditing = false; EditingFacilityId.Invalidate();
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
	OnEditFinished.Broadcast();
}
void UFF_FacilityLightingControls::HandleCancelClicked() { CancelEdit(); OnEditFinished.Broadcast(); }
void UFF_FacilityLightingControls::HandleObserveClicked()
{
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
	OnObserveRequested.Broadcast();
}
void UFF_FacilityLightingControls::HandlePickerColor(FLinearColor Color)
{ if(PickerCaptureFacilityId==CurrentViewData.FacilityInstanceId) RequestColor(Color,true); }

bool UFF_FacilityLightingControls::IsEditingText() const
{
	for (const UEditableTextBox* Field : {Input_LightingColor.Get(),Input_LightingH.Get(),Input_LightingS.Get(),Input_LightingV.Get(),Input_LightingR.Get(),Input_LightingG.Get(),Input_LightingB.Get()})
		if (IsLightingFieldFocused(Field)) return true;
	return false;
}

bool UFF_FacilityLightingControls::IsPointerCaptured() const
{
	return bBrightnessCaptured || (ColorPicker_Lighting && ColorPicker_Lighting->GetCachedWidget().IsValid() &&
		ColorPicker_Lighting->GetCachedWidget()->HasMouseCapture());
}

void UFF_FacilityLightingControls::RefreshColorFields()
{
	const FColor RGB=CurrentViewData.LightingState.Color.ToFColorSRGB();
	const FLinearColor HSV=FLinearColor(RGB.R/255.f,RGB.G/255.f,RGB.B/255.f).LinearRGBToHSV();
	const int32 Values[]={FMath::RoundToInt(HSV.R),FMath::RoundToInt(HSV.G*100),FMath::RoundToInt(HSV.B*100),RGB.R,RGB.G,RGB.B};
	UEditableTextBox* Fields[]={Input_LightingH,Input_LightingS,Input_LightingV,Input_LightingR,Input_LightingG,Input_LightingB};
	for(int32 I=0;I<6;++I) if(Fields[I] && !IsLightingFieldFocused(Fields[I])) Fields[I]->SetText(FText::AsNumber(Values[I]));
	if(ColorPicker_Lighting) ColorPicker_Lighting->SetColor(CurrentViewData.LightingState.Color);
}

void UFF_FacilityLightingControls::HandleChannelTextChanged(const FText&)
{
	if (!bApplyingViewData)
	{
		ChannelEditFacilityId=CurrentViewData.FacilityInstanceId;
		bChannelEditingHSV=IsLightingFieldFocused(Input_LightingH) || IsLightingFieldFocused(Input_LightingS) || IsLightingFieldFocused(Input_LightingV);
	}
}

void UFF_FacilityLightingControls::HandleChannelsCommitted(const FText&, ETextCommit::Type Method)
{
	if (!CanSendCommand() || Method==ETextCommit::OnCleared || ChannelEditFacilityId!=CurrentViewData.FacilityInstanceId) return;
	UEditableTextBox* Fields[]={Input_LightingH,Input_LightingS,Input_LightingV,Input_LightingR,Input_LightingG,Input_LightingB};
	const bool bHSV=bChannelEditingHSV;
	const int32 Start=bHSV?0:3;
	float Values[3];
	for(int32 I=0;I<3;++I)
	{
		if(!Fields[Start+I]) return;
		const FString Input=Fields[Start+I]->GetText().ToString();
		if(!Input.IsNumeric()) { Fields[Start+I]->SetError(LOCTEXT("NumberRequired","숫자를 입력하세요.")); return; }
		Values[I]=FCString::Atof(*Input);
		const float Max=bHSV?(I==0?360.f:100.f):255.f;
		if(!FMath::IsFinite(Values[I]) || Values[I]<0 || Values[I]>Max)
		{ Fields[Start+I]->SetError(FText::Format(LOCTEXT("ChannelRange","0~{0} 사이로 입력하세요."),FText::AsNumber(Max))); return; }
		Fields[Start+I]->ClearError();
	}
	FLinearColor RGB=bHSV?FLinearColor(FMath::Fmod(Values[0],360.f),Values[1]/100.f,Values[2]/100.f).HSVToLinearRGB():FLinearColor(Values[0]/255.f,Values[1]/255.f,Values[2]/255.f);
	ChannelEditFacilityId.Invalidate();
	RequestColor(FLinearColor::FromSRGBColor(FColor(FMath::RoundToInt(RGB.R*255),FMath::RoundToInt(RGB.G*255),FMath::RoundToInt(RGB.B*255))),true);
}

#undef LOCTEXT_NAMESPACE
