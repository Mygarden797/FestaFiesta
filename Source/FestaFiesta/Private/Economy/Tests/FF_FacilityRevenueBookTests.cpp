// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/Rules/FF_FacilityRevenueBook.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"

/**
 * 설치 시설별(인스턴스) 매출 장부 검증.
 *
 * 앞쪽은 순수 값 타입 규칙이고, 뒤쪽은 UFF_FestivalProgressionSubsystem 이 실제로 이 장부를
 * 원장·자금과 같은 호출에서 채우는지 본다. 월드를 만들지 않는 이유는 FF_DayBoundaryTests.cpp 와 같다.
 */
namespace FFF_FacilityRevenueBookTestHelpers
{
	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	struct FFacilityRevenueHarness
	{
		bool Initialize()
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			Progression = GameInstance ? NewObject<UFF_FestivalProgressionSubsystem>(GameInstance) : nullptr;
			Definition = GameInstance ? NewObject<UFF_FestivalDefinition>(GameInstance) : nullptr;
			CostTable = GameInstance ? NewObject<UDataTable>(GameInstance) : nullptr;
			if (!Progression || !Definition || !CostTable)
			{
				return false;
			}

			// 표가 비면 IsDefinitionValid 가 Error 를 찍고 자동화 프레임워크는 Error 를 실패로 본다.
			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
			Definition->FacilityCostTable = CostTable;
			Definition->FestivalId = TEXT("FacilityRevenueAutomationFestival");
			Definition->InitialGold = 10000;
			// 이 테스트는 매출 귀속만 본다. 임대료가 걷히면 잔액 기대값에 섞인다.
			Definition->DailyVenueRentGold = 0;

			Progression->StartFestival(Definition);
			return true;
		}

		UGameInstance* GameInstance = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
	};
}

// 같은 시설은 누적되고, 다른 시설과 섞이지 않으며, 결제 건수도 같이 센다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueBook_RecordAccumulatesPerInstance, "FestaFiesta.Economy.FacilityRevenueBook.RecordAccumulatesPerInstance", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityRevenueBook_RecordAccumulatesPerInstance::RunTest(const FString& Parameters)
{
	FFF_FacilityRevenueBook Book;
	const FGuid FirstStall = FGuid::NewGuid();
	const FGuid SecondStall = FGuid::NewGuid();

	TestTrue(TEXT("첫 결제를 기록한다"), Book.Record(FirstStall, 48));
	TestTrue(TEXT("같은 시설의 두 번째 결제를 기록한다"), Book.Record(FirstStall, 52));
	TestTrue(TEXT("다른 시설의 결제를 기록한다"), Book.Record(SecondStall, 60));

	FFF_FacilityInstanceRevenue First;
	FFF_FacilityInstanceRevenue Second;
	TestTrue(TEXT("첫 시설을 찾는다"), Book.Find(FirstStall, First));
	TestTrue(TEXT("둘째 시설을 찾는다"), Book.Find(SecondStall, Second));
	TestEqual(TEXT("같은 시설은 오늘 매출이 누적된다"), First.TodayGold, 100);
	TestEqual(TEXT("같은 시설은 누적 매출도 누적된다"), First.TotalGold, 100);
	TestEqual(TEXT("같은 시설의 오늘 결제 건수"), First.TodayPaymentCount, 2);
	TestEqual(TEXT("같은 시설의 누적 결제 건수"), First.TotalPaymentCount, 2);
	TestEqual(TEXT("다른 시설은 따로 쌓인다"), Second.TotalGold, 60);
	TestEqual(TEXT("다른 시설의 결제 건수"), Second.TotalPaymentCount, 1);
	TestEqual(TEXT("시설 두 곳만 장부에 있다"), Book.Num(), 2);

	return true;
}

// 무효 GUID·0 이하 금액은 장부를 바꾸지 않는다. 종류별 표만 있는 매출(입장료 등)이 여기로 새면 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueBook_IgnoresInvalidRecords, "FestaFiesta.Economy.FacilityRevenueBook.IgnoresInvalidRecords", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityRevenueBook_IgnoresInvalidRecords::RunTest(const FString& Parameters)
{
	FFF_FacilityRevenueBook Book;
	const FGuid Stall = FGuid::NewGuid();

	TestFalse(TEXT("무효 GUID 는 기록하지 않는다"), Book.Record(FGuid(), 100));
	TestFalse(TEXT("0 은 기록하지 않는다"), Book.Record(Stall, 0));
	TestFalse(TEXT("음수는 기록하지 않는다"), Book.Record(Stall, -5));
	TestEqual(TEXT("장부가 비어 있다"), Book.Num(), 0);

	FFF_FacilityInstanceRevenue Missing;
	Missing.TotalGold = 999;
	TestFalse(TEXT("기록 없는 시설은 false 다"), Book.Find(Stall, Missing));
	TestEqual(TEXT("기록 없는 시설의 출력은 0 으로 초기화된다"), Missing.TotalGold, 0);

	TestTrue(TEXT("상한 근처 값을 기록한다"), Book.Record(Stall, MAX_int32 - 1));
	TestTrue(TEXT("상한을 넘는 값을 기록한다"), Book.Record(Stall, 10));
	FFF_FacilityInstanceRevenue Saturated;
	Book.Find(Stall, Saturated);
	TestEqual(TEXT("누적은 int32 상한에서 포화한다"), Saturated.TotalGold, MAX_int32);
	TestEqual(TEXT("포화해도 결제 건수는 센다"), Saturated.TotalPaymentCount, 2);

	return true;
}

