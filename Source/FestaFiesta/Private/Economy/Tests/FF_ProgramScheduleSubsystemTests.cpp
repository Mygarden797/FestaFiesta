// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"

/**
 * 프로그램 편성이 Gold·예약 슬롯을 한 트랜잭션으로 움직이는지 검증한다 (S8 2·3단계).
 *
 * 판정 규칙은 FF_ProgramScheduleRulesTests.cpp 가, 배정 DP 는
 * FF_ProgramReservationSubsystemTests.cpp 가 갖는다. 여기는 **셋이 실제로 이어지는가**를 본다.
 *
 * 월드를 만들지 않는다. 후보 그룹은 브리지가 crowd 에서 모아 넘기는 값이라 직접 만들어 넣는다.
 */
namespace FFF_ProgramScheduleSubsystemTestHelpers
{
	const FName Workshop(TEXT("Program_SandArtWorkshop"));
	const FName Yoga(TEXT("Program_SunsetYoga"));

	constexpr int32 BaseStaff = 6;

	struct FProgramHarness
	{
		bool Initialize(int32 InitialGold)
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			if (GameInstance == nullptr) return false;

			Progression = NewObject<UFF_FestivalProgressionSubsystem>(GameInstance);
			Reservations = NewObject<UFF_ProgramReservationSubsystem>(GameInstance);
			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			CostTable = NewObject<UDataTable>(GameInstance);
			if (Progression == nullptr || Reservations == nullptr
				|| Definition == nullptr || CostTable == nullptr)
			{
				return false;
			}

			// 표가 비면 IsDefinitionValid 가 Error 를 찍고 자동화가 그것을 실패로 취급한다.
			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
			Definition->FacilityCostTable = CostTable;

			Definition->FestivalId = TEXT("ProgramScheduleAutomationFestival");
			Definition->InitialGold = InitialGold;
			Definition->InitialStaffCount = BaseStaff;

			// 프로덕션은 Initialize 가 물린다. 여기서는 서브시스템 컬렉션이 없으므로 직접 준다.
			Progression->SetProgramReservations(Reservations);

			// 카탈로그는 생성자 기본값(공방·요가)을 쓴다 — 저작 값이 바뀌면 같이 깨지는 것이 맞다.
			Progression->StartFestival(Definition);

			WorkshopFacility = FGuid::NewGuid();
			YogaFacility = FGuid::NewGuid();
			return true;
		}

		// 🔴 값으로 복사해서 돌려준다. `GetProgramCatalog()` 는 배열을 값으로 주므로
		// 그 안을 가리키는 포인터를 돌려주면 함수가 끝나는 순간 죽은 메모리를 가리킨다.
		// 단독 실행에서는 해제된 값이 그대로 남아 통과하고, 전체 스위트에서는 재사용된
		// 메모리를 읽어 기대값이 0 이 된다 — 테스트가 실행 순서에 따라 갈린다.
		bool FindCard(FName ProgramId, FFF_ProgramCard& OutCard) const
		{
			const TArray<FFF_ProgramCard> Catalog = Progression->GetProgramCatalog();
			const FFF_ProgramCard* Found = Catalog.FindByPredicate(
				[ProgramId](const FFF_ProgramCard& Card) { return Card.ProgramId == ProgramId; });
			if (Found == nullptr)
			{
				return false;
			}
			OutCard = *Found;
			return true;
		}

		EFF_ProgramPurchaseResult Schedule(FName ProgramId)
		{
			return Progression->TryScheduleProgram(
				ProgramId, ProgramId == Workshop ? WorkshopFacility : YogaFacility);
		}

		bool FindReservationFor(int32 Index, FFF_ProgramReservation& Out) const
		{
			const TArray<FFF_ScheduledProgram> Scheduled = Progression->GetScheduledPrograms();
			return Scheduled.IsValidIndex(Index)
				&& Reservations->FindReservation(Scheduled[Index].ReservationId, Out);
		}

