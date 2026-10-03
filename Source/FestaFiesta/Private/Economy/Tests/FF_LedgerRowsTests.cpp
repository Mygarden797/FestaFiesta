// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"

/**
 * 결산 표 읽기 모델 검증 (UI_API.md 2026-08-19 백종태 님 요청).
 *
 * 원장 자체의 집계는 FF_DayLedgerTests.cpp, 스냅샷 수명은 FF_DayBoundaryTests.cpp 가 갖는다.
 * 여기는 **"UI 가 받는 표가 맞는가"** 만 본다 — 줄 제외·순서·정렬·날짜 해석.
 *
 * 🔴 이 API 는 날짜를 인자로 받는다. 어제 만든 스냅샷의 세 함정(Day1 은 스냅샷이 없고,
 *    Day3 뒤에는 경계가 없어 스냅샷이 Day2 에 멈추고, Report 중에는 라이브가 맞다)을
 *    호출부가 외우지 않게 하려는 것이다. Day 하나만 넘기면 안쪽에서 고른다.
 *
 * **월드를 만들지 않는다.** FF_DayBoundaryTests.cpp 와 같은 이유다(정산 경로가
 * GetGameInstance() 를 쓰지 않으므로 GameInstance 를 Outer 로 NewObject 하면 된다).
 */
namespace FFF_LedgerRowsTestHelpers
{
	constexpr int32 HireCost = 150;
	constexpr int32 BaseStaff = 6;
	constexpr int32 MaxHires = 6;

	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	// 같은 이유로 이 파일은 `using namespace` 를 쓰지 않는다.
	struct FLedgerRowsHarness
	{
		bool Initialize(int32 InitialGold)
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			if (GameInstance == nullptr)
			{
				return false;
			}

			Progression = NewObject<UFF_FestivalProgressionSubsystem>(GameInstance);
			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			CostTable = NewObject<UDataTable>(GameInstance);
			if (Progression == nullptr || Definition == nullptr || CostTable == nullptr)
			{
				return false;
			}

			// 표가 비어 있으면 IsDefinitionValid 가 Error 를 찍고 자동화가 실패로 취급한다.
			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
			Definition->FacilityCostTable = CostTable;

			Definition->FestivalId = TEXT("LedgerRowsAutomationFestival");
			Definition->InitialGold = InitialGold;
			Definition->StaffHireCostGold = HireCost;
			Definition->InitialStaffCount = BaseStaff;
			Definition->MaxHiredStaffCount = MaxHires;

			Progression->StartFestival(Definition);
			return true;
		}

		UGameInstance* GameInstance = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
	};

	// 표에서 ReasonId 로 줄을 찾는다. 없으면 nullptr.
	const FFF_LedgerRow* FindRow(const TArray<FFF_LedgerRow>& Rows, FName ReasonId)
	{
		return Rows.FindByPredicate(
			[ReasonId](const FFF_LedgerRow& Row) { return Row.ReasonId == ReasonId; });
	}

	// 표의 ReasonId 를 순서대로 이어 붙인다. 순서 검증용.
	FString JoinReasonIds(const TArray<FFF_LedgerRow>& Rows)
	{
		TArray<FString> Names;
		for (const FFF_LedgerRow& Row : Rows)
		{
			Names.Add(Row.ReasonId.ToString());
		}
		return FString::Join(Names, TEXT(","));
	}

	// 부스별 표는 별도 구조체다 — 경제가 시설의 한글 이름을 모르므로 DisplayName 이 없다.
	FString JoinFacilityIds(const TArray<FFF_FacilityRevenueRow>& Rows)
	{
		TArray<FString> Names;
		for (const FFF_FacilityRevenueRow& Row : Rows)
		{
			Names.Add(Row.FacilityId.ToString());
		}
		return FString::Join(Names, TEXT(","));
	}
}

