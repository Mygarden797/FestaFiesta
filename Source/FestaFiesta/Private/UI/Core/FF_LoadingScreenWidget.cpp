// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Core/FF_LoadingScreenWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/Data/FF_UIPalette.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

UFF_LoadingScreenWidget::UFF_LoadingScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LoadingMessage(LOCTEXT("FestivalLoadingMessage", "축제를 준비하고 있습니다"))
	, TipMessage(LOCTEXT("FestivalLoadingTip", "운영 중에도 시간은 계속 흐릅니다. 상황을 확인하며 계획을 조정하세요."))
{
}

void UFF_LoadingScreenWidget::SetLoadingContext(const FText& InFestivalName)
{
	FestivalName = InFestivalName;
	RefreshView();
}

void UFF_LoadingScreenWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshView();
}

void UFF_LoadingScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshView();
}

void UFF_LoadingScreenWidget::RefreshView()
{
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();

	if (Border_Background && Palette)
	{
		Border_Background->SetBrushColor(Palette->Frontend.Void);
	}

	if (Text_FestivalName)
	{
		const FText DisplayFestivalName = FestivalName.IsEmpty() && IsDesignTime()
			? LOCTEXT("LoadingFestivalDesignPreview", "축제 이름")
			: FestivalName;
		Text_FestivalName->SetText(DisplayFestivalName);
		Text_FestivalName->SetVisibility(
			DisplayFestivalName.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (Palette)
		{
			Text_FestivalName->SetColorAndOpacity(FSlateColor(Palette->Frontend.Title));
		}
	}

	if (Text_Loading)
	{
		Text_Loading->SetText(LoadingMessage);
		if (Palette)
		{
			Text_Loading->SetColorAndOpacity(FSlateColor(Palette->Frontend.Text));
		}
	}

	if (Text_Tip)
	{
		Text_Tip->SetText(TipMessage);
		Text_Tip->SetVisibility(
			TipMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		if (Palette)
		{
			Text_Tip->SetColorAndOpacity(FSlateColor(Palette->Frontend.Sub));
		}
	}
}

#undef LOCTEXT_NAMESPACE