		/**
		 * 스폰이 예약 그룹을 데려온 것을 흉내낸다. 주문서를 그대로 확정으로 돌린다 —
		 * 프로덕션에서 crowdAI 가 `ConfirmReservedParty` 로 하는 일이다.
		 *
		 * @param HeadcountLimit 이만큼만 확정한다. 스폰이 정원을 못 채운 상황을 만들 때 쓴다
		 */
		int32 SpawnReservedParties(int32 HeadcountLimit = MAX_int32)
		{
			int32 Confirmed = 0;
			int32 NextPartyId = 1;
			for (const FFF_ReservedPartyRequest& Request : Progression->GetReservedPartyRequests())
			{
				if (Confirmed + Request.GroupSize > HeadcountLimit)
				{
					continue;
				}
				if (Reservations->ConfirmReservedParty(
					Request.ReservationId, NextPartyId++, Request.GroupSize))
				{
					Confirmed += Request.GroupSize;
				}
			}
			return Confirmed;
		}

		UGameInstance* GameInstance = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_ProgramReservationSubsystem* Reservations = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
		FGuid WorkshopFacility;
		FGuid YogaFacility;
	};
}

// 편성하면 Gold 가 한 번 빠지고, **예약 슬롯이 생긴다.** 인원은 건드리지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_SchedulingOpensReservation, "FestaFiesta.Economy.ProgramSchedule.SchedulingOpensReservation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_SchedulingOpensReservation::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	FProgramHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000))) return false;

	FFF_ProgramCard Card;
	if (!TestTrue(TEXT("공방 카드가 카탈로그에 있다"), Harness.FindCard(Workshop, Card))) return false;

	TestEqual(TEXT("편성된다"), Harness.Schedule(Workshop),
		EFF_ProgramPurchaseResult::Success);

	TestEqual(TEXT("비용이 한 번 빠진다"),
		Harness.Progression->GetGold(), 10000 - Card.CostGold);
	// ★ 프로그램은 인원을 쓰지 않는다. 편성해도 Staff 총원이 그대로여야 한다.
	TestEqual(TEXT("편성해도 인원이 줄지 않는다"),
		Harness.Progression->GetTotalStaffCount(), BaseStaff);
	TestEqual(TEXT("원장에 프로그램 구매로 기록된다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::ProgramPurchase),
		Card.CostGold);

	// 🔴 예약 슬롯이 실제로 생겼는가. 없으면 배정·잠금·퇴장 금지가 붙을 자리가 없다.
	const TArray<FFF_ScheduledProgram> Scheduled = Harness.Progression->GetScheduledPrograms();
	if (!TestEqual(TEXT("편성 목록에 한 건 들어간다"), Scheduled.Num(), 1)) return false;
	TestEqual(TEXT("카드 id 가 유지된다"), Scheduled[0].ProgramId, Workshop);
	TestTrue(TEXT("예약 핸들이 유효하다"), Scheduled[0].ReservationId.IsValid());

	FFF_ProgramReservation Reservation;
	if (!TestTrue(TEXT("예약 슬롯을 조회할 수 있다"),
		Harness.FindReservationFor(0, Reservation))) return false;

	TestEqual(TEXT("정원이 카드에서 온다"), Reservation.Capacity, Card.Capacity);
	TestEqual(TEXT("시작 시각이 카드에서 온다"), Reservation.StartMinutes, Card.StartMinutes);
	TestEqual(TEXT("종료 시각이 카드에서 온다"), Reservation.EndMinutes, Card.EndMinutes);
	TestEqual(TEXT("시설이 편성 때 준 것과 같다"),
		Reservation.FacilityInstanceId, Harness.WorkshopFacility);

	return true;
}

// 실패하면 상태가 하나도 안 바뀐다. 슬롯도 안 생긴다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_FailedScheduleChangesNothing, "FestaFiesta.Economy.ProgramSchedule.FailedScheduleChangesNothing", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_FailedScheduleChangesNothing::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	// 공방(200) 도 못 살 잔액.
	FProgramHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(100))) return false;

	TestEqual(TEXT("잔액이 모자라 거절된다"), Harness.Schedule(Workshop),
		EFF_ProgramPurchaseResult::InsufficientGold);

	TestEqual(TEXT("잔액이 그대로다"), Harness.Progression->GetGold(), 100);
	TestEqual(TEXT("편성 목록이 비어 있다"),
		Harness.Progression->GetScheduledPrograms().Num(), 0);
	TestEqual(TEXT("원장에 지출이 안 남는다"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::ProgramPurchase), 0);

	// 🔴 예약 슬롯도 안 생긴다. 돈을 안 받았는데 프로그램이 열려 있으면 안 된다.
	TestEqual(TEXT("예약 슬롯이 생기지 않는다"),
		Harness.Reservations->GetReservationIds().Num(), 0);

	TestEqual(TEXT("없는 카드는 거절된다"),
		Harness.Progression->TryScheduleProgram(TEXT("Program_Nope"), Harness.WorkshopFacility),
		EFF_ProgramPurchaseResult::UnknownProgram);
	TestEqual(TEXT("여전히 잔액이 그대로다"), Harness.Progression->GetGold(), 100);

	return true;
}

