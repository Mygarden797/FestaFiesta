// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "UI/ClosingHUD/FF_ClosingPanelBase.h"
#include "FF_ClosingPanelFinale.generated.h"

class UFF_FestivalViewModelSubsystem;
class UTextBlock;

/**
 * 피날레가 지금 어느 시점에 있는가.
 *
 * !! 색으로만 구분하지 않는다 !! (AT-011) 단계마다 문구가 함께 붙는다.
 */
UENUM(BlueprintType)
enum class EFF_ClosingFinalePhase : uint8
{
	/** 사지 않았다. 이 패널은 아예 안 뜬다 (탭이 숨는다). */
	NotPurchased	UMETA(DisplayName = "미구매"),

	/** 샀고 아직 안 터졌다. 3일차 라이브가 끝나면 실행된다. */
	Scheduled		UMETA(DisplayName = "예정"),

	/** 터졌다. 보너스가 평가 점수에 들어갔다. */
	Done			UMETA(DisplayName = "완료")
};

/**
 * 마감·리포트의 `피날레` 패널.
 *
 * === 🔴 여기서 사지 않는다 ===
 * 피날레 구매는 **셋업의 `피날레` 탭**이 소유한다. 결산에서 살 수 있게 하면 "라이브를
 * 다 보고 나서 결과가 나쁘면 그때 산다" 가 되어 준비 단계의 결정이 의미를 잃는다.
 * 그래서 여기는 **읽기 전용**이고, 안 샀으면 탭 자체가 숨는다.
 *
 * === 언제 무엇을 보여주나 ===
 *   예정  등급 · 예정 보너스 · "3일차 라이브가 끝나면 실행됩니다"
 *   완료  등급 · 실제 보너스 · 평가 점수에 들어갔다는 사실
 */
UCLASS(Abstract, meta = (DisplayName = "FF Closing Panel - Finale"))
class FESTAFIESTA_API UFF_ClosingPanelFinale : public UFF_ClosingPanelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Closing|Finale")
	EFF_ClosingFinalePhase GetPhase() const { return Phase; }

protected:
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;

	UFUNCTION()
	void HandleFinaleGradeChanged(EFF_FinaleGrade NewGrade);

	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Finale")
	TObjectPtr<UTextBlock> Text_Grade;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Finale")
	TObjectPtr<UTextBlock> Text_Bonus;

	/** 지금 어느 시점인지. 숫자만 두면 이미 터진 것인지 알 수 없다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Finale")
	TObjectPtr<UTextBlock> Text_PhaseNote;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Finale")
	TObjectPtr<UTextBlock> Text_MissingNote;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();
	void RefreshAll();

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	EFF_ClosingFinalePhase Phase = EFF_ClosingFinalePhase::NotPurchased;
};
