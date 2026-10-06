#include "UI/Widgets/FF_PanelScrollStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ScrollBox.h"

FSlateBrush FFF_PanelScrollStyle::MakeBarBrush(const FLinearColor& Color) const
{
	const float Visible = FMath::Max(2.f, Thickness);
	const float Grab = FMath::Max(Visible, GrabWidth);
	// 잡는 폭과 보이는 폭의 차이만큼 투명한 테두리를 둘러 안쪽만 칠한다.
	// 엔진의 둥근 상자는 테두리 안쪽 반지름을 `반지름 - 테두리 두께` 로 그리므로 둘을 더해 준다.
	const float Inset = (Grab - Visible) * .5f;
	const float Radius = FMath::Clamp(CornerRadius, 0.f, Visible * .5f) + Inset;

	FSlateBrush Result;
	Result.DrawAs = ESlateBrushDrawType::RoundedBox;
	Result.TintColor = FSlateColor(Color);
	Result.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Result.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
	Result.OutlineSettings.Width = Inset;
	Result.OutlineSettings.Color = FSlateColor(Color.CopyWithNewOpacity(0.f));
	Result.OutlineSettings.bUseBrushTransparency = false;
	return Result;
}

void FFF_PanelScrollStyle::ApplyTo(const UWidgetTree* Tree) const
{
	if (!bApply || !Tree) { return; }
	const FSlateBrush Track = MakeBarBrush(TrackColor);
	const FSlateBrush Normal = MakeBarBrush(ThumbColor);
	const FSlateBrush Hovered = MakeBarBrush(ThumbHoveredColor);
	const FSlateBrush Dragged = MakeBarBrush(ThumbDraggedColor);
	const FSlateNoResource NoCap;
	const float Grab = FMath::Max(FMath::Max(2.f, Thickness), GrabWidth);
	Tree->ForEachWidget([&](UWidget* Widget)
	{
		UScrollBox* Scroll = Cast<UScrollBox>(Widget);
		if (!Scroll) { return; }
		FScrollBarStyle Style = Scroll->GetWidgetBarStyle();
		Style.SetVerticalBackgroundImage(Track);
		Style.SetHorizontalBackgroundImage(Track);
		// 위·아래 끝 조각은 막대 앞뒤 트랙을 따로 그려 막대 끝에 홈이 생긴다. 트랙은 바탕 한 장으로 그린다.
		Style.SetVerticalTopSlotImage(NoCap);
		Style.SetVerticalBottomSlotImage(NoCap);
		Style.SetHorizontalTopSlotImage(NoCap);
		Style.SetHorizontalBottomSlotImage(NoCap);
		Style.SetNormalThumbImage(Normal);
		Style.SetHoveredThumbImage(Hovered);
		Style.SetDraggedThumbImage(Dragged);
		Style.SetThickness(Grab);
		Scroll->SetWidgetBarStyle(Style);
		Scroll->SetScrollbarThickness(FVector2D(Grab, Grab));
		Scroll->SetScrollbarPadding(Padding);
	});
}
