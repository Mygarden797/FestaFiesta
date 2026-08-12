// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "FF_FestivalDefinition.generated.h"

class UDataTable;

/**
 * 축제 1종의 운영 규칙과 밸런스 값. 
 * 밸런스 값을 여기 한 곳에 모아 코드 상수로 박지 않는다.
 * [참고] data/metadata/spec/economy_spec.md §2.
 * 
 * 주석의 [기획서]는 팀 합의가 필요한 값, [밸런스]는 자유롭게 조정하는 값이다.
 *
 * 추후, 다양한 축제로 확장하기 때문에 싱글턴 대신 Primary Data Asset을 사용한다. (FestivalId 로 식별)
 */
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_FestivalDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFF_FestivalDefinition();

	// --- 식별 -------------------------------------------------------------

	// 축제 식별자 (예: "Busan_SeaFestival")
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival")
	FName FestivalId;

	// 운영 일수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival", meta = (ClampMin = "1"))
	int32 FestivalDayCount = 3;

	// 일일 목표 방문객 수. 달성하면 DailyGoalBonusGold 를 받는다. [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival", meta = (ClampMin = "0"))
	int32 DailyVisitorGoal = 500;

	// --- 초기 자원 --------------------------------------------------------

	// 시작 Gold. [기획서] §8.2.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Initial", meta = (ClampMin = "0"))
	int32 InitialGold = 10000;

	// 시작 Staff. [기획서] §8.2 "중형 Event 1개 해결 가능, 2개 동시는 어려운 수준"
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Initial", meta = (ClampMin = "0"))
	int32 InitialStaffCount = 6;

	// --- 고정비 -----------------------------------------------------------

	// 임대료 + 임금이 3일 합계 7,200 이다.
	// 초기 자금의 72% 가 가만히 있어도 나가기 때문에 "돈을 벌어야 하는 이유"가 된다.

	// 장소 임대료 (Gold/일). [기획서] §8.2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Recurring", meta = (ClampMin = "0"))
	int32 DailyVenueRentGold = 1500;

	// Staff 임금 (Gold/명·일). [기획서] §8.2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Recurring", meta = (ClampMin = "0"))
	int32 DailyStaffWageGold = 150;

	// 추가 고용 1명당 (Gold). [기획서] §8.2 — 단 "허용 후보"이고 기능 확정이 아니다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Recurring", meta = (ClampMin = "0"))
	int32 AdditionalHireCostGold = 600;

	// --- 수익원 -----------------------------------------------------------

	// 두 수익원의 역할이 다르다 (economy_spec.md §2.6):
	//   입장료   → 고정비를 덮는 안정 수입. 방문객을 늘려서 키운다
	//   수수료   → Finale 를 사는 성장 자금. 부스를 잘 지어서 키운다

	// 1인당 입장료 (Gold). [밸런스] 3일 7,500 으로 고정비 7,200 을 덮는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0"))
	int32 AdmissionFeeGold = 5;

	// [참고] 유료 프로그램은 플레이어가 Gold 를 내서 직접 산 것이므로 체험비를 전부 받는다.
	// 유료 프로그램 외 '부스 매출' 중 플레이어 몫 (0.0~1.0). [밸런스] 3일 9,000.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BoothCommissionRate = 0.12f;

	// 일일 목표(DailyVisitorGoal) 달성 보너스 (Gold). [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0"))
	int32 DailyGoalBonusGold = 300;

	// 미니게임 1회 최대 보상 (Gold). [밸런스] 플레이 성과에 따라 0~이 값
	// [기획서] 미니게임 성과가 자원에 영향을 준다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0"))
	int32 MinigameRewardMaxGold = 150;

	// --- 홍보 -------------------------------------------------------------

	// Promotion 에서 1회 선택해 3일 전체에 적용된다. [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Promotion")
	FFF_PromotionTier PromotionBasic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Promotion")
	FFF_PromotionTier PromotionFocused;

	// --- 뒷정리 -----------------------------------------------------------

	// TODO : 다음 날에 주는 영향 계수가 정해지면 추가한다.

	// 최소 뒷정리 (Gold). [기획서] §8.2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Cleanup", meta = (ClampMin = "0"))
	int32 CleanupCostMinimalGold = 300;

	// 표준 뒷정리 (Gold). [기획서] §8.2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Cleanup", meta = (ClampMin = "0"))
	int32 CleanupCostStandardGold = 700;

	// 완전 뒷정리 (Gold). [기획서] §8.2
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Cleanup", meta = (ClampMin = "0"))
	int32 CleanupCostFullGold = 1200;

	// --- 피날레 -----------------------------------------------------------

	// 등급별 가격·보너스. 생성자가 [기획서] §10 값으로 채운다.
	// 등급과 목표를 첫날부터 공개한다. Day3 MorningCheck 에서 구매한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Finale")
	TArray<FFF_FinaleTier> FinaleTiers;

	// --- 참조 표 ----------------------------------------------------------

	// 시설 설치비 표. 행 구조체는 FFF_FacilityCostRow 여야 한다.
	// RowName 은 "Cost.<대분류>.<소분류>" 형태다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Tables")
	TObjectPtr<UDataTable> FacilityCostTable;

	// !! Event 소모품 비용은 아직 없다 !!
	// [기획서] §8.2 에 100~300 이 있지만 UFF_FestivalEventSubsystem이 아직 미구현이다.
	// TODO : UFF_FestivalEventSubsystem 구현 후, Event 소모품 비용 추가 

	// --- 검증 -------------------------------------------------------------

	// 오류가 없으면 true.
	UFUNCTION(BlueprintPure, Category = "Festival")
	bool IsDefinitionValid(TArray<FText>& OutErrors) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
