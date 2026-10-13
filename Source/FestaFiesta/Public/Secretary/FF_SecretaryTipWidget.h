// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_SecretaryTipWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UHorizontalBox;
class UImage;
class UTextBlock;

/* 운영 중 비서 훈수 말풍선(2026-10-09). 왼쪽 아래에 동그란 얼굴 + 한두 줄이 잠깐 떴다 사라진다.

   WBP 없이 코드로 짠다(사건 토스트 FF_IncidentToast 와 같은 방식). 모양 값은 게임 공용 팔레트 · Pretendard
   글꼴을 그대로 쓴다. 얼굴은 대사창 초상화(튜토리얼 설정 Speaker Portraits)에서 얼굴 영역만 둥글게 잘라 쓴다.

   ─── 안 하는 것 ─────────────────────────────────────────────────────────────
   - 화면을 막지 않는다. 말풍선 칸만 눌리고(누르면 닫힘) 나머지는 아래로 지나간다.
   - 무엇을 말할지 정하지 않는다. FF_SecretarySubsystem 이 고른 문장을 띄우기만 한다.
*/
UCLASS()
class FESTAFIESTA_API UFF_SecretaryTipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 문장을 띄운다. 이미 떠 있으면 새 문장으로 바꾸고 시간을 처음부터 잰다.
	// VisibleOverride 가 0 보다 크면 그만큼 떠 있는다(결산 한마디처럼 오래 읽을 말). 아니면 설정의 훈수 시간.
	void ShowTip(const FText& Text, float VisibleOverride = -1.f);

	// 지금 닫는다(사라지는 모양을 거친다).
	void Dismiss();

	bool IsShowing() const { return Phase != EPhase::Hidden; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	enum class EPhase : uint8
	{
		Hidden,
		FadingIn,
		Visible,
		FadingOut
	};

	void BuildTree();
	void ApplyPhaseLook();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> Box_Tip;

	UPROPERTY(Transient)
	TObjectPtr<UImage> Image_Face;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_Tip;

	EPhase Phase = EPhase::Hidden;
	float PhaseTime = 0.f;
	float VisibleSeconds = 7.f;

	static constexpr float FadeInSeconds = 0.25f;
	static constexpr float FadeOutSeconds = 0.3f;
	static constexpr float SlideDistance = 14.f;
};
