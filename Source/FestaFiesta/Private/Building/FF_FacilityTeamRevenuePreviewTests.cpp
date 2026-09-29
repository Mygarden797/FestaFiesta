// Team quotes must match the installed facility's actual completion settlement.
#include "Building/FF_FacilityUpgradeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_FacilityUpgradeTypes.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "Building/Synergy/FF_FacilitySynergySubsystem.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "DataTables/FF_FacilityRevenueTableRow.h"
#include "Engine/DataTable.h"
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
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "UObject/StrongObjectPtr.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityTeamRevenuePreviewTest,
	"FestaFiesta.Building.FacilityUpgrade.TeamRevenueMatchesSettlement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityTeamRevenuePreviewTest::RunTest(const FString& Parameters)
{
	UFFZSDeveloperSettings* ZoneSettings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> GroundGuard(ZoneSettings->bEnableGroundChecks, false);
	TGuardValue<bool> CollisionGuard(ZoneSettings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> NavigationGuard(ZoneSettings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> PoliciesGuard(ZoneSettings->bRequireProductionPolicyDomains, false);
	TGuardValue<bool> PresetsGuard(ZoneSettings->bRestrictZoneIdsToProjectPresets, false);
	TGuardValue<EFFZSChangePhase> PhaseGuard(ZoneSettings->InitialPhase, EFFZSChangePhase::InitialSetup);
	TGuardValue<int32> CellsGuard(ZoneSettings->MaxEnumeratedCellsPerFacility, 1'000'000);

	const FName PlainId(TEXT("Booth_FoodStall_TeamRevenuePlain"));
	const FName UpgradedId(TEXT("Booth_FoodStall_TeamRevenueUpgraded"));
	TStrongObjectPtr<UDataTable> SynergyTable(NewObject<UDataTable>());
	SynergyTable->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	FFF_FacilitySynergyRow SynergyRule;
	SynergyRule.DisplayName = FText::FromString(TEXT("Team quote test"));
	SynergyRule.TargetFacilityId = UpgradedId;
	SynergyRule.PartnerFacilityId = PlainId;
	SynergyRule.MaxDistanceMeters = 15.f;
	SynergyRule.RevenueBonusPercent = 25.f;
	SynergyRule.StackingGroup = TEXT("TeamRevenueTest");
	SynergyTable->AddRow(TEXT("TeamRevenueTest"), SynergyRule);
	UFF_FacilitySynergySettings* SynergySettings = GetMutableDefault<UFF_FacilitySynergySettings>();
	TGuardValue<TSoftObjectPtr<UDataTable>> SynergyTableGuard(
		SynergySettings->SynergyTable, TSoftObjectPtr<UDataTable>(SynergyTable.Get()));
	TGuardValue<float> SynergyCapGuard(SynergySettings->MaxTotalBonusPercent, 50.f);

	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!TestNotNull(TEXT("A game instance exists"), GameInstance)) return false;
	UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	UFF_QueueSubsystem* Queue = World->GetSubsystem<UFF_QueueSubsystem>();
	UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge = World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>();
	UFF_FacilityUpgradeSubsystem* Upgrades = World->GetSubsystem<UFF_FacilityUpgradeSubsystem>();
	UFF_FacilitySynergySubsystem* Synergies = World->GetSubsystem<UFF_FacilitySynergySubsystem>();
	UFF_FestivalProgressionSubsystem* Progression = GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!TestTrue(TEXT("Revenue, queue, and upgrade subsystems exist"),
		Zones && Queue && QueueBridge && Upgrades && Synergies && Progression)) return false;

	UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
	UDataTable* Costs = NewObject<UDataTable>(GameInstance);
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow Cost;
	Cost.InstallCost = 100;
	const FName CostKey(TEXT("Cost.Booth.FoodStall.TeamRevenue"));
	Costs->AddRow(CostKey, Cost);
	TStrongObjectPtr<UDataTable> Revenues(NewObject<UDataTable>(GameInstance));
	Revenues->RowStruct = FFF_FacilityRevenueTableRow::StaticStruct();
	FFF_FacilityRevenueTableRow PlainRow;
	PlainRow.RevenuePerUseGold = 20;
	PlainRow.bOverrideCommissionRate = true;
	PlainRow.CommissionRate = .12f;
	Revenues->AddRow(PlainId, PlainRow);
	FFF_FacilityRevenueTableRow UpgradedRow;
	UpgradedRow.RevenuePerUseGold = 100;
	UpgradedRow.bOverrideCommissionRate = true;
	UpgradedRow.CommissionRate = .25f;
	UpgradedRow.RevenueMultiplierByLevel.Add(2, 2.f);
	Revenues->AddRow(UpgradedId, UpgradedRow);
	Definition->FestivalId = TEXT("TeamRevenuePreviewAutomation");
	Definition->InitialGold = 5000;
	Definition->FacilityCostTable = Costs;
	Definition->FacilityRevenueTable = TSoftObjectPtr<UDataTable>(Revenues.Get());
	Definition->BoothCommissionRate = .5f; // The facility-specific rates must take precedence.
	Progression->StartFestival(Definition);
	Queue->SetPaused(true);

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>(
		AFFZSGameplayZone::StaticClass(), FTransform::Identity, Spawn);
	const FName ZoneId(TEXT("GZ_DadaPocha"));
	if (!TestTrue(TEXT("The installation zone is registered"),
		Zone && Zone->ApplyAcceptedZonePreset(ZoneId) && Zones->RegisterZone(Zone))) return false;

	auto Install = [&](const FName DefinitionId, const int32 X, const bool bWithLevels)
	{
		FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
		Spec.FacilityDefinitionId = DefinitionId;
		Spec.InstallCostKey = CostKey;
		Spec.bOverrideRevenuePerUseGold = true;
		Spec.RevenuePerUseGold = 999; // The table also takes precedence over this saved fee.
		Spec.bUsesFacilityLevels = bWithLevels;
		Spec.UpgradeTiers.Reset();
		if (bWithLevels)
		{
			FFFZSFacilityUpgradeTier Tier;
			Tier.TargetLevel = 2;
			Tier.CostMultiplier = 0.f;
			Tier.ServiceDurationMultiplier = 1.f;
			Tier.RevenueMultiplier = 1.5f;
			Spec.UpgradeTiers.Add(Tier);
		}
		FFFZSGridPlacement Placement;
		Placement.OriginCell = FIntPoint(X, 1000);
		const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(
			Spec, ZoneId, Placement, Zones->GetWorldRevision());
		const FFFZSCommandResult Result = Zones->ExecuteCommand(Command);
		TestTrue(*FString::Printf(TEXT("Facility installs: %s"), *Result.Message.ToString()), Result.bSucceeded);
		return Result.bSucceeded ? Command.FacilityInstanceId : FGuid();
	};

	const FGuid PlainFacility = Install(PlainId, 1000, false);
	if (!PlainFacility.IsValid()) return false;
	FFF_FacilityUpgradePreview Quote;
	auto CheckQuoteAndSettlement = [&](const FGuid FacilityId, const TArray<int32>& Expected)
	{
		if (!TestTrue(TEXT("A current quote resolves"), Upgrades->GetUpgradePreview(FacilityId, Quote)) ||
			!TestEqual(TEXT("The quote contains one through six-person examples"),
				Quote.CurrentRevenueByTeamSizeGold.Num(), 6)) return false;
		const int32 QueueId = QueueBridge->FindQueueFacilityId(FacilityId);
		if (!TestTrue(TEXT("The installed facility is mapped to a queue"), QueueId != INDEX_NONE)) return false;
		for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			TestEqual(*FString::Printf(TEXT("%d-person quote rounds the whole team once"), Index + 1),
				Quote.CurrentRevenueByTeamSizeGold[Index], Expected[Index]);
			const int32 Before = Progression->GetTodayRevenue();
			Queue->OnPartyServiceCompleted.Broadcast(QueueId, 98700 + Index, Index + 1, 0.f);
			TestEqual(*FString::Printf(TEXT("%d-person completed payment agrees with the quote"), Index + 1),
				Progression->GetTodayRevenue() - Before, Quote.CurrentRevenueByTeamSizeGold[Index]);
		}
		return true;
	};

	if (!CheckQuoteAndSettlement(PlainFacility, {2, 5, 7, 10, 12, 14})) return false;
	TestFalse(TEXT("Team quotes are available without facility levels"), Quote.bHasLevels);
	TestNotEqual(TEXT("A four-person quote is not four rounded one-person amounts"),
		Quote.CurrentRevenueByTeamSizeGold[3], Quote.CurrentRevenuePerUseGold * 4);

	PlainRow.RevenuePerUseGold = 1;
	PlainRow.CommissionRate = .125f;
	Revenues->AddRow(PlainId, PlainRow);
	if (!CheckQuoteAndSettlement(PlainFacility, {0, 0, 0, 1, 1, 1})) return false;
	TestEqual(TEXT("A zero one-person quote does not mean all teams are free"), Quote.CurrentRevenuePerUseGold, 0);
	PlainRow.RevenuePerUseGold = 0;
	Revenues->AddRow(PlainId, PlainRow);
	if (!CheckQuoteAndSettlement(PlainFacility, {0, 0, 0, 0, 0, 0})) return false;
	TestTrue(TEXT("An explicit free price remains available"), Quote.bRevenueAvailable);

	const FGuid UpgradedFacility = Install(UpgradedId, 2500, true);
	if (!UpgradedFacility.IsValid()) return false;
	FText UpgradeFailure;
	if (!TestTrue(TEXT("The second facility upgrades"), Upgrades->TryUpgradeFacility(UpgradedFacility, 1, UpgradeFailure)))
	{
		AddError(UpgradeFailure.ToString());
		return false;
	}
	TestEqual(TEXT("The nearby partner applies its revenue bonus"), Synergies->GetRevenueMultiplier(UpgradedFacility), 1.25f);
	if (!CheckQuoteAndSettlement(UpgradedFacility, {63, 125, 188, 250, 313, 375})) return false;
	TestEqual(TEXT("The current table level multiplier supersedes the authored tier"), Quote.CurrentRevenueMultiplier, 2.f);
	TestFalse(TEXT("A missing facility cannot produce a quote"), Upgrades->GetUpgradePreview(FGuid::NewGuid(), Quote));
	TestTrue(TEXT("An unavailable quote clears previous team examples"), Quote.CurrentRevenueByTeamSizeGold.IsEmpty());
	return true;
}

#endif
