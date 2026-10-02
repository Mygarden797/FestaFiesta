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


// 피날레 구매 결과. UI 가 실패 이유를 그대로 화면에 쓸 수 있게 bool 이 아니다.
// [참고] 저장되지 않는 값이라 순서를 바꿔도 된다.
UENUM(BlueprintType)
enum class EFF_FinalePurchaseResult : uint8
{
	Success             UMETA(DisplayName = "구매 완료"),
	FestivalNotStarted  UMETA(DisplayName = "축제가 시작되지 않았다"),
	InvalidGrade        UMETA(DisplayName = "구매할 수 없는 등급"),
	UnknownGrade        UMETA(DisplayName = "저작되지 않은 등급"),
	AlreadyPurchased    UMETA(DisplayName = "이미 구매했다"),
	InsufficientGold    UMETA(DisplayName = "Gold 부족")
};

/**
 * 하루짜리 유료 프로그램 한 장.
 *
 * 아침(Setup·MorningCheck)에 Gold 를 걸고 편성하면, 그 프로그램을 좋아하는 손님이
 * 정원만큼 추가로 방문한다. 정원이 보장되므로 체험비는 확정값이다 — 이 카드의 값어치는
 * 체험비뿐 아니라 데려온 손님이 부스에서 쓰는 돈에도 있다.
 *
 * 대가는 Gold 와 기회비용뿐이다. **Staff 는 쓰지 않는다** — Staff 는 사건 대응 축이라
 * 편성에 끌려가면 안 된다.
 * 같은 Gold 로 부스를 지으면 남은 날 내내 벌지만, 프로그램은 그날 하루만 번다.
 *
 * DisplayName 을 FText 로 직접 갖는다. 카드 이름은 enum 이 아니라 데이터라
 * UMETA 런타임 조회 함정(UFF_EconomyDisplayTextLibrary 참고)을 처음부터 피한다.
 */
USTRUCT(BlueprintType)
struct FFF_ProgramCard
{
	GENERATED_BODY()

	// 카탈로그 안에서 유일하다. 원장에 매출 출처로도 들어간다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Program")
	FName ProgramId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Program")
	FText DisplayName;

	// 편성 시 1회 차감.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Program", meta = (ClampMin = "1"))
	int32 CostGold = 0;

	// 그날 받을 수 있는 최대 참가 인원.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Program", meta = (ClampMin = "1"))
	int32 Capacity = 0;

	// 시설 수익 표에 참가비가 없는 경우의 기본값. 부스 수수료와 달리 전액이며 0은 무료다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Program", meta = (ClampMin = "0", ToolTip = "시설 수익 데이터테이블의 프로그램 참가비가 우선합니다. 해당 시설·프로그램 항목이 없으면 이 값을 사용하며, 0은 무료입니다."))
	int32 FeePerPerson = 0;

