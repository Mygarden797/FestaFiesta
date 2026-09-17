// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Tutorial/FF_TutorialTypes.h"
#include "FF_DialogueBoxWidget.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UFF_DialogueChoiceButton;

// 화면 아래에 붙는 대사창.
//
// 진행은 튜토리얼 서브시스템이 하고 여기서는 그리기만 한다.
// 다음 줄로 넘기는 판단도 서브시스템이 하므로, 클릭하면 그냥 알리기만 한다.
//
// BP 에서 위젯을 붙이려면 이름을 맞춰야 한다.
//   SpeakerText     화자 이름
//   BodyText        대사
//   AdvanceButton   클릭 영역. 대사창 전체를 덮는 투명 버튼으로 두면 편하다
//   ChoiceContainer 선택지 버튼이 들어갈 자리
UCLASS(Abstract, meta = (DisplayName = "FF Dialogue Box"))
class FESTAFIESTA_API UFF_DialogueBoxWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 선택지를 눌렀을 때 자식 버튼이 부른다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Dialogue")
	void HandleChoiceSelected(int32 ChoiceIndex);

	// 확인 창에서 "건너뛰기" 를 골랐을 때 BP 가 부른다.
	//
	// 버튼을 누르는 즉시 끝내지 않는 이유는, 되돌릴 수 없는 선택이라서다.
	// 조작 안내까지 사라지므로 한 번 물어봐야 한다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Dialogue")
	void ConfirmSkipTutorial();

protected:
	// 줄이 바뀌었다. 애니메이션이나 초상화 교체를 여기에 붙인다.
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Dialogue", meta = (DisplayName = "On Line Changed"))
	void BP_OnLineChanged(const FFF_DialogueLine& Line);

	// 대사창이 뜨거나 내려간다. 페이드를 여기에 붙인다.
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Dialogue", meta = (DisplayName = "On Dialogue Active Changed"))
	void BP_OnDialogueActiveChanged(bool bActive);

	// 건너뛰기 버튼을 눌렀다. 여기서 확인 창을 띄운다.
	//
	// 확인 창을 C++ 에서 만들지 않는 이유는, 그러면 이 위젯이 다른 UI 를 알아야 하고
	// 창 모양을 바꿀 때마다 여기도 고쳐야 하기 때문이다.
	// 띄우는 건 BP 가 하고, 사용자가 확인하면 ConfirmSkipTutorial 을 부른다.
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Dialogue", meta = (DisplayName = "On Skip Requested"))
	void BP_OnSkipRequested();

	// 선택지 버튼을 만들 클래스. 지정하지 않으면 선택지가 표시되지 않는다.
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|Dialogue")
	TSubclassOf<UFF_DialogueChoiceButton> ChoiceButtonClass;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Dialogue")
	TObjectPtr<UTextBlock> SpeakerText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Dialogue")
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Dialogue")
	TObjectPtr<UButton> AdvanceButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Dialogue")
	TObjectPtr<UPanelWidget> ChoiceContainer;

	// 건너뛰기 버튼. 없어도 동작한다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Dialogue")
	TObjectPtr<UButton> SkipButton;

private:
	UFUNCTION()
	void HandleLineChanged(const FFF_DialogueLine& Line);

	UFUNCTION()
	void HandleDialogueActiveChanged(bool bActive);

	UFUNCTION()
	void HandleAdvanceClicked();

	UFUNCTION()
	void HandleSkipClicked();

	void RebuildChoices(const FFF_DialogueLine& Line);
	void ClearChoices();

	// 선택지가 떠 있는 동안에는 클릭으로 넘기지 못하게 한다.
	// 안 그러면 아무 데나 눌러서 선택을 건너뛰게 된다.
	void SetAdvanceEnabled(bool bEnabled);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_DialogueChoiceButton>> ChoiceButtons;

	bool bBound = false;
};

// 선택지 버튼 하나.
//
// 자기 번호를 들고 있다가 눌리면 대사창에 알린다.
UCLASS(Abstract, meta = (DisplayName = "FF Dialogue Choice Button"))
class FESTAFIESTA_API UFF_DialogueChoiceButton : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void Setup(UFF_DialogueBoxWidget* InOwner, int32 InChoiceIndex, const FText& InText);

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Dialogue", meta = (DisplayName = "On Choice Text Set"))
	void BP_OnChoiceTextSet(const FText& Text);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Dialogue")
	TObjectPtr<UTextBlock> ChoiceText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Dialogue")
	TObjectPtr<UButton> ChoiceButton;

private:
	UFUNCTION()
	void HandleClicked();

	TWeakObjectPtr<UFF_DialogueBoxWidget> Owner;
	int32 ChoiceIndex = INDEX_NONE;
};
