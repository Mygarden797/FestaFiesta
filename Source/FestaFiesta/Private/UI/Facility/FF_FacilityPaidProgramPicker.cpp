// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityPaidProgramPicker.h"

#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/Facility/FF_FacilityPaidProgramCard.h"

#define LOCTEXT_NAMESPACE "FFFacilityPaidProgramPicker"

FSoftClassPath UFF_FacilityPaidProgramPicker::GetDefaultCardClassPath()
{
	return FSoftClassPath(TEXT(
		"/Game/FestaFiesta/UI/Facility/WBP_FF_FacilityPaidProgramCard."
		"WBP_FF_FacilityPaidProgramCard_C"));
}

void UFF_FacilityPaidProgramPicker::NativePreConstruct()
{
	Super::NativePreConstruct();
	// State colors remain editable in the WBP Class Defaults.
}

void UFF_FacilityPaidProgramPicker::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_ReturnToFacility)
		Button_ReturnToFacility->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
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
			LOCTEXT("NoRefundNotice", "오늘만 운영 · 당일 취소 및 환불 불가\n편성 후 시설 이동·강화·철거가 제한됩니다."));
	}
	ClearPreview();
}

void UFF_FacilityPaidProgramPicker::ApplyViewData(
	const FFF_FacilityPaidProgramPickerViewData& ViewData)
{
	CurrentViewData = ViewData;
	bCommitPending = false;
	bPurchaseCompleted = false;
	if (Border_Body) Border_Body->SetVisibility(ESlateVisibility::Visible);
	if (Border_Footer) Border_Footer->SetVisibility(ESlateVisibility::Visible);
	if (Panel_PurchaseComplete) Panel_PurchaseComplete->SetVisibility(ESlateVisibility::Collapsed);
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
				UFF_FacilityPaidProgramCard* Card = GetOwningPlayer()
					? CreateWidget<UFF_FacilityPaidProgramCard>(GetOwningPlayer(), CardClass)
					: CreateWidget<UFF_FacilityPaidProgramCard>(GetWorld(), CardClass);
				if (!Card)
				{
					continue;
				}
				Card->ApplyViewData(CardData);
				Card->OnSelected.AddUObject(this, &ThisClass::HandleCardSelected);
				if (UScrollBoxSlot* CardSlot = Cast<UScrollBoxSlot>(
					ScrollBox_Programs->AddChild(Card)))
				{
					CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
					CardSlot->SetHorizontalAlignment(HAlign_Fill);
				}
				SpawnedCards.Add(Card);
			}
		}
	}
	ClearPreview();
	// A sole compatible program is already the useful review selection. Selection itself
	// never broadcasts OnConfirmed; the separate purchase button still commits explicitly.
	if (ViewData.Cards.Num() == 1) HandleCardSelected(ViewData.Cards[0].ProgramId);
	OnViewDataApplied(ViewData);
}

void UFF_FacilityPaidProgramPicker::ShowPurchaseComplete(
	const FName ProgramId, const int32 RemainingGold, const int32 RemainingStaff)
{
	const FFF_FacilityPaidProgramCardViewData* Card = FindCard(ProgramId);
	if (!Card) return;
	bCommitPending = false;
	bPurchaseCompleted = true;
	const FText Receipt = FText::Format(
		LOCTEXT("PurchaseReceipt", "{0}\n{1}\n{2}\n\n결제 {3} Gold\n남은 보유금 {4} Gold · 가용 인력 {5}명\n\n오늘 프로그램이 편성되었습니다.\n당일 취소 및 환불은 불가합니다.\n이 시설의 이동·강화·철거가 제한됩니다."),
		Card->DisplayName, CurrentViewData.FacilityName, Card->TimeText,
		FText::AsNumber(Card->CostGold), FText::AsNumber(RemainingGold), FText::AsNumber(RemainingStaff));
	if (Panel_PurchaseComplete && Text_PurchaseReceipt && Button_ReturnToFacility)
	{
		if (Text_ReceiptProgramName) Text_ReceiptProgramName->SetText(Card->DisplayName);
		if (Text_ReceiptResources) Text_ReceiptResources->SetText(FText::Format(
			LOCTEXT("ReceiptResources", "남은 보유금 {0} Gold\n가용 인력 {1}명"),
			FText::AsNumber(RemainingGold), FText::AsNumber(RemainingStaff)));
		Text_PurchaseReceipt->SetText(Text_ReceiptProgramName && Text_ReceiptResources
			? FText::Format(LOCTEXT("ReceiptBody", "{0}\n{1}\n결제 {2} Gold\n\n오늘 프로그램이 편성되었습니다.\n당일 취소 및 환불은 불가합니다.\n이 시설의 이동·강화·철거가 제한됩니다."),
				CurrentViewData.FacilityName, Card->TimeText, FText::AsNumber(Card->CostGold))
			: Receipt);
		Panel_PurchaseComplete->SetVisibility(ESlateVisibility::Visible);
		if (Border_Body) Border_Body->SetVisibility(ESlateVisibility::Collapsed);
		if (Border_Footer) Border_Footer->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		if (Text_PreviewName) Text_PreviewName->SetText(LOCTEXT("PurchaseCompleted", "구매·편성 완료"));
		if (Text_PreviewDetails) Text_PreviewDetails->SetText(Receipt);
		if (Text_Confirm) Text_Confirm->SetText(LOCTEXT("ReturnToFacility", "시설 정보로 돌아가기"));
		if (Button_Confirm) Button_Confirm->SetIsEnabled(true);
		if (ScrollBox_Programs) ScrollBox_Programs->SetIsEnabled(false);
	}
}

