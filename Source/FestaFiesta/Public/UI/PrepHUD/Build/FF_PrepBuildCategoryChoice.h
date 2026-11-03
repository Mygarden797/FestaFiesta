#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "FF_PrepBuildCategoryChoice.generated.h"

class UTexture2D;
class UBorder;

/** Saved catalog tabs expose their preview and identity in the Widget Blueprint Designer. */
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Catalog Category Choice"))
class FESTAFIESTA_API UFF_PrepBuildCategoryChoice : public UFF_PrepChoiceButton
{
	GENERATED_BODY()

public:
	/** -1 is All; other values are EFFZSFacilityCategory values. Runtime minor choices set their own ID. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab", meta = (DisplayName = "분류 ID (-1: 전체)"))
	int32 AuthoredChoiceId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab", meta = (DisplayName = "탭 이름"))
	FText AuthoredLabel = NSLOCTEXT("FestaFiestaPrepBuild", "FilterAll", "전체");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab", meta = (DisplayName = "탭 아이콘"))
	FSlateBrush AuthoredIcon;

	/** Major-category rails keep the localized label as a tooltip; minor choices retain captions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab", meta = (DisplayName = "아이콘만 표시"))
	bool bIconOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab", meta = (ClampMin = "1", DisplayName = "아이콘 전용 크기"))
	float IconOnlySize = 22.f;

	/** Optional generated icon atlas. Empty retains the authored brush unchanged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab|Atlas")
	TObjectPtr<UTexture2D> IconAtlas;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab|Atlas", meta = (ClampMin = "1", ClampMax = "128"))
	int32 IconAtlasColumns = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab|Atlas", meta = (ClampMin = "1", ClampMax = "128"))
	int32 IconAtlasRows = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab|Atlas", meta = (ClampMin = "0"))
	int32 IconAtlasIndex = 0;

	/** Keep the image's original colours while preserving normal label and selected-state styling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab|Atlas")
	bool bPreserveIconColours = false;

	/** Resolved from Designer settings with the original brush as a safe fallback. */
	FSlateBrush GetEffectiveIcon() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog Tab", meta = (DisplayName = "디자이너에서 선택 표시"))
	bool bPreviewSelected = false;

protected:
	/** Optional authored background keeps dark generated art readable on a selected tab. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_IconBackdrop;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void RefreshVisualState() override;

private:
	void RefreshIconOnlyLayout();

	ESlateVisibility AuthoredLabelVisibility = ESlateVisibility::SelfHitTestInvisible;
	bool bCapturedLabelVisibility = false;
	bool bCapturedIconOnlyLayout = false;
	bool bAppliedIconOnlyLayout = false;
	FMargin AuthoredNormalPadding;
	FMargin AuthoredPressedPadding;
	FMargin AuthoredButtonSlotPadding;
	EHorizontalAlignment AuthoredButtonHorizontalAlignment = HAlign_Center;
	EVerticalAlignment AuthoredButtonVerticalAlignment = VAlign_Center;
	FMargin AuthoredIconSlotPadding;
	FSlateChildSize AuthoredIconSlotSize;
	EHorizontalAlignment AuthoredIconHorizontalAlignment = HAlign_Fill;
	EVerticalAlignment AuthoredIconVerticalAlignment = VAlign_Center;
};
