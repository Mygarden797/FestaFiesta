// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/Facility/FF_FacilityPaidProgramTypes.h"
#include "FF_FacilityPaidProgramCard.generated.h"

class UButton;
class UCommonTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FFF_OnFacilityPaidProgramCardSelected, FName);

/** Designer-owned card presentation. Selection only changes the picker preview. */
UCLASS(Blueprintable, meta = (DisplayName = "FF Facility Paid Program Card"))
class FESTAFIESTA_API UFF_FacilityPaidProgramCard : public UUserWidget
{
	GENERATED_BODY()

public:
	FFF_OnFacilityPaidProgramCardSelected OnSelected;

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info|Paid Program")
	void ApplyViewData(const FFF_FacilityPaidProgramCardViewData& ViewData);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info|Paid Program")
	void SetSelected(bool bSelected);

	FName GetProgramId() const { return CurrentViewData.ProgramId; }

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleSelectClicked();

	UPROPERTY(Transient)
	FFF_FacilityPaidProgramCardViewData CurrentViewData;

	/** Designer-tunable runtime states; edit these in the card WBP Class Defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor SelectedColor = FLinearColor(0.02f, 0.62f, 0.72f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor UnselectedColor = FLinearColor(0.055f, 0.12f, 0.15f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor AvailableColor = FLinearColor(0.25f, 1.0f, 0.67f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor UnavailableColor = FLinearColor(1.0f, 0.58f, 0.32f, 1.0f);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Select;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ProgramName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ProgramMeta;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ProgramAvailability;
};