// 값이 0 인 사유는 줄 자체를 만들지 않는다 (백종태 님 지정).
// 0 줄을 그리면 원인 14종이 매번 다 뜨고 실제로 일어난 일이 묻힌다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LedgerRows_ZeroReasonsAreExcluded, "FestaFiesta.Economy.LedgerRows.ZeroReasonsAreExcluded", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LedgerRows_ZeroReasonsAreExcluded::RunTest(const FString& Parameters)
{
	FFF_LedgerRowsTestHelpers::FLedgerRowsHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	// 매출 5종 중 2종만 만든다.
	Harness.Progression->AddRevenue(1200, EFF_EarnReason::AdmissionFee);
	Harness.Progression->AddRevenue(300, EFF_EarnReason::MinigameReward);

	const TArray<FFF_LedgerRow> EarnRows = Harness.Progression->GetEarnRowsForDay(1);
	TestEqual(TEXT("만든 2종만 줄이 된다"), EarnRows.Num(), 2);
	TestNotNull(TEXT("입장료 줄이 있다"),
		FFF_LedgerRowsTestHelpers::FindRow(EarnRows, TEXT("AdmissionFee")));
	TestNotNull(TEXT("미니게임 줄이 있다"),
		FFF_LedgerRowsTestHelpers::FindRow(EarnRows, TEXT("MinigameReward")));
	TestNull(TEXT("0 인 부스 수수료는 줄이 없다"),
		FFF_LedgerRowsTestHelpers::FindRow(EarnRows, TEXT("FacilityCommission")));

	// 거절할 수 없는 지출이 없어져서 아무것도 안 사면 지출 줄이 하나도 없다.
	const TArray<FFF_LedgerRow> SpendRows = Harness.Progression->GetSpendRowsForDay(1);
	TestEqual(TEXT("산 것이 없으면 지출 줄도 없다"), SpendRows.Num(), 0);

	// 인원을 고용하면 고용비 줄이 생긴다.
	TestTrue(TEXT("인원 2명을 고용한다"), Harness.Progression->TryHireStaff(2));
	const TArray<FFF_LedgerRow> SpendRowsAfterHire = Harness.Progression->GetSpendRowsForDay(1);
	TestEqual(TEXT("고용비 한 줄이 생긴다"), SpendRowsAfterHire.Num(), 1);

	// 줄이 생기면 금액도 실제 값이어야 한다.
	const FFF_LedgerRow* HireRow =
		FFF_LedgerRowsTestHelpers::FindRow(SpendRowsAfterHire, TEXT("StaffHire"));
	if (TestNotNull(TEXT("고용비 줄을 찾는다"), HireRow))
	{
		TestEqual(TEXT("고용비가 2명분이다"), HireRow->Amount,
			2 * FFF_LedgerRowsTestHelpers::HireCost);
	}

	// 만들어진 줄의 금액은 절대 0 이 아니다 (0 제외 규칙의 반대 방향).
	for (const FFF_LedgerRow& Row : SpendRowsAfterHire)
	{
		TestNotEqual(TEXT("표에 0 원 줄이 섞이지 않는다"), Row.Amount, 0);
	}

	return true;
}

// 줄 순서는 enum 선언 순서다. 기록 순서가 아니다.
// TMap/기록 순서에 의존하면 같은 하루가 실행마다 다른 순서로 그려진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LedgerRows_RowsFollowEnumDeclarationOrder, "FestaFiesta.Economy.LedgerRows.RowsFollowEnumDeclarationOrder", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LedgerRows_RowsFollowEnumDeclarationOrder::RunTest(const FString& Parameters)
{
	FFF_LedgerRowsTestHelpers::FLedgerRowsHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	// **enum 역순으로** 기록한다. 그래도 표는 선언 순서로 나와야 한다.
	Harness.Progression->AddRevenue(50, EFF_EarnReason::MinigameReward);      // = 4
	Harness.Progression->AddRevenue(40, EFF_EarnReason::DailyGoalBonus);      // = 3
	Harness.Progression->AddRevenue(30, EFF_EarnReason::ProgramFee);          // = 2
	Harness.Progression->AddRevenue(20, EFF_EarnReason::FacilityCommission, TEXT("Booth_A"));
	Harness.Progression->AddRevenue(10, EFF_EarnReason::AdmissionFee);        // = 0

	const TArray<FFF_LedgerRow> EarnRows = Harness.Progression->GetEarnRowsForDay(1);
	TestEqual(TEXT("매출 5종이 전부 줄이 된다"), EarnRows.Num(), 5);
	TestEqual(TEXT("매출 줄이 enum 선언 순서다"),
		FFF_LedgerRowsTestHelpers::JoinReasonIds(EarnRows),
		FString(TEXT("AdmissionFee,FacilityCommission,ProgramFee,DailyGoalBonus,MinigameReward")));

	// 금액이 줄과 짝이 맞는지 — 순서만 맞고 값이 밀리면 표가 조용히 틀린다.
	TestEqual(TEXT("첫 줄 금액이 입장료 값이다"), EarnRows[0].Amount, 10);
	TestEqual(TEXT("마지막 줄 금액이 미니게임 값이다"), EarnRows[4].Amount, 50);

	return true;
}

