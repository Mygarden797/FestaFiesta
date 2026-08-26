// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Economy/FF_FestivalEconomyTypes.h"
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
};
