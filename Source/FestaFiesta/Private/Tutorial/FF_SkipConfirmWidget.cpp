// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tutorial/FF_SkipConfirmWidget.h"

#include "Components/Button.h"
#include "Tutorial/FF_TutorialSubsystem.h"

void UFF_SkipConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (SkipButton)
	{
		SkipButton->OnClicked.AddDynamic(this, &UFF_SkipConfirmWidget::HandleSkipClicked);
	}

	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UFF_SkipConfirmWidget::HandleCancelClicked);
	}
}

void UFF_SkipConfirmWidget::NativeDestruct()
{
	if (SkipButton)
	{
		SkipButton->OnClicked.RemoveDynamic(this, &UFF_SkipConfirmWidget::HandleSkipClicked);
	}

	if (CancelButton)
	{
		CancelButton->OnClicked.RemoveDynamic(this, &UFF_SkipConfirmWidget::HandleCancelClicked);
	}

	Super::NativeDestruct();
}

void UFF_SkipConfirmWidget::HandleSkipClicked()
{
	Close(true);
}

void UFF_SkipConfirmWidget::HandleCancelClicked()
{
	Close(false);
}

void UFF_SkipConfirmWidget::Close(const bool bSkipped)
{
	if (bClosing)
	{
		return;
	}
	bClosing = true;

	// 창을 먼저 알린다. 아래에서 자기를 제거하므로 그 뒤에는 아무것도 못 한다.
	BP_OnClosing(bSkipped);

	if (bSkipped)
	{
		if (UFF_TutorialSubsystem* Tutorial = UFF_TutorialSubsystem::Get(this))
		{
			Tutorial->SkipTutorial();
		}
	}

	RemoveFromParent();
}
