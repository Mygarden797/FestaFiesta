#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_FacilityRevenueTableRow.generated.h"

/** 시설별 매출·수수료·레벨 배율·프로그램 참가비·미니게임 보상. 행 이름은 FacilityDefinitionId와 같아야 한다. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilityRevenueTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Revenue", meta = (DisplayName = "시설 이름", ToolTip = "시설을 쉽게 찾기 위한 표시 이름입니다. 실제 연결은 왼쪽 Row Name과 시설 Data Asset의 Facility Definition Id가 같은지로 결정됩니다."))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Revenue", meta = (ClampMin = "0", UIMin = "0", DisplayName = "1인 1회 매출 (Gold)", ToolTip = "방문객 1명이 시설을 1회 이용할 때의 일반 매출입니다. 0은 무료입니다. 실제 플레이어 수입은 이 값에 이용 인원, 수수료율, 레벨별 수익 배율과 시너지 배율을 곱한 후 반올림합니다. 시설 Data Asset의 개별 매출보다 이 표가 우선하며, 변경 값은 다음 이용 정산부터 기존 설치 시설에도 적용됩니다. 별도 유료 프로그램 참가비는 아래 프로그램별 참가비에서 설정합니다."))
	int32 RevenuePerUseGold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Revenue", meta = (DisplayName = "시설별 수수료율 사용", ToolTip = "켜면 아래 수수료율을 사용합니다. 축제 Definition의 Booth Commission Rate보다 이 표의 설정이 우선합니다. 끄면 기존 축제 수수료율을 사용합니다."))
	bool bOverrideCommissionRate = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Revenue", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", EditCondition = "bOverrideCommissionRate", DisplayName = "수수료율 (0~1)", ToolTip = "일반 매출 중 플레이어가 받는 비율입니다. 0.12는 12%, 1은 100%, 0은 수입 없음입니다. 시설별 수수료율 사용이 켜져 있으면 이 값이 축제 공통값보다 우선합니다. 유료 프로그램 참가비에는 적용하지 않습니다."))
	float CommissionRate = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Revenue", meta = (DisplayName = "레벨별 수익 배율", ToolTip = "왼쪽은 시설 레벨(1 이상), 오른쪽은 일반 매출의 배율(0 이상)입니다. 예: 1→1.0, 2→1.5, 3→2.25. 해당 레벨이 있으면 시설 Data Asset의 강화 수익 배율보다 이 표가 우선합니다. 빠진 레벨은 기존 배율을 사용합니다. 이미 강화된 시설도 다음 계산부터 반영됩니다. 시너지 배율은 별도로 곱하며 프로그램 참가비에는 적용하지 않습니다."))
	TMap<int32, float> RevenueMultiplierByLevel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Revenue", meta = (DisplayName = "프로그램별 1인 참가비 (Gold)", ToolTip = "왼쪽은 Program Id, 오른쪽은 해당 시설에서 열리는 유료 프로그램의 1인 참가비(0 이상)입니다. 해당 프로그램이 있으면 축제 Program Catalog의 참가비보다 이 표가 우선하며 0은 무료입니다. 없는 프로그램은 기존 참가비를 사용합니다. 실제 참가 인원에 곱해 전액 수입으로 받으며 수수료·강화·시너지 배율은 적용하지 않습니다."))
	TMap<FName, int32> ProgramFeesPerPerson;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Revenue", meta = (DisplayName = "시설별 미니게임 보상 사용", ToolTip = "켜면 이 시설에서 실행하는 미니게임의 최대 보상을 아래 값으로 정합니다. 미니게임 Data Asset과 축제 공통 보상보다 이 표가 우선합니다. 끄면 기존 미니게임 보상 설정을 사용합니다. 미니게임이 연결되지 않은 시설에 켜도 미니게임 기능이 추가되지는 않습니다."))
	bool bOverrideMinigameRewardGold = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facility Revenue", meta = (ClampMin = "0", UIMin = "0", EditCondition = "bOverrideMinigameRewardGold", DisplayName = "미니게임 최대 보상 (Gold)", ToolTip = "미니게임을 정상 완료하고 최대 보상 달성 점수를 채웠을 때 받는 Gold입니다. 낮은 점수는 기존 점수 비율에 따라 줄어들며 0은 보상 없음입니다. 시설별 미니게임 보상 사용이 켜져 있으면 이 표가 기존 보상값보다 우선합니다. 수수료·레벨·시너지 배율은 적용하지 않으며 종료 정산 시점의 값을 사용합니다."))
	int32 MinigameRewardAtMaxScoreGold = 0;

	// 에디터와 축제 시작 검증이 같은 규칙을 사용한다.
	bool IsRowValid(TArray<FText>& OutErrors) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
