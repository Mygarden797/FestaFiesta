// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Save/FF_Saveable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Economy/Rules/FF_FestivalFunds.h"
#include "Economy/Rules/FF_DayLedger.h"
#include "Economy/Rules/FF_FacilityRevenueBook.h"
#include "Economy/Rules/FF_StaffPool.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "NPC/Party/FF_ProgramReservation.h"
#include "NPC/Party/FF_VisitorDistributionBuilder.h"
#include "FF_FestivalProgressionSubsystem.generated.h"

class UFF_ProgramReservationSubsystem;

class UFF_FestivalDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnEconomyValueChanged, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnFinaleGradeChanged, EFF_FinaleGrade, NewGrade);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnPromotionGradeChanged, EFF_PromotionGrade, NewGrade);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnProgramScheduleChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnFestivalDefinitionChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnFacilityRevenueChanged, FGuid, FacilityInstanceId);

/**
 * 잔액 규칙은 FFF_FestivalFunds(순수 구조체)가 갖고, 여기는 "상태 보관과 통지"만 한다.
 * 축제 세션의 진행 상태를 소유한다. [참고] data/metadata/spec/economy_spec.md §4.4
 *
 * [참고] Getter + 변경 델리게이트를 쌍으로 준다
 * [참고] 전부 Gold(int32)이고 상한이 없다.
 */
UCLASS()
/* 저장에 담기는 경제 상태.

   하루가 시작되는 순간만 저장하므로 담을 것이 적다. 장부와 편성은 정산할 때
   정리되고, 알바는 매일 다시 뽑는다. 그래서 남는 것은 누적된 값뿐이다.

   스타듀밸리처럼 "정산이 끝난 상태" 만 저장하고 다음 날 아침부터 시작한다.
*/
USTRUCT()
struct FFF_ProgressionSave
{
	GENERATED_BODY()

	UPROPERTY() int32 Gold = 0;
	UPROPERTY() int32 TotalRevenue = 0;
	UPROPERTY() int32 LastProcessedDay = 0;
	UPROPERTY() int32 EndedDay = 0;
};