// 시설 핸들이 무효하면 편성이 거절되고 Gold 도 안 빠진다.
// 슬롯 없이 차감만 되면 돈은 나갔는데 아무도 배정되지 않는 상태가 남는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_InvalidFacilityDoesNotSpend, "FestaFiesta.Economy.ProgramSchedule.InvalidFacilityDoesNotSpend", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_InvalidFacilityDoesNotSpend::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	FProgramHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000))) return false;

	TestEqual(TEXT("무효한 시설이면 거절된다"),
		Harness.Progression->TryScheduleProgram(Workshop, FGuid()),
		EFF_ProgramPurchaseResult::UnknownProgram);
	TestEqual(TEXT("잔액이 그대로다"), Harness.Progression->GetGold(), 10000);
	TestEqual(TEXT("편성 목록이 비어 있다"),
		Harness.Progression->GetScheduledPrograms().Num(), 0);
	TestEqual(TEXT("예약 슬롯도 없다"), Harness.Reservations->GetReservationIds().Num(), 0);

	return true;
}

// ★ 몇 개를 열든 인원은 그대로다. 편성을 막는 것은 잔액과 하루 중복뿐이다 —
// 두 축(시설=버는 축, Staff=지키는 축)이 갈라져 있다는 사실을 소유자 쪽에서 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_SchedulingLeavesStaffUntouched, "FestaFiesta.Economy.ProgramSchedule.SchedulingLeavesStaffUntouched", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_SchedulingLeavesStaffUntouched::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	FProgramHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000))) return false;

	FFF_ProgramCard WorkshopCard;
	FFF_ProgramCard YogaCard;
	if (!TestTrue(TEXT("공방 카드가 있다"), Harness.FindCard(Workshop, WorkshopCard))
		|| !TestTrue(TEXT("요가 카드가 있다"), Harness.FindCard(Yoga, YogaCard)))
	{
		return false;
	}

	TestEqual(TEXT("공방이 편성된다"), Harness.Schedule(Workshop),
		EFF_ProgramPurchaseResult::Success);
	TestEqual(TEXT("요가도 편성된다"), Harness.Schedule(Yoga),
		EFF_ProgramPurchaseResult::Success);

	// 둘을 열어도 Staff 는 한 명도 안 묶인다. 편성의 대가는 Gold 뿐이다.
	TestEqual(TEXT("둘을 열어도 인원이 그대로다"),
		Harness.Progression->GetTotalStaffCount(), BaseStaff);
	TestEqual(TEXT("예약 슬롯이 둘이다"), Harness.Reservations->GetReservationIds().Num(), 2);

	TestEqual(TEXT("같은 카드는 하루에 한 번만"), Harness.Schedule(Workshop),
		EFF_ProgramPurchaseResult::AlreadyScheduled);

	return true;
}

// CanScheduleProgram 은 상태를 바꾸지 않는다. 버튼 활성화에 쓰이므로 매 프레임 불릴 수 있다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_CanScheduleDoesNotMutate, "FestaFiesta.Economy.ProgramSchedule.CanScheduleDoesNotMutate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_CanScheduleDoesNotMutate::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	FProgramHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000))) return false;

	for (int32 Index = 0; Index < 5; ++Index)
	{
		TestEqual(TEXT("판정이 계속 같다"),
			Harness.Progression->CanScheduleProgram(Workshop),
			EFF_ProgramPurchaseResult::Success);
	}

	TestEqual(TEXT("잔액이 안 바뀐다"), Harness.Progression->GetGold(), 10000);
	TestEqual(TEXT("예약 슬롯이 안 생긴다"), Harness.Reservations->GetReservationIds().Num(), 0);

	Harness.Schedule(Workshop);
	TestEqual(TEXT("편성 뒤에는 중복으로 바뀐다"),
		Harness.Progression->CanScheduleProgram(Workshop),
		EFF_ProgramPurchaseResult::AlreadyScheduled);

	return true;
}

