// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Economy/Rules/FF_FestivalEvaluationRules.h"
#include "FF_EconomyDisplayTextLibrary.generated.h"

// 경제 enum 의 한글 표시 문자열
// UMETA(DisplayName) 을 런타임에 읽으면 안 된다.

UCLASS()
class FESTAFIESTA_API UFF_EconomyDisplayTextLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	static FText GetSpendReasonText(EFF_SpendReason Reason);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	static FText GetEarnReasonText(EFF_EarnReason Reason);

	// 피날레 구매 실패 이유. UI 가 그대로 화면에 쓴다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	static FText GetFinalePurchaseResultText(EFF_FinalePurchaseResult Result);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	static FText GetFinaleGradeText(EFF_FinaleGrade Grade);

	// 프로그램 편성 실패 이유. UI 가 그대로 화면에 쓴다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	static FText GetProgramPurchaseResultText(EFF_ProgramPurchaseResult Result);

	// 입장료 변경 실패 이유. UI 가 그대로 화면에 쓴다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	static FText GetAdmissionFeeResultText(EFF_AdmissionFeeResult Result);

	// 홍보 등급 이름. 카드 제목에 쓴다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	static FText GetPromotionGradeText(EFF_PromotionGrade Grade);

	// 축제 평가 등급(S~D). 피날레 등급 문자열과 섞지 않는다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	static FText GetFestivalEvaluationGradeText(EFF_FestivalEvaluationGrade Grade);

	// 축제 평가 항목 이름(만족도·방문객·재무·화제성).
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	static FText GetFestivalEvaluationAxisText(EFF_FestivalEvaluationAxis Axis);
};