void UFF_FacilityPaidProgramPicker::SetPickerOpen(const bool bOpen)
{
	SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

bool UFF_FacilityPaidProgramPicker::IsPointerOverInteractivePanel() const
{
	if (GetVisibility() == ESlateVisibility::Collapsed ||
		GetVisibility() == ESlateVisibility::Hidden || !Border_Picker)
	{
		return false;
	}
	return Border_Picker->IsHovered() ||
		(FSlateApplication::IsInitialized() &&
			Border_Picker->GetCachedGeometry().IsUnderLocation(
				FSlateApplication::Get().GetCursorPos()));
}

void UFF_FacilityPaidProgramPicker::HandleCardSelected(const FName ProgramId)
{
	if (bPurchaseCompleted || bCommitPending) return;
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
	const bool bSeparateMetrics = Text_PreviewTime && Text_RequiredStaff && Text_Capacity &&
		Text_TicketFee && Text_MaxRevenue && Text_PurchaseCost;
	if (Text_PreviewDetails) Text_PreviewDetails->SetVisibility(bSeparateMetrics
		? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (Text_PreviewDescription) Text_PreviewDescription->SetVisibility(bSeparateMetrics
		? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	if (Text_PreviewTime) Text_PreviewTime->SetText(CardData->TimeText);
	if (Text_RequiredStaff) Text_RequiredStaff->SetText(FText::Format(
		LOCTEXT("StaffMetric", "{0}명"), FText::AsNumber(CardData->RequiredStaff)));
	if (Text_Capacity) Text_Capacity->SetText(FText::Format(
		LOCTEXT("CapacityMetric", "{0}명"), FText::AsNumber(CardData->CapacityPeople)));
	if (Text_TicketFee) Text_TicketFee->SetText(FText::Format(
		LOCTEXT("TicketFeeMetric", "{0} Gold"), FText::AsNumber(CardData->TicketFeeGold)));
	if (Text_MaxRevenue) Text_MaxRevenue->SetText(FText::Format(
		LOCTEXT("MaxRevenueMetric", "최대 매출 {0} Gold\n정원 모두 참여 시 · 확정 수익이 아닙니다"),
		FText::AsNumber(CardData->MaxRevenueGold)));
	if (Text_PurchaseCost) Text_PurchaseCost->SetText(FText::Format(
		LOCTEXT("PurchaseCostMetric", "{0} Gold"), FText::AsNumber(CardData->CostGold)));
	if (Text_PreviewAvailability)
	{
		Text_PreviewAvailability->SetText(CardData->AvailabilityText);
		Text_PreviewAvailability->SetColorAndOpacity(FSlateColor(
			CardData->bCanSchedule ? FeedbackSuccessColor : FeedbackErrorColor));
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
	if (Text_ResourcePreview)
	{
		const int32 GoldAfter = CurrentViewData.AvailableGold - CardData->CostGold;
		const int32 StaffAfter = CurrentViewData.AvailableStaff - CardData->RequiredStaff;
		const FText GoldResult = GoldAfter >= 0
			? FText::Format(LOCTEXT("GoldAfterPurchase", "{0} Gold"), FText::AsNumber(GoldAfter))
			: FText::Format(LOCTEXT("GoldShortfall", "{0} Gold 부족"), FText::AsNumber(-GoldAfter));
		const FText StaffResult = StaffAfter >= 0
			? FText::Format(LOCTEXT("StaffAfterPurchase", "{0}명"), FText::AsNumber(StaffAfter))
			: FText::Format(LOCTEXT("StaffShortfall", "{0}명 부족"), FText::AsNumber(-StaffAfter));
		Text_ResourcePreview->SetText(FText::Format(
			LOCTEXT("ResourcePreview", "보유금 {0} Gold → {1}\n가용 인력 {2}명 → {3}"),
			FText::AsNumber(CurrentViewData.AvailableGold),
			GoldResult,
			FText::AsNumber(CurrentViewData.AvailableStaff),
			StaffResult));
	}
}

void UFF_FacilityPaidProgramPicker::HandleConfirmClicked()
{
	if (bPurchaseCompleted) { HandleCloseClicked(); return; }
	if (bCommitPending) return;
	const FFF_FacilityPaidProgramCardViewData* CardData = FindCard(SelectedProgramId);
	if (CardData && CardData->bCanSchedule && CurrentViewData.FacilityInstanceId.IsValid())
	{
		bCommitPending = true;
		if (Button_Confirm) Button_Confirm->SetIsEnabled(false);
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
	for (UCommonTextBlock* Metric : {Text_PreviewTime.Get(), Text_RequiredStaff.Get(), Text_Capacity.Get(),
		Text_TicketFee.Get(), Text_MaxRevenue.Get(), Text_PurchaseCost.Get()})
		if (Metric) Metric->SetText(FText::FromString(TEXT("—")));
	if (ScrollBox_Programs) ScrollBox_Programs->SetIsEnabled(true);
	if (Text_ResourcePreview) Text_ResourcePreview->SetText(FText::Format(
		LOCTEXT("CurrentResources", "보유금 {0} Gold · 가용 인력 {1}명"),
		FText::AsNumber(CurrentViewData.AvailableGold), FText::AsNumber(CurrentViewData.AvailableStaff)));
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
