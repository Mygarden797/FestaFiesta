#include "Building/Synergy/FF_FacilitySynergyEvaluator.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#if WITH_EDITOR
#include "FF_FacilitySynergyTableBuilder.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Economy/Rules/FF_BoothRevenueRules.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#endif

namespace
{
	FFF_FacilitySynergyPoint Point(int32 Id, FName Definition, double X, double Y = 0., double Z = 0.)
	{
		return {FGuid(0, 0, 0, Id), Definition, FVector(X, Y, Z)};
	}
	FFF_FacilitySynergyRule Rule(FName Id, FName Target, FName Partner, float Bonus = 10.f, FName Group = NAME_None)
	{
		FFF_FacilitySynergyRule Result;
		Result.Id = Id;
		Result.Row.TargetFacilityId = Target;
		Result.Row.PartnerFacilityId = Partner;
		Result.Row.RevenueBonusPercent = Bonus;
		Result.Row.MaxDistanceMeters = 15.f;
		Result.Row.StackingGroup = Group;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergyDistanceBoundaryTest,
	"FestaFiesta.Building.Synergy.DistanceBoundaryAndInvalidData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergyDistanceBoundaryTest::RunTest(const FString& Parameters)
{
	FFF_FacilitySynergyEvaluator Evaluator;
	FFF_FacilitySynergyRule Invalid = Rule(TEXT("Invalid"), TEXT("Food"), NAME_None, 90.f);
	FFF_FacilitySynergyRule Disabled = Rule(TEXT("Disabled"), TEXT("Food"), TEXT("DisabledPartner"), 80.f);
	Disabled.Row.bEnabled = false;
	Evaluator.SetRules({Rule(TEXT("Valid"), TEXT("Food"), TEXT("Rest")), Invalid, Disabled}, 50.f);
	const TArray<FName>& RelatedToFood = Evaluator.GetRelatedFacilityDefinitionIds(TEXT("Food"));
	TestTrue(TEXT("Catalog relations work before world facilities exist and skip invalid/disabled rows"),
		RelatedToFood.Num() == 1 && RelatedToFood[0] == FName(TEXT("Rest")));
	TestTrue(TEXT("Catalog relations include the reverse helper-to-beneficiary direction"),
		Evaluator.GetRelatedFacilityDefinitionIds(TEXT("Rest")).Contains(TEXT("Food")));
	TestTrue(TEXT("An unknown definition has no catalog relation"), Evaluator.GetRelatedFacilityDefinitionIds(TEXT("Unknown")).IsEmpty());
	const FFF_FacilitySynergyPoint Target = Point(1, TEXT("Food"), 0.);
	Evaluator.SetFacilities({Target, Point(2, TEXT("Rest"), 1500., 0., 10000.)});
	TestEqual(TEXT("15m boundary included; height ignored; invalid/disabled rows skipped"), Evaluator.Evaluate(Target).TotalPercent, 10.f);
	Evaluator.SetFacilities({Target, Point(2, TEXT("Rest"), 1500.01)});
	TestEqual(TEXT("Just outside exact horizontal radius"), Evaluator.Evaluate(Target).TotalPercent, 0.f);
	TestEqual(TEXT("Direction is not reciprocal"), Evaluator.Evaluate(Point(2, TEXT("Rest"), 1500.)).TotalPercent, 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergyStackingTest,
	"FestaFiesta.Building.Synergy.StrongestGroupDuplicatesAndCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergyStackingTest::RunTest(const FString& Parameters)
{
	FFF_FacilitySynergyEvaluator Evaluator;
	Evaluator.SetRules({Rule(TEXT("Weak"), TEXT("Food"), TEXT("Rest"), 10.f, TEXT("Comfort")),
		Rule(TEXT("Strong"), TEXT("Food"), TEXT("Shade"), 20.f, TEXT("Comfort")),
		Rule(TEXT("Independent"), TEXT("Food"), TEXT("Rest"), 15.f)}, 30.f);
	TestEqual(TEXT("Several authored rows for one partner produce one catalog suggestion"),
		Evaluator.GetRelatedFacilityDefinitionIds(TEXT("Food")).Num(), 2);
	const FFF_FacilitySynergyPoint Target = Point(1, TEXT("Food"), 0.);
	Evaluator.SetFacilities({Point(3, TEXT("Rest"), 250.), Point(4, TEXT("Shade"), 500.), Target, Point(2, TEXT("Rest"), 100.)});
	const FFF_FacilitySynergyEvaluation Result = Evaluator.Evaluate(Target);
	TestEqual(TEXT("20 + 15 is capped to 30; duplicated rests do not stack"), Result.TotalPercent, 30.f);
	float AppliedSum = 0.f;
	for (const FFF_FacilitySynergyMatch& Match : Result.Matches) AppliedSum += Match.AppliedPercent;
	TestEqual(TEXT("Preview rule contributions add up to paid benefit"), AppliedSum, Result.TotalPercent);
	Evaluator.SetRules({Rule(TEXT("Independent"), TEXT("Food"), TEXT("Rest"), 15.f),
		Rule(TEXT("Strong"), TEXT("Food"), TEXT("Shade"), 20.f, TEXT("Comfort")),
		Rule(TEXT("Weak"), TEXT("Food"), TEXT("Rest"), 10.f, TEXT("Comfort"))}, 30.f);
	TestEqual(TEXT("Rule order does not affect result"), Evaluator.Evaluate(Target).TotalPercent, 30.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergyDiscoveryRewardRowTest,
	"FestaFiesta.Building.Synergy.DiscoveryRewardRowContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergyDiscoveryRewardRowTest::RunTest(const FString& Parameters)
{
	FFF_FacilitySynergyRow Row = Rule(TEXT("Reward"), TEXT("Food"), TEXT("Rest")).Row;
	TestEqual(TEXT("New combinations default to a 100 Gold discovery reward"), Row.DiscoveryRewardGold, 100);
	Row.DiscoveryRewardGold = 0;
	TestTrue(TEXT("A designer may keep discovery enabled without a Gold reward"), Row.IsUsable());
	Row.DiscoveryRewardGold = 275;
	TestTrue(TEXT("A designer-authored integer reward is valid"), Row.IsUsable());
	Row.DiscoveryRewardGold = -1;
	FString Failure;
	TestFalse(TEXT("A negative discovery reward is invalid"), Row.IsUsable(&Failure));
	TestFalse(TEXT("Invalid reward explains the validation failure"), Failure.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergyPreviewOverlayTest,
	"FestaFiesta.Building.Synergy.MoveAndDuplicatePreviewConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergyPreviewOverlayTest::RunTest(const FString& Parameters)
{
	FFF_FacilitySynergyEvaluator Evaluator;
	Evaluator.SetRules({Rule(TEXT("Comfort"), TEXT("Food"), TEXT("Rest"))}, 50.f);
	const FFF_FacilitySynergyPoint Target = Point(1, TEXT("Food"), 0.);
	const FFF_FacilitySynergyPoint Partner = Point(2, TEXT("Rest"), 100.);
	Evaluator.SetFacilities({Target, Partner});
	FFF_FacilitySynergyPreview Preview;
	Evaluator.BuildPreview(TEXT("Rest"), FVector(200., 0., 0.), FGuid(), Preview);
	TestEqual(TEXT("Duplicate supporting installation shows existing target"), Preview.Entries.Num(), 1);
	if (!Preview.Entries.IsEmpty())
	{
		TestEqual(TEXT("Duplicate retains 10 before"), Preview.Entries[0].CurrentTotalBonusPercent, 10.f);
		TestEqual(TEXT("Duplicate does not promise 20 after"), Preview.Entries[0].ProposedTotalBonusPercent, 10.f);
	}
	Evaluator.BuildPreview(TEXT("Rest"), FVector(5000., 0., 0.), Partner.InstanceId, Preview);
	TestEqual(TEXT("Move includes beneficiary at old location"), Preview.Entries.Num(), 1);
	if (!Preview.Entries.IsEmpty())
	{
		TestEqual(TEXT("Old bonus shown"), Preview.Entries[0].CurrentTotalBonusPercent, 10.f);
		TestEqual(TEXT("Old moving instance excluded from proposed bonus"), Preview.Entries[0].ProposedTotalBonusPercent, 0.f);
		Evaluator.SetFacilities({Target, Point(2, TEXT("Rest"), 5000.)});
		TestEqual(TEXT("Committed evaluation equals preview"), Evaluator.Evaluate(Target).TotalPercent, Preview.Entries[0].ProposedTotalBonusPercent);
	}
	// A size-changing upgrade moves the beneficiary's centre even when its grid corner stays put.
	Evaluator.SetFacilities({Target, Point(2, TEXT("Rest"), -1500.)});
	const FFF_FacilitySynergyPoint GrownTarget = Point(1, TEXT("Food"), 100.);
	TestEqual(TEXT("Before resize the beneficiary is exactly in range"), Evaluator.Evaluate(Target).TotalPercent, 10.f);
	const float ProposedTargetBonus = Evaluator.Evaluate(GrownTarget, &GrownTarget, Target.InstanceId).TotalPercent;
	TestEqual(TEXT("Proposed resized centre loses the boundary bonus"), ProposedTargetBonus, 0.f);
	Evaluator.SetFacilities({GrownTarget, Point(2, TEXT("Rest"), -1500.)});
	TestEqual(TEXT("Beneficiary-only proposal agrees with committed resized centre"), Evaluator.Evaluate(GrownTarget).TotalPercent, ProposedTargetBonus);
	Evaluator.SetRules({Rule(TEXT("SameType"), TEXT("Rest"), TEXT("Rest"))}, 50.f);
	Evaluator.SetFacilities({Partner});
	Evaluator.BuildPreview(TEXT("Rest"), FVector(100., 0., 0.), Partner.InstanceId, Preview);
	TestEqual(TEXT("A same-type rule with only the moving self has one missing-partner row"), Preview.Entries.Num(), 1);
	for (const FFF_FacilitySynergyPreviewEntry& Entry : Preview.Entries)
		TestEqual(TEXT("Moving facility cannot count its old position as its own partner"), Entry.ProposedTotalBonusPercent, 0.f);
	return true;
}

#if WITH_EDITOR
namespace
{
	UDataTable* ReadProductionSynergyCsv(FAutomationTestBase& Test)
	{
		FString Csv;
		if (!Test.TestTrue(TEXT("Production synergy CSV is readable"), FFileHelper::LoadFileToString(Csv,
			*(FPaths::ProjectDir() / TEXT("data/unreal/facility_synergies_v0.csv"))))) return nullptr;
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
		Table->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
		const TArray<FString> Problems = Table->CreateTableFromCSVString(Csv);
		if (!Test.TestTrue(*FString::Printf(TEXT("Production CSV imports without problems: %s"),
			*FString::Join(Problems, TEXT("; "))), Problems.IsEmpty())) return nullptr;
		return Table;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergySeedCatalogTest,
	"FestaFiesta.Building.Synergy.SeedCatalogContains100DistinctCombinations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergySeedCatalogTest::RunTest(const FString& Parameters)
{
	const UDataTable* Table = ReadProductionSynergyCsv(*this);
	if (!Table) return false;
	TestEqual(TEXT("The shipping seed contains exactly 100 combinations"), Table->GetRowNames().Num(), 100);
	FString Failure;
	const bool bReferencesValid = FFFacilitySynergyTableBuilder::ValidateReferencedDefinitions(*Table, Failure);
	if (!TestTrue(*FString::Printf(TEXT("All 100 seed recipes resolve to actual facility definition IDs: %s"), *Failure),
		bReferencesValid))
	{
		AddError(Failure);
		return false;
	}
	TMap<FName, TSet<FName>> PartnersByTarget;
	TSet<FString> Titles;
	int32 EnabledCount = 0;
	for (const FName RowName : Table->GetRowNames())
	{
		const auto& Row = *Table->FindRow<FFF_FacilitySynergyRow>(RowName, TEXT("Seed catalog contract"));
		if (Row.bEnabled) ++EnabledCount;
		TestTrue(*FString::Printf(TEXT("%s has valid numbers including a nonnegative discovery reward"), *RowName.ToString()), Row.IsUsable());
		TestFalse(*FString::Printf(TEXT("%s has a designer-authored title"), *RowName.ToString()), Row.DisplayName.IsEmpty());
		TSet<FName>& Partners = PartnersByTarget.FindOrAdd(Row.TargetFacilityId);
		TestFalse(*FString::Printf(TEXT("%s is a distinct target/partner combination"), *RowName.ToString()), Partners.Contains(Row.PartnerFacilityId));
		Partners.Add(Row.PartnerFacilityId);
		const FString Title = Row.DisplayName.ToString();
		TestFalse(*FString::Printf(TEXT("%s has a distinct recipe title"), *RowName.ToString()), Titles.Contains(Title));
		Titles.Add(Title);
	}
	TestEqual(TEXT("All 100 shipping recipes are enabled"), EnabledCount, 100);
	TestEqual(TEXT("Every one of the 31 revenue booth types has combinations"), PartnersByTarget.Num(), 31);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergyDiscoveryRewardImportTest,
	"FestaFiesta.Building.Synergy.DiscoveryRewardCsvImport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergyDiscoveryRewardImportTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
	Table->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	const FString Csv = TEXT("Name,bEnabled,DisplayName,TargetFacilityId,PartnerFacilityId,MaxDistanceMeters,RevenueBonusPercent,StackingGroup,DiscoveryRewardGold\n")
		TEXT("Free,True,Free discovery,Food,Rest,15,10,FoodComfort,0\n")
		TEXT("Custom,True,Custom discovery,Food,Shade,15,10,FoodComfort,275\n");
	const TArray<FString> Problems = Table->CreateTableFromCSVString(Csv);
	TestTrue(TEXT("Reward column imports without CSV warnings"), Problems.IsEmpty());
	const auto* Free = Table->FindRow<FFF_FacilitySynergyRow>(TEXT("Free"), TEXT("Reward import"));
	const auto* Custom = Table->FindRow<FFF_FacilitySynergyRow>(TEXT("Custom"), TEXT("Reward import"));
	if (!TestNotNull(TEXT("Zero-reward row imports"), Free) || !TestNotNull(TEXT("Custom-reward row imports"), Custom)) return false;
	TestEqual(TEXT("An explicit zero reward does not fall back to 100 Gold"), Free->DiscoveryRewardGold, 0);
	TestEqual(TEXT("CSV imports the exact authored Gold reward"), Custom->DiscoveryRewardGold, 275);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergyProductionExpansionTest,
	"FestaFiesta.Building.Synergy.ProductionDataAndEvaluatorParity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergyProductionExpansionTest::RunTest(const FString& Parameters)
{
	// Gameplay reads the configured DataTable. Test that actual authored data, so a
	// later Designer edit is validated without forcing it back to CSV seed values.
	const UDataTable* Table = GetDefault<UFF_FacilitySynergySettings>()->SynergyTable.LoadSynchronous();
	if (!TestNotNull(TEXT("Configured runtime synergy table loads"), Table)) return false;
	FString FailureReason;
	const bool bReferencesValid = FFFacilitySynergyTableBuilder::ValidateReferencedDefinitions(*Table, FailureReason);
	if (!TestTrue(*FString::Printf(TEXT("All enabled production IDs resolve to their actual PDA Spec: %s"), *FailureReason),
		bReferencesValid)) return false;

	TArray<FFF_FacilitySynergyRule> Rules;
	TSet<FName> TargetIds, PartnerIds;
	float MaximumRadius = 0.f;
	const UAssetManager* Manager = UAssetManager::GetIfInitialized();
	for (const FName RowName : Table->GetRowNames())
	{
		const auto& Row = *Table->FindRow<FFF_FacilitySynergyRow>(RowName, TEXT("Production balance"));
		if (!Row.bEnabled) continue;
		TestTrue(*FString::Printf(TEXT("%s obeys the authored numeric contract"), *RowName.ToString()), Row.IsUsable());
		TestFalse(*FString::Printf(TEXT("%s has a designer-authored display title"), *RowName.ToString()), Row.DisplayName.IsEmpty());
		const auto* Target = Cast<UFFZSFacilityDefinition>(Manager->GetPrimaryAssetPath(FPrimaryAssetId(
			UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), Row.TargetFacilityId)).TryLoad());
		if (TestNotNull(TEXT("Revenue target definition loads"), Target))
			TestTrue(TEXT("Every beneficiary is an actual Booth"), Target->Spec.Category == EFFZSFacilityCategory::Booth);
		Rules.Add({RowName, Row});
		TargetIds.Add(Row.TargetFacilityId);
		PartnerIds.Add(Row.PartnerFacilityId);
		MaximumRadius = FMath::Max(MaximumRadius, Row.MaxDistanceMeters);
	}

	FFF_FacilitySynergyEvaluator Evaluator;
	const float ConfiguredCap = GetDefault<UFF_FacilitySynergySettings>()->MaxTotalBonusPercent;
	const float Cap = FMath::IsFinite(ConfiguredCap) ? FMath::Clamp(ConfiguredCap, 0.f, 1000.f) : 0.f;
	for (const auto& Rule : Rules)
	{
		// Isolate one authored rule so future overlapping target/partner rows remain legal.
		Evaluator.SetRules({Rule}, Cap);
		TestTrue(TEXT("Production codex relation includes the helper before installation"),
			Evaluator.GetRelatedFacilityDefinitionIds(Rule.Row.TargetFacilityId).Contains(Rule.Row.PartnerFacilityId));
		TestTrue(TEXT("Helper-side codex relation links back to its actual income target"),
			Evaluator.GetRelatedFacilityDefinitionIds(Rule.Row.PartnerFacilityId).Contains(Rule.Row.TargetFacilityId));
		const auto Target = Point(1, Rule.Row.TargetFacilityId, 0.);
		const double RadiusCm = Rule.Row.MaxDistanceMeters * 100.;
		Evaluator.SetFacilities({Target, Point(2, Rule.Row.PartnerFacilityId, RadiusCm, 0., 5000.)});
		TestEqual(*FString::Printf(TEXT("%s applies its real bonus at the inclusive boundary"), *Rule.Id.ToString()),
			Evaluator.Evaluate(Target).TotalPercent, FMath::Min(Rule.Row.RevenueBonusPercent, Cap));
		Evaluator.SetFacilities({Target, Point(2, Rule.Row.PartnerFacilityId, RadiusCm + 1.)});
		TestEqual(*FString::Printf(TEXT("%s stops paying outside its own radius"), *Rule.Id.ToString()),
			Evaluator.Evaluate(Target).TotalPercent, 0.f);
	}
	Evaluator.SetRules(Rules, Cap);
	for (const FName TargetId : TargetIds)
	{
		// This small non-spatial reference calculation follows current authoring values;
		// adding combinations, renaming them or tuning bonuses does not require test edits.
		TMap<FName, float> GroupMaximums;
		float ExpectedTotal = 0.f;
		for (const auto& Rule : Rules)
		{
			if (Rule.Row.TargetFacilityId != TargetId) continue;
			if (Rule.Row.StackingGroup.IsNone()) ExpectedTotal += Rule.Row.RevenueBonusPercent;
			else
			{
				float& Maximum = GroupMaximums.FindOrAdd(Rule.Row.StackingGroup);
				Maximum = FMath::Max(Maximum, Rule.Row.RevenueBonusPercent);
			}
		}
		for (const auto& Group : GroupMaximums) ExpectedTotal += Group.Value;
		ExpectedTotal = FMath::Min(ExpectedTotal, Cap);
		const auto Target = Point(1, TargetId, 0.);
		TArray<FFF_FacilitySynergyPoint> Facilities{Target};
		int32 Instance = 2;
		for (const FName PartnerId : PartnerIds)
		{
			Facilities.Add(Point(Instance++, PartnerId, 0.));
			Facilities.Add(Point(Instance++, PartnerId, 0.));
		}
		Evaluator.SetFacilities(Facilities);
		const float ActualBonus = Evaluator.Evaluate(Target).TotalPercent;
		TestEqual(*FString::Printf(TEXT("%s: all helpers and duplicate copies obey the authored group budget"), *TargetId.ToString()),
			ActualBonus, ExpectedTotal);
		FFF_FacilitySynergyPreview Preview;
		Evaluator.BuildPreview(TargetId, Target.WorldLocation, Target.InstanceId, Preview);
		TestTrue(TEXT("Every production beneficiary has a placement/inspector preview"), Preview.bRulesAvailable && !Preview.Entries.IsEmpty());
		for (const auto& Entry : Preview.Entries)
		{
			if (Entry.TargetInstanceId != Target.InstanceId) continue;
			TestEqual(TEXT("Inspector current bonus matches committed evaluation"), Entry.CurrentTotalBonusPercent, ActualBonus);
			TestEqual(TEXT("Unchanged-location preview matches committed evaluation"), Entry.ProposedTotalBonusPercent, ActualBonus);
		}
		// A large fixture receipt exposes every conservative bonus without changing the
		// shipping settlement rule: commission is rounded once after all multipliers.
		TestEqual(TEXT("Existing settlement combines the production bonus with a level multiplier"),
			FFF_BoothRevenueRules::ResolveCommission(1000, 3, .2f, 1.5f * (1.f + ActualBonus / 100.f)),
			FFF_BoothRevenueRules::ResolveCommission(1000, 3, .2f, 1.5f * (1.f + ExpectedTotal / 100.f)));
		for (int32 Index = 1; Index < Facilities.Num(); ++Index) Facilities[Index].WorldLocation.X = MaximumRadius * 100. + 1.;
		Evaluator.SetFacilities(Facilities);
		TestEqual(TEXT("Moving every helper away removes all derived bonuses"), Evaluator.Evaluate(Target).TotalPercent, 0.f);
	}
	TestEqual(TEXT("Existing rounding can leave a small receipt unchanged despite a valid six-percent bonus"),
		FFF_BoothRevenueRules::ResolveCommission(10, 1, .2f, 1.06f),
		FFF_BoothRevenueRules::ResolveCommission(10, 1, .2f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SynergyAppendOnlyTest,
	"FestaFiesta.Building.Synergy.AppendOnlyPreservesDesignerRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_SynergyAppendOnlyTest::RunTest(const FString& Parameters)
{
	const UDataTable* Source = ReadProductionSynergyCsv(*this);
	if (!Source) return false;
	UDataTable* Destination = NewObject<UDataTable>(GetTransientPackage());
	Destination->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	const auto* SeedBurger = Source->FindRow<FFF_FacilitySynergyRow>(TEXT("BurgerPicnic"), TEXT("Append fixture"));
	const auto* SeedIceCream = Source->FindRow<FFF_FacilitySynergyRow>(TEXT("IceCreamShade"), TEXT("Append fixture"));
	if (!SeedBurger || !SeedIceCream) return false;
	FFF_FacilitySynergyRow DesignerBurger = *SeedBurger;
	DesignerBurger.DisplayName = NSLOCTEXT("FFSynergyAppendTest", "CustomName", "내가 만든 한낮의 작은 축제");
	DesignerBurger.RevenueBonusPercent = 11.f;
	DesignerBurger.MaxDistanceMeters = 13.f;
	DesignerBurger.DiscoveryRewardGold = 375;
	Destination->AddRow(TEXT("BurgerPicnic"), DesignerBurger);
	Destination->AddRow(TEXT("IceCreamShade"), *SeedIceCream);
	Destination->AddRow(TEXT("DesignerOnlyCombination"), DesignerBurger);
	TArray<FName> Added;
	FString Failure;
	if (!TestTrue(TEXT("Explicit append accepts independent designer names and existing numeric edits"),
		FFFacilitySynergyTableBuilder::AppendMissingRows(*Destination, *Source, Added, Failure)))
	{
		AddError(Failure);
		return false;
	}
	TestEqual(TEXT("Only the currently missing CSV row keys are appended"), Added.Num(), Source->GetRowNames().Num() - 2);
	TestEqual(TEXT("User-only rows are retained alongside all current seed rows"), Destination->GetRowNames().Num(), Source->GetRowNames().Num() + 1);
	TestTrue(TEXT("Every field and localized name in the edited existing row remains intact"),
		FFF_FacilitySynergyRow::StaticStruct()->CompareScriptStruct(&DesignerBurger,
			Destination->FindRow<FFF_FacilitySynergyRow>(TEXT("BurgerPicnic"), TEXT("Check preserved row")), 0));
	TestTrue(TEXT("The untouched second starter row remains intact"),
		FFF_FacilitySynergyRow::StaticStruct()->CompareScriptStruct(SeedIceCream,
			Destination->FindRow<FFF_FacilitySynergyRow>(TEXT("IceCreamShade"), TEXT("Check preserved row")), 0));
	for (const FName RowName : Added)
	{
		const auto* AddedRow = Destination->FindRow<FFF_FacilitySynergyRow>(RowName, TEXT("Appended discovery reward"));
		const auto* SourceRow = Source->FindRow<FFF_FacilitySynergyRow>(RowName, TEXT("Seed discovery reward"));
		TestEqual(*FString::Printf(TEXT("%s retains the seed discovery reward when appended"), *RowName.ToString()),
			AddedRow->DiscoveryRewardGold, SourceRow->DiscoveryRewardGold);
	}
	const FString BeforeRepeat = Destination->GetTableAsJSON();
	TestTrue(TEXT("Repeated append succeeds"), FFFacilitySynergyTableBuilder::AppendMissingRows(*Destination, *Source, Added, Failure));
	TestEqual(TEXT("Repeated append has no new rows"), Added.Num(), 0);
	TestEqual(TEXT("Repeated append leaves the complete table unchanged"), Destination->GetTableAsJSON(), BeforeRepeat);

	UDataTable* BadBatch = NewObject<UDataTable>(GetTransientPackage());
	BadBatch->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	BadBatch->AddRow(TEXT("OtherwiseValidNewRow"), *SeedBurger);
	FFF_FacilitySynergyRow BadReference = *SeedBurger;
	// This plausible filename-derived value differs from the real SunsetLandmark Spec ID.
	BadReference.PartnerFacilityId = TEXT("PhotoSpot_Landmark_Landmark");
	BadBatch->AddRow(TEXT("BadNewRow"), BadReference);
	TestFalse(TEXT("An unknown new facility ID rejects the whole append batch"),
		FFFacilitySynergyTableBuilder::AppendMissingRows(*Destination, *BadBatch, Added, Failure));
	TestTrue(TEXT("Failure identifies the actual bad reference"), Failure.Contains(TEXT("PhotoSpot_Landmark_Landmark")));
	TestEqual(TEXT("Rejected append does not report any committed rows"), Added.Num(), 0);
	TestEqual(TEXT("No partial additions or designer data changes on failed preflight"), Destination->GetTableAsJSON(), BeforeRepeat);
	return true;
}
#endif // WITH_EDITOR
#endif
