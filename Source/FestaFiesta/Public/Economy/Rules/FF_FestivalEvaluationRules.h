#pragma once

#include "CoreMinimal.h"
#include "FF_FestivalEvaluationRules.generated.h"

// 축제 평가 등급. 피날레 구매 등급(EFF_FinaleGrade, 불꽃쇼 C~S)과 다른 것이다.
// [주의] 저장 기록에 들어갈 수 있다 — 값은 뒤에만 추가한다.
UENUM(BlueprintType)
enum class EFF_FestivalEvaluationGrade : uint8
{
	D = 0	UMETA(DisplayName = "D"),
	C = 1	UMETA(DisplayName = "C"),
	B = 2	UMETA(DisplayName = "B"),
	A = 3	UMETA(DisplayName = "A"),
	S = 4	UMETA(DisplayName = "S")
};

// 평가 항목.
UENUM(BlueprintType)
enum class EFF_FestivalEvaluationAxis : uint8
{
	Satisfaction = 0	UMETA(DisplayName = "만족도"),
	Visitors     = 1	UMETA(DisplayName = "방문객"),
	Finance      = 2	UMETA(DisplayName = "재무"),
	Buzz         = 3	UMETA(DisplayName = "화제성")
};

// 평가 입력. 월드 없이 만들 수 있게 값만 담는다.
USTRUCT(BlueprintType)
struct FFF_FestivalEvaluationInput
{
	GENERATED_BODY()

	// 날마다 기록한 만족도(0~100). 비어 있으면 중립 50 으로 본다.
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Evaluation")
	TArray<int32> DailySatisfaction;

	// 피날레 구매로 받는 만족도 보너스. 평균 뒤에 더한다.
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Evaluation")
	int32 FinaleSatisfactionBonus = 0;

	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Evaluation")
	int32 TotalAdmittedVisitors = 0;

	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Evaluation")
	int32 DailyVisitorGoal = 0;

	// 목표 달성률의 분모가 되는 날 수.
	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Evaluation")
	int32 DaysPlayed = 0;

	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Evaluation")
	int32 InitialGold = 0;

	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Evaluation")
	int32 FinalGold = 0;

	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Evaluation")
	float Buzz = 50.f;

	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Evaluation")
	float WeightSatisfaction = 0.4f;

	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Evaluation")
	float WeightVisitors = 0.25f;

	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Evaluation")
	float WeightFinance = 0.2f;

	UPROPERTY(BlueprintReadWrite, Category = "FestaFiesta|Evaluation")
	float WeightBuzz = 0.15f;
};

// 축제 평가 결과. 결과 화면이 그대로 그린다.
USTRUCT(BlueprintType)
struct FFF_FestivalEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Evaluation")
	EFF_FestivalEvaluationGrade Grade = EFF_FestivalEvaluationGrade::D;

	// 0~100. 네 항목의 가중 평균.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Evaluation")
	int32 TotalScore = 0;

	// 항목별 점수, 모두 0~100.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Evaluation")
	int32 SatisfactionScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Evaluation")
	int32 VisitorScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Evaluation")
	int32 FinanceScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Evaluation")
	int32 BuzzScore = 0;

	// 가장 잘한 항목과 가장 못한 항목. 점수가 같으면 Satisfaction → Visitors → Finance → Buzz 순으로 앞선 것.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Evaluation")
	EFF_FestivalEvaluationAxis StrongestAxis = EFF_FestivalEvaluationAxis::Satisfaction;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Evaluation")
	EFF_FestivalEvaluationAxis WeakestAxis = EFF_FestivalEvaluationAxis::Satisfaction;

	// 등급별 한 줄 평. 예: "모두가 다시 오고 싶어 하는 축제였다"
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Evaluation")
	FText Headline;

	// 가장 못한 항목에 대한 한 줄. S 등급이면 비어 있다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Evaluation")
	FText WeakestAxisComment;
};

// 축제 평가 순수 함수.
struct FESTAFIESTA_API FFF_FestivalEvaluationRules
{
	// 등급 경계: S >= 85, A >= 70, B >= 55, C >= 40, 그 밖은 D.
	static EFF_FestivalEvaluationGrade GradeForScore(int32 TotalScore);

	// 재무 점수: 원금 그대로면 50, 두 배면 100, 전부 잃으면 0. 초기 자금이 0 이하면 50.
	static int32 CalcFinanceScore(int32 InitialGold, int32 FinalGold);

	static FFF_FestivalEvaluation Evaluate(const FFF_FestivalEvaluationInput& Input);
};
