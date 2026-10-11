// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_QueueEconomyBridgeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/FF_BaseCrowd.h"
#include "Building/FF_FacilityUpgradeSubsystem.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "Building/Synergy/FF_FacilitySynergySubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "DataTables/FF_FacilityRevenueTableRow.h"
#include "Engine/DataTable.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Economy/Rules/FF_BoothRevenueRules.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/AutomationTest.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Queue/FF_QueueTypes.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Facility/FF_FacilityProgramMarkerSubsystem.h"
#include "UObject/UnrealType.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

/**
 * Queue → 경제 브리지를 검증한다 (S10).
 *
 * 완료 신호 구독, 시설별 요금·효과, 실제 팀 이용 완료에서 원장까지의 정산을 검증한다.
 * 실제 Queue 테스트는 가입 때의 인원 보존, 완료당 한 번 정산, 포기·취소 무과금을 확인한다.
 *
 * 수수료 금액 자체의 규칙(반올림·파티 단위)은 FF_BoothRevenueRulesTests.cpp 가 갖는다.
 */
namespace FFF_QueueEconomyBridgeTestHelpers
{
	// 구독을 이름으로 확인한다. HandlePartyServiceCompleted 는 private 이라 메서드 포인터를 못 쓰므로
	// (IsAlreadyBound 는 &Class::Method 를 요구한다) FMulticastScriptDelegate::Contains 를 쓴다.
	// UFUNCTION 이라 동적 델리게이트가 이름으로 들고 있어서 이쪽으로 조회된다.
	const FName HandlerName(TEXT("HandlePartyServiceCompleted"));

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
		bool Initialize(FAutomationTestBase* Test, int32 StartingGold = InitialGold)
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
			// Legacy-price fixtures must not depend on changes to the production revenue table.
			Definition->FacilityRevenueTable.Reset();

			Definition->FestivalId = TEXT("QueueEconomyAutomationFestival");
			Definition->InitialGold = StartingGold;

