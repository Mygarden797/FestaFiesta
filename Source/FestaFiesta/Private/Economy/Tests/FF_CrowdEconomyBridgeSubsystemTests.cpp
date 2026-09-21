// Fill out your copyright notice in the Description page of Project Settings.

#include "Economy/FF_CrowdEconomyBridgeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_FestivalSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"

/**
 * Crowd ↔ 경제 브리지를 검증한다 (입장료·예상 방문객 반영, 2026-09-18).
 *
 * 배선 테스트는 두 방향(Crowd→경제, 경제→Crowd)을 각각 본다. 통합 테스트는
 * 실제 델리게이트를 그대로 흘려서 원장·Crowd 값이 움직이는지 본다 — 어느 쪽도 흉내내지 않는다.
 */
namespace FFF_CrowdEconomyBridgeTestHelpers
{
	const FName HandlerName(TEXT("HandleVisitorsAdmitted"));

	// StartFestival은 IsDefinitionValid를 거친다 — FacilityCostTable이 비어 있으면
	// "[경제] 축제 정의 오류" Error 로그가 찍히고, 자동화 프레임워크는 테스트 중 Error 로그
	// 하나만 떠도 그 테스트를 자동으로 Fail 처리한다(어서션과 무관하게). 이 프로젝트의
	// GameInstance 기반 테스트는 GameInstance 생성 시 Config의 DefaultFestival(정상 가격표
	// 있는 실제 자산)을 먼저 자동 시작하는데, 바로 뒤에 이 픽스처가 자기 Definition으로
	// 덮어 시작하면서 그 정의에 가격표가 없어 위 Error가 났다(`FF_FinalePurchaseTests.cpp`와
	// 같은 이유 — 2026-09-18 전체 스위트 첫 실행에서 발견).
	UDataTable* MakeMinimalFacilityCostTable(UObject* Outer)
	{
		UDataTable* CostTable = NewObject<UDataTable>(Outer);
		if (CostTable)
		{
			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
		}
		return CostTable;
	}

	// Queue 브리지 테스트와 같은 이유로 격리 월드를 쓴다 — Crowd 서브시스템도 지면·내비게이션
	// 설정에 영향을 받을 수 있어 기존 관례를 그대로 따른다.
	struct FCrowdEconomyHarness
	{
		bool Initialize(FAutomationTestBase* Test, int32 AdmissionFeeGold = 5)
		{
			if (!TestWorld.CreateTestWorld(EWorldType::Game))
			{
				TestWorld.ForwardErrorMessages(Test);
				return false;
			}

			World = TestWorld.GetTestWorld();
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			CrowdSim = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
			Bridge = World ? World->GetSubsystem<UFF_CrowdEconomyBridgeSubsystem>() : nullptr;
			Progression = GameInstance
				? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
				: nullptr;

			if (!World || !GameInstance || !CrowdSim || !Bridge || !Progression)
			{
				return false;
			}

			// FTestWorldWrapper 가 BeginPlay 를 안 불렀을 수도 있다 — GameInstanceSubsystem
			// 구독은 Initialize 시점이 아니라 여기서 확실히 걸리게 한다(FF_RequiredFacilitiesSubsystem
			// 테스트와 같은 이유).
			Bridge->OnWorldBeginPlay(*World);

			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			CostTable = MakeMinimalFacilityCostTable(GameInstance);
			if (!Definition || !CostTable) { return false; }

			Definition->FacilityCostTable = CostTable;
			Definition->FestivalId = TEXT("CrowdEconomyAutomationFestival");
			Definition->AdmissionFeeGold = AdmissionFeeGold;

			Progression->StartFestival(Definition);
			return true;
		}

		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		UFF_CrowdSimulationSubsystem* CrowdSim = nullptr;
		UFF_CrowdEconomyBridgeSubsystem* Bridge = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
	};
}

// --- 배선 -----------------------------------------------------------------

