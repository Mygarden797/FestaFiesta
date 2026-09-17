// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_DialogueBoxWidget.h"

#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Tutorial/FF_SkipConfirmWidget.h"
#include "Tutorial/FF_TutorialSettings.h"
#include "Tutorial/FF_TutorialSubsystem.h"

void UFF_DialogueBoxWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (AdvanceButton)
	{
		AdvanceButton->OnClicked.AddDynamic(this, &UFF_DialogueBoxWidget::HandleAdvanceClicked);
	}

	if (SkipButton)
	{
		SkipButton->OnClicked.AddDynamic(this, &UFF_DialogueBoxWidget::HandleSkipClicked);
	}

	UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	if (!Tutorial)
	{
		// 튜토리얼 시스템이 없는 화면에서도 이 위젯을 쓸 수 있게 조용히 넘긴다.
		return;
	}

	Tutorial->OnLineChanged.AddDynamic(this, &UFF_DialogueBoxWidget::HandleLineChanged);
	Tutorial->OnDialogueActiveChanged.AddDynamic(this, &UFF_DialogueBoxWidget::HandleDialogueActiveChanged);
	bBound = true;

	// 이미 진행 중인 대사가 있으면 지금 상태를 반영한다.
	// 위젯이 대사보다 늦게 만들어지는 경우가 있어서, 그때 첫 줄이 조용히 빠진다.
	if (Tutorial->IsDialogueActive())
	{
		HandleDialogueActiveChanged(true);
		HandleLineChanged(Tutorial->GetCurrentLine());
	}
	else
	{
		HandleDialogueActiveChanged(false);
	}
}

void UFF_DialogueBoxWidget::NativeDestruct()
{
	if (bBound)
	{
		if (UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this))
		{
			Tutorial->OnLineChanged.RemoveDynamic(this, &UFF_DialogueBoxWidget::HandleLineChanged);
			Tutorial->OnDialogueActiveChanged.RemoveDynamic(this, &UFF_DialogueBoxWidget::HandleDialogueActiveChanged);
		}
		bBound = false;
	}

	if (AdvanceButton)
	{
		AdvanceButton->OnClicked.RemoveDynamic(this, &UFF_DialogueBoxWidget::HandleAdvanceClicked);
	}

	if (SkipButton)
	{
		SkipButton->OnClicked.RemoveDynamic(this, &UFF_DialogueBoxWidget::HandleSkipClicked);
	}

	ClearChoices();
	Super::NativeDestruct();
}

void UFF_DialogueBoxWidget::HandleLineChanged(const FFF_DialogueLine& Line)
{
	if (SpeakerText)
	{
		SpeakerText->SetText(Line.Speaker);

		// 화자가 없으면 이름칸을 숨긴다. 내레이션이나 독백일 때 빈 칸이 남으면 어색하다.
		SpeakerText->SetVisibility(
			Line.Speaker.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	if (BodyText)
	{
		BodyText->SetText(Line.Text);
	}

	RebuildChoices(Line);

	// 선택지가 있으면 클릭으로 넘길 수 없다. 조작을 기다리는 줄도 마찬가지다.
	const bool bCanAdvance =
		Line.Kind != EFF_DialogueLineKind::Choice && !Line.RequiresPlayerAction();
	SetAdvanceEnabled(bCanAdvance);

	BP_OnLineChanged(Line);
}

void UFF_DialogueBoxWidget::HandleDialogueActiveChanged(const bool bActive)
{
	// 루트는 스스로 클릭을 먹지 않는다. 자식만 받게 한다.
	//
	// Visible 로 두면 위젯 영역 전체가 히트 판정을 가져간다. 이 위젯은 화면 전체에
	// 붙어 있어서, 그러면 AdvanceButton 을 숨겨도 아래 UI 가 전부 막힌다.
	SetVisibility(bActive
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);

	if (!bActive)
	{
		ClearChoices();
	}

	BP_OnDialogueActiveChanged(bActive);
}

void UFF_DialogueBoxWidget::HandleAdvanceClicked()
{
	if (UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this))
	{
		// 넘길 수 있는지는 서브시스템이 판단한다. 여기서 또 검사하면 규칙이 두 곳에 생긴다.
		Tutorial->Advance();
	}
}

void UFF_DialogueBoxWidget::HandleSkipClicked()
{
	// 바로 끝내지 않는다. 되돌릴 수 없는 선택이라 확인을 받는다.
	//
	// 확인 창을 여기서 띄우는 이유는, BP 그래프에 로직을 두면 깃에서 병합도
	// 리뷰도 안 되기 때문이다. BP 는 배치와 모양만 맡는다.
	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	UClass* ConfirmClass = Settings ? Settings->SkipConfirmWidgetClass.LoadSynchronous() : nullptr;

	if (!ConfirmClass)
	{
		// 확인 창이 지정되지 않았다. 확인 없이 끝내지는 않는다.
		UE_LOG(LogFestaFiestaTutorial, Warning,
			TEXT("[튜토리얼] 건너뛰기 확인 창이 지정되지 않았다. Project Settings 를 확인할 것."));
		BP_OnSkipRequested();
		return;
	}

	UFF_SkipConfirmWidget* Confirm = CreateWidget<UFF_SkipConfirmWidget>(GetOwningPlayer(), ConfirmClass);
	if (Confirm)
	{
		Confirm->AddToViewport(Settings->SkipConfirmZOrder);
	}

	BP_OnSkipRequested();
}

void UFF_DialogueBoxWidget::ConfirmSkipTutorial()
{
	if (UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this))
	{
		Tutorial->SkipTutorial();
	}
}

