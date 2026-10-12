#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Staff/FF_StaffManagementTypes.h"
#include "FF_StaffFacilityRowWidget.generated.h"

class UButton;
class UTextBlock;
DECLARE_DELEGATE_OneParam(FFF_OnStaffFacilityAction, FGuid);

/** 안내시설 한 곳의 배정 상태. 시설 ID로 요청하고 결과는 다음 원본 조회로 표시한다. */
UCLASS(Abstract)
class FESTAFIESTA_API UFF_StaffFacilityRowWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetFacility(const FFF_StaffFacilityManagementView& View, bool bLive);
	FFF_OnStaffFacilityAction OnAssignRequested;
	FFF_OnStaffFacilityAction OnUnassignRequested;
	FFF_OnStaffFacilityAction OnLocateRequested;
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	UFUNCTION() void HandleAssign();
	UFUNCTION() void HandleUnassign();
	UFUNCTION() void HandleLocate();
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FacilityName;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_AssignedCapacity;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_FacilityStatus;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Assign;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Unassign;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Locate;
private:
	FFF_StaffFacilityManagementView Current;
};