// 🔴 편성은 하루짜리다. 하루 경계에서 편성과 **예약 슬롯이 같이** 비워진다.
// 슬롯이 남으면 오늘 스폰된 파티가 어제 시각에 잠기거나 퇴장을 막힌다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_ScheduleAndSlotsClearAtDayBoundary, "FestaFiesta.Economy.ProgramSchedule.ScheduleAndSlotsClearAtDayBoundary", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_ScheduleAndSlotsClearAtDayBoundary::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	FProgramHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000))) return false;

	Harness.Schedule(Workshop);
	Harness.Schedule(Yoga);
	TestEqual(TEXT("이틀째 전에는 둘 다 편성돼 있다"),
		Harness.Progression->GetScheduledPrograms().Num(), 2);
	TestEqual(TEXT("예약 슬롯도 둘이다"), Harness.Reservations->GetReservationIds().Num(), 2);

	Harness.Progression->BeginNewDay(2);

	TestEqual(TEXT("편성이 비워진다"),
		Harness.Progression->GetScheduledPrograms().Num(), 0);
	TestEqual(TEXT("예약 슬롯도 같이 지워진다"),
		Harness.Reservations->GetReservationIds().Num(), 0);

	// 어제 산 것이 오늘 또 막지 않는다 — 매일 다시 사는 것이 이 기능의 리듬이다.
	TestEqual(TEXT("같은 카드를 오늘 다시 편성할 수 있다"), Harness.Schedule(Workshop),
		EFF_ProgramPurchaseResult::Success);

	return true;
}

// 새 축제를 시작하면 편성도 슬롯도 남아 있지 않다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_RestartClearsSchedule, "FestaFiesta.Economy.ProgramSchedule.RestartClearsSchedule", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_RestartClearsSchedule::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	FProgramHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000))) return false;

	Harness.Schedule(Workshop);
	TestEqual(TEXT("편성돼 있다"), Harness.Progression->GetScheduledPrograms().Num(), 1);

	Harness.Progression->StartFestival(Harness.Definition);

	TestEqual(TEXT("새 판에는 편성이 없다"),
		Harness.Progression->GetScheduledPrograms().Num(), 0);
	TestEqual(TEXT("새 판에는 슬롯도 없다"),
		Harness.Reservations->GetReservationIds().Num(), 0);
	return true;
}

// 축제가 시작되지 않았으면 카탈로그가 비고 편성도 정산도 아무 일이 없다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_BeforeFestivalStartsNothingWorks, "FestaFiesta.Economy.ProgramSchedule.BeforeFestivalStartsNothingWorks", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_BeforeFestivalStartsNothingWorks::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UFF_FestivalProgressionSubsystem* Progression =
		NewObject<UFF_FestivalProgressionSubsystem>(GameInstance);
	if (!TestNotNull(TEXT("서브시스템이 만들어진다"), Progression)) return false;

	TestEqual(TEXT("카탈로그가 비어 있다"), Progression->GetProgramCatalog().Num(), 0);
	TestEqual(TEXT("편성 판정이 축제 미시작이다"),
		Progression->CanScheduleProgram(Workshop),
		EFF_ProgramPurchaseResult::FestivalNotStarted);
	TestEqual(TEXT("편성 시도도 축제 미시작이다"),
		Progression->TryScheduleProgram(Workshop, FGuid::NewGuid()),
		EFF_ProgramPurchaseResult::FestivalNotStarted);
	TestEqual(TEXT("시작 정산이 0 이다"), Progression->SettleStartedPrograms(240.f), 0);
	TestEqual(TEXT("안전망 정산도 0 이다"), Progression->SettleScheduledPrograms(), 0);

	return true;
}

