// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "NPC/Party/FF_NeedState.h"
#include "NPC/Insights/FF_SnsTypes.h"
#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"

#include "FF_FestivalViewModelSubsystem.generated.h"

class UFF_FestivalStageSubsystem;
class UFF_FestivalProgressionSubsystem;
class UFF_StageEconomyBridgeSubsystem;
class UFF_SatisfactionMetricsSubsystem;
class UFF_CrowdInsightsSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnIntMetricChanged, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFF_OnStageViewChanged, EFF_FestivalStage, NewStage, int32, NewDay);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnWeatherViewChanged, EFF_WeatherType, NewWeather);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnBoolViewChanged, bool, bNewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnFinaleGradeViewChanged, EFF_FinaleGrade, NewGrade);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSnsSocialPostChanged, FFF_LiveSocialPost, NewPost);

// 편성 목록이 바뀌었다. 원본과 같이 인자를 싣지 않는다 — 무엇이 바뀌었는지는 다시 읽는다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnProgramScheduleViewChanged);

/**
 * 홍보 등급.
 *
 * 🔴 **이것은 임시 UI 쪽 enum 이다.** 경제 시스템에는 홍보 등급을 가리키는 타입이 없다 —
 * `UFF_FestivalDefinition` 이 `PromotionBasic`·`PromotionFocused` 두 필드를 따로 들고 있을 뿐이다.
 * 차감은 `RequestPurchasePromotion` 이 `TrySpend(.., Promotion)` 으로 하고 있다.
 *
 * 🔴 **다만 `VisitorMultiplier`(1.08·1.18) 를 소비하는 곳이 아직 0곳이다** — 사면 돈만 나가고
 * 방문객은 안 늘어난다. 곱할 대상인 "오늘 예상 방문객"이 경제에 없기 때문이다.
 *
 * 김시리님이 `EFF_PromotionGrade` 와 구매 API 를 만들면 **이 enum 을 지우고 그것으로 바꾼다.**
 * 그때 고칠 곳은 이 파일과 홍보 패널뿐이다 — 위젯 트리는 그대로다.
 */
UENUM(BlueprintType)
enum class EFF_PromotionChoice : uint8
{
	/** 안 함. 데이터가 아니라 미구매 상태다 (0 Gold · 배수 1.0). */
	None		UMETA(DisplayName = "안 함"),

	Basic		UMETA(DisplayName = "기본"),
	Focused		UMETA(DisplayName = "집중")
};

/** 등급이 바뀌었다. 지금은 UI 안에서만 도는 값이다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnPromotionViewChanged, EFF_PromotionChoice, NewChoice);

/**
 * 홍보 등급 한 줄. 화면이 `UFF_FestivalDefinition` 을 직접 읽지 않게 한다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PromotionOptionView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Promotion")
	EFF_PromotionChoice Choice = EFF_PromotionChoice::None;

	/** 홍보비. `안 함` 은 0 이고 그 0 은 확정 정보다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Promotion")
	int32 CostGold = 0;

	/** 일일 방문객에 곱한다. `안 함` 은 1.0. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Promotion")
	float VisitorMultiplier = 1.f;
};

/**
 * 프로그램을 열 수 있는 시설 한 곳.
 *
 * === 왜 UI 가 시설 종류를 직접 판정하지 않나 ===
 * 카드의 `AllowedFacilityTypes` 와 시설 이름 규칙을 맞춰 보는 판정은 이미
 * `FFF_ProgramScheduleRules::IsFacilityTypeAllowed` 가 소유한다. UI 가 같은 비교를 다시 쓰면
 * 규칙이 두 곳에 생긴다 — ViewModel 은 그 함수를 **불러서** 목록만 만든다 (UI 규칙 2).
 */
/**
 * 시설의 설치 진행 상태. ZoneSystem 의 `EFFZSFacilityState` 를 UI 쪽으로 옮긴 것이다.
 *
 * === 🔴 왜 그대로 안 쓰고 옮기나 ===
 * 플러그인 enum 을 화면 코드가 직접 쓰면, 저쪽에서 항목을 더하거나 이름을 바꿀 때마다
 * UI 가 같이 깨진다. 여기서 한 번 옮겨 두면 고칠 자리가 이 파일 하나다.
 *
 * `Unknown` 이 있는 것도 그래서다 — 모르는 값이 오면 `준비 중` 으로 **지어내지 않는다.**
 */
UENUM(BlueprintType)
enum class EFF_LiveFacilityState : uint8
{
	Unknown		UMETA(DisplayName = "모름"),
	Planned		UMETA(DisplayName = "계획"),
	Validated	UMETA(DisplayName = "검증됨"),
	Active		UMETA(DisplayName = "운영 중"),
	Closed		UMETA(DisplayName = "폐쇄"),
	MovePending	UMETA(DisplayName = "이동 대기"),
	Removed		UMETA(DisplayName = "철거됨")
};

/** 시설 대기열의 상태. ZoneSystem 의 `EFFZSQueueState` 를 UI 쪽으로 옮긴 것이다. */
UENUM(BlueprintType)
enum class EFF_LiveFacilityQueueState : uint8
{
	Unknown		UMETA(DisplayName = "모름"),
	Open		UMETA(DisplayName = "받는 중"),
	Full		UMETA(DisplayName = "정원 참"),
	Draining	UMETA(DisplayName = "비우는 중"),
	Closed		UMETA(DisplayName = "닫힘"),
	Blocked		UMETA(DisplayName = "막힘")
};