			Progression->StartFestival(Definition);
			Queue->SetPaused(true);

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
		bool InstallFoodStall(FString& OutFailure, bool bWithLevels = false,
			const FFFZSFacilitySpec* AuthoredSpec = nullptr, int32 X = 1000)
		{
			FFFZSFacilitySpec Spec =
				FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
			Spec.FacilityDefinitionId = BoothDefinitionId;
			Spec.InstallCostKey = NAME_None;
			Spec.bUsesFacilityLevels = bWithLevels;
			if (bWithLevels)
			{
				FFFZSFacilityUpgradeTier Level2;
				Level2.TargetLevel = 2;
				Level2.ServiceDurationMultiplier = 1.f;
				Level2.RevenueMultiplier = 1.5f;
				FFFZSFacilityUpgradeTier Level3;
				Level3.TargetLevel = 3;
				Level3.CostMultiplier = .6f;
				Level3.ServiceDurationMultiplier = 1.f;
				Level3.RevenueMultiplier = 2.25f;
				Spec.UpgradeTiers = { Level2, Level3 };
			}
			if (AuthoredSpec)
			{
				Spec = *AuthoredSpec;
			}

			FFFZSGridPlacement Placement;
			Placement.OriginCell = FIntPoint(X, 1000);

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
			// 수수료 검증에는 실제 Agent/Party 데이터만 필요하다. 렌더 Actor 풀은 비워
			// 관계없는 군중 메시·애니메이션 로드를 피하고 Agent 생성·파티 인원 조회는 유지한다.
			CrowdSim->InitializeSimulationWithBounds(FVector::ZeroVector, FBox::BuildAABB(FVector::ZeroVector, FVector(10000.f, 10000.f, 10000.f)), 0, SpawnedAgentCount + 4, AFF_BaseCrowd::StaticClass());
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

// 브리지가 월드에 생기고 Queue 의 확정 인원이 담긴 완료 신호를 실제로 구독한다.
// 구독이 빠지면 매출이 0 이 되는데 게임은 정상으로 보이므로, 이 배선 자체를 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_SubscribesToPartyServiceCompleted, "FestaFiesta.Economy.QueueBridge.SubscribesToPartyServiceCompleted", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_SubscribesToPartyServiceCompleted::RunTest(const FString& Parameters)
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

	TestTrue(TEXT("브리지가 확정 인원이 담긴 이용 완료 신호를 구독한다"),
		Queue->OnPartyServiceCompleted.Contains(Bridge, HandlerName));
	TestFalse(TEXT("일반 퇴장 신호는 중복 정산 경로가 아니다"),
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
			Queue->OnPartyServiceCompleted.Contains(Bridge, HandlerName));
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
 * ID 변환(int32→FGuid) · 이름 파싱 · 객단가 조회 · 전달받은 인원 · 원장 기록을 검증한다.
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
	Harness.Queue->OnPartyServiceCompleted.Broadcast(
		Harness.QueueFacilityId, Harness.PartyId, MemberCount, 30.f);

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

	// 설치 시설 인스턴스 장부 — 같은 종류의 부스가 여러 개여도 이 부스 한 곳의 값이다 (시설 정보 패널용).
	FFF_FacilityInstanceRevenue InstanceRevenue;
	TestTrue(TEXT("인스턴스 장부에 이 부스가 있다"),
		Harness.Progression->GetFacilityInstanceRevenue(Harness.FacilityInstanceId, InstanceRevenue));
	TestEqual(TEXT("인스턴스 오늘 매출이 수수료와 같다"), InstanceRevenue.TodayGold, Expected);
	TestEqual(TEXT("인스턴스 누적 매출이 수수료와 같다"), InstanceRevenue.TotalGold, Expected);
	TestEqual(TEXT("인스턴스 결제 건수는 파티 한 번이다"), InstanceRevenue.TodayPaymentCount, 1);

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
	FFF_FacilityInstanceRevenue InstanceRevenue;
	TestFalse(TEXT("인스턴스 장부에도 결제가 없다"),
		Harness.Progression->GetFacilityInstanceRevenue(Harness.FacilityInstanceId, InstanceRevenue));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_FacilityAuthoredRevenue,
	"FestaFiesta.Economy.QueueBridge.FacilityAuthoredRevenue",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_FacilityAuthoredRevenue::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;
	FScopedQueueEconomyTestSettings SettingsGuard;
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("Facility revenue world initializes"), Harness.Initialize(this))) return false;
	Harness.Definition->RevenuePerUseByMinorToken.Add(TEXT("FoodStall"), 20);
	Harness.Definition->BoothCommissionRate = .25f;
	UFF_FacilityUpgradeSubsystem* Upgrades = Harness.World->GetSubsystem<UFF_FacilityUpgradeSubsystem>();
	UFF_FacilityProgramMarkerSubsystem* Markers = Harness.World->GetSubsystem<UFF_FacilityProgramMarkerSubsystem>();
	if (!TestNotNull(TEXT("Upgrade quote owner"), Upgrades) || !TestNotNull(TEXT("Revenue badge owner"), Markers)) return false;

	// Each definition has the same minor category, but supplies its own copied installation spec.
	const int32 AuthoredAmounts[] = {40, 80, 0, 999};
	const int32 EffectiveAmounts[] = {40, 80, 0, 20};
	TArray<TStrongObjectPtr<UFFZSFacilityDefinition>> Assets;
	TArray<FGuid> FacilityIds;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(AuthoredAmounts); ++Index)
	{
		TStrongObjectPtr<UFFZSFacilityDefinition> Asset(NewObject<UFFZSFacilityDefinition>());
		Asset->Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
		Asset->Spec.FacilityDefinitionId = FName(*FString::Printf(TEXT("Booth_FoodStall_AuthoredRevenue%d"), Index));
		Asset->Spec.InstallCostKey = NAME_None;
		Asset->Spec.bOverrideRevenuePerUseGold = Index != 3;
		Asset->Spec.RevenuePerUseGold = AuthoredAmounts[Index];
		Asset->Spec.bUsesFacilityLevels = true;
		FFFZSFacilityUpgradeTier Tier;
		Tier.TargetLevel = 2;
		Tier.ServiceDurationMultiplier = 1.f;
		Tier.RevenueMultiplier = 1.5f;
		Asset->Spec.UpgradeTiers = {Tier};
		FString Failure;
		if (!Harness.InstallFoodStall(Failure, false, &Asset->Spec, 1000 + Index * 1500))
		{
			AddError(Failure);
			return false;
		}
		FacilityIds.Add(Harness.FacilityInstanceId);
		Assets.Add(MoveTemp(Asset));
		FFF_FacilityUpgradePreview Quote;
		if (!TestTrue(TEXT("Every facility exposes its revenue quote"), Upgrades->GetUpgradePreview(FacilityIds.Last(), Quote))) return false;
		TestTrue(TEXT("An explicit free price is still an available quote"), Quote.bRevenueAvailable);
		TestEqual(TEXT("Displayed base income resolves the individual definition"),
			Quote.BaseRevenuePerUseGold, EffectiveAmounts[Index] / 4);
		TestEqual(TEXT("Displayed current income resolves the individual definition"),
			Quote.CurrentRevenuePerUseGold, EffectiveAmounts[Index] / 4);
		TestEqual(TEXT("Displayed upgrade income multiplies the authored base"),
			Quote.NextRevenuePerUseGold, FMath::RoundToInt(EffectiveAmounts[Index] * .25f * 1.5f));
	}

	Markers->Tick(0.f);
	FFF_FacilityWorldBadgeData Badge;
	TestTrue(TEXT("A paid facility shows a zero-to-date revenue badge before its first use"),
		Markers->GetBadgeData(FacilityIds[0], Badge) && Badge.bShowRevenue && Badge.TodayGold == 0);
	TestFalse(TEXT("An explicit free FoodStall does not inherit the paid legacy badge"),
		Markers->GetBadgeData(FacilityIds[2], Badge) && Badge.bShowRevenue);
	const int32 MemberCount = Harness.SpawnPartyAndGetMemberCount();
	if (!TestTrue(TEXT("Real paying party exists"), MemberCount > 0)) return false;
	for (int32 Index = 0; Index < FacilityIds.Num(); ++Index)
	{
		const int32 Before = Harness.Progression->GetTodayRevenue();
		Harness.Queue->OnPartyServiceCompleted.Broadcast(Harness.QueueBridge->FindQueueFacilityId(FacilityIds[Index]),
			Harness.PartyId, MemberCount, 0.f);
		const int32 Expected = EffectiveAmounts[Index] * MemberCount / 4;
		TestEqual(TEXT("Actual settlement uses the same per-facility amount as its quote"),
			Harness.Progression->GetTodayRevenue() - Before, Expected);
		FFF_FacilityInstanceRevenue Ledger;
		const bool bHasLedger = Harness.Progression->GetFacilityInstanceRevenue(FacilityIds[Index], Ledger);
		TestEqual(TEXT("Only paid uses create a facility ledger"), bHasLedger, Expected > 0);
		if (bHasLedger)
		{
			TestEqual(TEXT("Facility ledger records its own payout"), Ledger.TodayGold, Expected);
			Markers->Tick(0.f);
			TestTrue(TEXT("World badge displays the same actual earned amount"),
				Markers->GetBadgeData(FacilityIds[Index], Badge) && Badge.TodayGold == Expected);
		}
	}

	const FFFZSWorldSnapshot Snapshot = Harness.ZoneSystem->CaptureWorldSnapshot();
	Assets[0]->Spec.RevenuePerUseGold = 999;
	TArray<FText> Errors;
	if (!TestTrue(TEXT("Saved facilities restore with their captured revenue settings"),
		Harness.QueueBridge->RestoreZoneWorldSnapshotAndSynchronize(Snapshot, Errors)))
	{
		for (const FText& Error : Errors) AddError(Error.ToString());
		return false;
	}
	const AFFZSFacilityActor* Restored = Harness.ZoneSystem->FindFacility(FacilityIds[0]);
	if (!TestNotNull(TEXT("Restored facility"), Restored)) return false;
	TestTrue(TEXT("Snapshot preserves explicit pricing mode"), Restored->GetFacilitySpec().bOverrideRevenuePerUseGold);
	TestEqual(TEXT("Snapshot uses the installed amount even after the source asset changes"),
		Restored->GetFacilitySpec().RevenuePerUseGold, 40);
	const int32 BeforeRestoredUse = Harness.Progression->GetTodayRevenue();
	Harness.Queue->OnPartyServiceCompleted.Broadcast(Harness.QueueBridge->FindQueueFacilityId(FacilityIds[0]),
		Harness.PartyId, MemberCount, 0.f);
	TestEqual(TEXT("Restored settlement still uses the saved facility amount"),
		Harness.Progression->GetTodayRevenue() - BeforeRestoredUse, 10 * MemberCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_TableRevenueEditsInstalledAndRestoredFacilities,
	"FestaFiesta.Economy.QueueBridge.TableRevenueEditsInstalledAndRestoredFacilities",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_TableRevenueEditsInstalledAndRestoredFacilities::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_QueueEconomyBridgeTestHelpers;
	FScopedQueueEconomyTestSettings SettingsGuard;
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("Table revenue world initializes"), Harness.Initialize(this))) return false;
	Harness.Definition->RevenuePerUseByMinorToken.Add(TEXT("FoodStall"), 20);
	Harness.Definition->BoothCommissionRate = .25f;
	TStrongObjectPtr<UDataTable> Table(NewObject<UDataTable>());
	Table->RowStruct = FFF_FacilityRevenueTableRow::StaticStruct();
	Harness.Definition->FacilityRevenueTable = Table.Get();
	UFF_FacilityUpgradeSubsystem* Upgrades = Harness.World->GetSubsystem<UFF_FacilityUpgradeSubsystem>();
	UFF_FacilityProgramMarkerSubsystem* Markers = Harness.World->GetSubsystem<UFF_FacilityProgramMarkerSubsystem>();
	if (!TestNotNull(TEXT("Upgrade quote owner"), Upgrades) || !TestNotNull(TEXT("Revenue badge owner"), Markers)) return false;

	// Three explicit rows and both compatibility paths share one paid minor category.
	const int32 EffectiveAmounts[] = {48, 80, 0, 64, 20};
	TArray<TStrongObjectPtr<UFFZSFacilityDefinition>> Assets;
	TArray<FGuid> FacilityIds;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(EffectiveAmounts); ++Index)
	{
		TStrongObjectPtr<UFFZSFacilityDefinition> Asset(NewObject<UFFZSFacilityDefinition>());
		Asset->Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
		Asset->Spec.FacilityDefinitionId = FName(*FString::Printf(TEXT("Booth_FoodStall_TableRevenue%d"), Index));
		Asset->Spec.InstallCostKey = NAME_None;
		Asset->Spec.bOverrideRevenuePerUseGold = Index != 4;
		Asset->Spec.RevenuePerUseGold = Index == 3 ? 64 : 120;
		Asset->Spec.bUsesFacilityLevels = true;
		FFFZSFacilityUpgradeTier Tier;
		Tier.TargetLevel = 2;
		Tier.ServiceDurationMultiplier = 1.f;
		Tier.RevenueMultiplier = 1.5f;
		Asset->Spec.UpgradeTiers = {Tier};
		if (Index < 3)
		{
			FFF_FacilityRevenueTableRow Row;
			Row.RevenuePerUseGold = EffectiveAmounts[Index];
			if (Index == 0)
			{
				Row.bOverrideCommissionRate = true;
				Row.CommissionRate = .5f;
				Row.RevenueMultiplierByLevel = {{1, .75f}, {2, 2.f}};
			}
			Table->AddRow(Asset->Spec.FacilityDefinitionId, Row);
		}
		FString Failure;
		if (!Harness.InstallFoodStall(Failure, false, &Asset->Spec, 1000 + Index * 1500))
		{
			AddError(Failure);
			return false;
		}
		FacilityIds.Add(Harness.FacilityInstanceId);
		Assets.Add(MoveTemp(Asset));
		FFF_FacilityUpgradePreview Quote;
		if (!TestTrue(TEXT("Every table and fallback facility exposes a quote"), Upgrades->GetUpgradePreview(FacilityIds.Last(), Quote))) return false;
		TestTrue(TEXT("Zero authored revenue is an available quote"), Quote.bRevenueAvailable);
		const int32 BaseQuote = Index == 0 ? 18 : EffectiveAmounts[Index] / 4;
		TestEqual(TEXT("The quote combines the row's commission and level-one multiplier"), Quote.BaseRevenuePerUseGold, BaseQuote);
		TestEqual(TEXT("Current quote agrees with the base at level one"), Quote.CurrentRevenuePerUseGold, BaseQuote);
		TestEqual(TEXT("The next-level quote multiplies table revenue before rounding"),
			Quote.NextRevenuePerUseGold, Index == 0 ? 48 : FMath::RoundToInt(EffectiveAmounts[Index] * .25f * 1.5f));
	}
	Markers->Tick(0.f);
	FFF_FacilityWorldBadgeData Badge;
	TestTrue(TEXT("A paid table row displays a revenue badge before earning"),
		Markers->GetBadgeData(FacilityIds[0], Badge) && Badge.bShowRevenue && Badge.TodayGold == 0);
	TestFalse(TEXT("A free table row suppresses the old paid Data Asset badge"),
		Markers->GetBadgeData(FacilityIds[2], Badge) && Badge.bShowRevenue);
	FText UpgradeFailure;
	if (!TestTrue(TEXT("A table-priced facility can upgrade"), Upgrades->TryUpgradeFacility(FacilityIds[0], 1, UpgradeFailure)))
	{
		AddError(UpgradeFailure.ToString());
		return false;
	}
	const int32 MemberCount = Harness.SpawnPartyAndGetMemberCount();
	if (!TestTrue(TEXT("A real party supplies the settlement member count"), MemberCount > 0)) return false;
	for (int32 Index = 0; Index < FacilityIds.Num(); ++Index)
	{
		const int32 Before = Harness.Progression->GetTodayRevenue();
		Harness.Queue->OnPartyServiceCompleted.Broadcast(Harness.QueueBridge->FindQueueFacilityId(FacilityIds[Index]),
			Harness.PartyId, MemberCount, 0.f);
		const int32 Expected = FFF_BoothRevenueRules::ResolveCommission(
			EffectiveAmounts[Index], MemberCount, Index == 0 ? .5f : .25f, Index == 0 ? 2.f : 1.f);
		TestEqual(TEXT("Settlement consumes the same table/fallback value as the quote"),
			Harness.Progression->GetTodayRevenue() - Before, Expected);
		FFF_FacilityInstanceRevenue Ledger;
		const bool bHasLedger = Harness.Progression->GetFacilityInstanceRevenue(FacilityIds[Index], Ledger);
		TestEqual(TEXT("Free uses do not create a paid ledger entry"), bHasLedger, Expected > 0);
		if (bHasLedger) TestEqual(TEXT("Each installation records its own actual income"), Ledger.TodayGold, Expected);
	}

	const FFFZSWorldSnapshot Snapshot = Harness.ZoneSystem->CaptureWorldSnapshot();
	const int32 EarnedBeforeEdit = Harness.Progression->GetTodayRevenue();
	FFF_FacilityRevenueTableRow Edited;
	Edited.RevenuePerUseGold = 100;
	Edited.bOverrideCommissionRate = true;
	Edited.CommissionRate = .25f;
	Edited.RevenueMultiplierByLevel = {{1, .75f}, {2, 3.f}};
	Table->AddRow(Assets[0]->Spec.FacilityDefinitionId, Edited);
	Edited = FFF_FacilityRevenueTableRow();
	Edited.RevenuePerUseGold = 36;
	Table->AddRow(Assets[2]->Spec.FacilityDefinitionId, Edited);
	Markers->Tick(0.f);
	TestTrue(TEXT("Editing zero to paid refreshes a previously free badge without a new payment"),
		Markers->GetBadgeData(FacilityIds[2], Badge) && Badge.bShowRevenue && Badge.TodayGold == 0);
	TestEqual(TEXT("Balance edits never rewrite already earned money"), Harness.Progression->GetTodayRevenue(), EarnedBeforeEdit);
	FFF_FacilityUpgradePreview EditedQuote;
	if (!TestTrue(TEXT("The existing upgraded facility refreshes its quote"), Upgrades->GetUpgradePreview(FacilityIds[0], EditedQuote))) return false;
	TestEqual(TEXT("The installed facility uses its edited base, rate and level-one multiplier"), EditedQuote.BaseRevenuePerUseGold, 19);
	TestEqual(TEXT("Its current level immediately uses the edited multiplier before rounding"), EditedQuote.CurrentRevenuePerUseGold, 75);
	const int32 ChangedCommission = FFF_BoothRevenueRules::ResolveCommission(100, MemberCount, .25f, 3.f);
	Harness.Queue->OnPartyServiceCompleted.Broadcast(Harness.QueueBridge->FindQueueFacilityId(FacilityIds[0]), Harness.PartyId, MemberCount, 0.f);
	TestEqual(TEXT("The next use pays the edited price without reinstalling"),
		Harness.Progression->GetTodayRevenue(), EarnedBeforeEdit + ChangedCommission);

	Assets[0]->Spec.RevenuePerUseGold = 999;
	TArray<FText> Errors;
	if (!TestTrue(TEXT("A snapshot captured before the balance edit restores"),
		Harness.QueueBridge->RestoreZoneWorldSnapshotAndSynchronize(Snapshot, Errors)))
	{
		for (const FText& Error : Errors) AddError(Error.ToString());
		return false;
	}
	const AFFZSFacilityActor* Restored = Harness.ZoneSystem->FindFacility(FacilityIds[0]);
	if (!TestNotNull(TEXT("The saved facility exists again"), Restored)) return false;
	TestEqual(TEXT("The old save still contains its original per-asset amount"), Restored->GetFacilitySpec().RevenuePerUseGold, 120);
	TestEqual(TEXT("The saved upgrade multiplier is preserved"), Restored->GetUpgradeRevenueMultiplier(), 1.5f);
	if (!TestTrue(TEXT("The restored facility exposes a fresh quote"), Upgrades->GetUpgradePreview(FacilityIds[0], EditedQuote))) return false;
	TestEqual(TEXT("The table supersedes both the saved price and multiplier after restore"), EditedQuote.CurrentRevenuePerUseGold, 75);
	const int32 BeforeRestoredUse = Harness.Progression->GetTodayRevenue();
	Harness.Queue->OnPartyServiceCompleted.Broadcast(Harness.QueueBridge->FindQueueFacilityId(FacilityIds[0]), Harness.PartyId, MemberCount, 0.f);
	TestEqual(TEXT("The restored facility settles at the newly edited table price"),
		Harness.Progression->GetTodayRevenue() - BeforeRestoredUse, ChangedCommission);
	Markers->Tick(0.f);
	FFF_FacilityInstanceRevenue RestoredLedger;
	TestTrue(TEXT("Restored settlement retains its per-installation ledger"), Harness.Progression->GetFacilityInstanceRevenue(FacilityIds[0], RestoredLedger));
	TestTrue(TEXT("The badge displays the actual ledger total after restore"),
		Markers->GetBadgeData(FacilityIds[0], Badge) && Badge.TodayGold == RestoredLedger.TodayGold);
	Edited.bOverrideCommissionRate = true;
	Edited.CommissionRate = 0.f;
	Table->AddRow(Assets[2]->Spec.FacilityDefinitionId, Edited);
	Markers->Tick(0.f);
	TestFalse(TEXT("Zero table commission suppresses an unearned facility's income badge"),
		Markers->GetBadgeData(FacilityIds[2], Badge) && Badge.bShowRevenue);
	Edited.CommissionRate = .25f;
	Edited.RevenueMultiplierByLevel.Add(1, 0.f);
	Table->AddRow(Assets[2]->Spec.FacilityDefinitionId, Edited);
	Markers->Tick(0.f);
	TestFalse(TEXT("Zero table level multiplier also suppresses an unearned income badge"),
		Markers->GetBadgeData(FacilityIds[2], Badge) && Badge.bShowRevenue);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_UpgradeRevenueCompletesAndRestores,
	"FestaFiesta.Economy.QueueBridge.UpgradeRevenueCompletesAndRestores",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_UpgradeRevenueCompletesAndRestores::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;
	FScopedQueueEconomyTestSettings SettingsGuard;
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("Revenue test world initializes"), Harness.Initialize(this))) return false;
	FString InstallFailure;
	const bool bInstalled = Harness.InstallFoodStall(InstallFailure, true);
	if (!TestTrue(*FString::Printf(TEXT("Upgradeable booth installs: %s"), *InstallFailure), bInstalled)) return false;
	UFF_FacilityUpgradeSubsystem* Upgrades = Harness.World->GetSubsystem<UFF_FacilityUpgradeSubsystem>();
	UFF_FestivalStageSubsystem* Stage = Harness.World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>();
	if (!TestNotNull(TEXT("Upgrade authority exists"), Upgrades) || !TestNotNull(TEXT("Stage exists"), Stage)) return false;
	Stage->CurrentStage = EFF_FestivalStage::Setup;
	FText Failure;
	if (!TestTrue(TEXT("A revenue-only level can be purchased"), Upgrades->TryUpgradeFacility(Harness.FacilityInstanceId, 1, Failure)))
	{
		AddError(Failure.ToString());
		return false;
	}
	TestEqual(TEXT("First purchase grants an absolute 1.5 revenue multiplier"),
		Harness.ZoneSystem->FindFacility(Harness.FacilityInstanceId)->GetUpgradeRevenueMultiplier(), 1.5f);
	const int32 MemberCount = Harness.SpawnPartyAndGetMemberCount();
	if (!TestTrue(TEXT("A real party is available for completion"), MemberCount > 0)) return false;
	const int32 RevenuePerUse = FFF_BoothRevenueRules::FindRevenuePerUse(
		Harness.Definition->RevenuePerUseByMinorToken, TEXT("FoodStall"));
	const auto ExecuteLifecycle = [&](EFFZSCommandType Type)
	{
		FFFZSCommand Command;
		Command.CommandId = FGuid::NewGuid();
		Command.TransactionId = FGuid::NewGuid();
		Command.ExpectedWorldRevision = Harness.ZoneSystem->GetWorldRevision();
		Command.Type = Type;
		Command.FacilityInstanceId = Harness.FacilityInstanceId;
		return Harness.ZoneSystem->ExecuteCommand(Command).bSucceeded;
	};
	const auto CompleteUse = [&]()
	{
		FFF_JoinRequest Join;
		Join.PartyId = Harness.PartyId;
		Join.MemberCount = MemberCount;
		if (!Harness.QueueBridge->TryJoinZoneFacility(Harness.FacilityInstanceId, Join).bAccepted) return false;
		Harness.QueueBridge->NotifyZoneFacilityPartyArrived(Harness.FacilityInstanceId, Join.PartyId);
		Harness.Queue->AdvanceFixedSteps(1500);
		return Harness.QueueBridge->GetZoneFacilityQueueState(Harness.FacilityInstanceId).OccupiedMemberCount == 0;
	};
	const int32 InitialRevenue = Harness.Progression->GetTodayRevenue();
	Harness.Queue->OnPartyExited.Broadcast(Harness.QueueFacilityId, Harness.PartyId, EFF_QueueExit::Abandoned, 0.f);
	Harness.Queue->OnPartyExited.Broadcast(Harness.QueueFacilityId, Harness.PartyId, EFF_QueueExit::Cancelled, 0.f);
	TestEqual(TEXT("Upgraded abandoned and cancelled parties still earn nothing"),
		Harness.Progression->GetTodayRevenue(), InitialRevenue);
	if (!TestTrue(TEXT("Upgraded booth opens"), ExecuteLifecycle(EFFZSCommandType::Open))
		|| !TestTrue(TEXT("Real service produces its completion event"), CompleteUse())) return false;
	const int32 Level2Commission = FFF_BoothRevenueRules::ResolveCommission(
		RevenuePerUse, MemberCount, Harness.Definition->BoothCommissionRate, 1.5f);
	TestEqual(TEXT("Actual completion pays the authored 1.5 multiplier"),
		Harness.Progression->GetTodayRevenue(), InitialRevenue + Level2Commission);
	if (!TestTrue(TEXT("Empty booth closes normally"), ExecuteLifecycle(EFFZSCommandType::Close))
		|| !TestTrue(TEXT("Second revenue-only level can be purchased"),
			Upgrades->TryUpgradeFacility(Harness.FacilityInstanceId, 2, Failure))) return false;
	const FFFZSWorldSnapshot Snapshot = Harness.ZoneSystem->CaptureWorldSnapshot();
	TArray<FText> RestoreErrors;
	if (!TestTrue(TEXT("Snapshot restores revenue and the Party queue together"),
		Harness.QueueBridge->RestoreZoneWorldSnapshotAndSynchronize(Snapshot, RestoreErrors)))
	{
		for (const FText& Error : RestoreErrors) AddError(Error.ToString());
		return false;
	}
	TestEqual(TEXT("Snapshot retains the stable queue ID"),
		Harness.QueueBridge->FindQueueFacilityId(Harness.FacilityInstanceId), Harness.QueueFacilityId);
	TestEqual(TEXT("Snapshot restores 2.25 as an absolute multiplier"),
		Harness.ZoneSystem->FindFacility(Harness.FacilityInstanceId)->GetUpgradeRevenueMultiplier(), 2.25f);
	if (!TestTrue(TEXT("Restored upgraded booth opens"), ExecuteLifecycle(EFFZSCommandType::Open))
		|| !TestTrue(TEXT("Real service completes after restore"), CompleteUse())) return false;
	const int32 Level3Commission = FFF_BoothRevenueRules::ResolveCommission(
		RevenuePerUse, MemberCount, Harness.Definition->BoothCommissionRate, 2.25f);
	TestEqual(TEXT("Completion after restore pays 2.25 rather than compounding 1.5"),
		Harness.Progression->GetTodayRevenue(), InitialRevenue + Level2Commission + Level3Commission);
	Harness.Definition->RevenuePerUseByMinorToken.Add(TEXT("FoodStall"), 0);
	const int32 BeforeFreeUse = Harness.Progression->GetTodayRevenue();
	if (!TestTrue(TEXT("The same upgraded facility can complete a free use"), CompleteUse())) return false;
	TestEqual(TEXT("An upgraded free use does not add revenue"), Harness.Progression->GetTodayRevenue(), BeforeFreeUse);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_UpgradeRevenueRespectsFundsCapacity,
	"FestaFiesta.Economy.QueueBridge.UpgradeRevenueRespectsFundsCapacity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_UpgradeRevenueRespectsFundsCapacity::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;
	FScopedQueueEconomyTestSettings SettingsGuard;
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("High-balance revenue world initializes"), Harness.Initialize(this, MAX_int32 - 100))) return false;
	FString InstallFailure;
	if (!Harness.InstallFoodStall(InstallFailure, true))
	{
		AddError(InstallFailure);
		return false;
	}
	UFF_FacilityUpgradeSubsystem* Upgrades = Harness.World->GetSubsystem<UFF_FacilityUpgradeSubsystem>();
	FText Failure;
	if (!TestTrue(TEXT("High-balance booth upgrades"), Upgrades
		&& Upgrades->TryUpgradeFacility(Harness.FacilityInstanceId, 1, Failure))) return false;
	const int32 MemberCount = Harness.SpawnPartyAndGetMemberCount();
	if (!TestTrue(TEXT("Real Party data exists for the high-value payout"), MemberCount > 0)) return false;
	Harness.Definition->RevenuePerUseByMinorToken.Add(TEXT("FoodStall"), MAX_int32);
	const int32 GoldBefore = Harness.Progression->GetGold();
	const int32 AcceptedAmount = MAX_int32 - GoldBefore;
	const int32 QuotedAmount = FFF_BoothRevenueRules::ResolveCommission(
		MAX_int32, MemberCount, Harness.Definition->BoothCommissionRate, 1.5f);
	if (!TestTrue(TEXT("Upgraded commission exceeds the remaining balance capacity"), QuotedAmount > AcceptedAmount)) return false;
	Harness.Queue->OnPartyServiceCompleted.Broadcast(Harness.QueueFacilityId, Harness.PartyId, MemberCount, 0.f);
	TestEqual(TEXT("The balance saturates without becoming negative"), Harness.Progression->GetGold(), MAX_int32);
	TestEqual(TEXT("Total revenue records only the accepted amount"), Harness.Progression->GetTotalRevenue(), AcceptedAmount);
	TestEqual(TEXT("Today's revenue records the same accepted amount"), Harness.Progression->GetTodayRevenue(), AcceptedAmount);
	int32 LedgerCommission = 0;
	for (const FFF_LedgerRow& Row : Harness.Progression->GetEarnRowsForDay(1))
	{
		if (Row.ReasonId == TEXT("FacilityCommission")) LedgerCommission = Row.Amount;
	}
	TestEqual(TEXT("The ledger matches the amount credited to funds"), LedgerCommission, AcceptedAmount);
	const TArray<FFF_FacilityRevenueRow> FacilityRows = Harness.Progression->GetRevenueByFacilityRowsForDay(1);
	if (!TestEqual(TEXT("There is one facility revenue row"), FacilityRows.Num(), 1)) return false;
	TestEqual(TEXT("The facility breakdown also records the accepted amount"), FacilityRows[0].Amount, AcceptedAmount);
	Harness.Queue->OnPartyServiceCompleted.Broadcast(Harness.QueueFacilityId, Harness.PartyId, MemberCount, 0.f);
	TestEqual(TEXT("Further payouts at the balance limit leave funds unchanged"), Harness.Progression->GetGold(), MAX_int32);
	TestEqual(TEXT("Rejected overflow is not reported as earned revenue"), Harness.Progression->GetTotalRevenue(), AcceptedAmount);
	const TArray<FFF_FacilityRevenueRow> RowsAfterRejectedPayout = Harness.Progression->GetRevenueByFacilityRowsForDay(1);
	if (!TestEqual(TEXT("Rejected overflow does not append another facility row"), RowsAfterRejectedPayout.Num(), 1)) return false;
	TestEqual(TEXT("Rejected overflow does not increase the existing ledger row"), RowsAfterRejectedPayout[0].Amount, AcceptedAmount);
	return true;
}

