// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_DialogueBoxWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
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

	if (BoxAdvanceButton)
	{
		BoxAdvanceButton->OnClicked.AddDynamic(this, &UFF_DialogueBoxWidget::HandleAdvanceClicked);
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

	if (BoxAdvanceButton)
	{
		BoxAdvanceButton->OnClicked.RemoveDynamic(this, &UFF_DialogueBoxWidget::HandleAdvanceClicked);
	}

	if (SkipButton)
	{
		SkipButton->OnClicked.RemoveDynamic(this, &UFF_DialogueBoxWidget::HandleSkipClicked);
	}

	ClearChoices();
	Super::NativeDestruct();
}

void UFF_DialogueBoxWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdatePanelLift(MyGeometry);
}

void UFF_DialogueBoxWidget::UpdatePanelLift(const FGeometry& MyGeometry)
{
	// 카드 묶음은 WBP 빌더(FF.TutorialDialogueUMG.Rebuild)가 붙인 이름이다. 없는 옛 WBP 면 옮기지 않는다.
	UWidget* Card = GetWidgetFromName(TEXT("Panel_Dialogue"));
	if (!Card)
	{
		return;
	}

	float Lift = 0.f;
	const UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this);
	const UWidget* Panel = Tutorial ? Tutorial->GetDialogueLiftPanel() : nullptr;
	if (Panel && Card->IsVisible())
	{
		/* 배치(layout) 변환으로 잰다. 그리기 변환에는 여기서 준 이동이 섞여 있어, 그것으로 재면
		   올린 만큼 덜 겹친다고 보고 다시 내려와 매 프레임 흔들린다. */
		const FGeometry& PanelGeometry = Panel->GetCachedGeometry();
		const FGeometry& CardGeometry = Card->GetCachedGeometry();
		const FSlateLayoutTransform CardLayout = CardGeometry.GetAccumulatedLayoutTransform();
		const float Scale = CardLayout.GetScale();

		if (Scale > KINDA_SMALL_NUMBER && PanelGeometry.GetLocalSize().Y > 0.f)
		{
			const float PanelTop = PanelGeometry.GetAccumulatedLayoutTransform().GetTranslation().Y;
			const float CardTop = CardLayout.GetTranslation().Y;
			const float CardBottom = CardTop + CardGeometry.GetLocalSize().Y * Scale;
			const float ScreenTop = MyGeometry.GetAccumulatedLayoutTransform().GetTranslation().Y;

			if (CardBottom > PanelTop)
			{
				// 패널 윗변 위로 올리되 화면 위로 넘기지는 않는다.
				const float Wanted = (CardBottom - PanelTop) / Scale + PanelLiftMargin;
				const float Room = FMath::Max(0.f, (CardTop - ScreenTop) / Scale - PanelLiftMargin);
				Lift = FMath::Min(Wanted, Room);
			}
		}
	}

	if (FMath::IsNearlyEqual(Lift, AppliedPanelLift, 0.5f))
	{
		return;
	}
	AppliedPanelLift = Lift;

	const FVector2D Offset(0.f, -Lift);
	Card->SetRenderTranslation(Offset);
	if (PortraitImage)
	{
		PortraitImage->SetRenderTranslation(Offset);
	}
	if (BoxAdvanceButton)
	{
		BoxAdvanceButton->SetRenderTranslation(Offset);
	}
}

