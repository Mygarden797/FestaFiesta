// FF_FacilitySynergyTypes.h
// 역할: 조합 표의 행·미리보기 자료형과 작성 데이터의 유효성 규칙이다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_FacilitySynergyTypes.generated.h"

/** 한 행 = 수익을 받는 시설과 도움을 주는 시설의 한 방향 조합. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilitySynergyRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="조합", meta=(DisplayName="사용", ToolTip="끄면 이 행의 조합을 적용하지 않습니다."))
	bool bEnabled = true;
	/** Authored recipe title, independent of either facility's localized display name. IDs define the relation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="조합", meta=(DisplayName="조합 이름", ToolTip="디자이너가 정하는 조합 제목입니다. 시설 표시명과 별개이며 시설 연결은 아래 ID를 사용합니다."))
	FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="조합", meta=(DisplayName="수익을 받는 시설 ID", GetOptions="FestaFiesta.FF_FacilitySynergySettings.GetFacilityIdOptions", ToolTip="시설 정의의 FacilityDefinitionId를 선택합니다. 이 시설의 이용 수수료가 증가합니다."))
	FName TargetFacilityId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="조합", meta=(DisplayName="함께 놓을 시설 ID", GetOptions="FestaFiesta.FF_FacilitySynergySettings.GetFacilityIdOptions", ToolTip="가까이 설치할 시설의 FacilityDefinitionId를 선택합니다. 이 시설 자체의 수익은 변하지 않습니다."))
	FName PartnerFacilityId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="조합", meta=(DisplayName="최대 거리 (미터)", ClampMin="0.01", ClampMax="1000", Units="m", ToolTip="시설 중심 사이의 수평 거리입니다. 경계 거리까지 포함하며 존 경계를 넘어도 적용됩니다."))
	float MaxDistanceMeters = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="조합", meta=(DisplayName="추가 수익 (%)", ClampMin="0.01", ClampMax="1000", ToolTip="10을 입력하면 이용 수수료가 10% 증가합니다. 0.1이 아닙니다. 강화 배수에 곱해집니다."))
	float RevenueBonusPercent = 10.f;
	/** One-time profile discovery reward, independent of the recurring proximity revenue bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="조합", meta=(DisplayName="최초 발견 보상 (골드)", ClampMin="0", UIMin="0", ToolTip="이 조합을 발견한 뒤 도감의 보상 받기 버튼으로 한 번 수령할 골드입니다. 0이면 보상 없음이며, 이미 수령한 보상은 다시 지급하지 않습니다."))
	int32 DiscoveryRewardGold = 100;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="조합", meta=(DisplayName="중복 방지 그룹", ToolTip="같은 이름의 그룹에서는 가장 큰 보너스 하나만 적용합니다. 비우면 각 행이 독립적으로 더해집니다. 같은 행의 상대 시설 여러 개는 중첩되지 않습니다."))
	FName StackingGroup;

	bool IsUsable(FString* OutError = nullptr) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

/** 배치 안내는 실제 수익 평가와 같은 규칙으로 계산한다. 총합 차이는 해당 수익 시설 기준. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilitySynergyPreviewEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName RuleId;
	/** Original localized recipe title; UI supplies a readable fallback when empty, never a rule ID. */
	UPROPERTY(BlueprintReadOnly) FText DisplayName;
	UPROPERTY(BlueprintReadOnly) FName TargetFacilityId;
	UPROPERTY(BlueprintReadOnly) FName PartnerFacilityId;
	UPROPERTY(BlueprintReadOnly) FGuid TargetInstanceId;
	UPROPERTY(BlueprintReadOnly) FGuid PartnerInstanceId;
	UPROPERTY(BlueprintReadOnly) FVector TargetWorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FVector PartnerWorldLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float MaxDistanceMeters = 0.f;
	UPROPERTY(BlueprintReadOnly) float DistanceMeters = 0.f;
	UPROPERTY(BlueprintReadOnly) float ConfiguredBonusPercent = 0.f;
	UPROPERTY(BlueprintReadOnly) float AppliedBonusPercent = 0.f;
	UPROPERTY(BlueprintReadOnly) float CurrentTotalBonusPercent = 0.f;
	UPROPERTY(BlueprintReadOnly) float ProposedTotalBonusPercent = 0.f;
	/** 상대가 한 개라도 있는가. 설치하는 시설이 보조 시설이면 수익 시설의 존재를 뜻한다. */
	UPROPERTY(BlueprintReadOnly) bool bHasPartner = false;
	UPROPERTY(BlueprintReadOnly) bool bInRange = false;
	UPROPERTY(BlueprintReadOnly) bool bCandidateIsTarget = true;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilitySynergyPreview
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bRulesAvailable = false;
	UPROPERTY(BlueprintReadOnly) TArray<FFF_FacilitySynergyPreviewEntry> Entries;
};
