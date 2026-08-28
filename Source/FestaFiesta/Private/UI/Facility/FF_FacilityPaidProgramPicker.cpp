// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityPaidProgramPicker.h"

#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "UI/Facility/FF_FacilityPaidProgramCard.h"

#define LOCTEXT_NAMESPACE "FFFacilityPaidProgramPicker"

FSoftClassPath UFF_FacilityPaidProgramPicker::GetDefaultCardClassPath()
{
	return FSoftClassPath(TEXT(
		"/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramCard."
		"WBP_FF_FacilityPaidProgramCard_C"));
}

void UFF_FacilityPaidProgramPicker::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	if (Text_NoRefundNotice)
	{
		Text_NoRefundNotice->SetText(
			LOCTEXT("NoRefundNotice", "구매·편성 후 당일 취소 및 환불은 불가합니다."));
	}
	ClearPreview();
}

void UFF_FacilityPaidProgramPicker::ApplyViewData(
	const FFF_FacilityPaidProgramPickerViewData& ViewData)
{
	CurrentViewData = ViewData;
	SelectedProgramId = NAME_None;
	if (Text_FacilityName)
	{
		Text_FacilityName->SetText(FText::Format(
			LOCTEXT("FacilityContext", "{0} 전용 프로그램"), ViewData.FacilityName));
	}
	if (Text_Empty)
	{
		Text_Empty->SetVisibility(
			ViewData.Cards.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Text_Empty->SetText(LOCTEXT(
			"NoCompatiblePrograms", "이 시설에서 운영할 수 있는 유료 프로그램이 없습니다."));
	}
	if (Text_Feedback)
	{
		Text_Feedback->SetText(ViewData.FeedbackText);
		Text_Feedback->SetVisibility(
			ViewData.FeedbackText.IsEmpty()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::HitTestInvisible);
		Text_Feedback->SetColorAndOpacity(FSlateColor(
			ViewData.bFeedbackIsError ? FeedbackErrorColor : FeedbackSuccessColor));
	}

	SpawnedCards.Reset();
	if (ScrollBox_Programs)
	{
		ScrollBox_Programs->ClearChildren();
		UClass* CardClass = CardWidgetClass
			? CardWidgetClass.Get()
			: GetDefaultCardClassPath().TryLoadClass<UFF_FacilityPaidProgramCard>();
		if (CardClass && CardClass->IsChildOf(UFF_FacilityPaidProgramCard::StaticClass()))
		{
			for (const FFF_FacilityPaidProgramCardViewData& CardData : ViewData.Cards)
			{
				UFF_FacilityPaidProgramCard* Card = CreateWidget<UFF_FacilityPaidProgramCard>(
					GetOwningPlayer(), CardClass);
				if (!Card)
				{
					continue;
				}
				Card->ApplyViewData(CardData);
				Card->OnSelected.AddUObject(this, &ThisClass::HandleCardSelected);
				ScrollBox_Programs->AddChild(Card);
				SpawnedCards.Add(Card);
			}
		}
	}
	ClearPreview();
}

void UFF_FacilityPaidProgramPicker::SetPickerOpen(const bool bOpen)
{
	SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

bool UFF_FacilityPaidProgramPicker::IsPointerOverInteractivePanel() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && Border_Picker && Border_Picker->IsHovered();
}

void UFF_FacilityPaidProgramPicker::HandleCardSelected(const FName ProgramId)
{
	const FFF_FacilityPaidProgramCardViewData* CardData = FindCard(ProgramId);
	if (!CardData)
	{
		return;
	}
	SelectedProgramId = ProgramId;
	for (UFF_FacilityPaidProgramCard* Card : SpawnedCards)
	{
		if (Card)
		{
			Card->SetSelected(Card->GetProgramId() == ProgramId);
		}
	}
	if (Text_PreviewName)
	{
		Text_PreviewName->SetText(CardData->DisplayName);
	}
	if (Text_PreviewDescription)
	{
		Text_PreviewDescription->SetText(CardData->Description);
	}
	if (Text_PreviewDetails)
	{
		Text_PreviewDetails->SetText(FText::Format(
			LOCTEXT("PreviewDetails", "{0}\n{1} · {2}\n{3} · {4}"),
			CardData->TimeText,
			CardData->StaffText,
			CardData->CapacityText,
			CardData->TicketFeeText,
			CardData->CostText));
	}
	if (Text_PreviewAvailability)
	{
		Text_PreviewAvailability->SetText(CardData->AvailabilityText);
	}
	if (Text_Confirm)
	{
		Text_Confirm->SetText(FText::Format(
			LOCTEXT("ConfirmWithCost", "{0} Gold로 구매·편성"),
			FText::AsNumber(CardData->CostGold)));
	}
	if (Button_Confirm)
	{
		Button_Confirm->SetIsEnabled(CardData->bCanSchedule);
	}
}

void UFF_FacilityPaidProgramPicker::HandleConfirmClicked()
{
	const FFF_FacilityPaidProgramCardViewData* CardData = FindCard(SelectedProgramId);
	if (CardData && CardData->bCanSchedule && CurrentViewData.FacilityInstanceId.IsValid())
	{
		OnConfirmed.Broadcast(CardData->ProgramId, CurrentViewData.FacilityInstanceId);
	}
}

void UFF_FacilityPaidProgramPicker::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

const FFF_FacilityPaidProgramCardViewData* UFF_FacilityPaidProgramPicker::FindCard(
	const FName ProgramId) const
{
	return CurrentViewData.Cards.FindByPredicate(
		[ProgramId](const FFF_FacilityPaidProgramCardViewData& Card)
		{
			return Card.ProgramId == ProgramId;
		});
}

void UFF_FacilityPaidProgramPicker::ClearPreview()
{
	SelectedProgramId = NAME_None;
	if (Text_PreviewName)
	{
		Text_PreviewName->SetText(LOCTEXT("SelectProgram", "프로그램을 선택하세요"));
	}
	if (Text_PreviewDescription)
	{
		Text_PreviewDescription->SetText(LOCTEXT(
			"SelectionIsPreview", "카드 선택만으로는 결제되지 않습니다."));
	}
	if (Text_PreviewDetails)
	{
		Text_PreviewDetails->SetText(FText::GetEmpty());
	}
	if (Text_PreviewAvailability)
	{
		Text_PreviewAvailability->SetText(FText::GetEmpty());
	}
	if (Text_Confirm)
	{
		Text_Confirm->SetText(LOCTEXT("ConfirmDisabled", "구매·편성"));
	}
	if (Button_Confirm)
	{
		Button_Confirm->SetIsEnabled(false);
	}
}

#undef LOCTEXT_NAMESPACE
