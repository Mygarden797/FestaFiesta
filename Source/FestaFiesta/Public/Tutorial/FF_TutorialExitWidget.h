// FF_TutorialExitWidget.h
// 역할: 튜토리얼 대사가 끝난 뒤 튜토리얼 레벨을 나가는 버튼 하나를 띄운다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "FF_TutorialExitWidget.generated.h"

class UButton;
class UTextBlock;

/* 튜토리얼을 끝까지 본 뒤 남는 "나가기" 버튼.

   대사가 끝나면 대사창과 함께 건너뛰기 버튼이 사라져, 흐름을 타지 않고 레벨을 연 경우
   (에디터에서 튜토리얼 맵으로 바로 실행 등)에는 레벨을 나갈 방법이 없었다(피드백: "끝날 때까지 갇힘").
   흐름을 타고 들어왔으면 대사가 끝나는 대로 레벨을 떠나므로 이 버튼은 잠깐만 보인다.

   ─── WBP 없이 코드로 그린다 ───────────────────────────────────────────────────
   버튼 하나라 애셋을 따로 두지 않는다. 색은 LightTeal 팔레트, 글꼴은 프로젝트 UI 글꼴이다.
   자리는 대사창의 건너뛰기 버튼이 있던 화면 오른쪽 위다.
*/
UCLASS()
class FESTAFIESTA_API UFF_TutorialExitWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleExitClicked();

	UPROPERTY(Transient)
	TObjectPtr<UButton> ExitButton;
};