// 🔴 정산 — 정원을 정확히 채우면 그만큼 체험비가 들어온다.
//
// 프로그램을 사면 정원만큼의 손님이 **추가로** 온다. 주문서 인원 합이 곧 정원이라
// 조합을 맞출 일이 없고, 그래서 플레이어가 산 정원만큼 받는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_FullHouseEarnsFullFee, "FestaFiesta.Economy.ProgramSchedule.FullHouseEarnsFullFee", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_FullHouseEarnsFullFee::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	FProgramHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000))) return false;

	FFF_ProgramCard Card;
	if (!TestTrue(TEXT("공방 카드가 있다"), Harness.FindCard(Workshop, Card))) return false;

	Harness.Schedule(Workshop);
	const int32 GoldAfterPurchase = Harness.Progression->GetGold();

	// 산 정원만큼 손님이 온다. 주문서 인원 합이 곧 정원이라 조합을 맞출 일이 없다.
	TestEqual(TEXT("주문한 인원이 정원과 같다"),
		Harness.SpawnReservedParties(), Card.Capacity);

	const int32 Earned = Harness.Progression->SettleStartedPrograms(Card.StartMinutes);

	TestEqual(TEXT("정원만큼 체험비가 들어온다"), Earned, Card.CalcMaxRevenue());
	TestEqual(TEXT("잔액이 그만큼 늘어난다"),
		Harness.Progression->GetGold(), GoldAfterPurchase + Earned);
	TestEqual(TEXT("원장에 체험비로 기록된다"),
		Harness.Progression->GetDayLedger().GetEarnByReason(EFF_EarnReason::ProgramFee), Earned);

	// 🔴 정원을 채우면 실제로 이익이 난다 — 비용을 넘겼다.
	TestTrue(TEXT("정원을 채우면 비용보다 많이 번다"), Earned > Card.CostGold);

	FFF_ProgramReservation Reservation;
	if (TestTrue(TEXT("예약을 조회한다"), Harness.FindReservationFor(0, Reservation)))
	{
		TestEqual(TEXT("빈자리가 없다"), Reservation.UnfilledSeats, 0);
		TestEqual(TEXT("배정 인원이 정원과 같다"),
			Reservation.AssignedHeadcount, Card.Capacity);
	}

	return true;
}

// 🔴 스폰이 정원을 못 채우면 그것은 **결함**이다 — 플레이 위험이 아니다.
//
// 그래도 계산은 온 사람 기준이어야 한다. 매출을 정원으로 고정해 버리면 스폰이 깨져도
// 원장이 멀쩡해 보여서 아무도 모른다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_UnderfilledSpawnIsVisible, "FestaFiesta.Economy.ProgramSchedule.UnderfilledSpawnIsVisible", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_UnderfilledSpawnIsVisible::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	FProgramHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000))) return false;

	FFF_ProgramCard Card;
	if (!TestTrue(TEXT("공방 카드가 있다"), Harness.FindCard(Workshop, Card))) return false;

	Harness.Schedule(Workshop);

	// 스폰이 6명만 데려온 상황을 만든다(풀 소진 등).
	const int32 Spawned = Harness.SpawnReservedParties(6);
	TestEqual(TEXT("6명만 확정됐다"), Spawned, 6);

	const int32 Earned = Harness.Progression->SettleStartedPrograms(Card.StartMinutes);

	TestEqual(TEXT("온 사람만큼만 받는다"), Earned, Spawned * Card.FeePerPerson);
	TestTrue(TEXT("정원을 채웠을 때보다 적다"), Earned < Card.CalcMaxRevenue());

	FFF_ProgramReservation Reservation;
	if (TestTrue(TEXT("예약을 조회한다"), Harness.FindReservationFor(0, Reservation)))
	{
		// 이 값이 0 이 아니면 스폰 배선이 깨진 것이다. 정산이 Warning 으로 올린다.
		TestEqual(TEXT("빈자리가 결함 신호로 남는다"),
			Reservation.UnfilledSeats, Card.Capacity - Spawned);
	}

	// 스폰이 아예 안 붙으면 0 이다. 배선 전 상태가 이것이라 조용히 통과하면 안 된다.
	FProgramHarness Empty;
	if (TestTrue(TEXT("두 번째 픽스처가 구성된다"), Empty.Initialize(10000)))
	{
		Empty.Schedule(Workshop);
		TestEqual(TEXT("아무도 안 오면 체험비가 0 이다"),
			Empty.Progression->SettleStartedPrograms(Card.StartMinutes), 0);

		// 0 원도 처리가 끝난 것이다. 안전망이 다시 집으면 뒤늦은 정산 Warning 이 헛것이 된다.
		TestEqual(TEXT("안전망이 그 카드를 다시 집지 않는다"),
			Empty.Progression->SettleScheduledPrograms(), 0);
	}

	return true;
}

