// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_QueueEconomyBridgeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Economy/Rules/FF_BoothRevenueRules.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Queue/FF_QueueTypes.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

/**
 * Queue → 경제 브리지를 검증한다 (S10).
 *
 * 앞쪽 세 테스트는 배선만 본다(누가 무엇에 연결돼 있는가).
 * 마지막 두 개는 **실제로 원장이 움직이는지** 를 본다 — 시설 설치부터 결제까지
 * 체인 전체를 태우는 유일한 테스트이고, 오늘 작업의 완료 증거다.
 *
 * 수수료 금액 자체의 규칙(반올림·파티 단위)은 FF_BoothRevenueRulesTests.cpp 가 갖는다.
 */
namespace FFF_QueueEconomyBridgeTestHelpers
{
	// 구독을 이름으로 확인한다. HandlePartyExited 는 private 이라 메서드 포인터를 못 쓰므로
	// (IsAlreadyBound 는 &Class::Method 를 요구한다) FMulticastScriptDelegate::Contains 를 쓴다.
	// UFUNCTION 이라 동적 델리게이트가 이름으로 들고 있어서 이쪽으로 조회된다.
	const FName HandlerName(TEXT("HandlePartyExited"));

	const FName TestZoneId(TEXT("GZ_DadaPocha"));

	// 소분류 토큰이 FoodStall 이라 §2.4 의 객단가 20 이 걸린다.
	const FName BoothDefinitionId(TEXT("Booth_FoodStall_QueueEconomyAutomation"));

	constexpr int32 InitialGold = 10000;
	constexpr int32 SpawnedAgentCount = 6;

	// Zone System 의 기본값을 테스트용으로 낮추고 원복한다.
	// 격리 월드에는 지면·내비게이션이 없어서 이걸 끄지 않으면 설치가 전부 거부된다.
	class FScopedQueueEconomyTestSettings
	{
	public:
		FScopedQueueEconomyTestSettings()
			: Settings(GetMutableDefault<UFFZSDeveloperSettings>())
			, bGroundChecks(Settings->bEnableGroundChecks)
			, bWorldCollisionChecks(Settings->bEnableWorldCollisionChecks)
			, bNavigationChecks(Settings->bEnableNavigationProjectionChecks)
			, bProductionPolicies(Settings->bRequireProductionPolicyDomains)
			, bRestrictZoneIds(Settings->bRestrictZoneIdsToProjectPresets)
			, InitialPhase(Settings->InitialPhase)
			, MaxEnumeratedCells(Settings->MaxEnumeratedCellsPerFacility)
		{
			Settings->bEnableGroundChecks = false;
			Settings->bEnableWorldCollisionChecks = false;
			Settings->bEnableNavigationProjectionChecks = false;
			Settings->bRequireProductionPolicyDomains = false;
			Settings->bRestrictZoneIdsToProjectPresets = false;
			Settings->InitialPhase = EFFZSChangePhase::InitialSetup;
			Settings->MaxEnumeratedCellsPerFacility = 1'000'000;
		}

		~FScopedQueueEconomyTestSettings()
		{
			Settings->bEnableGroundChecks = bGroundChecks;
			Settings->bEnableWorldCollisionChecks = bWorldCollisionChecks;
			Settings->bEnableNavigationProjectionChecks = bNavigationChecks;
			Settings->bRequireProductionPolicyDomains = bProductionPolicies;
			Settings->bRestrictZoneIdsToProjectPresets = bRestrictZoneIds;
			Settings->InitialPhase = InitialPhase;
			Settings->MaxEnumeratedCellsPerFacility = MaxEnumeratedCells;
		}

	private:
		UFFZSDeveloperSettings* Settings;
		bool bGroundChecks;
		bool bWorldCollisionChecks;
		bool bNavigationChecks;
		bool bProductionPolicies;
		bool bRestrictZoneIds;
		EFFZSChangePhase InitialPhase;
		int32 MaxEnumeratedCells;
	};

	// 부스 하나를 실제로 설치하고, 그 부스를 이용할 파티를 실제로 스폰한다.
	// 체인의 어느 고리도 흉내내지 않는다 — 흉내내면 그 고리가 끊겨도 테스트가 통과한다.
	struct FQueueEconomyHarness
	{
		bool Initialize(FAutomationTestBase* Test)
		{
			if (!TestWorld.CreateTestWorld(EWorldType::Game))
			{
				TestWorld.ForwardErrorMessages(Test);
				return false;
			}

			World = TestWorld.GetTestWorld();
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
			Queue = World ? World->GetSubsystem<UFF_QueueSubsystem>() : nullptr;
			QueueBridge = World ? World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>() : nullptr;
			CrowdSim = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
			Progression = GameInstance
				? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
				: nullptr;

			if (!World || !GameInstance || !ZoneSystem || !Queue || !QueueBridge
				|| !CrowdSim || !Progression)
			{
				return false;
			}

			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			CostTable = NewObject<UDataTable>(GameInstance);
			if (!Definition || !CostTable)
			{
				return false;
			}

			// 표가 비거나 금액이 0 이하면 설치가 거부된다.
			// 설치비는 잔액만 건드리고 매출(TodayRevenue)에는 안 섞이므로 §2.2 의 400 을 그대로 쓴다.
			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			FFF_FacilityCostRow FoodStallRow;
			FoodStallRow.InstallCost = 400;
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FoodStallRow);
			Definition->FacilityCostTable = CostTable;