class FESTAFIESTA_API UFF_FestivalProgressionSubsystem
	: public UGameInstanceSubsystem, public IFF_Saveable
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// ── 저장 참여 ───────────────────────────────────────────────────────────

	virtual FName GetSaveKey() const override { return TEXT("Progression"); }

	/* 다른 것이 기대는 값이라 가장 먼저 복원한다.

	   시설을 되돌릴 때 설치 비용을 치르므로, 돈이 먼저 들어와 있어야 한다.
	*/
	virtual EFF_SaveLoadPhase GetSaveLoadPhase() const override
	{
		return EFF_SaveLoadPhase::Global;
	}

	virtual void WriteSaveData(TArray<uint8>& OutBytes) const override;
	virtual void ReadSaveData(const TArray<uint8>& InBytes) override;

	// --- 축제 시작 -------------------------------------------------------

	// 축제를 시작하고 자금을 초기 자금으로 맞춘다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	void StartFestival(const UFF_FestivalDefinition* InDefinition);

	// GameInstance 는 `OpenLevel` 을 넘어 살아남으므로 `Initialize()` 는 게임을 켤 때 한 번만
	// 브리지가 `OnFestivalStarted` 를 받아 이 함수를 부른다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	void RestartFestival();

	// 축제 시작됐는지
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	bool IsFestivalStarted() const { return Definition != nullptr; }

	// 현재 축제 정의. 시작 전이면 nullptr.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	const UFF_FestivalDefinition* GetDefinition() const { return Definition; }

	// StartFestival 완료 후 발행. 같은 자금으로 다른 축제를 시작해도 UI 조건을 갱신한다.
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Festival")
	FFF_OnFestivalDefinitionChanged OnFestivalDefinitionChanged;

	// --- Gold ------------------------------------------------------------

	// 지금 쓸 수 있는 잔액. 상단 바 표시와 구매 가능 판정에 쓴다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetGold() const { return Funds.GetGold(); }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnEconomyValueChanged OnGoldChanged;

	// 이 금액을 지출할 수 있는지. 상태를 바꾸지 않는다. UI 구매 버튼 비활성화 판정용.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	bool CanAfford(int32 Amount) const { return Funds.CanAfford(Amount); }

	// 지출한다.
	// 성공하면 true 이고 잔액이 그만큼 줄어든다. DayLedger에 원인별로 기록된다.
	// 실패하면 상태를 전혀 바꾸지 않고 델리게이트도 쏘지 않는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	bool TrySpend(int32 Amount, EFF_SpendReason Reason);
	
	// 누적 매출, 상단바 노출용
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetTotalRevenue() const { return Funds.GetTotalRevenue(); }
	
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnEconomyValueChanged OnTotalRevenueChanged;
	
	// 오늘 매출. Report 화면용 - 하루 경계마다 리셋된다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetTodayRevenue() const { return Funds.GetTodayRevenue(); }
	
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnEconomyValueChanged OnTodayRevenueChanged;
	
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	void AddRevenue(int32 Amount, EFF_EarnReason Reason, FName FacilityId = NAME_None);

	// 설치 시설 한 곳에 귀속되는 매출. FacilityId(종류)는 부스별 표로, FacilityInstanceId(Zone 인스턴스)는
	// 시설별 장부로 간다. 자금·원장은 AddRevenue 와 같은 경로라 세 장부의 합이 항상 맞는다.
	// 부스 수수료(Queue 브리지)와 유료 프로그램 체험비가 부른다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	void AddFacilityRevenue(int32 Amount, EFF_EarnReason Reason, FName FacilityId, FGuid FacilityInstanceId);

	// 설치 시설 한 곳의 오늘·누적 매출과 결제 건수. 시설 정보 패널이 읽는다.
	// 기록이 없으면 false 이고 OutRevenue 는 전부 0 이다 — "아직 결제 없음"과 같은 뜻이다.
	// 오늘 값은 하루 경계에서 0 이 되고 누적은 새 축제에서만 0 이 된다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	bool GetFacilityInstanceRevenue(FGuid FacilityInstanceId, FFF_FacilityInstanceRevenue& OutRevenue) const;

	// 시설 한 곳의 매출이 바뀔 때 그 시설의 GUID 로 쏜다. 구독 직후 현재값은 GetFacilityInstanceRevenue 로 읽는다.
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnFacilityRevenueChanged OnFacilityRevenueChanged;

	// 잔액을 늘린다. 시설 철거 환급이 유일한 호출자다.
	// TotalRevenue는 안 건드린다 — 환급은 새 매출이 아니라 이미 쓴 돈이 되돌아온 것이다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	void Refund(int32 Amount);

	/** Credits a one-time codex reward without counting it as facility sales or demolition refunds.
	 * The codex progress subsystem owns the persistent first-discovery check. */
	void GrantDiscoveryGold(int32 Amount);

	// Report·테스트가 원인별·부스별 분해와 환급 총액을 읽는 창구.
	const FFF_DayLedger& GetDayLedger() const { return DayLedger; }
	
	// 방금 끝난 하루의 원장 — "하루 경계를 넘은 뒤"에만 유효하다.
	// [주의] Report 안에서는 아직 경계를 안 넘었으므로 여기엔 '어제' 값이 들어 있다.
	//		Day1 Report 에는 스냅샷이 아예 없다 (EndedDay == 0)
	//		Day3 뒤에는 경계가 없어서 최종 결산까지 스냅샷이 Day2 에 멈춘다.
	// [규칙] 그리려는 날짜 == GetEndedDay() 일 때만 이것을 쓰고, 아니면 GetDayLedger() 를 쓴다.
	const FFF_DayLedger& GetEndedDayLedger() const { return EndedDayLedger; }

	// GetEndedDayLedger() 가 어느 날짜의 것인지. 0 이면 아직 끝난 하루가 없다.
	// GetLastProcessedDay() 와 하루 어긋난다 — 이쪽은 '닫힌 날', 저쪽은 '열린 날'이다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetEndedDay() const { return EndedDay; }
	
	
	// --- 결산 표 (UI 읽기 모델) ---------------------------------------------
	//
	// 날짜를 인자로 받는다. 원장이 두 개(라이브·스냅샷)라 "어느 쪽인가"를 호출부가
	// 판단하면 Day1·Day3 에서 조용히 틀린 표가 나온다. 그리려는 날짜만 넘기면 안쪽에서 고른다.
	// 들고 있는 것은 두 칸뿐이다 — 진행 중인 하루와 방금 닫힌 하루.

	// 그 날짜의 원장을 갖고 있는가.
	// [주의] 아래 4종이 **빈 결과인 것과 "그 날 값이 0" 인 것을 구분하는 유일한 수단**이다.
	//        표를 그리기 전에 이걸 먼저 물어야 한다 (NeedMetrics 의 GetSampleCount()==0 과 같은 원칙).
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	bool HasLedgerForDay(int32 Day) const;

	// 매출 5종을 enum 선언 순서로. 값 0 인 사유는 줄을 만들지 않는다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	TArray<FFF_LedgerRow> GetEarnRowsForDay(int32 Day) const;

	// 지출 사유를 enum 선언 순서로. 값 0 인 사유는 줄을 만들지 않는다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	TArray<FFF_LedgerRow> GetSpendRowsForDay(int32 Day) const;

	// 부스별 매출을 FacilityId 오름차순으로. UI 가 다시 정렬하지 않는다.
	// [주의] 표시 이름은 주지 않는다 — 경제는 시설의 한글 이름을 모른다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	TArray<FFF_FacilityRevenueRow> GetRevenueByFacilityRowsForDay(int32 Day) const;

	// 시설 철거 환급 총액. **매출이 아니다** — GetEarnRowsForDay 에 섞이지 않는다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetTotalRefundForDay(int32 Day) const;
	
	
	
	// --- 하루 경계 ---------------------------------------------------------
	
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	void BeginNewDay(int32 NewDay);
	
	// 하루 경계를 마지막으로 처리한 날. 축제 시작 전이면 0.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetLastProcessedDay() const { return LastProcessedDay; }
	
	
	// --- Staff ------------------------------------------------------------
	//
	// 고용은 1회 구매이고 축제가 끝날 때까지 남는다. 하루 경계에서 줄지 않는다.
	// [주의] 스태프는 시설 성능(회전율·객단가)에도, 유료 프로그램 편성에도 관여하지 않는다.
	//        시설은 시설 강화가 올리고, 프로그램은 Gold 로만 산다. 스태프가 쓰일 곳은
	//        앞으로 붙을 사건 대응뿐이다.

	// 무페이 기본 Staff.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetBaseStaffCount() const
	{
		return StaffPool.GetBaseStaffCount();
	}

	// 이번 축제에 고용한 인원. 돈을 낸 만큼이고 남은 날 내내 유지된다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetHiredStaffCount() const
	{
		return StaffPool.GetHiredStaffCount();
	}

	// 지금 일하는 전체 인원.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetTotalStaffCount() const
	{
		return StaffPool.GetTotalStaffCount();
	}

	// RequestedCount 명을 고용하고 비용을 지불한다. 축제가 끝날 때까지 남는다.
	// 잔액이 모자라거나 총원 상한을 넘으면 거부하고 상태를 전혀 바꾸지 않는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	bool TryHireStaff(int32 RequestedCount);

	// 지금 잔액으로 추가 고용할 수 있는 최대 인원. 상태를 바꾸지 않는다.
	// UI 슬라이더 상한과 버튼 비활성화 판정용.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetAffordableHireCount() const;

	// 축제 총원 상한까지 남은 고용 가능 인원. 잔액은 보지 않는다.
	// `GetAffordableHireCount()`과 다르면 막은 것이 상한인지 돈인지 화면에서 구분할 수 있다.
	// [주의] "왜 더 못 뽑는가"를 화면에 쓰기 위한 것이다. 고용 가능 판정에 쓰지 말 것.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetRemainingHireCap() const;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnEconomyValueChanged OnTotalStaffCountChanged;
	
	
	// --- 피날레 -----------------------------------------------------------

	// 피날레를 구매한다. 실패 이유를 결과로 돌려주고, 실패 시 상태를 전혀 바꾸지 않는다.
	// Day3 MorningCheck 판정은 UFF_StageEconomyBridgeSubsystem 에서 진행
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	EFF_FinalePurchaseResult TryPurchaseFinale(EFF_FinaleGrade Grade);

	// 구매한 등급. 안 샀으면 None.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	EFF_FinaleGrade GetPurchasedFinaleGrade() const { return PurchasedFinaleGrade; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnFinaleGradeChanged OnFinaleGradeChanged;

	// 구매한 등급의 만족도 가산값 (0~100 척도). 안 샀으면 0.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetFinaleSatisfactionBonus() const;


	// --- 홍보 -------------------------------------------------------------

	// 홍보를 구매한다. 축제당 1회이고 변경·취소가 없다(피날레와 같은 규칙).
	// 단계 판정은 여기서 하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	EFF_PromotionPurchaseResult TryPurchasePromotion(EFF_PromotionGrade Grade);

	// 구매한 홍보 등급. 안 샀으면 None.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	EFF_PromotionGrade GetPurchasedPromotionGrade() const { return PurchasedPromotionGrade; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnPromotionGradeChanged OnPromotionGradeChanged;

	/**
	 * 홍보를 반영한 하루 예상 방문객. 축제가 시작되지 않았으면 `INDEX_NONE`("모름").
	 * [주의] 0 과 `INDEX_NONE` 은 다르다. 0 은 "아무도 안 온다", `INDEX_NONE` 은 "알 수 없다" 다.
	 * 🔴 crowd 가 `AFF_SpawnActor::UnitCount` 를 쓰므로, 그쪽이 이 창구를 읽기 전까지 홍보를 사도 손님은 안 늘어난다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	int32 GetExpectedVisitors() const;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnEconomyValueChanged OnExpectedVisitorsChanged;


	// --- 유료 프로그램 -----------------------------------------------------

	// 편성할 수 있는 카드 전체. 축제가 시작되지 않았으면 빈 배열.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	TArray<FFF_ProgramCard> GetProgramCatalog() const;

	// 선택 시설의 최신 수익 표 참가비를 적용한 카드 전체. 비용·정원·허용 시설은 그대로다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	TArray<FFF_ProgramCard> GetProgramCatalogForFacility(FGuid FacilityInstanceId) const;

	// 오늘 편성한 카드와 그 예약 핸들. 하루 경계에서 비워진다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	TArray<FFF_ScheduledProgram> GetScheduledPrograms() const { return ScheduledPrograms; }

	// 편성 가능한지만 본다. 상태를 바꾸지 않으므로 버튼 활성화·툴팁에 그대로 쓴다.
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Economy")
	EFF_ProgramPurchaseResult CanScheduleProgram(FName ProgramId) const;

	/**
	 * 프로그램을 오늘 편성한다. 실패 시 상태를 바꾸지 않는다.
	 * 허용 단계(Setup·MorningCheck) 판정은 호출부가 한다.
	 *
	 * @param FacilityInstanceId 프로그램을 여는 시설. ZoneSystem 의 인스턴스 핸들이다
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	EFF_ProgramPurchaseResult TryScheduleProgram(FName ProgramId, FGuid FacilityInstanceId);

	/**
	 * 시작 시각이 지난 프로그램의 체험비를 원장에 넣는다. **정상 경로다.**
	 *
	 * 참가자는 스폰될 때 이미 확정된다 (`UFF_ProgramReservationSubsystem::ConfirmReservedParty`).
	 * 프로그램을 사면 정원만큼의 손님이 추가로 방문하므로, 프로그램이 열리는 순간 체험비는
	 * 확정값이다 — 마감까지 기다릴 이유가 없다. 부스 수수료도 이미 Live 중 실시간이다.
	 *
	 * 한 번 걷은 프로그램은 다시 걷지 않는다. Live 동안 매 초 불려도 안전하다.
	 *
	 * @param NowMinutes Live 경과 시각(축제분). `UFF_FestivalStageSubsystem::TryGetLiveElapsedMinutes` 로 얻는다
	 * @return 이번 호출에서 원장에 들어간 체험비 총액
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	int32 SettleStartedPrograms(float NowMinutes);

	/**
	 * 아직 안 걷은 프로그램의 체험비를 전부 걷는다. **안전망이다.**
	 *
	 * Closing 진입에서 한 번 부른다. 여기서 걷히는 것이 있으면 시작 시각 정산이 그 카드를
	 * 놓쳤다는 뜻이라 Warning 을 남긴다 — 카드 시각이 그날 Live 길이를 넘었거나
	 * Live 를 스킵한 경우다.
	 *
	 * @return 이번 호출에서 원장에 들어간 체험비 총액. 정상이면 0 이다
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Economy")
	int32 SettleScheduledPrograms();

	/**
	 * 오늘 편성한 프로그램들이 데려올 손님의 주문서. (스폰 쪽에 넘길 목록)
	 *
	 * 아침에 편성이 끝난 뒤 그날 유입을 시작할 때 한 번 읽는다.
	 * 합계 인원은 편성한 카드들의 정원 합과 같다.
	 */
	TArray<FFF_ReservedPartyRequest> GetReservedPartyRequests() const;

	// 예약 슬롯의 소유자를 물린다.
	void SetProgramReservations(UFF_ProgramReservationSubsystem* InReservations);

	// 오늘 편성 목록이 바뀌면 쏜다. 인원 표시도 같이 갱신해야 한다.
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Economy")
	FFF_OnProgramScheduleChanged OnScheduledProgramsChanged;

