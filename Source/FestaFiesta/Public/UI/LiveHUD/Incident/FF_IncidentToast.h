// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_IncidentToast.generated.h"

class UBorder;
class UCanvasPanel;
class UTextBlock;

/**
 * 사건 처리 결과를 화면 위쪽 가운데에 잠깐 띄우는 알림. WBP 없이 C++ 만으로 트리를 만든다.
 *
 * Show 를 부를 때마다 문구와 색을 바꾸고 DisplaySeconds 동안 보여 준 뒤, 마지막 FadeSeconds 동안 흐려지며 사라진다.
 * 시간은 위젯 틱의 실제 시간으로 센다. 배속·일시정지와 상관없이 같은 시간 동안 읽을 수 있어야 한다.
 * 전체가 HitTestInvisible 이라 월드 클릭을 막지 않는다. 숨어 있을 때는 Collapsed.
 */
UCLASS(meta = (DisplayName = "FF Incident Toast"))
class FESTAFIESTA_API UFF_IncidentToast : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Incident")
	void Show(const FText& Message, bool bPositive);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Incident")
	FText GetMessage() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Incident")
	bool IsShowing() const { return RemainingSeconds > 0.f; }

	/** 틱 없이 시간을 흘린다. 자동화 테스트와 NativeTick 이 같은 길을 쓴다. */
	void AdvanceTime(float DeltaSeconds);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	float DisplaySeconds = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	float FadeSeconds = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	float TopOffset = 96.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	FLinearColor PositiveColor = FLinearColor(0.07f, 0.32f, 0.2f, 0.94f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Incident")
	FLinearColor NegativeColor = FLinearColor(0.45f, 0.12f, 0.1f, 0.94f);

private:
	void BuildTree();
	void ApplyVisibility();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Card;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_Message;

	float RemainingSeconds = 0.f;
};
