// FF_TutorialSkipWidget.h
// 역할: 실습장을 지나기 전 국가 지도에 "튜토리얼 건너뛰기" 버튼 하나를 띄운다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "FF_TutorialSkipWidget.generated.h"

class UButton;
class UFF_SkipConfirmWidget;

/* 실습장을 마치거나 건너뛰기 전, 국가 지도에 남는 "튜토리얼 건너뛰기" 버튼.

   대사창의 건너뛰기 버튼은 대사가 끝나면 대사창과 같이 사라진다. 그래서 로비 이야기를 대충 넘긴 사람은
   다대포가 왜 잠겼는지 모른 채 막혔고, 실습장을 안 하고 넘어갈 길도 없었다(유저 테스트 2026-10-13: 막혀서 이유를 물으러 옴).
   실습장을 지나기 전까지는 대사창의 건너뛰기 버튼이 있던 자리에 이 버튼을 남긴다.

   누르면 대사창과 같은 확인 창(튜토리얼 설정의 Skip Confirm Widget Class)을 띄우고, 거기서 건너뛰면
   실습장 안과 같은 진짜 건너뛰기다(UFF_TutorialSubsystem::SkipTutorial — 실습장 잠김 · 다른 축제 열림).

   ─── WBP 없이 코드로 그린다 ───────────────────────────────────────────────────
   UFF_TutorialExitWidget 과 같은 모양 · 자리(화면 오른쪽 위)다. 띄우고 내리는 건 튜토리얼 서브시스템이 한다(UpdateMapSkipButton).
*/
UCLASS()
class FESTAFIESTA_API UFF_TutorialSkipWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleSkipClicked();

	UPROPERTY(Transient)
	TObjectPtr<UButton> SkipButton;

	// 띄운 확인 창. 연타로 두 개가 겹쳐 뜨지 않게 한다 — 겹치면 하나를 닫아도 뒤에 하나가 남는다.
	TWeakObjectPtr<UFF_SkipConfirmWidget> OpenSkipConfirm;
};
