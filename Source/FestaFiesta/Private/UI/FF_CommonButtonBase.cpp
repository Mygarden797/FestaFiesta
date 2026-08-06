// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FF_CommonButtonBase.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "UI/FF_UILog.h"

void UFF_CommonButtonBase::SetButtonLabel(const FText& InLabel)
{
	ButtonLabel = InLabel;

	if (Text_Label)
	{
		Text_Label->SetText(ButtonLabel);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Text_Label 바인딩이 없어 라벨을 적용하지 못했다."), *GetName());
	}
}

void UFF_CommonButtonBase::ApplyIconBrush()
{
	if (!Image_Icon)
	{
		return;
	}

	// 비어 있으면 WBP에 설정된 브러시를 그대로 둔다.
	// 무조건 덮어쓰면 아이콘을 지정하지 않은 버튼의 아이콘이 사라진다.
	if (IconBrush.GetResourceObject() == nullptr)
	{
		return;
	}

	Image_Icon->SetBrush(IconBrush);
}

void UFF_CommonButtonBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너에서도 문구·아이콘·상태가 보이도록 미리 반영한다.
	SetButtonLabel(ButtonLabel);
	ApplyIconBrush();
	RefreshVisualState();
}

void UFF_CommonButtonBase::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind + Sync의 Sync에 해당한다.
	// 이벤트는 앞으로의 변화만 알려주므로, 생성 시점의 현재 상태를 여기서 한 번 반영한다.
	SetButtonLabel(ButtonLabel);
	ApplyIconBrush();
	RefreshVisualState();
}

void UFF_CommonButtonBase::NativeOnClicked()
{
	Super::NativeOnClicked();
	OnFFButtonClicked.Broadcast(this);
}

void UFF_CommonButtonBase::NativeOnHovered()
{
	Super::NativeOnHovered();
	RefreshVisualState();
}

void UFF_CommonButtonBase::NativeOnUnhovered()
{
	Super::NativeOnUnhovered();
	RefreshVisualState();
}

void UFF_CommonButtonBase::NativeOnEnabled()
{
	Super::NativeOnEnabled();
	RefreshVisualState();
}

void UFF_CommonButtonBase::NativeOnDisabled()
{
	Super::NativeOnDisabled();
	RefreshVisualState();
}

void UFF_CommonButtonBase::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);

	bHasFocusPath = true;
	RefreshVisualState();
}

void UFF_CommonButtonBase::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);

	bHasFocusPath = false;
	RefreshVisualState();
}

void UFF_CommonButtonBase::RefreshVisualState()
{
	// 배경 브러시는 여기서 다루지 않는다. CBS_ 스타일 에셋이 상태별로 알아서 그린다.
	// 여기서는 스타일이 손대지 않는 자식 위젯(아이콘·Chevron·라벨)과 전체 불투명도만 맞춘다.
	//
	// 부분 갱신을 하지 않고 매번 전체를 다시 선언한다.
	// 이벤트 순서가 어떻든 결과가 같아지므로 상태 불일치가 생기지 않는다.

	const bool bInteractable = GetIsEnabled();

	// !! 호버만 본다. 포커스는 넣지 않는다 !!
	//
	// CBS_ 스타일의 배경 브러시는 호버에서만 밝은 것으로 바뀐다.
	// 여기에 포커스를 더하면 "배경은 어두운데 글자만 남색"이 되어 글자가 사라진다.
	// 실제로 버튼을 클릭한 뒤(호버는 풀리고 포커스는 남은 상태) 라벨이 안 보이는 버그가 있었다.
	//
	// 색을 정하는 기준은 배경을 정하는 기준과 반드시 같아야 한다.
	// 포커스 표시는 색이 아니라 Image_FocusRing으로 한다.
	const bool bHighlighted = bInteractable && IsHovered();

	const FLinearColor ContentColor =
		!bInteractable ? DisabledContentColor
		: bHighlighted ? HighlightedContentColor
		: NormalContentColor;

	if (Text_Label)
	{
		if (bTintLabel)
		{
			Text_Label->SetColorAndOpacity(FSlateColor(ContentColor));
		}
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Text_Label 바인딩이 없다. WBP에 같은 이름의 CommonTextBlock이 있는지 확인할 것."),
			*GetName());
	}

	if (Image_Icon)
	{
		Image_Icon->SetColorAndOpacity(ContentColor);
	}

	if (Image_Chevron)
	{
		Image_Chevron->SetColorAndOpacity(ContentColor);

		const bool bChevronVisible = !bShowChevronOnlyWhenHighlighted || bHighlighted;
		Image_Chevron->SetVisibility(bChevronVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// 포커스는 색이 아니라 형태로 표시한다.
	// 색상만으로 상태를 구분하면 안 되고(AT-011), 배경이 어두운 상태에서도 보여야 한다.
	if (Image_FocusRing)
	{
		Image_FocusRing->SetVisibility(
			(bInteractable && bHasFocusPath) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// 비활성은 색뿐 아니라 불투명도로도 표시한다.
	SetRenderOpacity(bInteractable ? 1.0f : DisabledOpacity);

	BP_OnVisualStateRefreshed(bHighlighted, bInteractable);
}
