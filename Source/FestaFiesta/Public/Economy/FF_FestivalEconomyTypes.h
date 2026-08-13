// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FestivalEconomyTypes.generated.h"

/**
 * 피날레 구매 등급.
 *
 * [주의]
 * 저장 슬롯에 들어갈 값이라 enum 순서를 바꾸면 예전 저장이 다른 등급으로 읽힌다. 
 * 새 등급은 뒤에만 추가한다.
 *
 * None : 예산이 부족해서 미구매 (Finale 를 제거하지 않고 최소 연출)
 */
UENUM(BlueprintType)
enum class EFF_FinaleGrade : uint8
{
	None = 0	UMETA(DisplayName = "미구매"),
	C    = 1	UMETA(DisplayName = "C"),
	B    = 2	UMETA(DisplayName = "B"),
	A    = 3	UMETA(DisplayName = "A"),
	S    = 4	UMETA(DisplayName = "S")
};

// 피날레 등급 1종의 가격과 보너스
USTRUCT(BlueprintType)
struct FFF_FinaleTier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale")
	EFF_FinaleGrade Grade = EFF_FinaleGrade::None;

	// 구매 비용 (Gold). 기획서 §10: C/B/A/S = 2,000 / 4,000 / 6,000 / 8,000
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale", meta = (ClampMin = "0"))
	int32 CostGold = 0;

	// ScoreSatisfaction 가산값 (0~100 척도). 기획서 §10: C/B/A/S = +5 / +12 / +20 / +30
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Finale", meta = (ClampMin = "0"))
	int32 SatisfactionBonus = 0;
};

// 홍보 등급 1종의 비용과 효과. Promotion Stage 에서 한 번 선택하고 3일 전체에 적용된다.
USTRUCT(BlueprintType)
struct FFF_PromotionTier
{
	GENERATED_BODY()

	// 홍보비 (Gold). 안 함 = 0
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Promotion", meta = (ClampMin = "0"))
	int32 CostGold = 0;

	// 일일 방문객 수에 곱한다. 1.0 이면 효과 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Promotion", meta = (ClampMin = "1.0"))
	float VisitorMultiplier = 1.f;
};

// 지출 원인
// [주의] 저장 슬롯에 들어갈 값이다 — 새 원인은 뒤에만 추가한다.
UENUM(BlueprintType)
enum class EFF_SpendReason : uint8
{
	FacilityInstall	= 0		UMETA(DisplayName = "시설 설치비"),
	ProgramPurchase	= 1		UMETA(DisplayName = "유료 프로그램 구매"),
	Promotion       = 2		UMETA(DisplayName = "홍보비"),
	VenueRent       = 3		UMETA(DisplayName = "장소 임대료"),
	StaffWages      = 4		UMETA(DisplayName = "Staff 임금"),
	StaffHire       = 5		UMETA(DisplayName = "추가 고용"),
	EventSupplies   = 6		UMETA(DisplayName = "Event 소모품"),
	Cleanup         = 7		UMETA(DisplayName = "뒷정리"),
	FinaleFireworks = 8		UMETA(DisplayName = "피날레 폭죽")
};

// 매출 원인
UENUM(BlueprintType)
enum class EFF_EarnReason : uint8
{
	AdmissionFee		= 0     UMETA(DisplayName = "NPC 입장료"),
	FacilityCommission  = 1		UMETA(DisplayName = "부스 수수료"),
	ProgramFee          = 2		UMETA(DisplayName = "유료 프로그램 체험비"),
	DailyGoalBonus      = 3		UMETA(DisplayName = "일일 목표 보너스"),
	MinigameReward      = 4		UMETA(DisplayName = "미니게임 보상")
};