// 줄에는 한글 이름과 짧은 enum 이름이 함께 실린다.
// 종태님이 한글을 그대로 그리고 enum 이름을 작게 병기하신다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LedgerRows_RowCarriesKoreanNameAndShortId, "FestaFiesta.Economy.LedgerRows.RowCarriesKoreanNameAndShortId", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LedgerRows_RowCarriesKoreanNameAndShortId::RunTest(const FString& Parameters)
{
	FFF_LedgerRowsTestHelpers::FLedgerRowsHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	// 지출 줄은 플레이어가 산 것으로만 생긴다. 인원을 고용해 한 줄을 만든다.
	const int32 HireCount = 2;
	TestTrue(TEXT("인원을 고용한다"), Harness.Progression->TryHireStaff(HireCount));

	const TArray<FFF_LedgerRow> SpendRows = Harness.Progression->GetSpendRowsForDay(1);
	const FFF_LedgerRow* HireRow =
		FFF_LedgerRowsTestHelpers::FindRow(SpendRows, TEXT("StaffHire"));

	if (!TestNotNull(TEXT("고용비 줄을 찾는다"), HireRow))
	{
		return false;
	}

	// ReasonId 는 **짧은 이름**이다. "EFF_SpendReason::StaffHire" 가 아니다.
	TestEqual(TEXT("ReasonId 가 짧은 enum 이름이다"),
		HireRow->ReasonId.ToString(), FString(TEXT("StaffHire")));

	// DisplayName 은 UMETA(DisplayName) 한글이다.
	// [주의] UMETA 메타데이터는 에디터 전용이라 패키징 빌드에서는 폴백될 수 있다.
	//        이 테스트는 에디터 동작만 고정한다 (피날레 실패 이유도 같은 방식이다).
	TestEqual(TEXT("DisplayName 이 한글이다"),
		HireRow->DisplayName.ToString(), FString(TEXT("Staff 고용")));

	TestEqual(TEXT("금액이 실제 고용비다"), HireRow->Amount,
		HireCount * FFF_LedgerRowsTestHelpers::HireCost);

	return true;
}

// 부스별 표는 FacilityId 오름차순이다. UI 가 다시 정렬하지 않는다(백종태 님 지정).
// 🔴 TMap 순회 순서는 FName 등록 순서(= 에셋 로딩 순서)에 의존한다 — 2026-08-10 결정론
//    버그와 같은 종류다. 정렬을 강제하지 않으면 실행마다 표의 줄 순서가 바뀐다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LedgerRows_FacilityRowsAreSortedAscending, "FestaFiesta.Economy.LedgerRows.FacilityRowsAreSortedAscending", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LedgerRows_FacilityRowsAreSortedAscending::RunTest(const FString& Parameters)
{
	FFF_LedgerRowsTestHelpers::FLedgerRowsHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	// **역순으로** 기록한다.
	Harness.Progression->AddRevenue(30, EFF_EarnReason::FacilityCommission, TEXT("Booth_Zebra"));
	Harness.Progression->AddRevenue(10, EFF_EarnReason::FacilityCommission, TEXT("Booth_Apple"));
	Harness.Progression->AddRevenue(20, EFF_EarnReason::FacilityCommission, TEXT("Booth_Mango"));

	const TArray<FFF_FacilityRevenueRow> FacilityRows =
		Harness.Progression->GetRevenueByFacilityRowsForDay(1);

	TestEqual(TEXT("부스 3개가 줄이 된다"), FacilityRows.Num(), 3);
	TestEqual(TEXT("FacilityId 오름차순이다"),
		FFF_LedgerRowsTestHelpers::JoinFacilityIds(FacilityRows),
		FString(TEXT("Booth_Apple,Booth_Mango,Booth_Zebra")));

	// 정렬이 금액을 따라 움직였는지 확인 — 키만 정렬하고 값이 안 따라오면 표가 틀린다.
	TestEqual(TEXT("Apple 금액"), FacilityRows[0].Amount, 10);
	TestEqual(TEXT("Mango 금액"), FacilityRows[1].Amount, 20);
	TestEqual(TEXT("Zebra 금액"), FacilityRows[2].Amount, 30);

	// 같은 부스에 두 번 들어오면 합산된 한 줄이다.
	Harness.Progression->AddRevenue(5, EFF_EarnReason::FacilityCommission, TEXT("Booth_Apple"));
	const TArray<FFF_FacilityRevenueRow> AfterSecondSale =
		Harness.Progression->GetRevenueByFacilityRowsForDay(1);
	TestEqual(TEXT("부스 수가 늘지 않는다"), AfterSecondSale.Num(), 3);
	TestEqual(TEXT("같은 부스 매출이 합산된다"), AfterSecondSale[0].Amount, 15);

	// FacilityId 없는 매출(입장료)은 부스별 표에 안 들어간다.
	Harness.Progression->AddRevenue(999, EFF_EarnReason::AdmissionFee);
	TestEqual(TEXT("FacilityId 없는 매출은 부스별 표에 없다"),
		Harness.Progression->GetRevenueByFacilityRowsForDay(1).Num(), 3);

	return true;
}

