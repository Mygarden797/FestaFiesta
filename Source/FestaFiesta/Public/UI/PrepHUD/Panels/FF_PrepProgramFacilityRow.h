#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "FF_PrepProgramFacilityRow.generated.h"

class UBorder;
class UButton;
class UTextBlock;
struct FFF_PrepFacilityOption;

DECLARE_DELEGATE_OneParam(FFF_OnPrepProgramFacilityAction, int32);

/** 설치된 시설 한 곳. 편성 대상으로 고르기와 월드 위치 보기를 독립된 버튼으로 제공한다. */
UCLASS(Abstract, Blueprintable)
class FESTAFIESTA_API UFF_PrepProgramFacilityRow : public UUserWidget
{
	GENERATED_BODY()
public:
	void ConfigureFacility(int32 InIndex, const FFF_PrepFacilityOption& Facility);
	void SetFacilityState(bool bSelected, bool bScheduled);
	FFF_OnPrepProgramFacilityAction OnFacilitySelected;
	FFF_OnPrepProgramFacilityAction OnLocateRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_Selection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Select;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Locate;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SelectionMark;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FacilityName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Location;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Facility")
	FLinearColor SelectedBackgroundColor = FLinearColor::FromSRGBColor(FColor(227, 245, 251));
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Facility")
	FLinearColor SelectedOutlineColor = FLinearColor::FromSRGBColor(FColor(91, 173, 204));
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Facility")
	FLinearColor HoveredBackgroundColor = FLinearColor::FromSRGBColor(FColor(240, 247, 249));
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Facility")
	FLinearColor SelectedHoveredBackgroundColor = FLinearColor::FromSRGBColor(FColor(215, 238, 247));
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Facility")
	FText ScheduledText = NSLOCTEXT("FestaFiestaPrepProgram", "FacilityRowScheduled", "✓ 오늘 편성 완료");
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Program Facility")
	FText LocateTooltip = NSLOCTEXT("FestaFiestaPrepProgram", "FacilityRowLocateTooltip", "시설 위치를 확인합니다. 프로그램을 다시 열면 선택이 유지됩니다.");

private:
	void RefreshRowBackground();
	UFUNCTION() void HandleSelectClicked();
	UFUNCTION() void HandleLocateClicked();
	int32 FacilityIndex = INDEX_NONE;
	FSlateBrush NormalBrush;
	bool bFacilitySelected = false;
	bool bPointerOverRow = false;
};
