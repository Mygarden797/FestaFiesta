#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Time/FF_FestivalStage.h"
#include "UI/Staff/FF_StaffManagementTypes.h"
#include "FF_StaffManagementWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UFF_StaffFacilityRowWidget;
class UFF_StaffTrainingWidget;
class UFF_FestivalViewModelSubsystem;
DECLARE_DELEGATE_OneParam(FFF_OnStaffIncidentRequested, FFF_IncidentHandle);

/** 셋업·라이브 공통 관리 화면. 활성 상태에서만 원본을 구독하고 세부 이동 상태를 읽는다. */
UCLASS(Abstract)
class FESTAFIESTA_API UFF_StaffManagementWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void ActivateManagement(bool bLive);
	void DeactivateManagement();
	void ConfigureFacilityRowDefaults(TSubclassOf<UFF_StaffFacilityRowWidget> InRowClass);
	FSimpleDelegate OnRevealWorldRequested;
	FFF_OnStaffIncidentRequested OnIncidentRequested;
	UPROPERTY(EditDefaultsOnly, Category="Staff") TSubclassOf<UFF_StaffFacilityRowWidget> FacilityRowClass;
	UPROPERTY(EditDefaultsOnly, Category="Staff", meta=(ClampMin="0")) float FacilityRowSpacing = 8.f;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	UFUNCTION() void HandleChanged();
	UFUNCTION() void HandleCountChanged(int32 Value);
	UFUNCTION() void HandleStageChanged(EFF_FestivalStage Stage, int32 Day);
	UFUNCTION() void HandleFillVacancies();
	UFUNCTION() void HandleViewIncident();
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Phase;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Subtitle;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_TotalStaff;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_StaffBreakdown;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SecondLabel;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SecondValue;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_SecondNote;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ThirdLabel;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ThirdValue;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ThirdNote;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FourthLabel;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FourthValue;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FourthNote;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_FillVacancies;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UVerticalBox> Box_Facilities;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FacilitiesEmpty;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_AssignmentGuide;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_AssignmentResult;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UFF_StaffTrainingWidget> Widget_StaffTraining;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> Group_Incident;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_IncidentSummary;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_ViewIncident;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ManagementNote;
private:
	void Sync();
	void HandleAssign(FGuid FacilityId);
	void HandleUnassign(FGuid FacilityId);
	void HandleLocate(FGuid FacilityId);
	void SetResult(const FText& Result);
	UPROPERTY(Transient) TObjectPtr<UFF_FestivalViewModelSubsystem> ViewModel;
	UPROPERTY(Transient) TMap<FGuid, TObjectPtr<UFF_StaffFacilityRowWidget>> Rows;
	FFF_StaffManagementSnapshot Snapshot;
	bool bActive = false;
	bool bLiveMode = false;
	bool bChanging = false;
	bool bSyncing = false;
	float RefreshElapsed = 0.f;
};
