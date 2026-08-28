// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/FF_StatCard.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

void UFF_StatCard::NativePreConstruct()
{
	Super::NativePreConstruct();

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		UE_LOG(LogFestaFiestaUI, Error, TEXT("UFF_StatCard: 팔레트를 읽을 수 없어 색을 적용하지 않았다."));
		return;
	}

	if (Border_Card)
	{
		Border_Card->SetBrushColor(Palette->InGame.Paper);
	}

	if (Text_Label)
	{
		Text_Label->SetColorAndOpacity(FSlateColor(Palette->InGame.InkMuted));
	}

	if (Text_Caption)
	{
		Text_Caption->SetColorAndOpacity(FSlateColor(Palette->InGame.InkMuted));
	}

	ApplyValueColor(true);

	if (Text_Provisional)
	{
		// 문구를 여기서 넣는다. 디자이너에서 빈 칸으로 보이면 무슨 배지인지 알 수 없다.
		Text_Provisional->SetText(LOCTEXT("StatCardProvisional", "잠정"));
		Text_Provisional->SetColorAndOpacity(FSlateColor(Palette->InGame.Amber));

		// 기본은 꺼짐이다. 근거가 없는 값에만 켠다.
		Text_Provisional->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFF_StatCard::ApplyValueColor(bool bAvailable)
{
	if (!Text_Value)
	{
		return;
	}

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		return;
	}

	Text_Value->SetColorAndOpacity(FSlateColor(
		bAvailable ? Palette->InGame.Ink : Palette->InGame.InkFaint));
}

void UFF_StatCard::SetStat(const FText& InLabel, const FText& InValue)
{
	if (!Text_Label || !Text_Value)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_StatCard::SetStat: Text_Label 또는 Text_Value 바인딩이 없다. WBP 위젯 이름을 확인한다."));
		return;
	}

	Text_Label->SetText(InLabel);
	SetValue(InValue);
}

void UFF_StatCard::SetValue(const FText& InValue)
{
	if (!Text_Value)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_StatCard::SetValue: Text_Value 바인딩이 없다. WBP 위젯 이름을 확인한다."));
		return;
	}

	Text_Value->SetText(InValue);

	// 값이 다시 들어왔으므로 `데이터 없음` 표시를 되돌린다.
	// 되돌리지 않으면 값이 채워졌는데도 흐린 글자로 남는다.
	ApplyValueColor(true);
}

void UFF_StatCard::SetCaption(const FText& InCaption)
{
	if (!Text_Caption)
	{
		// 보조문구가 없는 카드도 정상이다.
		return;
	}

	// 빈 문구면 칸을 숨긴다. `-` 를 넣지 않는다 (docs/ui/ui-scope.md 3절).
	if (InCaption.IsEmpty())
	{
		Text_Caption->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	Text_Caption->SetText(InCaption);
	Text_Caption->SetVisibility(ESlateVisibility::Visible);
}

void UFF_StatCard::SetUnavailable(const FText& InReason)
{
	if (!Text_Value)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_StatCard::SetUnavailable: Text_Value 바인딩이 없다."));
		return;
	}

	if (InReason.IsEmpty())
	{
		// 이유 없이 값을 비우면 화면에 빈 칸만 남아 왜 없는지 알 수 없다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_StatCard::SetUnavailable: 이유가 비어 있다. `데이터 없음` 같은 문구를 넘긴다."));
	}

	Text_Value->SetText(InReason);
	ApplyValueColor(false);
}

void UFF_StatCard::SetProvisional(bool bProvisional)
{
	if (!Text_Provisional)
	{
		if (bProvisional)
		{
			// 켜려고 했는데 자리가 없으면 화면에 근거 없음이 안 드러난다. 조용히 넘기지 않는다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("UFF_StatCard::SetProvisional: Text_Provisional 바인딩이 없어 `잠정` 배지를 띄우지 못했다."));
		}
		return;
	}

	Text_Provisional->SetVisibility(
		bProvisional ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

#undef LOCTEXT_NAMESPACE
