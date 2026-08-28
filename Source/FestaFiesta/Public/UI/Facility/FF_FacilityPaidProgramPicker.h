// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/Facility/FF_FacilityPaidProgramTypes.h"
#include "FF_FacilityPaidProgramPicker.generated.h"

class UBorder;
class UButton;
class UCommonTextBlock;
class UScrollBox;
class UFF_FacilityPaidProgramCard;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FFF_OnFacilityPaidProgramConfirmed, FName, FGuid);

/**
 * Facility-scoped paid-program picker.
 *
 * It owns only local selection/preview state. The explicit confirmation event is revalidated and
 * committed by FacilityInfoSubsystem through ProgramSchedulingSubsystem.
 */
UCLASS(Blueprintable, meta = (DisplayName = "FF Facility Paid Program Picker"))
class FESTAFIESTA_API UFF_FacilityPaidProgramPicker : public UUserWidget
{
	GENERATED_BODY()

public:
	FFF_OnFacilityPaidProgramConfirmed OnConfirmed;
	FSimpleMulticastDelegate OnCloseRequested;

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info|Paid Program")
	void ApplyViewData(const FFF_FacilityPaidProgramPickerViewData& ViewData);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info|Paid Program")
	void SetPickerOpen(bool bOpen);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Facility Info|Paid Program")
	bool IsPointerOverInteractivePanel() const;

	FGuid GetFacilityInstanceId() const { return CurrentViewData.FacilityInstanceId; }

	static FSoftClassPath GetDefaultCardClassPath();

protected:
	virtual void NativeOnInitialized() override;

private:
	void HandleCardSelected(FName ProgramId);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCloseClicked();

	const FFF_FacilityPaidProgramCardViewData* FindCard(FName ProgramId) const;
	void ClearPreview();

	UPROPERTY(Transient)
	FFF_FacilityPaidProgramPickerViewData CurrentViewData;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_FacilityPaidProgramCard>> SpawnedCards;

	FName SelectedProgramId = NAME_None;

	/** Optional designer override. Unset uses WBP_FF_FacilityPaidProgramCard. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content",
		meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UFF_FacilityPaidProgramCard> CardWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor FeedbackErrorColor = FLinearColor(1.0f, 0.32f, 0.28f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor FeedbackSuccessColor = FLinearColor(0.25f, 1.0f, 0.67f, 1.0f);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_Picker;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Empty;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UScrollBox> ScrollBox_Programs;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_PreviewName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_PreviewDescription;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_PreviewDetails;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_PreviewAvailability;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_NoRefundNotice;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Feedback;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Confirm;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Confirm;
};
