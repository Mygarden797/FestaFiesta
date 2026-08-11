// FF_BuildRemovalConfirmWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_BuildRemovalConfirmWidget.generated.h"

class STextBlock;

// 철거 대상을 월드에서 계속 보면서 두 번째 확인 입력을 할 수 있게 하는 제품용 안내 패널이다.
// Blueprint 자산이 없어도 Slate 폴백을 직접 구성하므로 Shipping에서도 시설명과 조작법이 표시된다.
UCLASS(NotBlueprintable, Transient)
class FESTAFIESTA_API UFF_BuildRemovalConfirmWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetFacilityName(const FText& InFacilityName);
	void SetFeedback(const FText& InFeedback, bool bIsError);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	FText FacilityName;
	FText Feedback;
	bool bFeedbackIsError = false;
	TSharedPtr<STextBlock> PromptTextBlock;
	TSharedPtr<STextBlock> FeedbackTextBlock;

	FText MakePromptText() const;
	void RefreshTextPresentation();
};
