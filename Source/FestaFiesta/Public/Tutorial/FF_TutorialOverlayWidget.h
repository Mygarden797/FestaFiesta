// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "FF_TutorialOverlayWidget.generated.h"

// 튜토리얼 중 화면을 어둡게 덮고 지시한 위치만 밝게 남긴다.
//
// ─── 입력을 막지 않는다 ─────────────────────────────────────────────────────
// Visibility 를 Hit Test Invisible 로 두어 클릭이 그대로 아래로 지나간다.
// 막아두면 플레이어가 갇히고, UI 가 바뀔 때마다 막을 대상을 다시 챙겨야 한다.
//
// 어차피 지시한 걸 하기 전에는 튜토리얼이 넘어가지 않으므로, 딴 데를 눌러도
// 결국 돌아오게 된다. 시각적으로 유도만 하는 편이 유지가 쉽다.
//
// ─── 왜 직접 그리는가 ───────────────────────────────────────────────────────
// 구멍 뚫린 사각형을 이미지로 만들면 위치와 크기가 매번 달라져서 늘어난다.
// 어두운 사각형 네 장으로 목표 주변을 감싸면 크기가 자유롭고 늘어남도 없다.
UCLASS(Abstract, meta = (DisplayName = "FF Tutorial Overlay"))
class FESTAFIESTA_API UFF_TutorialOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

	/* 보기 전용 패널이 열려 있을 때만 클릭을 받는다. 받으면 패널을 닫는다.

	   평소에는 클릭을 그대로 아래로 흘린다(아래 "입력을 막지 않는다" 참고). 대사 중에 준비 화면
	   패널이 열려 있으면 그 패널은 보여주기만 하고 누를 수 없어야 한다(피드백). 이 위젯이 이미
	   준비 화면 위에 화면 전체로 떠 있어서, 그동안만 클릭을 받아 패널을 닫는 데 쓴다.
	*/
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

protected:
	// 덮개 색. 알파로 어두운 정도를 정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Tutorial")
	FLinearColor MaskColor = FLinearColor(0.f, 0.f, 0.f, 0.65f);

	// 강조 테두리 색.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Tutorial")
	FLinearColor OutlineColor = FLinearColor(1.f, 0.85f, 0.3f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Tutorial", meta = (ClampMin = "0.0"))
	float OutlineThickness = 3.f;

	// 목표 주변에 남길 여백. 버튼에 딱 붙으면 답답해 보인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Tutorial", meta = (ClampMin = "0.0"))
	float HolePadding = 8.f;

	// 테두리가 깜빡이는 주기(초). 0 이하면 깜빡이지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Tutorial", meta = (ClampMin = "0.0"))
	float PulsePeriodSeconds = 1.2f;

	// 깜빡일 때 가장 흐려지는 정도.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Tutorial", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PulseMinAlpha = 0.35f;

	// 강조 대상이 바뀌었다. 화살표나 손가락 아이콘을 붙이고 싶으면 여기에 건다.
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Tutorial", meta = (DisplayName = "On Highlight Rect Changed"))
	void BP_OnHighlightRectChanged(FVector2D TopLeft, FVector2D Size, bool bHasTarget);

	// 세계 지점을 가리키는 중이다. 화살표를 붙이고 싶으면 여기에 건다.
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Tutorial", meta = (DisplayName = "On Highlight Circle Changed"))
	void BP_OnHighlightCircleChanged(FVector2D Center, float Radius, bool bHasTarget);

	// 세계 지점을 가리킬 때 화면 전체를 덮을지.
	//
	// 덮으면 시선이 확실히 모이는데, 주변 상황을 못 본다.
	// "여기 놓으세요" 는 주변을 봐야 하므로 기본은 끈다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|Tutorial")
	bool bMaskScreenForWorldTarget = false;

private:
	UFUNCTION()
	void HandleHighlightAnchorChanged(FName AnchorId);

	UFUNCTION()
	void HandleDialogueActiveChanged(bool bActive);

	// 등록된 위젯의 화면 영역을 구한다. 없으면 false.
	bool ResolveHighlightRect(FVector2D& OutTopLeft, FVector2D& OutSize) const;

	// 세계 마커를 화면 좌표로 옮긴다. 없거나 화면 밖이면 false.
	//
	// 탑뷰라 대상이 카메라 뒤로 가거나 가려지는 경우가 거의 없다.
	// 자유 시점이었으면 훨씬 까다로웠을 것이다.
	bool ResolveWorldCircle(FVector2D& OutCenter, float& OutRadius) const;

	FName CurrentAnchor = NAME_None;

	// 지난 프레임에 구한 영역. 매 프레임 다시 구하는 이유는 스크롤이나 창 크기 변경으로
	// 위젯이 움직일 수 있기 때문이다.
	FVector2D CachedTopLeft = FVector2D::ZeroVector;
	FVector2D CachedSize = FVector2D::ZeroVector;
	bool bHasCachedRect = false;

	// 세계 지점을 가리킬 때. UI 와 모양을 다르게 해서 무엇을 가리키는지 구분한다.
	FVector2D CachedWorldCenter = FVector2D::ZeroVector;
	float CachedWorldRadius = 0.f;
	bool bHasCachedCircle = false;

	float PulseTime = 0.f;
	bool bBound = false;

	// 보기 전용 패널 때문에 지금 클릭을 받고 있는지.
	bool bBlockingViewOnlyPanel = false;

	// 클릭을 받을지에 맞춰 보이기 설정을 바꾼다.
	void SetBlockingViewOnlyPanel(bool bBlock);
};
