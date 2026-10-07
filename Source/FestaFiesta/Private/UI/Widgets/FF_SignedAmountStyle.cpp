#include "UI/Widgets/FF_SignedAmountStyle.h"

#include "Components/TextBlock.h"

FText FFF_SignedAmountStyle::Format(const int64 Amount, const FText& PositiveFormat)
{
	if (Amount <= 0) { return FText::AsNumber(Amount); }
	FFormatNamedArguments Args;
	Args.Add(TEXT("Amount"), FText::AsNumber(Amount));
	return FText::Format(PositiveFormat, Args);
}

void FFF_SignedAmountStyle::ApplyColor(UTextBlock* Text, const TOptional<int64> Amount) const
{
	if (!Text) { return; }
	const int64 Value = Amount.Get(0);
	Text->SetColorAndOpacity(FSlateColor(Value > 0 ? PositiveColor : Value < 0 ? NegativeColor : NeutralColor));
}
