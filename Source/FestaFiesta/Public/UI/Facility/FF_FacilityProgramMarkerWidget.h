// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_FacilityProgramMarkerWidget.generated.h"

UENUM()
enum class EFF_FacilityProgramMarkerState : uint8
{
	Hidden,
	Available,
	Scheduled
};

/** Asset-independent, non-interactive calendar badge above an installed program facility. */
UCLASS()
class FESTAFIESTA_API UFF_FacilityProgramMarkerWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	UFF_FacilityProgramMarkerWidget(const FObjectInitializer& ObjectInitializer);
	void SetMarkerState(EFF_FacilityProgramMarkerState InState);
	EFF_FacilityProgramMarkerState GetMarkerState() const { return MarkerState; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	EFF_FacilityProgramMarkerState MarkerState = EFF_FacilityProgramMarkerState::Hidden;
	FText Label;
	FSlateFontInfo LabelFont;

	UPROPERTY(Transient)
	TObjectPtr<UObject> FontAsset;
};