void UFF_DialogueBoxWidget::HandleChoiceSelected(const int32 ChoiceIndex)
{
	if (UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this))
	{
		Tutorial->SelectChoice(ChoiceIndex);
	}
}

void UFF_DialogueBoxWidget::RebuildChoices(const FFF_DialogueLine& Line)
{
	ClearChoices();

	if (Line.Kind != EFF_DialogueLineKind::Choice || !ChoiceContainer || !ChoiceButtonClass)
	{
		return;
	}

	for (int32 Index = 0; Index < Line.Choices.Num(); ++Index)
	{
		UFF_DialogueChoiceButton* Button =
			CreateWidget<UFF_DialogueChoiceButton>(this, ChoiceButtonClass);
		if (!Button)
		{
			continue;
		}

		Button->Setup(this, Index, Line.Choices[Index].Text);
		ChoiceContainer->AddChild(Button);
		ChoiceButtons.Add(Button);
	}
}

void UFF_DialogueBoxWidget::ClearChoices()
{
	for (UFF_DialogueChoiceButton* Button : ChoiceButtons)
	{
		if (Button)
		{
			Button->RemoveFromParent();
		}
	}
	ChoiceButtons.Reset();

	if (ChoiceContainer)
	{
		ChoiceContainer->ClearChildren();
	}
}

void UFF_DialogueBoxWidget::SetAdvanceEnabled(const bool bEnabled)
{
	if (!AdvanceButton)
	{
		return;
	}

	AdvanceButton->SetIsEnabled(bEnabled);

	// 클릭 영역이 선택지를 덮고 있으면 선택지를 못 누른다.
	// 비활성만으로는 히트 판정이 남는 경우가 있어 아예 숨긴다.
	AdvanceButton->SetVisibility(
		bEnabled ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

// ── 선택지 버튼 ─────────────────────────────────────────────────────────────

void UFF_DialogueChoiceButton::NativeConstruct()
{
	Super::NativeConstruct();

	if (ChoiceButton)
	{
		ChoiceButton->OnClicked.AddDynamic(this, &UFF_DialogueChoiceButton::HandleClicked);
	}
}

void UFF_DialogueChoiceButton::Setup(
	UFF_DialogueBoxWidget* InOwner, const int32 InChoiceIndex, const FText& InText)
{
	Owner = InOwner;
	ChoiceIndex = InChoiceIndex;

	if (ChoiceText)
	{
		ChoiceText->SetText(InText);
	}
	BP_OnChoiceTextSet(InText);
}

void UFF_DialogueChoiceButton::HandleClicked()
{
	if (UFF_DialogueBoxWidget* OwnerBox = Owner.Get())
	{
		OwnerBox->HandleChoiceSelected(ChoiceIndex);
	}
}
