// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/Panels/FF_PrepDetailCard.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UFF_PrepDetailCard::ApplyCardState(EFF_PrepCardState InState, const FText& InBadgeText)
{
	CardState = InState;

	// 베이스의 선택 표현(배경 브러시 교체)과 카드 상태를 한 번에 맞춘다.
	// 여기서 부르지 않으면 배경과 글자색이 서로 다른 상태를 가리킨다.
	SetSelected(InState == EFF_PrepCardState::Selected);

	// 확정했거나 잠긴 카드는 누를 수 없다. **비활성만으로 끝내지 않고** 배지에 이유를 남긴다.
	const bool bInteractable =
		InState == EFF_PrepCardState::Selectable || InState == EFF_PrepCardState::Selected;

	if (Button_Root)
	{
		Button_Root->SetIsEnabled(bInteractable);
	}

	SetTextOrHide(Text_Badge, InBadgeText);
	ApplyContentColor();
}

void UFF_PrepDetailCard::ApplyContentColor()
{
	// 🔴 전체 재선언. 베이스는 `Text_Label`·`Image_Icon` 만 칠하므로 나머지는 매번 다시 정한다.
	// 일부만 갱신하면 `선택함 → 잠김` 처럼 상태를 건너뛸 때 이전 색이 남는다.
	FLinearColor Content = NormalContentColor;
	switch (CardState)
	{
	case EFF_PrepCardState::Selected:
		// 선택되면 배경이 어두워진다. 밝은 글자가 아니면 안 읽힌다.
		Content = SelectedContentColor;
		break;

	case EFF_PrepCardState::Committed:
	case EFF_PrepCardState::Locked:
		Content = DisabledContentColor;
		break;

	default:
		break;
	}

	TArray<UTextBlock*> StateTexts;
	CollectStateTexts(StateTexts);

	// 🔴 `Text_Label` 도 여기서 다시 칠한다. 베이스의 `RefreshVisualState` 는 **선택/비선택
	// 2상태만** 알아서 `확정함`·`잠김` 에서 제목만 진한 `Ink` 로 남는다 — 한 장의 카드가
	// 두 상태를 동시에 가리키게 된다. `ApplyCardState` 가 `SetSelected` 뒤에 이걸 부르므로
	// 순서상 여기서 덮는 것이 마지막이다.
	StateTexts.Add(Text_Label);

	const FSlateColor ContentSlate(Content);
	for (UTextBlock* TextBlock : StateTexts)
	{
		if (TextBlock)
		{
			TextBlock->SetColorAndOpacity(ContentSlate);
		}
	}

	// 배지는 본문과 다르게 둔다 — 잠긴 카드에서 이유까지 흐려지면 읽으라고 쓴 문구가 안 보인다.
	// 확정한 카드만 흐리게 한다. 그건 이미 끝난 사실이라 눈에 덜 띄어도 된다.
	if (Text_Badge)
	{
		const FLinearColor BadgeColor =
			(CardState == EFF_PrepCardState::Selected) ? SelectedContentColor
			: (CardState == EFF_PrepCardState::Committed) ? DisabledContentColor
			: NormalContentColor;

		Text_Badge->SetColorAndOpacity(FSlateColor(BadgeColor));
	}
}

void UFF_PrepDetailCard::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
{
	if (!TextBlock)
	{
		return;
	}

	TextBlock->SetText(Value);
	TextBlock->SetVisibility(Value.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}
