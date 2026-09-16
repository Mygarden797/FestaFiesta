// The source of catalog highlights survives inspector dismissal for browsing,
// but never outlives an explicit cancellation, removed source, or restored world.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/UI/FF_BuildCatalogScreen.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildSynergyCatalogContextTest,
	"FestaFiesta.Building.BuildMode.SynergyCatalogContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildSynergyCatalogContextTest::RunTest(const FString& Parameters)
{
	UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
	TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> Navigation(Settings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
	TGuardValue<EFFZSChangePhase> Phase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UFF_BuildModeSubsystem* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
	UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	if (!TestNotNull(TEXT("Real build context owner"), Build) || !TestNotNull(TEXT("Real zone registry"), Zones)) return false;
	const FName ZoneId(TEXT("GZ_DadaPocha"));
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!TestNotNull(TEXT("Fixture zone"), Zone) || !TestTrue(TEXT("Preset applied"), Zone->ApplyAcceptedZonePreset(ZoneId)) ||
		!TestTrue(TEXT("Zone registered"), Zones->RegisterZone(Zone))) return false;
	const auto Install = [&](const FName DefinitionId, const TCHAR* Name, const int32 X)
	{
		FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec();
		Spec.FacilityDefinitionId = DefinitionId;
		Spec.DisplayName = FText::FromString(Name);
		Spec.InstallCostKey = NAME_None;
		FFFZSGridPlacement Placement;
		Placement.OriginCell = FIntPoint(X, 1000);
		const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(Spec, ZoneId, Placement, Zones->GetWorldRevision());
		const FFFZSCommandResult Result = Zones->ExecuteCommand(Command);
		TestTrue(*FString::Printf(TEXT("Fixture install: %s"), *Result.Message.ToString()), Result.bSucceeded);
		return Result.bSucceeded ? Zones->FindFacility(Command.FacilityInstanceId) : nullptr;
	};
	AFFZSFacilityActor* Burger = Install(TEXT("Booth_FoodStall_Burger"), TEXT("Burger"), 1000);
	AFFZSFacilityActor* IceCream = Install(TEXT("Booth_FoodStall_IceCream"), TEXT("IceCream"), 4000);
	if (!Burger || !IceCream) return false;
	// Input/inspector selection itself is covered by FacilityInspection. This seam
	// starts at its successful endpoint and exercises the real cached relationship API.
	Build->State = EFF_BuildModeState::BuildMode;
	Build->PendingRemovalFacility = Burger;
	Build->SetSynergyCatalogContext(*Burger);
	TestTrue(TEXT("Shipped Burger rule suggests Seating"), Build->GetSynergyCatalogFacilityIds().Contains(TEXT("Rest_Seating_Seating")));
	TestEqual(TEXT("Context uses the catalog facility name"), Build->GetSynergyCatalogSourceName().ToString(), FString(TEXT("Burger")));
	Build->ClearPendingRemovalSelection(true);
	TestFalse(TEXT("Browsing dismisses the action selection"), Build->PendingRemovalFacility.IsValid());
	TestTrue(TEXT("Browsing keeps highlights after the inspector closes"), Build->GetSynergyCatalogFacilityIds().Contains(TEXT("Rest_Seating_Seating")));
	Build->ClearPendingRemovalSelection();
	TestTrue(TEXT("Explicit cancellation clears both ids and source label"),
		Build->GetSynergyCatalogFacilityIds().IsEmpty() && Build->GetSynergyCatalogSourceName().IsEmpty());
	Build->SetSynergyCatalogContext(*Burger);
	TestFalse(TEXT("Reselecting the same facility after cancellation restores context"), Build->GetSynergyCatalogFacilityIds().IsEmpty());
	Build->SetSynergyCatalogContext(*IceCream);
	TestTrue(TEXT("A different source replaces partners without stale Burger highlights"),
		Build->GetSynergyCatalogFacilityIds().Contains(TEXT("Rest_Shade_Shade")) &&
		!Build->GetSynergyCatalogFacilityIds().Contains(TEXT("Rest_Seating_Seating")));
	Build->State = EFF_BuildModeState::CatalogOpen;
	FFFZSCommand Remove;
	Remove.CommandId = FGuid::NewGuid();
	Remove.TransactionId = FGuid::NewGuid();
	Remove.ExpectedWorldRevision = Zones->GetWorldRevision();
	Remove.Type = EFFZSCommandType::Remove;
	Remove.FacilityInstanceId = IceCream->GetFacilityInstanceId();
	TestTrue(TEXT("Source removal succeeds through the real world command"), Zones->ExecuteCommand(Remove).bSucceeded);
	TestTrue(TEXT("Source removal event clears highlights while the catalog is open"), Build->GetSynergyCatalogFacilityIds().IsEmpty());
	Build->SetSynergyCatalogContext(*Burger);
	Build->HandleWorldSnapshotRestored();
	TestTrue(TEXT("Restored actor registry clears stale context"), Build->GetSynergyCatalogSourceName().IsEmpty());
	Build->SetSynergyCatalogContext(*Burger);
	Build->SetState(EFF_BuildModeState::Closed);
	TestTrue(TEXT("Closed state clears the context before consumers process closure"), Build->GetSynergyCatalogFacilityIds().IsEmpty());

	UFF_BuildCatalogScreen* CatalogScreen = NewObject<UFF_BuildCatalogScreen>();
	FFF_BuildCatalogEntry BurgerEntry;
	BurgerEntry.FacilityDefinitionId = TEXT("Booth_FoodStall_Burger");
	BurgerEntry.Category = EFFZSFacilityCategory::Booth;
	BurgerEntry.MinorToken = TEXT("FoodStall");
	BurgerEntry.bBuildLimitReached = true;
	FFF_BuildCatalogEntry SeatingEntry;
	SeatingEntry.FacilityDefinitionId = TEXT("Rest_Seating_Seating");
	SeatingEntry.Category = EFFZSFacilityCategory::Rest;
	SeatingEntry.MinorToken = TEXT("Seating");
	const TArray<FFF_BuildCatalogEntry> Entries{BurgerEntry, SeatingEntry};
	CatalogScreen->SetSynergyContext({SeatingEntry.FacilityDefinitionId}, FText::FromString(TEXT("Burger")), true);
	CatalogScreen->SetCatalogEntries(Entries);
	TestEqual(TEXT("First legacy view opens the cross-category partner"), CatalogScreen->SelectedCategory, EFFZSFacilityCategory::Rest);
	TestTrue(TEXT("Other categories expose the number of authored partners"), CatalogScreen->GetCategoryLabel(EFFZSFacilityCategory::Rest).ToString().Contains(TEXT("조합 1")));
	CatalogScreen->HandleCategoryClicked(EFFZSFacilityCategory::Booth);
	CatalogScreen->SetCatalogEntries(Entries);
	TestEqual(TEXT("Later catalog refresh preserves the user's chosen category"), CatalogScreen->SelectedCategory, EFFZSFacilityCategory::Booth);
	CatalogScreen->SetSynergyContext({}, FText::GetEmpty());
	TestEqual(TEXT("Clearing context removes the category marker"), CatalogScreen->GetCategoryLabel(EFFZSFacilityCategory::Rest).ToString(), FString(TEXT("휴게")));
	int32 PlacementRequests = 0;
	CatalogScreen->OnFacilitySelected.AddLambda([&PlacementRequests](FName) { ++PlacementRequests; });
	CatalogScreen->HandleSkinTabClicked();
	TestFalse(TEXT("Unknown focus target is rejected"), CatalogScreen->FocusFacility(TEXT("UnknownFacility")));
	TestFalse(TEXT("Empty focus target is rejected"), CatalogScreen->FocusFacility(NAME_None));
	TestTrue(TEXT("Missing target preserves the current tab"), CatalogScreen->SelectedTopTab == UFF_BuildCatalogScreen::ETopTab::Skin);
	TestTrue(TEXT("Existing but installation-limited facility can still be found"), CatalogScreen->FocusFacility(BurgerEntry.FacilityDefinitionId));
	TestTrue(TEXT("Facility focus switches from Skin to Build"), CatalogScreen->SelectedTopTab == UFF_BuildCatalogScreen::ETopTab::Build);
	TestEqual(TEXT("Facility focus selects its actual category"), CatalogScreen->SelectedCategory, EFFZSFacilityCategory::Booth);
	TestTrue(TEXT("Cross-category focus succeeds"), CatalogScreen->FocusFacility(SeatingEntry.FacilityDefinitionId));
	TestEqual(TEXT("Cross-category focus reaches the partner category"), CatalogScreen->SelectedCategory, EFFZSFacilityCategory::Rest);
	TestEqual(TEXT("Finding a facility never starts placement"), PlacementRequests, 0);
	CatalogScreen->HandleCategoryClicked(EFFZSFacilityCategory::Booth);
	CatalogScreen->SetCatalogEntries(Entries);
	TestEqual(TEXT("One-time focus does not pin later user category choices"), CatalogScreen->SelectedCategory, EFFZSFacilityCategory::Booth);
	CatalogScreen->OnFacilitySelected.Clear();
	return true;
}

#endif
