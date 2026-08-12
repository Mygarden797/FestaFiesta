// FF_BuildRemovalConfirmWidget.cpp

#include "Building/UI/FF_BuildRemovalConfirmWidget.h"

#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
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
		// Ignore the non-interactive overlay itself while preserving child hit tests.
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(24.0f, 24.0f, 24.0f, 72.0f))
		[
			SNew(SBox)
			.WidthOverride(620.0f)
			.MaxDesiredWidth(720.0f)
			[
				SNew(SBorder)
				.Padding(FMargin(24.0f, 18.0f))
				.BorderBackgroundColor(FLinearColor(0.025f, 0.04f, 0.065f, 0.96f))
				.OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent& MouseEvent)
				{
					// Prevent clicks on panel chrome/text from reaching the world. Right-click
					// remains unhandled so the build-mode cancellation shortcut still works.
					return MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
						? FReply::Handled()
						: FReply::Unhandled();
				})
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ActionTitle", "시설 작업"))
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 13))
						.ColorAndOpacity(FLinearColor(0.6f, 0.76f, 0.96f))
						.Justification(ETextJustify::Center)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 5.0f, 0.0f, 0.0f)
					[
						SAssignNew(PromptTextBlock, STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 20))
						.ColorAndOpacity(FLinearColor::White)
						.Justification(ETextJustify::Center)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ActionHint", "원하는 작업을 선택하세요."))
						.ColorAndOpacity(FLinearColor(0.82f, 0.86f, 0.92f))
						.Justification(ETextJustify::Center)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 14.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(0.0f, 0.0f, 5.0f, 0.0f)
						[
							SNew(SButton)
							.ContentPadding(FMargin(18.0f, 10.0f))
							.ButtonColorAndOpacity(FLinearColor(0.08f, 0.45f, 0.76f, 1.0f))
							.ForegroundColor(FLinearColor::White)
							.HAlign(HAlign_Center)
							.OnClicked_UObject(this, &UFF_BuildRemovalConfirmWidget::HandleMoveClicked)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("MoveButton", "이동 (M)"))
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14))
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(5.0f, 0.0f)
						[
							SNew(SButton)
							.ContentPadding(FMargin(18.0f, 10.0f))
							.ButtonColorAndOpacity(FLinearColor(0.72f, 0.09f, 0.08f, 1.0f))
							.ForegroundColor(FLinearColor::White)
							.HAlign(HAlign_Center)
							.OnClicked_UObject(this, &UFF_BuildRemovalConfirmWidget::HandleRemoveClicked)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("RemoveButton", "철거 (Enter)"))
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14))
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(5.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.ContentPadding(FMargin(18.0f, 10.0f))
							.ButtonColorAndOpacity(FLinearColor(0.25f, 0.28f, 0.33f, 1.0f))
							.ForegroundColor(FLinearColor::White)
							.HAlign(HAlign_Center)
							.OnClicked_UObject(this, &UFF_BuildRemovalConfirmWidget::HandleCancelClicked)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("CancelButton", "취소"))
								.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 14))
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 11.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("KeyboardHint", "M: 이동 | Enter: 철거 | 우클릭: 취소"))
						.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 11))
						.ColorAndOpacity(FLinearColor(0.62f, 0.68f, 0.76f))
						.Justification(ETextJustify::Center)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 8.0f, 0.0f, 0.0f)
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
	return FText::Format(LOCTEXT("FacilityPrompt", "{0}"), Name);
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

FReply UFF_BuildRemovalConfirmWidget::HandleMoveClicked()
{
	OnMoveRequested.Broadcast();
	return FReply::Handled();
}

FReply UFF_BuildRemovalConfirmWidget::HandleRemoveClicked()
{
	OnRemoveRequested.Broadcast();
	return FReply::Handled();
}

FReply UFF_BuildRemovalConfirmWidget::HandleCancelClicked()
{
	OnCancelRequested.Broadcast();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