// 철거 환급은 매출이 아니다 (2026-08-13 규칙). 별도 창구로만 나간다.
// EarnRows 에 섞이면 종태님 화면이 "매출이 늘었다"로 잘못 읽는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LedgerRows_RefundIsSeparatedFromEarnRows, "FestaFiesta.Economy.LedgerRows.RefundIsSeparatedFromEarnRows", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LedgerRows_RefundIsSeparatedFromEarnRows::RunTest(const FString& Parameters)
{
	FFF_LedgerRowsTestHelpers::FLedgerRowsHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	Harness.Progression->AddRevenue(1000, EFF_EarnReason::AdmissionFee);
	Harness.Progression->Refund(500);

	const TArray<FFF_LedgerRow> EarnRows = Harness.Progression->GetEarnRowsForDay(1);
	TestEqual(TEXT("매출 줄은 입장료 하나뿐이다"), EarnRows.Num(), 1);
	TestEqual(TEXT("매출 금액에 환급이 안 섞였다"), EarnRows[0].Amount, 1000);

	TestEqual(TEXT("환급은 별도 창구로 나온다"),
		Harness.Progression->GetTotalRefundForDay(1), 500);

	// 환급은 지출 표에도 없다 — 이미 쓴 돈이 되돌아온 것이라 새 지출이 아니다.
	for (const FFF_LedgerRow& Row : Harness.Progression->GetSpendRowsForDay(1))
	{
		TestNotEqual(TEXT("환급이 지출 줄로 새지 않는다"), Row.Amount, 500);
	}

	return true;
}

