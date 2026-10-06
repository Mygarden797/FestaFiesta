#include "UI/PrepHUD/Panels/FF_PrepFinaleCard.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Economy/FF_EconomyDisplayTextLibrary.h"

UFF_PrepFinaleCard::UFF_PrepFinaleCard()
{
	NormalContentColor = FLinearColor::FromSRGBColor(FColor(0x17, 0x42, 0x37));
	SelectedContentColor = NormalContentColor;
	DisabledContentColor = FLinearColor::FromSRGBColor(FColor(0x65, 0x7F, 0x75));
}

void UFF_PrepFinaleCard::ConfigureTier(
	const FFF_FinaleTier& Tier, const FFF_FinaleTierPresentation& Presentation)
{
	Grade = Tier.Grade;
	const FText GradeText = UFF_EconomyDisplayTextLibrary::GetFinaleGradeText(Grade);
	ConfigureChoice(static_cast<int32>(Grade), FText::Format(TitleFormat, GradeText));
	SetTextOrHide(Text_Grade, GradeText);
	SetTextOrHide(Text_Description, Presentation.Description);
	SetTextOrHide(Text_Cost, FText::AsNumber(Tier.CostGold));
	SetTextOrHide(Text_Bonus, FText::Format(BonusFormat, FText::AsNumber(Tier.SatisfactionBonus)));
	FFF_FinaleTierPresentation::ApplyPreview(Image_Preview, Presentation.PreviewTexture);
	ApplyContentColor();
}

void FFF_FinaleTierPresentation::ApplyPreview(UImage* Image, UTexture2D* Texture)
{
	if (!Image) { return; }
	FSlateBrush PreviewBrush = Image->GetBrush();
	PreviewBrush.SetResourceObject(Texture);
	if (Texture && PreviewBrush.GetImageSize().IsNearlyZero() && Texture->GetSizeX() > 0 && Texture->GetSizeY() > 0)
	{
		PreviewBrush.SetImageSize(FVector2D(Texture->GetSizeX(), Texture->GetSizeY()));
	}
	Image->SetBrush(PreviewBrush);
	Image->SetVisibility(Texture ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
}

void UFF_PrepFinaleCard::CollectStateTexts(TArray<UTextBlock*>& OutTexts) const
{
	OutTexts.Add(Text_Description);
	OutTexts.Add(Text_Cost);
	OutTexts.Add(Text_Bonus);
	// 그림 위의 밝은 등급 배지는 본문 잠금 색과 독립적이다.
}

void UFF_PrepFinaleCard::ApplyFinaleState(EFF_PrepCardState State, const FText& BadgeText)
{
	ApplyCardState(State, BadgeText);
	// 구매 완료 카드에도 선택 테두리를 남겨 확정한 등급을 바로 찾게 한다.
	if (State == EFF_PrepCardState::Committed)
	{
		SetSelected(true);
		ApplyContentColor();
	}
	if (Button_Root)
	{
		// 비활성 버튼은 Normal 대신 Disabled 브러시를 그리므로 확정 테두리도 반영한다.
		FButtonStyle Style = Button_Root->GetStyle();
		Style.Disabled = Style.Normal;
		Button_Root->SetStyle(Style);
	}
	if (Border_Badge)
	{
		Border_Badge->SetVisibility(BadgeText.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}
