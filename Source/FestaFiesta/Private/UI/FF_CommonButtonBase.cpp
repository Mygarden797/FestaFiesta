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

void UFF_CommonButtonBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 디자이너에서도 문구와 상태가 보이도록 미리 반영한다.
	SetButtonLabel(ButtonLabel);
	RefreshVisualState();
}

void UFF_CommonButtonBase::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind + Sync의 Sync에 해당한다.
	// 이벤트는 앞으로의 변화만 알려주므로, 생성 시점의 현재 상태를 여기서 한 번 반영한다.
	SetButtonLabel(ButtonLabel);
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
	// 여기서는 스타일이 손대지 않는 자식 위젯(아이콘·Chevron)과 전체 불투명도만 맞춘다.
	//
	// 부분 갱신을 하지 않고 매번 전체를 다시 선언한다.
	// 이벤트 순서가 어떻든 결과가 같아지므로 상태 불일치가 생기지 않는다.

	const bool bInteractable = GetIsEnabled();
	const bool bHighlighted = bInteractable && (IsHovered() || bHasFocusPath);

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

	// 비활성은 색뿐 아니라 불투명도로도 표시한다.
	SetRenderOpacity(bInteractable ? 1.0f : DisabledOpacity);

	BP_OnVisualStateRefreshed(bHighlighted, bInteractable);
}
