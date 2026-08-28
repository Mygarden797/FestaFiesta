// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/FF_StateChip.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/Data/FF_UIMetricBandSettings.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UILog.h"
#include "UI/Widgets/FF_MetricRowWidget.h"

void UFF_StateChip::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 배경은 상태와 무관하게 고정이다. 상태색은 문구와 아이콘에만 들어간다.
	if (!Border_Chip)
	{
		return;
	}

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		UE_LOG(LogFestaFiestaUI, Error, TEXT("UFF_StateChip: 팔레트를 읽을 수 없어 배경을 칠하지 않았다."));
		return;
	}

	Border_Chip->SetBrushColor(Palette->InGame.PaperSunken);
}

void UFF_StateChip::SetMetricState(int32 Value)
{
	if (!Text_Label)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_StateChip::SetMetricState: Text_Label 바인딩이 없다. WBP 위젯 이름을 확인한다."));
		return;
	}

	// 구간을 못 찾으면 임의의 색으로 칠하지 않는다. 틀린 색이 빈 칸보다 나쁘다.
	const UFF_UIMetricBandSettings* Settings = UFF_UIMetricBandSettings::Get();
	const FFF_UIMetricBand* Band = Settings ? Settings->FindBand(Value) : nullptr;
	if (!Band)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_StateChip::SetMetricState: %d 에 해당하는 구간이 없어 칩을 숨겼다. "
				 "Project Settings > Game > FestaFiesta UI Metric Bands 를 확인한다."), Value);
		SetChipVisible(false);
		return;
	}

	// 문구와 아이콘 브러시·틴트는 ApplyBand 가 전부 한다. 같은 판정을 여기서 다시 쓰지 않는다.
	// 위에서 구간을 한 번 더 조회하는 것은 **문구 색**에 쓸 값이 필요해서다 —
	// ApplyBand 는 라벨의 글자색을 건드리지 않는다.
	UFF_MetricRowWidget::ApplyBand(Value, Text_Label, Image_Icon);

	Text_Label->SetColorAndOpacity(FSlateColor(Band->Color));

	if (Image_Icon)
	{
		// 아이콘 에셋이 아직 없으면 ApplyBand 가 브러시를 비운다. 그때는 칸을 숨긴다.
		// ApplyBand 가 이미 동기 로드했으므로 Get() 으로 결과를 볼 수 있다.
		const bool bHasIcon = Band->Icon.Get() != nullptr;
		Image_Icon->SetVisibility(bHasIcon ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	SetChipVisible(true);
}

void UFF_StateChip::SetCustomState(const FText& InLabel, const FLinearColor& InColor, UTexture2D* InIcon)
{
	if (!Text_Label)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_StateChip::SetCustomState: Text_Label 바인딩이 없다. WBP 위젯 이름을 확인한다."));
		return;
	}

	if (InLabel.IsEmpty())
	{
		// 문구 없이 색만 넣으면 색맹 사용자에게 상태가 전달되지 않는다 (AT-011).
		// 시그니처로 막을 수 없는 부분이라 여기서 소리를 낸다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_StateChip::SetCustomState: 문구가 비어 있다. 색만으로 상태를 구분할 수 없다 (AT-011)."));
	}

	Text_Label->SetText(InLabel);
	Text_Label->SetColorAndOpacity(FSlateColor(InColor));

	if (Image_Icon)
	{
		if (InIcon)
		{
			// bMatchSize 를 켜지 않는다. 켜면 브러시 ImageSize 가 텍스처 크기로 덮여
			// 24x24 로 박아 둔 값이 사라지고 칩이 부풀어 오른다.
			Image_Icon->SetBrushFromTexture(InIcon, false);
			Image_Icon->SetColorAndOpacity(InColor);
			Image_Icon->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			// 아이콘이 없으면 칸을 숨긴다. 빈 사각형을 남기지 않는다.
			Image_Icon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	SetChipVisible(true);
}

void UFF_StateChip::SetChipVisible(bool bVisible)
{
	// Hidden 이 아니라 Collapsed 다. Hidden 은 자리를 남겨 빈 칸이 생긴다.
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
