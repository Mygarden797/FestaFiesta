// FF_FacilityProgramMarkerWidget.cpp
// 역할: 시설 위에 프로그램과 수입 상태를 요약해서 표시한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityProgramMarkerWidget.h"
#include "Building/Presentation/FF_BuildTypography.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Font.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UI/Data/FF_UIPalette.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "FFFacilityProgramMarker"

UFF_FacilityProgramMarkerWidget::UFF_FacilityProgramMarkerWidget(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(false);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> UFF_FacilityProgramMarkerWidget::RebuildWidget()
{
	bAuthoredLayout = WidgetTree && WidgetTree->RootWidget;
	if (bAuthoredLayout) return Super::RebuildWidget();
	LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
	FontAsset = LoadObject<UFont>(nullptr,
		TEXT("/Game/Asset/Font/NEXON_Lv2_Gothic_OTF_Bold_Font.NEXON_Lv2_Gothic_OTF_Bold_Font"));
	if (FontAsset)
	{
		LabelFont = FSlateFontInfo(FontAsset, 11, FFBuildTypography::Typeface(*CastChecked<UFont>(FontAsset.Get())));
	}
	return SAssignNew(NativeBounds, SBox).WidthOverride(bShowRevenue ? 220.f : 156.f)
		.HeightOverride(bShowRevenue && MarkerState != EFF_FacilityProgramMarkerState::Hidden ? 70.f : 34.f)
		.Visibility(EVisibility::HitTestInvisible);
}

void UFF_FacilityProgramMarkerWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (!IsDesignTime()) RefreshPresentation();
}

void UFF_FacilityProgramMarkerWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	NativeBounds.Reset();
}

void UFF_FacilityProgramMarkerWidget::SetMarkerState(const EFF_FacilityProgramMarkerState InState)
{
	MarkerState = InState;
	Label = MarkerState == EFF_FacilityProgramMarkerState::Scheduled
		? LOCTEXT("Scheduled", "프로그램 편성됨")
		: LOCTEXT("Available", "프로그램 가능");
	RefreshPresentation();
}

void UFF_FacilityProgramMarkerWidget::SetRevenue(bool bInShowRevenue, bool bInEconomyAvailable, int32 InTodayGold)
{
	bShowRevenue = bInShowRevenue;
	bEconomyAvailable = bInEconomyAvailable;
	TodayGold = FMath::Max(0, InTodayGold);
	RefreshPresentation();
}

FText UFF_FacilityProgramMarkerWidget::GetRevenueText() const
{
	return bEconomyAvailable ? FText::Format(LOCTEXT("RevenueGold", "{0} Gold"), FText::AsNumber(TodayGold))
		: LOCTEXT("RevenueUnavailable", "—");
}

void UFF_FacilityProgramMarkerWidget::RefreshPresentation()
{
	const bool bShowProgram = MarkerState != EFF_FacilityProgramMarkerState::Hidden;
	if (Panel_Program) Panel_Program->SetVisibility(bShowProgram ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Switcher_Program) Switcher_Program->SetActiveWidgetIndex(MarkerState == EFF_FacilityProgramMarkerState::Scheduled ? 1 : 0);
	if (Panel_Revenue) Panel_Revenue->SetVisibility(bShowRevenue ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Text_RevenueValue) Text_RevenueValue->SetText(GetRevenueText());
	if (NativeBounds)
	{
		NativeBounds->SetWidthOverride(bShowRevenue ? 220.f : 156.f);
		NativeBounds->SetHeightOverride(bShowRevenue && bShowProgram ? 70.f : 34.f);
	}
	SetVisibility(!bShowProgram && !bShowRevenue
		? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	InvalidateLayoutAndVolatility();
}

int32 UFF_FacilityProgramMarkerWidget::NativePaint(const FPaintArgs& Args,
	const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, const int32 LayerId,
	const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (bAuthoredLayout || (MarkerState == EFF_FacilityProgramMarkerState::Hidden && !bShowRevenue))
	{
		return BaseLayer;
	}

	const FFF_UIPaletteInGame Palette;
	const bool bShowProgram = MarkerState != EFF_FacilityProgramMarkerState::Hidden;
	if (bShowRevenue)
	{
		const float Top = bShowProgram ? 36.f : 0.f;
		static const FSlateRoundedBoxBrush RevenueBrush(FLinearColor::White, 7.f);
		FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 1,
			AllottedGeometry.ToPaintGeometry(FVector2f(AllottedGeometry.GetLocalSize().X, 34.f), FSlateLayoutTransform(FVector2f(0, Top))),
			&RevenueBrush, ESlateDrawEffect::None, Palette.Paper);
		FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 2,
			AllottedGeometry.ToPaintGeometry(FVector2f(64.f, 22.f), FSlateLayoutTransform(FVector2f(10.f, Top + 8.f))),
			LOCTEXT("TodayRevenue", "오늘 매출"), LabelFont, ESlateDrawEffect::None, Palette.InkMuted);
		FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 2,
			AllottedGeometry.ToPaintGeometry(FVector2f(142.f, 22.f), FSlateLayoutTransform(FVector2f(76.f, Top + 8.f))),
			GetRevenueText(), LabelFont, ESlateDrawEffect::None, Palette.Ink);
	}
	if (!bShowProgram) return BaseLayer + 2;
	const FLinearColor Color = MarkerState == EFF_FacilityProgramMarkerState::Scheduled
		? Palette.Info : Palette.Neutral;
	static const FSlateRoundedBoxBrush BadgeBrush(FLinearColor::White, 17.f);
	FSlateDrawElement::MakeBox(OutDrawElements, BaseLayer + 1,
		AllottedGeometry.ToPaintGeometry(FVector2f(AllottedGeometry.GetLocalSize().X, 34.f), FSlateLayoutTransform()),
		&BadgeBrush, ESlateDrawEffect::None, Color);

	// Draw a calendar directly so the icon survives missing emoji fonts and cooked assets.
	auto DrawLine = [&](TArray<FVector2f> Points)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 2,
			AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
			FLinearColor::White, true, 1.7f);
	};
	DrawLine({{11.f, 11.f}, {28.f, 11.f}, {28.f, 25.f}, {11.f, 25.f}, {11.f, 11.f}});
	DrawLine({{11.f, 15.f}, {28.f, 15.f}});
	DrawLine({{15.f, 8.f}, {15.f, 13.f}});
	DrawLine({{24.f, 8.f}, {24.f, 13.f}});
	if (MarkerState == EFF_FacilityProgramMarkerState::Scheduled)
	{
		DrawLine({{15.f, 20.f}, {18.f, 23.f}, {24.f, 18.f}});
	}
	else
	{
		DrawLine({{16.f, 20.f}, {23.f, 20.f}});
		DrawLine({{19.5f, 17.f}, {19.5f, 23.f}});
	}
	FSlateDrawElement::MakeText(OutDrawElements, BaseLayer + 2,
		AllottedGeometry.ToPaintGeometry(FVector2f(112.f, 22.f),
			FSlateLayoutTransform(FVector2f(35.f, 8.f))),
		Label, LabelFont, ESlateDrawEffect::None, FLinearColor::White);
	return BaseLayer + 2;
}

#undef LOCTEXT_NAMESPACE