// One end-to-end contract: preview, authority, integer commission, movement and restore agree.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_SynergyPreviewPaymentAndLifecycle,
	"FestaFiesta.Building.Synergy.PreviewPaymentAndLifecycle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_SynergyPreviewPaymentAndLifecycle::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;
	FScopedQueueEconomyTestSettings SettingsGuard;
	TStrongObjectPtr<UDataTable> SynergyTable(NewObject<UDataTable>());
	SynergyTable->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	const FName PartnerDefinitionId(TEXT("Booth_FoodStall_SynergyPartner"));
	FFF_FacilitySynergyRow Rule;
	Rule.DisplayName = FText::FromString(TEXT("Test food pair"));
	Rule.TargetFacilityId = BoothDefinitionId;
	Rule.PartnerFacilityId = PartnerDefinitionId;
	Rule.MaxDistanceMeters = 15.f;
	Rule.RevenueBonusPercent = 25.f;
	Rule.StackingGroup = TEXT("FoodComfort");
	SynergyTable->AddRow(TEXT("FoodComfort"), Rule);
	UFF_FacilitySynergySettings* Settings = GetMutableDefault<UFF_FacilitySynergySettings>();
	TGuardValue<TSoftObjectPtr<UDataTable>> TableGuard(Settings->SynergyTable, TSoftObjectPtr<UDataTable>(SynergyTable.Get()));
	TGuardValue<float> CapGuard(Settings->MaxTotalBonusPercent, 50.f);
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("Synergy payment world initializes"), Harness.Initialize(this))) return false;
	Harness.Definition->RevenuePerUseByMinorToken.Add(TEXT("FoodStall"), 20);
	Harness.Definition->BoothCommissionRate = .2f;
	FFFZSFacilitySpec AuthoredSpec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	AuthoredSpec.FacilityDefinitionId = BoothDefinitionId;
	AuthoredSpec.InstallCostKey = NAME_None;
	AuthoredSpec.bOverrideRevenuePerUseGold = true;
	AuthoredSpec.RevenuePerUseGold = 100;
	AuthoredSpec.bUsesFacilityLevels = true;
	FFFZSFacilityUpgradeTier AuthoredTier;
	AuthoredTier.TargetLevel = 2;
	AuthoredTier.ServiceDurationMultiplier = 1.f;
	AuthoredTier.RevenueMultiplier = 1.5f;
	AuthoredSpec.UpgradeTiers = {AuthoredTier};
	FString Failure;
	if (!Harness.InstallFoodStall(Failure, true, &AuthoredSpec)) { AddError(Failure); return false; }
	UFF_FacilitySynergySubsystem* Synergy = Harness.World->GetSubsystem<UFF_FacilitySynergySubsystem>();
	UFF_FacilityUpgradeSubsystem* Upgrades = Harness.World->GetSubsystem<UFF_FacilityUpgradeSubsystem>();
	if (!TestNotNull(TEXT("Synergy subsystem exists"), Synergy) || !TestNotNull(TEXT("Upgrade subsystem exists"), Upgrades)) return false;
	FText UpgradeFailure;
	if (!TestTrue(TEXT("Revenue-only upgrade applies"), Upgrades->TryUpgradeFacility(Harness.FacilityInstanceId, 1, UpgradeFailure))) return false;
	TestEqual(TEXT("No partner means no bonus"), Synergy->GetBonusPercent(Harness.FacilityInstanceId), 0.f);
	const FVector TargetLocation = Harness.ZoneSystem->FindFacility(Harness.FacilityInstanceId)->GetActorLocation();
	FFF_FacilitySynergyPreview Preview;
	Synergy->BuildPlacementPreview(PartnerDefinitionId, TargetLocation + FVector(1500, 0, 0), FGuid(), Preview);
	if (!TestEqual(TEXT("Placing the helper explains the affected revenue facility"), Preview.Entries.Num(), 1)) return false;
	TestEqual(TEXT("Preview includes the exact radius boundary"), Preview.Entries[0].ProposedTotalBonusPercent, 25.f);
	TestEqual(TEXT("Preview does not mutate actual income"), Synergy->GetBonusPercent(Harness.FacilityInstanceId), 0.f);

	FFFZSFacilitySpec PartnerSpec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	PartnerSpec.FacilityDefinitionId = PartnerDefinitionId;
	PartnerSpec.InstallCostKey = NAME_None;
	FFFZSGridPlacement PartnerPlacement;
	PartnerPlacement.OriginCell = FIntPoint(2500, 1000);
	const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(PartnerSpec, TestZoneId, PartnerPlacement, Harness.ZoneSystem->GetWorldRevision());
	const FFFZSCommandResult InstallResult = Harness.ZoneSystem->ExecuteCommand(Install);
	if (!TestTrue(*FString::Printf(TEXT("Partner installs: %s"), *InstallResult.Message.ToString()), InstallResult.bSucceeded)) return false;
	TestEqual(TEXT("Installed result agrees with preview"), Synergy->GetBonusPercent(Harness.FacilityInstanceId), 25.f);
	TestEqual(TEXT("One-way rule grants no income to the helper"), Synergy->GetBonusPercent(Install.FacilityInstanceId), 0.f);
	FFF_FacilityUpgradePreview Quote;
	if (!TestTrue(TEXT("Facility quote remains available"), Upgrades->GetUpgradePreview(Harness.FacilityInstanceId, Quote))) return false;
	TestEqual(TEXT("Quote combines authored base, upgrade and synergy before rounding"), Quote.CurrentRevenuePerUseGold,
		FFF_BoothRevenueRules::ResolveCommission(100, 1, .2f, 1.5f * 1.25f));
	const int32 MemberCount = Harness.SpawnPartyAndGetMemberCount();
	if (!TestTrue(TEXT("Real party data exists"), MemberCount > 0)) return false;
	const int32 RevenueBefore = Harness.Progression->GetTodayRevenue();
	Harness.Queue->OnPartyServiceCompleted.Broadcast(Harness.QueueFacilityId, Harness.PartyId, MemberCount, 0.f);
	TestEqual(TEXT("Paid completion uses the authored base and combined multiplier"), Harness.Progression->GetTodayRevenue() - RevenueBefore,
		FFF_BoothRevenueRules::ResolveCommission(100, MemberCount, .2f, 1.5f * 1.25f));
	FFFZSWorldSnapshot Snapshot = Harness.ZoneSystem->CaptureWorldSnapshot();
	const uint64 RegistryBefore = Harness.ZoneSystem->GetFacilityRegistryRevision();
	FFFZSCommand Move;
	Move.CommandId = FGuid::NewGuid();
	Move.TransactionId = FGuid::NewGuid();
	Move.Type = EFFZSCommandType::Move;
	Move.ExpectedWorldRevision = Harness.ZoneSystem->GetWorldRevision();
	Move.FacilityInstanceId = Install.FacilityInstanceId;
	Move.ZoneId = TestZoneId;
	Move.Placement.OriginCell = FIntPoint(5000, 1000);
	if (!TestTrue(TEXT("Partner moves outside the radius"), Harness.ZoneSystem->ExecuteCommand(Move).bSucceeded)) return false;
	TestEqual(TEXT("Move invalidates the revenue cache"), Synergy->GetBonusPercent(Harness.FacilityInstanceId), 0.f);
	// Reuse the current transaction revision with different saved placements. A cache keyed
	// only by WorldRevision would retain the incorrect zero bonus after this restore.
	Snapshot.WorldRevision = Harness.ZoneSystem->GetWorldRevision();
	TArray<FText> RestoreErrors;
	if (!TestTrue(TEXT("Authoritative snapshot restores"), Harness.QueueBridge->RestoreZoneWorldSnapshotAndSynchronize(Snapshot, RestoreErrors)))
	{
		for (const FText& Error : RestoreErrors) AddError(Error.ToString());
		return false;
	}
	TestTrue(TEXT("Registry generation never rewinds with saved world revision"), Harness.ZoneSystem->GetFacilityRegistryRevision() > RegistryBefore);
	TestEqual(TEXT("Restore rebuilds derived income instead of retaining the moved cache"), Synergy->GetBonusPercent(Harness.FacilityInstanceId), 25.f);
	FFFZSCommand Remove;
	Remove.CommandId = FGuid::NewGuid();
	Remove.TransactionId = FGuid::NewGuid();
	Remove.Type = EFFZSCommandType::Remove;
	Remove.ExpectedWorldRevision = Harness.ZoneSystem->GetWorldRevision();
	Remove.FacilityInstanceId = Install.FacilityInstanceId;
	if (!TestTrue(TEXT("Restored helper can be removed"), Harness.ZoneSystem->ExecuteCommand(Remove).bSucceeded)) return false;
	TestEqual(TEXT("Removal invalidates the income cache"), Synergy->GetBonusPercent(Harness.FacilityInstanceId), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_TeamCompletionUsesCapturedMembers,
	"FestaFiesta.Economy.QueueBridge.TeamCompletionUsesCapturedMembers",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_TeamCompletionUsesCapturedMembers::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;
	FScopedQueueEconomyTestSettings SettingsGuard;
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("Team settlement world initializes"), Harness.Initialize(this))) return false;
	Harness.Definition->BoothCommissionRate = .25f;
	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	Spec.FacilityDefinitionId = BoothDefinitionId;
	Spec.InstallCostKey = NAME_None;
	Spec.bOverrideRevenuePerUseGold = true;
	Spec.RevenuePerUseGold = 100;
	Spec.QueueRules.ServiceCapacity = 2;
	Spec.QueueRules.ServiceDurationSeconds = 1.f;
	const FFFZSAnchorDefinition* FirstServiceAnchor = Spec.Anchors.FindByPredicate(
		[](const FFFZSAnchorDefinition& Anchor) { return Anchor.Type == EFFZSAnchorType::Service && Anchor.Index == 0; });
	if (!TestNotNull(TEXT("The fixture provides service anchor zero"), FirstServiceAnchor)) return false;
	FFFZSAnchorDefinition SecondServiceAnchor = *FirstServiceAnchor;
	SecondServiceAnchor.Index = 1;
	SecondServiceAnchor.LocalTransform.AddToTranslation(FVector(-100.f, 0.f, 0.f));
	Spec.Anchors.Add(SecondServiceAnchor);
	FString Failure;
	if (!Harness.InstallFoodStall(Failure, false, &Spec)) { AddError(Failure); return false; }
	FFFZSCommand Open;
	Open.CommandId = FGuid::NewGuid();
	Open.TransactionId = FGuid::NewGuid();
	Open.Type = EFFZSCommandType::Open;
	Open.ExpectedWorldRevision = Harness.ZoneSystem->GetWorldRevision();
	Open.FacilityInstanceId = Harness.FacilityInstanceId;
	if (!TestTrue(TEXT("Team booth opens"), Harness.ZoneSystem->ExecuteCommand(Open).bSucceeded)) return false;
	Harness.Queue->SimulationStepSeconds = .05f;

	// Deterministic queue requests have no live Crowd record. Completion must carry its
	// admitted roster itself rather than asking Crowd for a potentially changed party.
	FFF_JoinRequest Solo;
	Solo.PartyId = 400001;
	Solo.MemberCount = 1;
	FFF_JoinRequest Family;
	Family.PartyId = 400002;
	Family.MemberCount = 4;
	if (!TestTrue(TEXT("One-person team is admitted"),
		Harness.QueueBridge->TryJoinZoneFacility(Harness.FacilityInstanceId, Solo).bAccepted)
		|| !TestTrue(TEXT("Four-person team is admitted"),
			Harness.QueueBridge->TryJoinZoneFacility(Harness.FacilityInstanceId, Family).bAccepted)) return false;
	TestEqual(TEXT("Both teams retain five people for occupancy"),
		Harness.Queue->GetQueueState(Harness.QueueFacilityId).OccupiedMemberCount, 5);
	Family.MemberCount = 99;
	const int32 RevenueBefore = Harness.Progression->GetTodayRevenue();
	Harness.QueueBridge->NotifyZoneFacilityPartyArrived(Harness.FacilityInstanceId, Solo.PartyId);
	Harness.QueueBridge->NotifyZoneFacilityPartyArrived(Harness.FacilityInstanceId, Family.PartyId);
	Harness.Queue->AdvanceFixedSteps(19);
	TestEqual(TEXT("Admission and service start do not charge"), Harness.Progression->GetTodayRevenue(), RevenueBefore);
	Harness.Queue->AdvanceFixedSteps(2);
	TestEqual(TEXT("Both teams finish within the same one-second service interval"),
		Harness.Queue->GetQueueState(Harness.QueueFacilityId).OccupiedMemberCount, 0);
	TestEqual(TEXT("Completed one-person and four-person teams pay for exactly five people"),
		Harness.Progression->GetTodayRevenue() - RevenueBefore, 125);
	FFF_FacilityInstanceRevenue Revenue;
	if (!TestTrue(TEXT("Team payments enter the facility ledger"),
		Harness.Progression->GetFacilityInstanceRevenue(Harness.FacilityInstanceId, Revenue))) return false;
	TestEqual(TEXT("There are two payments, independent of the number of people"), Revenue.TodayPaymentCount, 2);
	Harness.Queue->FlushPendingEvents();
	Harness.Queue->AdvanceFixedSteps(40);
	TestEqual(TEXT("Flushing and advancing completed teams cannot pay again"),
		Harness.Progression->GetTodayRevenue() - RevenueBefore, 125);

	// Reusing the party ID for a genuinely new visit remains a new paid completion.
	Family.MemberCount = 4;
	if (!TestTrue(TEXT("A completed team may visit again"),
		Harness.QueueBridge->TryJoinZoneFacility(Harness.FacilityInstanceId, Family).bAccepted)) return false;
	Harness.QueueBridge->NotifyZoneFacilityPartyArrived(Harness.FacilityInstanceId, Family.PartyId);
	Harness.Queue->AdvanceFixedSteps(21);
	TestEqual(TEXT("A new completed visit pays once using its own captured count"),
		Harness.Progression->GetTodayRevenue() - RevenueBefore, 225);
	Harness.Progression->GetFacilityInstanceRevenue(Harness.FacilityInstanceId, Revenue);
	TestEqual(TEXT("The repeat visit creates exactly one additional payment"), Revenue.TodayPaymentCount, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_UnfinishedTeamsNeverPay,
	"FestaFiesta.Economy.QueueBridge.UnfinishedTeamsNeverPay",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_UnfinishedTeamsNeverPay::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;
	FScopedQueueEconomyTestSettings SettingsGuard;
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("Unfinished-team world initializes"), Harness.Initialize(this))) return false;
	FString Failure;
	if (!Harness.InstallFoodStall(Failure)) { AddError(Failure); return false; }
	const auto ChangeLifecycle = [&](EFFZSCommandType Type)
	{
		FFFZSCommand Command;
		Command.CommandId = FGuid::NewGuid();
		Command.TransactionId = FGuid::NewGuid();
		Command.Type = Type;
		Command.ExpectedWorldRevision = Harness.ZoneSystem->GetWorldRevision();
		Command.FacilityInstanceId = Harness.FacilityInstanceId;
		return Harness.ZoneSystem->ExecuteCommand(Command).bSucceeded;
	};
	if (!TestTrue(TEXT("Unfinished-team booth opens"), ChangeLifecycle(EFFZSCommandType::Open))) return false;
	const int32 RevenueBefore = Harness.Progression->GetTodayRevenue();
	FFF_JoinRequest Request;
	Request.PartyId = 500001;
	Request.MemberCount = 3;
	Request.MaxWaitSeconds = .001f;
	if (!TestTrue(TEXT("Waiting team joins"),
		Harness.QueueBridge->TryJoinZoneFacility(Harness.FacilityInstanceId, Request).bAccepted)) return false;
	Harness.Queue->AdvanceFixedSteps(1);
	Request.PartyId = 500002;
	Request.MemberCount = 4;
	Request.MaxWaitSeconds = 600.f;
	if (!TestTrue(TEXT("Next team joins"),
		Harness.QueueBridge->TryJoinZoneFacility(Harness.FacilityInstanceId, Request).bAccepted)) return false;
	Harness.QueueBridge->NotifyZoneFacilityPartyArrived(Harness.FacilityInstanceId, Request.PartyId);
	Harness.Queue->AdvanceFixedSteps(1);
	if (!TestTrue(TEXT("Emergency closure cancels the active team"),
		ChangeLifecycle(EFFZSCommandType::EmergencyClose))) return false;
	Harness.Queue->FlushPendingEvents();
	Harness.Queue->AdvanceFixedSteps(1500);
	TestEqual(TEXT("Abandonment and active-service cancellation produce no income"),
		Harness.Progression->GetTodayRevenue(), RevenueBefore);
	FFF_FacilityInstanceRevenue Revenue;
	TestFalse(TEXT("No payment entry exists for either unfinished team"),
		Harness.Progression->GetFacilityInstanceRevenue(Harness.FacilityInstanceId, Revenue));
	const TArray<FFF_QueueLog>& Logs = Harness.Queue->GetQueueLogsRef();
	if (!TestEqual(TEXT("Both unfinished visits have exit records"), Logs.Num(), 2)) return false;
	TestEqual(TEXT("The first team abandoned"), Logs[0].ExitType, EFF_QueueExit::Abandoned);
	TestEqual(TEXT("The second team was cancelled"), Logs[1].ExitType, EFF_QueueExit::Cancelled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_EmptyEntranceDiscoversAndPaysBusiness,
	"FestaFiesta.Economy.QueueBridge.EmptyEntranceDiscoversAndPaysBusiness",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_EmptyEntranceDiscoversAndPaysBusiness::RunTest(const FString& Parameters)
{
	using namespace FFF_QueueEconomyBridgeTestHelpers;
	FScopedQueueEconomyTestSettings SettingsGuard;
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("Discovery and revenue world initializes"), Harness.Initialize(this))) return false;
	Harness.Definition->BoothCommissionRate = .25f;
	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	Spec.FacilityDefinitionId = BoothDefinitionId;
	Spec.InstallCostKey = NAME_None;
	Spec.bOverrideRevenuePerUseGold = true;
	Spec.RevenuePerUseGold = 100;
	Spec.QueueRules.ServiceDurationSeconds = 1.f;
	FString Failure;
	if (!Harness.InstallFoodStall(Failure, false, &Spec)) { AddError(Failure); return false; }
	FFFZSCommand Open;
	Open.CommandId = FGuid::NewGuid();
	Open.TransactionId = FGuid::NewGuid();
	Open.Type = EFFZSCommandType::Open;
	Open.ExpectedWorldRevision = Harness.ZoneSystem->GetWorldRevision();
	Open.FacilityInstanceId = Harness.FacilityInstanceId;
	if (!TestTrue(TEXT("The paid booth opens"), Harness.ZoneSystem->ExecuteCommand(Open).bSucceeded)) return false;

	FFFZSReadModel Model = Harness.ZoneSystem->BuildReadModel();
	FFFZSZoneReadModel Entrance;
	Entrance.ZoneId = TEXT("EmptyEntrance");
	Entrance.NeighborZoneIds = {TEXT("EmptyWalkway")};
	Model.Zones.Add(Entrance);
	FFFZSZoneReadModel Walkway;
	Walkway.ZoneId = TEXT("EmptyWalkway");
	Walkway.NeighborZoneIds = {Entrance.ZoneId, TestZoneId};
	Model.Zones.Add(Walkway);
	const auto Candidates = FFF_ZoneSystemBridge::BuildCandidates(Model, Entrance.ZoneId, FVector::ZeroVector);
	if (!TestEqual(TEXT("Guests in the empty entrance can discover the operating business"), Candidates.Num(), 1)) return false;
	FGuid DiscoveredFacility;
	if (!TestTrue(TEXT("The candidate resolves to an installed facility"),
		FFF_ZoneSystemBridge::TryParseDestinationId(Candidates[0].DestinationId, DiscoveredFacility))) return false;
	TestEqual(TEXT("Discovery points at the paid booth"), DiscoveredFacility, Harness.FacilityInstanceId);

	FFF_JoinRequest Visit;
	Visit.PartyId = 600001;
	Visit.MemberCount = 4;
	if (!TestTrue(TEXT("The discovered business admits the team"),
		Harness.QueueBridge->TryJoinZoneFacility(DiscoveredFacility, Visit).bAccepted)) return false;
	Harness.Queue->SimulationStepSeconds = .05f;
	const int32 RevenueBefore = Harness.Progression->GetTodayRevenue();
	Harness.Queue->AdvanceFixedSteps(25);
	TestEqual(TEXT("Walking to the queue does not generate unearned revenue"), Harness.Progression->GetTodayRevenue(), RevenueBefore);
	Harness.QueueBridge->NotifyZoneFacilityPartyArrived(DiscoveredFacility, Visit.PartyId);
	Harness.Queue->AdvanceFixedSteps(25);
	TestEqual(TEXT("Four completed visits at 100 gold and 25 percent pay 100 gold"),
		Harness.Progression->GetTodayRevenue() - RevenueBefore, 100);
	FFF_FacilityInstanceRevenue Revenue;
	if (!TestTrue(TEXT("The payment belongs to the discovered facility"),
		Harness.Progression->GetFacilityInstanceRevenue(DiscoveredFacility, Revenue))) return false;
	TestEqual(TEXT("One team completes exactly one payment"), Revenue.TodayPaymentCount, 1);
	TestEqual(TEXT("Facility information and festival revenue agree"), Revenue.TodayGold, 100);
	Harness.Queue->AdvanceFixedSteps(25);
	TestEqual(TEXT("A finished visit never pays again"), Harness.Progression->GetTodayRevenue() - RevenueBefore, 100);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_MainProgramBreakSettlesAndResumes,
	"FestaFiesta.Economy.QueueBridge.MainProgramBreakSettlesAndResumes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_MainProgramBreakSettlesAndResumes::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_QueueEconomyBridgeTestHelpers;
	FScopedQueueEconomyTestSettings SettingsGuard;
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("Performance-break settlement world initializes"), Harness.Initialize(this))) return false;
	auto* Stage = Harness.World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>();
	if (!TestNotNull(TEXT("The real stage clock is available"), Stage)) return false;
	FObjectPropertyBase* ScheduleProperty = FindFProperty<FObjectPropertyBase>(
		UFF_FestivalStageSubsystem::StaticClass(), TEXT("ProgramScheduleTable"));
	if (!TestNotNull(TEXT("The fixture can supply its own program schedule"), ScheduleProperty)) return false;
	UDataTable* Schedule = NewObject<UDataTable>(Stage);
	Schedule->RowStruct = FFF_LiveTriggerEvent::StaticStruct();
	const auto AddProgramEvent = [Schedule](FName RowName, float AtSeconds, bool bStartsShow)
	{
		FFF_LiveTriggerEvent Event;
		Event.Day = 1;
		Event.TriggerSeconds = AtSeconds;
		Event.EventLabel = RowName.ToString();
		Event.bIsMainProgramLockStart = bStartsShow;
		Schedule->AddRow(RowName, Event);
	};
	AddProgramEvent(TEXT("FirstStart"), 180.f, true);
	AddProgramEvent(TEXT("FirstEnd"), 210.f, false);
	AddProgramEvent(TEXT("SecondStart"), 240.f, true);
	AddProgramEvent(TEXT("SecondEnd"), 270.f, false);
	ScheduleProperty->SetObjectPropertyValue_InContainer(Stage, Schedule);

	Harness.Definition->BoothCommissionRate = .25f;
	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	Spec.FacilityDefinitionId = BoothDefinitionId;
	Spec.InstallCostKey = NAME_None;
	Spec.bOverrideRevenuePerUseGold = true;
	Spec.RevenuePerUseGold = 100;
	Spec.QueueRules.ServiceCapacity = 1;
	Spec.QueueRules.ServiceDurationSeconds = 10.f;
	Spec.QueueRules.AbandonAfterSeconds = 0.f;
	FString Failure;
	if (!Harness.InstallFoodStall(Failure, false, &Spec)) { AddError(Failure); return false; }
	FFFZSCommand Open;
	Open.CommandId = FGuid::NewGuid();
	Open.TransactionId = FGuid::NewGuid();
	Open.Type = EFFZSCommandType::Open;
	Open.ExpectedWorldRevision = Harness.ZoneSystem->GetWorldRevision();
	Open.FacilityInstanceId = Harness.FacilityInstanceId;
	if (!TestTrue(TEXT("The test business opens"), Harness.ZoneSystem->ExecuteCommand(Open).bSucceeded)) return false;
	Stage->CurrentDay = 1;
	Stage->CurrentStage = EFF_FestivalStage::Live;
	Stage->ElapsedLiveSeconds = 176.9f;
	TestFalse(TEXT("Before the three-minute boundary, normal business is available"), Stage->IsMainProgramQueueBreakActive());
	Harness.Queue->SimulationStepSeconds = .05f;
	const int32 RevenueBefore = Harness.Progression->GetTodayRevenue();
	const auto Visit = [&](int32 PartyId, int32 Members)
	{
		FFF_JoinRequest Request;
		Request.PartyId = PartyId;
		Request.MemberCount = Members;
		return Harness.QueueBridge->TryJoinZoneFacility(Harness.FacilityInstanceId, Request);
	};
	if (!TestTrue(TEXT("The four-person team joins before preparation"), Visit(700001, 4).bAccepted)
		|| !TestTrue(TEXT("A two-person waiting team also joins"), Visit(700002, 2).bAccepted)) return false;
	Harness.QueueBridge->NotifyZoneFacilityPartyArrived(Harness.FacilityInstanceId, 700001);
	Harness.Queue->AdvanceFixedSteps(40);
	TestEqual(TEXT("Only the arrived first team has started service"),
		Harness.Queue->GetQueueState(Harness.QueueFacilityId).ActiveServicePartyCount, 1);
	TestEqual(TEXT("Partial service and mere admission have not charged yet"), Harness.Progression->GetTodayRevenue(), RevenueBefore);

	Stage->ElapsedLiveSeconds = 177.f;
	TestTrue(TEXT("Preparation begins exactly three festival minutes before the show"), Stage->IsMainProgramQueueBreakActive());
	Harness.QueueBridge->SynchronizeMainProgramQueueBreak();
	TestEqual(TEXT("Only the four people already using the booth pay at the boundary"),
		Harness.Progression->GetTodayRevenue() - RevenueBefore, 100);
	TestEqual(TEXT("Both using and waiting teams leave before the show"),
		Harness.Queue->GetQueueState(Harness.QueueFacilityId).OccupiedMemberCount, 0);
	TestFalse(TEXT("The queue snapshot rejects new business during preparation"),
		Harness.Queue->GetQueueState(Harness.QueueFacilityId).bAcceptingNew);
	FFF_FacilityInstanceRevenue Revenue;
	if (!TestTrue(TEXT("The boundary settlement reaches the actual facility ledger"),
		Harness.Progression->GetFacilityInstanceRevenue(Harness.FacilityInstanceId, Revenue))) return false;
	TestEqual(TEXT("One using team creates exactly one payment"), Revenue.TodayPaymentCount, 1);
	TestEqual(TEXT("The facility ledger records the same 100 gold"), Revenue.TodayGold, 100);
	const TArray<FFF_QueueLog>& FirstLogs = Harness.Queue->GetQueueLogsRef();
	if (!TestEqual(TEXT("Both first-show visits produce real queue exit logs"), FirstLogs.Num(), 2)) return false;
	TestEqual(TEXT("Partial service is settled as served"), FirstLogs[0].ExitType, EFF_QueueExit::Served);
	TestTrue(TEXT("Settlement happened before the full service duration elapsed"),
		FirstLogs[0].ServiceSeconds > 0.f && FirstLogs[0].ServiceSeconds < Spec.QueueRules.ServiceDurationSeconds);
	TestEqual(TEXT("The waiting team is cancelled without service"), FirstLogs[1].ExitType, EFF_QueueExit::Cancelled);
	TestEqual(TEXT("The waiting team consumed no service time"), FirstLogs[1].ServiceSeconds, 0.f);
	Harness.QueueBridge->SynchronizeMainProgramQueueBreak();
	Harness.Queue->FlushPendingEvents();
	Harness.Queue->AdvanceFixedSteps(240);
	TestEqual(TEXT("Repeated synchronization, dispatch, and fixed steps never double-charge"),
		Harness.Progression->GetTodayRevenue() - RevenueBefore, 100);
	TestEqual(TEXT("New preparation-period admissions are rejected"), Visit(700003, 1).Result, EFF_QueueJoinResult::RejectedClosed);
	Stage->ElapsedLiveSeconds = 209.9f;
	TestEqual(TEXT("New admissions stay blocked throughout the show"), Visit(700004, 1).Result, EFF_QueueJoinResult::RejectedClosed);

	Stage->ElapsedLiveSeconds = 210.f;
	Harness.QueueBridge->SynchronizeMainProgramQueueBreak();
	TestFalse(TEXT("The first performance break ends at the scheduled end"), Stage->IsMainProgramQueueBreakActive());
	if (!TestTrue(TEXT("Normal business resumes immediately after the show"), Visit(700005, 1).bAccepted)) return false;
	Harness.QueueBridge->NotifyZoneFacilityPartyArrived(Harness.FacilityInstanceId, 700005);
	Harness.Queue->AdvanceFixedSteps(201);
	TestEqual(TEXT("An ordinary post-show visit still completes and pays normally"),
		Harness.Progression->GetTodayRevenue() - RevenueBefore, 125);

	Stage->ElapsedLiveSeconds = 236.9f;
	if (!TestTrue(TEXT("A three-person team joins before the second show"), Visit(700006, 3).bAccepted)
		|| !TestTrue(TEXT("Another team waits for the second show boundary"), Visit(700007, 2).bAccepted)) return false;
	Harness.QueueBridge->NotifyZoneFacilityPartyArrived(Harness.FacilityInstanceId, 700006);
	Harness.Queue->AdvanceFixedSteps(40);
	Stage->ElapsedLiveSeconds = 237.f;
	// No bridge Tick or explicit synchronization has run at this boundary. The admission
	// facade must observe the new clock window before it can accept another team.
	TestEqual(TEXT("The first boundary-time join synchronizes and rejects before the bridge ticks"),
		Visit(700008, 1).Result, EFF_QueueJoinResult::RejectedClosed);
	TestEqual(TEXT("The second show settles its existing three-person service exactly once"),
		Harness.Progression->GetTodayRevenue() - RevenueBefore, 200);
	TestEqual(TEXT("The second boundary also drains all old teams"),
		Harness.Queue->GetQueueState(Harness.QueueFacilityId).OccupiedMemberCount, 0);
	Harness.QueueBridge->SynchronizeMainProgramQueueBreak();
	Harness.Queue->FlushPendingEvents();
	Stage->ElapsedLiveSeconds = 270.f;
	Harness.QueueBridge->SynchronizeMainProgramQueueBreak();
	TestTrue(TEXT("The second show releases its temporary admission block"),
		Harness.Queue->GetQueueState(Harness.QueueFacilityId).bAcceptingNew);
	TestTrue(TEXT("The final facility revenue remains queryable"),
		Harness.Progression->GetFacilityInstanceRevenue(Harness.FacilityInstanceId, Revenue));
	TestEqual(TEXT("Two boundary completions and one ordinary visit make three payments"), Revenue.TodayPaymentCount, 3);
	TestEqual(TEXT("Waiting teams and repeated boundaries add no extra gold"), Revenue.TodayGold, 200);
	TestEqual(TEXT("All five accepted visits were accounted for exactly once"), Harness.Queue->GetQueueLogsRef().Num(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_ProductionTableCompletionUpdatesTodayBadge,
	"FestaFiesta.Economy.QueueBridge.ProductionTableCompletionUpdatesTodayBadge",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_ProductionTableCompletionUpdatesTodayBadge::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_QueueEconomyBridgeTestHelpers;
	FScopedQueueEconomyTestSettings SettingsGuard;
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("Production-price settlement world initializes"), Harness.Initialize(this))) return false;
	TStrongObjectPtr<UDataTable> RevenueTable(LoadObject<UDataTable>(nullptr,
		TEXT("/Game/FestaFiesta/Data/Economy/DT_FacilityRevenues.DT_FacilityRevenues")));
	if (!TestNotNull(TEXT("The actual facility revenue table loads"), RevenueTable.Get())) return false;
	const FName DefinitionId(TEXT("Booth_ActivityGame_BalloonDartStall"));
	const FFF_FacilityRevenueTableRow* Row = RevenueTable->FindRow<FFF_FacilityRevenueTableRow>(
		DefinitionId, TEXT("ProductionTableCompletionUpdatesTodayBadge"));
	if (!TestNotNull(TEXT("The balloon dart booth has an authored revenue row"), Row)
		|| !TestTrue(TEXT("The production booth has a positive customer fee"), Row->RevenuePerUseGold > 0)) return false;
	Harness.Definition->FacilityRevenueTable = RevenueTable.Get();
	UFF_FacilityUpgradeSubsystem* Upgrades = Harness.World->GetSubsystem<UFF_FacilityUpgradeSubsystem>();
	UFF_FacilityProgramMarkerSubsystem* Markers = Harness.World->GetSubsystem<UFF_FacilityProgramMarkerSubsystem>();
	if (!TestNotNull(TEXT("The facility quote owner exists"), Upgrades)
		|| !TestNotNull(TEXT("The today-revenue badge owner exists"), Markers)) return false;

	// The isolated layout avoids map/navigation dependencies while using the real production price.
	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	Spec.FacilityDefinitionId = DefinitionId;
	Spec.InstallCostKey = TEXT("Cost.Booth.FoodStall");
	Spec.bOverrideRevenuePerUseGold = true;
	Spec.RevenuePerUseGold = 999; // A stale saved facility fee must not override the table.
	Spec.QueueRules.ServiceDurationSeconds = 1.f;
	Spec.QueueRules.AbandonAfterSeconds = 0.f;
	FString Failure;
	if (!Harness.InstallFoodStall(Failure, false, &Spec)) { AddError(Failure); return false; }
	FFFZSCommand Open;
	Open.CommandId = FGuid::NewGuid();
	Open.TransactionId = FGuid::NewGuid();
	Open.Type = EFFZSCommandType::Open;
	Open.ExpectedWorldRevision = Harness.ZoneSystem->GetWorldRevision();
	Open.FacilityInstanceId = Harness.FacilityInstanceId;
	if (!TestTrue(TEXT("The table-priced booth opens"), Harness.ZoneSystem->ExecuteCommand(Open).bSucceeded)) return false;
	Harness.Queue->SimulationStepSeconds = .05f;
	FFF_FacilityUpgradePreview Quote;
	if (!TestTrue(TEXT("Facility information resolves its team quote"),
		Upgrades->GetUpgradePreview(Harness.FacilityInstanceId, Quote))) return false;
	if (!TestEqual(TEXT("The quote includes all supported team sizes"), Quote.CurrentRevenueByTeamSizeGold.Num(), 6)) return false;
	Markers->Tick(0.f);
	FFF_FacilityWorldBadgeData Badge;
	if (!TestTrue(TEXT("The booth shows zero today revenue before anyone completes a use"),
		Markers->GetBadgeData(Harness.FacilityInstanceId, Badge) && Badge.bShowRevenue && Badge.TodayGold == 0)) return false;

	const float Rate = Row->bOverrideCommissionRate ? Row->CommissionRate : Harness.Definition->BoothCommissionRate;
	const float* AuthoredLevelMultiplier = Row->RevenueMultiplierByLevel.Find(1);
	const float LevelMultiplier = AuthoredLevelMultiplier ? *AuthoredLevelMultiplier : 1.f;
	const int32 GoldBeforeVisits = Harness.Progression->GetGold();
	const int32 InitialTodayRevenue = Harness.Progression->GetTodayRevenue();
	int32 Earned = 0;
	int32 Payments = 0;
	for (const int32 Members : {1, 4, 6})
	{
		const int32 Expected = FMath::RoundToInt32(
			static_cast<double>(Row->RevenuePerUseGold) * Members * Rate * LevelMultiplier);
		if (!TestTrue(TEXT("The production example produces actual income"), Expected > 0)) return false;
		TestEqual(TEXT("The visible team quote uses the production table and full team rounding"),
			Quote.CurrentRevenueByTeamSizeGold[Members - 1], Expected);
		FFF_JoinRequest Visit;
		Visit.PartyId = 810000 + Members;
		Visit.MemberCount = Members;
		if (!TestTrue(TEXT("The team enters the real facility queue"),
			Harness.QueueBridge->TryJoinZoneFacility(Harness.FacilityInstanceId, Visit).bAccepted)) return false;
		Harness.Queue->AdvanceFixedSteps(25);
		TestEqual(TEXT("Joining and walking to the booth do not add revenue"),
			Harness.Progression->GetTodayRevenue(), InitialTodayRevenue + Earned);
		Harness.QueueBridge->NotifyZoneFacilityPartyArrived(Harness.FacilityInstanceId, Visit.PartyId);
		Harness.Queue->AdvanceFixedSteps(10);
		TestEqual(TEXT("A team still using the booth has not paid yet"),
			Harness.Progression->GetTodayRevenue(), InitialTodayRevenue + Earned);
		Harness.Queue->AdvanceFixedSteps(11);
		Earned += Expected;
		++Payments;
		TestEqual(TEXT("Completed use adds the quoted amount to spendable Gold"),
			Harness.Progression->GetGold(), GoldBeforeVisits + Earned);
		TestEqual(TEXT("Completed use adds the same amount to festival today revenue"),
			Harness.Progression->GetTodayRevenue(), InitialTodayRevenue + Earned);
		FFF_FacilityInstanceRevenue Ledger;
		if (!TestTrue(TEXT("The completed use records the installed facility's ledger"),
			Harness.Progression->GetFacilityInstanceRevenue(Harness.FacilityInstanceId, Ledger))) return false;
		TestEqual(TEXT("Facility information reads the same today amount"), Ledger.TodayGold, Earned);
		TestEqual(TEXT("Each completed team creates exactly one payment"), Ledger.TodayPaymentCount, Payments);
		Markers->Tick(0.f);
		if (!TestTrue(TEXT("The completed facility still has a visible revenue badge"),
			Markers->GetBadgeData(Harness.FacilityInstanceId, Badge) && Badge.bShowRevenue)) return false;
		TestEqual(TEXT("The world today-revenue badge immediately matches the real ledger"), Badge.TodayGold, Earned);
		Harness.Queue->FlushPendingEvents();
		Harness.Queue->AdvanceFixedSteps(25);
		TestEqual(TEXT("Further dispatch and ticks do not settle the same use twice"),
			Harness.Progression->GetTodayRevenue(), InitialTodayRevenue + Earned);
	}
	return true;
}


