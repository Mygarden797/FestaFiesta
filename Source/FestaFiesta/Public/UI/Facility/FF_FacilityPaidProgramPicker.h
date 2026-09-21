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

	/** Called only after the authoritative scheduling transaction succeeds. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info|Paid Program")
	void ShowPurchaseComplete(FName ProgramId, int32 RemainingGold);

	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Facility Info|Paid Program")
	void OnViewDataApplied(const FFF_FacilityPaidProgramPickerViewData& ViewData);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Facility Info|Paid Program")
	bool IsPointerOverInteractivePanel() const;

	FGuid GetFacilityInstanceId() const { return CurrentViewData.FacilityInstanceId; }

	static FSoftClassPath GetDefaultCardClassPath();

protected:
	virtual void NativePreConstruct() override;
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
	bool bPurchaseCompleted = false;
	bool bCommitPending = false;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Border_Body;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Border_Footer;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_PurchaseComplete;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_PurchaseReceipt;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ResourcePreview;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_PreviewTime;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Capacity;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_TicketFee;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_MaxRevenue;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_PurchaseCost;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ReceiptProgramName;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ReceiptResources;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_ReturnToFacility;

	/** Optional designer override. Unset uses WBP_FF_FacilityPaidProgramCard. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Content",
		meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UFF_FacilityPaidProgramCard> CardWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor FeedbackErrorColor =
		FLinearColor::FromSRGBColor(FColor(0xD7, 0x4E, 0x43));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor FeedbackSuccessColor =
		FLinearColor::FromSRGBColor(FColor(0x3F, 0x8F, 0x6D));

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