	/**
	 * 프로그램이 열리는 시각. Live 경과분이다 (FFF_ProgramReservation 과 같은 기준).
	 * 편성하면 그대로 예약 슬롯으로 넘어가고, 거기서 세 가지가 갈린다:
	 *   - 잠금       `IsLockedAt`      — Start ~ End 사이에는 목적지를 다시 안 고른다
	 *   - 퇴장 금지   `IsExitBlockedAt` — 확정부터 End 까지 못 나간다(+ ApplyExitGrace 여유)
	 *   - 도착 판정   `ReturnTrip::Evaluate` — 다른 곳을 들렀다 Start 까지 올 수 있는가
	 *
	 * 카드마다 시간대가 정해져 있다.
	 * 플레이어가 시각을 끌어 옮기는 편집기가 아니라 "이 시간대의 이 프로그램"을 고르는 것이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Program", meta = (ClampMin = "0"))
	float StartMinutes = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Program", meta = (ClampMin = "0"))
	float EndMinutes = 0.f;

	/**
	 * 이 프로그램이 데려오는 손님의 생애주기 아키타입.
	 *
	 * 프로그램을 사면 정원만큼의 손님이 추가로 방문하고, 그 손님은 이 아키타입으로 태어난다.
	 * "마침 온 사람 중에서 취향이 맞는 쪽을 고른다"가 아니라 처음부터 좋아할 사람을 만든다 —
	 * 그래서 취향 점수 계산도 조합 최적화도 필요 없다.
	 *
	 * 비워 두면 아키타입 없는 손님이 온다. `DT_Archetype` 의 ArchetypeId 와 같은 값을 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Program")
	FName PreferredArchetypeId;

	/**
	 * 이 프로그램을 열 수 있는 시설 종류. 시설 이름 규칙의 `대분류_소분류` 토큰을 쓴다
	 * (예: `Booth_CraftExperience`, `Performance_ExperienceStage`).
	 *
	 * 요가를 화장실에서 여는 것을 막는다. 시설의 `대분류_소분류` 는 이름 규칙이 이미
	 * 강제하고 있어서 별도 태그 없이 이 목록만으로 판정된다.
	 *
	 * 비워 두면 아무 시설에서나 열린다 — 저작 실수로 비는 경우가 있어 그때는 경고를 남긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Program")
	TArray<FName> AllowedFacilityTypes;

	// 정원을 다 채웠을 때 그날 매출. 정원이 보장되므로 확정 매출이기도 하다.
	int32 CalcMaxRevenue() const
	{
		return static_cast<int32>(FMath::Min<int64>(MAX_int32,
			static_cast<int64>(FMath::Max(0, Capacity)) * FMath::Max(0, FeePerPerson)));
	}

	bool IsValid() const
	{
		return !ProgramId.IsNone()
			&& CostGold > 0
			&& Capacity > 0
			&& FeePerPerson >= 0
			&& EndMinutes > StartMinutes;
	}
};

// 오늘 편성한 프로그램 하나. 카드와 그 카드가 만든 예약 슬롯을 묶는다.
// 예약(FFF_ProgramReservation)은 ProgramId 를 모르므로, 정산할 때 체험비를 찾으려면
// 이 짝이 있어야 한다.
USTRUCT(BlueprintType)
struct FFF_ScheduledProgram
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Program")
	FName ProgramId;

	// UFF_ProgramReservationSubsystem::RegisterProgram 이 돌려준 핸들.
	UPROPERTY(BlueprintReadOnly, Category = "Program")
	FGuid ReservationId;

	// 체험비를 이미 걷었는가. 프로그램 시작 시각에 켜지고 하루 경계에서 편성과 함께 사라진다.
	// 시작 정산과 Closing 안전망이 같은 카드를 두 번 걷지 않게 막는 수단이다.
	UPROPERTY(BlueprintReadOnly, Category = "Program")
	bool bFeeSettled = false;

	// 편성 당시 시설 종류. 시설 액터가 사라져도 최신 수익 표에서 해당 참가비를 찾는다.
	UPROPERTY(BlueprintReadOnly, Category = "Program")
	FName FacilityDefinitionId;

	// 편성한 시설. "같은 카드는 하루 한 번" 이 시설 단위라서 필요하다.
	UPROPERTY(BlueprintReadOnly, Category = "Program")
	FGuid FacilityInstanceId;
};

// 프로그램 편성 결과. UI 가 실패 이유를 그대로 화면에 쓸 수 있게 bool 이 아니다.
// [참고] 저장되지 않는 값이라 순서를 바꿔도 된다.
UENUM(BlueprintType)
enum class EFF_ProgramPurchaseResult : uint8
{
	Success             UMETA(DisplayName = "편성 완료"),
	FestivalNotStarted  UMETA(DisplayName = "축제가 시작되지 않았다"),
	UnknownProgram      UMETA(DisplayName = "없는 프로그램"),
	AlreadyScheduled    UMETA(DisplayName = "오늘 이미 편성했다"),
	InsufficientGold    UMETA(DisplayName = "Gold 부족"),
	FacilityTypeMismatch UMETA(DisplayName = "이 시설에서는 열 수 없다")
};

// 홍보 등급. 1회 선택하고 3일 전체에 적용된다.
// UI 쪽 `EFF_PromotionChoice` 와 값이 1:1 이다.
// [주의] 저장 슬롯에 들어가는 값이다. 이름만 바꾸고 숫자는 유지한다 — Basic 이 "지역 SNS", Focused 가 "전국 홍보" 다.
UENUM(BlueprintType)
enum class EFF_PromotionGrade : uint8
{
	None       = 0	UMETA(DisplayName = "홍보 없음"),
	Basic      = 1	UMETA(DisplayName = "지역 SNS"),
	Focused    = 2	UMETA(DisplayName = "전국 홍보"),
	Influencer = 3	UMETA(DisplayName = "인플루언서")
};

// 입장료 변경 결과. 화면이 실패 사유를 그대로 쓴다.
UENUM(BlueprintType)
enum class EFF_AdmissionFeeResult : uint8
{
	Success             UMETA(DisplayName = "변경 완료"),
	FestivalNotStarted  UMETA(DisplayName = "축제가 시작되지 않았다"),
	OutOfRange          UMETA(DisplayName = "허용 범위 밖이다"),
	LockedDuringLive    UMETA(DisplayName = "운영 중에는 바꿀 수 없다")
};

/**
 * 하루 방문객 예보. 준비 화면의 미리보기와 결산 설명이 쓴다.
 *
 * 방문객 = 기본 × 홍보 배수 × 입장료 배수 × Buzz 배수 (내림).
 * [주의] 실제 입장은 군중 동시 인원 상한(Agent 풀)에 막힐 수 있다. 이 값은 "올 사람" 이지 "들어온 사람" 이 아니다.
 */
USTRUCT(BlueprintType)
struct FFF_VisitorForecast
{
	GENERATED_BODY()

	// 최종 예상 방문객.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	int32 ExpectedVisitors = 0;

