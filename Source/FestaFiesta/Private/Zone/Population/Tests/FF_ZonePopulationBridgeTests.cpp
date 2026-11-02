// FF_ZonePopulationBridgeTests.cpp

// 존 인구 집계와 보고 계약을 자동화 테스트로 고정한다.
// 흐름: 실제 Zone Actor 2개를 등록한 Game World에서 순수 집계 함수와 플러그인 보고 API를 직접 호출한다.
// 불변식: 인원 0인 존도 결과에 남고, 비활성 슬롯은 캐시까지 비우며, 직전 존은 경계에서 유지된다.
// 실패/성능: 변경한 개발자 설정은 범위 종료 시 반드시 복원하고 Crowd 시뮬레이션에는 의존하지 않는다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Zone/FF_ZonePopulationBridgeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

namespace
{
	// 테스트 전용 ZoneId를 쓰기 위해 프로젝트 프리셋 제한만 잠시 끈다.
	// 사전순으로 A < B 이므로 "힌트가 없으면 A" 라는 결정적 기준점이 생긴다.
	const FName PopTestZoneAId(TEXT("GZ_PopTestA"));
	const FName PopTestZoneBId(TEXT("GZ_PopTestB"));

	class FScopedUnrestrictedZoneIds
	{
	public:
		FScopedUnrestrictedZoneIds()
			: Settings(GetMutableDefault<UFFZSDeveloperSettings>())
			, bRestrictZoneIds(Settings->bRestrictZoneIdsToProjectPresets)
		{
			Settings->bRestrictZoneIdsToProjectPresets = false;
		}

		~FScopedUnrestrictedZoneIds()
		{
			Settings->bRestrictZoneIdsToProjectPresets = bRestrictZoneIds;
		}

	private:
		UFFZSDeveloperSettings* Settings;
		bool bRestrictZoneIds;
	};

