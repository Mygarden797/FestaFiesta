#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "UI/PrepHUD/Panels/FF_PrepRecommendedFacilities.h"
#include "FF_PrepRecommendedFacilityRow.generated.h"

class UButton;
class UImage;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FFF_OnRecommendedFacilityPlaceRequested, EFF_PrepRecommendedFacilityKind);

/** 권장 시설 안내 한 행. 문구·종류는 인스턴스 Details, 크기·색·간격은 행 WBP Designer에서 편집한다. */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "FF Prep Recommended Facility Row"))
class FESTAFIESTA_API UFF_PrepRecommendedFacilityRow : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recommended Facility")
	EFF_PrepRecommendedFacilityKind Kind = EFF_PrepRecommendedFacilityKind::Lighting;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recommended Facility")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recommended Facility")
	FText TitleText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recommended Facility", meta = (MultiLine = true))
	FText DescriptionText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recommended Facility")
	FText ActionText;

	/** 작은 카드에서는 설명을 배치 버튼의 툴팁으로 옮긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recommended Facility")
	bool bCompactPresentation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recommended Facility", meta = (EditCondition = "bCompactPresentation"))
	FText CompactTitleText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recommended Facility", meta = (EditCondition = "bCompactPresentation"))
	FText CompactActionText = NSLOCTEXT("FestaFiestaPrepCheck", "RecommendedCompactAction", "배치하기 ›");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recommended Facility")
	FSlateBrush IconBrush;

	void SetPlacementEnabled(bool bCanPlace, const FText& Reason);
	FFF_OnRecommendedFacilityPlaceRequested OnPlaceRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void SynchronizeProperties() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_Place;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Title;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Description;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Action;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_Icon;

	UFUNCTION() void HandlePlaceClicked();

private:
	void ApplyPresentation();
	void ApplyPlacementState();

	bool bHasPlacementState = false;
	bool bPlacementAllowed = true;
	FText PlacementDisabledReason;
};
