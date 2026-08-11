// FF_BuildRemovalConfirmWidget.cpp

#include "Building/UI/FF_BuildRemovalConfirmWidget.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FFBuildRemovalConfirmWidget"

void UFF_BuildRemovalConfirmWidget::SetFacilityName(const FText& InFacilityName)
{
	FacilityName = InFacilityName;
	Feedback = FText::GetEmpty();
	bFeedbackIsError = false;
	RefreshTextPresentation();
}

void UFF_BuildRemovalConfirmWidget::SetFeedback(const FText& InFeedback, const bool bIsError)
{
	Feedback = InFeedback;
	bFeedbackIsError = bIsError;
	RefreshTextPresentation();
}

TSharedRef<SWidget> UFF_BuildRemovalConfirmWidget::RebuildWidget()
{
	TSharedRef<SWidget> Root =
		SNew(SOverlay)
		.Visibility(EVisibility::HitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(24.0f, 24.0f, 24.0f, 72.0f))
		[
			SNew(SBox)
			.MaxDesiredWidth(720.0f)
			[
				SNew(SBorder)
				.Padding(FMargin(24.0f, 16.0f))
				.BorderBackgroundColor(FLinearColor(0.045f, 0.025f, 0.03f, 0.94f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(PromptTextBlock, STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 20))
						.ColorAndOpacity(FLinearColor(1.0f, 0.82f, 0.82f))
						.Justification(ETextJustify::Center)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT(
							"RemovalHint",
							"같은 시설을 다시 클릭하거나 Enter: 철거  |  우클릭: 취소"))
						.ColorAndOpacity(FLinearColor(0.92f, 0.95f, 1.0f))
						.Justification(ETextJustify::Center)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 6.0f, 0.0f, 0.0f)
					[
						SAssignNew(FeedbackTextBlock, STextBlock)
						.Justification(ETextJustify::Center)
					]
				]
			]
		];

	RefreshTextPresentation();
	return Root;
}

void UFF_BuildRemovalConfirmWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	PromptTextBlock.Reset();
	FeedbackTextBlock.Reset();
}

FText UFF_BuildRemovalConfirmWidget::MakePromptText() const
{
	const FText Name = FacilityName.IsEmpty()
		? LOCTEXT("UnknownFacility", "선택한 시설")
		: FacilityName;
	return FText::Format(LOCTEXT("RemovalPrompt", "‘{0}’ 시설을 철거할까요?"), Name);
}

void UFF_BuildRemovalConfirmWidget::RefreshTextPresentation()
{
	if (PromptTextBlock.IsValid())
	{
		PromptTextBlock->SetText(MakePromptText());
	}
	if (FeedbackTextBlock.IsValid())
	{
		FeedbackTextBlock->SetText(Feedback);
		FeedbackTextBlock->SetColorAndOpacity(
			bFeedbackIsError
				? FLinearColor(1.0f, 0.35f, 0.3f)
				: FLinearColor(0.35f, 1.0f, 0.65f));
		FeedbackTextBlock->SetVisibility(
			Feedback.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
	}
}

#undef LOCTEXT_NAMESPACE
