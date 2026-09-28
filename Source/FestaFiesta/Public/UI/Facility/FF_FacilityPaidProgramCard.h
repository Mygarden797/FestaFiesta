// FF_FacilityPaidProgramCard.h
// 역할: 유료 프로그램 하나의 이름·가격·편성 가능 여부를 표시한다.

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
	virtual void NativePreConstruct() override;
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION()
	void HandleSelectClicked();

	UPROPERTY(Transient)
	FFF_FacilityPaidProgramCardViewData CurrentViewData;

	/** Runtime state tints share the construction mode's pearl and cyan palette. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor SelectedColor =
		FLinearColor::FromSRGBColor(FColor(0xD3, 0xF1, 0xFF));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor UnselectedColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor AvailableColor =
		FLinearColor::FromSRGBColor(FColor(0x3D, 0x3C, 0x4F));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor UnavailableColor =
		FLinearColor::FromSRGBColor(FColor(0xD7, 0x4E, 0x43));

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Select;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ProgramName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ProgramMeta;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ProgramAvailability;
};