// 🔴 프로그램을 둘 사면 손님도 둘 다 온다 — 나눠 갖지 않는다.
// 합이 두 정원의 합(28)이라는 것이 "프로그램 = 모객 수단"의 계산상 근거다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_EachProgramBringsItsOwnGuests, "FestaFiesta.Economy.ProgramSchedule.EachProgramBringsItsOwnGuests", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_EachProgramBringsItsOwnGuests::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	FProgramHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000))) return false;

	FFF_ProgramCard WorkshopCard;
	FFF_ProgramCard YogaCard;
	if (!TestTrue(TEXT("공방 카드가 있다"), Harness.FindCard(Workshop, WorkshopCard))
		|| !TestTrue(TEXT("요가 카드가 있다"), Harness.FindCard(Yoga, YogaCard)))
	{
		return false;
	}

	Harness.Schedule(Workshop);
	Harness.Schedule(Yoga);

	const int32 ExpectedTotal = WorkshopCard.Capacity + YogaCard.Capacity;
	TestEqual(TEXT("두 정원을 합한 만큼 손님이 온다"),
		Harness.SpawnReservedParties(), ExpectedTotal);

	int32 TotalAssigned = 0;
	TSet<int32> AllGroupIds;
	for (const FFF_ScheduledProgram& Scheduled : Harness.Progression->GetScheduledPrograms())
	{
		FFF_ProgramReservation Reservation;
		if (!Harness.Reservations->FindReservation(Scheduled.ReservationId, Reservation))
		{
			continue;
		}
		TestEqual(TEXT("각 프로그램이 정원을 다 채운다"),
			Reservation.UnfilledSeats, 0);
		TotalAssigned += Reservation.AssignedHeadcount;

		// 한 파티가 두 프로그램에 들어가면 잠금 구간이 겹쳐 판정할 시각이 없어진다.
		for (const int32 GroupId : Reservation.AssignedGroupIds)
		{
			TestFalse(TEXT("같은 그룹이 두 프로그램에 배정되지 않는다"),
				AllGroupIds.Contains(GroupId));
			AllGroupIds.Add(GroupId);
		}
	}

	TestEqual(TEXT("배정 인원 합이 두 정원의 합이다"), TotalAssigned, ExpectedTotal);

	const float AfterBothOpen = FMath::Max(WorkshopCard.StartMinutes, YogaCard.StartMinutes);
	TestEqual(TEXT("체험비도 두 프로그램 몫이 다 들어온다"),
		Harness.Progression->SettleStartedPrograms(AfterBothOpen),
		WorkshopCard.CalcMaxRevenue() + YogaCard.CalcMaxRevenue());

	return true;
}

