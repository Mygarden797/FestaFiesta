// 순이익처럼 부호에 뜻이 있는 금액의 표시 규칙. 일일·최종 결산이 같은 규칙을 쓴다.
#pragma once

#include "CoreMinimal.h"
#include "FF_SignedAmountStyle.generated.h"

class UTextBlock;

/**
 * 양수는 초록, 음수는 빨강, 0과 모르는 값은 기본색.
 * 색만으로 구분하지 않도록 양수에는 `+`, 음수에는 `-` 를 함께 붙인다 (AT-011).
 * 색은 각 위젯의 Class Defaults에서 고친다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SignedAmountStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amount")
	FLinearColor PositiveColor = FLinearColor::FromSRGBColor(FColor(25, 139, 91));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amount")
	FLinearColor NegativeColor = FLinearColor::FromSRGBColor(FColor(196, 50, 46));

	/** 0과 모르는 값(—)의 색. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Amount")
	FLinearColor NeutralColor = FLinearColor::FromSRGBColor(FColor(23, 66, 55));

	/** 양수만 `PositiveFormat`({Amount})으로 감싼다. 음수는 숫자의 `-` 를 그대로 쓴다. */
	static FText Format(int64 Amount, const FText& PositiveFormat);

	/** 금액 부호에 맞는 색을 칠한다. 금액을 모르면 기본색. */
	void ApplyColor(UTextBlock* Text, TOptional<int64> Amount) const;
};
