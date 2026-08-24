// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/FF_FocusableWidgetBase.h"

#include "Components/Image.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UILog.h"

UFF_FocusableWidgetBase::UFF_FocusableWidgetBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 포커스를 받지 못하면 링을 그릴 일도 없다.
	// 이 값은 **생성 시점에만** 적용된다 (엔진 주석: not modifiable at runtime).
	SetIsFocusable(true);
}

void UFF_FocusableWidgetBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!Image_FocusRing)
	{
		// 링이 없으면 게임패드로 위치를 알 수 없다. 조용히 넘어가지 않는다 (C-024).
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("%s: Image_FocusRing 바인딩이 없어 포커스 표시를 그릴 수 없다. "
				 "전역 RenderFocusRule=Never 라 엔진이 대신 그려주지 않는다."),
			*GetClass()->GetName());
		return;
	}

	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (Palette)
	{
		// 링 색은 팔레트가 소유한다. 디자이너에서 손으로 덮지 않는다.
		Image_FocusRing->SetColorAndOpacity(
			bUseFrontendPalette ? Palette->Frontend.EdgeStrong : Palette->InGame.TealDeep);
	}

	// 디자이너에서는 보이게 두면 배치가 편하지만, 실행 시작은 항상 꺼진 상태여야 한다.
	RefreshFocusVisual();
}

void UFF_FocusableWidgetBase::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnAddedToFocusPath(InFocusEvent);

	bHasUIFocus = true;
	RefreshFocusVisual();
}

void UFF_FocusableWidgetBase::NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent)
{
	Super::NativeOnRemovedFromFocusPath(InFocusEvent);

	bHasUIFocus = false;
	RefreshFocusVisual();
}

void UFF_FocusableWidgetBase::RefreshFocusVisual()
{
	if (Image_FocusRing)
	{
		// Collapsed 가 아니라 Hidden 이다. 링은 겹쳐 그리는 장식이라 자리를 차지하지 않고,
		// Collapsed 로 껐다 켜면 레이아웃이 흔들린다.
		Image_FocusRing->SetVisibility(
			bHasUIFocus ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	BP_OnFocusVisualChanged(bHasUIFocus);
}
