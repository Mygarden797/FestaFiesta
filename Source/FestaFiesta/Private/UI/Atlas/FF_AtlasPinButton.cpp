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

	// 실제 표시는 `RefreshPinVisual` 이 한다 — 잠김 여부에 따라 문구가 바뀌므로
	// 두 곳에서 쓰면 갈라진다.
	BadgeText = Badge;

	RefreshPinVisual();

	// 🔴 `SetIsEnabled(false)` 를 쓰지 않는다. 잠긴 핀에도 **포커스는 간다** —
	//    비활성 버튼은 게임패드 포커스에서 빠져 그 나라가 없는 것처럼 된다.
	//    선택을 막는 것은 화면이 사유 문구와 함께 처리한다.
}

void UFF_AtlasPinButton::NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent)
{
	// 베이스가 `bHasFocusPath` 를 켜고 포커스 링을 갱신한다. 먼저 부른다.
	Super::NativeOnAddedToFocusPath(InFocusEvent);

	// `PinId` 가 아직 없는 상태(SetPinData 전)에는 알리지 않는다.
	// 화면이 `NAME_None` 을 받으면 선택을 지우는 것으로 해석해 방금 띄운 카드를 도로 닫는다.
	if (!PinId.IsNone())
	{
		OnPinFocusReceived.Broadcast(this);
	}
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

	if (Text_PinBadge)
	{
		// 🔴 잠긴 핀은 배지 대신 **상태 문구**를 띄운다. 지도 위 마커는 작아서 채움/테두리
		//    차이만으로는 구분되지 않는다 (AT-011, `icon-inventory.md` 의 `● ◐ ○` 금지와 같은 이유).
		//    지역 이름은 카드에서 읽을 수 있으므로 여기서는 상태가 더 중요하다.
		const FText Shown = bSelectable
			? BadgeText
			: NSLOCTEXT("FestaFiestaUI", "AtlasPinLockedTag", "준비 중");

		// 값이 없으면 칸을 숨긴다. `0` 이나 `-` 를 넣지 않는다 (ui-scope 제약 5).
		Text_PinBadge->SetText(Shown);
		Text_PinBadge->SetVisibility(
			Shown.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

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
	if (Image_PinLockGlyph)
	{
		Image_PinLockGlyph->SetVisibility(bSelectable ? ESlateVisibility::Collapsed : Visible);
	}
	if (Image_PinSelectedGlyph)
	{
		Image_PinSelectedGlyph->SetVisibility(bSelected ? Visible : ESlateVisibility::Collapsed);
	}
}

void UFF_AtlasPinButton::SetPinVisible(bool bVisible)
{
	// `Collapsed` 가 아니라 `Hidden` 이다 — 캔버스 절대 좌표를 매 프레임 갱신하므로
	// 레이아웃에서 빼지 않아도 되고, 다시 보일 때 위치가 이미 맞아 있다.
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}
