// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Festival/FF_RequiredFacilityTypes.h"
#include "FF_FestivalDefinition.generated.h"

class UDataTable;

/**
 * 축제 1종의 운영 규칙과 밸런스 값. 
 * 축제 공통 밸런스 값을 관리한다. 시설별 이용 매출은 FacilityRevenueTable에서 설정한다.
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

	/**
	 * 홍보 전 하루 방문객. 여기에 홍보 배수를 곱한 값이 `GetExpectedVisitors()` 다.
	 *
	 * 🔴 지금은 `AFF_SpawnActor::UnitCount` 와 같은 값을 저작해야 한다.
	 * crowd 가 `GetExpectedVisitors()` 를 읽게 되면 이 값이 출처가 된다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival", meta = (ClampMin = "0"))
	int32 BaseDailyVisitors = 500;

	// Setup 완료 및 운영 진입에 필요한 시설. 기본은 SunsetMainStage 1개. 빈 목록이면 조건 없음.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Setup", meta = (TitleProperty = "FacilityDefinition"))
	TArray<FFF_RequiredFacility> RequiredSetupFacilities;

	// --- 초기 자원 --------------------------------------------------------

	// 시작 Gold. [기획서] §8.2.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Initial", meta = (ClampMin = "0"))
	int32 InitialGold = 10000;

	// 무페이 기본 Staff. [기획서] §8.2 "중형 Event 1개 해결 가능, 2개 동시는 어려운 수준"
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Initial", meta = (ClampMin = "0"))
	int32 InitialStaffCount = 6;

	// --- 스태프 고용 -----------------------------------------------------------

	// 기본 Staff 는 무페이다. 추가 인원은 **1회 구매이고 축제가 끝날 때까지 남는다** —
	// 매일 다시 뽑는 일당 알바가 아니다.
	//
	// 영구라서 값어치가 남은 날짜에 비례한다. 고정가라 일찍 살수록 이득이고,
	// 그것을 규칙으로 받아들인 것이다(Day2·3 의 수요는 숙련도 강화가 담당한다).

	// 1명 고용 비용 (Gold, 1회). [밸런스]
	// 예전 일당 150 x 3일 = 450 이 등가 기준점이었다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Staff", meta = (ClampMin = "0"))
	int32 StaffHireCostGold = 450;

	// 축제 전체에서 고용할 수 있는 인원 상한 (명). [밸런스]
	// [주의] '하루 상한' 이 아니라 **총원 상한** 이다. 영구 고용이라 하루로 끊으면
	//        상한이 사실상 없어진다(사흘이면 18명).
	// 기본 6명 대비 +6 이면 최대 12명
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Staff", meta = (ClampMin = "0"))
	int32 MaxHiredStaffCount = 6;

	// --- 수익원 -----------------------------------------------------------

	// 두 수익원의 역할이 다르다 (economy_spec.md §2.6):
	//   입장료   → 손님 수만 따라오는 안정 수입. 방문객을 늘려서 키운다
	//   수수료   → Finale 를 사는 성장 자금. 부스를 잘 지어서 키운다
	//
	// [주의] 원래 입장료의 역할은 "고정비(임대료)를 덮는 것"이었다. 임대료를 없앤 뒤로
	//        덮을 것이 없어져 3일 7,500 이 통째로 순이익이 됐다 — 초기 자금과 함께
	//        다시 앉혀야 하는 값이다.

	// 1인당 입장료 기준값 (Gold). [밸런스] 3일 7,500
	// 플레이어가 매일 아침 바꿀 수 있다. 이 값은 축제 시작 시 입장료이자 "수요 배수 1.0" 인 지점이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0"))
	int32 AdmissionFeeGold = 5;

	// 플레이어가 고를 수 있는 입장료 범위 (Gold). [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0"))
	int32 AdmissionFeeMinGold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0"))
	int32 AdmissionFeeMaxGold = 15;

	// 입장료 수요 곡선: 배수 = 1 - Slope × (입장료 - 기준값), 아래 두 값으로 자른다. [밸런스]
	// 0.05 면 기준보다 5 Gold 비쌀 때 방문객 ×0.75, 10 Gold 비쌀 때 ×0.5.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0.0"))
	float AdmissionFeeDemandSlope = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0.0"))
	float AdmissionFeeMinVisitorMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "1.0"))
	float AdmissionFeeMaxVisitorMultiplier = 1.15f;

	// [참고] 유료 프로그램은 플레이어가 Gold 를 내서 직접 산 것이므로 체험비를 전부 받는다.
	// 유료 프로그램 외 '부스 매출' 중 플레이어 몫 (0.0~1.0). [밸런스] 3일 9,000.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BoothCommissionRate = 0.12f;
	
	// 시설별 수익표에 행이 없고 Data Asset의 개별 매출도 끈 기존 시설만 쓰는 기본값.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (DisplayName = "Default Revenue Per Use by Minor Token", ToolTip = "시설 수익표에 해당 시설 행이 없고 시설 Data Asset의 개별 매출 설정도 꺼져 있을 때만 적용합니다. 시설별 조정은 Facility Revenue Table의 수치를 바꾸세요. 금액은 1명당 1회 매출이며 실제 수입에는 Booth Commission Rate가 적용됩니다."))
	TMap<FName, int32> RevenuePerUseByMinorToken;

	// 일일 목표(DailyVisitorGoal) 달성 보너스 (Gold). [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0"))
	int32 DailyGoalBonusGold = 300;

	// 미니게임 1회 최대 보상 (Gold). [밸런스] 플레이 성과에 따라 0~이 값
	// [기획서] 미니게임 성과가 자원에 영향을 준다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0", ToolTip = "시설 수익표의 미니게임 보상 사용이 꺼져 있고 미니게임 Data Asset의 최대 보상도 -1일 때 쓰는 공통 기본값입니다. 시설별 보상 조정은 DT_FacilityRevenues의 미니게임 최대 보상을 사용하세요."))
	int32 MinigameRewardMaxGold = 150;

	// --- 홍보 -------------------------------------------------------------

	// Promotion 에서 1회 선택해 3일 전체에 적용된다. [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Promotion")
	FFF_PromotionTier PromotionBasic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Promotion")
	FFF_PromotionTier PromotionFocused;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Promotion")
	FFF_PromotionTier PromotionInfluencer;

	// 홍보를 안 샀을 때의 방문객 배수. 1.0 미만이라 "안 하면 손해" 다. [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Promotion", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float NoPromotionVisitorMultiplier = 0.85f;

	// --- 화제성(Buzz) -------------------------------------------------------

	// Buzz 는 0~100, 축제 시작 시 50(중립). 방문객 배수 = 1 + (Buzz - 50) / 50 × Swing. [밸런스]
	// 0.15 면 Buzz 0 에서 ×0.85, 100 에서 ×1.15.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Buzz", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BuzzVisitorSwing = 0.15f;

	// 하루가 바뀔 때 중립(50)과의 거리를 이 비율만큼 줄인다. 화제는 식는다. [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Buzz", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BuzzDailyDecayToNeutral = 0.25f;

	// SNS 글 1건이 Buzz 를 움직이는 최대치. 실제 값은 × (화제 강도 / 100), 긍정 +, 부정 -. [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Buzz", meta = (ClampMin = "0.0"))
	float BuzzPerSnsPost = 0.6f;

	// 유료 프로그램 1개가 열릴 때 오르는 Buzz. [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Buzz", meta = (ClampMin = "0.0"))
	float BuzzPerProgramOpened = 3.f;

	// --- 축제 평가(엔딩) ------------------------------------------------------

	// 종합 점수 = 가중 평균(만족도, 방문객 달성률, 재무, 화제성). 합이 1 이 아니어도 정규화한다. [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Evaluation", meta = (ClampMin = "0.0"))
	float EvaluationWeightSatisfaction = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Evaluation", meta = (ClampMin = "0.0"))
	float EvaluationWeightVisitors = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Evaluation", meta = (ClampMin = "0.0"))
	float EvaluationWeightFinance = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Evaluation", meta = (ClampMin = "0.0"))
	float EvaluationWeightBuzz = 0.15f;

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

	// --- 유료 프로그램 -----------------------------------------------------

	// 아침에 편성할 수 있는 프로그램 카드. 생성자가 v0 값으로 채운다.
	// 하루짜리다.** 편성은 매일 아침 다시 하고 하루 경계에서 비워진다 
	// 비용은 "정원을 다 채웠을 때 매출"의 55~62% 로 잡았다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Program")
	TArray<FFF_ProgramCard> ProgramCatalog;

	// --- 참조 표 ----------------------------------------------------------

	// 시설 설치비 표. 행 구조체는 FFF_FacilityCostRow 여야 한다.
	// 표준 RowName 은 "Cost.<대분류>.<소분류>.<고유이름>" 형태다.
	// 자동 조회는 표준 4-part → 구형 3-part → "Cost.<대분류>.Default" 순으로 호환 폴백한다.
	// Facility Spec의 InstallCostKey가 명시되면 그 예외 키가 자동 생성 키보다 우선한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Tables")
	TObjectPtr<UDataTable> FacilityCostTable;

	// 행 이름은 시설의 FacilityDefinitionId와 동일하다. 표에 없는 시설만 기존 설정을 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Tables", meta = (RequiredAssetDataTags = "RowStructure=/Script/FestaFiesta.FF_FacilityRevenueTableRow", ToolTip = "시설별 매출을 한곳에서 조정하는 표입니다. 기본 표는 /Game/FestaFiesta/Data/Economy/DT_FacilityRevenues입니다. Row Name이 Facility Definition Id와 같은 행의 1인 1회 매출을 사용합니다. 시설 Data Asset 설정보다 우선하며, 변경하면 이미 설치하거나 저장 후 복원한 시설도 다음 정산부터 적용됩니다."))
	TSoftObjectPtr<UDataTable> FacilityRevenueTable;

	UFUNCTION(BlueprintPure, Category = "Festival|Tables")
	UDataTable* GetFacilityRevenueTable() const;

	// --- 철거 ------------------------------------------------------------

	// 시설을 철거할 때 설치비 중 되돌려받는 비율 (0.0~1.0). [밸런스]
	// 1.0 이면 배치 실수의 비용이 0 이라 위치를 고민할 이유가 사라진다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Demolition", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FacilityDemolitionRefundRate = 0.5f;

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
