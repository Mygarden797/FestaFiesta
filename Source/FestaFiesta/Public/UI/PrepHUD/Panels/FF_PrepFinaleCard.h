#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "UI/PrepHUD/Panels/FF_PrepDetailCard.h"
#include "FF_PrepFinaleCard.generated.h"

class UBorder;
class UImage;
class UTexture2D;

/** 연출 그림과 설명만 UI가 소유한다. 가격과 보너스는 축제 정의에서 읽는다. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FinaleTierPresentation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale")
	TObjectPtr<UTexture2D> PreviewTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale")
	FText Description;

	/**
	 * 등급 그림을 넣는다. 그림 칸은 원래 크기를 비율대로 줄이는 ScaleBox 라 브러시 크기가 0 이면 그림도 0 이 된다.
	 * 렌더링 없이(NullRHI) 만든 WBP 는 "텍스처 크기에 맞춤" 이 0×0 으로 저장됐다 — 비어 있으면 텍스처 크기를 쓴다.
	 * `SetBrushFromTexture` 는 같은 텍스처면 아무것도 안 하므로 브러시를 복사해 다시 넣는다.
	 */
	static void ApplyPreview(UImage* Image, UTexture2D* Texture);
};

UCLASS(Abstract, meta = (DisplayName = "FF Prep Finale Card"))
class FESTAFIESTA_API UFF_PrepFinaleCard : public UFF_PrepDetailCard
{
	GENERATED_BODY()

public:
	UFF_PrepFinaleCard();
	void ConfigureTier(const FFF_FinaleTier& Tier, const FFF_FinaleTierPresentation& Presentation);
	void ApplyFinaleState(EFF_PrepCardState State, const FText& BadgeText);
	EFF_FinaleGrade GetGrade() const { return Grade; }

protected:
	virtual void CollectStateTexts(TArray<UTextBlock*>& OutTexts) const override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UImage> Image_Preview;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UBorder> Border_Badge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_Grade;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_Description;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_Cost;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_Bonus;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText TitleFormat = NSLOCTEXT("FestaFiestaFinale", "CardTitle", "피날레 {0}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText BonusFormat = NSLOCTEXT("FestaFiestaFinale", "Bonus", "평가 만족도 +{0}");

private:
	EFF_FinaleGrade Grade = EFF_FinaleGrade::None;
};