// 🔴 이 API 의 핵심: 날짜 하나로 라이브와 스냅샷을 알아서 고른다.
// 호출부는 "어느 원장인가"를 몰라도 되고, 그래서 어제 고정한 세 함정을 외울 필요가 없다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LedgerRows_DayResolvesLiveAndSnapshot, "FestaFiesta.Economy.LedgerRows.DayResolvesLiveAndSnapshot", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LedgerRows_DayResolvesLiveAndSnapshot::RunTest(const FString& Parameters)
{
	FFF_LedgerRowsTestHelpers::FLedgerRowsHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	// --- Day1 Report (경계 넘기 전) — Day1 요청이 라이브로 간다 ---
	Harness.Progression->AddRevenue(1200, EFF_EarnReason::AdmissionFee);
	Harness.Progression->AddRevenue(300, EFF_EarnReason::FacilityCommission, TEXT("Booth_A"));

	TestTrue(TEXT("Day1 원장을 들고 있다"), Harness.Progression->HasLedgerForDay(1));
	{
		const TArray<FFF_LedgerRow> Rows = Harness.Progression->GetEarnRowsForDay(1);
		TestEqual(TEXT("Day1 매출 2줄"), Rows.Num(), 2);
		TestEqual(TEXT("Day1 입장료 금액"), Rows[0].Amount, 1200);
	}

	// --- 경계를 넘는다. Day1 은 스냅샷으로, Day2 는 라이브로 갈린다 ---
	Harness.Progression->BeginNewDay(2);

	TestTrue(TEXT("경계 후에도 Day1 을 들고 있다"), Harness.Progression->HasLedgerForDay(1));
	TestTrue(TEXT("Day2 도 들고 있다"), Harness.Progression->HasLedgerForDay(2));

	{
		// 같은 Day1 요청이 이제 스냅샷에서 온다. 호출부 코드는 한 글자도 안 바뀌었다.
		const TArray<FFF_LedgerRow> Day1Rows = Harness.Progression->GetEarnRowsForDay(1);
		TestEqual(TEXT("Day1 매출이 그대로 2줄"), Day1Rows.Num(), 2);
		TestEqual(TEXT("Day1 입장료가 보존된다"), Day1Rows[0].Amount, 1200);

		const TArray<FFF_FacilityRevenueRow> Day1Facility =
			Harness.Progression->GetRevenueByFacilityRowsForDay(1);
		TestEqual(TEXT("Day1 부스별도 보존된다"), Day1Facility.Num(), 1);
		TestEqual(TEXT("Day1 부스 금액"), Day1Facility[0].Amount, 300);
		TestEqual(TEXT("Day1 부스 식별자"), Day1Facility[0].FacilityId.ToString(),
			FString(TEXT("Booth_A")));
	}

	{
		// Day2 는 방금 열렸으니 매출도 지출도 없다. 하루 경계는 아무것도 걷지 않는다.
		const TArray<FFF_LedgerRow> Day2Earn = Harness.Progression->GetEarnRowsForDay(2);
		TestEqual(TEXT("Day2 매출은 아직 없다"), Day2Earn.Num(), 0);

		const TArray<FFF_LedgerRow> Day2Spend = Harness.Progression->GetSpendRowsForDay(2);
		TestEqual(TEXT("Day2 지출도 아직 없다"), Day2Spend.Num(), 0);
	}

	// --- Day2 에 매출을 넣어도 Day1 스냅샷은 안 흔들린다 ---
	Harness.Progression->AddRevenue(777, EFF_EarnReason::MinigameReward);
	TestEqual(TEXT("Day1 은 여전히 2줄"), Harness.Progression->GetEarnRowsForDay(1).Num(), 2);
	TestEqual(TEXT("Day2 가 1줄이 된다"), Harness.Progression->GetEarnRowsForDay(2).Num(), 1);

	return true;
}

// 🔴 Day3 계약: 최종 결산은 라이브다.
// 어제 스냅샷만 있을 때는 UI 가 Day3 에서 Day2 표를 그릴 수 있었다.
// 날짜로 물으면 그 경로가 구조적으로 사라진다 — 이 테스트가 그걸 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LedgerRows_FinalDayReadsLiveLedger, "FestaFiesta.Economy.LedgerRows.FinalDayReadsLiveLedger", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LedgerRows_FinalDayReadsLiveLedger::RunTest(const FString& Parameters)
{
	FFF_LedgerRowsTestHelpers::FLedgerRowsHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	Harness.Progression->AddRevenue(100, EFF_EarnReason::AdmissionFee);
	Harness.Progression->BeginNewDay(2);
	Harness.Progression->AddRevenue(200, EFF_EarnReason::AdmissionFee);
	Harness.Progression->BeginNewDay(3);

	// Day3 은 뒤에 경계가 없다(AdvanceToNextDay 가 CurrentDay>=3 에서 early return).
	Harness.Progression->AddRevenue(300, EFF_EarnReason::AdmissionFee);

	// 스냅샷은 Day2 에 멈춰 있다.
	TestEqual(TEXT("닫힌 날은 Day2 다"), Harness.Progression->GetEndedDay(), 2);

	// 그런데 Day3 을 물으면 라이브가 온다 — 함정이 발동하지 않는다.
	{
		const TArray<FFF_LedgerRow> Day3Rows = Harness.Progression->GetEarnRowsForDay(3);
		TestEqual(TEXT("Day3 매출 1줄"), Day3Rows.Num(), 1);
		TestEqual(TEXT("Day3 금액이 300 이다 (Day2 의 200 이 아니다)"), Day3Rows[0].Amount, 300);
	}

	// Day2 를 물으면 스냅샷이 온다.
	{
		const TArray<FFF_LedgerRow> Day2Rows = Harness.Progression->GetEarnRowsForDay(2);
		TestEqual(TEXT("Day2 금액이 200 이다"), Day2Rows[0].Amount, 200);
	}

	return true;
}

