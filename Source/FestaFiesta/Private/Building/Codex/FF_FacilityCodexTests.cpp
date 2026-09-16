#include "Building/Codex/FF_FacilityCodexModel.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
	FFF_FacilitySynergyRule CodexRule(const FName Id, const FName Target, const FName Partner,
		const float Bonus = 10.f, const FName Group = NAME_None)
	{
		FFF_FacilitySynergyRule Rule;
		Rule.Id = Id;
		Rule.Row.TargetFacilityId = Target;
		Rule.Row.PartnerFacilityId = Partner;
		Rule.Row.RevenueBonusPercent = Bonus;
		Rule.Row.MaxDistanceMeters = 15.f;
		Rule.Row.StackingGroup = Group;
		return Rule;
	}
	FFFZSFacilityReadModel CodexFacility(const int32 Id, const FName Definition, const double X,
		const EFFZSFacilityState State = EFFZSFacilityState::Validated)
	{
		FFFZSFacilityReadModel Facility;
		Facility.FacilityInstanceId = FGuid(0, 0, 0, Id);
		Facility.FacilityDefinitionId = Definition;
		Facility.WorldLocation = FVector(X, 0., 0.);
		Facility.State = State;
		return Facility;
	}
	const FFF_FacilityCodexEntry* CodexEntry(const FFF_FacilityCodexModel& Model, const FName RuleId)
	{
		return Model.GetEntries().FindByPredicate([RuleId](const FFF_FacilityCodexEntry& Entry) { return Entry.RuleId == RuleId; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexDiscoveryTest,
	"FestaFiesta.Building.Codex.DiscoverySurvivesRemovalAndRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_FacilityCodexDiscoveryTest::RunTest(const FString& Parameters)
{
	FFF_FacilityCodexModel Model;
	Model.SetRules({CodexRule(TEXT("FoodRest"), TEXT("Food"), TEXT("Rest"))}, 50.f);
	FFFZSReadModel World;
	Model.Evaluate(World, {});
	TestEqual(TEXT("Unbuilt recipe remains available"), Model.GetEntries().Num(), 1);
	TestFalse(TEXT("Catalog viewing does not discover"), Model.GetEntries()[0].bDiscovered);
	World.Facilities = {CodexFacility(1, TEXT("Food"), 0.)};
	Model.Evaluate(World, {});
	TestFalse(TEXT("Installing only one recipe ingredient cannot discover"), Model.GetEntries()[0].bDiscovered);
	World.Facilities.Add(CodexFacility(2, TEXT("Rest"), 4000., EFFZSFacilityState::Planned));
	Model.Evaluate(World, {});
	TestFalse(TEXT("The second ingredient must finish installation"), Model.GetEntries()[0].bDiscovered);
	World.Facilities[1].State = EFFZSFacilityState::Validated;
	Model.Evaluate(World, {});
	TestTrue(TEXT("Two completed installations discover even outside the effect radius"), Model.GetEntries()[0].bDiscovered);
	TestTrue(TEXT("Both distinct installed ingredients are present"), Model.GetEntries()[0].bHasInstalledPair);
	TestFalse(TEXT("Discovery alone does not satisfy distance"), Model.GetEntries()[0].bConditionMet);
	TestFalse(TEXT("Far installed pair grants no revenue bonus"), Model.GetEntries()[0].bEffectApplied);
	TestEqual(TEXT("Actual far distance remains available"), Model.GetEntries()[0].ClosestDistanceMeters, 40.f);
	TestTrue(TEXT("A new discovery is emitted once"), Model.GetNewDiscoveries().Contains(TEXT("FoodRest")));
	const TSet<FName> SavedDiscoveries = Model.GetNewDiscoveries();
	World.Facilities[1].WorldLocation = FVector(1500., 0., 9000.);
	Model.Evaluate(World, SavedDiscoveries);
	TestTrue(TEXT("Inclusive horizontal boundary still applies the real effect"), Model.GetEntries()[0].bEffectApplied);
	TestEqual(TEXT("Exact boundary distance"), Model.GetEntries()[0].ClosestDistanceMeters, 15.f);
	World.Facilities[1].WorldLocation.X = 1500.01;
	Model.Evaluate(World, SavedDiscoveries);
	TestFalse(TEXT("Moving just outside radius removes live effect"), Model.GetEntries()[0].bEffectApplied);
	TestTrue(TEXT("Movement preserves collection history"), Model.GetEntries()[0].bDiscovered);
	World.Facilities.Reset();
	Model.Evaluate(World, SavedDiscoveries);
	TestTrue(TEXT("Removal preserves collection history"), Model.GetEntries()[0].bDiscovered);
	TestFalse(TEXT("History does not pretend the pair remains installed"), Model.GetEntries()[0].bHasInstalledPair);
	TestFalse(TEXT("Removed layout is not marked active"), Model.GetEntries()[0].bConditionMet);
	TestTrue(TEXT("No repeated write requested"), Model.GetNewDiscoveries().IsEmpty());
	FFF_FacilityCodexModel Reloaded;
	Reloaded.SetRules({CodexRule(TEXT("FoodRest"), TEXT("Food"), TEXT("Rest"))}, 50.f);
	Reloaded.Evaluate(World, SavedDiscoveries);
	TestEqual(TEXT("New world reuses profile history"), FFF_FacilityCodexModel::Summarize(Reloaded.GetEntries()).DiscoveredCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexRevenueParityTest,
	"FestaFiesta.Building.Codex.StackingCapAndDirectionMatchRevenue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_FacilityCodexRevenueParityTest::RunTest(const FString& Parameters)
{
	FFF_FacilityCodexModel Model;
	Model.SetRules({CodexRule(TEXT("Weak"), TEXT("Food"), TEXT("Rest"), 10.f, TEXT("Comfort")),
		CodexRule(TEXT("Strong"), TEXT("Food"), TEXT("Shade"), 20.f, TEXT("Comfort")),
		CodexRule(TEXT("Independent"), TEXT("Food"), TEXT("Rest"), 15.f)}, 30.f);
	FFFZSReadModel World;
	World.Facilities = {CodexFacility(1, TEXT("Food"), 0.), CodexFacility(2, TEXT("Rest"), 100.),
		CodexFacility(3, TEXT("Rest"), 200.), CodexFacility(4, TEXT("Shade"), 300.)};
	Model.Evaluate(World, {});
	const FFF_FacilityCodexEntry* Weak = CodexEntry(Model, TEXT("Weak"));
	const FFF_FacilityCodexEntry* Strong = CodexEntry(Model, TEXT("Strong"));
	const FFF_FacilityCodexEntry* Independent = CodexEntry(Model, TEXT("Independent"));
	if (!TestNotNull(TEXT("Weak recipe"), Weak) || !TestNotNull(TEXT("Strong recipe"), Strong)
		|| !TestNotNull(TEXT("Independent recipe"), Independent)) { return false; }
	TestTrue(TEXT("Suppressed same-group recipe still meets collection condition"), Weak->bConditionMet && Weak->bDiscovered);
	TestFalse(TEXT("Suppressed recipe never promises a payment"), Weak->bEffectApplied);
	TestEqual(TEXT("Winning group receives 20 percent"), Strong->AppliedBonusPercent, 20.f);
	TestEqual(TEXT("Cap leaves only 10 percent for independent recipe"), Independent->AppliedBonusPercent, 10.f);
	TestEqual(TEXT("Duplicate supporting instances do not duplicate beneficiary counts"), Independent->ConditionMetCount, 1);
	const FFF_FacilityCodexSummary Summary = FFF_FacilityCodexModel::Summarize(Model.GetEntries());
	TestEqual(TEXT("All three recipes are discovered"), Summary.DiscoveredCount, 3);
	TestEqual(TEXT("Only two recipes currently contribute"), Summary.EffectAppliedCount, 2);
	World.Facilities.RemoveAt(0);
	Model.Evaluate(World, {});
	TestEqual(TEXT("Supporting facilities alone do not reverse the recipe"), FFF_FacilityCodexModel::Summarize(Model.GetEntries()).ConditionMetCount, 0);
	Model.SetRules({CodexRule(TEXT("DisabledRevenue"), TEXT("Food"), TEXT("Rest"))}, 0.f);
	World.Facilities.Add(CodexFacility(1, TEXT("Food"), 0.));
	Model.Evaluate(World, {});
	TestTrue(TEXT("Zero revenue cap still permits layout discovery"), Model.GetEntries()[0].bDiscovered);
	TestFalse(TEXT("Zero revenue cap reports no applied effect"), Model.GetEntries()[0].bEffectApplied);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexEligibilityTest,
	"FestaFiesta.Building.Codex.ExcludesInvalidRulesAndUninstalledFacilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_FacilityCodexEligibilityTest::RunTest(const FString& Parameters)
{
	FFF_FacilityCodexModel Model;
	FFF_FacilitySynergyRule Disabled = CodexRule(TEXT("Disabled"), TEXT("Food"), TEXT("Rest"));
	Disabled.Row.bEnabled = false;
	Model.SetRules({Disabled, CodexRule(TEXT("Invalid"), TEXT("Food"), NAME_None),
		CodexRule(TEXT("Valid"), TEXT("Food"), TEXT("Rest"))}, 50.f);
	TestEqual(TEXT("Only enabled valid rules count toward completion"), Model.GetEntries().Num(), 1);
	FFFZSReadModel World;
	World.Facilities = {CodexFacility(1, TEXT("Food"), 0., EFFZSFacilityState::Closed),
		CodexFacility(2, TEXT("Rest"), 100., EFFZSFacilityState::Planned),
		CodexFacility(3, TEXT("Rest"), 200., EFFZSFacilityState::Removed)};
	Model.Evaluate(World, {});
	TestFalse(TEXT("Planned and removed facilities cannot unlock a recipe"), Model.GetEntries()[0].bDiscovered);
	World.Facilities.Add(CodexFacility(4, TEXT("Rest"), 300., EFFZSFacilityState::MovePending));
	World.Facilities[3].bNeedsRelocation = true;
	Model.Evaluate(World, {});
	TestTrue(TEXT("A previously installed facility remains installed while relocation is required"), Model.GetEntries()[0].bDiscovered);
	TestTrue(TEXT("Relocation does not erase either installed ingredient"), Model.GetEntries()[0].bHasInstalledPair);
	TestFalse(TEXT("Relocation still blocks the current layout condition"), Model.GetEntries()[0].bConditionMet);
	TestFalse(TEXT("Relocation still blocks the current revenue effect"), Model.GetEntries()[0].bEffectApplied);
	TestEqual(TEXT("Relocating partner is absent from the live distance index"), Model.GetEntries()[0].ClosestDistanceMeters, -1.f);
	World.Facilities[3].bNeedsRelocation = false;
	const FFFZSFacilityReadModel DuplicateFacility = World.Facilities[0];
	World.Facilities.Add(DuplicateFacility);
	Model.Evaluate(World, {});
	TestTrue(TEXT("Closed installed target participates, matching live synergy"), Model.GetEntries()[0].bEffectApplied);
	TestEqual(TEXT("A duplicated read-model GUID cannot inflate counts"), Model.GetEntries()[0].EffectAppliedCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexSameTypeInstallationTest,
	"FestaFiesta.Building.Codex.SameTypeRecipeRequiresDistinctCompletedInstallations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_FacilityCodexSameTypeInstallationTest::RunTest(const FString& Parameters)
{
	FFF_FacilityCodexModel Model;
	Model.SetRules({CodexRule(TEXT("TwoRest"), TEXT("Rest"), TEXT("Rest"))}, 50.f);
	FFFZSReadModel World;
	World.Facilities = {CodexFacility(1, TEXT("Rest"), 0., EFFZSFacilityState::Active)};
	Model.Evaluate(World, {});
	TestFalse(TEXT("One completed facility cannot pair with itself"), Model.GetEntries()[0].bDiscovered);
	const FFFZSFacilityReadModel Duplicate = World.Facilities[0];
	World.Facilities.Add(Duplicate);
	Model.Evaluate(World, {});
	TestFalse(TEXT("A duplicate read-model row is not a second installation"), Model.GetEntries()[0].bHasInstalledPair);
	World.Facilities.Add(CodexFacility(2, TEXT("Rest"), 5000., EFFZSFacilityState::Planned));
	Model.Evaluate(World, {});
	TestFalse(TEXT("A planned second instance is not completed"), Model.GetEntries()[0].bDiscovered);
	World.Facilities.Last().State = EFFZSFacilityState::Closed;
	Model.Evaluate(World, {});
	TestTrue(TEXT("Two different completed instance GUIDs discover the same-type recipe"), Model.GetEntries()[0].bDiscovered);
	TestTrue(TEXT("Multiplicity is satisfied regardless of distance"), Model.GetEntries()[0].bHasInstalledPair);
	TestFalse(TEXT("Far same-type pair is discovered without inventing a bonus"), Model.GetEntries()[0].bEffectApplied);
	const TSet<FName> SavedDiscoveries = Model.GetNewDiscoveries();
	World.Facilities.Last().State = EFFZSFacilityState::Removed;
	Model.Evaluate(World, {});
	TestFalse(TEXT("A removed second instance cannot create a new discovery"), Model.GetEntries()[0].bDiscovered);
	Model.Evaluate(World, SavedDiscoveries);
	TestTrue(TEXT("A previously discovered same-type recipe remains recorded after removal"), Model.GetEntries()[0].bDiscovered);
	TestFalse(TEXT("The historical discovery does not count as a second live facility"), Model.GetEntries()[0].bHasInstalledPair);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexFilterTest,
	"FestaFiesta.Building.Codex.FiltersBothRecipeSidesAndDisplayNames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_FacilityCodexFilterTest::RunTest(const FString& Parameters)
{
	FFF_FacilityCodexEntry Entry;
	Entry.TargetFacilityId = TEXT("Food");
	Entry.PartnerFacilityId = TEXT("Rest");
	Entry.DisplayName = FText::FromString(TEXT("피크닉"));
	Entry.TargetName = FText::FromString(TEXT("푸드트럭"));
	Entry.PartnerName = FText::FromString(TEXT("나무 그늘"));
	Entry.TargetCategory = EFFZSFacilityCategory::Booth;
	FFF_FacilityCodexFilter Filter;
	Filter.FacilityDefinitionId = TEXT("Rest");
	TestTrue(TEXT("Inspector partner side finds its recipe"), FFF_FacilityCodexModel::MatchesFilter(Entry, Filter));
	Filter.SearchText = TEXT(" 그늘 ");
	TestTrue(TEXT("Localized name search trims surrounding whitespace"), FFF_FacilityCodexModel::MatchesFilter(Entry, Filter));
	Filter.State = EFF_FacilityCodexFilterState::Undiscovered;
	TestTrue(TEXT("Unknown recipes remain browsable"), FFF_FacilityCodexModel::MatchesFilter(Entry, Filter));
	Entry.bDiscovered = true;
	TestFalse(TEXT("Newly discovered recipe exits the unknown filter"), FFF_FacilityCodexModel::MatchesFilter(Entry, Filter));
	Filter.State = EFF_FacilityCodexFilterState::Active;
	Entry.bConditionMet = true;
	TestFalse(TEXT("Active means a real contribution, not only distance satisfied"), FFF_FacilityCodexModel::MatchesFilter(Entry, Filter));
	Entry.bEffectApplied = true;
	TestTrue(TEXT("Applied contribution matches active"), FFF_FacilityCodexModel::MatchesFilter(Entry, Filter));
	Filter.bFilterByCategory = true;
	Filter.Category = EFFZSFacilityCategory::Rest;
	TestFalse(TEXT("Category follows the beneficiary facility"), FFF_FacilityCodexModel::MatchesFilter(Entry, Filter));
	return true;
}

#endif
