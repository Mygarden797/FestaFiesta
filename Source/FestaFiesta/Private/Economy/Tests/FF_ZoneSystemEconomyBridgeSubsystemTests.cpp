// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_ZoneSystemEconomyBridgeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSExternalPolicy.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_FestivalSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace
{
	// 격리된 설치·환급 테스트의 가격이다. 프로덕션 밸런스의 두 번째 원천으로 쓰지 않는다.
	// 아래 ContentContract는 실제 DT 행을 읽어 비용 해석 결과와 직접 비교한다.
	constexpr int32 FoodStallInstallCost = 700;

	// 설치 → 철거 → 재설치까지 되는 잔액. 설치비가 오르면 같이 오른다.
	constexpr int32 HarnessStartingGold = FoodStallInstallCost * 3;
	const FName TestZoneId(TEXT("GZ_DadaPocha"));
	const FName AutomationFoodStallCostKey(
		TEXT("Cost.Booth.FoodStall.EconomyBridgeAutomation"));
	const FName ProductionFoodTruckCostKey(
		TEXT("Cost.Booth.FoodStall.Taco"));

	/** Restores every mutable Zone System default changed by these integration tests. */
	class FScopedEconomyBridgeTestSettings
	{
	public:
		FScopedEconomyBridgeTestSettings()
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

		~FScopedEconomyBridgeTestSettings()
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

	/**
	 * Owns an isolated Game world and its GameInstance-scoped economy data.
	 * No project asset or process-global Festival setting is modified.
	 */
	class FZoneEconomyTestHarness
	{
	public:
		bool Initialize(FAutomationTestBase* Test, const int32 InitialGold)
		{
			if (!TestWorld.CreateTestWorld(EWorldType::Game))
			{
				TestWorld.ForwardErrorMessages(Test);
				return false;
			}

			World = TestWorld.GetTestWorld();
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			ZoneSystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
			EconomyBridge = World
				? World->GetSubsystem<UFF_ZoneSystemEconomyBridgeSubsystem>()
				: nullptr;
			Progression = GameInstance
				? GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>()
				: nullptr;
			if (!World || !GameInstance || !ZoneSystem || !EconomyBridge || !Progression)
			{
				return false;
			}

			CostTable = NewObject<UDataTable>(GameInstance);
			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			if (!CostTable || !Definition)
			{
				return false;
			}

			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			FFF_FacilityCostRow FoodStallRow;
			FoodStallRow.InstallCost = FoodStallInstallCost;
			CostTable->AddRow(AutomationFoodStallCostKey, FoodStallRow);

			Definition->FestivalId = TEXT("EconomyBridgeAutomationFestival");
			Definition->InitialGold = InitialGold;
			Definition->FacilityCostTable = CostTable;

			// 섞여 들어온다. 하루 경계 정산 자체는 FF_DayBoundaryTests.cpp 가 검증한다.

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

		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		UFFZSWorldSubsystem* ZoneSystem = nullptr;
		UFF_ZoneSystemEconomyBridgeSubsystem* EconomyBridge = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
		UDataTable* CostTable = nullptr;
		AFFZSGameplayZone* Zone = nullptr;
	};

	FFFZSFacilitySpec MakeDerivedCostFoodStallSpec()
	{
		FFFZSFacilitySpec Spec =
			FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
		// A None override exercises the production Major.Minor.Unique naming convention.
		Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_EconomyBridgeAutomation");
		Spec.InstallCostKey = NAME_None;
		return Spec;
	}

	FFFZSCommand MakeInstallCommand(
		const FZoneEconomyTestHarness& Harness,
		const FFFZSFacilitySpec& Spec,
		const FIntPoint OriginCell = FIntPoint(1000, 1000))
	{
		FFFZSGridPlacement Placement;
		Placement.OriginCell = OriginCell;
		return UFFZSGridLibrary::MakeInstallCommand(
			Spec, TestZoneId, Placement, Harness.ZoneSystem->GetWorldRevision());
	}

	FFFZSCommand MakeMoveCommand(
		const FZoneEconomyTestHarness& Harness,
		const FFFZSCommand& InstallCommand,
		const FIntPoint OriginCell)
	{
		FFFZSCommand Command;
		Command.CommandId = FGuid::NewGuid();
		Command.TransactionId = FGuid::NewGuid();
		Command.ExpectedWorldRevision = Harness.ZoneSystem->GetWorldRevision();
		Command.Type = EFFZSCommandType::Move;
		Command.FacilityInstanceId = InstallCommand.FacilityInstanceId;
		Command.FacilitySpec = InstallCommand.FacilitySpec;
		Command.ZoneId = TestZoneId;
		Command.Placement.OriginCell = OriginCell;
		Command.Placement.QuarterTurns = 1;
		return Command;
	}

	FFFZSCommand MakeLifecycleCommand(
		const FZoneEconomyTestHarness& Harness,
		const FFFZSCommand& InstallCommand,
		const EFFZSCommandType Type)
	{
		FFFZSCommand Command;
		Command.CommandId = FGuid::NewGuid();
		Command.TransactionId = FGuid::NewGuid();
		Command.ExpectedWorldRevision = Harness.ZoneSystem->GetWorldRevision();
		Command.Type = Type;
		Command.FacilityInstanceId = InstallCommand.FacilityInstanceId;
		Command.FacilitySpec = InstallCommand.FacilitySpec;
		Command.ZoneId = TestZoneId;
		Command.Placement = InstallCommand.Placement;
		return Command;
	}

	FFFZSCommand MakeRemoveCommand(
		const FZoneEconomyTestHarness& Harness,
		const FGuid& FacilityInstanceId)
	{
		FFFZSCommand Command;
		Command.CommandId = FGuid::NewGuid();
		Command.TransactionId = FGuid::NewGuid();
		Command.ExpectedWorldRevision = Harness.ZoneSystem->GetWorldRevision();
		Command.Type = EFFZSCommandType::Remove;
		// 철거 명령은 인스턴스 ID만 싣는다. 비용 키는 브리지가 권위 액터의 Spec에서 다시 읽어야 한다.
		Command.FacilityInstanceId = FacilityInstanceId;
		return Command;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemEconomyContentContractTest,
	"FestaFiesta.Economy.ZoneSystemBridge.ContentContract.DefaultFestivalCostsFoodTruck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemEconomyContentContractTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UFF_ZoneSystemEconomyBridgeSubsystem* BridgeDefaults =
		GetDefault<UFF_ZoneSystemEconomyBridgeSubsystem>();
	if (!TestNotNull(TEXT("Economy bridge class defaults are available"), BridgeDefaults))
	{
		return false;
	}
	TestTrue(TEXT("Packaged Game worlds create the economy bridge"),
		BridgeDefaults->DoesSupportWorldType(EWorldType::Game));
	TestTrue(TEXT("PIE worlds create the economy bridge"),
		BridgeDefaults->DoesSupportWorldType(EWorldType::PIE));
	TestTrue(TEXT("Standalone Game Preview worlds keep construction charging enabled"),
		BridgeDefaults->DoesSupportWorldType(EWorldType::GamePreview));
	TestFalse(TEXT("Editor asset-preview worlds do not create the economy bridge"),
		BridgeDefaults->DoesSupportWorldType(EWorldType::EditorPreview));

	const UFF_FestivalSettings* FestivalSettings = UFF_FestivalSettings::Get();
	if (!TestNotNull(TEXT("Project settings expose a default Festival"), FestivalSettings))
	{
		return false;
	}
	const UFF_FestivalDefinition* FestivalDefinition =
		FestivalSettings->DefaultFestival.LoadSynchronous();
	if (!TestNotNull(TEXT("The configured default Festival asset loads"),
		FestivalDefinition))
	{
		return false;
	}

	const UDataTable* CostTable = FestivalDefinition->FacilityCostTable.Get();
	if (!TestNotNull(TEXT("The default Festival references a facility cost table"),
		CostTable))
	{
		return false;
	}
	TestEqual(TEXT("The live Festival uses the canonical DT_FacilityCosts asset"),
		CostTable->GetPathName(),
		FString(TEXT("/Game/FestaFiesta/Data/Economy/DT_FacilityCosts.DT_FacilityCosts")));

	UAssetManager& AssetManager = UAssetManager::Get();
	TArray<FPrimaryAssetId> FacilityAssetIds;
	AssetManager.GetPrimaryAssetIdList(
		UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(),
		FacilityAssetIds);
	FacilityAssetIds.Sort([](const FPrimaryAssetId& Left, const FPrimaryAssetId& Right)
	{
		return Left.ToString() < Right.ToString();
	});
	TestTrue(TEXT("The Asset Manager exposes at least one production FacilityDefinition"),
		!FacilityAssetIds.IsEmpty());

	int32 AutomaticDefinitionCount = 0;
	const FString CostLookupContext = TEXT("FFFZoneSystemEconomyContentContractTest");
	for (const FPrimaryAssetId& AssetId : FacilityAssetIds)
	{
		const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(AssetId);
		const UFFZSFacilityDefinition* Definition =
			Cast<UFFZSFacilityDefinition>(AssetPath.TryLoad());
		if (!TestNotNull(
			*FString::Printf(TEXT("FacilityDefinition loads for cost coverage: %s"),
				*AssetId.ToString()),
			Definition))
		{
			continue;
		}
		if (!Definition->Spec.InstallCostKey.IsNone())
		{
			// 레거시 ID를 보존하는 호환 에셋도 예외 가격 키가 실제 행을 가리켜야 한다.
			// 명시 키를 가진 정의를 검사에서 제외하면 누락된 가격이 숨어 버린다.
			const FName ExplicitKey = Definition->Spec.InstallCostKey;
			const FFF_FacilityCostRow* ExplicitRow = CostTable->FindRow<FFF_FacilityCostRow>(
				ExplicitKey, CostLookupContext, false);
			if (!TestNotNull(
				*FString::Printf(TEXT("Explicit cost override has an exact production row: %s -> %s"),
					*AssetId.ToString(), *ExplicitKey.ToString()), ExplicitRow))
			{
				continue;
			}
			int32 ResolvedCost = 0;
			FName MatchedRow = NAME_None;
			FString OverrideFailure;
			TestTrue(*FString::Printf(TEXT("Explicit cost override resolves: %s"), *AssetId.ToString()),
				FFF_FacilityCostResolver::FindInstallCost(
					Definition->Spec, CostTable, ResolvedCost, MatchedRow, OverrideFailure));
			TestEqual(TEXT("Explicit override resolves its exact row without a fallback"), MatchedRow, ExplicitKey);
			TestEqual(TEXT("Explicit override preserves the live table price"), ResolvedCost, ExplicitRow->InstallCost);
			TestTrue(TEXT("Production override prices satisfy the shared installation cost rule"),
				FFF_FacilityCostResolver::IsValidInstallCost(ResolvedCost));
			continue;
		}

		++AutomaticDefinitionCount;
		FName GeneratedCostKey = NAME_None;
		FString KeyFailureReason;
		if (!TestTrue(
			*FString::Printf(TEXT("Automatic four-part cost key resolves: %s"),
				*AssetId.ToString()),
			FFF_FacilityCostResolver::ResolveCostKey(
				Definition->Spec,
				GeneratedCostKey,
				KeyFailureReason)))
		{
			AddError(FString::Printf(
				TEXT("%s cost-key generation failed: %s"),
				*AssetId.ToString(),
				*KeyFailureReason));
			continue;
		}

		TArray<FString> KeyParts;
		GeneratedCostKey.ToString().ParseIntoArray(KeyParts, TEXT("."), false);
		TestEqual(
			*FString::Printf(TEXT("Automatic cost key has four parts: %s -> %s"),
				*AssetId.ToString(),
				*GeneratedCostKey.ToString()),
			KeyParts.Num(),
			4);

		const FFF_FacilityCostRow* ExactRow =
			CostTable->FindRow<FFF_FacilityCostRow>(
				GeneratedCostKey,
				CostLookupContext,
				false);
		TestNotNull(
			*FString::Printf(TEXT("Canonical cost table contains the exact automatic row: %s -> %s"),
				*AssetId.ToString(),
				*GeneratedCostKey.ToString()),
			ExactRow);
	}
	TestTrue(TEXT("At least one production FacilityDefinition uses automatic cost naming"),
		AutomaticDefinitionCount > 0);

	const UFFZSFacilityDefinition* FoodTruckDefinition =
		LoadObject<UFFZSFacilityDefinition>(
			nullptr,
			TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/"
				"PDA_FFZS_Booth_FoodStall_Taco."
				"PDA_FFZS_Booth_FoodStall_Taco"));
	if (!TestNotNull(TEXT("The production FoodTruck FacilityDefinition loads"),
		FoodTruckDefinition))
	{
		return false;
	}
	TestTrue(TEXT("FoodTruck keeps InstallCostKey empty to use the naming convention"),
		FoodTruckDefinition->Spec.InstallCostKey.IsNone());

	int32 InstallCost = 0;
	FName MatchedCostRow = NAME_None;
	FString FailureReason;
	if (!TestTrue(TEXT("The production FoodTruck resolves against the live cost table"),
		FFF_FacilityCostResolver::FindInstallCost(
			FoodTruckDefinition->Spec,
			CostTable,
			InstallCost,
			MatchedCostRow,
			FailureReason)))
	{
		AddError(FString::Printf(TEXT("FoodTruck cost resolution failed: %s"),
			*FailureReason));
		return false;
	}
	TestEqual(TEXT("FoodTruck resolves to its four-part facility-specific economy row"),
		MatchedCostRow, ProductionFoodTruckCostKey);
	const FFF_FacilityCostRow* ProductionRow = CostTable->FindRow<FFF_FacilityCostRow>(
		ProductionFoodTruckCostKey, CostLookupContext, false);
	if (!TestNotNull(TEXT("The canonical Taco facility has its own production cost row"), ProductionRow))
	{
		return false;
	}
	TestTrue(TEXT("The live FoodTruck installation cost satisfies the shared cost rule"),
		FFF_FacilityCostResolver::IsValidInstallCost(ProductionRow->InstallCost));
	TestEqual(TEXT("The resolver returns the authoritative table price without a hardcoded balance value"),
		InstallCost, ProductionRow->InstallCost);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemEconomyInstallAndReplayTest,
	"FestaFiesta.Economy.ZoneSystemBridge.InstallChargesResolvedCostOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemEconomyInstallAndReplayTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedEconomyBridgeTestSettings SettingsGuard;
	FZoneEconomyTestHarness Harness;
	if (!TestTrue(TEXT("An isolated Zone/economy test world initializes"),
		Harness.Initialize(this, HarnessStartingGold)))
	{
		return false;
	}

	TestTrue(TEXT("The bridge implements the FFZS external policy contract"),
		Harness.EconomyBridge->GetClass()->ImplementsInterface(
			UFFZSExternalPolicy::StaticClass()));
	TestEqual(TEXT("The bridge registers in the Economy policy domain"),
		IFFZSExternalPolicy::Execute_GetFFZoneSystemPolicyDomain(Harness.EconomyBridge),
		EFFZSPolicyDomain::Economy);

	const FFFZSCommand InstallCommand =
		MakeInstallCommand(Harness, MakeDerivedCostFoodStallSpec());
	const FFFZSCommandResult FirstResult =
		Harness.ZoneSystem->ExecuteCommand(InstallCommand);
	if (!TestTrue(TEXT("A valid affordable Install succeeds"), FirstResult.bSucceeded))
	{
		return false;
	}
	TestEqual(TEXT("None InstallCostKey resolves to the four-part automatic row and spends the live cost"),
		Harness.Progression->GetGold(), HarnessStartingGold - FoodStallInstallCost);
	TestNotNull(TEXT("The successful Install registers its Facility"),
		Harness.ZoneSystem->FindFacility(InstallCommand.FacilityInstanceId));

	const FFFZSCommandResult ReplayResult =
		Harness.ZoneSystem->ExecuteCommand(InstallCommand);
	TestTrue(TEXT("The same command is served from the FFZS idempotency receipt"),
		ReplayResult.bSucceeded && ReplayResult.bWasDuplicateReplay);
	TestEqual(TEXT("A duplicate replay does not spend the installation cost twice"),
		Harness.Progression->GetGold(), HarnessStartingGold - FoodStallInstallCost);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemEconomyZeroCostInstallTest,
	"FestaFiesta.Economy.ZoneSystemBridge.ZeroCostInstallsWithoutGoldAndRefundsZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemEconomyZeroCostInstallTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedEconomyBridgeTestSettings SettingsGuard;
	FZoneEconomyTestHarness Harness;
	if (!TestTrue(TEXT("An isolated Zone/economy test world initializes"),
		Harness.Initialize(this, HarnessStartingGold)))
	{
		return false;
	}
	// Festival authoring requires positive initial funds; spend them before testing a free install.
	if (!TestTrue(TEXT("Existing spending empties the wallet before the free installation"),
		Harness.Progression->TrySpend(HarnessStartingGold, EFF_SpendReason::Promotion)))
	{
		return false;
	}
	TestEqual(TEXT("The free installation starts with no Gold"), Harness.Progression->GetGold(), 0);

	FFF_FacilityCostRow FreeRow;
	FreeRow.InstallCost = 0;
	Harness.CostTable->AddRow(AutomationFoodStallCostKey, FreeRow);
	const FFFZSCommand InstallCommand =
		MakeInstallCommand(Harness, MakeDerivedCostFoodStallSpec());
	TestTrue(TEXT("A free facility passes placement preview with no Gold"),
		Harness.ZoneSystem->ValidatePlacement(InstallCommand).bIsValid);
	if (!TestTrue(TEXT("An authored zero-cost facility installs with no Gold"),
		Harness.ZoneSystem->ExecuteCommand(InstallCommand).bSucceeded))
	{
		return false;
	}
	TestNotNull(TEXT("The free installation registers a Facility"),
		Harness.ZoneSystem->FindFacility(InstallCommand.FacilityInstanceId));
	TestEqual(TEXT("A free installation preserves the zero balance"),
		Harness.Progression->GetGold(), 0);
	TestEqual(TEXT("A free installation records no installation expense"),
		Harness.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::FacilityInstall), 0);

	const FFFZSCommandResult ReplayResult =
		Harness.ZoneSystem->ExecuteCommand(InstallCommand);
	TestTrue(TEXT("A free installation still has an idempotency receipt"),
		ReplayResult.bSucceeded && ReplayResult.bWasDuplicateReplay);
	TestEqual(TEXT("Replaying a free installation preserves the zero balance"),
		Harness.Progression->GetGold(), 0);

	if (!TestTrue(TEXT("Removing the free facility succeeds"),
		Harness.ZoneSystem->ExecuteCommand(
			MakeRemoveCommand(Harness, InstallCommand.FacilityInstanceId)).bSucceeded))
	{
		return false;
	}
	TestNull(TEXT("Removing the free facility unregisters it"),
		Harness.ZoneSystem->FindFacility(InstallCommand.FacilityInstanceId));
	TestEqual(TEXT("Removing a free facility grants no Gold"),
		Harness.Progression->GetGold(), 0);
	TestEqual(TEXT("Removing a free facility records no refund"),
		Harness.Progression->GetDayLedger().GetTotalRefund(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemEconomyNegativeCostTest,
	"FestaFiesta.Economy.ZoneSystemBridge.NegativeCostFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemEconomyNegativeCostTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedEconomyBridgeTestSettings SettingsGuard;
	FZoneEconomyTestHarness Harness;
	if (!TestTrue(TEXT("An isolated Zone/economy test world initializes"),
		Harness.Initialize(this, HarnessStartingGold)))
	{
		return false;
	}

	FFF_FacilityCostRow NegativeRow;
	NegativeRow.InstallCost = -1;
	Harness.CostTable->AddRow(AutomationFoodStallCostKey, NegativeRow);
	const FFFZSCommand InstallCommand =
		MakeInstallCommand(Harness, MakeDerivedCostFoodStallSpec());
	const FFFZSPlacementValidation PreviewValidation =
		Harness.ZoneSystem->ValidatePlacement(InstallCommand);
	TestFalse(TEXT("A negative installation cost is rejected in the placement preview"),
		PreviewValidation.bIsValid);
	TestTrue(TEXT("A negative cost preview reports invalid configuration"),
		PreviewValidation.Reasons.Contains(EFFZSPlacementFailure::DataInvalid));
	const FFFZSCommandResult Result = Harness.ZoneSystem->ExecuteCommand(InstallCommand);
	TestFalse(TEXT("A negative installation cost cannot install a facility"), Result.bSucceeded);
	TestEqual(TEXT("A negative installation cost reports invalid configuration"),
		Result.FailureReason, EFFZSPlacementFailure::DataInvalid);
	TestEqual(TEXT("A negative installation cost does not grant or spend Gold"),
		Harness.Progression->GetGold(), HarnessStartingGold);
	TestNull(TEXT("A negative installation cost never creates a Facility"),
		Harness.ZoneSystem->FindFacility(InstallCommand.FacilityInstanceId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemEconomyInsufficientFundsTest,
	"FestaFiesta.Economy.ZoneSystemBridge.InsufficientFundsRejectsInstall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemEconomyInsufficientFundsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedEconomyBridgeTestSettings SettingsGuard;
	FZoneEconomyTestHarness Harness;
	if (!TestTrue(TEXT("An isolated Zone/economy test world initializes"),
		Harness.Initialize(this, FoodStallInstallCost - 1)))
	{
		return false;
	}

	const FFFZSCommand InstallCommand =
		MakeInstallCommand(Harness, MakeDerivedCostFoodStallSpec());
	const FFFZSPlacementValidation PreviewValidation =
		Harness.ZoneSystem->ValidatePlacement(InstallCommand);
	TestFalse(TEXT("The placement preview includes economy policy rejection"),
		PreviewValidation.bIsValid);
	TestTrue(TEXT("The preview exposes the stable insufficient-funds reason"),
		PreviewValidation.Reasons.Contains(EFFZSPlacementFailure::InsufficientFunds));
	TestFalse(TEXT("A policy rejection replaces the stale local-success summary"),
		PreviewValidation.Summary.ToString().Contains(TEXT("배치가 유효합니다")));
	TestTrue(TEXT("The preview preserves the actionable economy detail"),
		PreviewValidation.Summary.ToString().Contains(TEXT("설치비")));
	const FFFZSCommandResult Result = Harness.ZoneSystem->ExecuteCommand(InstallCommand);
	TestFalse(TEXT("An Install costing more than the current balance is rejected"),
		Result.bSucceeded);
	TestEqual(TEXT("The rejection has a stable insufficient-funds reason code"),
		Result.FailureReason, EFFZSPlacementFailure::InsufficientFunds);
	TestEqual(TEXT("A rejected Install leaves Gold unchanged"),
		Harness.Progression->GetGold(), FoodStallInstallCost - 1);
	TestNull(TEXT("A rejected Install never registers a Facility"),
		Harness.ZoneSystem->FindFacility(InstallCommand.FacilityInstanceId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemEconomyMoveIsFreeTest,
	"FestaFiesta.Economy.ZoneSystemBridge.MoveDoesNotSpend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemEconomyMoveIsFreeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedEconomyBridgeTestSettings SettingsGuard;
	FZoneEconomyTestHarness Harness;
	if (!TestTrue(TEXT("An isolated Zone/economy test world initializes"),
		Harness.Initialize(this, HarnessStartingGold)))
	{
		return false;
	}

	const FFFZSCommand InstallCommand =
		MakeInstallCommand(Harness, MakeDerivedCostFoodStallSpec());
	const FFFZSCommandResult InstallResult =
		Harness.ZoneSystem->ExecuteCommand(InstallCommand);
	if (!TestTrue(TEXT("The paid Install succeeds before Move"),
		InstallResult.bSucceeded))
	{
		return false;
	}
	const int32 GoldAfterInstall = Harness.Progression->GetGold();

	const FFFZSCommand MoveCommand =
		MakeMoveCommand(Harness, InstallCommand, FIntPoint(5000, 1000));
	const FFFZSCommandResult MoveResult =
		Harness.ZoneSystem->ExecuteCommand(MoveCommand);
	TestTrue(TEXT("A valid Move succeeds"), MoveResult.bSucceeded);
	TestEqual(TEXT("Move is not an Install and therefore spends no Gold"),
		Harness.Progression->GetGold(), GoldAfterInstall);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemEconomyRollbackTest,
	"FestaFiesta.Economy.ZoneSystemBridge.RollbackDoesNotSpend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemEconomyRollbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedEconomyBridgeTestSettings SettingsGuard;
	FZoneEconomyTestHarness Harness;
	if (!TestTrue(TEXT("An isolated Zone/economy test world initializes"),
		Harness.Initialize(this, HarnessStartingGold)))
	{
		return false;
	}

	FFFZSPolicyContext Context;
	Context.Command = MakeInstallCommand(Harness, MakeDerivedCostFoodStallSpec());
	Context.CurrentWorldRevision = Harness.ZoneSystem->GetWorldRevision();
	const FFFZSPolicyDecision PreValidate =
		IFFZSExternalPolicy::Execute_PreValidateFFZoneSystemChange(
			Harness.EconomyBridge, Context);
	if (!TestTrue(TEXT("The affordable Install passes economy pre-validation"),
		PreValidate.bAllowed))
	{
		return false;
	}
	const FFFZSPolicyDecision Tentative =
		IFFZSExternalPolicy::Execute_ApplyTentativeFFZoneSystemChange(
			Harness.EconomyBridge, Context);
	if (!TestTrue(TEXT("The economy policy can stage the Install transaction"),
		Tentative.bAllowed))
	{
		return false;
	}
	TestEqual(TEXT("Tentative policy application does not spend before world commit"),
		Harness.Progression->GetGold(), HarnessStartingGold);

	IFFZSExternalPolicy::Execute_RollbackFFZoneSystemChange(
		Harness.EconomyBridge, Context);
	TestEqual(TEXT("A rolled-back Install transaction leaves Gold unchanged"),
		Harness.Progression->GetGold(), HarnessStartingGold);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemEconomyMissingCostTest,
	"FestaFiesta.Economy.ZoneSystemBridge.MissingCostFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemEconomyMissingCostTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedEconomyBridgeTestSettings SettingsGuard;
	FZoneEconomyTestHarness Harness;
	if (!TestTrue(TEXT("An isolated Zone/economy test world initializes"),
		Harness.Initialize(this, HarnessStartingGold)))
	{
		return false;
	}

	FFFZSFacilitySpec Spec = MakeDerivedCostFoodStallSpec();
	Spec.InstallCostKey = TEXT("Cost.Booth.UnconfiguredAutomationPrice");
	const FFFZSCommand InstallCommand = MakeInstallCommand(Harness, Spec);
	const FFFZSPlacementValidation PreviewValidation =
		Harness.ZoneSystem->ValidatePlacement(InstallCommand);
	TestFalse(TEXT("A missing installation cost is rejected in the placement preview"),
		PreviewValidation.bIsValid);
	TestTrue(TEXT("A missing cost preview reports invalid configuration"),
		PreviewValidation.Reasons.Contains(EFFZSPlacementFailure::DataInvalid));
	const FFFZSCommandResult Result = Harness.ZoneSystem->ExecuteCommand(InstallCommand);
	TestFalse(TEXT("An Install with no exact or category-default cost row fails closed"),
		Result.bSucceeded);
	TestEqual(TEXT("Missing economy data is reported as invalid configuration"),
		Result.FailureReason, EFFZSPlacementFailure::DataInvalid);
	TestEqual(TEXT("Cost resolution failure does not spend Gold"),
		Harness.Progression->GetGold(), HarnessStartingGold);
	TestNull(TEXT("Cost resolution failure does not create a Facility"),
		Harness.ZoneSystem->FindFacility(InstallCommand.FacilityInstanceId));

	Harness.Definition->FacilityCostTable = nullptr;
	const FFFZSCommand MissingTableCommand =
		MakeInstallCommand(Harness, MakeDerivedCostFoodStallSpec());
	const FFFZSCommandResult MissingTableResult =
		Harness.ZoneSystem->ExecuteCommand(MissingTableCommand);
	TestFalse(TEXT("A missing cost table cannot make a facility free"), MissingTableResult.bSucceeded);
	TestEqual(TEXT("A missing cost table reports invalid configuration"),
		MissingTableResult.FailureReason, EFFZSPlacementFailure::DataInvalid);
	TestEqual(TEXT("A missing cost table leaves Gold unchanged"),
		Harness.Progression->GetGold(), HarnessStartingGold);
	TestNull(TEXT("A missing cost table never creates a Facility"),
		Harness.ZoneSystem->FindFacility(MissingTableCommand.FacilityInstanceId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemEconomyLifecycleCommandsAreFreeTest,
	"FestaFiesta.Economy.ZoneSystemBridge.LifecycleCommandsDoNotSpend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemEconomyLifecycleCommandsAreFreeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedEconomyBridgeTestSettings SettingsGuard;
	FZoneEconomyTestHarness Harness;
	if (!TestTrue(TEXT("An isolated Zone/economy test world initializes"),
		Harness.Initialize(this, HarnessStartingGold)))
	{
		return false;
	}

	const FFFZSCommand InstallCommand =
		MakeInstallCommand(Harness, MakeDerivedCostFoodStallSpec());
	if (!TestTrue(TEXT("The paid Install succeeds before the free commands"),
		Harness.ZoneSystem->ExecuteCommand(InstallCommand).bSucceeded))
	{
		return false;
	}
	const int32 GoldAfterInstall = Harness.Progression->GetGold();

	// Remove now refunds a configured share of the install cost — see
	// RemoveRefundsConfiguredShareOfInstallCost. Open/Close/EmergencyClose still carry no price,
	// so the economy policy passes them through untouched regardless of phase.
	const EFFZSCommandType FreeCommands[] = {
		EFFZSCommandType::Open,
		EFFZSCommandType::Close,
		EFFZSCommandType::EmergencyClose
	};

	for (const EFFZSCommandType Type : FreeCommands)
	{
		FFFZSPolicyContext Context;
		Context.Command = MakeLifecycleCommand(Harness, InstallCommand, Type);
		Context.CurrentWorldRevision = Harness.ZoneSystem->GetWorldRevision();

		TestTrue(TEXT("The economy policy allows a command that has no install cost"),
			IFFZSExternalPolicy::Execute_PreValidateFFZoneSystemChange(
				Harness.EconomyBridge, Context).bAllowed);
		TestTrue(TEXT("A command with no install cost stages without an economy reservation"),
			IFFZSExternalPolicy::Execute_ApplyTentativeFFZoneSystemChange(
				Harness.EconomyBridge, Context).bAllowed);
		IFFZSExternalPolicy::Execute_CommitFFZoneSystemChange(Harness.EconomyBridge, Context);
	}

	TestEqual(TEXT("Open/Close/EmergencyClose spend nothing"),
		Harness.Progression->GetGold(), GoldAfterInstall);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemEconomyDemolitionRefundTest,
	"FestaFiesta.Economy.ZoneSystemBridge.RemoveRefundsConfiguredShareOfInstallCost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemEconomyDemolitionRefundTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedEconomyBridgeTestSettings SettingsGuard;
	FZoneEconomyTestHarness Harness;
	if (!TestTrue(TEXT("An isolated Zone/economy test world initializes"),
		Harness.Initialize(this, HarnessStartingGold)))
	{
		return false;
	}

	TestEqual(TEXT("The shipped default refunds 50% of the installation cost"),
		Harness.Definition->FacilityDemolitionRefundRate, 0.5f);

	const FFFZSCommand InstallCommand =
		MakeInstallCommand(Harness, MakeDerivedCostFoodStallSpec());
	if (!TestTrue(TEXT("The paid Install succeeds before demolition"),
		Harness.ZoneSystem->ExecuteCommand(InstallCommand).bSucceeded))
	{
		return false;
	}
	const int32 GoldAfterInstall = HarnessStartingGold - FoodStallInstallCost;
	TestEqual(TEXT("Installing the FoodStall spends its live cost"),
		Harness.Progression->GetGold(), GoldAfterInstall);

	const FFFZSCommandResult RemoveResult = Harness.ZoneSystem->ExecuteCommand(
		MakeRemoveCommand(Harness, InstallCommand.FacilityInstanceId));
	if (!TestTrue(TEXT("Removing an idle Facility succeeds"), RemoveResult.bSucceeded))
	{
		return false;
	}
	// 절반만 돌아온다. 전액이면 배치 실수의 비용이 0이 되어 위치를 고민할 이유가 사라진다.
	const int32 HalfRefund = FoodStallInstallCost / 2;
	TestEqual(TEXT("Demolition refunds 50% of the installation cost"),
		Harness.Progression->GetGold(), GoldAfterInstall + HalfRefund);
	TestNull(TEXT("The refunded Facility is unregistered"),
		Harness.ZoneSystem->FindFacility(InstallCommand.FacilityInstanceId));
	TestEqual(TEXT("The refund is recorded in the day ledger's refund total"),
		Harness.Progression->GetDayLedger().GetTotalRefund(), HalfRefund);

	// 기획 담당자가 에디터에서 비율만 바꾸면 환급액이 따라와야 한다. 코드에 0.5가 박혀 있으면 여기서 드러난다.
	Harness.Definition->FacilityDemolitionRefundRate = 0.8f;
	const FFFZSCommand SecondInstall = MakeInstallCommand(
		Harness, MakeDerivedCostFoodStallSpec(), FIntPoint(5000, 1000));
	if (!TestTrue(TEXT("A second Install succeeds after the rate change"),
		Harness.ZoneSystem->ExecuteCommand(SecondInstall).bSucceeded))
	{
		return false;
	}
	const int32 GoldAfterSecondInstall =
		GoldAfterInstall + HalfRefund - FoodStallInstallCost;
	TestEqual(TEXT("The second Install spends the live cost again"),
		Harness.Progression->GetGold(), GoldAfterSecondInstall);

	TestTrue(TEXT("Removing the second Facility succeeds"),
		Harness.ZoneSystem->ExecuteCommand(
			MakeRemoveCommand(Harness, SecondInstall.FacilityInstanceId)).bSucceeded);

	// 80% 로 바꿨으니 절반보다 많이 돌아와야 한다. 코드에 0.5 가 박혀 있으면 여기서 드러난다.
	const int32 EightyPercentRefund =
		FMath::RoundToInt(FoodStallInstallCost * 0.8f);
	TestTrue(TEXT("The edited 80% rate refunds more than the 50% rate did"),
		EightyPercentRefund > HalfRefund);
	TestEqual(TEXT("The edited 80% rate is what actually comes back"),
		Harness.Progression->GetGold(), GoldAfterSecondInstall + EightyPercentRefund);
	TestEqual(TEXT("Both demolitions accumulate in the same day's refund total"),
		Harness.Progression->GetDayLedger().GetTotalRefund(), HalfRefund + EightyPercentRefund);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