// 하루 경계는 오늘 값만 지우고 누적은 남긴다. 새 축제는 전부 지운다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueBook_DayBoundaryKeepsTotals, "FestaFiesta.Economy.FacilityRevenueBook.DayBoundaryKeepsTotals", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityRevenueBook_DayBoundaryKeepsTotals::RunTest(const FString& Parameters)
{
	FFF_FacilityRevenueBook Book;
	const FGuid Stall = FGuid::NewGuid();
	Book.Record(Stall, 300);
	Book.Record(Stall, 200);

	Book.BeginNewDay();
	FFF_FacilityInstanceRevenue AfterBoundary;
	TestTrue(TEXT("경계를 넘어도 시설은 장부에 남는다"), Book.Find(Stall, AfterBoundary));
	TestEqual(TEXT("오늘 매출은 0 으로 돌아간다"), AfterBoundary.TodayGold, 0);
	TestEqual(TEXT("오늘 결제 건수도 0 으로 돌아간다"), AfterBoundary.TodayPaymentCount, 0);
	TestEqual(TEXT("누적 매출은 남는다"), AfterBoundary.TotalGold, 500);
	TestEqual(TEXT("누적 결제 건수도 남는다"), AfterBoundary.TotalPaymentCount, 2);

	Book.Record(Stall, 50);
	FFF_FacilityInstanceRevenue NextDay;
	Book.Find(Stall, NextDay);
	TestEqual(TEXT("다음 날 매출은 오늘 값부터 다시 센다"), NextDay.TodayGold, 50);
	TestEqual(TEXT("다음 날 매출도 누적에 더해진다"), NextDay.TotalGold, 550);

	Book.Reset();
	TestEqual(TEXT("새 축제는 장부를 비운다"), Book.Num(), 0);

	return true;
}

// 경제 서브시스템이 원장·자금과 같은 호출에서 인스턴스 장부를 채운다.
// 시설 핸들 없는 매출(입장료)은 인스턴스 장부에 들어가지 않고, 하루 경계·새 축제 규칙이 그대로 적용된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityRevenueBook_ProgressionAttributesRevenueToInstance, "FestaFiesta.Economy.FacilityRevenueBook.ProgressionAttributesRevenueToInstance", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityRevenueBook_ProgressionAttributesRevenueToInstance::RunTest(const FString& Parameters)
{
	FFF_FacilityRevenueBookTestHelpers::FFacilityRevenueHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize()))
	{
		return false;
	}

	const FGuid Stall = FGuid::NewGuid();
	const FName StallDefinition(TEXT("Booth_FoodStall_RevenueBookAutomation"));

	FFF_FacilityInstanceRevenue Revenue;
	TestFalse(TEXT("결제 전에는 인스턴스 매출이 없다"),
		Harness.Progression->GetFacilityInstanceRevenue(Stall, Revenue));

	Harness.Progression->AddFacilityRevenue(120, EFF_EarnReason::FacilityCommission, StallDefinition, Stall);
	Harness.Progression->AddRevenue(2500, EFF_EarnReason::AdmissionFee);

	TestTrue(TEXT("시설 핸들이 붙은 매출은 인스턴스 장부에 들어간다"),
		Harness.Progression->GetFacilityInstanceRevenue(Stall, Revenue));
	TestEqual(TEXT("인스턴스 오늘 매출"), Revenue.TodayGold, 120);
	TestEqual(TEXT("인스턴스 누적 매출"), Revenue.TotalGold, 120);
	TestEqual(TEXT("인스턴스 결제 건수"), Revenue.TodayPaymentCount, 1);
	TestEqual(TEXT("종류별 부스 표에도 같은 금액이 들어간다"),
		Harness.Progression->GetDayLedger().GetRevenueByFacility().FindRef(StallDefinition), 120);
	TestEqual(TEXT("오늘 매출 합계는 입장료를 포함한다 — 인스턴스 장부는 합계를 대신하지 않는다"),
		Harness.Progression->GetTodayRevenue(), 2620);
	TestEqual(TEXT("잔액도 같은 호출에서 늘어난다"), Harness.Progression->GetGold(), 12620);

	Harness.Progression->BeginNewDay(2);
	TestTrue(TEXT("하루 경계를 넘어도 시설 기록은 남는다"),
		Harness.Progression->GetFacilityInstanceRevenue(Stall, Revenue));
	TestEqual(TEXT("하루 경계에서 오늘 매출은 0 이 된다"), Revenue.TodayGold, 0);
	TestEqual(TEXT("하루 경계에서 누적 매출은 남는다"), Revenue.TotalGold, 120);

	Harness.Progression->StartFestival(Harness.Definition);
	TestFalse(TEXT("새 축제는 인스턴스 장부를 비운다"),
		Harness.Progression->GetFacilityInstanceRevenue(Stall, Revenue));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
