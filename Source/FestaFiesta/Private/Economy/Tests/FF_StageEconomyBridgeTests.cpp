// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/FF_StageEconomyBridgeSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"

/**
 * 하루 경계의 **트리거**를 검증한다 (S6 스텝 5). [참고] economy_spec.md §5.1 S6
 *
 * 정산 결과 자체는 FF_DayBoundaryTests.cpp 가 갖는다. 여기는 "언제 부르는가"만 본다 —
 * 어느 전이가 하루를 열고, 어느 전이가 열지 않고, 같은 날을 두 번 열면 어떻게 되는가.
 *
 * **Stage 서브시스템을 세우지 않는다.** UFF_FestivalStageSubsystem 은 FTickableGameObject 라
 * NewObject 하는 순간 틱 목록에 들어가고, 표가 없는 상태로 틱하면 제멋대로 단계를 넘긴다.
 * ProcessStageEntry 가 날짜를 인자로 받는 덕분에 Stage 없이 진입만 재현할 수 있다.
 */
namespace FFF_StageEconomyBridgeTestHelpers
{
	// 2026-08-14 결정값 (E-23). 임금은 고정비가 아니라 하루 고정비는 임대료뿐이다.
	constexpr int32 DailyRent = 2400;
	constexpr int32 InitialGold = 10000;
	constexpr int32 BaseStaff = 6;

	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	// 같은 이유로 이 파일은 `using namespace` 를 쓰지 않는다 — 파일 스코프 using 은
	// 유니티 블롭 안의 남의 번역 단위까지 오염시킨다.
	struct FStageBridgeHarness
	{
		bool Initialize()
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			if (GameInstance == nullptr)
			{
				return false;
			}

			Progression = NewObject<UFF_FestivalProgressionSubsystem>(GameInstance);
			Bridge = NewObject<UFF_StageEconomyBridgeSubsystem>(GameInstance);
			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			CostTable = NewObject<UDataTable>(GameInstance);
			Reservations = NewObject<UFF_ProgramReservationSubsystem>(GameInstance);
			if (Progression == nullptr || Bridge == nullptr
				|| Definition == nullptr || CostTable == nullptr
				|| Reservations == nullptr)
			{
				return false;
			}

			// 프로덕션은 Initialize 가 물린다. 재시작이 예약 슬롯까지 비우는지 보려면 필요하다.
			Progression->SetProgramReservations(Reservations);

			// 시설을 짓지 않아도 표가 필요하다. 비어 있으면 IsDefinitionValid 가 Error 를 찍고
			// UE 자동화 프레임워크는 Error 로그 자체를 테스트 실패로 취급한다.
			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
			Definition->FacilityCostTable = CostTable;

			Definition->FestivalId = TEXT("StageBridgeAutomationFestival");
			Definition->InitialGold = InitialGold;
			Definition->DailyVenueRentGold = DailyRent;
			Definition->InitialStaffCount = BaseStaff;

			// StartFestival 이 Day1 을 정산한다. 브리지는 Day2 부터 관여한다.
			Progression->StartFestival(Definition);

			// 프로덕션에서는 Initialize 가 이 결선을 한다. 여기서는 서브시스템 컬렉션이
			// 없으므로(NewObject 한 GameInstance 다) 같은 창구를 직접 부른다.
			Bridge->SetProgression(Progression);
			return true;
		}

		UGameInstance* GameInstance = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_StageEconomyBridgeSubsystem* Bridge = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
		UFF_ProgramReservationSubsystem* Reservations = nullptr;
	};
}