/**
 * 라이브 화면이 보는 시설 하나. **화면은 이 값을 그대로 그리고 계산을 하지 않는다.**
 *
 * ⚠️ 시설에 사람이 읽을 이름(`FText`)을 가진 필드가 아직 없다. `DisplayName` 은
 * 정의 id 의 고유 토큰에서 만든 임시 값이다 — `FFF_PrepFacilityOption` 과 같은 사정이다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LiveFacilityView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Facility")
	FGuid FacilityInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Facility")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Facility")
	FName ZoneId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Facility")
	EFF_LiveFacilityState State = EFF_LiveFacilityState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Facility")
	EFF_LiveFacilityQueueState QueueState = EFF_LiveFacilityQueueState::Unknown;

	/** 줄 서 있는 인원. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Facility")
	int32 WaitingCount = 0;

	/** 지금 이용 중인 인원. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Facility")
	int32 ActiveServiceCount = 0;

	/** 완료된 손님의 평균 대기 시간(초). 아직 아무도 못 끝냈으면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Facility")
	int32 AverageWaitSeconds = INDEX_NONE;

	/**
	 * **지금 줄을 서면** 얼마나 기다리는가(초). 모르면 `INDEX_NONE`.
	 *
	 * 위의 `AverageWaitSeconds` 와 다른 값이다 —
	 *   평균 대기   **이미 끝낸 사람**이 실제로 기다린 시간 (과거)
	 *   예상 대기   지금 서면 얼마나 (미래)
	 * 둘을 한 칸에 섞으면 "줄이 짧아졌는데 숫자가 안 준다" 가 된다.
	 *
	 * 🔴 `UnreachableWaitSeconds`(86400) 는 **대기 시간이 아니라 `닿을 수 없음` 표식**이다.
	 *    그대로 그리면 `1440분` 이 뜬다. 여기서 걸러 `INDEX_NONE` 으로 바꾼다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Facility")
	int32 ExpectedWaitSeconds = INDEX_NONE;

	/** 분당 처리 인원. 모르면 음수. 대기가 긴 이유가 `느림` 인지 `사람이 많음` 인지 가른다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Facility")
	float ServiceRatePerMinute = -1.f;

	/** 자리가 겹치거나 존을 벗어나 다시 놓아야 한다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Facility")
	bool bNeedsRelocation = false;
};

/**
 * 관객 욕구 한 종류의 현재 상태.
 *
 * 원본은 `UFF_NpcNeedMetricsSubsystem` 이다. 그 타입을 UI 헤더로 끌고 오지 않고 옮긴다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LiveNeedView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Crowd")
	EFF_NeedType NeedType = EFF_NeedType::Hunger;

	/** 이 욕구가 미해소인 관객 비율. 0~100. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Crowd")
	float UnmetRatioPercent = 0.f;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PrepFacilityOption
{
	GENERATED_BODY()

	/** `TryScheduleProgram` 에 그대로 넘기는 핸들. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Program")
	FGuid FacilityInstanceId;

	/**
	 * 화면에 쓸 이름. 지금은 시설 식별자의 **고유 토큰**이다.
	 *
	 * ⚠️ 시설에 사람이 읽을 이름(`FText`)을 가진 필드가 아직 없다 — `UFFZSFacilityDefinition` 에는
	 * 썸네일만 있다. 진짜 표시 이름이 생기면 여기만 바꾼다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Program")
	FText DisplayName;

	/** 어느 구역인지. 같은 종류 부스가 여러 개일 때 구분에 쓴다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|ViewModel|Program")
	FName ZoneId;
};

/**
 * UI용 Read Model.
 *
 * === 이 클래스의 존재 이유 ===
 * UI 위젯은 게임 시스템을 직접 참조하지 않고 **이것만** 본다.
 * 각 시스템이 바뀌어도 위젯을 고치지 않기 위해서다.
 *
 * === 현재 상태: 일부만 진짜 값 (2026-08-14 기준) ===
 * 아직 더미인 지표가 남아 있다. 실제 소유자의 구현이 생기면 이 클래스 안쪽만 교체하고
 * 위젯은 그대로 둔다. 그게 이 계층을 두는 이유다.
 *
 * | 값                     | 실제 소유자      | 현재        |
 * |------------------------|------------------|-------------|
 * | Stage / Day            | 강민서           | **진짜 값** |
 * | 남은 시간              | 강민서           | **진짜 값** |
 * | Gold                   | 김시리           | **진짜 값** (`OnGoldChanged` 구독) |
 * | SimSatisfaction        | 김시리           | **진짜 값** (`OnSimSatisfactionChanged` 구독. 대기·밀도·취향매칭·Need 충족도 **4항**) |
 * | └ 분해값 4종           | 김시리           | **진짜 값** — SimSatisfaction과 같은 1초 주기에 같이 갱신된다 |
 * | ScoreSatisfaction      | 김시리           | **진짜 값** — `Clamp(SimSatisfaction + FinaleBonus, 0, 100)`. `OnSimSatisfactionChanged`·`OnFinaleGradeChanged` 둘 다 바뀔 때 재계산한다. UI 문구의 "연출 보정" 항은 정의·계수가 없어 이번엔 포함하지 않았다(`FF_LivePanelReport.h` 참고, 백종태 소유 문구라 별도 결정 필요) |
 * | Safety                 | 이정원           | 더미 (소유자 API 없음) |
 * | TotalRevenue           | 김시리           | 더미 — **소유자 API는 완료, 구독 연결만 남음** |
 * | TodayRevenue           | 김시리           | 더미 — **소유자 API는 완료, 구독 연결만 남음** |
 *
 * `GetTotalRevenue()`/`GetTodayRevenue()`와 `OnTotalRevenueChanged`/`OnTodayRevenueChanged`가
 * `UFF_FestivalProgressionSubsystem`에 이미 있다. Gold와 같은 모양이므로 같은 Bind + Sync로 붙인다.
 * 원인별·부스별 분해는 `GetDayLedger()`로 읽을 수 있다 (2026-08-18 확인). `FFF_DayLedger`는
 * 원인별 지출·원인별 매출·부스별 매출(`GetRevenueByFacility()`)·철거 환급을 따로 쌓고 하루 경계에서
 * `Reset()`된다. Blueprint 노출은 없고 C++ const 참조라 Report 화면이 직접 읽는다.
 *
 * Staff도 소유자 API가 생겼다 — `GetBaseStaffCount()`/`GetHiredStaffCount()`/`GetTotalStaffCount()`.
 * 무페이 기본 팀 + 고용 인원 구성이고, 고용은 1회 구매라 축제가 끝날 때까지 남는다.
 * 거절할 수 없는 지출은 없다 — 잔액이 모자라면 그 지출이 거부되고 화면에서는 버튼이 막힌다.
 * 상세는 리포 루트 `UI_API.md`.
 *
 * === 값 규격 (2026-08-06 결정) ===
 * - Satisfaction / Safety : int32 0~100, 높을수록 좋음.
 * - Gold / Revenue : int32 금액. 0~100 상한을 적용하지 않는다.
 *
 * **Gold와 Revenue는 다른 값이다.** 헷갈리면 화면에 틀린 숫자가 뜬다.
 * - `Gold`         : 지금 쓸 수 있는 **잔액**. 지출로 줄어든다. 초기 10,000, 축제 종료 시 초기화.
 * - `TotalRevenue` : 축제 전체 **누적 매출**. 줄어들지 않는다. 상단 바 표시용.
 * - `TodayRevenue` : 오늘 하루 매출. Report에서 "오늘 얼마 벌었나"에 쓴다.
 *
 * 대략 `Gold ≒ 초기자금 + TotalRevenue - 누적지출`이지만, **UI가 이 식을 계산하지 않는다.**
 * 셋 다 소유자에게서 그대로 받는다. 근거: docs/design/game-overview.md 5절
 *
 * Buzz는 여기 없다. 기획서에서 Utility AI의 점수 항이며 dev-schedule 기준 9월 투입 대상이다.
 * 좋고 나쁨 구간은 아직 미정이라 UI에서 색을 칠하지 않는다. 상세는 리포 루트 UI_API.md 참고.
 *
 * === 갱신 주기 (2026-08-06 결정) ===
 * 지표는 **일정 틱마다** 바뀐다. 소스가 매 틱 흔들려도 위젯을 매 틱 갱신하면 안 되므로
 * 여기서 `PollInterval`마다 샘플링하고 **정수값이 실제로 바뀔 때만** 델리게이트를 쏜다.
 * 지표를 int32로 둔 것이 여기서 이득이 된다. 위젯은 이 처리를 몰라도 된다.
 *
 * === 시간을 자체 계산하지 않는다 ===
 * 시계 배율이 출처마다 달라 아직 미확정이다(30초 / 60초 / 코드 180초×4).
 * UI는 남은 시간을 스스로 계산하지 않고 Stage 시스템이 주는 값을 그대로 표시만 한다.
 * 근거: docs/memory/time-contract-draft-2026-08-06.md
 */
