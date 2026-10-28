#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Economy/FF_FacilityCostResolver.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Save/FF_SaveArchive.h"
#include "Save/FF_ZoneSaveBridge.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_ProjectZoneTestFixture.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
	// Emit the actual tagged archive format with exactly the pre-queue fields.
	// This exercises missing-property migration without maintaining a second save struct.
	struct FLegacyZoneSaveArchive : FObjectAndNameAsStringProxyArchive
	{
		explicit FLegacyZoneSaveArchive(FArchive& Inner)
			: FObjectAndNameAsStringProxyArchive(Inner, true) {}

		virtual bool ShouldSkipProperty(const FProperty* Property) const override
		{
			const FName Name = Property ? Property->GetFName() : NAME_None;
			return Name == TEXT("bHasQueueConfiguration") || Name == TEXT("QueueLayout") ||
				Name == TEXT("QueueSettings") || FObjectAndNameAsStringProxyArchive::ShouldSkipProperty(Property);
		}
	};

	TArray<uint8> MakeLegacyBytes(const FFF_ZoneSaveState& State)
	{
		FBufferArchive Buffer;
		FLegacyZoneSaveArchive Archive(Buffer);
		Archive.ArIsSaveGame = false;
		FFF_ZoneSaveState Copy = State;
		FFF_ZoneSaveState::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return MoveTemp(Buffer);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneQueueSaveArchiveTest,
	"FestaFiesta.Save.ZoneQueues.TaggedArchiveAndLegacyDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_ZoneQueueSaveArchiveTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFF_ZoneSaveState Original;
	FFF_ZoneFacilitySave& Custom = Original.Facilities.AddDefaulted_GetRef();
	Custom.DefinitionId = TEXT("Booth_FoodStall_QueueSave");
	Custom.ZoneId = FFProjectZoneTest::ZoneId();
	Custom.InstanceId = FGuid::NewGuid();
	Custom.Placement.OriginCell = FIntPoint(1250, 1700);
	Custom.Placement.YawCentidegrees = 3700;
	Custom.UpgradeLevel = 3;
	Custom.bHasQueueConfiguration = true;
	Custom.QueueLayout = EFFZSQueueLayout::UShape;
	Custom.QueueSettings.bEnabled = true;
	Custom.QueueSettings.MaxWaitingTeams = 7;
	Custom.QueueSettings.SlotSpacingCm = 175.f;
	Custom.QueueSettings.LaneWidthCm = 600.f;
	FFF_ZoneFacilitySave Disabled = Custom;
	Disabled.InstanceId = FGuid::NewGuid();
	Disabled.QueueLayout = EFFZSQueueLayout::Authored;
	Disabled.QueueSettings.bEnabled = false;
	Original.Facilities.Add(Disabled);

	TArray<uint8> Bytes;
	UFF_SaveArchive::WriteStruct(Bytes, Original);
	FFF_ZoneSaveState Reloaded;
	if (!TestTrue(TEXT("Queue settings use the game's actual save archive"), UFF_SaveArchive::ReadStruct(Bytes, Reloaded)) ||
		!TestEqual(TEXT("Both configured and authored entries survive"), Reloaded.Facilities.Num(), 2)) return false;
	TestTrue(TEXT("Enabled values and original shape round-trip exactly"),
		FFF_ZoneSaveState::StaticStruct()->CompareScriptStruct(&Original, &Reloaded, 0));
	TestTrue(TEXT("An explicitly disabled new configuration is distinguishable from an old save"),
		Reloaded.Facilities[1].bHasQueueConfiguration && !Reloaded.Facilities[1].QueueSettings.bEnabled);

	FFF_ZoneSaveState Legacy;
	if (!TestTrue(TEXT("A save containing only the old property tags remains readable"),
		UFF_SaveArchive::ReadStruct(MakeLegacyBytes(Original), Legacy)) ||
		!TestEqual(TEXT("Legacy facility count is retained"), Legacy.Facilities.Num(), 2)) return false;
	for (int32 Index = 0; Index < Legacy.Facilities.Num(); ++Index)
	{
		TestEqual(TEXT("Existing facility IDs survive missing queue fields"), Legacy.Facilities[Index].InstanceId, Original.Facilities[Index].InstanceId);
		TestTrue(TEXT("Existing placement survives missing queue fields"), Legacy.Facilities[Index].Placement == Original.Facilities[Index].Placement);
		TestEqual(TEXT("Existing upgrade level survives missing queue fields"), Legacy.Facilities[Index].UpgradeLevel, 3);
		TestFalse(TEXT("Missing queue fields select legacy geometry preservation during restoration"), Legacy.Facilities[Index].bHasQueueConfiguration);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ZoneQueueSaveRestoreTest,
	"FestaFiesta.Save.ZoneQueues.RestoreCustomAndAuthoredConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_ZoneQueueSaveRestoreTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
	TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> Navigation(Settings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
	TGuardValue<EFFZSChangePhase> Phase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	auto* Bridge = World->GetSubsystem<UFF_ZoneSaveBridge>();
	auto* Economy = World->GetGameInstance()->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!TestNotNull(TEXT("Zone authority"), Zones) || !TestNotNull(TEXT("Real world save participant"), Bridge) ||
		!TestNotNull(TEXT("Festival economy"), Economy)) return false;

	const UFFZSFacilityDefinition* Definition = LoadObject<UFFZSFacilityDefinition>(nullptr,
		TEXT("/Game/FestaFiesta/Building/FF_Data_Building/Booth/PDA_FFZS_Booth_FoodStall_Taco.PDA_FFZS_Booth_FoodStall_Taco"));
	if (!TestNotNull(TEXT("Production queue definition loads without mutation"), Definition)) return false;
	TStrongObjectPtr<UFF_FestivalDefinition> Festival(NewObject<UFF_FestivalDefinition>(World->GetGameInstance()));
	TStrongObjectPtr<UDataTable> Costs(NewObject<UDataTable>(World->GetGameInstance()));
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow Cost;
	Cost.InstallCost = 0;
	FName CostKey;
	FString CostFailure;
	const bool bResolvedCost = FFF_FacilityCostResolver::ResolveCostKey(Definition->Spec, CostKey, CostFailure);
	if (!TestTrue(*FString::Printf(TEXT("Production definition resolves its real installation cost key: %s"), *CostFailure),
		bResolvedCost)) return false;
	Costs->AddRow(CostKey, Cost);
	int32 ResolvedInstallCost = INDEX_NONE;
	FName MatchedRow;
	const bool bFoundCost = FFF_FacilityCostResolver::FindInstallCost(
		Definition->Spec, Costs.Get(), ResolvedInstallCost, MatchedRow, CostFailure);
	if (!TestTrue(*FString::Printf(TEXT("Save fixture's cost table satisfies normal purchase validation: %s"), *CostFailure),
		bFoundCost)) return false;
	Festival->FestivalId = TEXT("QueueSaveRoundTrip");
	Festival->FacilityCostTable = Costs.Get();
	Festival->InitialGold = 10000;
	Economy->StartFestival(Festival.Get());
	Zones->SetChangePhase(EFFZSChangePhase::InitialSetup);
	auto* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!FFProjectZoneTest::RegisterZone(*this, Zone, Zones)) return false;

	TArray<FGuid> Ids;
	for (const bool bCustom : {true, false})
	{
		FFFZSGridPlacement Placement;
		Placement.OriginCell = FIntPoint(bCustom ? 2000 : 5000, 2000);
		Placement.YawCentidegrees = 0;
		FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommandFromDefinition(Definition, Zone->ZoneId, Placement, Zones->GetWorldRevision());
		Install.FacilitySpec.QueueLayout = bCustom ? EFFZSQueueLayout::Right : EFFZSQueueLayout::Authored;
		Install.FacilitySpec.QueueSettings.bEnabled = bCustom;
		Install.FacilitySpec.QueueSettings.MaxWaitingTeams = 7;
		Install.FacilitySpec.QueueSettings.SlotSpacingCm = 175.f;
		Install.FacilitySpec.QueueSettings.LaneWidthCm = 600.f;
		const FFFZSCommandResult Result = Zones->ExecuteCommand(Install);
		if (!TestTrue(*FString::Printf(TEXT("Source facility installs through authority: %s"), *Result.Message.ToString()), Result.bSucceeded)) return false;
		Ids.Add(Install.FacilityInstanceId);
	}

	TArray<uint8> Bytes;
	Bridge->WriteSaveData(Bytes);
	FFF_ZoneSaveState Captured;
	if (!TestTrue(TEXT("Actual participant saves readable bytes"), UFF_SaveArchive::ReadStruct(Bytes, Captured))) return false;
	TestEqual(TEXT("Saved world contains exactly two facilities"), Captured.Facilities.Num(), 2);
	Bridge->ReadSaveData(Bytes);
	TestEqual(TEXT("Restoration replaces previous actors instead of retaining overlapping originals"), Zones->GetPlacedFacilityCount(), 2);
	for (int32 Index = 0; Index < Ids.Num(); ++Index)
	{
		const AFFZSFacilityActor* Facility = Zones->FindFacility(Ids[Index]);
		if (!TestNotNull(TEXT("Stable facility identity survives restoration"), Facility)) return false;
		const FFFZSFacilitySpec& Base = Facility->GetBaseFacilitySpec();
		TestEqual(TEXT("Custom and authored queue modes both survive restoration"), Base.QueueSettings.bEnabled, Index == 0);
		TestEqual(TEXT("The user's shape survives restoration"), Base.QueueLayout,
			Index == 0 ? EFFZSQueueLayout::Right : EFFZSQueueLayout::Authored);
		if (Index == 0)
		{
			TestEqual(TEXT("User team capacity persists"), Base.QueueSettings.MaxWaitingTeams, 7);
			TestEqual(TEXT("User spacing persists"), Base.QueueSettings.SlotSpacingCm, 175.f);
			TestEqual(TEXT("User width persists"), Base.QueueSettings.LaneWidthCm, 600.f);
		}
		else
		{
			TestTrue(TEXT("Explicitly authored mode retains the original queue path"),
				Facility->GetFacilitySpec().QueueSplinePointsCm == Definition->Spec.QueueSplinePointsCm);
		}
	}

	Bridge->ReadSaveData(MakeLegacyBytes(Captured));
	TestEqual(TEXT("An old save restores both facilities through normal install validation"), Zones->GetPlacedFacilityCount(), 2);
	for (const FGuid& Id : Ids)
	{
		const AFFZSFacilityActor* Facility = Zones->FindFacility(Id);
		if (!TestNotNull(TEXT("Legacy entries retain stable instance IDs"), Facility)) return false;
		TestFalse(TEXT("Missing old settings preserve authored geometry instead of expanding a saved layout"),
			Facility->GetBaseFacilitySpec().QueueSettings.bEnabled);
		TestEqual(TEXT("Old saves had no stored shape and retain authored mode"), Facility->GetQueueLayout(), EFFZSQueueLayout::Authored);
		const FFFZSFacilitySpec& RestoredSpec = Facility->GetFacilitySpec();
		TestEqual(TEXT("Legacy capacity remains exactly as originally authored"), RestoredSpec.QueueRules.MaxWaitingAgents,
			Definition->Spec.QueueRules.MaxWaitingAgents);
		TestTrue(TEXT("Legacy waiting points remain exactly as originally authored"),
			RestoredSpec.QueueSplinePointsCm == Definition->Spec.QueueSplinePointsCm);
		TestTrue(TEXT("Legacy reserved queue rectangles remain exactly as originally authored"),
			RestoredSpec.QueueReservedRects == Definition->Spec.QueueReservedRects);
		TestTrue(TEXT("Legacy reserved queue cells remain exactly as originally authored"),
			RestoredSpec.QueueReservedCells == Definition->Spec.QueueReservedCells);
	}
	return true;
}

#endif
