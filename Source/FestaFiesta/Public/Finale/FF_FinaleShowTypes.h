// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Engine/DataAsset.h"
#include "FF_FinaleShowTypes.generated.h"

class UNiagaraSystem;

// 피날레 불꽃 연출 관련 타입.
//
// 등급은 경제 쪽이 소유한다. 여기서는 그 등급을 받아 연출 수치로 바꾸는 일만 한다.
// 등급을 정하거나 구매를 처리하지 않는다.

DECLARE_LOG_CATEGORY_EXTERN(LogFestaFiestaFinale, Log, All);

// 한 등급이 만들어내는 연출의 규모.
//
// 수치를 코드가 아니라 데이터 애셋에 두는 이유는, 이런 값은 굴려보면서 정해지기 때문이다.
// 발수 30 이 화려한지 초라한지는 화면을 봐야 알 수 있고, 그때마다 빌드를 다시 하고 싶지 않다.
USTRUCT(BlueprintType)
struct FFF_FinaleGradeSpec
{
	GENERATED_BODY()

	// 연출이 끝날 때까지 쏘는 총 발수.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0"))
	int32 BurstCount = 12;

	// 연출 전체 길이(초). 발수를 이 시간에 나눠 쏜다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.5"))
	float DurationSeconds = 20.f;

	// 한 번에 동시에 터지는 수. 늘리면 하늘이 꽉 찬다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "1", ClampMax = "16"))
	int32 SimultaneousShells = 1;

	// 발사 지점을 중심으로 이 반경 안에 흩어서 쏜다. 0 이면 한 점에서만 터진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.0"))
	float SpreadRadius = 3000.f;

	// 터지는 높이 범위. 등급이 높을수록 위아래로 넓게 퍼뜨린다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.0"))
	float BurstHeightMin = 15000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.0"))
	float BurstHeightMax = 25000.f;

	// 폭발 크기 배수. 등급이 높을수록 크게 터진다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.1"))
	float BurstScale = 1.f;

	// 올라가는 궤적의 길이 배수. 크기와 따로 두면 "작지만 높이 올라가는" 연출도 만들 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale", meta = (ClampMin = "0.1"))
	float TrailLength = 1.f;

	// 이 등급에서 쓸 색. 비어 있으면 이펙트 자체 색을 그대로 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale")
	TArray<FLinearColor> Colors;

	// 터질 때 재생할 이펙트. 지정하지 않으면 발사하지 않고 로그만 남긴다.
	// 에셋이 아직 없어도 나머지 시스템이 돌아가야 해서 필수로 두지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Finale")
	TSoftObjectPtr<UNiagaraSystem> BurstEffect;

	// 발사 간격. 총 발수와 지속 시간에서 계산한다.
	// 발수가 0 이거나 하나뿐이면 간격이 의미 없으므로 전체 길이를 돌려준다.
	float GetIntervalSeconds() const
	{
		const int32 Volleys = FMath::Max(1, GetVolleyCount());
		if (Volleys <= 1)
		{
			return FMath::Max(0.5f, DurationSeconds);
		}
		return FMath::Max(0.05f, DurationSeconds / static_cast<float>(Volleys));
	}

	// 몇 번에 나눠 쏘는가. 동시 발사 수가 크면 횟수가 줄어든다.
	int32 GetVolleyCount() const
	{
		const int32 PerVolley = FMath::Max(1, SimultaneousShells);
		return FMath::DivideAndRoundUp(FMath::Max(0, BurstCount), PerVolley);
	}

	bool IsPlayable() const
	{
		return BurstCount > 0 && DurationSeconds > 0.f;
	}
};

// Niagara 이펙트에 넘기는 User 파라미터 이름.
//
// 이펙트 쪽에서 같은 이름으로 User 파라미터를 만들어두면 값이 자동으로 들어간다.
// 이름이 다르면 조용히 무시된다 — 크래시도 경고도 없으므로 오타를 조심해야 한다.
// 그래서 문자열을 여기 한 곳에만 두고 코드가 직접 쓰지 않게 한다.
namespace FFF_FinaleParams
{
	inline const FName BurstColor(TEXT("BurstColor"));
	inline const FName BurstScale(TEXT("BurstScale"));
	inline const FName TrailLength(TEXT("TrailLength"));
}

// 등급별 연출 수치를 담는 데이터 애셋.
//
// 지정하지 않아도 동작한다. 없으면 코드에 박힌 기본값을 쓴다.
// 레벨과 에셋이 아직 없는 단계라, 아무것도 안 붙어 있어도 굴러가는 게 중요하다.
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_FinaleShowData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale")
	FFF_FinaleGradeSpec NoneGrade;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale")
	FFF_FinaleGradeSpec GradeC;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale")
	FFF_FinaleGradeSpec GradeB;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale")
	FFF_FinaleGradeSpec GradeA;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale")
	FFF_FinaleGradeSpec GradeS;

	const FFF_FinaleGradeSpec& GetSpec(EFF_FinaleGrade Grade) const;
};

// 데이터 애셋이 없을 때 쓰는 기본 수치.
//
// 미구매 등급도 최소 연출을 남긴다. 아무것도 안 터지면 "축제가 그냥 끝났다" 가 되는데,
// 그건 예산을 못 쓴 결과이지 축제가 없었던 건 아니다.
namespace FFF_FinaleDefaults
{
	FESTAFIESTA_API FFF_FinaleGradeSpec MakeSpec(EFF_FinaleGrade Grade);
}