// MorningCheck 진입만 하루 경계다.
//
// 단계가 8개에서 늘어나면 이 테스트가 먼저 깨진다. 새 단계는 자동으로 "경계 아님"이 되는데,
// 그게 맞는지는 사람이 판단해야 하는 일이라 개수를 같이 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageEconomyBridge_OnlyMorningCheckIsDayBoundary, "FestaFiesta.Economy.StageEconomyBridge.OnlyMorningCheckIsDayBoundary", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageEconomyBridge_OnlyMorningCheckIsDayBoundary::RunTest(const FString& Parameters)
{
	const UEnum* StageEnum = StaticEnum<EFF_FestivalStage>();
	if (!TestNotNull(TEXT("단계 열거형을 찾는다"), StageEnum))
	{
		return false;
	}

	// _MAX 를 뺀 실제 단계 수.
	const int32 StageCount = StageEnum->NumEnums() - 1;
	TestEqual(TEXT("단계가 8개다 — 늘어나면 경계 판정을 다시 봐야 한다"), StageCount, 8);

	for (int32 Index = 0; Index < StageCount; ++Index)
	{
		const EFF_FestivalStage Stage =
			static_cast<EFF_FestivalStage>(StageEnum->GetValueByIndex(Index));
		const bool bIsBoundary = UFF_StageEconomyBridgeSubsystem::IsDayBoundaryStage(Stage);

		if (Stage == EFF_FestivalStage::MorningCheck)
		{
			TestTrue(TEXT("MorningCheck 은 하루 경계다"), bIsBoundary);
		}
		else
		{
			TestFalse(
				*FString::Printf(TEXT("%s 는 하루 경계가 아니다"),
					*StageEnum->GetNameStringByIndex(Index)),
				bIsBoundary);
		}
	}

	return true;
}

// 🔴 프로그램 편성은 Live 전에만 가능하다 — 특히 Live·Closing 에서는 안 된다.
//
// 이미 지나간 시각의 카드를 사면 예약 슬롯은 생기는데 손님이 올 시점이 없어서, 비용만 빠지고
// 체험비 0 으로 정산된 뒤 정원 미달 Warning 이 뜬다. 크래시가 없어서 조용히 손해만 보는 함정이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageEconomyBridge_OnlyMorningStagesAllowProgramScheduling, "FestaFiesta.Economy.StageEconomyBridge.OnlyMorningStagesAllowProgramScheduling", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageEconomyBridge_OnlyMorningStagesAllowProgramScheduling::RunTest(const FString& Parameters)
{
	const UEnum* StageEnum = StaticEnum<EFF_FestivalStage>();
	if (!TestNotNull(TEXT("단계 열거형을 찾는다"), StageEnum))
	{
		return false;
	}

	const int32 StageCount = StageEnum->NumEnums() - 1;

	for (int32 Index = 0; Index < StageCount; ++Index)
	{
		const EFF_FestivalStage Stage =
			static_cast<EFF_FestivalStage>(StageEnum->GetValueByIndex(Index));
		const bool bAllowed = UFF_StageEconomyBridgeSubsystem::IsProgramSchedulingStage(Stage);

		const bool bExpected = Stage == EFF_FestivalStage::Setup
			|| Stage == EFF_FestivalStage::MorningCheck;

		TestEqual(
			*FString::Printf(TEXT("%s 에서 편성 %s"),
				*StageEnum->GetNameStringByIndex(Index),
				bExpected ? TEXT("가능") : TEXT("불가")),
			bAllowed, bExpected);
	}

	// 🔴 Live 가 시작된 뒤로는 어느 단계에서도 안 된다. 위 루프에 이미 포함되지만,
	// 이 둘이 뚫리는 것이 실제 손해로 이어지는 경로라 따로 못박는다.
	TestFalse(TEXT("Live 중에는 편성할 수 없다"),
		UFF_StageEconomyBridgeSubsystem::IsProgramSchedulingStage(EFF_FestivalStage::Live));
	TestFalse(TEXT("Closing 에도 편성할 수 없다"),
		UFF_StageEconomyBridgeSubsystem::IsProgramSchedulingStage(EFF_FestivalStage::Closing));

	// 시설 배치(`IsReconfigureAllowed`)는 PromotionPreview 도 허용한다. 편성은 안 한다 —
	// 두 규칙이 갈리는 지점이라, 나중에 합치고 싶어지면 여기가 먼저 깨져야 한다.
	TestFalse(TEXT("PromotionPreview 는 편성 단계가 아니다"),
		UFF_StageEconomyBridgeSubsystem::IsProgramSchedulingStage(
			EFF_FestivalStage::PromotionPreview));

	return true;
}