private:
	friend class UFF_FacilityUpgradeSubsystem;
	// Synchronous facility purchase: reserve without events, record on commit, publish only after all systems commit.
	bool ReserveFacilityUpgradeSpend(FGuid CommandId, int32 Amount);
	void CommitFacilityUpgradeSpend(FGuid CommandId);
	void RollbackFacilityUpgradeSpend(FGuid CommandId);
	void PublishFacilityUpgradeSpend(FGuid CommandId);
	FGuid PendingFacilityUpgradeCommand;
	int32 PendingFacilityUpgradeCost = 0;
	int32 GoldBeforeFacilityUpgrade = 0;
	bool bFacilityUpgradeSpendCommitted = false;

	// Project Settings 의 DefaultFestival 을 읽어 축제를 시작한다.
	// 저작이 비어 있으면 Warning 만 남기고 시작하지 않는다.
	void StartConfiguredFestival();

	// 이 시설에서 이 프로그램을 열 수 있는가.
	// 시설 종류를 ZoneSystem 에서 읽어 카드의 허용 목록과 대조한다.
	bool IsFacilityTypeAllowedForCard(const FFF_ProgramCard& Card, const FGuid& FacilityInstanceId) const;

	FName ResolveFacilityDefinitionId(FGuid FacilityInstanceId) const;

	/**
	 * 프로그램 하나의 체험비를 원장에 넣고 `bFeeSettled` 를 켠다.
	 * 시작 시각 정산과 Closing 안전망이 같은 계산을 쓰게 하는 유일한 구현이다.
	 *
	 * @param bLateSettlement 안전망 경로인가. Warning 문구가 갈린다
	 * @return 원장에 들어간 금액. 이미 걷었거나 카드·예약을 못 찾으면 0
	 */
	int32 SettleOneProgram(FFF_ScheduledProgram& Scheduled, bool bLateSettlement);

	// 값이 실제로 바뀔 때만 통지한다.
	void BroadcastExpectedVisitorsIfChanged(int32 PreviousExpectedVisitors);
	void BroadcastGoldIfChanged(int32 PreviousGold);
	void BroadcastTotalRevenueIfChanged(int32 PreviousTotalRevenue);
	void BroadcastTodayRevenueIfChanged(int32 PreviousTodayRevenue);
	void BroadcastTotalStaffCountIfChanged(int32 PreviousTotalStaffCount);
	
	// 하루 경제 정산을 수행한다.
	// 같은 날 이미 처리했다면 아무 작업도 하지 않고 false 를 반환한다.
	bool TryProcessNewDay(int32 NewDay);
	
	// Day 에 해당하는 원장을 찾는다. 안 들고 있으면 nullptr.
	const FFF_DayLedger* FindLedgerForDay(int32 Day) const;

	// 잔액 규칙의 소유자. UObject 가 아니라 값 타입이다.
	FFF_FestivalFunds Funds;
	
	// 당일 원인별/시설별 분해. 여기서만 건드린다.
	FFF_DayLedger DayLedger;
	
	// 직전 하루의 원장 스냅샷. TryProcessNewDay 의 Reset 직전에서만 갱신한다
	FFF_DayLedger EndedDayLedger;

	// 설치 시설별(인스턴스) 매출. 부스별 표(종류)와 달리 하루 경계에서 오늘 값만 지운다.
	FFF_FacilityRevenueBook FacilityRevenueBook;

	// AddRevenue·AddFacilityRevenue 의 유일한 구현. 자금·원장·시설별 장부를 한 번에 갱신하고 통지한다.
	void RecordRevenue(int32 Amount, EFF_EarnReason Reason, FName FacilityId, const FGuid& FacilityInstanceId);

	// EndedDayLedger 가 담고 있는 날짜. 0 = 아직 끝난 하루 없음.
	// LastProcessedDay 와 하루 어긋난다 (닫힌 날 vs 열린 날). 둘을 섞어 쓰지 않는다.
	int32 EndedDay = 0;

	// 무페이 기본 팀 + 지금까지 고용한 인원.
	FFF_StaffPool StaffPool;

	// 하루 경계를 마지막으로 처리한 날. 같은 날 중복 처리를 막는 수단이다.
	int32 LastProcessedDay = 0;
	
	// 구매한 피날레 등급. 축제당 1회이고 되돌릴 수 없다.
	EFF_FinaleGrade PurchasedFinaleGrade = EFF_FinaleGrade::None;

	// 구매한 홍보 등급. 피날레와 같은 규칙이다.
	EFF_PromotionGrade PurchasedPromotionGrade = EFF_PromotionGrade::None;

	// 오늘 편성한 프로그램. 카드가 하루짜리라 하루 경계에서 비운다.
	TArray<FFF_ScheduledProgram> ScheduledPrograms;

	// 예약 슬롯의 소유자. GameInstance 스코프로 수명이 같아서 캐시해도 된다.
	UPROPERTY()
	TObjectPtr<UFF_ProgramReservationSubsystem> ProgramReservations;

	// 편성된 카드의 ProgramId 만 뽑는다. 순수 판정 함수가 그 형태를 받는다.
	TArray<FName> GetScheduledProgramIds() const;
	
	// 현재 축제 정의. 시작 전이면 nullptr.
	UPROPERTY()
	TObjectPtr<const UFF_FestivalDefinition> Definition;
};
