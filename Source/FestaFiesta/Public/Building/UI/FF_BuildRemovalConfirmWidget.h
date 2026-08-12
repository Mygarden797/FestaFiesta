// FF_BuildRemovalConfirmWidget.h

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_BuildRemovalConfirmWidget.generated.h"

class STextBlock;

// 선택한 시설의 이동/철거/취소 작업을 월드 화면 위에서 제공하는 제품용 액션 패널이다.
// Blueprint 자산이 없어도 Slate 폴백을 직접 구성하므로 Shipping에서도 시설명과 조작법이 표시된다.
UCLASS(NotBlueprintable, Transient)
class FESTAFIESTA_API UFF_BuildRemovalConfirmWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	// Build-mode code binds to these native events with UObject-aware delegate handles.
	FSimpleMulticastDelegate OnMoveRequested;
	FSimpleMulticastDelegate OnRemoveRequested;
	FSimpleMulticastDelegate OnCancelRequested;

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
	FReply HandleMoveClicked();
	FReply HandleRemoveClicked();
	FReply HandleCancelClicked();
};
