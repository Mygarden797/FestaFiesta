// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/ViewModel/FF_SatisfactionBreakdown.h"
#include "FF_SatisfactionBreakdownWidget.generated.h"

class UFont;
class UTextBlock;
class UVerticalBox;

/**
 * 결산 만족도 카드의 "점수가 깎인 이유". WBP 없이 C++ 로 트리를 만든다.
 *
 * 100 에서 항목별로 깎인 점수를 큰 순서로 적고, 줄마다 게임에서 직접 볼 수 있었던 근거(시설 수 · 대기 초 · 비율)를 붙인다.
 * 항목 점수를 더하면 정확히 100 - 만족도가 된다. 마지막 줄은 가장 많이 깎은 항목을 줄이는 방법이다.
 * 결산 패널이 WBP 의 만족도 카드 안(막대 아래)에 끼워 넣는다.
 */
UCLASS(meta = (DisplayName = "FF Satisfaction Breakdown"))
class FESTAFIESTA_API UFF_SatisfactionBreakdownWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetBreakdown(const FFF_SatisfactionBreakdown& InBreakdown);

	int32 GetLossRowCount() const { return Breakdown.bValid ? Breakdown.Losses.Num() : 0; }
	FText GetSummaryText() const;
	FText GetHintText() const { return Breakdown.Hint; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report")
	FText TitleText = NSLOCTEXT("FFSatisfactionBreakdownWidget", "Title", "점수가 깎인 이유");

	/** {Lost}: 깎인 점수 합, {Score}: 만족도 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report")
	FText SummaryFormat = NSLOCTEXT("FFSatisfactionBreakdownWidget", "Summary", "100점에서 {Lost}점이 깎여 {Score}점이 됐어요.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report", meta = (ClampMin = "40.0"))
	float LabelWidth = 96.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report", meta = (ClampMin = "30.0"))
	float PointsWidth = 46.f;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildTree();
	void RefreshRows();
	UTextBlock* MakeText(const FText& Text, const TCHAR* Typeface, int32 Size, const FLinearColor& Color, bool bWrap);

	FFF_SatisfactionBreakdown Breakdown;

	UPROPERTY(Transient)
	TObjectPtr<UFont> UiFont;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Root;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryLabel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> Rows;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HintLabel;
};
