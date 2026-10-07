#include "UI/Frontend/FF_FinaleConfirmDialog.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Economy/FF_EconomyDisplayTextLibrary.h"
#include "UI/FF_CommonButtonBase.h"

void UFF_FinaleConfirmDialog::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &UFF_FinaleConfirmDialog::HandleCloseClicked);
	}
	if (Button_Cancel) Button_Cancel->SetButtonLabel(CancelLabel);
}

void UFF_FinaleConfirmDialog::NativeDestruct()
{
	if (Button_Close)
	{
		Button_Close->OnClicked.RemoveDynamic(this, &UFF_FinaleConfirmDialog::HandleCloseClicked);
	}
	Super::NativeDestruct();
}

void UFF_FinaleConfirmDialog::HandleCloseClicked()
{
	// 공용 취소 경로를 타야 풀 재사용 시에도 결과가 한 번만 전달된다.
	HandleButtonClicked(Button_Cancel);
}

void UFF_FinaleConfirmDialog::ConfigureFinale(const FFF_FinaleTier& Tier,
	const FFF_FinaleTierPresentation& Presentation, int32 GoldAfter,
	bool bCanConfirm, const FText& BlockReason)
{
	const FText GradeText = UFF_EconomyDisplayTextLibrary::GetFinaleGradeText(Tier.Grade);
	Configure(FText::Format(TitleFormat, GradeText), BodyText, {}, ConfirmLabel);
	if (Button_Cancel) Button_Cancel->SetButtonLabel(CancelLabel);
	// 카드와 같은 그림 칸(ScaleBox) — 브러시 크기가 0으로 저장된 WBP도 보이게 공용 경로로 넣는다.
	FFF_FinaleTierPresentation::ApplyPreview(Image_Preview, Presentation.PreviewTexture);
	if (Text_Grade) Text_Grade->SetText(FText::Format(GradeFormat, GradeText));
	if (Text_Bonus) Text_Bonus->SetText(FText::Format(BonusFormat, FText::AsNumber(Tier.SatisfactionBonus)));
	if (Text_Cost) Text_Cost->SetText(FText::AsNumber(Tier.CostGold));
	if (Text_GoldAfter) Text_GoldAfter->SetText(FText::AsNumber(GoldAfter));
	SetPurchaseAvailability(bCanConfirm, BlockReason);
}

void UFF_FinaleConfirmDialog::SetPurchaseAvailability(bool bCanConfirm, const FText& BlockReason)
{
	if (Button_Confirm) Button_Confirm->SetIsEnabled(bCanConfirm);
	if (Text_BlockReason)
	{
		Text_BlockReason->SetText(BlockReason);
		Text_BlockReason->SetVisibility(BlockReason.IsEmpty()
			? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible);
	}
}