// 화장실은 메인 공연 줄 휴지에도 손님을 받는다. 공연을 보는 동안에도 화장실 욕구는 차기 때문이다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_QueueEconomyBridge_ToiletStaysOpenDuringMainProgram,
	"FestaFiesta.Economy.QueueBridge.ToiletStaysOpenDuringMainProgram",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_QueueEconomyBridge_ToiletStaysOpenDuringMainProgram::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace FFF_QueueEconomyBridgeTestHelpers;
	FScopedQueueEconomyTestSettings SettingsGuard;
	FQueueEconomyHarness Harness;
	if (!TestTrue(TEXT("화장실 휴지 테스트 월드"), Harness.Initialize(this))) return false;
	auto* Stage = Harness.World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>();
	FObjectPropertyBase* ScheduleProperty = FindFProperty<FObjectPropertyBase>(
		UFF_FestivalStageSubsystem::StaticClass(), TEXT("ProgramScheduleTable"));
	if (!TestNotNull(TEXT("단계 시계"), Stage) || !TestNotNull(TEXT("공연 일정 표 속성"), ScheduleProperty)) return false;
	UDataTable* Schedule = NewObject<UDataTable>(Stage);
	Schedule->RowStruct = FFF_LiveTriggerEvent::StaticStruct();
	const auto AddProgramEvent = [Schedule](FName RowName, float AtSeconds, bool bStartsShow)
	{
		FFF_LiveTriggerEvent Event;
		Event.Day = 1;
		Event.TriggerSeconds = AtSeconds;
		Event.EventLabel = RowName.ToString();
		Event.bIsMainProgramLockStart = bStartsShow;
		Schedule->AddRow(RowName, Event);
	};
	AddProgramEvent(TEXT("FirstStart"), 180.f, true);
	AddProgramEvent(TEXT("FirstEnd"), 210.f, false);
	ScheduleProperty->SetObjectPropertyValue_InContainer(Stage, Schedule);

	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	Spec.Category = EFFZSFacilityCategory::Toilet;
	Spec.FacilityDefinitionId = TEXT("Toilet_Standard_Standard");
	// 테스트 설치비 표에는 음식 부스 행만 있다. 화장실도 그 행으로 값을 매긴다
	Spec.InstallCostKey = TEXT("Cost.Booth.FoodStall");
	Spec.QueueRules.ServiceCapacity = 1;
	Spec.QueueRules.ServiceDurationSeconds = 10.f;
	Spec.QueueRules.AbandonAfterSeconds = 0.f;
	FString Failure;
	if (!Harness.InstallFoodStall(Failure, false, &Spec)) { AddError(Failure); return false; }
	FFFZSCommand Open;
	Open.CommandId = FGuid::NewGuid();
	Open.TransactionId = FGuid::NewGuid();
	Open.Type = EFFZSCommandType::Open;
	Open.ExpectedWorldRevision = Harness.ZoneSystem->GetWorldRevision();
	Open.FacilityInstanceId = Harness.FacilityInstanceId;
	if (!TestTrue(TEXT("화장실이 연다"), Harness.ZoneSystem->ExecuteCommand(Open).bSucceeded)) return false;
	Stage->CurrentDay = 1;
	Stage->CurrentStage = EFF_FestivalStage::Live;

	const auto Visit = [&](int32 PartyId)
	{
		FFF_JoinRequest Request;
		Request.PartyId = PartyId;
		Request.MemberCount = 2;
		return Harness.QueueBridge->TryJoinZoneFacility(Harness.FacilityInstanceId, Request);
	};
	Stage->ElapsedLiveSeconds = 176.f;
	if (!TestTrue(TEXT("공연 전 줄"), Visit(710001).bAccepted)) return false;
	Harness.QueueBridge->NotifyZoneFacilityPartyArrived(Harness.FacilityInstanceId, 710001);

	Stage->ElapsedLiveSeconds = 190.f;
	TestTrue(TEXT("공연 줄 휴지 중이다"), Stage->IsMainProgramQueueBreakActive());
	Harness.QueueBridge->SynchronizeMainProgramQueueBreak();
	TestFalse(TEXT("화장실은 공연 정지 대상이 아니다"), Harness.QueueBridge->PausesForMainProgram(Harness.QueueFacilityId));
	TestEqual(TEXT("공연이 시작돼도 쓰던 팀을 내보내지 않는다"),
		Harness.Queue->GetQueueState(Harness.QueueFacilityId).OccupiedMemberCount, 2);
	TestTrue(TEXT("공연 중에도 새 손님을 받는다"), Harness.Queue->GetQueueState(Harness.QueueFacilityId).bAcceptingNew);
	TestTrue(TEXT("공연 중 줄 가입"), Visit(710002).bAccepted);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
