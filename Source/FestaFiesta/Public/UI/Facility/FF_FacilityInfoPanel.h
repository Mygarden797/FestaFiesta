// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/Facility/FF_FacilityInfoTypes.h"
#include "FF_FacilityInfoPanel.generated.h"

class UBorder;
class UButton;
class UCommonTextBlock;
class UImage;
class UWidget;

/**
 * Native contract for WBP_FF_FacilityInfoPanel.
 *
 * The WBP owns layout and styling; this class owns data projection and button events. Keeping
 * gameplay queries outside the widget prevents Blueprint bindings from running every frame.
 */
UCLASS(Blueprintable, meta = (DisplayName = "FF Facility Info Panel"))
class FESTAFIESTA_API UFF_FacilityInfoPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnMoveRequested;
	FSimpleMulticastDelegate OnRemoveRequested;
	FSimpleMulticastDelegate OnCloseRequested;

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info")
	void ApplyViewData(const FFF_FacilityInfoViewData& ViewData);

	/** Opens without making the full-screen wrapper a hit target; closes by collapsing it. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info")
	void SetPanelOpen(bool bOpen);

	/** True only while the cursor is over the visible card, not the full-screen overlay. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Facility Info")
	bool IsPointerOverInteractivePanel() const;

	const FFF_FacilityInfoViewData& GetCurrentViewData() const { return CurrentViewData; }

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

private:
	UFUNCTION()
	void HandleMoveClicked();

	UFUNCTION()
	void HandleRemoveClicked();

	UFUNCTION()
	void HandleCloseClicked();

	void SetText(UCommonTextBlock* Target, const FText& Text) const;

	UPROPERTY(Transient)
	FFF_FacilityInfoViewData CurrentViewData;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_Panel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Move;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Remove;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Image_Thumbnail;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ThumbnailPlaceholder;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_BuildSection;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_LiveSection;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Context;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Category;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Zone;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityState;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_RelocationWarning;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Footprint;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Clearance;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_InstallCost;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_RecoveryCost;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_QueueCapacity;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ServiceDuration;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_OperationState;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_WaitingParties;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Occupancy;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_RemainingService;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ExpectedWait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Throughput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Feedback;
};
