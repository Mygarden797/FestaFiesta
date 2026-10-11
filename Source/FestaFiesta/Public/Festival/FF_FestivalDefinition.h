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
	int32 DailyVisitorGoal = 900;

	// 날마다 목표에 더하는 인원. Day N 목표 = DailyVisitorGoal + 이 값 × (N - 1). [밸런스]
	// 고정 900 은 홍보 없이도 채워져 방문객 점수가 늘 만점이었다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival", meta = (ClampMin = "0"))
	int32 DailyVisitorGoalGrowthPerDay = 100;

	int32 GetDailyVisitorGoalForDay(int32 Day) const
	{
		return DailyVisitorGoal > 0 ? DailyVisitorGoal + DailyVisitorGoalGrowthPerDay * FMath::Max(Day - 1, 0) : 0;
	}

	/**
	 * 하루 방문객(누적)의 기준. 홍보·입장료·화제성 배수를 곱한 값이 `GetExpectedVisitors()` 이고,
	 * Crowd 는 그 값으로 그날 도착 인원을 정한다. `AFF_SpawnActor::UnitCount` 는 경제가 없을 때의 폴백이다.
	 * Agent 풀은 누적이 아니라 동시 인원 상한이다. Day1 실측으로 누적 776명일 때 동시 607명, 1493명일 때 735명이었다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival", meta = (ClampMin = "0"))
	int32 BaseDailyVisitors = 1000;

	// Setup 완료 및 운영 진입에 필요한 시설. 빈 목록이면 조건 없음.
	// 기본은 SunsetMainStage 1개 + 메인안내소 1개(스태프 1명 이상 배정까지). 안내소는 사건 대응 스태프가
	// 출발하는 집이라, 설치만 하고 배정을 잊으면 사건에 아무도 출동하지 못한다 — 그래서 배정까지 요구한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Setup", meta = (TitleProperty = "FacilityDefinition"))
	TArray<FFF_RequiredFacility> RequiredSetupFacilities;

	// --- 초기 자원 --------------------------------------------------------

	// 시작 Gold. [기획서] §8.2.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Initial", meta = (ClampMin = "0"))
	int32 InitialGold = 10000;

	// 무페이 기본 Staff. 메인안내소 필수 배정 1명만 채운다 — 사건 대응 인력은 고용으로 산다.
	// 6명이면 고용을 안 해도 사건이 다 풀려 스태프 고용이 선택지가 되지 않았다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Initial", meta = (ClampMin = "0"))
	int32 InitialStaffCount = 1;

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

	// 시설 보너스 이전의 축제 기본 고용 한도. Info 시설의 StaffCapacity 합이 여기에 더해진다.
	// 최대 총원 = InitialStaffCount + MaxHiredStaffCount + 설치된 Info 시설 정원 합.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Staff", meta = (ClampMin = "0", ToolTip = "안내시설 보너스를 제외한 기본 추가 고용 한도입니다. Info 시설을 설치하면 해당 시설의 Staff Capacity만큼 한도가 더 늘어납니다. 설치 자체는 스태프를 고용하지 않습니다."))
	int32 MaxHiredStaffCount = 6;

	// --- 수익원 -----------------------------------------------------------

	// 두 수익원의 역할이 다르다 (economy_spec.md §2.6):
	//   입장료   → 손님 수만 따라오는 안정 수입. 방문객을 늘려서 키운다
	//   수수료   → Finale 를 사는 성장 자금. 부스를 잘 지어서 키운다
	//
	// [주의] 원래 입장료의 역할은 "고정비(임대료)를 덮는 것"이었다. 임대료를 없앤 뒤로
	//        덮을 것이 없어져 3일 7,500 이 통째로 순이익이 됐다 — 초기 자금과 함께
	//        다시 앉혀야 하는 값이다.

	// 1인당 입장료 기준값 (Gold). [밸런스]
	// 5 일 때 매출의 95% 이상이 입장료라 시설을 안 지어도 점수가 나왔다. 3 으로 낮춰 부스 매출이 4할쯤 되게 했다.
	// 플레이어가 매일 아침 바꿀 수 있다. 이 값은 축제 시작 시 입장료이자 "수요 배수 1.0" 인 지점이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0"))
	int32 AdmissionFeeGold = 3;

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
	// 유료 프로그램 외 '부스 매출' 중 플레이어 몫 (0.0~1.0). [밸런스]
	// 부스는 축제가 직영한다(1.0). 0.12 일 때 1회 이용이 4 Gold 남짓이라 부스를 지을 이유가 없었다.
	// DT_FacilityRevenues 의 행별 수수료 덮어쓰기는 꺼 두었다(Tools/DataTables/use_festival_commission_rate.py).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Revenue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BoothCommissionRate = 1.0f;
	
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

	// 평판 배수: 어제 만족도(0~100)가 다음날 방문객을 움직인다. 50(중립)에서 ×1.00, 0 에서 Min, 100 에서 Max.
	// Day1 은 어제가 없어 ×1.00 이다. 폭은 홍보 등급 폭을 넘기지 않는다(staff_event_design.md 5절). [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Reputation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReputationMinVisitorMultiplier = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Reputation", meta = (ClampMin = "1.0"))
	float ReputationMaxVisitorMultiplier = 1.25f;

	// 하루가 바뀔 때 중립(50)과의 거리를 이 비율만큼 줄인다. 화제는 식는다. [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Buzz", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BuzzDailyDecayToNeutral = 0.4f;

	// SNS 글 1건이 Buzz 를 움직이는 최대치. 실제 값은 × (화제 강도 / 100), 긍정 +, 부정 -. [밸런스]
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Buzz", meta = (ClampMin = "0.0"))
	float BuzzPerSnsPost = 0.3f;

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

	// 재무 만점 이익(Gold). 재무 점수 = 100 × (최종 - 시작) / 이 값. 본전이면 0점. [밸런스]
	// 3일 경제 시나리오(FestaFiesta.Scenario.Economy.ThreeDays)로 맞췄다: 최소 배치는 B, 시설 25개 + 인플루언서 + 피날레 S 는 S 에 닿는다.
	// 구역 13개 맵에서는 시설을 18개 이상 지은 판의 이익이 약 17,800 이라, 그 판들은 만점에 닿고 최소 배치(이익 11,000~13,000)만 덜 받게 둔다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Evaluation", meta = (ClampMin = "0"))
	int32 FinanceTargetProfitGold = 17000;

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
