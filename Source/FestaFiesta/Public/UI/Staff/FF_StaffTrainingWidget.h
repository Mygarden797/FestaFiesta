// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Time/FF_FestivalStage.h"
#include "UI/Staff/FF_StaffManagementTypes.h"
#include "UI/Staff/FF_StaffPromotionTypes.h"
#include "FF_StaffTrainingWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UFF_FestivalViewModelSubsystem;
class UFF_StaffPromotionRowWidget;

/** 셋업·라이브 공통 숙련도 목록. 소유 패널이 활성화한 동안만 원본 변경을 구독한다. */
UCLASS(Abstract, meta = (DisplayName = "FF Staff Training"))
class FESTAFIESTA_API UFF_StaffTrainingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|StaffTraining")
	void ActivateRoster();
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|StaffTraining")
	void DeactivateRoster();
	void ConfigureStaffRowDefaults(TSubclassOf<UFF_StaffPromotionRowWidget> InRowClass);
	/** 소유 관리 패널이 읽은 동일 스냅샷으로 배정·업무 상태를 표시한다. */
	void ApplyManagementSnapshot(const FFF_StaffManagementSnapshot& Snapshot);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Rows")
	TSubclassOf<UFF_StaffPromotionRowWidget> StaffRowClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Rows", meta = (ClampMin = "0"))
	float StaffRowSpacing = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText CountFormat = NSLOCTEXT("FestaFiestaStaffTraining", "Count", "{Count}명");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText UnknownValueText = NSLOCTEXT("FestaFiestaStaffTraining", "UnknownValue", "—");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText RosterNoteText = NSLOCTEXT("FestaFiestaStaffTraining", "RosterNote", "직원별 현재 레벨과 오늘 승급 가능 여부를 확인하세요.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText RosterEmptyText = NSLOCTEXT("FestaFiestaStaffTraining", "RosterEmpty", "현재 보유한 스태프가 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText RosterUnavailableText = NSLOCTEXT("FestaFiestaStaffTraining", "RosterUnavailable", "스태프 명단을 확인할 수 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText UnassignedEmptyText = NSLOCTEXT("FestaFiestaStaffTraining", "UnassignedEmpty", "미배정 스태프가 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText PendingResultText = NSLOCTEXT("FestaFiestaStaffTraining", "PendingResult", "승급 요청을 확인하고 있습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text", meta = (ToolTip = "{Name}: 직원 이름, {Level}: 원본에서 확인한 현재 레벨"))
	FText PromotionSuccessFormat = NSLOCTEXT("FestaFiestaStaffTraining", "PromotionSuccess", "{Name} · Lv.{Level} 승급 완료");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text", meta = (ToolTip = "{Reason}: 현재 승급 불가 사유"))
	FText PromotionFailureFormat = NSLOCTEXT("FestaFiestaStaffTraining", "PromotionFailure", "승급하지 못했습니다. {Reason}");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText PromotionUnconfirmedText = NSLOCTEXT("FestaFiestaStaffTraining", "PromotionUnconfirmed", "승급 결과를 확인할 수 없습니다. 현재 명단을 다시 확인해 주세요.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText PromotionRetryText = NSLOCTEXT("FestaFiestaStaffTraining", "PromotionRetry", "현재 조건을 다시 확인해 주세요.");

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleRosterChanged();
	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage NewStage, int32 NewDay);
	UFUNCTION()
	void HandleAllStaffClicked();
	UFUNCTION()
	void HandleUnassignedStaffClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UButton> Button_AllStaff;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UButton> Button_UnassignedStaff;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_AllStaffFilter;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_UnassignedStaffFilter;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_TotalStaff;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_BaseStaff;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_HiredStaff;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_RosterNote;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_RosterEmpty;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_PromotionResult;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UVerticalBox> Box_StaffRoster;

private:
	void BindViewModel();
	void UnbindViewModel();
	void SyncRoster();
	void RefreshFilterLabels();
	void HandlePromoteRequested(int32 StaffId);
	void SetRowsPending(bool bPending);
	void SetResult(const FText& Result);
	FText FormatCount(int32 Count) const;
	FText GetFailureReason(int32 StaffId) const;

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UFF_StaffPromotionRowWidget>> RowsByStaffId;
	UPROPERTY(Transient)
	FFF_StaffManagementSnapshot ManagementSnapshot;
	bool bHasManagementSnapshot = false;
	bool bOnlyUnassigned = false;
	bool bRosterActive = false;
	bool bRequestInFlight = false;
	bool bRosterDirty = false;
	bool bSyncAfterStageEvent = false;
};
