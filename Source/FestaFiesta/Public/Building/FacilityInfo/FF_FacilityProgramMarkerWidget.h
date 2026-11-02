// FF_FacilityProgramMarkerWidget.h
// 역할: 시설 위에 프로그램과 수입 상태를 요약해서 표시한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_FacilityProgramMarkerWidget.generated.h"

UENUM(BlueprintType)
enum class EFF_FacilityProgramMarkerState : uint8
{
	Hidden,
	Available,
	Scheduled
};

class UTextBlock;
class UWidgetSwitcher;
class SBox;

/** Authored, non-interactive program/revenue rows; native calendar fallback remains asset-independent. */
UCLASS(Blueprintable)
class FESTAFIESTA_API UFF_FacilityProgramMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFF_FacilityProgramMarkerWidget(const FObjectInitializer& ObjectInitializer);
	void SetMarkerState(EFF_FacilityProgramMarkerState InState);
	EFF_FacilityProgramMarkerState GetMarkerState() const { return MarkerState; }
	void SetRevenue(bool bInShowRevenue, bool bInEconomyAvailable, int32 InTodayGold);
	FText GetRevenueText() const;
	bool HasRevenueRow() const { return bShowRevenue; }
	FMargin GetViewportSafePadding() const { return ViewportSafePadding; }
	float GetBadgeSeparation() const { return BadgeSeparation; }
	float GetMaximumDisplacement() const { return MaximumDisplacement; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativePreConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> Panel_Program;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidgetSwitcher> Switcher_Program;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidget> Panel_Revenue;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_RevenueValue;
	/** Logical viewport units, after DPI scaling. HUD and panel geometry adds further exclusions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout|World Badge")
	FMargin ViewportSafePadding = FMargin(16, 80, 16, 116);
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout|World Badge", meta=(ClampMin="0"))
	float BadgeSeparation = 8.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout|World Badge", meta=(ClampMin="0"))
	float MaximumDisplacement = 280.f;

private:
	void RefreshPresentation();
	EFF_FacilityProgramMarkerState MarkerState = EFF_FacilityProgramMarkerState::Hidden;
	FText Label;
	FSlateFontInfo LabelFont;
	bool bShowRevenue = false;
	bool bEconomyAvailable = false;
	bool bAuthoredLayout = false;
	int32 TodayGold = 0;
	TSharedPtr<SBox> NativeBounds;

	UPROPERTY(Transient)
	TObjectPtr<UObject> FontAsset;
};