	// 기본 경계 스플라인은 로컬 (0,0)~(10000,10000)cm다. 즉 Actor 위치는 100m 존의 한가운데가
	// 아니라 모서리이므로, 두 존을 X로 100m 띄우면 x=10000 변을 정확히 공유한다.
	AFFZSGameplayZone* SpawnRegisteredZone(UWorld& World, UFFZSWorldSubsystem& ZoneSystem, const FName ZoneId, const FVector& Location)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AFFZSGameplayZone* Zone = World.SpawnActor<AFFZSGameplayZone>(
			AFFZSGameplayZone::StaticClass(), FTransform(Location), SpawnParameters);
		if (!Zone || !Zone->ApplyAcceptedZonePreset(ZoneId) || !ZoneSystem.RegisterZone(Zone))
		{
			return nullptr;
		}
		return Zone;
	}

	FFF_ZoneAgentSlot MakeActiveSlot(const FVector& Location)
	{
		FFF_ZoneAgentSlot Slot;
		Slot.Location = Location;
		Slot.bActive = true;
		return Slot;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZonePopulationTallyTest,
	"FestaFiesta.ZoneIntegration.Population.Tally",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZonePopulationTallyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedUnrestrictedZoneIds SettingsGuard;
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("ZoneSystem이 준비된다"), ZoneSystem))
	{
		return false;
	}

	const AFFZSGameplayZone* ZoneA = SpawnRegisteredZone(*World, *ZoneSystem, PopTestZoneAId, FVector::ZeroVector);
	const AFFZSGameplayZone* ZoneB = SpawnRegisteredZone(*World, *ZoneSystem, PopTestZoneBId, FVector(10000.0, 0.0, 0.0));
	if (!TestNotNull(TEXT("Zone A가 등록된다"), ZoneA) || !TestNotNull(TEXT("Zone B가 등록된다"), ZoneB))
	{
		return false;
	}

	const TArray<FName> ZoneIds = ZoneSystem->GetRegisteredZoneIds();
	if (!TestEqual(TEXT("등록 존은 2개다"), ZoneIds.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("등록 존 ID는 사전순이다"), ZoneIds[0], PopTestZoneAId);

	// Zone A 안 1명, 어느 존에도 없는 1명, 비활성 슬롯 1개.
	TArray<FFF_ZoneAgentSlot> Slots;
	Slots.Add(MakeActiveSlot(FVector(5000.0, 5000.0, 0.0)));
	Slots.Add(MakeActiveSlot(FVector(-50000.0, -50000.0, 0.0)));
	Slots.AddDefaulted();

	// 비활성 슬롯에 남아 있는 직전 존이 지워지는지 보려고 미리 값을 넣어 둔다.
	TArray<FName> LastZoneBySlot;
	LastZoneBySlot.SetNum(3);
	LastZoneBySlot[2] = PopTestZoneBId;

	TMap<FName, int32> PopulationByZoneId;
	int32 OutsideZoneCount = 0;
	UFF_ZonePopulationBridgeSubsystem::TallyByZone(
		*ZoneSystem, ZoneIds, Slots, LastZoneBySlot, PopulationByZoneId, OutsideZoneCount);

	// 인원 0인 존이 결과에서 빠지면 보고 루프가 그 존을 건너뛰고, 감쇠가 없는 플러그인 쪽에는
	// 마지막 값이 영원히 남는다. 그래서 "B가 0으로 존재한다"가 이 테스트의 핵심이다.
	TestEqual(TEXT("결과에는 등록 존이 전부 들어 있다"), PopulationByZoneId.Num(), 2);
	TestEqual(TEXT("Zone A 인원은 1명이다"), PopulationByZoneId.FindRef(PopTestZoneAId), 1);
	TestEqual(TEXT("사람이 없는 Zone B도 0으로 보고 대상에 남는다"), PopulationByZoneId.FindRef(PopTestZoneBId), 0);
	TestEqual(TEXT("존 밖 인원은 1명이다"), OutsideZoneCount, 1);
	TestEqual(TEXT("비활성 슬롯의 직전 존 캐시는 비워진다"), LastZoneBySlot[2], FName(NAME_None));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZonePopulationBoundaryHysteresisTest,
	"FestaFiesta.ZoneIntegration.Population.BoundaryHysteresis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZonePopulationBoundaryHysteresisTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedUnrestrictedZoneIds SettingsGuard;
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("ZoneSystem이 준비된다"), ZoneSystem))
	{
		return false;
	}

	if (!SpawnRegisteredZone(*World, *ZoneSystem, PopTestZoneAId, FVector::ZeroVector)
		|| !SpawnRegisteredZone(*World, *ZoneSystem, PopTestZoneBId, FVector(10000.0, 0.0, 0.0)))
	{
		return false;
	}

	const TArray<FName> ZoneIds = ZoneSystem->GetRegisteredZoneIds();

	// 두 존이 공유하는 변 위의 점. 경계 판정만으로는 양쪽 모두 "안"이라 사전순으로 A가 이긴다.
	TArray<FFF_ZoneAgentSlot> Slots;
	Slots.Add(MakeActiveSlot(FVector(10000.0, 5000.0, 0.0)));

	TMap<FName, int32> PopulationByZoneId;
	int32 OutsideZoneCount = 0;

	TArray<FName> NoHint;
	UFF_ZonePopulationBridgeSubsystem::TallyByZone(
		*ZoneSystem, ZoneIds, Slots, NoHint, PopulationByZoneId, OutsideZoneCount);
	TestEqual(TEXT("직전 존이 없으면 사전순 첫 존으로 센다"), PopulationByZoneId.FindRef(PopTestZoneAId), 1);

	// 캐시를 실제로 쓰는지 보는 지점이다. 쓰지 않으면 매 사이클 등록 존 전체를 다시 판정하게 되고,
	// 경계에 선 사람이 프레임마다 두 존을 오가며 두 존의 인구가 같이 출렁인다.
	TArray<FName> HintZoneB;
	HintZoneB.Add(PopTestZoneBId);
	UFF_ZonePopulationBridgeSubsystem::TallyByZone(
		*ZoneSystem, ZoneIds, Slots, HintZoneB, PopulationByZoneId, OutsideZoneCount);
	TestEqual(TEXT("직전 존이 있으면 경계에서 그 존을 유지한다"), PopulationByZoneId.FindRef(PopTestZoneBId), 1);
	TestEqual(TEXT("직전 존을 유지했으므로 반대쪽 존은 0이다"), PopulationByZoneId.FindRef(PopTestZoneAId), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZonePopulationReportContractTest,
	"FestaFiesta.ZoneIntegration.Population.ReportContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZonePopulationReportContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedUnrestrictedZoneIds SettingsGuard;
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("ZoneSystem이 준비된다"), ZoneSystem))
	{
		return false;
	}

	const AFFZSGameplayZone* ZoneA = SpawnRegisteredZone(*World, *ZoneSystem, PopTestZoneAId, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Zone A가 등록된다"), ZoneA))
	{
		return false;
	}

	// 반환값이 "표본이 반영됐는가"여야 호출자가 경과 시간을 이월할지 판단할 수 있다.
	TestTrue(TEXT("등록된 존 보고는 성공한다"),
		ZoneSystem->ReportZonePopulation(PopTestZoneAId, 7, 1.0f));
	TestFalse(TEXT("등록되지 않은 존 보고는 실패한다"),
		ZoneSystem->ReportZonePopulation(FName(TEXT("GZ_PopTestMissing")), 5, 1.0f));
	TestEqual(TEXT("보고한 인원이 읽기 모델에 반영된다"), ZoneA->GetReadModel().ReportedPopulation, 7);

	// 감쇠가 없으므로 0을 명시적으로 보내야 값이 내려간다. 이 계약이 깨지면 사람이 다 빠져나간
	// 구역이 계속 혼잡한 것으로 남아 Utility AI가 영원히 그곳을 피한다.
	TestTrue(TEXT("0명 보고도 성공한다"),
		ZoneSystem->ReportZonePopulation(PopTestZoneAId, 0, 1.0f));
	TestEqual(TEXT("0을 보고하면 인원이 실제로 0이 된다"), ZoneA->GetReadModel().ReportedPopulation, 0);

	// 음수는 호출자의 계산 실수다. 플러그인이 0으로 보정하는 계약을 여기서 고정한다.
	TestTrue(TEXT("음수 보고도 거부되지 않는다"),
		ZoneSystem->ReportZonePopulation(PopTestZoneAId, -3, 1.0f));
	TestEqual(TEXT("음수 인원은 0으로 보정된다"), ZoneA->GetReadModel().ReportedPopulation, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZonePopulationReportCycleTest,
	"FestaFiesta.ZoneIntegration.Population.ReportCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZonePopulationReportCycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FScopedUnrestrictedZoneIds SettingsGuard;
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	UFFZSWorldSubsystem* ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	UFF_ZonePopulationBridgeSubsystem* Bridge =
		World ? World->GetSubsystem<UFF_ZonePopulationBridgeSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("ZoneSystem이 준비된다"), ZoneSystem)
		|| !TestNotNull(TEXT("인구 브리지 subsystem이 생성된다"), Bridge))
	{
		return false;
	}

	const AFFZSGameplayZone* ZoneA = SpawnRegisteredZone(*World, *ZoneSystem, PopTestZoneAId, FVector::ZeroVector);
	if (!TestNotNull(TEXT("Zone A가 등록된다"), ZoneA))
	{
		return false;
	}

	// 사람이 있었던 상태에서 시작한다. 사이클이 0을 덮어써야 "다 빠져나갔다"가 반영된다.
	ZoneSystem->ReportZonePopulation(PopTestZoneAId, 11, 1.0f);
	TestEqual(TEXT("사전 조건: 인원 11명"), ZoneA->GetReadModel().ReportedPopulation, 11);

	// 에이전트가 없는 월드에서도 사이클은 성립해야 한다. 여기서 false가 나오면 실제 세션에서도
	// 보고가 통째로 누락된다 — Tick 경로 전체가 이 함수 하나를 부른다.
	TestTrue(TEXT("에이전트가 없어도 보고 사이클은 성립한다"), Bridge->ReportNow());

	const FFF_ZonePopulationCycle Cycle = Bridge->GetLastCycle();
	TestEqual(TEXT("사이클 번호가 올라간다"), Cycle.CycleIndex, 1);
	TestEqual(TEXT("등록 존 1개에 보고된다"), Cycle.ReportedZoneCount, 1);
	TestEqual(TEXT("거부된 존은 없다"), Cycle.RejectedZoneCount, 0);
	TestEqual(TEXT("빈 월드의 인원은 0명이다"), Cycle.SampledAgentCount, 0);
	TestEqual(TEXT("사이클 결과에 존이 남아 있다"), Cycle.PopulationByZoneId.FindRef(PopTestZoneAId), 0);
	TestEqual(TEXT("직전 인원이 0으로 덮어써진다"), ZoneA->GetReadModel().ReportedPopulation, 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