void UFF_DialogueBoxWidget::HandleLineChanged(const FFF_DialogueLine& Line)
{
	// 화자가 없으면 이름칸을 숨긴다. 내레이션이나 독백일 때 빈 칸이 남으면 어색하다.
	// 이름표가 있으면 이름표째 숨긴다. 글자만 숨기면 이름표 배경이 빈 채로 남는다.
	const ESlateVisibility SpeakerVisibility =
		Line.Speaker.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible;
	if (SpeakerText)
	{
		SpeakerText->SetText(Line.Speaker);
		SpeakerText->SetVisibility(SpeakerVisibility);
	}
	if (SpeakerPlate)
	{
		SpeakerPlate->SetVisibility(SpeakerVisibility);
	}

	if (BodyText)
	{
		BodyText->SetText(Line.Text);

		// 선택지 줄은 대사가 비어 있다. 빈 글줄이 한 줄 높이를 차지해 선택지 위가 비지 않게 접는다.
		BodyText->SetVisibility(
			Line.Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	RebuildChoices(Line);

	ApplyClickArea(Line);
	ApplyBackground(Line);
	ApplyPortrait(Line);

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

		// 대사가 끝나면 배경도 치운다. 안 그러면 그림이 화면에 남는다.
		if (BackgroundImage)
		{
			BackgroundImage->SetVisibility(ESlateVisibility::Collapsed);
		}

		// 초상화도 치운다. 다음 대사가 다른 사람으로 시작하면 한 프레임 옛 얼굴이 보인다.
		if (PortraitImage)
		{
			PortraitImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		CurrentPortraitSpeaker.Reset();
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

	// 비서 브리핑은 되돌릴 것이 없다. 묻지 않고 그 브리핑만 닫는다(튜토리얼 안내는 그대로).
	if (UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this))
	{
		if (Tutorial->IsBriefingActive())
		{
			Tutorial->StopScript();
			return;
		}

		// 로비 이야기(삼촌 · 비서)도 묻지 않는다. 실습장으로 가는 줄로 넘기기만 하고 건너뜀으로 기록하지 않는다.
		// 그 줄부터 누르면 false 라 아래 확인 창으로 간다(실습장 안과 같은 진짜 건너뛰기).
		if (Tutorial->TrySkipLobbyStory())
		{
			return;
		}
	}

	// 이미 떠 있으면 또 만들지 않는다. 겹친 창은 하나를 닫아도 뒤에 하나가 남는다.
	if (const UFF_SkipConfirmWidget* Existing = OpenSkipConfirm.Get())
	{
		if (Existing->IsInViewport())
		{
			return;
		}
	}

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
		OpenSkipConfirm = Confirm;
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

void UFF_DialogueBoxWidget::ApplyBackground(const FFF_DialogueLine& Line)
{
	if (!BackgroundImage)
	{
		return;
	}

	if (Line.bClearBackground)
	{
		BackgroundImage->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// 지정이 없으면 앞 줄의 그림을 그대로 둔다.
	// 한 장면이 여러 줄로 이어질 때 매 줄에 같은 경로를 적지 않아도 된다.
	if (Line.BackgroundImage.IsNull())
	{
		return;
	}

	UTexture2D* Texture = Line.BackgroundImage.LoadSynchronous();
	if (!Texture)
	{
		// 경로를 잘못 적었다. 앞 그림을 그대로 두고 알린다.
		UE_LOG(LogFestaFiestaTutorial, Warning,
			TEXT("[대사] 배경 그림 %s 를 불러오지 못했다."),
			*Line.BackgroundImage.ToString());
		return;
	}

	BackgroundImage->SetBrushFromTexture(Texture, false);
	BackgroundImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UFF_DialogueBoxWidget::ApplyPortrait(const FFF_DialogueLine& Line)
{
	if (!PortraitImage)
	{
		return;
	}

	/* 선택지 줄은 건드리지 않는다.

	   선택지는 주인공의 대답이라 화자 칸이 비어 있다. 여기서 숨기면 대화 상대가
	   대답을 고르는 순간 사라졌다가 다음 줄에 다시 나타나 깜빡인다.
	*/
	if (Line.Kind == EFF_DialogueLineKind::Choice)
	{
		return;
	}

	const FString Speaker = Line.Speaker.ToString().TrimStartAndEnd();

	// 내레이션이다. 얼굴을 띄울 사람이 없다.
	if (Speaker.IsEmpty())
	{
		PortraitImage->SetVisibility(ESlateVisibility::Collapsed);
		CurrentPortraitSpeaker.Reset();
		return;
	}

	// 같은 사람이 이어서 말한다. 그대로 둔다.
	if (Speaker == CurrentPortraitSpeaker)
	{
		return;
	}

	const UFF_TutorialSettings* Settings = UFF_TutorialSettings::Get();
	const TSoftObjectPtr<UTexture2D>* Entry = Settings ? Settings->SpeakerPortraits.Find(Speaker) : nullptr;
	UTexture2D* Texture = Entry ? Entry->LoadSynchronous() : nullptr;

	if (!Texture)
	{
		// 목록에 없거나 경로가 틀렸다. 한 번만 알린다. 그림이 아직 없는 화자는 정상이다.
		if (!WarnedMissingSpeakers.Contains(Speaker))
		{
			WarnedMissingSpeakers.Add(Speaker);
			UE_LOG(LogFestaFiestaTutorial, Log,
				TEXT("[대사] 화자 '%s' 의 초상화가 없어 숨긴다. Project Settings > FestaFiesta Tutorial > Speaker Portraits."),
				*Speaker);
		}

		PortraitImage->SetVisibility(ESlateVisibility::Collapsed);
		CurrentPortraitSpeaker.Reset();
		return;
	}

	// 원본 크기로 맞추지 않는다(false). 크기는 BP 에서 잡은 박스를 따른다.
	PortraitImage->SetBrushFromTexture(Texture, false);
	PortraitImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	CurrentPortraitSpeaker = Speaker;
}

void UFF_DialogueBoxWidget::ApplyClickArea(const FFF_DialogueLine& Line)
{
	// 선택지가 있으면 클릭으로 넘길 수 없다. 조작을 기다리는 줄도 마찬가지다.
	const bool bCanAdvance =
		Line.Kind != EFF_DialogueLineKind::Choice && !Line.RequiresPlayerAction();

	const bool bFullScreen = bCanAdvance
		&& Line.ClickArea == EFF_DialogueClickArea::FullScreen;
	const bool bBoxOnly = bCanAdvance
		&& Line.ClickArea == EFF_DialogueClickArea::BoxOnly;

	// 비활성만으로는 히트 판정이 남는 경우가 있어 아예 숨긴다.
	auto Apply = [](UButton* Button, const bool bOn)
	{
		if (!Button)
		{
			return;
		}
		Button->SetIsEnabled(bOn);
		Button->SetVisibility(bOn ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};

	Apply(AdvanceButton, bFullScreen);
	Apply(BoxAdvanceButton, bBoxOnly);

	// 넘길 수 있을 때만 "다음" 을 보여 준다. 클릭은 받지 않는다 — 클릭은 위의 두 버튼이 받는다.
	if (AdvanceHint)
	{
		AdvanceHint->SetVisibility(bCanAdvance ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
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
