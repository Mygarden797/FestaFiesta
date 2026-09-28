// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "FF_SkipConfirmWidget.generated.h"

class UButton;

/* 튜토리얼을 건너뛸지 묻는 창.

   버튼을 누르는 즉시 끝내지 않는 이유는 되돌릴 수 없는 선택이기 때문이다.
   서사뿐 아니라 조작 안내까지 사라지므로 한 번 물어봐야 한다.

   BP 에서 위젯을 붙이려면 이름을 맞춰야 한다.
     SkipButton     건너뛰기
     CancelButton   취소
   둘 다 없어도 컴파일은 되지만, 그러면 창을 닫을 방법이 없다.
*/
UCLASS(Abstract, meta = (DisplayName = "FF Skip Confirm"))
class FESTAFIESTA_API UFF_SkipConfirmWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

protected:
	// 창이 닫힌다. 페이드나 소리를 붙이고 싶으면 여기에 건다.
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Tutorial", meta = (DisplayName = "On Closing"))
	void BP_OnClosing(bool bSkipped);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Tutorial")
	TObjectPtr<UButton> SkipButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Tutorial")
	TObjectPtr<UButton> CancelButton;

private:
	UFUNCTION()
	void HandleSkipClicked();

	UFUNCTION()
	void HandleCancelClicked();

	/* 대사가 끝나면 창도 닫는다.

	   창이 떠 있는 사이에 대사가 끝까지 가거나 다른 경로로 멈출 수 있다. 창이
	   남아 있으면 끝까지 본 사람이 `건너뛰기` 를 눌러 안내를 전역으로 끄게 된다.
	*/
	UFUNCTION()
	void HandleDialogueActiveChanged(bool bActive);

	void Close(bool bSkipped);

	// 두 번 눌러도 한 번만 처리한다. 클릭이 빨리 두 번 들어오면
	// 이미 사라진 창에서 또 닫으려 든다.
	bool bClosing = false;
};
