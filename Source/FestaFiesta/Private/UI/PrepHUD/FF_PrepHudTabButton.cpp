#include "UI/PrepHUD/FF_PrepHudTabButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"

void UFF_PrepHudTabButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (Text_Label && !DisplayLabelOverride.IsEmpty())
	{
		Text_Label->SetText(DisplayLabelOverride);
	}
	if (Image_Icon && DesignerIcon.GetResourceObject())
	{
		Image_Icon->SetBrush(DesignerIcon);
	}
	RefreshVisualState();
}

bool UFF_PrepHudTabButton::HasFinaleAttention() const
{
	return IsDesignTime() ? bPreviewFinaleAttention : bFinaleAttention;
}

bool UFF_PrepHudTabButton::HasRequiredAttention() const
{
	return IsDesignTime() ? bPreviewRequiredAttention : bRequiredAttention;
}

void UFF_PrepHudTabButton::SetRequiredAttention(bool bEnabled, const FText& Hint)
{
	bRequiredAttention = bEnabled;
	SetToolTipText(bEnabled ? Hint : FText::GetEmpty());
	if (TSharedPtr<SWidget> SlateWidget = GetCachedWidget())
	{
		SlateWidget->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void UFF_PrepHudTabButton::SetFinaleAttention(bool bEnabled)
{
	if (bFinaleAttention == bEnabled)
	{
		return;
	}
	bFinaleAttention = bEnabled;
	FinaleSheenElapsed = 0.f;
	RefreshVisualState();
	InvalidateLayoutAndVolatility();
}

void UFF_PrepHudTabButton::RefreshVisualState()
{
	// 먼저 WBP 원본을 복원한다. 구매 후 강조를 끄거나 선택을 바꿔도 금색이 남지 않는다.
	Super::RefreshVisualState();
	if (!HasFinaleAttention() || !Button_Root)
	{
		return;
	}
	FButtonStyle Style = Button_Root->GetStyle();
	const float OutlineWidth = IsSelected() ? 2.f : 1.f;
	Style.SetNormal(FSlateRoundedBoxBrush(FinaleFillColor, 6.f, FinaleOutlineColor, OutlineWidth));
	Style.SetHovered(FSlateRoundedBoxBrush(FMath::Lerp(FinaleFillColor, FLinearColor::White, 0.16f),
		6.f, FinaleOutlineColor, OutlineWidth));
	Style.SetPressed(FSlateRoundedBoxBrush(FinaleFillColor * FLinearColor(0.91f, 0.91f, 0.91f, 1.f),
		6.f, FinaleOutlineColor, 2.f));
	Button_Root->SetStyle(Style);
	if (Text_Label)
	{
		Text_Label->SetColorAndOpacity(FinaleContentColor);
	}
	if (Image_Icon)
	{
		Image_Icon->SetColorAndOpacity(FinaleContentColor);
	}
}

void UFF_PrepHudTabButton::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (HasFinaleAttention() && bAnimateFinaleSheen && !IsDesignTime())
	{
		FinaleSheenElapsed = FMath::Fmod(FinaleSheenElapsed + InDeltaTime, FMath::Max(2.f, FinaleSheenPeriod));
		if (TSharedPtr<SWidget> SlateWidget = GetCachedWidget())
		{
			SlateWidget->Invalidate(EInvalidateWidgetReason::Paint);
		}
	}
}

int32 UFF_PrepHudTabButton::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	int32 AttentionLayer = BaseLayer;
	if (HasRequiredAttention() && Text_Label && FSlateApplication::IsInitialized())
	{
		const FVector2f LocalSize(AllottedGeometry.GetLocalSize());
		const float Diameter = FMath::Clamp(RequiredBadgeDiameter, 16.f, 40.f);
		const FVector2f BadgeSize(Diameter, Diameter);
		// 탭 크기/슬롯을 바꾸지 않고 우상단에 겹친다. 배지 자체는 클릭을 가로채지 않는다.
		const FVector2f Position(LocalSize.X - Diameter + 3.f, -Diameter * 0.35f);
		const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
		const FSlateRoundedBoxBrush Brush(RequiredBadgeColor, Diameter * .5f, FLinearColor::White, 1.5f);
		FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 1,
			AllottedGeometry.ToPaintGeometry(BadgeSize, FSlateLayoutTransform(Position)),
			&Brush, ESlateDrawEffect::None, RequiredBadgeColor * Tint);
		FSlateFontInfo Font = Text_Label->GetFont();
		Font.Size = FMath::RoundToInt(Diameter * .64f);
		const FText Mark = FText::FromString(TEXT("!"));
		const FVector2f TextSize(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Mark, Font));
		FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 2,
			AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(Position + (BadgeSize - TextSize) * .5f)),
			Mark, Font, ESlateDrawEffect::None, FLinearColor::White * Tint);
		AttentionLayer = BaseLayer + 2;
	}
	if (!HasFinaleAttention())
	{
		return AttentionLayer;
	}
	const FVector2f Size(AllottedGeometry.GetLocalSize());
	if (Size.X <= 8.f || Size.Y <= 8.f)
	{
		return BaseLayer;
	}
	const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
	const FGeometry Interior = AllottedGeometry.MakeChild(Size - FVector2f(4.f, 4.f),
		FSlateLayoutTransform(FVector2f(2.f, 2.f)));
	// 위쪽 유리 반사와 느리게 한 번 지나가는 광택만 그린다. 별도 위젯·입력 영역은 만들지 않는다.
	FSlateDrawElement::MakeGradient(OutDrawElements, BaseLayer + 1, Interior.ToPaintGeometry(),
		{ FSlateGradientStop(FVector2f(0.f, 0.f), FLinearColor(1.f, 1.f, 1.f, 0.28f) * Tint),
		  FSlateGradientStop(FVector2f(0.f, Size.Y * 0.6f), FLinearColor(1.f, 1.f, 1.f, 0.f)) },
		Orient_Vertical, ESlateDrawEffect::None, FVector4f(5.f));
	OutDrawElements.PushClip(FSlateClippingZone(Interior));
	const float Phase = bAnimateFinaleSheen && !IsDesignTime()
		? FMath::Clamp(FinaleSheenElapsed / 1.3f, 0.f, 1.f) : 0.6f;
	if (Phase < 1.f)
	{
		const float X = FMath::Lerp(-Size.Y - 30.f, Size.X + Size.Y + 30.f, Phase);
		const TArray<FVector2f> Points { FVector2f(X, 0.f), FVector2f(X - Size.Y * 0.65f, Size.Y) };
		FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 2, AllottedGeometry.ToPaintGeometry(),
			Points, ESlateDrawEffect::None, FLinearColor(1.f, 1.f, 1.f, FinaleSheenOpacity) * Tint, true, 14.f);
	}
	OutDrawElements.PopClip();

	if (Text_Label && !FinaleBadgeText.IsEmpty() && FSlateApplication::IsInitialized())
	{
		FSlateFontInfo Font = Text_Label->GetFont();
		Font.Size = 9;
		const FVector2f TextSize(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(FinaleBadgeText, Font));
		const FVector2f BadgeSize = TextSize + FVector2f(10.f, 2.f);
		const FVector2f BadgePosition(Size.X - BadgeSize.X - 3.f, -BadgeSize.Y * 0.5f);
		static const FSlateRoundedBoxBrush BadgeBrush(FLinearColor::White, 6.f);
		FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 3,
			AllottedGeometry.ToPaintGeometry(BadgeSize, FSlateLayoutTransform(BadgePosition)),
			&BadgeBrush, ESlateDrawEffect::None, FinaleContentColor * Tint);
		FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 4,
			AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(BadgePosition + FVector2f(5.f, 1.f))),
			FinaleBadgeText, Font, ESlateDrawEffect::None, FLinearColor(1.f, 0.95f, 0.78f, 1.f) * Tint);
	}
	return BaseLayer + 4;
}