			Definition->FestivalId = TEXT("QueueEconomyAutomationFestival");
			Definition->InitialGold = InitialGold;
			// 이 테스트는 매출만 본다. 임대료가 걷히면 잔액 기대값에 섞인다.
			Definition->DailyVenueRentGold = 0;

			Progression->StartFestival(Definition);

			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Zone = World->SpawnActor<AFFZSGameplayZone>(
				AFFZSGameplayZone::StaticClass(), FTransform::Identity, SpawnParameters);

			return Zone
				&& Zone->ApplyAcceptedZonePreset(TestZoneId)
				&& ZoneSystem->RegisterZone(Zone);
		}

		// 음식 부스 하나를 설치한다. 성공하면 Queue 쪽 int32 ID 까지 잡아 둔다.
		// 실패 지점을 갈라서 보고한다 — 설치 실패와 Queue 미등록은 원인이 완전히 다르다.
		bool InstallFoodStall(FString& OutFailure)
		{
			FFFZSFacilitySpec Spec =
				FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
			Spec.FacilityDefinitionId = BoothDefinitionId;
			Spec.InstallCostKey = NAME_None;

			FFFZSGridPlacement Placement;
			Placement.OriginCell = FIntPoint(1000, 1000);

			const FFFZSCommand InstallCommand = UFFZSGridLibrary::MakeInstallCommand(
				Spec, TestZoneId, Placement, ZoneSystem->GetWorldRevision());

			const FFFZSCommandResult Result = ZoneSystem->ExecuteCommand(InstallCommand);
			if (!Result.bSucceeded)
			{
				OutFailure = FString::Printf(TEXT("설치 명령이 거부됐다: %s (%s)"),
					*UEnum::GetValueAsString(Result.FailureReason),
					*Result.Message.ToString());
				return false;
			}

			FacilityInstanceId = InstallCommand.FacilityInstanceId;
			QueueFacilityId = QueueBridge->FindQueueFacilityId(FacilityInstanceId);
			if (QueueFacilityId == INDEX_NONE)
			{
				OutFailure = TEXT("설치는 됐지만 Queue 에 등록되지 않았다 (QueueRules 가 없는 사양일 수 있다)");
				return false;
			}

			return true;
		}

		// 파티 하나를 실제로 스폰하고 그 파티의 인원을 돌려준다.
		int32 SpawnPartyAndGetMemberCount()
		{
			CrowdSim->InitializeSimulation(
				FVector::ZeroVector, 10000.f, 8, SpawnedAgentCount + 4, AFF_BaseCrowd::StaticClass());
			CrowdSim->SpawnAgents(SpawnedAgentCount, FVector::ZeroVector, 500.f);

			PartyId = CrowdSim->GetAgentPartyID(0);
			return PartyId == INDEX_NONE ? 0 : CrowdSim->GetPartyMembers(PartyId).Num();
		}

		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		UFFZSWorldSubsystem* ZoneSystem = nullptr;
		UFF_QueueSubsystem* Queue = nullptr;
		UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge = nullptr;
		UFF_CrowdSimulationSubsystem* CrowdSim = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
		AFFZSGameplayZone* Zone = nullptr;
		FGuid FacilityInstanceId;
		int32 QueueFacilityId = INDEX_NONE;
		int32 PartyId = INDEX_NONE;
	};
}

// --- 결제 판정 -------------------------------------------------------------

