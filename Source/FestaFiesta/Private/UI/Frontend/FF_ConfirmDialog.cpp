// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Frontend/FF_ConfirmDialog.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_UILog.h"
#include "UI/Widgets/FF_StatCard.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

void UFF_ConfirmDialog::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 버튼이 자기를 넘겨주므로 핸들러 하나로 둘을 구분한다.
	if (Button_Cancel)
	{
		Button_Cancel->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_ConfirmDialog::HandleButtonClicked);
		Button_Cancel->SetButtonLabel(LOCTEXT("ConfirmDialogCancel", "취소"));
	}
	else
	{
		// 취소가 없으면 되돌릴 방법이 없는 창이 된다. 조용히 넘어가지 않는다.
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("%s: Button_Cancel 바인딩이 없다. 확인창에 취소가 없으면 안 된다."),
			*GetClass()->GetName());
	}

	if (Button_Confirm)
	{
		Button_Confirm->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_ConfirmDialog::HandleButtonClicked);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("%s: Button_Confirm 바인딩이 없다."), *GetClass()->GetName());
	}
}

void UFF_ConfirmDialog::NativeDestruct()
{
	// 해제는 조건부, 정리는 무조건. Clear() 를 쓰지 않는다 — BP 가 붙인 것까지 지워진다.
	if (Button_Cancel)
	{
		Button_Cancel->OnFFButtonClicked.RemoveDynamic(this, &UFF_ConfirmDialog::HandleButtonClicked);
	}
	if (Button_Confirm)
	{
		Button_Confirm->OnFFButtonClicked.RemoveDynamic(this, &UFF_ConfirmDialog::HandleButtonClicked);
	}

	// 창이 닫혔는데 아무 결과도 안 갔으면 취소로 본다. ESC·뒤로 가기로 닫는 경로가 그렇다.
	// 구독하는 쪽이 "확인도 취소도 안 왔다" 를 처리하지 않아도 되게 한다.
	if (!bResolved)
	{
		bResolved = true;
		OnCancelled.Broadcast();
	}

	Super::NativeDestruct();
}

UWidget* UFF_ConfirmDialog::NativeGetDesiredFocusTarget() const
{
	// 항상 취소다. 되돌릴 수 없는 행동에서 기본 포커스가 확인이면,
	// 게임패드로 확인 버튼을 두 번 눌러 실행되는 사고가 난다.
	if (Button_Cancel)
	{
		return Button_Cancel;
	}

	return Super::NativeGetDesiredFocusTarget();
}

void UFF_ConfirmDialog::Configure(
	const FText& InTitle,
	const FText& InBody,
	const TArray<FFF_ConfirmDialogRow>& InRows,
	const FText& InConfirmLabel)
{
	if (Text_Title)
	{
		Text_Title->SetText(InTitle);
	}

	if (Text_Body)
	{
		if (InBody.IsEmpty())
		{
			// 무엇이 되돌릴 수 없는지 안 적으면 확인창의 의미가 없다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("%s: 설명이 비어 있다. 되돌릴 수 없다는 사실과 그 결과를 적는다."),
				*GetClass()->GetName());
		}
		Text_Body->SetText(InBody);
	}

	if (Button_Confirm)
	{
		Button_Confirm->SetButtonLabel(
			InConfirmLabel.IsEmpty() ? LOCTEXT("ConfirmDialogConfirm", "확인") : InConfirmLabel);
	}

	// --- 수치 표 ---
	if (!VerticalBox_Rows)
	{
		return;
	}

	VerticalBox_Rows->ClearChildren();

	// 값이 없으면 칸을 숨긴다. 빈 표를 남기지 않는다.
	if (InRows.Num() == 0)
	{
		VerticalBox_Rows->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (!RowWidgetClass)
	{
		// 표를 그릴 수 없는데 조용히 넘어가면 수치가 없는 확인창이 된다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("%s: RowWidgetClass 가 없어 수치 %d줄을 그리지 못했다. WBP 에서 WBP_FF_StatCard 를 지정한다."),
			*GetClass()->GetName(), InRows.Num());
		VerticalBox_Rows->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	VerticalBox_Rows->SetVisibility(ESlateVisibility::Visible);

	for (const FFF_ConfirmDialogRow& Row : InRows)
	{
		UFF_StatCard* Card = CreateWidget<UFF_StatCard>(this, RowWidgetClass);
		if (!Card)
		{
			UE_LOG(LogFestaFiestaUI, Error,
				TEXT("%s: 수치 행 생성 실패 (%s)."), *GetClass()->GetName(), *RowWidgetClass->GetName());
			continue;
		}

		Card->SetStat(Row.Label, Row.Value);
		VerticalBox_Rows->AddChild(Card);
	}
}

void UFF_ConfirmDialog::HandleButtonClicked(UFF_CommonButtonBase* Button)
{
	// 되돌릴 수 없는 행동이 두 번 실행되면 안 된다. 연타·게임패드 중복 입력을 막는다.
	if (bResolved)
	{
		return;
	}
	bResolved = true;

	const bool bConfirmed = (Button == Button_Confirm);
	if (bConfirmed)
	{
		OnConfirmed.Broadcast();
	}
	else
	{
		OnCancelled.Broadcast();
	}

	// 결과가 하나뿐이고 열려 있는 창을 추적하는 주인이 없으므로 스스로 닫는다.
	// 준비 단계 패널(UFF_PanelFrame)은 반대다 — 그쪽은 UFF_PrepHudBar 가 소유한다.
	DeactivateWidget();
}

#undef LOCTEXT_NAMESPACE