// 브리지가 월드에 생기고 Crowd 의 입장 신호를 실제로 구독한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdEconomyBridge_SubscribesToVisitorsAdmitted, "FestaFiesta.Economy.CrowdBridge.SubscribesToVisitorsAdmitted", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdEconomyBridge_SubscribesToVisitorsAdmitted::RunTest(const FString& Parameters)
{
	using namespace FFF_CrowdEconomyBridgeTestHelpers;

	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	if (!TestNotNull(TEXT("테스트 월드를 만든다"), World))
	{
		return false;
	}

	UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	UFF_CrowdEconomyBridgeSubsystem* Bridge = World->GetSubsystem<UFF_CrowdEconomyBridgeSubsystem>();

	if (!TestNotNull(TEXT("Crowd 서브시스템이 있다"), Crowd)
		|| !TestNotNull(TEXT("브리지가 월드에 생긴다"), Bridge))
	{
		return false;
	}

	TestTrue(TEXT("브리지가 입장 신호를 구독한다"),
		Crowd->OnVisitorsAdmitted.Contains(Bridge, HandlerName));

	return true;
}

// 월드가 사라지면 구독도 같이 사라진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdEconomyBridge_UnsubscribesWithWorld, "FestaFiesta.Economy.CrowdBridge.UnsubscribesWithWorld", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdEconomyBridge_UnsubscribesWithWorld::RunTest(const FString& Parameters)
{
	using namespace FFF_CrowdEconomyBridgeTestHelpers;

	UFF_CrowdSimulationSubsystem* Crowd = nullptr;
	UFF_CrowdEconomyBridgeSubsystem* Bridge = nullptr;

	{
		FTestWorldWrapper TestWorld;
		if (!TestWorld.CreateTestWorld(EWorldType::Game))
		{
			TestWorld.ForwardErrorMessages(this);
			return false;
		}

		UWorld* World = TestWorld.GetTestWorld();
		if (!TestNotNull(TEXT("테스트 월드를 만든다"), World))
		{
			return false;
		}

		Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
		Bridge = World->GetSubsystem<UFF_CrowdEconomyBridgeSubsystem>();
		if (!TestNotNull(TEXT("Crowd 서브시스템이 있다"), Crowd)
			|| !TestNotNull(TEXT("브리지가 월드에 생긴다"), Bridge))
		{
			return false;
		}

		TestTrue(TEXT("월드가 사는 동안은 구독돼 있다"),
			Crowd->OnVisitorsAdmitted.Contains(Bridge, HandlerName));
	}

	// 여기서 TestWorld 소멸자가 월드를 정리하고 두 서브시스템의 Deinitialize 가 불린다.
	return true;
}

// --- 통합: 원장까지 --------------------------------------------------------

// 입장 신호를 실제로 흘려서 입장료가 원장에 꽂히는 것까지 본다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdEconomyBridge_AdmissionAddsRevenue, "FestaFiesta.Economy.CrowdBridge.AdmissionAddsRevenue", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdEconomyBridge_AdmissionAddsRevenue::RunTest(const FString& Parameters)
{
	using namespace FFF_CrowdEconomyBridgeTestHelpers;

	FCrowdEconomyHarness Harness;
	if (!TestTrue(TEXT("격리 월드가 초기화된다"), Harness.Initialize(this, /*AdmissionFeeGold=*/5)))
	{
		return false;
	}

	const int32 RevenueBefore = Harness.Progression->GetTodayRevenue();

	// 실제 신호를 그대로 흘린다 — 498명이 입장했다고 가정.
	Harness.CrowdSim->OnVisitorsAdmitted.Broadcast(498);

	TestEqual(TEXT("오늘 매출이 인원 × 입장료만큼 늘었다"),
		Harness.Progression->GetTodayRevenue(), RevenueBefore + 498 * 5);

	int32 AdmissionRowAmount = 0;
	for (const FFF_LedgerRow& Row : Harness.Progression->GetEarnRowsForDay(1))
	{
		if (Row.ReasonId == TEXT("AdmissionFee"))
		{
			AdmissionRowAmount = Row.Amount;
		}
	}
	TestEqual(TEXT("원장의 입장료 줄이 그 금액이다"), AdmissionRowAmount, 498 * 5);

	return true;
}