/**
 * 🔴 EFF_QueueExit 는 장원재 님 enum 이다. 값이 늘어나면 여기서 죽는다.
 *
 * 새 퇴장 유형이 추가되면 IsPaidExit 가 조용히 false 를 돌려주고, 그 시설은
 * 매출 0 이 된다. 그 0 은 "아무도 안 썼다" 와 부스별 표에서 구분되지 않는다.
 * 개수를 고정해 두면 그 시점에 빨간불이 떠서 "이건 결제인가" 를 묻게 된다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_PaidExitIsServedOnly, "FestaFiesta.Economy.QueueBridge.PaidExitIsServedOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_PaidExitIsServedOnly::RunTest(const FString& Parameters)
{
	const UEnum* ExitEnum = StaticEnum<EFF_QueueExit>();
	if (!TestNotNull(TEXT("퇴장 유형 열거형을 찾는다"), ExitEnum))
	{
		return false;
	}

	// _MAX 를 뺀 실제 값의 수.
	const int32 ExitCount = ExitEnum->NumEnums() - 1;
	TestEqual(TEXT("퇴장 유형이 3개다 — 늘어나면 결제 판정을 다시 봐야 한다"), ExitCount, 3);

	for (int32 Index = 0; Index < ExitCount; ++Index)
	{
		const EFF_QueueExit ExitType =
			static_cast<EFF_QueueExit>(ExitEnum->GetValueByIndex(Index));
		const bool bIsPaid = UFF_QueueEconomyBridgeSubsystem::IsPaidExit(ExitType);

		if (ExitType == EFF_QueueExit::Served)
		{
			TestTrue(TEXT("서비스 완료는 결제다"), bIsPaid);
		}
		else
		{
			TestFalse(
				*FString::Printf(TEXT("%s 는 결제가 아니다"),
					*ExitEnum->GetNameStringByIndex(Index)),
				bIsPaid);
		}
	}

	return true;
}

// --- 배선 -----------------------------------------------------------------

// 브리지가 월드에 생기고 Queue 의 퇴장 신호를 실제로 구독한다.
// 구독이 빠지면 매출이 0 이 되는데 게임은 정상으로 보이므로, 이 배선 자체를 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_SubscribesToPartyExited, "FestaFiesta.Economy.QueueBridge.SubscribesToPartyExited", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_SubscribesToPartyExited::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;

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

	UFF_QueueSubsystem* Queue = World->GetSubsystem<UFF_QueueSubsystem>();
	UFF_QueueEconomyBridgeSubsystem* Bridge = World->GetSubsystem<UFF_QueueEconomyBridgeSubsystem>();

	if (!TestNotNull(TEXT("Queue 서브시스템이 있다"), Queue)
		|| !TestNotNull(TEXT("브리지가 월드에 생긴다"), Bridge))
	{
		return false;
	}

	TestTrue(TEXT("브리지가 퇴장 신호를 구독한다"),
		Queue->OnPartyExited.Contains(Bridge, HandlerName));

	return true;
}

// 월드가 사라지면 구독도 같이 사라진다.
// 브리지를 World 스코프에 둔 이유가 이것이다 — GameInstance 에 뒀다면 월드가 바뀔 때마다
// 죽은 Queue 를 가리키는 구독이 쌓인다. Deinitialize 를 직접 부르지 않고 월드를 무너뜨려
// 실제 수명주기를 그대로 태운다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_UnsubscribesWithWorld, "FestaFiesta.Economy.QueueBridge.UnsubscribesWithWorld", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_UnsubscribesWithWorld::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;

	UFF_QueueSubsystem* Queue = nullptr;
	UFF_QueueEconomyBridgeSubsystem* Bridge = nullptr;

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

		Queue = World->GetSubsystem<UFF_QueueSubsystem>();
		Bridge = World->GetSubsystem<UFF_QueueEconomyBridgeSubsystem>();
		if (!TestNotNull(TEXT("Queue 서브시스템이 있다"), Queue)
			|| !TestNotNull(TEXT("브리지가 월드에 생긴다"), Bridge))
		{
			return false;
		}

		TestTrue(TEXT("월드가 사는 동안은 구독돼 있다"),
			Queue->OnPartyExited.Contains(Bridge, HandlerName));
	}

	// 여기서 TestWorld 소멸자가 월드를 정리하고 두 서브시스템의 Deinitialize 가 불린다.
	// Queue 가 이미 파괴됐을 수 있으므로 델리게이트를 다시 만지지 않는다 —
	// 이 테스트가 지키는 것은 "해제 없이 월드가 무너지면 터진다" 가 아니라
	// 그 경로를 실제로 한 번 태워서 크래시·ensure 가 없다는 사실이다.
	return true;
}

// --- 통합: 원장까지 --------------------------------------------------------

/**
 * 🔴 오늘 작업의 완료 증거다.
 *
 * 부스를 실제로 설치하고 파티를 실제로 스폰한 뒤 이용 완료 신호를 흘려서,
 * 매출이 원장에 꽂히는 것까지 본다. 체인의 어느 고리도 흉내내지 않는다 —
 * ID 변환(int32→FGuid) · 이름 파싱 · 객단가 조회 · 인원 조회 · 원장 기록이 전부 진짜다.
 *
 * 이 프로젝트에서 프로덕션 경로로 매출이 들어오는 첫 테스트다.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_ServedExitAddsCommission, "FestaFiesta.Economy.QueueBridge.ServedExitAddsCommission", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_ServedExitAddsCommission::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;

	FScopedQueueEconomyTestSettings SettingsGuard;
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("격리 월드가 초기화된다"), Harness.Initialize(this)))
	{
		return false;
	}

	FString InstallFailure;
	if (!Harness.InstallFoodStall(InstallFailure))
	{
		AddError(FString::Printf(TEXT("음식 부스가 설치되고 Queue 에 등록되어야 한다 — %s"), *InstallFailure));
		return false;
	}

	const int32 MemberCount = Harness.SpawnPartyAndGetMemberCount();
	if (!TestTrue(TEXT("파티가 스폰되고 인원이 1명 이상이다"), MemberCount > 0))
	{
		return false;
	}

	const int32 RevenueBefore = Harness.Progression->GetTodayRevenue();

	// 실제 신호를 그대로 흘린다.
	Harness.Queue->OnPartyExited.Broadcast(
		Harness.QueueFacilityId, Harness.PartyId, EFF_QueueExit::Served, 30.f);

	// 금액 규칙 자체는 순수 함수 테스트가 갖는다. 여기서 보는 것은
	// **브리지가 올바른 객단가·인원·수수료율을 집어넣었는가** 다.
	const int32 Expected = FFF_BoothRevenueRules::ResolveCommission(
		FFF_BoothRevenueRules::FindRevenuePerUse(
			Harness.Definition->RevenuePerUseByMinorToken, TEXT("FoodStall")),
		MemberCount,
		Harness.Definition->BoothCommissionRate);

	TestTrue(TEXT("기대 수수료가 0 보다 크다 — 0 이면 이 테스트가 아무것도 안 본다"), Expected > 0);
	TestEqual(TEXT("오늘 매출이 수수료만큼 늘었다"),
		Harness.Progression->GetTodayRevenue(), RevenueBefore + Expected);

	// 원장 분해 — 사유가 부스 수수료로 잡혀야 한다.
	if (!TestTrue(TEXT("Day1 원장이 있다"), Harness.Progression->HasLedgerForDay(1)))
	{
		return false;
	}

	// 줄의 키는 enum 이 아니라 짧은 enum 이름(FName)이다. 표시 문자열은 DisplayName 이 따로 든다.
	int32 CommissionRowAmount = 0;
	for (const FFF_LedgerRow& Row : Harness.Progression->GetEarnRowsForDay(1))
	{
		if (Row.ReasonId == TEXT("FacilityCommission"))
		{
			CommissionRowAmount = Row.Amount;
		}
	}
	TestEqual(TEXT("원장의 부스 수수료 줄이 그 금액이다"), CommissionRowAmount, Expected);

	// 부스별 표 — 키가 FacilityDefinitionId 여야 종태님이 카탈로그로 이름을 해석할 수 있다.
	const TArray<FFF_FacilityRevenueRow> FacilityRows =
		Harness.Progression->GetRevenueByFacilityRowsForDay(1);
	if (!TestEqual(TEXT("부스별 표에 줄이 하나 생긴다"), FacilityRows.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("표의 키가 FacilityDefinitionId 다"), FacilityRows[0].FacilityId, BoothDefinitionId);
	TestEqual(TEXT("표의 금액이 수수료와 같다"), FacilityRows[0].Amount, Expected);

	return true;
}

// 줄을 섰다 나간 것은 이용이 아니다. 같은 신호에 유형만 바꿔서 원장이 안 움직이는지 본다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_UnpaidExitAddsNothing, "FestaFiesta.Economy.QueueBridge.UnpaidExitAddsNothing", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_UnpaidExitAddsNothing::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;

	FScopedQueueEconomyTestSettings SettingsGuard;
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("격리 월드가 초기화된다"), Harness.Initialize(this)))
	{
		return false;
	}

	FString InstallFailure;
	if (!Harness.InstallFoodStall(InstallFailure))
	{
		AddError(FString::Printf(TEXT("음식 부스가 설치되고 Queue 에 등록되어야 한다 — %s"), *InstallFailure));
		return false;
	}

	if (!TestTrue(TEXT("파티가 스폰된다"), Harness.SpawnPartyAndGetMemberCount() > 0))
	{
		return false;
	}

	const int32 RevenueBefore = Harness.Progression->GetTodayRevenue();

	Harness.Queue->OnPartyExited.Broadcast(
		Harness.QueueFacilityId, Harness.PartyId, EFF_QueueExit::Abandoned, 30.f);
	Harness.Queue->OnPartyExited.Broadcast(
		Harness.QueueFacilityId, Harness.PartyId, EFF_QueueExit::Cancelled, 30.f);

	TestEqual(TEXT("대기 포기·취소는 매출을 만들지 않는다"),
		Harness.Progression->GetTodayRevenue(), RevenueBefore);
	TestEqual(TEXT("부스별 표도 비어 있다"),
		Harness.Progression->GetRevenueByFacilityRowsForDay(1).Num(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