// 안 들고 있는 날짜는 빈 표 + HasLedgerForDay == false 다.
// "그 날 아무 일도 없었다(0)"와 "그 날을 안 들고 있다"는 다른 상태이고,
// 구분 수단이 HasLedgerForDay 하나뿐이다 (NeedMetrics 의 GetSampleCount() == 0 과 같은 원칙).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LedgerRows_UnheldDayIsDistinguishableFromZero, "FestaFiesta.Economy.LedgerRows.UnheldDayIsDistinguishableFromZero", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LedgerRows_UnheldDayIsDistinguishableFromZero::RunTest(const FString& Parameters)
{
	FFF_LedgerRowsTestHelpers::FLedgerRowsHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000)))
	{
		return false;
	}

	// 끝난 날의 원장은 모두 보관한다(최종 결산이 3일 합계를 내려고). 아직 안 일어난 Day4 는 없다.
	Harness.Progression->AddRevenue(100, EFF_EarnReason::AdmissionFee);
	Harness.Progression->BeginNewDay(2);
	Harness.Progression->BeginNewDay(3);

	TestTrue(TEXT("Day1 도 계속 들고 있다"), Harness.Progression->HasLedgerForDay(1));
	TestTrue(TEXT("Day2 는 스냅샷으로 들고 있다"), Harness.Progression->HasLedgerForDay(2));
	TestTrue(TEXT("Day3 은 라이브로 들고 있다"), Harness.Progression->HasLedgerForDay(3));

	// 안 들고 있는 날(Day4)은 빈 표를 준다 (크래시하지 않는다). Day1 은 100 Gold 입장료 한 줄이 남아 있다.
	TestEqual(TEXT("Day1 매출 표에 입장료 한 줄이 남아 있다"),
		Harness.Progression->GetEarnRowsForDay(1).Num(), 1);
	TestEqual(TEXT("Day1 지출 표는 비어 있다"),
		Harness.Progression->GetSpendRowsForDay(1).Num(), 0);
	TestEqual(TEXT("Day1 부스별 표가 비어 있다"),
		Harness.Progression->GetRevenueByFacilityRowsForDay(1).Num(), 0);
	TestEqual(TEXT("Day1 환급이 0 이다"), Harness.Progression->GetTotalRefundForDay(1), 0);

	// 🔴 Day2 는 들고 있지만 그 날 매출이 실제로 0 이다.
	// 표가 비어 있는 모양이 Day1 과 같으므로 HasLedgerForDay 로만 갈린다.
	TestEqual(TEXT("Day2 도 매출 표가 비어 있다"),
		Harness.Progression->GetEarnRowsForDay(2).Num(), 0);
	TestTrue(TEXT("그래도 Day2 는 들고 있다 — 표 모양으로는 구분이 안 된다"),
		Harness.Progression->HasLedgerForDay(2));

	// 범위를 벗어난 날짜도 조용히 빈 표다.
	TestFalse(TEXT("Day0 은 없다"), Harness.Progression->HasLedgerForDay(0));
	TestFalse(TEXT("음수 날짜도 없다"), Harness.Progression->HasLedgerForDay(-1));
	TestFalse(TEXT("Day4 는 아직 없다"), Harness.Progression->HasLedgerForDay(4));
	TestEqual(TEXT("Day0 표가 비어 있다"),
		Harness.Progression->GetEarnRowsForDay(0).Num(), 0);

	return true;
}

// 축제가 시작되지 않았으면 어떤 날짜도 들고 있지 않다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LedgerRows_NoLedgerBeforeFestivalStarts, "FestaFiesta.Economy.LedgerRows.NoLedgerBeforeFestivalStarts", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LedgerRows_NoLedgerBeforeFestivalStarts::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UFF_FestivalProgressionSubsystem* Progression =
		NewObject<UFF_FestivalProgressionSubsystem>(GameInstance);

	if (!TestNotNull(TEXT("Progression 이 만들어진다"), Progression))
	{
		return false;
	}

	TestFalse(TEXT("Day1 도 들고 있지 않다"), Progression->HasLedgerForDay(1));
	TestEqual(TEXT("매출 표가 비어 있다"), Progression->GetEarnRowsForDay(1).Num(), 0);
	TestEqual(TEXT("지출 표가 비어 있다"), Progression->GetSpendRowsForDay(1).Num(), 0);
	TestEqual(TEXT("환급이 0 이다"), Progression->GetTotalRefundForDay(1), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