// 0명 입장·축제 시작 전에는 아무 일도 일어나지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdEconomyBridge_IgnoresZeroAndPreFestival, "FestaFiesta.Economy.CrowdBridge.IgnoresZeroAndPreFestival", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdEconomyBridge_IgnoresZeroAndPreFestival::RunTest(const FString& Parameters)
{
	using namespace FFF_CrowdEconomyBridgeTestHelpers;

	// Keep this world genuinely pre-festival until the explicit StartFestival below.
	TGuardValue<TSoftObjectPtr<UFF_FestivalDefinition>> DefaultFestivalGuard(
		GetMutableDefault<UFF_FestivalSettings>()->DefaultFestival, TSoftObjectPtr<UFF_FestivalDefinition>());
	AddExpectedMessage(TEXT("DefaultFestival 이 지정되지 않았다"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1, false);
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFF_CrowdSimulationSubsystem* Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
	UFF_CrowdEconomyBridgeSubsystem* Bridge = World ? World->GetSubsystem<UFF_CrowdEconomyBridgeSubsystem>() : nullptr;
	UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
		: nullptr;
	if (!TestNotNull(TEXT("Crowd 서브시스템이 있다"), Crowd)
		|| !TestNotNull(TEXT("브리지가 있다"), Bridge)
		|| !TestNotNull(TEXT("경제 서브시스템이 있다"), Progression))
	{
		return false;
	}
	Bridge->OnWorldBeginPlay(*World);

	// 축제를 시작하지 않은 상태 — IsFestivalStarted() 가 false 다.
	TestFalse(TEXT("축제가 아직 시작 전이다"), Progression->IsFestivalStarted());
	Crowd->OnVisitorsAdmitted.Broadcast(100);
	TestEqual(TEXT("축제 시작 전 입장은 매출을 만들지 않는다"), Progression->GetTotalRevenue(), 0);

	auto* Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
	Definition->FacilityCostTable = MakeMinimalFacilityCostTable(GameInstance);
	Definition->FestivalId = TEXT("CrowdEconomyZeroCountFestival");
	Definition->AdmissionFeeGold = 5;
	Progression->StartFestival(Definition);

	const int32 RevenueBefore = Progression->GetTodayRevenue();
	Crowd->OnVisitorsAdmitted.Broadcast(0);
	TestEqual(TEXT("0명 입장은 매출을 만들지 않는다"), Progression->GetTodayRevenue(), RevenueBefore);

	return true;
}

// --- 경제 → Crowd: 예상 방문객 반영 ----------------------------------------

// 축제가 시작되면(홍보를 안 사도) BaseDailyVisitors 값이 Crowd 에 바로 채워진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdEconomyBridge_FestivalStartFillsExpectedVisitors, "FestaFiesta.Economy.CrowdBridge.FestivalStartFillsExpectedVisitors", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdEconomyBridge_FestivalStartFillsExpectedVisitors::RunTest(const FString& Parameters)
{
	using namespace FFF_CrowdEconomyBridgeTestHelpers;

	// Prevent project bootstrap from supplying visitors before this test starts its Festival.
	TGuardValue<TSoftObjectPtr<UFF_FestivalDefinition>> DefaultFestivalGuard(
		GetMutableDefault<UFF_FestivalSettings>()->DefaultFestival, TSoftObjectPtr<UFF_FestivalDefinition>());
	AddExpectedMessage(TEXT("DefaultFestival 이 지정되지 않았다"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1, false);
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UFF_CrowdSimulationSubsystem* Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
	UFF_CrowdEconomyBridgeSubsystem* Bridge = World ? World->GetSubsystem<UFF_CrowdEconomyBridgeSubsystem>() : nullptr;
	UFF_FestivalProgressionSubsystem* Progression = GameInstance
		? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
		: nullptr;
	if (!TestNotNull(TEXT("Crowd 서브시스템이 있다"), Crowd)
		|| !TestNotNull(TEXT("브리지가 있다"), Bridge)
		|| !TestNotNull(TEXT("경제 서브시스템이 있다"), Progression))
	{
		return false;
	}
	Bridge->OnWorldBeginPlay(*World);

	TestEqual(TEXT("축제 시작 전에는 override 가 비어 있다"),
		Crowd->GetExpectedVisitorCountOverride(), INDEX_NONE);

	auto* Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
	Definition->FacilityCostTable = MakeMinimalFacilityCostTable(GameInstance);
	Definition->FestivalId = TEXT("CrowdEconomyExpectedVisitorsFestival");
	Definition->BaseDailyVisitors = 500;
	Progression->StartFestival(Definition);

	TestEqual(TEXT("홍보를 안 사도 BaseDailyVisitors 로 채워진다"),
		Crowd->GetExpectedVisitorCountOverride(), Progression->GetExpectedVisitors());
	TestEqual(TEXT("미구매 배수는 1.0 이라 값 자체는 BaseDailyVisitors 다"),
		Crowd->GetExpectedVisitorCountOverride(), 500);

	return true;
}

// 홍보를 사서 예상 방문객이 바뀌면 Crowd 의 override 도 같이 바뀐다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdEconomyBridge_PromotionPurchaseUpdatesExpectedVisitors, "FestaFiesta.Economy.CrowdBridge.PromotionPurchaseUpdatesExpectedVisitors", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdEconomyBridge_PromotionPurchaseUpdatesExpectedVisitors::RunTest(const FString& Parameters)
{
	using namespace FFF_CrowdEconomyBridgeTestHelpers;

	FCrowdEconomyHarness Harness;
	if (!TestTrue(TEXT("격리 월드가 초기화된다"), Harness.Initialize(this)))
	{
		return false;
	}

	// PromotionBasic 은 Definition 생성자 기본값(500 Gold, 배수 1.08)을 그대로 쓴다 —
	// 이 테스트가 보려는 건 구매로 인한 갱신이지, 시작 시점 채움(앞선 테스트가 이미 봄)이 아니다.
	if (!TestEqual(TEXT("기본 홍보 구매가 성공한다"),
		Harness.Progression->TryPurchasePromotion(EFF_PromotionGrade::Basic),
		EFF_PromotionPurchaseResult::Success))
	{
		return false;
	}

	TestEqual(TEXT("구매 직후 Crowd 의 override 가 새 예상 방문객과 같다"),
		Harness.CrowdSim->GetExpectedVisitorCountOverride(), Harness.Progression->GetExpectedVisitors());
	TestTrue(TEXT("실제로 기본값보다 커졌다"),
		Harness.CrowdSim->GetExpectedVisitorCountOverride() > Harness.Definition->BaseDailyVisitors);

	return true;
}

// 🔴 2026-09-18 회귀 테스트 — StartFestival이 등급을 리셋만 하고 이벤트를 안 쏘던 문제.
// 이 브리지는 실제 UFUNCTION 구독자라, 여기서 값이 다시 기본값으로 떨어지는 것 자체가
// OnFestivalDefinitionChanged·OnExpectedVisitorsChanged 가 실제로 발행됐다는 증거다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdEconomyBridge_RestartResetsExpectedVisitors, "FestaFiesta.Economy.CrowdBridge.RestartResetsExpectedVisitors", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdEconomyBridge_RestartResetsExpectedVisitors::RunTest(const FString& Parameters)
{
	using namespace FFF_CrowdEconomyBridgeTestHelpers;

	FCrowdEconomyHarness Harness;
	if (!TestTrue(TEXT("격리 월드가 초기화된다"), Harness.Initialize(this)))
	{
		return false;
	}

	if (!TestEqual(TEXT("집중 홍보 구매가 성공한다"),
		Harness.Progression->TryPurchasePromotion(EFF_PromotionGrade::Focused),
		EFF_PromotionPurchaseResult::Success))
	{
		return false;
	}
	if (!TestTrue(TEXT("구매 직후엔 기본값보다 크다"),
		Harness.CrowdSim->GetExpectedVisitorCountOverride() > Harness.Definition->BaseDailyVisitors))
	{
		return false;
	}

	Harness.Progression->RestartFestival();

	TestEqual(TEXT("재시작하면 등급이 비워진다"),
		Harness.Progression->GetPurchasedPromotionGrade(), EFF_PromotionGrade::None);
	TestEqual(TEXT("Crowd 의 override 도 기본값으로 돌아온다 — 이벤트가 실제로 나갔다는 뜻"),
		Harness.CrowdSim->GetExpectedVisitorCountOverride(), Harness.Definition->BaseDailyVisitors);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
