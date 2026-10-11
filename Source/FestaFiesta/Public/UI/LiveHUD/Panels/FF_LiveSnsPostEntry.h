#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "FF_LiveSnsPostEntry.generated.h"

class UTextBlock;
class UBorder;
class UImage;
class UTexture2D;
class SFF_LiveSnsReadButton;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLiveSnsPostActivated, int64);

/**
 * 데이터만 갱신하고 트리·폰트·여백은 WBP가 소유하는 게시물 한 행.
 * 기존 카드형 행(`WBP_FF_LiveSnsPostEntry`)과 도크 SNS의 구분선형 행(`WBP_FF_LiveSnsFlyoutPostRow`)이 같이 쓴다.
 * 반응은 아이콘(또는 기호)과 문구를 함께 바꾸고 색은 보조로만 쓴다(AT-011).
 */
UCLASS(Abstract)
class FESTAFIESTA_API UFF_LiveSnsPostEntry : public UUserWidget
{
	GENERATED_BODY()
public:
	UFF_LiveSnsPostEntry(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live|SNS")
	void ConfigurePost(const FFF_LiveSocialPost& Post);
	/** 통합 소식의 도착 번호와 개별 읽음 입력을 연결한다. 행 재사용 시 매번 갱신한다. */
	void ConfigureReadInteraction(int64 InSerial, bool bEnabled);
	FOnLiveSnsPostActivated OnPostActivated;
	/** 새 글 빨간 점. `Image_NewDot` 이 없는 행(기존 카드형)은 아무것도 안 한다. */
	void SetNewMark(bool bNew);
	bool HasNewMark() const { return bNewMark; }
	/** 구분선형 행 최초 제작용. 이후 아이콘·색은 Class Defaults 에서 고친다. */
	void ConfigureIconRowDefaults(const TMap<EFF_LiveSnsSentiment, UTexture2D*>& Icons,
		const FLinearColor& Positive, const FLinearColor& Negative, const FLinearColor& Other);
	void ConfigureNewsRowDefaults(UTexture2D* InSocialIcon, UTexture2D* InOperationIcon);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_Author;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_Body;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_Sentiment;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_SentimentSymbol;
	/** 반응 아이콘. 크기는 Designer 브러시 크기를 쓰고 그림만 반응별로 바꾼다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UImage> Image_Sentiment;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UBorder> Border_Sentiment;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UBorder> Border_Card;
	/** 이름 옆 새 글 점. 크기·색은 Designer 에서 고친다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UWidget> Image_NewDot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> Image_Avatar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_Avatar;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Live|News") bool bShowSourceLabel = false;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Live|News") TObjectPtr<UTexture2D> SocialIcon;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Live|News") TObjectPtr<UTexture2D> OperationIcon;
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
	/** `Image_Sentiment` 에 넣을 반응별 아이콘. 비어 있는 반응은 아이콘을 숨기고 문구만 보인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	TMap<EFF_LiveSnsSentiment, TObjectPtr<UTexture2D>> SentimentIcons;
	/** 켜면 반응 아이콘을 반응 글자색으로 칠한다. 끄면 Designer 의 아이콘 색을 그대로 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	bool bTintSentimentIcon = true;
private:
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
	friend class FFF_LiveSnsPostReadInteractionTest;
#endif
	void RefreshPost();
	void RefreshReadToolTip();
	void HandleReadPressed();
	FReply HandleReadClicked();
	TSharedPtr<SFF_LiveSnsReadButton> ReadButton;
	int64 PostSerial = 0;
	int64 PressedPostSerial = 0;
	bool bReadInteractionEnabled = false;
	bool bReadPressValid = false;
	FFF_LiveSocialPost CurrentPost;
	bool bHasPost = false;
	bool bNewMark = false;
};
