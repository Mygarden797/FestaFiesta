// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Atlas/FF_AtlasPinButton.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UFF_AtlasPinButton::SetPinData(
	FName InPinId,
	const FText& Title,
	const FText& Badge,
	bool bInSelectable,
	const FText& InLockReason)
{
	PinId = InPinId;
	bSelectable = bInSelectable;
	bSelected = false;
	LockReason = InLockReason;

	// 표시를 부분 갱신하지 않고 전부 다시 선언한다. 어떤 순서로 들어와도 결과가 같다.
	// 이름은 베이스의 `Text_Label` 이 그린다.
	SetButtonLabel(Title);

	if (Text_PinBadge)
	{
		// 값이 없으면 칸을 숨긴다. `0` 이나 `-` 를 넣지 않는다 (ui-scope 제약 5).
		Text_PinBadge->SetText(Badge);
		Text_PinBadge->SetVisibility(
			Badge.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	RefreshPinVisual();

	// 🔴 `SetIsEnabled(false)` 를 쓰지 않는다. 잠긴 핀에도 **포커스는 간다** —
	//    비활성 버튼은 게임패드 포커스에서 빠져 그 나라가 없는 것처럼 된다.
	//    선택을 막는 것은 화면이 사유 문구와 함께 처리한다.
}

void UFF_AtlasPinButton::SetSelectedState(bool bInSelected)
{
	// 잠긴 핀은 선택 상태가 될 수 없다. 포커스는 갈 수 있고 카드에서 사유를 보여준다.
	bSelected = bInSelected && bSelectable;
	RefreshPinVisual();
}

void UFF_AtlasPinButton::RefreshPinVisual()
{
	const ESlateVisibility Visible = ESlateVisibility::HitTestInvisible;

	if (Image_PinDefault)
	{
		Image_PinDefault->SetVisibility(
			(bSelectable && !bSelected) ? Visible : ESlateVisibility::Collapsed);
	}
	if (Image_PinSelected)
	{
		Image_PinSelected->SetVisibility(
			(bSelectable && bSelected) ? Visible : ESlateVisibility::Collapsed);
	}
	if (Image_PinLock)
	{
		Image_PinLock->SetVisibility(
			bSelectable ? ESlateVisibility::Collapsed : Visible);
	}
	if (Image_PinChevrons)
	{
		Image_PinChevrons->SetVisibility(
			bSelected ? Visible : ESlateVisibility::Collapsed);
	}
}

void UFF_AtlasPinButton::SetPinVisible(bool bVisible)
{
	// `Collapsed` 가 아니라 `Hidden` 이다 — 캔버스 절대 좌표를 매 프레임 갱신하므로
	// 레이아웃에서 빼지 않아도 되고, 다시 보일 때 위치가 이미 맞아 있다.
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}
