// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Economy/Rules/FF_StaffPool.h"
#include "UI/Staff/FF_StaffPromotionTypes.h"
#include "FF_StaffPromotionRowWidget.generated.h"

class UButton;
class USizeBox;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FFF_OnStaffPromoteRequested, int32 /* StaffId */);

/** 직원 한 명의 숙련도 행. 승급 판정·레벨은 원본을 그대로 표시한다. */
UCLASS(Abstract, meta = (DisplayName = "FF Staff Promotion Row"))
class FESTAFIESTA_API UFF_StaffPromotionRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFF_StaffPromotionRowWidget(const FObjectInitializer& ObjectInitializer);

	void SetStaffMember(const FFF_StaffMember& Member, EFF_StaffPromotionState State, bool bPending = false);
	void SetRequestPending(bool bPending);
	void SetAssignmentStatus(const FText& Facility, const FText& Duty);
	void SetIncidentEffectText(const FText& Effect);
	FText GetStaffDisplayName() const;
	FText GetPromotionStatusText(EFF_StaffPromotionState State) const;
	int32 GetTrainingLevel() const { return CurrentMember.TrainingLevel; }

	FFF_OnStaffPromoteRequested OnPromoteRequested;

	/** 관리 목록은 가능한 동작 하나 또는 짧은 불가 사유만 표시한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining")
	bool bCompactManagementRow = false;
	/** 라이브 표는 고용 구분을 툴팁으로 옮기고 직원마다 한 줄을 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining")
	bool bCompactLiveLayout = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText StaffNameFormat = NSLOCTEXT("FestaFiestaStaffTraining", "StaffName", "스태프 {Id}");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText BaseStaffText = NSLOCTEXT("FestaFiestaStaffTraining", "BaseStaff", "기본");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText HiredStaffText = NSLOCTEXT("FestaFiestaStaffTraining", "HiredStaff", "고용");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText StaffLevelFormat = NSLOCTEXT("FestaFiestaStaffTraining", "StaffLevel", "Lv.{Level}");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText PromoteLabelText = NSLOCTEXT("FestaFiestaStaffTraining", "PromoteLabel", "승급");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText PendingLabelText = NSLOCTEXT("FestaFiestaStaffTraining", "PendingLabel", "확인 중");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText PendingStatusText = NSLOCTEXT("FestaFiestaStaffTraining", "PendingStatus", "승급 요청을 확인하고 있습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText ReadyText = NSLOCTEXT("FestaFiestaStaffTraining", "Ready", "오늘 승급 가능");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText NotStartedText = NSLOCTEXT("FestaFiestaStaffTraining", "NotStarted", "축제를 시작한 뒤 승급할 수 있습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText NotFoundText = NSLOCTEXT("FestaFiestaStaffTraining", "NotFound", "현재 명단에서 직원을 찾을 수 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText MaxLevelText = NSLOCTEXT("FestaFiestaStaffTraining", "MaxLevel", "최대 레벨입니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText HiredTodayText = NSLOCTEXT("FestaFiestaStaffTraining", "HiredToday", "고용 다음 날부터 승급할 수 있습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText AlreadyPromotedTodayText = NSLOCTEXT("FestaFiestaStaffTraining", "AlreadyPromotedToday", "오늘 승급 완료 · 다음 날 다시 확인하세요.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|StaffTraining|Text")
	FText UnavailableText = NSLOCTEXT("FestaFiestaStaffTraining", "Unavailable", "지금은 승급할 수 없습니다.");

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandlePromoteClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_StaffName;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_StaffKind;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_StaffLevel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_AssignmentFacility;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_AssignmentStatus;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_PromotionStatus;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UTextBlock> Text_Promote;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<UButton> Button_Promote;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|StaffTraining")
	TObjectPtr<USizeBox> SizeBox_Promote;

private:
	void RefreshRow();
	void RefreshAssignmentStatus();
	FText GetCompactPromotionStatusText(EFF_StaffPromotionState State) const;
	FFF_StaffMember CurrentMember;
	FText AssignmentFacility;
	FText AssignmentDuty;
	FText IncidentEffectText;
	bool bHasAssignmentStatus = false;
	EFF_StaffPromotionState CurrentState = EFF_StaffPromotionState::Unavailable;
	bool bHasMember = false;
	bool bRequestPending = false;
	bool bRestoreButtonFocus = false;
};