// 🔴 체험비는 프로그램이 **열리는 시각**에 들어온다 — 마감이 아니다.
//
// 참가자는 스폰될 때 확정되므로 시작 시각이면 이미 확정값이다. 브리지가 Live 동안 매 초
// 부르는 함수라 "아직 안 열렸으면 0, 열리면 한 번, 그 뒤로는 다시 0" 이 성립해야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_FeeArrivesWhenProgramOpens, "FestaFiesta.Economy.ProgramSchedule.FeeArrivesWhenProgramOpens", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_FeeArrivesWhenProgramOpens::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	FProgramHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000))) return false;

	FFF_ProgramCard WorkshopCard;
	FFF_ProgramCard YogaCard;
	if (!TestTrue(TEXT("공방 카드가 있다"), Harness.FindCard(Workshop, WorkshopCard))
		|| !TestTrue(TEXT("요가 카드가 있다"), Harness.FindCard(Yoga, YogaCard)))
	{
		return false;
	}

	// 공방이 먼저 열리고 요가가 나중에 열린다. 이 순서가 이 테스트의 전제다.
	if (!TestTrue(TEXT("공방이 요가보다 먼저 열린다"),
		WorkshopCard.StartMinutes < YogaCard.StartMinutes)) return false;

	Harness.Schedule(Workshop);
	Harness.Schedule(Yoga);
	Harness.SpawnReservedParties();

	const int32 GoldBeforeAnyOpens = Harness.Progression->GetGold();

	// 개장 직후. 손님은 이미 와 있지만 아직 아무 프로그램도 안 열렸다.
	TestEqual(TEXT("아직 안 열렸으면 아무것도 안 걷는다"),
		Harness.Progression->SettleStartedPrograms(0.f), 0);
	TestEqual(TEXT("시작 1분 전에도 안 걷는다"),
		Harness.Progression->SettleStartedPrograms(WorkshopCard.StartMinutes - 1.f), 0);
	TestEqual(TEXT("잔액이 그대로다"), Harness.Progression->GetGold(), GoldBeforeAnyOpens);

	// 공방이 열린다. 요가는 아직이다.
	TestEqual(TEXT("공방이 열리면 공방 몫만 들어온다"),
		Harness.Progression->SettleStartedPrograms(WorkshopCard.StartMinutes),
		WorkshopCard.CalcMaxRevenue());

	// 🔴 브리지는 매 초 부른다. 두 번 걷히면 하루 매출이 프로그램 수만큼 부푼다.
	for (int32 Tick = 0; Tick < 5; ++Tick)
	{
		TestEqual(TEXT("같은 프로그램을 다시 걷지 않는다"),
			Harness.Progression->SettleStartedPrograms(WorkshopCard.StartMinutes + Tick), 0);
	}

	TestEqual(TEXT("요가가 열리면 요가 몫이 들어온다"),
		Harness.Progression->SettleStartedPrograms(YogaCard.StartMinutes),
		YogaCard.CalcMaxRevenue());

	TestEqual(TEXT("원장의 체험비 합계가 두 카드 몫이다"),
		Harness.Progression->GetDayLedger().GetEarnByReason(EFF_EarnReason::ProgramFee),
		WorkshopCard.CalcMaxRevenue() + YogaCard.CalcMaxRevenue());

	// 🔴 Live 중에 다 걷혔으면 마감 안전망은 할 일이 없다. 여기서 0 이 아니면 이중 정산이다.
	TestEqual(TEXT("마감 안전망이 아무것도 안 걷는다"),
		Harness.Progression->SettleScheduledPrograms(), 0);

	return true;
}

// 🔴 Closing 안전망 — Live 중에 시작 시각이 안 지나간 카드도 그날 매출로 들어간다.
//
// 카드 시각이 그날 Live 길이를 넘거나 Live 를 건너뛰면 시작 정산이 그 카드를 못 본다.
// 하루 경계까지 미루면 원장이 리셋된 뒤라 그날 매출로 안 들어가므로 Closing 이 마지막 기회다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramScheduleSub_ClosingCatchesProgramsThatNeverOpened, "FestaFiesta.Economy.ProgramSchedule.ClosingCatchesProgramsThatNeverOpened", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramScheduleSub_ClosingCatchesProgramsThatNeverOpened::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramScheduleSubsystemTestHelpers;

	FProgramHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize(10000))) return false;

	FFF_ProgramCard Card;
	if (!TestTrue(TEXT("공방 카드가 있다"), Harness.FindCard(Workshop, Card))) return false;

	Harness.Schedule(Workshop);
	Harness.SpawnReservedParties();

	// 시작 시각이 한 번도 안 지나간 채로 Live 가 끝났다.
	TestEqual(TEXT("시작 정산이 못 걷었다"),
		Harness.Progression->SettleStartedPrograms(Card.StartMinutes - 1.f), 0);

	TestEqual(TEXT("마감에 온 사람만큼 걷는다"),
		Harness.Progression->SettleScheduledPrograms(), Card.CalcMaxRevenue());
	TestEqual(TEXT("원장에는 똑같이 체험비로 남는다"),
		Harness.Progression->GetDayLedger().GetEarnByReason(EFF_EarnReason::ProgramFee),
		Card.CalcMaxRevenue());

	TestEqual(TEXT("안전망을 또 불러도 다시 걷지 않는다"),
		Harness.Progression->SettleScheduledPrograms(), 0);

	// 하루 경계가 편성을 지우므로 정산 여부도 같이 사라진다. 어제 걷은 것이 오늘을 막지 않는다.
	Harness.Progression->BeginNewDay(2);
	Harness.Schedule(Workshop);
	Harness.SpawnReservedParties();
	TestEqual(TEXT("오늘 산 카드는 오늘 다시 걷힌다"),
		Harness.Progression->SettleStartedPrograms(Card.StartMinutes),
		Card.CalcMaxRevenue());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