// Day2·3 MorningCheck 진입이 실제로 정산을 연다.
// 3일 임대료 합계 7,200 은 §2.6 의 입장료 7,500 이 덮는 값이라 여기서 어긋나면 수지가 무너진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageEconomyBridge_MorningCheckEntryOpensNewDay, "FestaFiesta.Economy.StageEconomyBridge.MorningCheckEntryOpensNewDay", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageEconomyBridge_MorningCheckEntryOpensNewDay::RunTest(const FString& Parameters)
{
	FFF_StageEconomyBridgeTestHelpers::FStageBridgeHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize()))
	{
		return false;
	}

	// 브리지가 관여하기 전 상태. Day1 은 StartFestival 이 이미 정산했다.
	TestEqual(TEXT("Day1 은 브리지 없이 정산돼 있다"),
		Harness.Progression->GetLastProcessedDay(), 1);
	TestEqual(TEXT("Day1 임대료가 빠져 있다"), Harness.Progression->GetGold(),
		FFF_StageEconomyBridgeTestHelpers::InitialGold
			- FFF_StageEconomyBridgeTestHelpers::DailyRent);

	// Day2 진입. AdvanceToNextDay 가 CurrentDay 를 먼저 올리므로 브리지가 받는 날짜는 2 다.
	Harness.Bridge->ProcessStageEntry(EFF_FestivalStage::MorningCheck, 2);
	TestEqual(TEXT("Day2 가 정산된다"), Harness.Progression->GetLastProcessedDay(), 2);
	TestEqual(TEXT("임대료가 이틀치 나갔다"), Harness.Progression->GetGold(),
		FFF_StageEconomyBridgeTestHelpers::InitialGold
			- FFF_StageEconomyBridgeTestHelpers::DailyRent * 2);

	// Day3 진입.
	Harness.Bridge->ProcessStageEntry(EFF_FestivalStage::MorningCheck, 3);
	TestEqual(TEXT("Day3 가 정산된다"), Harness.Progression->GetLastProcessedDay(), 3);
	TestEqual(TEXT("3일 임대료 합계가 7,200 이다"), Harness.Progression->GetGold(),
		FFF_StageEconomyBridgeTestHelpers::InitialGold - 7200);

	// 미납이 생기지 않는 잔액이었다 — 임대료가 전액 현금으로 나갔다는 뜻이다.
	TestEqual(TEXT("미납이 없다"), Harness.Progression->GetTotalArrears(), 0);

	return true;
}

// 같은 날 MorningCheck 에 다시 들어와도 두 번 걷지 않는다.
//
// RequestTransition 에 같은 단계 가드가 없어서 재진입이 실제로 가능하다.
// 이중 징수는 크래시도 Error 로그도 없이 3일 수지만 조용히 깨뜨리는 종류의 사고다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageEconomyBridge_ReEntryOnSameDayDoesNotChargeTwice, "FestaFiesta.Economy.StageEconomyBridge.ReEntryOnSameDayDoesNotChargeTwice", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageEconomyBridge_ReEntryOnSameDayDoesNotChargeTwice::RunTest(const FString& Parameters)
{
	FFF_StageEconomyBridgeTestHelpers::FStageBridgeHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize()))
	{
		return false;
	}

	Harness.Bridge->ProcessStageEntry(EFF_FestivalStage::MorningCheck, 2);
	const int32 GoldAfterFirstEntry = Harness.Progression->GetGold();

	// MorningCheck → 다른 단계 → 다시 MorningCheck. 스킵·되돌림으로 실제로 생기는 경로다.
	Harness.Bridge->ProcessStageEntry(EFF_FestivalStage::Live, 2);
	Harness.Bridge->ProcessStageEntry(EFF_FestivalStage::MorningCheck, 2);

	TestEqual(TEXT("같은 날 재진입에서 임대료가 두 번 빠지지 않는다"),
		Harness.Progression->GetGold(), GoldAfterFirstEntry);
	TestEqual(TEXT("마지막 정산일이 그대로다"),
		Harness.Progression->GetLastProcessedDay(), 2);

	// 지난 날로 되돌아오는 진입도 무시된다 — 날짜의 권위는 Stage 지만 가드는 경제가 쥔다.
	Harness.Bridge->ProcessStageEntry(EFF_FestivalStage::MorningCheck, 1);
	TestEqual(TEXT("지난 날 진입은 무시된다"),
		Harness.Progression->GetGold(), GoldAfterFirstEntry);
	TestEqual(TEXT("마지막 정산일이 되돌아가지 않는다"),
		Harness.Progression->GetLastProcessedDay(), 2);

	return true;
}