	// 입장료가 기준값이었다면 올 방문객. ExpectedVisitors 와의 차이가 "입장료 때문에 덜(더) 온 사람" 이다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	int32 VisitorsAtReferenceFee = 0;

	// ExpectedVisitors - VisitorsAtReferenceFee. 비싸면 음수, 싸면 양수.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	int32 VisitorDeltaFromFee = 0;

	// 예상 입장료 수입 = ExpectedVisitors × AdmissionFeeGold.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	int32 ExpectedAdmissionRevenue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	int32 AdmissionFeeGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	float PromotionMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	float AdmissionFeeMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	float BuzzMultiplier = 1.f;
};

// 홍보 구매 결과. 화면이 실패 사유를 그대로 쓴다.
UENUM(BlueprintType)
enum class EFF_PromotionPurchaseResult : uint8
{
	Success             UMETA(DisplayName = "구매 완료"),
	FestivalNotStarted  UMETA(DisplayName = "축제가 시작되지 않았다"),
	AlreadyPurchased    UMETA(DisplayName = "이미 구매했다"),
	InvalidGrade        UMETA(DisplayName = "없는 등급이다"),
	InsufficientGold    UMETA(DisplayName = "Gold 부족")
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

	// 구매 순간 축제 화제성(Buzz)에 더하는 값. 0~100 척도.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Promotion", meta = (ClampMin = "0"))
	int32 StartingBuzz = 0;
};

// 지출 원인

// [주의] 저장 슬롯에 들어갈 값이다 — 새 원인은 뒤에만 추가한다.
UENUM(BlueprintType)
enum class EFF_SpendReason : uint8
{
	FacilityInstall	= 0		UMETA(DisplayName = "시설 설치비"),
	ProgramPurchase	= 1		UMETA(DisplayName = "유료 프로그램 구매"),
	Promotion       = 2		UMETA(DisplayName = "홍보비"),
	StaffHire       = 3		UMETA(DisplayName = "Staff 고용"),
	EventSupplies   = 4		UMETA(DisplayName = "Event 소모품"),
	Cleanup         = 5		UMETA(DisplayName = "뒷정리"),
	FinaleFireworks = 6		UMETA(DisplayName = "피날레 폭죽"),
	FacilityUpgrade = 7		UMETA(DisplayName = "시설 강화비")
};

// 매출 원인
UENUM(BlueprintType)
enum class EFF_EarnReason : uint8
{
	AdmissionFee		= 0     UMETA(DisplayName = "NPC 입장료"),
	FacilityCommission  = 1		UMETA(DisplayName = "부스 수수료"),
	ProgramFee          = 2		UMETA(DisplayName = "유료 프로그램 체험비"),
	DailyGoalBonus      = 3		UMETA(DisplayName = "일일 목표 보너스"),
	MinigameReward      = 4		UMETA(DisplayName = "미니게임 보상"),
	IncidentReward		= 5		UMETA(DisplayName = "사건 보상")
};

// 원인별 결산 표의 한 줄. UI 가 받아서 그대로 그린다.
USTRUCT(BlueprintType)
struct FFF_LedgerRow
{
	GENERATED_BODY()
	
	// 화면에 그리는 한글 이름. enum 의 UMETA(DisplayName) 이다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	FText DisplayName;

	// 줄을 식별하는 키. 짧은 enum 이름이다 (StaffHire 등).
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	FName ReasonId;

	// Gold. 0 인 줄은 애초에 만들지 않으므로 표에 실린 값은 항상 0 이 아니다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	int32 Amount = 0;
	
};

// 부스별 매출 표의 한 줄.
USTRUCT(BlueprintType)
struct FFF_FacilityRevenueRow
{
	GENERATED_BODY()

	// 시설 식별자. FacilityId 오름차순으로 정렬된 상태로 온다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	FName FacilityId;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	int32 Amount = 0;
};

// 설치 시설 한 곳(Zone 인스턴스 GUID)이 벌어들인 매출. 시설 정보 패널용 읽기 모델이다.
// 부스별 표(FFF_FacilityRevenueRow)가 종류별 합계인 것과 달리 "1번 음식 부스 vs 2번 음식 부스"를 구분한다.
// UFF_FestivalProgressionSubsystem::GetFacilityInstanceRevenue 가 돌려준다.
USTRUCT(BlueprintType)
struct FFF_FacilityInstanceRevenue
{
	GENERATED_BODY()

	// 오늘 매출(Gold). 하루 경계에서 0 으로 돌아간다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	int32 TodayGold = 0;

	// 축제 누적 매출(Gold). 줄어들지 않고 새 축제에서만 0 이 된다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	int32 TotalGold = 0;

	// 오늘 결제 건수. 부스 수수료는 파티 1회 이용이, 프로그램 체험비는 프로그램 1회가 한 건이다.
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	int32 TodayPaymentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Economy")
	int32 TotalPaymentCount = 0;
};
