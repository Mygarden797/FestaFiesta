#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "FF_RequiredFacilityTypes.generated.h"

class UFFZSFacilityDefinition;

// Setup 완료 전에 설치해야 하는 시설 종류와 최소 개수. 자동 배치는 하지 않는다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_RequiredFacility
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Setup")
	TSoftObjectPtr<UFFZSFacilityDefinition> FacilityDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Setup", meta = (ClampMin = "1"))
	int32 MinimumCount = 1;

	// 설치한 이 종류 시설들에 합쳐서 배정돼 있어야 하는 스태프 최소 인원. 0이면 설치만 요구한다.
	// 자동 배정은 하지 않는다 — 플레이어가 시설 정보의 `+ 배정`으로 채워야 조건이 풀린다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Setup", meta = (ClampMin = "0", UIMin = "0",
		ToolTip = "설치한 이 시설들에 배정돼 있어야 하는 스태프 최소 인원입니다. 0이면 설치만 요구합니다. Staff Capacity가 1 이상인 Info 시설에만 설정하세요. 스태프는 자동 배정되지 않으며 플레이어가 시설 정보의 '+ 배정'으로 채워야 다음 단계로 진행할 수 있습니다."))
	int32 MinimumAssignedStaff = 0;
};

// 카탈로그 카드/카테고리 느낌표와 설치 안내가 함께 사용하는 한 행.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_RequiredFacilityRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	FName FacilityDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	EFFZSFacilityCategory Category = EFFZSFacilityCategory::Booth;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	int32 MinimumCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	int32 InstalledCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	int32 MissingCount = 0;
	// 0이면 이 행은 스태프 배정을 요구하지 않는다(설치만 본다).
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	int32 RequiredStaffCount = 0;
	// 설치로 인정된 이 종류 시설들에 배정된 스태프 합. 출동 중인 인원도 소속이므로 센다.
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	int32 AssignedStaffCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	int32 MissingStaffCount = 0;
	// false이면 Category도 미확정이다. 임의의 카테고리에 느낌표를 붙이지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	bool bDefinitionValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	FText Guidance;

	bool RequiresStaffAssignment() const { return RequiredStaffCount > 0; }
	bool IsSatisfied() const { return bDefinitionValid && MissingCount == 0 && MissingStaffCount == 0; }
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_RequiredFacilitiesStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	TArray<FFF_RequiredFacilityRow> Facilities;
	// 정의/월드가 없거나 저작 오류가 있으면 false. 빈 필수 목록은 유효하다.
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	bool bCanProceed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	FText BlockingReason;

	// 설치가 덜 됐거나 저작 오류가 있는 행이 있는지. 스태프 배정만 남은 경우는 false다.
	// 카탈로그의 `필수 · 1일차 시작 전 설치` 범례처럼 '설치'를 안내하는 곳에 쓴다.
	bool HasMissingInstallations() const
	{
		for (const FFF_RequiredFacilityRow& Row : Facilities)
		{
			if (!Row.bDefinitionValid || Row.MissingCount > 0) { return true; }
		}
		return false;
	}
};

// 필수 시설 판정에 넣는 스태프 배정 상태. 월드 없이 값으로 만들 수 있어 판정 규칙을 그대로 회귀 검증한다.
struct FFF_RequiredFacilityStaffing
{
	// 스태프 배정 경계(UFF_StaffFacilitySubsystem)를 읽었는지. false면 배정 조건을 충족으로 보지 않는다.
	bool bKnown = false;
	// 시설 인스턴스별 배정 인원(출동 중 포함).
	TMap<FGuid, int32> AssignedStaffByFacility;
	// 어느 시설에도 배정되지 않은 보유 스태프 수. 모르면 INDEX_NONE.
	int32 UnassignedStaffCount = INDEX_NONE;
};