// MorningCheck 이 아닌 진입은 아무것도 하지 않는다.
// Live 진입이 하루를 열면 매일 두 번 임대료가 나가고, TodayRevenue 가 장중에 리셋된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageEconomyBridge_OtherStagesDoNotSettle, "FestaFiesta.Economy.StageEconomyBridge.OtherStagesDoNotSettle", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageEconomyBridge_OtherStagesDoNotSettle::RunTest(const FString& Parameters)
{
	FFF_StageEconomyBridgeTestHelpers::FStageBridgeHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize()))
	{
		return false;
	}

	// 오늘 매출을 만들어 둔다. 하루 경계가 잘못 열리면 이 값이 0 으로 리셋되므로
	// 잔액만 보는 것보다 더 촘촘한 감시선이 된다.
	Harness.Progression->AddRevenue(500, EFF_EarnReason::AdmissionFee);

	const int32 GoldBefore = Harness.Progression->GetGold();
	const int32 TodayRevenueBefore = Harness.Progression->GetTodayRevenue();

	const EFF_FestivalStage NonBoundaryStages[] = {
		EFF_FestivalStage::Setup,
		EFF_FestivalStage::PromotionPreview,
		EFF_FestivalStage::SunsetTransition,
		EFF_FestivalStage::Live,
		EFF_FestivalStage::Closing,
		EFF_FestivalStage::Report,
		EFF_FestivalStage::Finale
	};

	// 날짜는 2 로 넘긴다. "날짜가 바뀌었어도 단계가 아니면 안 연다"를 고정하려는 것이다.
	for (const EFF_FestivalStage Stage : NonBoundaryStages)
	{
		Harness.Bridge->ProcessStageEntry(Stage, 2);
	}

	TestEqual(TEXT("잔액이 변하지 않는다"), Harness.Progression->GetGold(), GoldBefore);
	TestEqual(TEXT("오늘 매출이 리셋되지 않는다"),
		Harness.Progression->GetTodayRevenue(), TodayRevenueBefore);
	TestEqual(TEXT("마지막 정산일이 Day1 에 머문다"),
		Harness.Progression->GetLastProcessedDay(), 1);

	return true;
}

// 🔴 2회차 축제가 1회차 값을 물려받지 않는다 (§B0).
//
// GameInstance 는 OpenLevel 을 넘어 살아남아서 경제의 Initialize 는 게임을 켤 때 한 번만
// 돈다. 이 신호가 없으면 프론트엔드를 다녀온 뒤 Gold·원장·미납·인원·예약이 1회차 마지막
// 값으로 남는다 — 크래시도 로그도 없이 두 번째 판이 처음부터 망가진 채로 시작한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageEconomyBridge_FestivalStartedResetsEconomy, "FestaFiesta.Economy.StageEconomyBridge.FestivalStartedResetsEconomy", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageEconomyBridge_FestivalStartedResetsEconomy::RunTest(const FString& Parameters)
{
	FFF_StageEconomyBridgeTestHelpers::FStageBridgeHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize()))
	{
		return false;
	}

	const int32 GoldAfterFirstDay = Harness.Progression->GetGold();

	// 1회차를 끝까지 굴린다 — 사흘치 임대료, 매출, 알바, 예약 슬롯까지 상태를 더럽힌다.
	Harness.Bridge->ProcessStageEntry(EFF_FestivalStage::MorningCheck, 2);
	Harness.Bridge->ProcessStageEntry(EFF_FestivalStage::MorningCheck, 3);
	Harness.Progression->AddRevenue(1500, EFF_EarnReason::AdmissionFee);
	Harness.Reservations->RegisterProgram(FGuid::NewGuid(), 60.f, 90.f, 12);

	TestNotEqual(TEXT("1회차가 실제로 진행됐다"),
		Harness.Progression->GetGold(), GoldAfterFirstDay);
	TestEqual(TEXT("1회차에 예약 슬롯이 남아 있다"),
		Harness.Reservations->GetReservationIds().Num(), 1);
	TestTrue(TEXT("1회차 매출이 쌓여 있다"), Harness.Progression->GetTotalRevenue() > 0);

	// 프론트엔드 복귀 후 2회차 시작. Stage 는 StartNewFestival 이 되돌리고,
	// 경제는 그 신호를 받는 이 창구가 되돌린다.
	Harness.Bridge->ProcessFestivalStarted();

	TestEqual(TEXT("잔액이 2회차 Day1 값으로 돌아온다"),
		Harness.Progression->GetGold(), GoldAfterFirstDay);
	TestEqual(TEXT("정산일이 Day1 로 돌아온다"),
		Harness.Progression->GetLastProcessedDay(), 1);
	TestEqual(TEXT("누적 매출이 비워진다"), Harness.Progression->GetTotalRevenue(), 0);
	TestEqual(TEXT("미납이 비워진다"), Harness.Progression->GetTotalArrears(), 0);
	TestEqual(TEXT("인원이 기본값으로 돌아온다"),
		Harness.Progression->GetTotalStaffCount(),
		FFF_StageEconomyBridgeTestHelpers::BaseStaff);
	TestEqual(TEXT("예약 슬롯이 지워진다"),
		Harness.Reservations->GetReservationIds().Num(), 0);

	return true;
}

