#include "UI/LiveHUD/Panels/FF_LiveNeedRowWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/FF_UIDisplayTextLibrary.h"

void UFF_LiveNeedRowWidget::SetRowData(const FFF_LiveNeedRow& InRow)
{
	RowData = InRow;
	bHasRowData = true;
	RefreshRow();
}

void UFF_LiveNeedRowWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (bHasRowData)
	{
		RefreshRow();
	}
}

void UFF_LiveNeedRowWidget::RefreshRow()
{
	if (Text_Name)
	{
		Text_Name->SetText(UFF_UIDisplayTextLibrary::GetNeedTypeText(RowData.View.NeedType));
	}
	if (Image_NeedIcon)
	{
		const TObjectPtr<UTexture2D>* Icon = NeedIcons.Find(RowData.View.NeedType);
		if (Icon && *Icon)
		{
			Image_NeedIcon->SetBrushFromTexture(Icon->Get(), false);
			Image_NeedIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			Image_NeedIcon->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	const bool bKnown = RowData.Severity != EFF_LiveNeedSeverity::Unknown && FMath::IsFinite(RowData.View.UnmetRatioPercent);
	const float Percent = bKnown ? FMath::Clamp(RowData.View.UnmetRatioPercent, 0.f, 100.f) : 0.f;
	const FText* Status = &UnknownText;
	const FLinearColor* Background = &UnknownBackgroundColor;
	const FLinearColor* BarColor = &OkBarColor;
	if (bKnown)
	{
		switch (RowData.Severity)
		{
		case EFF_LiveNeedSeverity::Bad:
			Status = &BadText;
			Background = &BadBackgroundColor;
			BarColor = &BadBarColor;
			break;
		case EFF_LiveNeedSeverity::Watch:
			Status = &WatchText;
			Background = &WatchBackgroundColor;
			BarColor = &WatchBarColor;
			break;
		default:
			Status = &OkText;
			Background = &OkBackgroundColor;
			break;
		}
	}
	if (Text_Percent)
	{
		Text_Percent->SetText(bKnown
			? FText::Format(PercentTextFormat, FFormatNamedArguments{{TEXT("Percent"), FText::AsNumber(FMath::RoundToInt(Percent))}})
			: MissingValueText);
	}
	if (Text_Status)
	{
		Text_Status->SetText(*Status);
	}
	if (Border_Status)
	{
		Border_Status->SetBrushColor(*Background);
	}
	if (Progress_Unmet)
	{
		// 원본의 단위는 0~100%. 표기와 같은 값을 0~1로 바꾼다. 미집계는 0%로 그리지 않는다.
		Progress_Unmet->SetPercent(Percent / 100.f);
		Progress_Unmet->SetFillColorAndOpacity(*BarColor);
		Progress_Unmet->SetVisibility(bKnown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}
	if (Text_Remedy)
	{
		const bool bNeedsRemedy = bKnown && (RowData.Severity == EFF_LiveNeedSeverity::Watch || RowData.Severity == EFF_LiveNeedSeverity::Bad);
		Text_Remedy->SetText(bNeedsRemedy ? UFF_UIDisplayTextLibrary::GetNeedRemedyText(RowData.View.NeedType) : FText::GetEmpty());
		Text_Remedy->SetVisibility(bNeedsRemedy ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