UCLASS()
class FESTAFIESTA_API UFF_FestivalViewModelSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- 지표 ---
	// 규칙: 값마다 "현재값 Getter + 변경 델리게이트"를 쌍으로 제공한다.
	// 델리게이트만 있으면 위젯이 늦게 생성될 때 초기값을 놓친다.

	/** 0~100. 대기·밀도·취향 매칭에서 나오는 순수 시뮬레이션 값. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetSimSatisfaction() const { return SimSatisfaction; }

	/**
	 * SimSatisfaction 분해값 4종. 왜 지금 만족도가 그 숫자인지 보여주기 위한 것이다 —
	 * 최종 숫자 하나만 보면 플레이어가 뭘 고쳐야 할지 모른다. 넷 다 0~100이고
	 * SimSatisfaction과 같은 주기(1초)에 같이 갱신된다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetSimSatisfactionWaitScore() const { return SimSatisfactionWaitScore; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetSimSatisfactionDensityScore() const { return SimSatisfactionDensityScore; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetSimSatisfactionPreferenceScore() const { return SimSatisfactionPreferenceScore; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetSimSatisfactionNeedFulfillmentScore() const { return SimSatisfactionNeedFulfillmentScore; }

	/**
	 * 0~100. `Clamp(SimSatisfaction + FinaleBonus, 0, 100)`. 등급 산출용 게임 점수.
	 * "연출 보정" 항은 아직 정의가 없어 포함하지 않는다(위 클래스 주석 표 참고).
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetScoreSatisfaction() const { return ScoreSatisfaction; }

	/** 0~100. 높을수록 안전. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetSafety() const { return Safety; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnSimSatisfactionChanged;

	/** 분해값 4종의 변경 델리게이트. 넷 다 OnSimSatisfactionChanged와 같은 순간에 함께 쏜다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnSimSatisfactionWaitScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnSimSatisfactionDensityScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnSimSatisfactionPreferenceScoreChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnSimSatisfactionNeedFulfillmentScoreChanged;

	/**
	 * SNS 라이브 피드. `UFF_CrowdInsightsSubsystem`의 `FFF_SnsPostRow` 링버퍼를
	 * `FFF_LiveSocialPost`(`FF_LivePanelSNS.h`, 백종태 소유)로 변환해 그대로 중계한다 —
	 * Insights는 자기 도메인 타입만 알고, 표현 타입은 여기서만 만든다. 최신 글이 [0].
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|SNS")
	const TArray<FFF_LiveSocialPost>& GetRecentSnsPosts() const { return RecentSnsPosts; }

	/** 새 글이 하나 생길 때마다. 위젯이 늦게 열려도 GetRecentSnsPosts()로 지금까지 쌓인 걸 한 번에 읽을 수 있다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|SNS")
	FFF_OnSnsSocialPostChanged OnSnsPostAdded;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnScoreSatisfactionChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnSafetyChanged;

	// --- 경제 ---
	// Gold(잔액)와 Revenue(매출)는 다른 값이다. 클래스 주석의 구분표를 먼저 읽는다.

	/** 지금 쓸 수 있는 잔액. 지출로 줄어든다. 상단 바·Finale 구매 가능 판정에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetGold() const { return Gold; }

	/** 축제 전체 누적 매출. 줄어들지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetTotalRevenue() const { return TotalRevenue; }

	/** 오늘 하루 매출. Report의 "오늘 얼마 벌었나"에 쓴다. UI가 누적값에서 빼서 만들지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetTodayRevenue() const { return TodayRevenue; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Economy")
	FFF_OnIntMetricChanged OnGoldChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Economy")
	FFF_OnIntMetricChanged OnTotalRevenueChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Economy")
	FFF_OnIntMetricChanged OnTodayRevenueChanged;

	// --- 스태프 (2026-08-26 추가 — U4 준비 단계 HUD) ---
	//
	// 무페이 기본 팀 + 이번 축제에 고용한 인원이다. 고용은 1회 구매이고 축제 끝까지 남는다.
	// **축제 시작 전에는 `INDEX_NONE`(모름)이다.** 0 을 띄우면 "스태프가 없다"는 틀린 사실이 된다.

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetBaseStaffCount() const { return BaseStaffCount; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetHiredStaffCount() const { return HiredStaffCount; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetTotalStaffCount() const { return TotalStaffCount; }

	/** 전체 인원이 바뀔 때. 기본·고용 내역도 이때 함께 갱신된다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Staff")
	FFF_OnIntMetricChanged OnTotalStaffCountChanged;

	// --- 고용 (2026-08-26 추가 — U5 인력 패널) ---
	//
	// 판정의 소유자는 경제 시스템이다. **UI 가 `Gold / 고용비` 를 직접 계산하지 않는다** —
	// 상한 규칙이 바뀌면 UI 의 계산식이 조용히 틀리게 된다.
	// 값이 없으면 전부 `INDEX_NONE`(모름)이고 화면은 그 칸을 숨긴다.

	/**
	 * 지금 추가로 고용할 수 있는 최대 인원. **잔액과 축제 총원 상한을 둘 다 반영한 값**이다.
	 * 스텝퍼 상한과 버튼 비활성 판정에 쓴다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetAffordableHireCount() const;

	/**
	 * 축제 총원 상한까지 남은 자리. 잔액은 보지 않는다.
	 *
	 * `GetAffordableHireCount()` 와 **둘 다 필요한 이유**는 막힌 이유를 구분하기
	 * 위해서다 — `상한을 다 썼다` 와 `돈이 없다` 는 플레이어가 할 행동이 다르다 (AT-011).
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetRemainingHireCap() const;

	/** Staff 1명 고용 비용(1회). 지출 표와 견적에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetStaffHireCostGold() const;

	/** 축제 총원 고용 상한. 화면에 `최대 N명` 을 쓰기 위한 값이다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Staff")
	int32 GetMaxHiredStaffCount() const;

	// --- 환경: 날씨 (2026-08-26 추가 — U5 점검 패널) ---
	//
	// 🔴 이 API 는 원래부터 있었다. `UI_API.md` 의 `필요한데 아직 없는 것` 표에 `날씨 없음` 으로
	// 남아 있던 것이 낡은 기록이었다 (같은 문서 `환경(날씨·야간)` 절에 이미 적혀 있었다).
	// **날씨 선택은 테스트 기능이 아니라 준비 단계의 플레이어 결정**이다.

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Environment")
	EFF_WeatherType GetSelectedWeather() const;

	/**
	 * 지금 날씨를 바꿀 수 있는지. Stage 시스템의 `IsReconfigureAllowed()` 를 그대로 중계한다
	 * (셋업·홍보·개장 준비에서만 true).
	 *
	 * !! 이 값으로 요청을 막지 않는다 !! 버튼 활성 표시에만 쓴다 — 최종 판정은 저쪽이 소유한다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Environment")
	bool CanChangeWeather() const;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Environment")
	FFF_OnWeatherViewChanged OnWeatherChanged;

	// --- 일일 목표·입장료 (2026-08-26 추가 — U5 점검 패널) ---
	//
	// 축제 정의의 밸런스 값이다. 모르면 `INDEX_NONE` 이고 화면은 그 칸을 숨긴다.

	/** 하루 목표 방문객. 달성하면 `GetDailyGoalBonusGold()` 를 받는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetDailyVisitorGoal() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetDailyGoalBonusGold() const;

	/** 1인당 입장료. 고정값이라 화면에 슬라이더를 두지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetAdmissionFeeGold() const;

	// --- 피날레 (점검 패널 요약 + U5 피날레 구매 패널) ---
	//
	// **축제당 1회이고 환불이 없다.** 그래서 이 표면은 다른 값들과 성격이 다르다 —
	// 구매 화면은 누르기 전에 확인창을 거쳐야 하고, 한 번 사면 모든 등급이 잠긴다.
	//
	// 등급·가격·보너스를 UI 가 하드코딩하지 않는다. 밸런스 값이라 데이터에서 온다.

	/** 구매한 등급. 안 샀으면 `None`. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	EFF_FinaleGrade GetPurchasedFinaleGrade() const;

	/** 구매한 등급의 만족도 가산값. 안 샀으면 0 — 여기 0 은 확정 정보다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetFinaleSatisfactionBonus() const;

	/**
	 * 살 수 있는 등급 전체와 각 가격·보너스. 축제가 시작되지 않았으면 빈 배열이다.
	 *
	 * 데이터에 `None` 등급은 들어가지 않는다 (`UFF_FestivalDefinition::Validate` 가 에러로 잡는다).
	 * 화면의 `미구매` 카드는 이 배열이 아니라 `GetPurchasedFinaleGrade()` 로 그린다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	TArray<FFF_FinaleTier> GetFinaleTiers() const;

	/**
	 * 지금 피날레를 살 수 있는 단계인지 (마지막 날 개장 준비).
	 *
	 * !! 이 값으로 요청을 막지 않는다 !! 카드 잠금 표시에만 쓴다 — 최종 판정은 경제가 소유한다.
	 * **이미 구매했어도 false 가 된다.** 막힌 이유를 화면이 구분하려면
	 * `GetPurchasedFinaleGrade()` 를 함께 봐야 한다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	bool CanPurchaseFinaleNow() const;

	/**
	 * 등급이 바뀔 때. 실질적으로 `None → 구매한 등급` 한 번만 울린다.
	 * 점검 패널의 요약 줄과 피날레 패널의 카드 잠금이 이걸로 갱신된다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Economy")
	FFF_OnFinaleGradeViewChanged OnPurchasedFinaleGradeChanged;

	// --- 유료 프로그램 (2026-08-27 추가 — U5 프로그램 패널) ---
	//
	// 하루짜리 카드다. 아침에 Gold 를 걸고 편성하면 정원만큼 손님이 추가로 온다.
	// [주의] 인원은 걸지 않는다 — Staff 는 사건 대응 축이라 프로그램과 분리돼 있다.

	/** 편성할 수 있는 카드 전체. 축제가 시작되지 않았으면 빈 배열이다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Program")
	TArray<FFF_ProgramCard> GetProgramCatalog() const;

	/** 오늘 이미 편성한 카드 id. 하루 경계에서 비워진다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Program")
	TArray<FName> GetScheduledProgramIds() const;

	/**
	 * 이 카드를 편성할 수 있는지. 상태를 바꾸지 않으므로 카드 잠금·사유 표시에 그대로 쓴다.
	 *
	 * !! 이 값으로 요청을 막지 않는다 !! 최종 판정은 경제가 소유한다.
	 * **시설 종류는 보지 않는다** — 그건 `GetEligibleFacilitiesForProgram()` 이 답한다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Program")
	EFF_ProgramPurchaseResult CanScheduleProgram(FName ProgramId) const;

	/**
	 * 지금이 편성할 수 있는 단계인지 (셋업 · 개장 준비).
	 *
	 * 🔴 **시설 배치(`IsReconfigureAllowed`)와 허용 단계가 다르다.** 홍보 프리뷰에서는 부스를
	 * 옮길 수는 있어도 편성은 못 한다. 두 규칙을 같은 것으로 묶지 않는다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Program")
	bool CanScheduleProgramNow() const;

	/**
	 * 이 카드를 열 수 있는 시설 목록. 없으면 빈 배열이고 화면은 `부스를 먼저 지으세요` 를 띄운다.
	 *
	 * 편성에는 **카드뿐 아니라 장소도** 필요하다 — `TryScheduleProgram` 이 시설 핸들을 받는다.
	 * 요가를 화장실에서 여는 경로를 막기 위해서다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Program")
	TArray<FFF_PrepFacilityOption> GetEligibleFacilitiesForProgram(FName ProgramId) const;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Program")
	FFF_OnProgramScheduleViewChanged OnScheduledProgramsChanged;

	// --- 관객 수 (2026-08-29 추가) ---

	/**
	 * 지금 축제장 안에 있는 관객 수. 못 읽으면 `INDEX_NONE`.
	 *
	 * === 🔴 이것은 **동시 인원**이지 누적 방문객이 아니다 ===
	 * 원본은 `UFF_CrowdSimulationSubsystem::GetActiveAgentCount()` 로, 스폰될 때 늘고
	 * 사라질 때 주는 값이다. **`DailyVisitorGoal`(오늘 500명) 의 분자로 쓰면 안 된다** —
	 * 그 목표는 하루 동안 들어온 총 인원이라, 동시 80명을 넣으면 진행률이 16% 에서
	 * 영원히 안 올라간다. 그럴듯해 보여서 눈으로는 안 걸린다.
	 *
	 * 누적 방문객 API 는 아직 없다 (`UI_API.md` 요청 목록). 그래서 일일 목표 진행은
	 * 여전히 `모름` 이고, 이 값은 **상태 바의 `관객` 칸에만** 쓴다 — 지금 얼마나 붐비는지는
	 * 그 자체로 볼 값이다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Crowd")
	int32 GetConcurrentVisitorCount() const;

	/**
	 * 오늘 예상 방문객. 못 읽었으면 `INDEX_NONE`.
	 *
	 * 경제가 `floor(기본 방문객 × 홍보 배수)` 로 계산한다. 홍보를 사면 이 값이 오른다.
	 *
	 * 🔴 **하루 단일 예상치다.** 3일 합계도, 범위도, 최대 동시 인원도 아니다.
	 * 그리고 **실제 입장 인원과 다르다** — 스폰은 아직 `AFF_SpawnActor::UnitCount` 를
	 * 쓰기 때문에 홍보 배수가 안 걸린다 (2026-09-02 김시리 확인).
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Crowd")
	int32 GetExpectedVisitors() const { return ExpectedVisitors; }

	/** 예상 방문객이 바뀔 때. 홍보를 사면 온다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Crowd")
	FFF_OnIntMetricChanged OnExpectedVisitorsChanged;

	/**
	 * 관객 욕구 5종의 미해소율. 순서는 `EFF_NeedType` 과 같다.
	 *
	 * 🔴 **`GetNeedSampleCount()` 를 함께 봐야 한다.** 표본이 없으면 이 함수는 전부 `0` 을
	 * 돌려주는데, 그것은 `아무도 불편하지 않다` 가 아니라 **아직 못 읽었다** 는 뜻이다.
	 * 0% 로 그리면 화면이 틀린 사실을 말한다 (2026-08-28 김시리 주의).
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Crowd")
	TArray<FFF_LiveNeedView> GetNeedMetrics() const;

	/** 직전 집계에 들어간 관객 수. **`0` 이면 데이터 없음**이고 `0%` 와 다르다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Crowd")
	int32 GetNeedSampleCount() const;

	// --- 시설 현황 (2026-08-29 추가 — U6 라이브 시설 패널) ---
	//
	// 원본은 ZoneSystem 의 `FFFZSFacilityReadModel` 이다. 그 타입을 UI 헤더로 끌고 오지 않고
	// 여기서 UI 용 값으로 옮긴다 — 플러그인 enum 이 바뀌어도 화면 코드는 안 건드리게 한다.

	/**
	 * 지금 놓여 있는 시설 전부. **철거된 것은 빠진다.**
	 *
	 * 🔴 빈 배열은 `시설이 없다` 와 `아직 못 읽었다` 를 구분하지 못한다. 화면은 단계로
	 * 판단한다 — 축제가 시작된 뒤에도 비어 있으면 정말로 없는 것이다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Facility")
	TArray<FFF_LiveFacilityView> GetLiveFacilities() const;

	// --- 홍보 (2026-08-27 추가 — U5 홍보 패널) ---
	//
	// 🔴 **여기만 절반이 더미다.** 인력·프로그램·피날레와 다르다.
	//
	// | 조각 | 어디 것인가 |
	// | --- | --- |
	// | 등급 비용·배수 | 경제 데이터 (`UFF_FestivalDefinition`) — **진짜** |
	// | Gold 차감·원장 기록 | 소유자 `TrySpend(..., EFF_SpendReason::Promotion)` — **진짜** |
	// | 어느 등급을 샀는가 | **이 클래스가 들고 있는 더미** |
	// | 배수가 방문객에 먹는 것 | **아무도 소비하지 않는다** |
	//
	// 마지막 줄이 중요하다 — 사도 방문객이 늘지 않는다. **그 사실을 화면에 적어야 한다.**
	// 김시리님 API 가 오면 아래 넷의 구현만 갈아끼운다 (`UI_API.md` 요청 표).

	/** 고를 수 있는 등급 전체. `안 함` 을 첫 줄로 포함한다. 축제 전에는 빈 배열이다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Promotion")
	TArray<FFF_PromotionOptionView> GetPromotionOptions() const;

	/** 구매한 등급. 안 샀으면 `None`. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Promotion")
	EFF_PromotionChoice GetPurchasedPromotion() const { return PurchasedPromotion; }

	/**
	 * 지금 홍보를 살 수 있는 단계인지 (셋업에서 한 번).
	 *
	 * ⚠️ **판정 소유자가 없어서 여기서 단계를 본다.** 프로그램의 `CanScheduleProgramNow()` 처럼
	 * 브리지가 답해 줘야 하는 질문인데 그 API 가 없다. 생기면 중계로 바꾼다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Promotion")
	bool CanPurchasePromotionNow() const;

	/**
	 * 홍보를 사 달라고 **요청한다.** 축제당 1회이고 3일 전체에 적용된다.
	 *
	 * Gold 차감은 소유자 `TrySpend` 가 하므로 실패하면 아무것도 바뀌지 않는다.
	 * 등급 보관만 이 클래스가 임시로 한다.
	 *
	 * @return 요청이 받아들여졌는지. 실패 이유는 화면이 `FFF_PrepPromotionRules` 로 구분한다
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Promotion")
	bool RequestPurchasePromotion(EFF_PromotionChoice Choice);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Promotion")
	FFF_OnPromotionViewChanged OnPurchasedPromotionChanged;

	// --- Command ---

	/**
	 * 날씨를 바꿔 달라고 **요청한다.** 허용되지 않는 단계면 Stage 시스템이 무시한다.
	 * 화면은 `OnWeatherChanged` 결과 이벤트로만 갱신된다 — 낙관적 갱신을 하지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Command")
	void RequestSetWeather(EFF_WeatherType NewWeather);

	/**
	 * Staff 를 `Count` 명 고용해 달라고 **요청한다.** 축제가 끝날 때까지 남는다.
	 *
	 * !! 부분 고용이 없다 !! 실패하면 아무것도 바뀌지 않는다. 그래서 낙관적 갱신을 하지 않고
	 * 화면은 `OnGoldChanged`·`OnTotalStaffCountChanged` 결과 이벤트로만 갱신된다.
	 *
	 * 돌려주는 값은 "요청이 받아들여졌는지"일 뿐이다. **실패 이유는 이 값에 없다** —
	 * 이유는 `GetRemainingHireCap()` 과 `GetAffordableHireCount()` 로
	 * 화면이 구분한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Command")
	bool RequestHireStaff(int32 Count);

	/**
	 * 피날레를 구매해 달라고 **요청한다.** 확인창을 통과한 뒤에만 부른다 — 되돌릴 수 없다.
	 *
	 * 인원 고용과 달리 **실패 이유를 그대로 돌려준다.** 등급마다 막히는 이유가 다르고
	 * (`Gold 부족` / `이미 구매` / `단계 아님`) 플레이어가 할 행동도 다르기 때문이다 (AT-011).
	 *
	 * !! 화면 문구를 이 enum 에서 직접 만들지 않는다 !!
	 * `UMETA(DisplayName)` 을 런타임에 읽으면 패키징 빌드에서 영문이 뜬다 (2026-08-19 김시리 정정).
	 * `UFF_EconomyDisplayTextLibrary::GetFinalePurchaseResultText()` 를 쓴다.
	 *
	 * 성공해도 화면을 직접 갱신하지 않는다. `OnPurchasedFinaleGradeChanged` · `OnGoldChanged`
	 * 결과 이벤트로만 갱신된다 — 낙관적 갱신 금지 (UI 규칙 2).
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Command")
	EFF_FinalePurchaseResult RequestPurchaseFinale(EFF_FinaleGrade Grade);

	/**
	 * 이 카드를 이 시설에 편성해 달라고 **요청한다.**
	 *
	 * 피날레와 달리 **확인창을 거치지 않는다.** 하루짜리이고 다음 날 다시 고를 수 있어서
	 * 되돌릴 수 없는 결정이 아니다. 확인창을 흔하게 쓰면 아무도 읽지 않게 된다.
	 *
	 * 실패하면 아무것도 바뀌지 않는다 — 예약 슬롯을 못 만들면 Gold 도 차감하지 않는다.
	 * 화면은 `OnScheduledProgramsChanged` · `OnGoldChanged` 결과 이벤트로만 갱신된다.
	 *
	 * 문구는 `UFF_EconomyDisplayTextLibrary` 가 소유한다. enum `DisplayName` 을 읽지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Command")
	EFF_ProgramPurchaseResult RequestScheduleProgram(FName ProgramId, FGuid FacilityInstanceId);

	/**
	 * 이 금액을 지금 지출할 수 있는지. 구매 버튼 비활성 판정에 쓴다.
	 *
	 * UI가 `GetGold() >= Amount`로 직접 비교하지 않는다 — 잔액 규칙의 소유자는 경제 시스템이고
	 * 나중에 예약금·할인 같은 조건이 붙으면 UI의 비교식이 조용히 틀리게 된다.
	 * 소유자가 없으면 false를 돌려준다. "살 수 있다"고 잘못 알리는 쪽이 더 나쁘다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	bool CanAfford(int32 Amount) const;

	// --- Stage (강민서 시스템 중계) ---

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	EFF_FestivalStage GetCurrentStage() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	int32 GetCurrentDay() const;

	// !! Slot은 의도적으로 노출하지 않는다 !!
	//
	// 2026-08-06 12시 스크럼에서 하루 4 Slot 구조를 없애고 날짜마다 Live를 한 번만 돌리기로 했다.
	// 시간 담당의 제거 작업이 아직 원격에 안 올라와서 UFF_FestivalStageSubsystem::CurrentSlot은
	// 여전히 존재하지만, UI가 그걸 화면에 띄우면 곧 지워질 개념을 플레이어에게 노출하게 된다.
	// 제거가 실제로 들어와도 이 클래스는 안 깨진다.
	// 근거: docs/design/stage-and-time.md 5절

	/** 남은 시간(초). Stage 시스템이 주는 값을 그대로 돌려준다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	float GetRemainingSeconds() const;

	/**
	 * Stage 표시 문구.
	 * 모르는 Stage가 와도 빈 화면 대신 원본 이름을 돌려준다.
	 * enum 순서나 개수에 의존하지 않는다 — 기획 7단계와 코드 8단계가 아직 다르다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	FText GetCurrentStageDisplayText() const;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Stage")
	FFF_OnStageViewChanged OnStageViewChanged;

	/**
	 * 남은 시간이 1초 단위로 바뀔 때만 쏜다. 매 틱 쏘면 위젯이 초당 수십 번 갱신된다.
	 *
	 * 2026-08-11부터 Stage 시스템의 `OnRemainingSecondsChanged`를 **구독해서 중계**한다.
	 * 폴링하지 않는다. 위젯 입장에서는 바뀐 게 없다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Stage")
	FFF_OnIntMetricChanged OnRemainingSecondsChanged;

	// --- Command (Stage 진행 요청) ---
	//
	// !! 위젯은 Stage 서브시스템을 직접 부르지 않는다 !!
	// UI 규칙 2 — "ViewModel로 읽고 Command로 요청한다". 여기가 그 Command 계층이다.
	// 소유자 시스템이 바뀌어도 위젯을 고치지 않기 위해 한 겹 둔다.
	//
	// 낙관적 갱신을 하지 않는다. 요청만 보내고 화면은 결과 이벤트(OnStageViewChanged)로 갱신된다.

	/** Report에서 `다음 날`을 눌렀을 때. Report가 아니거나 마지막 날이면 아무 일도 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Command")
	void RequestAdvanceToNextDay();

	/** 스킵 가능한 단계를 건너뛴다. 불가능한 단계면 Stage 시스템이 무시한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Command")
	void RequestSkipCurrentStage();

	/** `다음 날` 버튼을 켤지. Report 단계이고 마지막 날이 아닐 때만 true. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Command")
	bool CanAdvanceToNextDay() const;

	/**
	 * 마지막 날인지. **UI가 총 일수를 알지 못하게 하려고** Stage 시스템의 판단을 그대로 중계한다.
	 * `CanAdvanceToNextDay()`가 false인 이유를 화면이 구분해서 설명하려면 이게 필요하다
	 * (마지막 날이라서 vs 아직 Report가 아니라서).
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	bool IsFinalDay() const;

	/** 지금 Report 단계인지. Stage enum을 위젯이 직접 비교하지 않게 한다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	bool IsReportStage() const;

	/**
	 * `건너뛰기` 버튼을 켤지.
	 *
	 * DaySequence 표의 `bSkippable`을 읽는 `UFF_FestivalStageSubsystem::IsCurrentStageSkippable()`을
	 * 그대로 중계한다. 2026-08-13에 강민서가 public + BlueprintPure로 열어 줘서
	 * Stage 종류로 짐작하던 근사값을 걷어냈다.
	 *
	 * !! 이 값으로 요청을 막지 않는다 !! 버튼 활성/비활성 표시에만 쓴다.
	 * 스킵 가능 여부의 최종 판정은 Stage 시스템이 소유한다 (UI 규칙 2).
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Command")
	bool CanSkipCurrentStage() const;

	// --- 배속 (Live HUD 재생 컨트롤) ---

	/**
	 * 배속을 바꿔 달라고 요청한다.
	 *
	 * !! UI가 시간을 소유하지 않는다 !! 메인 공연 잠금 구간에서는 Stage 시스템이 요청을 무시하고
	 * 1배속을 유지한다. 여기서 미리 막지 않고 요청만 보낸다 — 잠금 판정의 소유자는 저쪽이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Command")
	void RequestSetTimeScale(float NewScale);

	/** 현재 배속. 버튼 선택 상태 표시에 쓴다. 소유자가 없으면 1을 돌려준다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	float GetTimeScale() const;

	/**
	 * 지금 배속을 바꿀 수 없는 구간인지 (메인 공연 잠금).
	 * 버튼을 비활성할 때 **이유를 문구로 함께** 보여준다. 흐려짐만으로는 왜 못 누르는지 알 수 없다 (AT-011).
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	bool IsTimeScaleLocked() const;

	/**
	 * 배속 잠금이 걸리거나 풀렸다.
	 *
	 * === 🔴 이게 없으면 화면이 조용히 거짓말을 한다 ===
	 * 잠금은 **단계 안에서** 바뀐다 — 라이브 도중 메인 공연이 시작되면 걸리고 끝나면 풀린다.
	 * 그런데 화면은 단계가 바뀔 때만 다시 그렸다. 그래서 4배속을 눌러 둔 채 공연 구간에
	 * 들어가면 시간만 1배속으로 느려지고 **버튼은 4배속이 눌린 채 이유도 안 뜬다.**
	 * 2026-09-03 백종태가 실기에서 "왜 혼자 느려지지" 로 발견했다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Stage")
	FFF_OnBoolViewChanged OnTimeScaleLockChanged;

	/**
	 * Live 중 게임 속 시각 문구 (예: `19:42`).
	 *
	 * Stage 시스템이 `FString`으로 주는 것을 `FText`로 감싸 돌려준다.
	 * Live가 아니면 빈 값이다 — 호출하는 쪽이 비어 있으면 시계를 숨긴다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	FText GetLiveClockText() const;

	/**
	 * Live 경과분을 게임 속 시각 문구로 바꾼다 (예: 90 → `19:30`).
	 *
	 * 프로그램 카드가 `StartMinutes`·`EndMinutes` 를 그대로 들고 있어서 필요하다.
	 * **UI 가 시작 시각에 분을 더해 계산하지 않는다** — 개장 시각과 배율의 소유자는 Stage 시스템이다.
	 * 소유자가 없으면 빈 값이고, 호출하는 쪽이 비어 있으면 시간 칸을 숨긴다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	FText GetLiveClockTextAtMinutes(int32 LiveMinutes) const;

	// --- 스텁 전용. 실제 소유자 구현이 붙으면 삭제한다 ---

	/**
	 * 0~100 지표를 강제로 설정한다. 값이 실제로 바뀔 때만 델리게이트를 쏘고 범위를 벗어나면 클램프한다.
	 * 경제 값은 단위가 달라 섞지 않는다. Debug_SetEconomy를 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Debug")
	void Debug_SetMetrics(int32 InSimSatisfaction, int32 InScoreSatisfaction, int32 InSafety);

	/** 경제 값을 강제로 설정한다. 금액이라 클램프하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Debug")
	void Debug_SetEconomy(int32 InGold, int32 InTotalRevenue, int32 InTodayRevenue);

	/** 현재 값 전부를 로그에 찍는다. 위젯이 없거나 화면을 못 볼 때 확인용. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Debug")
	void Debug_DumpToLog() const;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_FestivalViewModelSubsystem, STATGROUP_Tickables);
	}

protected:
	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	/** 소유자의 메인 공연 시작·종료를 잠금 변화 하나로 합쳐 중계한다. */
	UFUNCTION()
	void HandleMainProgramStarted(FString EventLabel);

	UFUNCTION()
	void HandleMainProgramEnded(FString EventLabel);

	/**
	 * Stage 시스템이 초 단위로 쏘는 값을 받아 위젯에 중계한다.
	 *
	 * 여기서 한 번 더 중복을 걸러낸다. Stage 쪽 `LastBroadcastSeconds`가 **Stage 전환 시
	 * 초기화되지 않아서**, 새 단계의 첫 초가 이전 값과 같으면 방송이 아예 안 오고,
	 * 반대로 Sync와 겹쳐 같은 값이 두 번 올 수도 있다. 위젯이 그걸 몰라도 되게 한다.
	 */
	UFUNCTION()
	void HandleRemainingSecondsChanged(int32 NewSeconds);

	/** 경제 시스템이 쏘는 Gold를 받아 위젯에 중계한다. */
	UFUNCTION()
	void HandleOwnerGoldChanged(int32 NewValue);

	UFUNCTION()
	void HandleOwnerTotalRevenueChanged(int32 NewValue);

	UFUNCTION()
	void HandleOwnerTodayRevenueChanged(int32 NewValue);

	UFUNCTION()
	void HandleOwnerExpectedVisitorsChanged(int32 NewValue);

	/**
	 * 환경 상태를 받아 **날씨만** 중계한다.
	 *
	 * 원본은 `(Weather, bIsNight)` 두 값을 함께 쏘는데 야간 전환에도 울린다. 날씨가 실제로
	 * 바뀔 때만 방송하지 않으면 위젯이 밤이 될 때마다 헛돈다.
	 */
	UFUNCTION()
	void HandleOwnerEnvironmentChanged(EFF_WeatherType NewWeather, bool bIsNight);

	/**
	 * 전체 인원을 받아 중계한다. 기본·고용 내역은 방송에 실려 오지 않으므로 여기서 다시 읽는다.
	 * 인자 하나만 믿고 내역을 갱신하지 않으면 "총 6명인데 기본 4 + 고용 0" 같은 화면이 된다.
	 */
	UFUNCTION()
	void HandleOwnerStaffCountChanged(int32 NewValue);

	/**
	 *
	 * 원본 델리게이트가 **인자를 싣지 않으므로** 핸들러도 파라미터가 없어야 한다
	 * (`FFF_OnProgramScheduleChanged` 는 `DECLARE_DYNAMIC_MULTICAST_DELEGATE`).
	 */
	UFUNCTION()
	void HandleOwnerProgramScheduleChanged();

	/** 피날레 등급 변경을 위젯에 중계한다. 같은 등급이 두 번 오면 넘긴다. */
	UFUNCTION()
	void HandleOwnerFinaleGradeChanged(EFF_FinaleGrade NewGrade);

	/** 만족도 시스템이 쏘는 SimSatisfaction을 받아 위젯에 중계한다. */
	UFUNCTION()
	void HandleOwnerSimSatisfactionChanged(int32 NewValue);

	/** Insights가 쏘는 글 한 건을 변환해 목록 맨 앞에 얹고 위젯에 중계한다. */
	UFUNCTION()
	void HandleOwnerSnsPostAdded(FFF_SnsPostRow NewPost);

	/** `FFF_SnsPostRow`(값)를 `FFF_LiveSocialPost`(표현)로 바꾼다. 도메인↔표현 경계는 이 함수 하나뿐이다. */
	static FFF_LiveSocialPost ConvertSnsPostToLiveSocialPost(const FFF_SnsPostRow& Row);

