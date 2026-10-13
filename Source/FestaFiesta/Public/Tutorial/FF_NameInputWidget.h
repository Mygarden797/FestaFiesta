// FF_NameInputWidget.h
// 역할: 대사의 이름 입력 줄에서 주인공 이름을 받는 작은 창을 띄운다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "FF_NameInputWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

/* 주인공 이름 입력 창(EFF_DialogueLineKind::NameInput).

   테스터 피드백(2026-10-13): 미연시처럼 "당신의 이름은?" 을 묻고, 넣은 이름이 대화창에 나오면 좋겠다.
   묻는 말은 대사창이 그대로 보여 주고, 이 창은 입력칸과 확인 버튼만 맡는다.
   이름을 넣고 확인(또는 Enter)을 누르면 UFF_TutorialSubsystem::SubmitPlayerName 이 받아 다음 줄로 넘긴다.
   띄우고 내리는 건 대사창이 줄을 받을 때 한다(UFF_DialogueBoxWidget::ApplyNameInput).

   ─── WBP 없이 코드로 그린다 ───────────────────────────────────────────────────
   입력칸 하나와 버튼 하나라 애셋을 따로 두지 않는다. 판 · 버튼 그림은 LightTeal 테마, 글꼴은 프로젝트 UI 글꼴이다.
   자리는 화면 가운데 조금 위 — 아래쪽 대사창(묻는 말)을 가리지 않는다.
*/
UCLASS()
class FESTAFIESTA_API UFF_NameInputWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION()
	void HandleTextChanged(const FText& Text);

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleConfirmClicked();

	// 입력한 이름을 넘긴다. 받아들여지면 대사창이 다음 줄에서 이 창을 내린다.
	void Submit();

	// 확인 버튼을 이름이 있을 때만 켠다.
	void RefreshConfirm();

	int32 GetMaxLength() const;

	UPROPERTY(Transient)
	TObjectPtr<UEditableTextBox> NameBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmLabel;

	// 확인 글자색. 켜졌을 때(진한 판 위 흰 글자) · 꺼졌을 때(옅은 판 위 흐린 글자).
	FLinearColor ConfirmReadyColor = FLinearColor::White;
	FLinearColor ConfirmIdleColor = FLinearColor::Gray;

	// 확인 버튼 모양을 한 번이라도 맞췄는지. 처음 한 번은 상태가 같아도 글자색을 칠한다.
	bool bConfirmStyled = false;

	// 띄운 뒤 입력칸에 커서를 한 번 넣는다. 화면에 붙기 전에는 포커스가 안 먹어서 틱에서 한다.
	bool bFocusedOnce = false;
};
