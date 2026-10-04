#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "FF_LiveSnsPostEntry.generated.h"

class UTextBlock;
class UBorder;

/** 데이터만 갱신하고 트리·폰트·여백은 WBP가 소유하는 게시물 한 행. */
UCLASS(Abstract)
class FESTAFIESTA_API UFF_LiveSnsPostEntry : public UUserWidget
{
	GENERATED_BODY()
public:
	UFF_LiveSnsPostEntry(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live|SNS")
	void ConfigurePost(const FFF_LiveSocialPost& Post);
protected:
	virtual void NativePreConstruct() override;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_Author;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_Body;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_Sentiment;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_SentimentSymbol;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UBorder> Border_Sentiment;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UBorder> Border_Card;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	FLinearColor PositiveColor = FLinearColor(0.075f, 0.28f, 0.21f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	FLinearColor NegativeColor = FLinearColor(0.38f, 0.15f, 0.025f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	FLinearColor NeutralColor = FLinearColor(0.20f, 0.25f, 0.23f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	FLinearColor UnknownColor = FLinearColor(0.20f, 0.25f, 0.23f, 1.f);
	/** 배지의 반응별 문구·기호·배경. 글꼴과 크기는 Designer 값을 그대로 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	TMap<EFF_LiveSnsSentiment, FLinearColor> SentimentBackgroundColors;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Text")
	TMap<EFF_LiveSnsSentiment, FText> SentimentLabels;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Text")
	TMap<EFF_LiveSnsSentiment, FText> SentimentSymbols;
private:
	void RefreshPost();
	FFF_LiveSocialPost CurrentPost;
	bool bHasPost = false;
};
