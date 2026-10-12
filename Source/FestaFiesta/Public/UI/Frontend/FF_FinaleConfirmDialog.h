#pragma once

#include "CoreMinimal.h"
#include "UI/Frontend/FF_ConfirmDialog.h"
#include "UI/PrepHUD/Panels/FF_PrepFinaleCard.h"
#include "FF_FinaleConfirmDialog.generated.h"

class UButton;
class UImage;

/** 피날레 확인창. 확인/취소·중복 입력·기본 취소 포커스는 공용 확인창을 따른다. */
UCLASS(Abstract, meta = (DisplayName = "FF Finale Confirm Dialog"))
class FESTAFIESTA_API UFF_FinaleConfirmDialog : public UFF_ConfirmDialog
{
	GENERATED_BODY()

public:
	void ConfigureFinale(const FFF_FinaleTier& Tier, const FFF_FinaleTierPresentation& Presentation,
		int32 GoldAfter, bool bCanConfirm, const FText& BlockReason);
	void SetPurchaseAvailability(bool bCanConfirm, const FText& BlockReason);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UFUNCTION() void HandleCloseClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UImage> Image_Preview;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_Grade;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_Bonus;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_Cost;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_GoldAfter;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_BlockReason;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText TitleFormat = NSLOCTEXT("FestaFiestaFinale", "ConfirmTitle", "피날레 {0}를 구매할까요?");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText BodyText = NSLOCTEXT("FestaFiestaFinale", "ConfirmBody", "축제당 한 번만 구매할 수 있습니다.\n구매 후에는 변경하거나 취소할 수 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText GradeFormat = NSLOCTEXT("FestaFiestaFinale", "ConfirmGrade", "피날레 {0}");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText BonusFormat = NSLOCTEXT("FestaFiestaFinale", "Bonus", "평가 만족도 +{0}");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText CancelLabel = NSLOCTEXT("FestaFiestaFinale", "Cancel", "돌아가기");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText ConfirmLabel = NSLOCTEXT("FestaFiestaFinale", "Confirm", "구매 확정");
};
