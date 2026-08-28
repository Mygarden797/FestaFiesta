// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/FF_HudTabButtonBase.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/FF_UILog.h"

void UFF_HudTabButtonBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Root)
	{
		Button_Root->OnClicked.AddUniqueDynamic(this, &UFF_HudTabButtonBase::HandleClicked);
	}
	else
	{
		// 이게 없으면 탭이 눌리지 않는데 화면은 정상으로 보인다. 조용히 넘기면 원인을 못 찾는다.
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] Button_Root 가 없어 탭을 누를 수 없다."), *GetName());
	}

	RefreshVisualState();
}

void UFF_HudTabButtonBase::ConfigureTab(int32 InTabId, const FText& InLabel, const FSlateBrush& InIconBrush)
{
	TabId = InTabId;

	if (Text_Label)
	{
		Text_Label->SetText(InLabel);
	}

	// 리소스가 없는 브러시를 넣으면 아이콘이 빈 사각형이 된다. 그때는 WBP 기본값을 유지한다.
	if (Image_Icon && InIconBrush.GetResourceObject())
	{
		Image_Icon->SetBrush(InIconBrush);
	}

	RefreshVisualState();
}

void UFF_HudTabButtonBase::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	RefreshVisualState();
}

void UFF_HudTabButtonBase::SetContentColors(const FLinearColor& InNormal, const FLinearColor& InSelected)
{
	NormalContentColor = InNormal;
	SelectedContentColor = InSelected;
	RefreshVisualState();
}

void UFF_HudTabButtonBase::RefreshVisualState()
{
	const FLinearColor ContentColor = bSelected ? SelectedContentColor : NormalContentColor;

	if (Text_Label)
	{
		Text_Label->SetColorAndOpacity(FSlateColor(ContentColor));
	}

	if (Image_Icon)
	{
		// 흰색 단색 아이콘을 틴트한다. 상태별로 텍스처를 따로 두지 않는다.
		Image_Icon->SetColorAndOpacity(ContentColor);

		// 🔴 텍스처가 없으면 접는다.
		// `UImage` 는 브러시에 리소스가 없어도 **흰 사각형을 그린다.** 틴트가 걸려 있으면
		// 라벨 앞에 검은 네모가 붙는다 — 아이콘을 아직 안 만든 상태에서 실제로 그렇게 나왔다
		// (2026-08-26). 빈 브러시를 "그리지 않음"으로 알아서 처리해 주지 않는다.
		const bool bHasIconTexture = Image_Icon->GetBrush().GetResourceObject() != nullptr;
		Image_Icon->SetVisibility(bHasIconTexture
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (!Button_Root)
	{
		return;
	}

	// 선택 상태에서 버튼 스타일을 덮어쓰기 전에 WBP 원본을 한 번만 보관한다.
	// 선택 해제 뒤 Normal·Hovered·Pressed 를 모두 원래 상태로 되돌리는 데 필요하다.
	if (!bCapturedDefaultBrushes)
	{
		const FButtonStyle& InitialStyle = Button_Root->GetStyle();
		DefaultNormalBrush = InitialStyle.Normal;
		DefaultHoveredBrush = InitialStyle.Hovered;
		DefaultPressedBrush = InitialStyle.Pressed;
		bCapturedDefaultBrushes = true;
	}

	// 배경은 버튼 스타일이 그린다. WBP 의 Pressed 슬롯을 선택 브러시로 사용한다.
	// 선택된 버튼은 Hovered 상태에서도 선택 브러시를 유지해야 한다. Normal 만 바꾸면
	// 마우스를 올리는 순간 WBP 의 Hovered 브러시로 돌아가 선택이 풀린 것처럼 보인다.
	// 색만 바꾸지 않고 배경 형태가 함께 바뀌어야 한다 (AT-011).
	FButtonStyle Style = Button_Root->GetStyle();
	if (bSelected)
	{
		Style.SetNormal(DefaultPressedBrush);
		Style.SetHovered(DefaultPressedBrush);
		Style.SetPressed(DefaultPressedBrush);
	}
	else
	{
		Style.SetNormal(DefaultNormalBrush);
		Style.SetHovered(DefaultHoveredBrush);
		Style.SetPressed(DefaultPressedBrush);
	}
	Button_Root->SetStyle(Style);
}

void UFF_HudTabButtonBase::HandleClicked()
{
	OnHudTabButtonClicked.ExecuteIfBound(TabId);
}