private:
	UFF_FestivalStageSubsystem* GetStageSubsystem() const;
	UFF_FestivalProgressionSubsystem* GetProgressionSubsystem() const;

	/** 피날레 구매 시점 판정의 소유자. Stage 와 경제를 함께 봐야 해서 브리지가 갖고 있다. */
	UFF_StageEconomyBridgeSubsystem* GetStageEconomyBridgeSubsystem() const;

	/** Stage 시스템 구독을 시도한다. */
	void TryBindStageSubsystem();

	/** 경제 시스템 구독을 시도한다. */
	void TryBindProgressionSubsystem();

	/** 만족도 시스템 구독을 시도한다. */
	void TryBindSatisfactionSubsystem();

	/** SNS Insights 구독을 시도한다. */
	void TryBindInsightsSubsystem();

	/**
	 * `ScoreSatisfaction = Clamp(SimSatisfaction + FinaleBonus, 0, 100)`을 다시 잰다.
	 * 두 입력 중 하나라도 바뀔 때마다 부른다(`HandleOwnerSimSatisfactionChanged`,
	 * `HandleOwnerFinaleGradeChanged`) — 소유자가 둘로 나뉘어 있어 한쪽만 구독해서는
	 * 최신값을 보장할 수 없다.
	 */
	void RecomputeScoreSatisfaction();

	/**
	 * 소유자 시스템에서 지표를 읽어 온다. PollInterval마다 Tick에서 호출된다.
	 * 지금은 소유자가 없어 아무것도 하지 않는다 — 구현이 붙으면 **여기 한 곳만** 채운다.
	 * 변화 감지와 통지는 ApplyIntMetric이 이미 처리하므로 위젯은 손대지 않는다.
	 */
	void PollMetricsFromSources();

	/** 값이 실제로 바뀔 때만 통지한다. 같은 값을 계속 쏘면 위젯이 헛돈다. */
	static void ApplyIntMetric(int32& Current, int32 NewValue, FFF_OnIntMetricChanged& ChangedDelegate);

	
	// 만족도 시스템 쪽 기본값과 맞춰 기본값 50
	// TryBindSatisfactionSubsystem이 실제 값으로 덮어쓴다
	UPROPERTY()
	int32 SimSatisfaction = 50;

	// 분해값 4종. SimSatisfaction과 같은 순간에 같이 갱신된다(HandleOwnerSimSatisfactionChanged)
	UPROPERTY()
	int32 SimSatisfactionWaitScore = 50;

	UPROPERTY()
	int32 SimSatisfactionDensityScore = 50;

	UPROPERTY()
	int32 SimSatisfactionPreferenceScore = 50;

	UPROPERTY()
	int32 SimSatisfactionNeedFulfillmentScore = 50;

	// SimSatisfaction·FinaleBonus 기본값(50, 0)과 맞춘 중립값.
	// RecomputeScoreSatisfaction이 두 소유자가 바인딩되는 대로 실값으로 덮어쓴다.
	UPROPERTY()
	int32 ScoreSatisfaction = 50;

	UPROPERTY()
	int32 Safety = 88;

	/** 초기 Gold 10,000은 game-overview.md 밸런스 시드 v0의 `assumption`이다. 확정값이 아니다. */
	UPROPERTY()
	int32 Gold = 10000;

	UPROPERTY()
	int32 TotalRevenue = 0;

	UPROPERTY()
	int32 TodayRevenue = 0;

	/** 중복 방송 차단용. 야간 전환으로 원본이 울려도 날씨가 같으면 넘긴다. */
	UPROPERTY()
	EFF_WeatherType LastBroadcastWeather = EFF_WeatherType::Clear;

	// 스태프는 더미를 두지 않는다. 소유자에게서 오기 전에는 `모름`이고 화면은 칸을 숨긴다.

	UPROPERTY()
	int32 BaseStaffCount = INDEX_NONE;

	UPROPERTY()
	int32 HiredStaffCount = INDEX_NONE;

	UPROPERTY()
	int32 TotalStaffCount = INDEX_NONE;

	/** 오늘 예상 방문객. 소유자 값이 오기 전에는 `모름` 이다. */
	UPROPERTY(Transient)
	int32 ExpectedVisitors = INDEX_NONE;

	/**
	 * 구매한 홍보 등급. **더미다** — 경제 시스템에 이 상태를 보관할 자리가 아직 없다.
	 *
	 * 소유자(`UFF_FestivalProgressionSubsystem::GetPurchasedPromotionGrade`)의 값을 미러링한다.
	 * 축제 재시작도 그쪽이 지우고 `OnPromotionGradeChanged` 로 알려 온다.
	 */
	UPROPERTY()
	EFF_PromotionChoice PurchasedPromotion = EFF_PromotionChoice::None;

	// 소유자 enum ↔ 화면 enum. 값이 1:1 이라 표가 아니라 switch 로 둔다.
	static EFF_PromotionGrade ToPromotionGrade(EFF_PromotionChoice Choice);
	static EFF_PromotionChoice ToPromotionChoice(EFF_PromotionGrade Grade);

	UFUNCTION()
	void HandleOwnerPromotionGradeChanged(EFF_PromotionGrade NewGrade);

	/** 중복 방송 차단용. Bind + Sync 와 실제 구매 방송이 같은 등급을 두 번 쏘지 않게 한다. */
	UPROPERTY()
	EFF_FinaleGrade LastBroadcastFinaleGrade = EFF_FinaleGrade::None;

	UPROPERTY()
	TObjectPtr<UFF_FestivalStageSubsystem> CachedStageSubsystem;

	UPROPERTY()
	TObjectPtr<UFF_FestivalProgressionSubsystem> CachedProgressionSubsystem;

	/** 경제 시스템을 못 찾았을 때 재시도한 횟수. */
	int32 ProgressionBindRetryCount = 0;

	UPROPERTY()
	TObjectPtr<UFF_SatisfactionMetricsSubsystem> CachedSatisfactionSubsystem;

	/** 만족도 시스템을 못 찾았을 때 재시도한 횟수. */
	int32 SatisfactionBindRetryCount = 0;

	UPROPERTY()
	TObjectPtr<UFF_CrowdInsightsSubsystem> CachedInsightsSubsystem;

	/** SNS Insights 시스템을 못 찾았을 때 재시도한 횟수. */
	int32 InsightsBindRetryCount = 0;

	/** 최신 글이 [0]. Insights 링버퍼 용량(80)과 맞춘다 — 몇 건을 화면에 그릴지는 위젯 몫이다. */
	UPROPERTY()
	TArray<FFF_LiveSocialPost> RecentSnsPosts;
	static constexpr int32 SnsPostViewCapacity = 80;

	/** 남은 시간 중복 방송 차단용. 구독 중계와 Sync가 같은 값을 두 번 쏘지 않게 한다. */
	int32 LastBroadcastRemainingSeconds = INDEX_NONE;

	/** 구독 직후 현재 남은 시간을 한 번 반영한다(Bind + Sync). 중복이면 아무것도 하지 않는다. */
	void BroadcastRemainingSecondsIfChanged(int32 NewSeconds);

	float PollAccumulator = 0.f;

	/**
	 * 폴링 주기(초). 0.1초면 남은 시간의 초 단위 변화를 놓치지 않으면서 부하가 없다.
	 * 지표도 같은 주기로 샘플링하므로 위젯 갱신은 지표당 최대 초당 10회로 묶인다.
	 * 금액처럼 자주 흔들리는 값도 여기서 걸린다.
	 */
	static constexpr float PollInterval = 0.1f;

	/** Stage 시스템을 못 찾았을 때 재시도한 횟수. 상한을 두고 로그를 남긴다. */
	int32 StageBindRetryCount = 0;
	static constexpr int32 MaxStageBindRetry = 100;

public:
	/** 0~100 지표의 상한. 위젯이 게이지 비율을 계산할 때 이 값을 쓴다. 하드코딩하지 않게 하기 위함. */
	static constexpr int32 MetricMax = 100;
};
