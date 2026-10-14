#pragma once

#include "CoreMinimal.h"
#include "Incident/FF_IncidentTypes.h"
#include "UI/Staff/FF_StaffPromotionTypes.h"
#include "FF_StaffManagementTypes.generated.h"

/** 원본 배정/예약/이동 상태를 합친 표시 상태. 알 수 없는 값을 대기로 대체하지 않는다. */
UENUM(BlueprintType)
enum class EFF_StaffDuty : uint8
{
	Unassigned, Ready, Moving, Working, Returning, Reserved, Unavailable,
	/** 준비 시간에 정상 마감된 시설의 재개장을 기다린다. 실제 출동 가능 인원은 아니다. */
	AwaitingOpening
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_StaffFacilityManagementView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FGuid FacilityId;
	UPROPERTY(BlueprintReadOnly) FName DefinitionId;
	UPROPERTY(BlueprintReadOnly) FText Label;
	UPROPERTY(BlueprintReadOnly) FText LocationLabel;
	UPROPERTY(BlueprintReadOnly) int32 Capacity = 0;
	UPROPERTY(BlueprintReadOnly) int32 Assigned = 0;
	UPROPERTY(BlueprintReadOnly) int32 Ready = 0;
	UPROPERTY(BlueprintReadOnly) int32 AwaitingOpening = 0;
	/** 출동 예약/이동/작업 인원. 복귀는 별도다. */
	UPROPERTY(BlueprintReadOnly) int32 Busy = 0;
	UPROPERTY(BlueprintReadOnly) int32 Returning = 0;
	UPROPERTY(BlueprintReadOnly) int32 Unavailable = 0;
	UPROPERTY(BlueprintReadOnly) bool bCanAssign = false;
	UPROPERTY(BlueprintReadOnly) bool bCanUnassign = false;
	UPROPERTY(BlueprintReadOnly) bool bRequiredStaff = false;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_StaffMemberManagementView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 StaffId = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FGuid FacilityId;
	UPROPERTY(BlueprintReadOnly) FText FacilityLabel;
	UPROPERTY(BlueprintReadOnly) FText DutyLabel;
	UPROPERTY(BlueprintReadOnly) EFF_StaffDuty Duty = EFF_StaffDuty::Unavailable;
	UPROPERTY(BlueprintReadOnly) int32 TrainingLevel = 1;
	UPROPERTY(BlueprintReadOnly) bool bIsBase = false;
	UPROPERTY(BlueprintReadOnly) bool bUnassigned = true;
	UPROPERTY(BlueprintReadOnly) EFF_StaffPromotionState PromotionState = EFF_StaffPromotionState::Unavailable;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_StaffIncidentManagementView
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FFF_IncidentHandle Handle;
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) FText Status;
	UPROPERTY(BlueprintReadOnly) EFF_IncidentResponseStage ResponseStage = EFF_IncidentResponseStage::None;
	UPROPERTY(BlueprintReadOnly) float RemainingWorkMinutes = 0.f;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_StaffManagementSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly) bool bCanManage = false;
	UPROPERTY(BlueprintReadOnly) bool bIncidentSourceAvailable = false;
	UPROPERTY(BlueprintReadOnly) int32 Total = 0;
	UPROPERTY(BlueprintReadOnly) int32 Assigned = 0;
	UPROPERTY(BlueprintReadOnly) int32 Unassigned = 0;
	UPROPERTY(BlueprintReadOnly) int32 Dispatchable = 0;
	/** 준비 시간의 정상 마감 시설에 배정된 개장 대기 인원. Dispatchable에 포함하지 않는다. */
	UPROPERTY(BlueprintReadOnly) int32 AwaitingOpening = 0;
	/** 복귀를 제외한 예약/이동/작업 인원. */
	UPROPERTY(BlueprintReadOnly) int32 Busy = 0;
	UPROPERTY(BlueprintReadOnly) int32 Returning = 0;
	/** 출발 지점 확인이 필요한 인원. 준비 시간의 정상 개장 대기는 제외한다. */
	UPROPERTY(BlueprintReadOnly) int32 Unavailable = 0;
	UPROPERTY(BlueprintReadOnly) int32 Capacity = 0;
	UPROPERTY(BlueprintReadOnly) int32 Vacancies = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FFF_StaffFacilityManagementView> Facilities;
	UPROPERTY(BlueprintReadOnly) TArray<FFF_StaffMemberManagementView> Members;
	UPROPERTY(BlueprintReadOnly) TArray<FFF_StaffIncidentManagementView> Incidents;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_StaffAssignmentResult
{
	GENERATED_BODY()
	/** 소유자 API가 실제 성공한 수. 사전 예상값이 아니다. */
	UPROPERTY(BlueprintReadOnly) int32 AssignedCount = 0;
	UPROPERTY(BlueprintReadOnly) FText Message;
};
