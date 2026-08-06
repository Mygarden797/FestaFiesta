// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FF_UIMetricBandSettings.h"

#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

UFF_UIMetricBandSettings::UFF_UIMetricBandSettings()
{
	// 2026-08-06 결정: 70 / 40. 근거는 헤더 주석.
	// 색은 자리표시자다. Glass Orbit 팔레트가 정해지면 Project Settings에서 바꾼다.
	//
	// 아이콘은 흰색 단색이고 위에서 Color로 틴트한다. 여기서 지정하는 것은 **기본값**이며
	// Config 프로퍼티라 DefaultGame.ini에 값이 있으면 그쪽이 이긴다.
	// 에셋이 없거나 경로가 틀려도 라벨과 색은 나온다 — ApplyBand가 null을 견딘다.
	const TCHAR* IconRoot = TEXT("/Game/FestaFiesta/UI/Icons/");

	FFF_UIMetricBand Good;
	Good.MinValue = 70;
	Good.Label = LOCTEXT("MetricBandGood", "좋음");
	Good.Color = FLinearColor(0.24f, 0.72f, 0.44f);
	Good.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("%sT_UI_Band_Good.T_UI_Band_Good"), IconRoot)));

	FFF_UIMetricBand Caution;
	Caution.MinValue = 40;
	Caution.Label = LOCTEXT("MetricBandCaution", "주의");
	Caution.Color = FLinearColor(0.90f, 0.68f, 0.20f);
	Caution.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("%sT_UI_Band_Caution.T_UI_Band_Caution"), IconRoot)));

	FFF_UIMetricBand Bad;
	Bad.MinValue = 0;
	Bad.Label = LOCTEXT("MetricBandBad", "나쁨");
	Bad.Color = FLinearColor(0.85f, 0.31f, 0.29f);
	Bad.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(FString::Printf(TEXT("%sT_UI_Band_Bad.T_UI_Band_Bad"), IconRoot)));

	Bands = { Good, Caution, Bad };
}

const UFF_UIMetricBandSettings* UFF_UIMetricBandSettings::Get()
{
	return GetDefault<UFF_UIMetricBandSettings>();
}

const FFF_UIMetricBand* UFF_UIMetricBandSettings::FindBand(int32 Value) const
{
	// 내림차순이므로 처음 걸리는 것이 답이다.
	for (const FFF_UIMetricBand& Band : Bands)
	{
		if (Value >= Band.MinValue)
		{
			return &Band;
		}
	}

	// 구간 배열이 비었거나 최하단이 0보다 크면 여기로 온다.
	// 임의의 색으로 대체하지 않는다. 호출부가 "구간 없음"으로 처리한다.
	return nullptr;
}

void UFF_UIMetricBandSettings::SortBandsDescending()
{
	Bands.Sort([](const FFF_UIMetricBand& A, const FFF_UIMetricBand& B)
	{
		return A.MinValue > B.MinValue;
	});
}

#if WITH_EDITOR
void UFF_UIMetricBandSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Project Settings에서 MinValue를 아무 순서로 넣어도 동작하게 한다.
	// 정렬을 사람 손에 맡기면 조용히 틀린 구간이 나온다.
	SortBandsDescending();
}
#endif

#undef LOCTEXT_NAMESPACE
