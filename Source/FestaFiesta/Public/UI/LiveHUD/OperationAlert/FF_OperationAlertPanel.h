// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/LiveHUD/OperationAlert/FF_OperationAlertTypes.h"
#include "FF_OperationAlertPanel.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UFont;
class UTextBlock;
class UVerticalBox;
class UFF_FestivalViewModelSubsystem;
class UFF_OperationAlertSubsystem;
class UFF_LiveUtilityDock;

/**
 * 운영 알림을 화면 오른쪽 위에 잠깐 띄우고, "지난 알림" 버튼으로 낸 알림을 다시 보여 준다. WBP 없이 C++ 로 트리를 만든다.
 *
 * 사건 알림(UFF_IncidentToast, 위쪽 가운데)과 같은 카드 모양을 쓰되 자리를 오른쪽으로 나눠 겹치지 않게 했다.
 * 카드는 DisplaySeconds 동안 보이다가 마지막 FadeSeconds 동안 흐려지며 사라진다. 시간은 위젯 틱의 실제 시간이라 배속과 무관하다.
 * Live 단계에서만 보인다. 카드는 클릭을 막지 않고 버튼과 펼친 목록만 클릭을 받는다.
 */
UCLASS(meta = (DisplayName = "FF Operation Alert Panel"))
class FESTAFIESTA_API UFF_OperationAlertPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxToasts = 3;

	void AddAlert(const FFF_OperationAlert& Alert);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|OperationAlert")
	void ToggleHistory();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|OperationAlert")
	bool IsHistoryOpen() const { return bHistoryOpen; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|OperationAlert")
	int32 GetToastCount() const { return Toasts.Num(); }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|OperationAlert")
	int32 GetHistoryCount() const { return History.Num(); }

	/** Index 0 이 가장 최근 카드다. */
	FText GetToastMessageAt(int32 Index) const;

	/** 틱 없이 시간을 흘린다. 자동화 테스트와 NativeTick 이 같은 길을 쓴다. */
	void AdvanceTime(float DeltaSeconds);

	/** 테스트가 단계와 상관없이 보이게 할 때 쓴다. */
	void SetForceVisible(bool bForce) { bForceVisible = bForce; ApplyVisibility(); }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|OperationAlert")
	float DisplaySeconds = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|OperationAlert")
	float FadeSeconds = 0.6f;

	/** 화면 오른쪽 위 모서리에서 떨어진 거리. 위쪽은 상단 상태 줄 아래로 내린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|OperationAlert")
	FVector2D ScreenMargin = FVector2D(24.f, 132.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|OperationAlert", meta = (ClampMin = "160.0"))
	float CardWidth = 340.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|OperationAlert", meta = (ClampMin = "1"))
	int32 MaxHistoryRows = 10;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	struct FToast
	{
		TObjectPtr<UBorder> Card;
		TObjectPtr<UTextBlock> Text;
		float RemainingSeconds = 0.f;
	};

	void BuildTree();
	void Bind();
	UBorder* MakeCard(const FFF_OperationAlert& Alert, bool bInHistory, UTextBlock*& OutText);
	void RemoveToastAt(int32 Index);
	void RefreshHistoryList();
	void RefreshButtonLabel();
	void ApplyVisibility();
	bool ShouldShow() const;
	bool UsesUnifiedNews();

	UFUNCTION()
	void HandleAlertRaised(const FFF_OperationAlert& Alert);

	UFUNCTION()
	void HandleHistoryClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> Canvas;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ToastList;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HistoryButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HistoryButtonLabel;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> HistoryPanel;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> HistoryList;

	UPROPERTY(Transient)
	TObjectPtr<UFont> UiFont;

	UPROPERTY(Transient)
	TObjectPtr<UFF_OperationAlertSubsystem> BoundAlerts;

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> BoundViewModel;

	TArray<FToast> Toasts;
	TArray<FFF_OperationAlert> History;
	bool bHistoryOpen = false;
	bool bForceVisible = false;
	bool bWasShown = false;
	TWeakObjectPtr<UFF_LiveUtilityDock> CachedNewsDock;
};
