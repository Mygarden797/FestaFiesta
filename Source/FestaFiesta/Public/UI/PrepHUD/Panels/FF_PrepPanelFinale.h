#pragma once

#include "CoreMinimal.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "UI/PrepHUD/Panels/FF_PrepFinaleCard.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "FF_PrepPanelFinale.generated.h"

class UFF_FestivalViewModelSubsystem;
class UFF_FinaleConfirmDialog;
class UPanelWidget;

/** 마지막 날 아침 점검의 피날레 선택·구매 화면. 경제 명령과 상태는 ViewModel을 통한다. */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Panel - Finale"))
class FESTAFIESTA_API UFF_PrepPanelFinale : public UFF_PrepPanelBase
{
	GENERATED_BODY()

public:
	UFF_PrepPanelFinale();
	void ConfigurePresentationDefaults(TSubclassOf<UFF_PrepFinaleCard> InCardClass,
		TSubclassOf<UFF_FinaleConfirmDialog> InConfirmClass,
		const TMap<EFF_FinaleGrade, FFF_FinaleTierPresentation>& InPresentations);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;

	UFUNCTION() void HandlePurchaseClicked();
	UFUNCTION() void HandleLaterClicked();
	UFUNCTION() void HandlePurchaseConfirmed();
	UFUNCTION() void HandlePurchaseCancelled();
	UFUNCTION() void HandleGoldChanged(int32 NewValue);
	UFUNCTION() void HandlePurchasedGradeChanged(EFF_FinaleGrade NewGrade);
	UFUNCTION() void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);
	void HandleCardClicked(int32 GradeId);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UPanelWidget> Box_FinaleCards;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_StageNotice;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_OnceNotice;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_SelectedFinale;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_PurchaseCost;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_GoldAfterValue;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_GoldAfterLabel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_BlockReason;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UTextBlock> Text_PurchaseLabel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UButton> Button_Later;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Finale")
	TObjectPtr<UButton> Button_Purchase;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale")
	TSubclassOf<UFF_PrepFinaleCard> CardWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale")
	TSubclassOf<UFF_FinaleConfirmDialog> ConfirmDialogClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale")
	TMap<EFF_FinaleGrade, FFF_FinaleTierPresentation> TierPresentations;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale")
	FString CardWidgetFallbackPath = TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepFinaleCard");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale")
	FString ConfirmDialogFallbackPath = TEXT("/Game/FestaFiesta/UI/PrepHUD/Finale/WBP_FF_FinaleConfirmDialog");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText StageNoticeFormat = NSLOCTEXT("FestaFiestaFinale", "StageNotice", "{0}일차 아침 점검");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText OnceNoticeText = NSLOCTEXT("FestaFiestaFinale", "OnceNotice", "축제당 한 번만 구매할 수 있습니다. 구매 후에는 변경하거나 취소할 수 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText SelectTierText = NSLOCTEXT("FestaFiestaFinale", "SelectTier", "피날레 등급을 선택하세요");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText SelectedTierFormat = NSLOCTEXT("FestaFiestaFinale", "SelectedTier", "피날레 {0}");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText PurchasedTierFormat = NSLOCTEXT("FestaFiestaFinale", "PurchasedTier", "피날레 {0} · 구매 완료");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText AfterBalanceLabel = NSLOCTEXT("FestaFiestaFinale", "AfterBalance", "구매 후 잔액");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText CurrentBalanceLabel = NSLOCTEXT("FestaFiestaFinale", "CurrentBalance", "현재 잔액");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText MissingValueText = NSLOCTEXT("FestaFiestaFinale", "Missing", "—");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText PurchaseLabel = NSLOCTEXT("FestaFiestaFinale", "Purchase", "피날레 구매하기");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText PurchasedLabel = NSLOCTEXT("FestaFiestaFinale", "Purchased", "구매 완료");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText SelectedBadgeText = NSLOCTEXT("FestaFiestaFinale", "SelectedBadge", "✓ 선택됨");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText PurchasedBadgeText = NSLOCTEXT("FestaFiestaFinale", "PurchasedBadge", "✓ 구매 완료");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText UnavailableStageText = NSLOCTEXT("FestaFiestaFinale", "UnavailableStage", "피날레는 마지막 날 아침 점검에서 구매할 수 있습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText StageBadgeText = NSLOCTEXT("FestaFiestaFinale", "StageBadge", "아침 점검에서 구매");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText InsufficientBalanceText = NSLOCTEXT("FestaFiestaFinale", "InsufficientBalance", "잔액이 부족합니다. 다른 등급을 선택하세요.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText PurchasedNoticeText = NSLOCTEXT("FestaFiestaFinale", "PurchasedNotice", "피날레 구매가 완료되었습니다. 등급은 변경하거나 취소할 수 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText UnknownDataText = NSLOCTEXT("FestaFiestaFinale", "UnknownData", "피날레 정보를 확인할 수 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Finale|Text")
	FText DialogUnavailableText = NSLOCTEXT("FestaFiestaFinale", "DialogUnavailable", "확인창을 열 수 없어 구매를 진행하지 않았습니다.");

private:
	void BindViewModel();
	void UnbindViewModel();
	void RebuildCards();
	void RefreshAll();
	void RefreshConfirmDialog();
	bool OpenConfirmDialog();
	void ReleaseConfirmDialog(bool bClose);
	bool IsPurchaseReady(EFF_FinaleGrade Grade) const;
	FText GetBlockReason(EFF_FinaleGrade Grade) const;
	const FFF_FinaleTier* FindTier(EFF_FinaleGrade Grade) const;
	FFF_FinaleTierPresentation GetPresentation(EFF_FinaleGrade Grade) const;
	static void SetText(UTextBlock* Block, const FText& Value);

	UPROPERTY(Transient) TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;
	UPROPERTY(Transient) TArray<TObjectPtr<UFF_PrepFinaleCard>> CardWidgets;
	TArray<FFF_FinaleTier> Tiers;
	TWeakObjectPtr<UFF_FinaleConfirmDialog> PendingConfirm;
	EFF_FinaleGrade SelectedGrade = EFF_FinaleGrade::None;
	EFF_FinaleGrade ConfirmGrade = EFF_FinaleGrade::None;
	FText LastFailureText;
	bool bSubmitting = false;
};