// 🔴 신호가 두 번 와도 상태가 같다.
//
// 1회차에는 Initialize 의 StartFestival 과 이 신호가 겹친다. StartFestival 이 완전한
// 리셋 함수라 겹쳐도 되는 것이 이 배선의 전제이므로, 그 전제를 고정해 둔다 —
// 만약 부분 리셋으로 바뀌면 Day1 임대료가 두 번 빠지고 여기서 먼저 깨진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageEconomyBridge_FestivalStartedIsIdempotent, "FestaFiesta.Economy.StageEconomyBridge.FestivalStartedIsIdempotent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageEconomyBridge_FestivalStartedIsIdempotent::RunTest(const FString& Parameters)
{
	FFF_StageEconomyBridgeTestHelpers::FStageBridgeHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize()))
	{
		return false;
	}

	// Initialize 가 이미 Day1 을 열어 둔 상태다. 여기서 신호가 오는 것이 1회차 상황이다.
	const int32 GoldAfterInitialize = Harness.Progression->GetGold();

	Harness.Bridge->ProcessFestivalStarted();
	const int32 GoldAfterFirstSignal = Harness.Progression->GetGold();

	TestEqual(TEXT("첫 신호가 Day1 임대료를 두 번 걷지 않는다"),
		GoldAfterFirstSignal, GoldAfterInitialize);

	Harness.Bridge->ProcessFestivalStarted();
	Harness.Bridge->ProcessFestivalStarted();

	TestEqual(TEXT("몇 번을 받아도 같은 상태다"),
		Harness.Progression->GetGold(), GoldAfterInitialize);
	TestEqual(TEXT("정산일도 Day1 에 머문다"),
		Harness.Progression->GetLastProcessedDay(), 1);

	return true;
}

// Progression 이 없으면 조용히 아무 일도 하지 않는다.
// 신호는 Stage 가 쏘므로 경제가 안 붙은 구성에서도 이 경로가 지나간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StageEconomyBridge_FestivalStartedWithoutProgressionIsSafe, "FestaFiesta.Economy.StageEconomyBridge.FestivalStartedWithoutProgressionIsSafe", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StageEconomyBridge_FestivalStartedWithoutProgressionIsSafe::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	if (!TestNotNull(TEXT("GameInstance 가 만들어진다"), GameInstance))
	{
		return false;
	}

	UFF_StageEconomyBridgeSubsystem* Bridge =
		NewObject<UFF_StageEconomyBridgeSubsystem>(GameInstance);
	if (!TestNotNull(TEXT("브리지가 만들어진다"), Bridge))
	{
		return false;
	}

	// Progression 을 물리지 않았다. 크래시하지 않는 것이 확인 내용이다.
	Bridge->ProcessFestivalStarted();

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
